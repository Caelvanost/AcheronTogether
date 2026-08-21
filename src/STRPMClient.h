#pragma once

#include "AcheronBridge.h"
#include "STRPMApi.h"

namespace AcheronTogether
{
    class STRPMClient
    {
    public:
        using StateCallback = std::function<void(STRPMApi::ConnectionID, PlayerState, std::uint64_t)>;
        using MappingCallback = std::function<void(const STRPMApi::ProxyMappingEvent&)>;

        static STRPMClient& GetSingleton();
        ~STRPMClient();

        bool Start(StateCallback stateCallback, MappingCallback mappingCallback);
        void Stop();
        bool SendState(const PlayerState& state, std::uint64_t revision);
        [[nodiscard]] std::optional<RE::FormID> ResolveProxy(STRPMApi::ConnectionID connectionID) const;

    private:
        static void __cdecl OnMessage(const STRPMApi::Message* message, void* userData);
        static void __cdecl OnProxyMapping(const STRPMApi::ProxyMappingEvent* event, void* userData);
        void HandleMessage(const STRPMApi::Message& message);
        void HandleProxyMapping(const STRPMApi::ProxyMappingEvent& event);
        static const char* ResultName(STRPMApi::Result result) noexcept;

        const STRPMApi::Interface* _api{ nullptr };
        const STRPMApi::ProxyResolverInterface* _resolver{ nullptr };
        STRPMApi::ListenerHandle _listener{};
        bool _resolverListenerRegistered{ false };
        std::atomic_bool _running{ false };
        StateCallback _stateCallback;
        MappingCallback _mappingCallback;
    };
}
