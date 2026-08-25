#include "PCH.h"
#include "StateSync.h"

namespace AcheronTogether
{
    namespace
    {
        constexpr auto kHeartbeatInterval = 5s;
        constexpr auto kOutdoorCheckpointInterval = 5min;
        constexpr auto kCellCheckpointDelay = 2s;
        constexpr auto kPartyWipeGrace = 1250ms;
        constexpr auto kDeathMinimumDelay = 1500ms;
        constexpr auto kDeathSafetyDelay = 3500ms;
        constexpr auto kRespawnCooldown = 3s;
        constexpr RE::FormID kXMarkerLocalFormID = 0x3B;
    }

    StateSync& StateSync::GetSingleton()
    {
        static StateSync instance;
        return instance;
    }

    StateSync::~StateSync()
    {
        Stop();
    }

    bool StateSync::Start()
    {
        if (_running.load()) {
            return true;
        }

        auto& acheron = AcheronBridge::GetSingleton();
        if (!acheron.Initialize()) {
            SKSE::log::error("Acheron Together disabled: Acheron bridge initialization failed");
            return false;
        }

        const auto networkReady = STRPMClient::GetSingleton().Start(
            [this](STRPMApi::ConnectionID connectionID, PlayerState state, std::uint64_t revision) {
                OnRemoteState(connectionID, state, revision);
            },
            [this](const STRPMApi::ProxyMappingEvent& event) {
                OnProxyMapping(event);
            });

        if (!networkReady) {
            SKSE::log::error("Acheron Together disabled: STRPM initialization failed");
            return false;
        }

        _running.store(true);
        _worker = std::jthread([this](std::stop_token stopToken) {
            while (!stopToken.stop_requested() && _running.load()) {
                std::this_thread::sleep_for(250ms);
                if (stopToken.stop_requested() || !_running.load()) {
                    break;
                }
                if (auto* tasks = SKSE::GetTaskInterface()) {
                    tasks->AddTask([this]() {
                        if (_running.load()) {
                            Tick();
                        }
                    });
                }
            }
        });

        SKSE::log::info("Acheron multiplayer defeat/respawn synchronization started");
        return true;
    }

    void StateSync::Stop()
    {
        if (!_running.exchange(false)) {
            return;
        }

        if (_worker.joinable()) {
            _worker.request_stop();
            _worker.join();
        }

        AcheronBridge::GetSingleton().SetConsequenceDisabled(false);
        STRPMClient::GetSingleton().Stop();

        _remoteStates.clear();
        _checkpointMarker = {};
        _haveLocalState = false;
        _localRevision = 0;
        _debugDeadOverride = false;
        SKSE::log::info("Acheron multiplayer synchronization stopped");
    }

    void StateSync::ResetSession()
    {
        AcheronBridge::GetSingleton().SetConsequenceDisabled(false);
        _remoteStates.clear();
        _checkpointMarker = {};
        _haveLocalState = false;
        _lastHeartbeat = {};
        _lastCellID = 0;
        _lastCellInterior = false;
        _cellTrackingInitialized = false;
        _checkpointPending = false;
        _pendingCheckpointAt = {};
        _lastOutdoorCheckpoint = {};
        _f5WasDown = false;
        _f6WasDown = false;
        _f7WasDown = false;
        _debugDeadOverride = false;
        _deathObserved = false;
        _deathObservedAt = {};
        _partyWipeObserved = false;
        _partyWipeObservedAt = {};
        _respawnCooldownUntil = {};
        SKSE::log::info("ACHNET session and checkpoint state reset");
    }

    PlayerState StateSync::ReadLocalState(RE::PlayerCharacter* player) const
    {
        PlayerState state{};
        if (!player) {
            return state;
        }

        state.acheron = AcheronBridge::GetSingleton().ReadState(player);
        state.dead = player->IsDead() || _debugDeadOverride;
        return state;
    }

    void StateSync::Tick()
    {
        if (!_running.load()) {
            return;
        }

        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player) {
            return;
        }

        HandleDebugHotkeys(player);

        const auto now = std::chrono::steady_clock::now();
        const auto state = ReadLocalState(player);

