# Acheron Together

Acheron Together is an experimental SKSE/CommonLibSSE-NG plugin that turns Acheron into a multiplayer defeat/death and checkpoint layer for Skyrim Together Reborn.

Current development version: **v0.3.0**.

## v0.3.0 highlights

v0.3.0 replaces the old F5 polling checkpoint design with event-driven checkpoints and adds a SkyUI MCM.

- checkpoints can update after **real Skyrim save events** reported by SKSE;
- manual saves, quicksaves and autosaves all use the same save-event path;
- interior-transition checkpoints remain available as a configurable fallback, so dungeon entrances still work even when autosaves are disabled;
- periodic outdoor checkpoints are configurable;
- checkpoint notifications can be enabled/disabled;
- individual true-death and party-wipe respawn can be enabled/disabled separately;
- respawn delays are configurable;
- a lightweight ESL-flagged `AcheronTogether.esp` hosts the SkyUI MCM only;
- the DLL remains responsible for gameplay and network logic;
- F6/F7 debug tests are now deterministic logical simulations, with equivalent buttons in the MCM.

## Intended gameplay

A player is considered **incapacitated** when they are either Acheron `Defeated` or truly `Dead`.

### One player is defeated

```text
Player 1 = Defeated
Player 2 = Alive
        ↓
No party respawn
        ↓
Player 2 remains active / may rescue Player 1
```

### True death

```text
Player 1 = Dead
Player 2 = Alive
        ↓
true-death delay
        ↓
Player 1 resurrects locally
        ↓
Player 1 returns to Player 1's local checkpoint
```

### Party wipe

```text
Player 1 = Defeated or Dead
Player 2 = Defeated or Dead
        ↓
all known STR players incapacitated
        ↓
party-wipe delay
        ↓
each client recovers its local player
        ↓
each client returns to its own checkpoint
```

## Checkpoints

Each client owns one invisible runtime `XMarker`. Acheron Together moves that marker when a checkpoint is successfully updated.

Default triggers:

- initial checkpoint after loading/new game;
- every SKSE `kSaveGame` event;
- two seconds after a cell transition involving an interior;
- every five real-time minutes outdoors while not in combat.

A checkpoint is never updated while the local player is `Defeated` or `Dead`.

When notifications are enabled, a successful update displays:

```text
Checkpoint updated.
```

### Why save events instead of F5?

F5 is only one possible way to save Skyrim and polling it was unreliable during quicksave processing. v0.3.0 listens to Skyrim/SKSE's actual save notification instead. Therefore manual saves, quicksaves and autosaves share the same path.

The interior-transition trigger intentionally remains separate. If Skyrim autosaves are disabled, entering a dungeon can still become a checkpoint.

## MCM

`AcheronTogether.esp` is a small ESL-flagged plugin containing only a Start Game Enabled quest used to host the SkyUI MCM.

### Checkpoints

- **On every Skyrim save**
- **On interior transition**
- **Initial checkpoint after load**
- **Show checkpoint notification**
- **Periodic outdoor checkpoint**
- **Outdoor interval**
- **Update checkpoint now**

### Respawn

- **Individual true-death respawn**
- **True-death delay**
- **Party-wipe respawn**
- **Party-wipe delay**

### Debug

- **Enable debug hotkeys**
- **F6 — Simulated Defeat**
- **F7 — Simulated True Death**
- **Simulate Defeat now**
- **Simulate True Death now**

All MCM settings are persisted in:

```text
Data/SKSE/Plugins/AcheronTogether.ini
```

The DLL reads that INI directly, so the core runtime does not depend on the MCM being open.

## Debug semantics

### F6 / Simulated Defeat

F6 now forces Acheron Together's local network state to `Defeated` for deterministic party-wipe testing. The plugin also asks Acheron to enter its real defeated state, but the logical override is authoritative for the test even if another mod prevents the visible bleedout transition.

This means F6 is useful even when the player remains visibly controllable.

Expected two-client test:

```text
P1 F6
→ P1 network state = Defeated
→ P2 alive
→ no respawn

P2 F6
→ P2 network state = Defeated
→ both clients know all players are incapacitated
→ party wipe
→ both return to their checkpoints
```

### F7 / Simulated True Death

F7 does not damage the player. It forces `dead=1` in Acheron Together, exercises the same individual respawn pipeline, then clears the override after respawn.

To test it visibly:

1. create a checkpoint;
2. move well away;
3. press F7;
4. verify the local player returns to the checkpoint after the configured delay.

## Network protocol

Channel:

```text
acherontogether
```

Wire payload:

```text
AT2|<revision>|<acheron-state>|<dead>
```

