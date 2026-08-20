#pragma once

namespace AcheronTogether
{
    enum class AcheronState : std::uint8_t
    {
        kNormal = 0,
        kPacified = 1,
        kDefeated = 2
    };

    class AcheronBridge
    {
    public:
        static AcheronBridge& GetSingleton();

        bool Initialize();
        [[nodiscard]] bool IsReady() const noexcept;
        [[nodiscard]] AcheronState ReadState(RE::Actor* actor) const;
        bool ApplyState(RE::Actor* actor, AcheronState state) const;

    private:
        bool DispatchActorCall(const char* functionName, RE::Actor* actor) const;
        bool DispatchRescue(RE::Actor* actor, bool undoPacify) const;
        [[nodiscard]] bool HasKeyword(RE::Actor* actor, RE::BGSKeyword* keyword) const;

        RE::BGSKeyword* _defeated{ nullptr };
        RE::BGSKeyword* _pacified{ nullptr };
    };
}