        if (!state.dead) {
            EnsureCheckpointMarker(player);
            UpdateCheckpointTracking(player, state, now);
        }

        const auto changed = !_haveLocalState || state != _localState;
        if (changed) {
            _localState = state;
            _haveLocalState = true;
            ++_localRevision;
            SKSE::log::info(
                "ACHNET LOCAL acheron={} dead={} revision={}",
                static_cast<unsigned>(_localState.acheron),
                _localState.dead ? 1 : 0,
                _localRevision);
            SendLocalState(true);
        } else if (_lastHeartbeat == std::chrono::steady_clock::time_point{} || now - _lastHeartbeat >= kHeartbeatInterval) {
            SendLocalState(true);
        }

        EvaluateRespawn(player, state, now);
    }

    void StateSync::SendLocalState(bool force)
    {
        if (!_haveLocalState || !_running.load()) {
            return;
        }

        const auto now = std::chrono::steady_clock::now();
        if (!force && _lastHeartbeat != std::chrono::steady_clock::time_point{} && now - _lastHeartbeat < kHeartbeatInterval) {
            return;
        }

        if (STRPMClient::GetSingleton().SendState(_localState, _localRevision)) {
            _lastHeartbeat = now;
        }
    }

    void StateSync::OnRemoteState(
        STRPMApi::ConnectionID connectionID,
        PlayerState state,
        std::uint64_t revision)
    {
        const bool firstRemote = _remoteStates.empty();
        auto& remote = _remoteStates[connectionID];
        if (revision < remote.revision) {
            SKSE::log::trace(
                "ACHNET stale state ignored connection={} revision={} known={}",
                connectionID,
                revision,
                remote.revision);
            return;
        }

        if (firstRemote) {
            AcheronBridge::GetSingleton().SetConsequenceDisabled(true);
        }

        remote.state = state;
        remote.revision = revision;
        ApplyRemote(connectionID);
    }

    void StateSync::OnProxyMapping(const STRPMApi::ProxyMappingEvent& event)
    {
        switch (event.type) {
        case STRPMApi::ProxyMappingEventType::kAdded:
        case STRPMApi::ProxyMappingEventType::kUpdated:
            AcheronBridge::GetSingleton().SetConsequenceDisabled(true);
            SKSE::log::info(
                "ACHNET PROXY connection={} form={:08X}",
                event.connectionID,
                event.newFormID);
            ApplyRemote(event.connectionID);
            SendLocalState(true);
            break;

        case STRPMApi::ProxyMappingEventType::kRemoved:
            _remoteStates.erase(event.connectionID);
            if (_remoteStates.empty()) {
                AcheronBridge::GetSingleton().SetConsequenceDisabled(false);
            }
            SKSE::log::info("ACHNET PROXY removed connection={}", event.connectionID);
            break;

        case STRPMApi::ProxyMappingEventType::kCleared:
            _remoteStates.clear();
            AcheronBridge::GetSingleton().SetConsequenceDisabled(false);
            SKSE::log::info("ACHNET PROXY mappings cleared");
            break;

        default:
            break;
        }
    }

    void StateSync::ApplyRemote(STRPMApi::ConnectionID connectionID)
    {
        const auto stateIt = _remoteStates.find(connectionID);
        if (stateIt == _remoteStates.end()) {
            return;
        }

        const auto formID = STRPMClient::GetSingleton().ResolveProxy(connectionID);
        if (!formID) {
            SKSE::log::trace("ACHNET pending state: proxy unresolved connection={}", connectionID);
            return;
        }

        auto* actor = RE::TESForm::LookupByID<RE::Actor>(*formID);
        if (!actor || actor->IsPlayerRef()) {
            SKSE::log::warn(
                "ACHNET proxy lookup rejected connection={} form={:08X}",
                connectionID,
                *formID);
            return;
        }

        if (stateIt->second.state.dead) {
            SKSE::log::trace(
                "ACHNET remote connection={} is truly dead; leaving death representation to STR",
                connectionID);
            return;
        }

        AcheronBridge::GetSingleton().ApplyState(actor, stateIt->second.state.acheron);
    }

    bool StateSync::EnsureCheckpointMarker(RE::PlayerCharacter* player)
    {
        if (!player) {
            return false;
        }

        if (auto marker = _checkpointMarker.get()) {
            return true;
        }

        auto* data = RE::TESDataHandler::GetSingleton();
        auto* xMarker = data ? data->LookupForm<RE::TESBoundObject>(kXMarkerLocalFormID, "Skyrim.esm") : nullptr;
        if (!xMarker) {
            SKSE::log::error("ACHRESP checkpoint XMarker 0000003B not found");
            return false;
        }

        auto placed = player->PlaceObjectAtMe(xMarker, true);
        if (!placed) {
            SKSE::log::error("ACHRESP failed to create runtime checkpoint marker");
            return false;
        }

        _checkpointMarker = placed->GetHandle();
        _lastOutdoorCheckpoint = std::chrono::steady_clock::now();
        SKSE::log::info(
            "ACHRESP checkpoint marker created form={:08X} cell={:08X}",
            placed->GetFormID(),
            player->GetParentCell() ? player->GetParentCell()->GetFormID() : 0);
        return true;
    }

    bool StateSync::UpdateCheckpoint(RE::PlayerCharacter* player, std::string_view reason)
    {
        if (!player) {
            return false;
        }

        const auto state = ReadLocalState(player);
        if (state.IsIncapacitated()) {
            return false;
        }

        if (!EnsureCheckpointMarker(player)) {
            return false;
        }

        auto marker = _checkpointMarker.get();
        if (!marker) {
            return false;
        }

        marker->MoveTo(player);
        _lastOutdoorCheckpoint = std::chrono::steady_clock::now();
        _checkpointPending = false;

        SKSE::log::info(
            "ACHRESP CHECKPOINT reason={} marker={:08X} cell={:08X} pos=({:.1f},{:.1f},{:.1f})",
            reason,
            marker->GetFormID(),
            player->GetParentCell() ? player->GetParentCell()->GetFormID() : 0,
            player->GetPositionX(),
            player->GetPositionY(),
            player->GetPositionZ());
        RE::DebugNotification("Checkpoint updated.");
        return true;
    }

    void StateSync::UpdateCheckpointTracking(
        RE::PlayerCharacter* player,
        const PlayerState& state,
        std::chrono::steady_clock::time_point now)
    {
        if (!player || state.IsIncapacitated()) {
            return;
        }

        auto* cell = player->GetParentCell();
        const auto cellID = cell ? cell->GetFormID() : 0;
        const bool interior = cell && cell->IsInteriorCell();

        if (!_cellTrackingInitialized) {
            _cellTrackingInitialized = true;
            _lastCellID = cellID;
            _lastCellInterior = interior;
            UpdateCheckpoint(player, "initial");
        } else if (cellID != _lastCellID) {
            const bool previousInterior = _lastCellInterior;
            _lastCellID = cellID;
            _lastCellInterior = interior;

            if (previousInterior || interior) {
                _checkpointPending = true;
                _pendingCheckpointAt = now + kCellCheckpointDelay;
                SKSE::log::trace("ACHRESP interior cell transition detected; checkpoint pending");
            }
        } else if (interior != _lastCellInterior) {
            _lastCellInterior = interior;
            _checkpointPending = true;
            _pendingCheckpointAt = now + kCellCheckpointDelay;
        }

        if (_checkpointPending && now >= _pendingCheckpointAt) {
            UpdateCheckpoint(player, "cell-transition");
        }

        const bool f5Down = (GetAsyncKeyState(VK_F5) & 0x8000) != 0;
        if (f5Down && !_f5WasDown) {
            UpdateCheckpoint(player, "F5");
        }
        _f5WasDown = f5Down;

        if (!interior &&
            _lastOutdoorCheckpoint != std::chrono::steady_clock::time_point{} &&
            now - _lastOutdoorCheckpoint >= kOutdoorCheckpointInterval &&
            !player->IsInCombat()) {
            UpdateCheckpoint(player, "outdoor-timer");
        }
    }

    void StateSync::HandleDebugHotkeys(RE::PlayerCharacter* player)
    {
        if (!player) {
            return;
        }

        const bool f6Down = (GetAsyncKeyState(VK_F6) & 0x8000) != 0;
        if (f6Down && !_f6WasDown) {
            SKSE::log::warn("ACHDEBUG F6 force defeat requested");
            RE::DebugNotification("Acheron Together: Force Defeat");
            AcheronBridge::GetSingleton().ApplyState(player, AcheronState::kDefeated);
        }
        _f6WasDown = f6Down;

        const bool f7Down = (GetAsyncKeyState(VK_F7) & 0x8000) != 0;
        if (f7Down && !_f7WasDown) {
            if (!_debugDeadOverride) {
                _debugDeadOverride = true;
                SKSE::log::warn("ACHDEBUG F7 simulated true death requested");
                RE::DebugNotification("Acheron Together: Simulated True Death");
            }
        }
        _f7WasDown = f7Down;
    }

    bool StateSync::IsPartyWiped(const PlayerState& localState) const
    {
        if (!localState.IsIncapacitated() || _remoteStates.empty()) {
            return false;
        }

        for (const auto& [connectionID, remote] : _remoteStates) {
            (void)connectionID;
            if (!remote.state.IsIncapacitated()) {
                return false;
            }
        }
        return true;
    }

    void StateSync::EvaluateRespawn(
        RE::PlayerCharacter* player,
        const PlayerState& state,
        std::chrono::steady_clock::time_point now)
    {
        if (!player || now < _respawnCooldownUntil) {
            return;
        }

        if (state.dead) {
            if (!_deathObserved) {
                _deathObserved = true;
                _deathObservedAt = now;
                SKSE::log::warn("ACHRESP local true death detected");
            }
        } else {
            _deathObserved = false;
            _deathObservedAt = {};
        }

        const bool partyWiped = IsPartyWiped(state);
        if (partyWiped) {
            if (!_partyWipeObserved) {
                _partyWipeObserved = true;
                _partyWipeObservedAt = now;
                SKSE::log::warn("ACHRESP party wipe candidate detected");
            }

            if (now - _partyWipeObservedAt >= kPartyWipeGrace) {
                RespawnLocal(player, "party-wipe");
                return;
            }
        } else {
            _partyWipeObserved = false;
            _partyWipeObservedAt = {};
        }

        if (_deathObserved) {
            const auto elapsed = now - _deathObservedAt;
            if ((elapsed >= kDeathMinimumDelay && !player->IsInKillMove()) || elapsed >= kDeathSafetyDelay) {
                RespawnLocal(player, _debugDeadOverride ? "debug-true-death" : "true-death");
            }
        }
    }

    bool StateSync::RespawnLocal(RE::PlayerCharacter* player, std::string_view reason)
    {
        if (!player) {
            return false;
        }

        auto marker = _checkpointMarker.get();
        if (!marker) {
            SKSE::log::error("ACHRESP respawn aborted: no checkpoint marker");
            return false;
        }

        const bool wasDead = player->IsDead();
        SKSE::log::warn(
            "ACHRESP RESPAWN reason={} dead={} acheron={} marker={:08X}",
            reason,
            wasDead ? 1 : 0,
            static_cast<unsigned>(AcheronBridge::GetSingleton().ReadState(player)),
            marker->GetFormID());

        if (wasDead) {
            player->Resurrect(false, true);
        }

        AcheronBridge::GetSingleton().ApplyState(player, AcheronState::kNormal);
        player->MoveTo(marker.get());

        _debugDeadOverride = false;
        _deathObserved = false;
        _deathObservedAt = {};
        _partyWipeObserved = false;
        _partyWipeObservedAt = {};
        _respawnCooldownUntil = std::chrono::steady_clock::now() + kRespawnCooldown;

        _localState = PlayerState{ AcheronState::kNormal, false };
        _haveLocalState = true;
        ++_localRevision;
        SendLocalState(true);

        SKSE::log::info("ACHRESP respawn dispatched successfully");
        return true;
    }
}
