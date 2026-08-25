# Acheron Together

Acheron Together is an experimental SKSE/CommonLibSSE-NG plugin that turns Acheron into a multiplayer defeat/death layer for Skyrim Together Reborn.

Current development version: **v0.2.2**.

## v0.2.2 scope

v0.2.2 keeps the multiplayer defeat/respawn framework and adds deterministic development controls so the network and respawn paths can be tested without relying on console damage or a random killmove.

Current functionality:

- synchronized Acheron `normal`, `pacified` and `defeated` states;
- synchronized true Skyrim `dead` state;
- local runtime checkpoints;
- controlled respawn after a true death, including lethal killmoves that bypass Acheron;
- multiplayer party-wipe detection;
- coordinated local recovery/respawn when every known STR player is incapacitated;
- Acheron consequence suppression while another STR player is present;
- HUD notification when a checkpoint is successfully updated;
- development hotkeys for forced Acheron defeat and simulated true death.

A player is considered **incapacitated** when they are either Acheron `Defeated` or truly `Dead`.

## Development hotkeys

These hotkeys are temporary development tools:

```text
F5 = update checkpoint
F6 = Force Defeat
F7 = Simulated True Death
```

### F6 — Force Defeat

F6 calls Acheron's normal defeat API on the local PlayerCharacter. This is the preferred way to test multiplayer defeat and party-wipe behavior.

Expected flow:

```text
F6
  ↓
Acheron.DefeatActor(local player)
  ↓
local state becomes Defeated
  ↓
AT2 state is sent to the other client
```

### F7 — Simulated True Death

F7 does not physically kill the PlayerCharacter. It sets Acheron Together's local debug-death override so the exact network/respawn pipeline used by a real lethal death can be tested deterministically.

Expected flow:

```text
F7
  ↓
local AT2 dead state = 1
  ↓
remote client receives Dead
  ↓
true-death grace period
  ↓
local respawn at checkpoint
  ↓
AT2 dead state returns to 0
```

This avoids depending on `player.kill`, `damageav`, or a random NPC killmove during development.

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

## Checkpoints

Acheron Together creates its own invisible runtime `XMarker` and moves it to the local player whenever a checkpoint is updated. It does not depend on `PartyBleedoutCheck.esp` or BCBS Respawn Patch.

A checkpoint is updated:

- when the runtime checkpoint system initializes;
- when F5 is pressed;
- two seconds after a cell transition involving an interior cell;
- every five real-time minutes outdoors while the player is not in combat.

Exterior-to-exterior cell changes do not create checkpoints.

No checkpoint is updated while the local player is `Defeated` or `Dead`.

Every successful checkpoint update now displays:

```text
Checkpoint updated.
```

and writes a detailed `ACHRESP CHECKPOINT reason=...` line to `AcheronTogether.log`.

Each client owns its own checkpoint.

## Network protocol

Channel:

```text
acherontogether
```

Wire payload:

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
1 = true or simulated death
```

Messages use reliable + ordered STRPM delivery. A five-second heartbeat resends the current local state for reconnect/late-proxy convergence.

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
dist/AcheronTogether-v0.2.2.zip
```

## Recommended v0.2.2 two-client test pass

1. Install v0.2.2 on both clients and join the same STR server.
2. Press F5 on both clients and verify the `Checkpoint updated.` HUD notification.
3. Move both players away from their checkpoints.
4. Press F6 on Player 1 only. Player 1 should become Acheron Defeated; Player 2 should remain active; no respawn should occur.
5. Press F6 on Player 2. Both players are now Defeated; the party wipe should trigger and both local players should return to their own checkpoint.
6. Reset to normal, then press F7 on Player 1 while Player 2 stays alive. Player 1 should enter the simulated Dead network state and individually respawn at its checkpoint.
7. Press F7 on Player 1 while Player 2 is Defeated. This should resolve as a party wipe.
8. Verify both logs contain the expected `ACHDEBUG`, `ACHNET`, and `ACHRESP` sequence.

## Logs

```text
Documents/My Games/Skyrim Special Edition/SKSE/AcheronTogether.log
```

Useful markers:

```text
ACHDEBUG F6 force defeat requested
ACHDEBUG F7 simulated true death requested
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

v0.2.2 is still an experimental development build until the complete two-client test matrix has passed.

The detailed design and validation matrix are in `docs/RESPAWN-DESIGN.md`.
