#include "PCH.h"
#include "Papyrus.h"
#include "Settings.h"
#include "StateSync.h"

namespace AcheronTogether::Papyrus
{
    namespace
    {
        bool GetBool(RE::StaticFunctionTag*, std::int32_t setting)
        {
            return Settings::GetSingleton().GetBool(setting);
        }

        void SetBool(RE::StaticFunctionTag*, std::int32_t setting, bool value)
        {
            Settings::GetSingleton().SetBool(setting, value);
        }

        float GetFloat(RE::StaticFunctionTag*, std::int32_t setting)
        {
            return Settings::GetSingleton().GetFloat(setting);
        }

        void SetFloat(RE::StaticFunctionTag*, std::int32_t setting, float value)
        {
            Settings::GetSingleton().SetFloat(setting, value);
        }

        void ReloadSettings(RE::StaticFunctionTag*)
        {
            Settings::GetSingleton().Load();
        }

        void ResetDefaults(RE::StaticFunctionTag*)
        {
            Settings::GetSingleton().ResetDefaults();
        }

        void RequestCheckpoint(RE::StaticFunctionTag*)
        {
            StateSync::GetSingleton().RequestCheckpoint();
        }

        void SimulateDefeat(RE::StaticFunctionTag*)
        {
            StateSync::GetSingleton().RequestSimulatedDefeat();
        }

        void SimulateTrueDeath(RE::StaticFunctionTag*)
        {
            StateSync::GetSingleton().RequestSimulatedTrueDeath();
        }
    }

    bool RegisterFunctions(RE::BSScript::IVirtualMachine* vm)
    {
        if (!vm) {
            return false;
        }

        constexpr auto kClass = "AcheronTogetherNative"sv;
        vm->RegisterFunction("GetBool", kClass, GetBool);
        vm->RegisterFunction("SetBool", kClass, SetBool);
        vm->RegisterFunction("GetFloat", kClass, GetFloat);
        vm->RegisterFunction("SetFloat", kClass, SetFloat);
        vm->RegisterFunction("ReloadSettings", kClass, ReloadSettings);
        vm->RegisterFunction("ResetDefaults", kClass, ResetDefaults);
        vm->RegisterFunction("RequestCheckpoint", kClass, RequestCheckpoint);
        vm->RegisterFunction("SimulateDefeat", kClass, SimulateDefeat);
        vm->RegisterFunction("SimulateTrueDeath", kClass, SimulateTrueDeath);

        SKSE::log::info("ACHMCM registered native Papyrus bridge");
        return true;
    }
}
