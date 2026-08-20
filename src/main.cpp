#include "PCH.h"
#include "StateSync.h"

#ifndef ACHERON_TOGETHER_VERSION
#define ACHERON_TOGETHER_VERSION "dev"
#endif

namespace
{
    void InitLogging()
    {
        auto path = SKSE::log::log_directory();
        if (!path) {
            return;
        }

        *path /= "AcheronTogether.log";
        auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true);
        auto log = std::make_shared<spdlog::logger>("AcheronTogether", std::move(sink));
        spdlog::set_default_logger(std::move(log));
        spdlog::set_level(spdlog::level::trace);
        spdlog::flush_on(spdlog::level::trace);
    }

    void OnSKSEMessage(SKSE::MessagingInterface::Message* message)
    {
        switch (message->type) {
        case SKSE::MessagingInterface::kDataLoaded:
            if (!AcheronTogether::StateSync::GetSingleton().Start()) {
                SKSE::log::critical("Acheron Together failed to start");
            }
            break;

        case SKSE::MessagingInterface::kPreLoadGame:
            AcheronTogether::StateSync::GetSingleton().ResetSession();
            break;

        default:
            break;
        }
    }
}

SKSEPluginLoad(const SKSE::LoadInterface* skse)
{
    InitLogging();
    SKSE::Init(skse);

    SKSE::log::info(
        "Acheron Together v{} loading (STRPM-only)",
        ACHERON_TOGETHER_VERSION);

    auto* messaging = SKSE::GetMessagingInterface();
    if (!messaging) {
        SKSE::log::critical("No SKSE messaging interface");
        return false;
    }

    messaging->RegisterListener(OnSKSEMessage);
    return true;
}
