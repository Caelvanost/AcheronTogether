Scriptname AcheronTogetherMCM extends SKI_ConfigBase

Int BOOL_SAVE = 0
Int BOOL_INTERIOR = 1
Int BOOL_OUTDOOR = 2
Int BOOL_INITIAL = 3
Int BOOL_NOTIFY = 4
Int BOOL_TRUE_DEATH = 5
Int BOOL_PARTY_WIPE = 6
Int BOOL_DEBUG_HOTKEYS = 7

Int FLOAT_OUTDOOR_MINUTES = 0
Int FLOAT_TRUE_DEATH_DELAY = 1
Int FLOAT_PARTY_WIPE_DELAY = 2

Int oidSave = -1
Int oidInterior = -1
Int oidOutdoor = -1
Int oidInitial = -1
Int oidNotify = -1
Int oidOutdoorMinutes = -1
Int oidCheckpointNow = -1

Int oidTrueDeath = -1
Int oidPartyWipe = -1
Int oidTrueDeathDelay = -1
Int oidPartyWipeDelay = -1

Int oidDebugHotkeys = -1
Int oidSimDefeat = -1
Int oidSimDeath = -1

Event OnConfigInit()
    ModName = "Acheron Together"
    Pages = New String[3]
    Pages[0] = "Checkpoints"
    Pages[1] = "Respawn"
    Pages[2] = "Debug"
EndEvent

Int Function GetVersion()
    Return 1
EndFunction

Event OnPageReset(String page)
    oidSave = -1
    oidInterior = -1
    oidOutdoor = -1
    oidInitial = -1
    oidNotify = -1
    oidOutdoorMinutes = -1
    oidCheckpointNow = -1
    oidTrueDeath = -1
    oidPartyWipe = -1
    oidTrueDeathDelay = -1
    oidPartyWipeDelay = -1
    oidDebugHotkeys = -1
    oidSimDefeat = -1
    oidSimDeath = -1

    If page == ""
        page = "Checkpoints"
    EndIf

    SetCursorFillMode(TOP_TO_BOTTOM)

    If page == "Checkpoints"
        AddHeaderOption("Checkpoint triggers")
        oidSave = AddToggleOption("On every Skyrim save", AcheronTogetherNative.GetBool(BOOL_SAVE))
        oidInterior = AddToggleOption("On interior transition", AcheronTogetherNative.GetBool(BOOL_INTERIOR))
        oidInitial = AddToggleOption("Initial checkpoint after load", AcheronTogetherNative.GetBool(BOOL_INITIAL))
        oidNotify = AddToggleOption("Show checkpoint notification", AcheronTogetherNative.GetBool(BOOL_NOTIFY))

        AddHeaderOption("Outdoor checkpoints")
        oidOutdoor = AddToggleOption("Periodic outdoor checkpoint", AcheronTogetherNative.GetBool(BOOL_OUTDOOR))
        oidOutdoorMinutes = AddSliderOption("Outdoor interval", AcheronTogetherNative.GetFloat(FLOAT_OUTDOOR_MINUTES), "{1} min")

        AddHeaderOption("Manual")
        oidCheckpointNow = AddTextOption("Update checkpoint now", "Run")

    ElseIf page == "Respawn"
        AddHeaderOption("True death")
        oidTrueDeath = AddToggleOption("Individual true-death respawn", AcheronTogetherNative.GetBool(BOOL_TRUE_DEATH))
        oidTrueDeathDelay = AddSliderOption("True-death delay", AcheronTogetherNative.GetFloat(FLOAT_TRUE_DEATH_DELAY), "{2} s")

        AddHeaderOption("Party wipe")
        oidPartyWipe = AddToggleOption("Party-wipe respawn", AcheronTogetherNative.GetBool(BOOL_PARTY_WIPE))
        oidPartyWipeDelay = AddSliderOption("Party-wipe delay", AcheronTogetherNative.GetFloat(FLOAT_PARTY_WIPE_DELAY), "{2} s")

    ElseIf page == "Debug"
        AddHeaderOption("Development tools")
        oidDebugHotkeys = AddToggleOption("Enable debug hotkeys", AcheronTogetherNative.GetBool(BOOL_DEBUG_HOTKEYS))
        AddTextOption("F6", "Simulated Defeat", OPTION_FLAG_DISABLED)
        AddTextOption("F7", "Simulated True Death", OPTION_FLAG_DISABLED)
        oidSimDefeat = AddTextOption("Simulate Defeat now", "Run")
        oidSimDeath = AddTextOption("Simulate True Death now", "Run")
    EndIf
EndEvent

