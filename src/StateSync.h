#pragma once

#include "AcheronBridge.h"
#include "STRPMClient.h"

namespace AcheronTogether
{
    class StateSync
    {
    public:
        static StateSync& GetSingleton();
        ~StateSync();

        bool Start();
        void Stop();
        void ResetSession();
        void OnGameLoaded();

        void OnGameSaved();
        void RequestCheckpoint();
        void RequestSimulatedDefeat();
        void RequestSimulatedTrueDeath();

    private:
        enum class CheckpointRequest : std::uint8_t
        {
            kNone = 0,
            kSave,
            kManual
        };

        struct RemoteState
        {
            PlayerState state{};
            std::uint64_t revision{ 0 };
        };

        void Tick();
        [[nodiscard]] PlayerState ReadLocalState(RE::PlayerCharacter* player) const;
        void SendLocalState(bool force);
        void OnRemoteState(STRPMApi::ConnectionID connectionID, PlayerState state, std::uint64_t revision);
        void OnProxyMapping(const STRPMApi::ProxyMappingEvent& event);
        void ApplyRemote(STRPMApi::ConnectionID connectionID);

        bool EnsureCheckpointMarker(RE::PlayerCharacter* player);
        bool UpdateCheckpoint(RE::PlayerCharacter* player, std::string_view reason);
        void UpdateCheckpointTracking(RE::PlayerCharacter* player, const PlayerState& state, std::chrono::steady_clock::time_point now);
        void HandleDebugHotkeys();
        void HandleDebugRequests(RE::PlayerCharacter* player);
        void EvaluateRespawn(RE::PlayerCharacter* player, const PlayerState& state, std::chrono::steady_clock::time_point now);
        bool IsPartyWiped(const PlayerState& localState) const;
        bool RespawnLocal(RE::PlayerCharacter* player, std::string_view reason);

        std::jthread _worker;
        std::atomic_bool _running{ false };
        std::atomic_bool _gameReady{ false };

        bool _haveLocalState{ false };
        PlayerState _localState{};
        std::uint64_t _localRevision{ 0 };
        std::chrono::steady_clock::time_point _lastHeartbeat{};
        std::chrono::steady_clock::time_point _lastSendAttempt{};
        std::unordered_map<STRPMApi::ConnectionID, RemoteState> _remoteStates;

        RE::ObjectRefHandle _checkpointMarker{};
        RE::FormID _lastCellID{ 0 };
        bool _lastCellInterior{ false };
        bool _cellTrackingInitialized{ false };
        bool _checkpointPending{ false };
        std::chrono::steady_clock::time_point _pendingCheckpointAt{};
        std::chrono::steady_clock::time_point _lastOutdoorCheckpoint{};

        std::atomic<CheckpointRequest> _checkpointRequest{ CheckpointRequest::kNone };
        std::atomic_bool _simulatedDefeatRequested{ false };
        std::atomic_bool _simulatedTrueDeathRequested{ false };

        bool _f6WasDown{ false };
        bool _f7WasDown{ false };
        bool _debugDefeatOverride{ false };
        bool _debugDeadOverride{ false };

        bool _deathObserved{ false };
        std::chrono::steady_clock::time_point _deathObservedAt{};
        bool _partyWipeObserved{ false };
        std::chrono::steady_clock::time_point _partyWipeObservedAt{};
        std::chrono::steady_clock::time_point _respawnCooldownUntil{};
    };
}
