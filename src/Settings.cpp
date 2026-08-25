#include "PCH.h"
#include "Settings.h"

namespace AcheronTogether
{
    namespace
    {
        constexpr auto kIniPath = "Data\\SKSE\\Plugins\\AcheronTogether.ini";

        bool ReadBool(const char* section, const char* key, bool fallback)
        {
            return GetPrivateProfileIntA(section, key, fallback ? 1 : 0, kIniPath) != 0;
        }

        float ReadFloat(const char* section, const char* key, float fallback)
        {
            char buffer[64]{};
            const auto fallbackString = std::to_string(fallback);
            GetPrivateProfileStringA(section, key, fallbackString.c_str(), buffer, static_cast<DWORD>(sizeof(buffer)), kIniPath);
            try {
                return std::stof(buffer);
            } catch (...) {
                return fallback;
            }
        }

        void WriteBool(const char* section, const char* key, bool value)
        {
            WritePrivateProfileStringA(section, key, value ? "1" : "0", kIniPath);
        }

        void WriteFloat(const char* section, const char* key, float value)
        {
            const auto text = std::to_string(value);
            WritePrivateProfileStringA(section, key, text.c_str(), kIniPath);
        }
    }

    Settings& Settings::GetSingleton()
    {
        static Settings instance;
        return instance;
    }

    void Settings::Load()
    {
        std::scoped_lock lock(_mutex);

        _settings.checkpointOnSave = ReadBool("Checkpoints", "OnSave", true);
        _settings.checkpointOnInteriorTransition = ReadBool("Checkpoints", "OnInteriorTransition", true);
        _settings.periodicOutdoorCheckpoints = ReadBool("Checkpoints", "PeriodicOutdoor", true);
        _settings.initialCheckpoint = ReadBool("Checkpoints", "InitialAfterLoad", true);
        _settings.showCheckpointNotification = ReadBool("Checkpoints", "ShowNotification", true);
        _settings.outdoorCheckpointMinutes = std::clamp(ReadFloat("Checkpoints", "OutdoorIntervalMinutes", 5.0f), 1.0f, 60.0f);

        _settings.individualTrueDeathRespawn = ReadBool("Respawn", "IndividualTrueDeath", true);
        _settings.partyWipeRespawn = ReadBool("Respawn", "PartyWipe", true);
        _settings.soloDefeatRespawn = ReadBool("Respawn", "SoloDefeat", true);
        _settings.trueDeathDelaySeconds = std::clamp(ReadFloat("Respawn", "TrueDeathDelaySeconds", 1.5f), 0.25f, 10.0f);
        _settings.partyWipeDelaySeconds = std::clamp(ReadFloat("Respawn", "PartyWipeDelaySeconds", 1.25f), 0.25f, 10.0f);
        _settings.soloDefeatDelaySeconds = std::clamp(ReadFloat("Respawn", "SoloDefeatDelaySeconds", 30.0f), 1.0f, 300.0f);

        _settings.debugHotkeys = ReadBool("Debug", "EnableHotkeys", true);

        SKSE::log::info(
            "ACHCFG loaded save={} interior={} outdoor={} initial={} notify={} trueDeath={} partyWipe={} soloDefeat={} debug={}",
            _settings.checkpointOnSave ? 1 : 0,
            _settings.checkpointOnInteriorTransition ? 1 : 0,
            _settings.periodicOutdoorCheckpoints ? 1 : 0,
            _settings.initialCheckpoint ? 1 : 0,
            _settings.showCheckpointNotification ? 1 : 0,
            _settings.individualTrueDeathRespawn ? 1 : 0,
            _settings.partyWipeRespawn ? 1 : 0,
            _settings.soloDefeatRespawn ? 1 : 0,
            _settings.debugHotkeys ? 1 : 0);
    }

    void Settings::ResetDefaults()
    {
        std::scoped_lock lock(_mutex);
        _settings = RuntimeSettings{};
        SaveLocked();
        SKSE::log::info("ACHCFG reset to defaults");
    }

    RuntimeSettings Settings::Snapshot() const
    {
        std::scoped_lock lock(_mutex);
        return _settings;
    }

    bool Settings::GetBool(std::int32_t setting) const
    {
        const auto s = Snapshot();
        switch (static_cast<BoolSetting>(setting)) {
        case BoolSetting::kCheckpointOnSave: return s.checkpointOnSave;
        case BoolSetting::kCheckpointOnInteriorTransition: return s.checkpointOnInteriorTransition;
        case BoolSetting::kPeriodicOutdoorCheckpoints: return s.periodicOutdoorCheckpoints;
        case BoolSetting::kInitialCheckpoint: return s.initialCheckpoint;
        case BoolSetting::kShowCheckpointNotification: return s.showCheckpointNotification;
        case BoolSetting::kIndividualTrueDeathRespawn: return s.individualTrueDeathRespawn;
        case BoolSetting::kPartyWipeRespawn: return s.partyWipeRespawn;
        case BoolSetting::kDebugHotkeys: return s.debugHotkeys;
        case BoolSetting::kSoloDefeatRespawn: return s.soloDefeatRespawn;
        default: return false;
        }
    }

