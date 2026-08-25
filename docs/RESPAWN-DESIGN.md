# Acheron Together Respawn Design

This document describes the multiplayer respawn architecture introduced in v0.2.0 and revised in v0.3.0.

## Goals

- Keep Acheron as the local defeat framework.
- Synchronize player defeat/death state through STRPluginMessagingAPI.
- Let a true Skyrim death, including lethal killmoves, cause a controlled respawn instead of a save reload.
- Trigger a coordinated party respawn when every known STR player is incapacitated.
- Keep checkpoints local to each client so every player returns to a position valid in their own world state.
- In v0.3.0, update checkpoints from real Skyrim save events rather than polling F5.

## Player state

A player publishes:

- Acheron state: normal, pacified, defeated;
- Skyrim dead flag.

A player is incapacitated when either `dead == true` or the Acheron state is `defeated`.

The v0.3.0 debug Defeat command can also force a local logical `defeated` override. This is intentionally independent of the visible Acheron bleedout so party-wipe tests remain deterministic.

## Consequences

When another STR player is present, Acheron Together requests `Acheron.DisableConsequence(true)`. This prevents a local single-player consequence from taking control before the multiplayer party state is known. Consequences are restored when the remote-player set becomes empty or the plugin stops.

## Respawn rules

### True death

A true local Skyrim death is published immediately. If individual true-death respawn is enabled, Acheron Together waits for the configured delay and, when possible, for the killmove to finish. A safety margin then resurrects the player and moves them to the local checkpoint.

### Party wipe

If party-wipe respawn is enabled and the local player plus every known remote STR player are `Defeated` or `Dead` continuously for the configured wipe delay, the local player is recovered and moved to the local checkpoint. Every client performs the same evaluation using its synchronized state table.

### No remote players

A solo Acheron defeat is never treated as a multiplayer party wipe. This avoids changing Acheron's normal solo defeat behavior simply because Acheron Together is installed.

## Checkpoints

Acheron Together creates an invisible runtime XMarker and moves it to the local player when a checkpoint is successfully updated.

v0.3.0 checkpoint triggers are individually configurable:

- initial checkpoint after load/new game;
- every SKSE `kSaveGame` message;
- two seconds after a cell transition involving an interior cell;
- periodic outdoor checkpoint while out of combat;
- manual MCM request.

`kSaveGame` is the authoritative save-based trigger. It is independent of which UI or key initiated the save, so manual saves, quicksaves and autosaves use the same checkpoint path.

The interior-transition trigger remains a deliberate fallback. If a player disables Skyrim autosaves, entering or leaving an interior can still create a checkpoint.

No checkpoint is updated while the local player is defeated or dead.

## MCM and persistence

v0.3.0 adds a SkyUI MCM hosted by a lightweight ESL-flagged `AcheronTogether.esp`.

The MCM configures:

- save checkpoint trigger;
- interior transition trigger;
- initial checkpoint;
- HUD checkpoint notification;
- periodic outdoor checkpoints and interval;
- individual true-death respawn and delay;
- party-wipe respawn and delay;
- debug hotkeys;
- manual checkpoint and debug test buttons.

Settings are persisted to:

```text
Data/SKSE/Plugins/AcheronTogether.ini
```

The DLL reads the INI directly. The MCM is an interface to the native settings layer rather than a second source of truth.

## Network protocol

Acheron Together publishes:

```text
AT2|<revision>|<acheron-state>|<dead>
```

where `dead` is `0` or `1`.

The old `AT1` receive format remains accepted as a non-dead state for diagnostics, but current builds transmit `AT2` only.

## v0.3.0 validation pass

1. Make a manual save and verify `ACHRESP CHECKPOINT reason=save`.
2. Quicksave and verify the same save-event path.
3. Trigger an autosave and verify the same path.
4. Disable autosaves, enter an interior, and verify the fallback `cell-transition` checkpoint.
5. Use the MCM manual checkpoint button and verify `reason=manual`.
6. Move away and simulate true death; verify individual return to the checkpoint.
7. With two clients, simulate Defeat on Player 1 only; no respawn should occur.
8. Simulate Defeat on Player 2; both clients should detect a party wipe and recover.
9. Disable party-wipe respawn in the MCM and verify the same simulated wipe no longer respawns.
10. Disconnect/reconnect one client and verify stale remote state is removed by ProxyResolver events.

This implementation remains experimental until the local and two-client validation matrix has been completed.
