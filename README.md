# Acheron Together

Acheron Together is an experimental SKSE/CommonLibSSE-NG plugin that turns Acheron into a multiplayer defeat/death and checkpoint layer for Skyrim Together Reborn.

Current development version: **v0.3.2**.

## v0.3.2 highlights

v0.3.2 adds delayed solo Defeat recovery while keeping the v0.3.1 checkpoint fixes.

- when no other STR player is present, a local `Defeated` state now starts a configurable solo respawn timer;
- the default solo Defeat delay is **30 seconds**;
- rescue/normal recovery cancels the solo timer immediately;
- true death cancels the solo timer and uses the existing true-death respawn path instead;
- connecting another STR player cancels the solo timer and restores normal multiplayer party-wipe semantics;
- the MCM exposes **Respawn after solo Defeat** and **Solo Defeat delay**;
- checkpoint marker resolution uses the Skyrim `XMarker` EditorID;
- checkpoint tracking waits for SKSE `kPostLoadGame` before becoming active;
- failed STRPM sends while disconnected are throttled to the heartbeat interval;
- saves, interior transitions and periodic outdoor checkpoints remain configurable.

## Intended gameplay

A player is considered **incapacitated** when they are either Acheron `Defeated` or truly `Dead`.

### Solo Defeat

```text
Only local player present
        ↓
Player = Defeated
        ↓
30 s default solo delay
        ↓
recover to Normal
        ↓
return to local checkpoint
```

If the player is rescued before the timer expires, the timer is cancelled. If the player becomes truly `Dead`, the true-death path takes priority.

### One player is defeated in multiplayer

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

Each client owns one invisible runtime `XMarker`. Acheron Together resolves the Skyrim static by EditorID and creates a local runtime reference only after the loaded game is ready.

Default triggers:

- initial checkpoint after `kPostLoadGame` / new game;
- every SKSE `kSaveGame` event;
- two seconds after a cell transition involving an interior;
- every five real-time minutes outdoors while not in combat.

A checkpoint is never updated while the local player is `Defeated` or `Dead`.

When notifications are enabled, a successful update displays:

```text
Checkpoint updated.
```

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

- **Respawn after solo Defeat**
- **Solo Defeat delay** — default 30 s
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

## Debug semantics

### F6 / Simulated Defeat

F6 forces Acheron Together's local state to `Defeated`. With no other STR player present, it now exercises the solo Defeat timer. With another player present, it continues to exercise multiplayer party-wipe logic.

Expected solo test:

```text
F6
→ Simulated Defeat
→ ACHRESP solo defeat detected
→ wait configured delay (30 s default)
→ ACHRESP solo defeat timeout reached
→ ACHRESP RESPAWN reason=solo-defeat
→ return to checkpoint
```

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

## Network protocol

Channel:

```text
acherontogether
```

Wire payload:

```text
AT2|<revision>|<acheron-state>|<dead>
```

Messages use reliable + ordered STRPM delivery. A five-second heartbeat resends the current local state for reconnect and late-proxy convergence. Failed sends while no STR session is connected use the same retry cadence rather than retrying every 250 ms worker tick.

Acheron Together is **STRPM-only**. There is no custom UDP transport.

## Requirements

Development target:

- Skyrim Special Edition / Anniversary Edition 1.6.1170
- SKSE64 2.2.6
- Skyrim Together Reborn 1.8.0
- STRPluginMessagingAPI 0.8.x with ProxyResolver
- Acheron 1.11.x
- SkyUI for the MCM

## Build

```powershell
.\build_release.bat
```

Expected output:

```text
dist/AcheronTogether-v0.3.2.zip
```

## v0.3.2 validation pass

### Solo Defeat

1. Install v0.3.2 and connect to an STR server with no other player in the party.
2. Create/update a checkpoint and move away from it.
3. Leave `Respawn after solo Defeat` enabled and `Solo Defeat delay` at 30 seconds.
4. Press F6 or use `Simulate Defeat now`.
5. Verify there is no immediate respawn.
6. After about 30 seconds, verify the player returns to the checkpoint.
7. Verify the log contains `ACHRESP solo defeat detected`, `ACHRESP solo defeat timeout reached`, and `ACHRESP RESPAWN reason=solo-defeat`.

### Multiplayer

1. Connect a second player.
2. Defeat only P1 and verify the solo timer is not used and no respawn occurs while P2 is alive.
3. Defeat P2 and verify the normal party-wipe path still returns both clients to their own checkpoints.

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
ACHRESP game load complete; checkpoint tracking enabled
ACHRESP checkpoint marker created
ACHRESP CHECKPOINT
ACHRESP solo defeat detected
ACHRESP solo defeat timer cancelled
ACHRESP solo defeat timeout reached
ACHRESP party wipe candidate detected
ACHRESP RESPAWN
ACHDEBUG simulated defeat requested
ACHDEBUG simulated true death requested
```

## Experimental status

v0.3.2 is a development build. Validate the new solo Defeat timer locally/alone on an STR server before running the full two-client matrix.