Event OnOptionSelect(Int option)
    Bool value

    If option == oidSave
        value = !AcheronTogetherNative.GetBool(BOOL_SAVE)
        AcheronTogetherNative.SetBool(BOOL_SAVE, value)
        SetToggleOptionValue(option, value)
    ElseIf option == oidInterior
        value = !AcheronTogetherNative.GetBool(BOOL_INTERIOR)
        AcheronTogetherNative.SetBool(BOOL_INTERIOR, value)
        SetToggleOptionValue(option, value)
    ElseIf option == oidOutdoor
        value = !AcheronTogetherNative.GetBool(BOOL_OUTDOOR)
        AcheronTogetherNative.SetBool(BOOL_OUTDOOR, value)
        SetToggleOptionValue(option, value)
    ElseIf option == oidInitial
        value = !AcheronTogetherNative.GetBool(BOOL_INITIAL)
        AcheronTogetherNative.SetBool(BOOL_INITIAL, value)
        SetToggleOptionValue(option, value)
    ElseIf option == oidNotify
        value = !AcheronTogetherNative.GetBool(BOOL_NOTIFY)
        AcheronTogetherNative.SetBool(BOOL_NOTIFY, value)
        SetToggleOptionValue(option, value)
    ElseIf option == oidTrueDeath
        value = !AcheronTogetherNative.GetBool(BOOL_TRUE_DEATH)
        AcheronTogetherNative.SetBool(BOOL_TRUE_DEATH, value)
        SetToggleOptionValue(option, value)
    ElseIf option == oidPartyWipe
        value = !AcheronTogetherNative.GetBool(BOOL_PARTY_WIPE)
        AcheronTogetherNative.SetBool(BOOL_PARTY_WIPE, value)
        SetToggleOptionValue(option, value)
    ElseIf option == oidDebugHotkeys
        value = !AcheronTogetherNative.GetBool(BOOL_DEBUG_HOTKEYS)
        AcheronTogetherNative.SetBool(BOOL_DEBUG_HOTKEYS, value)
        SetToggleOptionValue(option, value)
    ElseIf option == oidCheckpointNow
        AcheronTogetherNative.RequestCheckpoint()
        SetTextOptionValue(option, "Queued")
    ElseIf option == oidSimDefeat
        AcheronTogetherNative.SimulateDefeat()
        SetTextOptionValue(option, "Queued")
    ElseIf option == oidSimDeath
        AcheronTogetherNative.SimulateTrueDeath()
        SetTextOptionValue(option, "Queued")
    EndIf
EndEvent

Event OnOptionSliderOpen(Int option)
    If option == oidOutdoorMinutes
        SetSliderDialogStartValue(AcheronTogetherNative.GetFloat(FLOAT_OUTDOOR_MINUTES))
        SetSliderDialogDefaultValue(5.0)
        SetSliderDialogRange(1.0, 60.0)
        SetSliderDialogInterval(1.0)
    ElseIf option == oidTrueDeathDelay
        SetSliderDialogStartValue(AcheronTogetherNative.GetFloat(FLOAT_TRUE_DEATH_DELAY))
        SetSliderDialogDefaultValue(1.5)
        SetSliderDialogRange(0.25, 10.0)
        SetSliderDialogInterval(0.25)
    ElseIf option == oidPartyWipeDelay
        SetSliderDialogStartValue(AcheronTogetherNative.GetFloat(FLOAT_PARTY_WIPE_DELAY))
        SetSliderDialogDefaultValue(1.25)
        SetSliderDialogRange(0.25, 10.0)
        SetSliderDialogInterval(0.25)
    EndIf
EndEvent

Event OnOptionSliderAccept(Int option, Float value)
    If option == oidOutdoorMinutes
        AcheronTogetherNative.SetFloat(FLOAT_OUTDOOR_MINUTES, value)
        SetSliderOptionValue(option, value, "{1} min")
    ElseIf option == oidTrueDeathDelay
        AcheronTogetherNative.SetFloat(FLOAT_TRUE_DEATH_DELAY, value)
        SetSliderOptionValue(option, value, "{2} s")
    ElseIf option == oidPartyWipeDelay
        AcheronTogetherNative.SetFloat(FLOAT_PARTY_WIPE_DELAY, value)
        SetSliderOptionValue(option, value, "{2} s")
    EndIf
EndEvent

Event OnOptionHighlight(Int option)
    If option == oidSave
        SetInfoText("Updates the checkpoint after SKSE reports a real Skyrim save. This includes manual saves, quicksaves and autosaves.")
    ElseIf option == oidInterior
        SetInfoText("Fallback checkpoint two seconds after a cell transition involving an interior. Useful when autosaves are disabled.")
    ElseIf option == oidOutdoor
        SetInfoText("Creates periodic checkpoints outdoors while the player is not in combat.")
    ElseIf option == oidInitial
        SetInfoText("Creates an initial checkpoint after loading or starting a game.")
    ElseIf option == oidNotify
        SetInfoText("Shows 'Checkpoint updated.' only after the checkpoint marker was successfully moved.")
    ElseIf option == oidTrueDeath
        SetInfoText("A true Skyrim death can respawn the local player at the local checkpoint instead of relying on the normal death reload loop.")
    ElseIf option == oidPartyWipe
        SetInfoText("If every known Skyrim Together player is Defeated or Dead, each client recovers its local player at its own checkpoint.")
    ElseIf option == oidDebugHotkeys
        SetInfoText("Development-only shortcuts. F6 simulates Defeated state and F7 simulates true death.")
    ElseIf option == oidSimDefeat
        SetInfoText("Queues the same simulated Defeated state used by F6. Useful for deterministic two-client party-wipe tests.")
    ElseIf option == oidSimDeath
        SetInfoText("Queues the same simulated true-death state used by F7 to test individual respawn without needing a lethal killmove.")
    ElseIf option == oidCheckpointNow
        SetInfoText("Queues an immediate checkpoint update on the game thread.")
    EndIf
EndEvent
