#include "PCH.h"
#include "STRPMClient.h"

namespace AcheronTogether
{
    namespace
    {
        constexpr wchar_t kModuleName[] = L"STRPluginMessagingAPI.dll";
        constexpr char kChannel[] = "acherontogether";
        constexpr std::string_view kPrefix = "AT1|";
    }

    STRPMClient& STRPMClient::GetSingleton()
    {
        static STRPMClient instance;
        return instance;
    }

    STRPMClient::~STRPMClient()
    {
        Stop();
    }

    const char* STRPMClient::ResultName(STRPMApi::Result result) noexcept
    {
        switch (result) {
        case STRPMApi::Result::kOk: return "ok";
        case STRPMApi::Result::kNotAvailable: return "not-available";
        case STRPMApi::Result::kUnsupportedVersion: return "unsupported-version";
        case STRPMApi::Result::kInvalidArgument: return "invalid-argument";
        case STRPMApi::Result::kNotConnected: return "not-connected";
        case STRPMApi::Result::kChannelAlreadyRegistered: return "channel-already-registered";
        case STRPMApi::Result::kChannelNotRegistered: return "channel-not-registered";
        case STRPMApi::Result::kPayloadTooLarge: return "payload-too-large";
        case STRPMApi::Result::kRateLimited: return "rate-limited";
        case STRPMApi::Result::kTransportError: return "transport-error";
        case STRPMApi::Result::kTargetNotFound: return "target-not-found";
        default: return "unknown";
        }
    }

    bool STRPMClient::Start(StateCallback stateCallback, MappingCallback mappingCallback)
    {
        if (_running.load()) {
            return true;
        }

        const auto module = GetModuleHandleW(kModuleName);
        if (!module) {
            SKSE::log::error("ACHNET STRPM unavailable: STRPluginMessagingAPI.dll not loaded");
            return false;
        }

        const auto queryInterface = reinterpret_cast<STRPMApi::QueryInterfaceFn>(
            GetProcAddress(module, STRPMApi::kQueryInterfaceExportName));
        const auto queryResolver = reinterpret_cast<STRPMApi::QueryProxyResolverFn>(
            GetProcAddress(module, STRPMApi::kQueryProxyResolverExportName));

        if (!queryInterface || !queryResolver) {
            SKSE::log::error("ACHNET STRPM unavailable: required exports missing");
            return false;
        }

        const STRPMApi::Interface* api = nullptr;
        const auto apiResult = queryInterface(STRPMApi::kInterfaceVersion, &api);
        if (apiResult != STRPMApi::Result::kOk || !api || !api->registerChannel || !api->send) {
            SKSE::log::error("ACHNET STRPM interface load failed result={}", ResultName(apiResult));
            return false;
        }

        const STRPMApi::ProxyResolverInterface* resolver = nullptr;
        const auto resolverResult = queryResolver(STRPMApi::kProxyResolverVersion, &resolver);
        if (resolverResult != STRPMApi::Result::kOk || !resolver || !resolver->resolve) {
            SKSE::log::error("ACHNET ProxyResolver load failed result={}", ResultName(resolverResult));
            return false;
        }

        STRPMApi::ListenerHandle listener{};
        const auto registerResult = api->registerChannel(kChannel, &STRPMClient::OnMessage, this, &listener);
        if (registerResult != STRPMApi::Result::kOk) {
            SKSE::log::error("ACHNET channel registration failed result={}", ResultName(registerResult));
            return false;
        }

        _api = api;
        _resolver = resolver;
        _listener = listener;
        _stateCallback = std::move(stateCallback);
        _mappingCallback = std::move(mappingCallback);

        if (_resolver->registerListener) {
            _resolverListenerRegistered =
                _resolver->registerListener(&STRPMClient::OnProxyMapping, this) == STRPMApi::Result::kOk;
        }

        if (_api->setLocalDisplayName) {
            if (auto* player = RE::PlayerCharacter::GetSingleton()) {
                if (const auto* name = player->GetName(); name && *name) {
                    _api->setLocalDisplayName(name);
                }
            }
        }

        _running.store(true);
        SKSE::log::info(
            "ACHNET STRPM READY channel={} apiVersion={} proxyResolver={} resolverListener={}",
            kChannel,
            _api->version,
            _resolver ? 1 : 0,
            _resolverListenerRegistered ? 1 : 0);
        return true;
    }