    float Settings::GetFloat(std::int32_t setting) const
    {
        const auto s = Snapshot();
        switch (static_cast<FloatSetting>(setting)) {
        case FloatSetting::kOutdoorCheckpointMinutes: return s.outdoorCheckpointMinutes;
        case FloatSetting::kTrueDeathDelaySeconds: return s.trueDeathDelaySeconds;
        case FloatSetting::kPartyWipeDelaySeconds: return s.partyWipeDelaySeconds;
        case FloatSetting::kSoloDefeatDelaySeconds: return s.soloDefeatDelaySeconds;
        default: return 0.0f;
        }
    }

    void Settings::SetBool(std::int32_t setting, bool value)
    {
        {
            std::scoped_lock lock(_mutex);
            switch (static_cast<BoolSetting>(setting)) {
            case BoolSetting::kCheckpointOnSave: _settings.checkpointOnSave = value; break;
            case BoolSetting::kCheckpointOnInteriorTransition: _settings.checkpointOnInteriorTransition = value; break;
            case BoolSetting::kPeriodicOutdoorCheckpoints: _settings.periodicOutdoorCheckpoints = value; break;
            case BoolSetting::kInitialCheckpoint: _settings.initialCheckpoint = value; break;
            case BoolSetting::kShowCheckpointNotification: _settings.showCheckpointNotification = value; break;
            case BoolSetting::kIndividualTrueDeathRespawn: _settings.individualTrueDeathRespawn = value; break;
            case BoolSetting::kPartyWipeRespawn: _settings.partyWipeRespawn = value; break;
            case BoolSetting::kDebugHotkeys: _settings.debugHotkeys = value; break;
            case BoolSetting::kSoloDefeatRespawn: _settings.soloDefeatRespawn = value; break;
            default:
                SKSE::log::warn("ACHCFG ignored unknown bool setting {}", setting);
                return;
            }
            SaveLocked();
        }
        SKSE::log::info("ACHCFG bool {}={}", setting, value ? 1 : 0);
    }

    void Settings::SetFloat(std::int32_t setting, float value)
    {
        float stored = 0.0f;
        {
            std::scoped_lock lock(_mutex);
            switch (static_cast<FloatSetting>(setting)) {
            case FloatSetting::kOutdoorCheckpointMinutes:
                _settings.outdoorCheckpointMinutes = std::clamp(value, 1.0f, 60.0f);
                stored = _settings.outdoorCheckpointMinutes;
                break;
            case FloatSetting::kTrueDeathDelaySeconds:
                _settings.trueDeathDelaySeconds = std::clamp(value, 0.25f, 10.0f);
                stored = _settings.trueDeathDelaySeconds;
                break;
            case FloatSetting::kPartyWipeDelaySeconds:
                _settings.partyWipeDelaySeconds = std::clamp(value, 0.25f, 10.0f);
                stored = _settings.partyWipeDelaySeconds;
                break;
            case FloatSetting::kSoloDefeatDelaySeconds:
                _settings.soloDefeatDelaySeconds = std::clamp(value, 1.0f, 300.0f);
                stored = _settings.soloDefeatDelaySeconds;
                break;
            default:
                SKSE::log::warn("ACHCFG ignored unknown float setting {}", setting);
                return;
            }
            SaveLocked();
        }
        SKSE::log::info("ACHCFG float {}={:.2f}", setting, stored);
    }

    void Settings::SaveLocked() const
    {
        WriteBool("Checkpoints", "OnSave", _settings.checkpointOnSave);
        WriteBool("Checkpoints", "OnInteriorTransition", _settings.checkpointOnInteriorTransition);
        WriteBool("Checkpoints", "PeriodicOutdoor", _settings.periodicOutdoorCheckpoints);
        WriteBool("Checkpoints", "InitialAfterLoad", _settings.initialCheckpoint);
        WriteBool("Checkpoints", "ShowNotification", _settings.showCheckpointNotification);
        WriteFloat("Checkpoints", "OutdoorIntervalMinutes", _settings.outdoorCheckpointMinutes);

        WriteBool("Respawn", "IndividualTrueDeath", _settings.individualTrueDeathRespawn);
        WriteBool("Respawn", "PartyWipe", _settings.partyWipeRespawn);
        WriteBool("Respawn", "SoloDefeat", _settings.soloDefeatRespawn);
        WriteFloat("Respawn", "TrueDeathDelaySeconds", _settings.trueDeathDelaySeconds);
        WriteFloat("Respawn", "PartyWipeDelaySeconds", _settings.partyWipeDelaySeconds);
        WriteFloat("Respawn", "SoloDefeatDelaySeconds", _settings.soloDefeatDelaySeconds);

        WriteBool("Debug", "EnableHotkeys", _settings.debugHotkeys);
    }
}
