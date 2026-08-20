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
            AcheronState state{ AcheronState::kNormal };
            std::uint64_t revision{ 0 };
        };

        void Tick();
        void SendLocalState(bool force);
        void OnRemoteState(STRPMApi::ConnectionID connectionID, AcheronState state, std::uint64_t revision);
        void OnProxyMapping(const STRPMApi::ProxyMappingEvent& event);
        void ApplyRemote(STRPMApi::ConnectionID connectionID);

        std::jthread _worker;
        std::atomic_bool _running{ false };
        bool _haveLocalState{ false };
        AcheronState _localState{ AcheronState::kNormal };
        std::uint64_t _localRevision{ 0 };
        std::chrono::steady_clock::time_point _lastHeartbeat{};
        std::unordered_map<STRPMApi::ConnectionID, RemoteState> _remoteStates;
    };
}
