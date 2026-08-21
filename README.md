# Acheron Together

Acheron Together is an experimental SKSE/CommonLibSSE-NG plugin that turns Acheron into a multiplayer defeat/death layer for Skyrim Together Reborn.

Current development version: **v0.2.1**.

## v0.2.1 scope

v0.2.1 keeps the v0.2.0 multiplayer respawn framework and fixes the initial CommonLibSSE-NG 3.5.3 / MSVC build incompatibilities found during the first local compile pass.

Current functionality:

- synchronized Acheron `normal`, `pacified` and `defeated` states;
- synchronized true Skyrim `dead` state;
- local runtime checkpoints;
- controlled respawn after a true death, including lethal killmoves that bypass Acheron;
- multiplayer party-wipe detection;
- coordinated local recovery/respawn when every known STR player is incapacitated;
- Acheron consequence suppression while another STR player is present.

A player is considered **incapacitated** when they are either Acheron `Defeated` or truly `Dead`.

## Intended gameplay

### One player is defeated

```text
Player 1 = Defeated
Player 2 = Alive
        ↓
No respawn
        ↓
Player 2 can remain active / rescue Player 1
```

### A player suffers a lethal killmove

```text
Killmove / true Skyrim death
        ↓
AT2 dead state is published
        ↓
short killmove/death grace period
        ↓
local resurrection
        ↓
return to local checkpoint
```

### Party wipe

```text
Player 1 = Defeated or Dead
Player 2 = Defeated or Dead
        ↓
all known players incapacitated
        ↓
party-wipe grace period
        ↓
each client recovers its local player
        ↓
each client returns to its own checkpoint
```

This makes normal defeat different from a true lethal execution while still avoiding Skyrim's normal save-reload death loop.

## Architecture

```text
Acheron + Skyrim local state
        ↓
AcheronTogether.dll
        ↓
STRPluginMessagingAPI.dll
        ↓
official Skyrim Together Reborn connection
        ↓
STRPM ProxyResolver
        ↓
remote-player state table
        ↓
Acheron proxy state + party wipe evaluation
```

Acheron Together is **STRPM-only**. There is no custom UDP transport and no fallback network stack.

Remote Acheron state is applied through Acheron's public Papyrus API (`DefeatActor`, `RescueActor`, `PacifyActor`, `ReleaseActor`) rather than manually editing Acheron's internal victim tables.

## Acheron consequences

When another STR player is present Acheron Together requests:

```text
Acheron.DisableConsequence(true)
```

This prevents a local single-player Acheron consequence from taking control before the shared party state is known.

Acheron consequences are restored when the player returns to a solo state or Acheron Together stops.

The current gameplay loop therefore prioritizes **co-op rescue / party respawn** over Acheron's normal consequence quests during multiplayer sessions.

## Checkpoints

Acheron Together creates its own invisible runtime `XMarker` and moves it to the local player whenever a checkpoint is updated. It does not depend on `PartyBleedoutCheck.esp` or BCBS Respawn Patch.

A checkpoint is updated:

- when the runtime checkpoint system initializes;
- when F5 is pressed;
- two seconds after a cell transition involving an interior cell;
- every five real-time minutes outdoors while the player is not in combat.

Exterior-to-exterior cell changes do not create checkpoints.

No checkpoint is updated while the local player is `Defeated` or `Dead`.

Each client owns its own checkpoint. This is deliberate: STR clients can have different local cell/proxy timing, so the respawn destination is always a position valid in that client's game state.

## Network protocol

Channel:

```text
acherontogether
```

v0.2.1 wire payload:

```text
AT2|<revision>|<acheron-state>|<dead>
```

Acheron state values:

```text
0 = normal
1 = pacified
2 = defeated
```

Dead values:

```text
0 = alive
1 = true Skyrim death
```

Messages use reliable + ordered STRPM delivery. A five-second heartbeat resends the current local state for reconnect/late-proxy convergence.

The receiver still understands `AT1` packets for diagnostics, but v0.2.1 transmits `AT2` only. Both players should use the same Acheron Together version during testing.

## Requirements

Development target:

- Skyrim Special Edition / Anniversary Edition 1.6.1170
- SKSE64 2.2.6
- Skyrim Together Reborn 1.8.0
- STRPluginMessagingAPI 0.8.x with ProxyResolver
- Acheron 1.11.x

Acheron and STRPluginMessagingAPI must be installed on every client.

`Killmove Fixes` is not required by Acheron Together. If you want lethal killmoves to cause the true-death respawn path, do not globally disable NPC killmoves against the player.

## Build

```powershell
.\build_release.bat
```

The build script reads `VERSION` and creates:

```text
dist/AcheronTogether-v0.2.1.zip
```

Archive layout:

```text
SKSE/
└─ Plugins/
   └─ AcheronTogether.dll
```

## First v0.2.1 two-client test pass

1. Install the same Acheron, STRPM and Acheron Together v0.2.1 build on both clients.
2. Join the same STR server.
3. Verify `ACHNET STRPM READY` and `ACHNET PROXY` in both logs.
4. Defeat Player 1 while Player 2 remains alive: Player 1 must stay defeated and must **not** party-respawn.
5. Rescue Player 1 and verify both clients converge back to normal.
6. Repeat Player 2 -> Player 1.
7. Press F5, move away, then cause a true death/killmove. Verify the local player returns to the F5 checkpoint.
8. Defeat both players. After the wipe grace period, both should recover and return to their own checkpoints.
9. Test `Player 1 = Dead` + `Player 2 = Defeated` and verify it resolves as a party wipe.
10. Enter a dungeon, wait at least two seconds after the transition, move deeper, then force a true death and verify the entrance-area checkpoint.
11. Disconnect/reconnect one client and verify ProxyResolver removal prevents stale disconnected-player state from blocking wipe evaluation.
12. Verify there is no `AT2` packet storm; unchanged state should only heartbeat every five seconds.

## Logs

```text
Documents/My Games/Skyrim Special Edition/SKSE/AcheronTogether.log
```

Useful markers:

```text
ACHNET STRPM READY
ACHNET LOCAL
ACHNET TX
ACHNET RX
ACHNET PROXY
ACHNET APPLY
ACHRESP CHECKPOINT
ACHRESP local true death detected
ACHRESP party wipe candidate detected
ACHRESP RESPAWN
```

## Experimental status

v0.2.1 is still an experimental development build until the complete two-client test matrix has passed.

The detailed design and validation matrix are in `docs/RESPAWN-DESIGN.md`.
