#include "PCH.h"
#include "AcheronBridge.h"

namespace AcheronTogether
{
    namespace
    {
        using VM = RE::BSScript::Internal::VirtualMachine;
        using CallbackPtr = RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor>;

        template <class... Args>
        bool DispatchStatic(const char* functionName, Args... args)
        {
            auto* vm = VM::GetSingleton();
            if (!vm) {
                return false;
            }

            auto packed = RE::MakeFunctionArguments(std::move(args)...);
            CallbackPtr callback{};
            return vm->DispatchStaticCall("Acheron", functionName, packed, callback);
        }
    }

    AcheronBridge& AcheronBridge::GetSingleton()
    {
        static AcheronBridge instance;
        return instance;
    }

    bool AcheronBridge::Initialize()
    {
        auto* data = RE::TESDataHandler::GetSingleton();
        if (!data) {
            SKSE::log::error("Acheron bridge: TESDataHandler unavailable");
            return false;
        }

        _defeated = data->LookupForm<RE::BGSKeyword>(0x801, "Acheron.esm");
        _pacified = data->LookupForm<RE::BGSKeyword>(0x802, "Acheron.esm");

        if (!_defeated || !_pacified) {
            SKSE::log::error(
                "Acheron bridge: required Acheron.esm keywords missing defeated={} pacified={}",
                _defeated ? 1 : 0,
                _pacified ? 1 : 0);
            return false;
        }

        SKSE::log::info("Acheron bridge ready");
        return true;
    }

    bool AcheronBridge::IsReady() const noexcept
    {
        return _defeated && _pacified;
    }

    bool AcheronBridge::HasKeyword(RE::Actor* actor, RE::BGSKeyword* keyword) const
    {
        if (!actor || !keyword) {
            return false;
        }
        auto* ref = actor->GetObjectReference();
        auto* form = ref ? ref->As<RE::BGSKeywordForm>() : nullptr;
        return form && form->HasKeyword(keyword);
    }

    AcheronState AcheronBridge::ReadState(RE::Actor* actor) const
    {
        if (HasKeyword(actor, _defeated)) {
            return AcheronState::kDefeated;
        }
        if (HasKeyword(actor, _pacified)) {
            return AcheronState::kPacified;
        }
        return AcheronState::kNormal;
    }

    bool AcheronBridge::DispatchActorCall(const char* functionName, RE::Actor* actor) const
    {
        if (!actor) {
            return false;
        }
        if (!DispatchStatic(functionName, actor)) {
            SKSE::log::warn("Acheron bridge: failed to dispatch Acheron.{} for {:08X}", functionName, actor->GetFormID());
            return false;
        }
        return true;
    }

    bool AcheronBridge::DispatchRescue(RE::Actor* actor, bool undoPacify) const
    {
        if (!actor) {
            return false;
        }
        if (!DispatchStatic("RescueActor", actor, undoPacify)) {
            SKSE::log::warn("Acheron bridge: failed to dispatch Acheron.RescueActor for {:08X}", actor->GetFormID());
            return false;
        }
        return true;
    }

    bool AcheronBridge::SetConsequenceDisabled(bool disabled) const
    {
        if (!DispatchStatic("DisableConsequence", disabled)) {
            SKSE::log::warn("Acheron bridge: failed to dispatch Acheron.DisableConsequence({})", disabled ? 1 : 0);
            return false;
        }

        SKSE::log::info("Acheron consequences {} for multiplayer session", disabled ? "disabled" : "restored");
        return true;
    }

    bool AcheronBridge::ApplyState(RE::Actor* actor, AcheronState state) const
    {
        if (!IsReady() || !actor) {
            return false;
        }

        const auto current = ReadState(actor);
        if (current == state) {
            return true;
        }

        SKSE::log::info(
            "ACHNET APPLY actor={:08X} current={} target={}",
            actor->GetFormID(),
            static_cast<unsigned>(current),
            static_cast<unsigned>(state));

        switch (state) {
        case AcheronState::kNormal:
            return current == AcheronState::kDefeated ?
                DispatchRescue(actor, true) :
                DispatchActorCall("ReleaseActor", actor);

        case AcheronState::kPacified:
            return current == AcheronState::kDefeated ?
                DispatchRescue(actor, false) :
                DispatchActorCall("PacifyActor", actor);

        case AcheronState::kDefeated:
            return DispatchActorCall("DefeatActor", actor);

        default:
            return false;
        }
    }
}
