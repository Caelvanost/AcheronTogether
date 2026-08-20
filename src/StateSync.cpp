#include "PCH.h"
#include "StateSync.h"

namespace AcheronTogether
{
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

        if (!AcheronBridge::GetSingleton().Initialize()) {
            SKSE::log::error("Acheron Together disabled: Acheron bridge initialization failed");
            return false;
        }

        const auto networkReady = STRPMClient::GetSingleton().Start(
            [this](STRPMApi::ConnectionID connectionID, AcheronState state, std::uint64_t revision) {
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

        SKSE::log::info("Acheron state synchronization started");
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

        STRPMClient::GetSingleton().Stop();
        _remoteStates.clear();
        _haveLocalState = false;
        _localRevision = 0;
        SKSE::log::info("Acheron state synchronization stopped");
    }

    void StateSync::ResetSession()
    {
        _remoteStates.clear();
        _haveLocalState = false;
        _lastHeartbeat = {};
        SKSE::log::info("ACHNET session state reset");
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

        const auto state = AcheronBridge::GetSingleton().ReadState(player);
        const auto changed = !_haveLocalState || state != _localState;
        if (changed) {
            _localState = state;
            _haveLocalState = true;
            ++_localRevision;
            SKSE::log::info(
                "ACHNET LOCAL state={} revision={}",
                static_cast<unsigned>(_localState),
                _localRevision);
            SendLocalState(true);
            return;
        }

        const auto now = std::chrono::steady_clock::now();
        if (_lastHeartbeat == std::chrono::steady_clock::time_point{} || now - _lastHeartbeat >= 5s) {
            SendLocalState(true);
        }
    }

    void StateSync::SendLocalState(bool force)
    {
        if (!_haveLocalState || !_running.load()) {
            return;
        }

        const auto now = std::chrono::steady_clock::now();
        if (!force && _lastHeartbeat != std::chrono::steady_clock::time_point{} && now - _lastHeartbeat < 5s) {
            return;
        }

        if (STRPMClient::GetSingleton().SendState(_localState, _localRevision)) {
            _lastHeartbeat = now;
        }
    }

    void StateSync::OnRemoteState(
        STRPMApi::ConnectionID connectionID,
        AcheronState state,
        std::uint64_t revision)
    {
        auto& remote = _remoteStates[connectionID];
        if (revision < remote.revision) {
            SKSE::log::trace(
                "ACHNET stale state ignored connection={} revision={} known={}",
                connectionID,
                revision,
                remote.revision);
            return;
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
            SKSE::log::info(
                "ACHNET PROXY connection={} form={:08X}",
                event.connectionID,
                event.newFormID);
            ApplyRemote(event.connectionID);
            SendLocalState(true);
            break;

        case STRPMApi::ProxyMappingEventType::kRemoved:
            _remoteStates.erase(event.connectionID);
            break;

        case STRPMApi::ProxyMappingEventType::kCleared:
            _remoteStates.clear();
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

        AcheronBridge::GetSingleton().ApplyState(actor, stateIt->second.state);
    }
}