Acheron state:

```text
0 = normal
1 = pacified
2 = defeated
```

Dead state:

```text
0 = alive
1 = true Skyrim death / debug true-death override
```

Messages use reliable + ordered STRPM delivery. A five-second heartbeat resends the current local state for reconnect and late-proxy convergence.

Acheron Together is **STRPM-only**. There is no custom UDP transport.

## Acheron integration

Remote Acheron proxy states are applied through Acheron's public Papyrus functions:

- `DefeatActor`
- `RescueActor`
- `PacifyActor`
- `ReleaseActor`

When another STR player is present, Acheron Together requests:

```text
Acheron.DisableConsequence(true)
```

This prioritizes the multiplayer rescue / party-respawn loop over local single-player Acheron consequence quests. Consequences are restored when returning to a solo state or when Acheron Together stops.

## Requirements

Development target:

- Skyrim Special Edition / Anniversary Edition 1.6.1170
- SKSE64 2.2.6
- Skyrim Together Reborn 1.8.0
- STRPluginMessagingAPI 0.8.x with ProxyResolver
- Acheron 1.11.x
- SkyUI for the MCM

The DLL can still read `AcheronTogether.ini` directly, but `AcheronTogether.esp`/`AcheronTogetherMCM.pex` require SkyUI at runtime.

## Build

```powershell
.\build_release.bat
```

The Windows release script performs six stages:

1. configure CMake;
2. build `AcheronTogether.dll`;
3. compile the native/MCM Papyrus scripts with Bethesda's Papyrus compiler;
4. generate `AcheronTogether.esp` from tracked Spriggit YAML;
5. stage the default INI;
6. create the Vortex ZIP.

The script looks for Skyrim in common locations or uses `SKYRIM_ROOT` if set. SkyUI source scripts must be available under Skyrim's `Data/Source/Scripts` so `SKI_ConfigBase.psc` can be compiled against.

Spriggit CLI **0.40.1** is pinned by the repository. If the CLI is not already present under `build/tools`, the release script downloads the pinned `SpriggitCLI.zip` from the official Spriggit GitHub release and uses it to deserialize the tracked YAML into the lightweight ESP.

Expected output:

```text
dist/AcheronTogether-v0.3.0.zip
```

Archive layout:

```text
AcheronTogether.esp
Scripts/
├─ AcheronTogetherMCM.pex
└─ AcheronTogetherNative.pex
SKSE/
└─ Plugins/
   ├─ AcheronTogether.dll
   └─ AcheronTogether.ini
```

## First v0.3.0 validation pass

### Local checkpoint / MCM

1. Install v0.3.0.
2. Open the **Acheron Together** MCM and verify all three pages are visible.
3. Leave `On every Skyrim save` and notifications enabled.
4. Move somewhere recognizable and make a manual save.
5. Verify `Checkpoint updated.` appears.
6. Move again and quicksave; verify another update.
7. Trigger an autosave or enter a dungeon; verify the save or interior-transition trigger creates a checkpoint.
8. Disable notifications in the MCM and verify checkpoints continue to log without HUD text.
9. Use `Update checkpoint now` and verify the manual MCM trigger.
10. Move away, use `Simulate True Death now`, and verify the return to checkpoint.

### Two-client party wipe

1. Install the same v0.3.0 build on both clients.
2. Connect to the same STR server and verify `ACHNET PROXY` on both clients.
3. Create a checkpoint on both clients by saving.
4. On P1 use F6 or the MCM `Simulate Defeat now` button.
5. Verify no respawn while P2 remains alive.
6. On P2 simulate Defeat.
7. Verify both clients log `ACHRESP party wipe candidate detected`.
8. Verify both clients return their local player to their own checkpoint.

## Logs

```text
Documents/My Games/Skyrim Special Edition/SKSE/AcheronTogether.log
```

Useful markers:

```text
ACHCFG
ACHMCM
ACHNET STRPM READY
ACHNET LOCAL
ACHNET TX
ACHNET RX
ACHNET PROXY
ACHNET APPLY
ACHRESP save event queued checkpoint update
ACHRESP CHECKPOINT reason=save
ACHRESP CHECKPOINT reason=cell-transition
ACHRESP party wipe candidate detected
ACHRESP RESPAWN
ACHDEBUG simulated defeat requested
ACHDEBUG simulated true death requested
```

## Experimental status

v0.3.0 is a development build. Save-driven checkpoints and the MCM architecture should be validated locally before the full two-client matrix is considered stable.

The lower-level respawn design notes remain in `docs/RESPAWN-DESIGN.md`.
