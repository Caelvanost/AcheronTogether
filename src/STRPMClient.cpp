#include "PCH.h"
#include "STRPMClient.h"

namespace AcheronTogether
{
    namespace
    {
        constexpr wchar_t kModuleName[] = L"STRPluginMessagingAPI.dll";
        constexpr char kChannel[] = "acherontogether";
        constexpr std::string_view kPrefixV2 = "AT2|";
        constexpr std::string_view kPrefixV1 = "AT1|";

        bool ParseUnsigned(std::string_view text, std::uint64_t& value)
        {
            const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
            return result.ec == std::errc{} && result.ptr == text.data() + text.size();
        }

        bool ParseUnsigned(std::string_view text, unsigned& value)
        {
            const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
            return result.ec == std::errc{} && result.ptr == text.data() + text.size();
        }
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
            "ACHNET STRPM READY channel={} apiVersion={} proxyResolver={} resolverListener={} wire=AT2",
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

    bool STRPMClient::SendState(const PlayerState& state, std::uint64_t revision)
    {
        if (!_running.load() || !_api || !_api->send) {
            return false;
        }

        const auto payload = fmt::format(
            "{}{}|{}|{}",
            kPrefixV2,
            revision,
            static_cast<unsigned>(state.acheron),
            state.dead ? 1 : 0);

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
        PlayerState state{};
        std::uint64_t revision = 0;

        if (payload.starts_with(kPrefixV2)) {
            const auto first = payload.find('|', kPrefixV2.size());
            const auto second = first == std::string::npos ? std::string::npos : payload.find('|', first + 1);
            if (first == std::string::npos || second == std::string::npos) {
                SKSE::log::warn("ACHNET RX malformed AT2 payload={}", payload);
                return;
            }

            unsigned stateValue = 0;
            unsigned deadValue = 0;
            const auto view = std::string_view(payload);
            const auto revisionText = view.substr(kPrefixV2.size(), first - kPrefixV2.size());
            const auto stateText = view.substr(first + 1, second - first - 1);
            const auto deadText = view.substr(second + 1);

            if (!ParseUnsigned(revisionText, revision) ||
                !ParseUnsigned(stateText, stateValue) ||
                !ParseUnsigned(deadText, deadValue) ||
                stateValue > 2 || deadValue > 1) {
                SKSE::log::warn("ACHNET RX malformed AT2 payload={}", payload);
                return;
            }

            state.acheron = static_cast<AcheronState>(stateValue);
            state.dead = deadValue != 0;
        } else if (payload.starts_with(kPrefixV1)) {
            const auto first = payload.find('|', kPrefixV1.size());
            if (first == std::string::npos) {
                return;
            }

            unsigned stateValue = 0;
            const auto view = std::string_view(payload);
            const auto revisionText = view.substr(kPrefixV1.size(), first - kPrefixV1.size());
            const auto stateText = view.substr(first + 1);
            if (!ParseUnsigned(revisionText, revision) || !ParseUnsigned(stateText, stateValue) || stateValue > 2) {
                return;
            }

            state.acheron = static_cast<AcheronState>(stateValue);
            state.dead = false;
        } else {
            return;
        }

        const auto connectionID = message.sender.connectionID;
        SKSE::log::trace(
            "ACHNET RX connection={} rev={} acheron={} dead={}",
            connectionID,
            revision,
            static_cast<unsigned>(state.acheron),
            state.dead ? 1 : 0);

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
