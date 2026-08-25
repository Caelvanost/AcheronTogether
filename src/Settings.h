#pragma once

namespace AcheronTogether
{
    enum class BoolSetting : std::int32_t
    {
        kCheckpointOnSave = 0,
        kCheckpointOnInteriorTransition = 1,
        kPeriodicOutdoorCheckpoints = 2,
        kInitialCheckpoint = 3,
        kShowCheckpointNotification = 4,
        kIndividualTrueDeathRespawn = 5,
        kPartyWipeRespawn = 6,
        kDebugHotkeys = 7,
        kSoloDefeatRespawn = 8
    };

    enum class FloatSetting : std::int32_t
    {
        kOutdoorCheckpointMinutes = 0,
        kTrueDeathDelaySeconds = 1,
        kPartyWipeDelaySeconds = 2,
        kSoloDefeatDelaySeconds = 3
    };

    struct RuntimeSettings
    {
        bool checkpointOnSave{ true };
        bool checkpointOnInteriorTransition{ true };
        bool periodicOutdoorCheckpoints{ true };
        bool initialCheckpoint{ true };
        bool showCheckpointNotification{ true };
        bool individualTrueDeathRespawn{ true };
        bool partyWipeRespawn{ true };
        bool debugHotkeys{ true };
        bool soloDefeatRespawn{ true };

        float outdoorCheckpointMinutes{ 5.0f };
        float trueDeathDelaySeconds{ 1.5f };
        float partyWipeDelaySeconds{ 1.25f };
        float soloDefeatDelaySeconds{ 30.0f };
    };

    class Settings final
    {
    public:
        static Settings& GetSingleton();

        void Load();
        void ResetDefaults();

        [[nodiscard]] RuntimeSettings Snapshot() const;
        [[nodiscard]] bool GetBool(std::int32_t setting) const;
        [[nodiscard]] float GetFloat(std::int32_t setting) const;

        void SetBool(std::int32_t setting, bool value);
        void SetFloat(std::int32_t setting, float value);

    private:
        Settings() = default;
        void SaveLocked() const;

        mutable std::mutex _mutex;
        RuntimeSettings _settings{};
    };
}
