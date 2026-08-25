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

    private:
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
        void HandleDebugHotkeys(RE::PlayerCharacter* player);
        void EvaluateRespawn(RE::PlayerCharacter* player, const PlayerState& state, std::chrono::steady_clock::time_point now);
        bool IsPartyWiped(const PlayerState& localState) const;
        bool RespawnLocal(RE::PlayerCharacter* player, std::string_view reason);

        std::jthread _worker;
        std::atomic_bool _running{ false };

        bool _haveLocalState{ false };
        PlayerState _localState{};
        std::uint64_t _localRevision{ 0 };
        std::chrono::steady_clock::time_point _lastHeartbeat{};
        std::unordered_map<STRPMApi::ConnectionID, RemoteState> _remoteStates;

        RE::ObjectRefHandle _checkpointMarker{};
        RE::FormID _lastCellID{ 0 };
        bool _lastCellInterior{ false };
        bool _cellTrackingInitialized{ false };
        bool _checkpointPending{ false };
        std::chrono::steady_clock::time_point _pendingCheckpointAt{};
        std::chrono::steady_clock::time_point _lastOutdoorCheckpoint{};
        bool _f5WasDown{ false };
        bool _f6WasDown{ false };
        bool _f7WasDown{ false };

        bool _debugDeadOverride{ false };
        bool _deathObserved{ false };
        std::chrono::steady_clock::time_point _deathObservedAt{};
        bool _partyWipeObserved{ false };
        std::chrono::steady_clock::time_point _partyWipeObservedAt{};
        std::chrono::steady_clock::time_point _respawnCooldownUntil{};
    };
}