    void STRPMClient::Stop()
    {
        if (!_running.exchange(false)) {
            return;
        }

        if (_resolver && _resolverListenerRegistered && _resolver->unregisterListener) {
            _resolver->unregisterListener(&STRPMClient::OnProxyMapping, this);
        }
        if (_api && _listener.value && _api->unregisterChannel) {
            _api->unregisterChannel(_listener);
        }

        _resolverListenerRegistered = false;
        _listener = {};
        _resolver = nullptr;
        _api = nullptr;
        _stateCallback = {};
        _mappingCallback = {};
        SKSE::log::info("ACHNET STRPM stopped");
    }

    bool STRPMClient::SendState(AcheronState state, std::uint64_t revision)
    {
        if (!_running.load() || !_api || !_api->send) {
            return false;
        }

        const auto payload = fmt::format(
            "{}{}|{}",
            kPrefix,
            revision,
            static_cast<unsigned>(state));

        STRPMApi::Target target{};
        target.kind = STRPMApi::TargetKind::kAllPlayers;

        const auto result = _api->send(
            kChannel,
            target,
            payload.data(),
            payload.size(),
            STRPMApi::kMessageReliable | STRPMApi::kMessageOrdered);

        if (result != STRPMApi::Result::kOk) {
            SKSE::log::warn("ACHNET TX failed result={} payload={}", ResultName(result), payload);
            return false;
        }

        SKSE::log::trace("ACHNET TX {}", payload);
        return true;
    }

    std::optional<RE::FormID> STRPMClient::ResolveProxy(STRPMApi::ConnectionID connectionID) const
    {
        if (!_resolver || !_resolver->resolve || !connectionID) {
            return std::nullopt;
        }

        STRPMApi::ProxyFormID formID = 0;
        if (_resolver->resolve(connectionID, &formID) != STRPMApi::Result::kOk || !formID) {
            return std::nullopt;
        }
        return static_cast<RE::FormID>(formID);
    }

    void __cdecl STRPMClient::OnMessage(const STRPMApi::Message* message, void* userData)
    {
        if (message && userData) {
            static_cast<STRPMClient*>(userData)->HandleMessage(*message);
        }
    }

    void STRPMClient::HandleMessage(const STRPMApi::Message& message)
    {
        if (!message.data || !message.size || !message.sender.connectionID) {
            return;
        }

        const std::string payload(static_cast<const char*>(message.data), message.size);
        if (!payload.starts_with(kPrefix)) {
            return;
        }

        const auto first = payload.find('|', kPrefix.size());
        if (first == std::string::npos) {
            return;
        }

        std::uint64_t revision = 0;
        unsigned stateValue = 0;
        const auto revisionText = std::string_view(payload).substr(kPrefix.size(), first - kPrefix.size());
        const auto stateText = std::string_view(payload).substr(first + 1);

        const auto revisionResult = std::from_chars(revisionText.data(), revisionText.data() + revisionText.size(), revision);
        const auto stateResult = std::from_chars(stateText.data(), stateText.data() + stateText.size(), stateValue);
        if (revisionResult.ec != std::errc{} || stateResult.ec != std::errc{} || stateValue > 2) {
            SKSE::log::warn("ACHNET RX malformed payload={}", payload);
            return;
        }

        const auto connectionID = message.sender.connectionID;
        const auto state = static_cast<AcheronState>(stateValue);
        SKSE::log::trace("ACHNET RX connection={} rev={} state={}", connectionID, revision, stateValue);

        if (auto* tasks = SKSE::GetTaskInterface()) {
            tasks->AddTask([this, connectionID, state, revision]() {
                if (_stateCallback) {
                    _stateCallback(connectionID, state, revision);
                }
            });
        }
    }

    void __cdecl STRPMClient::OnProxyMapping(const STRPMApi::ProxyMappingEvent* event, void* userData)
    {
        if (event && userData) {
            static_cast<STRPMClient*>(userData)->HandleProxyMapping(*event);
        }
    }

    void STRPMClient::HandleProxyMapping(const STRPMApi::ProxyMappingEvent& event)
    {
        if (auto* tasks = SKSE::GetTaskInterface()) {
            tasks->AddTask([this, event]() {
                if (_mappingCallback) {
                    _mappingCallback(event);
                }
            });
        }
    }
}
