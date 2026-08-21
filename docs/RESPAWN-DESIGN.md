# Acheron Together Respawn Design

This document describes the first multiplayer respawn implementation introduced in v0.2.0.

## Goals

- Keep Acheron as the local defeat framework.
- Synchronize player defeat/death state through STRPluginMessagingAPI.
- Let a true Skyrim death (including lethal killmoves) cause a controlled respawn instead of a save reload.
- Trigger a coordinated party respawn when every known STR player is incapacitated.
- Keep checkpoints local to each client so every player returns to a valid position from their own world state.

## Player state

A player publishes two pieces of state:

- Acheron state: normal, pacified, defeated.
- Skyrim dead flag.

A player is considered incapacitated when either `dead == true` or Acheron reports `defeated`.

## Consequences

While Acheron Together is active, `Acheron.DisableConsequence(true)` is requested. This prevents a local Acheron consequence from taking control before the multiplayer party state is known. It is restored when Acheron Together stops.

## Respawn rules

### True death

A true local Skyrim death is published immediately. Acheron Together waits for a killmove to finish when possible, then resurrects the player and moves them to the local checkpoint. A safety timeout prevents the vanilla death/load flow from winning indefinitely.

### Party wipe

If the local player and every known remote STR player are `Defeated` or `Dead` continuously for the wipe grace period, the local player is recovered and moved to the local checkpoint. Every client performs the same deterministic evaluation using the synchronized state table.

### No remote players

Acheron Together does not treat a solo Acheron defeat as a multiplayer party wipe. This avoids changing Acheron's solo behavior simply because the plugin is installed.

## Checkpoints

Acheron Together creates an invisible persistent runtime XMarker and moves it to the local player when a checkpoint is updated.

Checkpoint updates currently occur:

- when the runtime marker is first created;
- on F5 rising edge;
- two seconds after a cell transition involving an interior cell;
- every five real-time minutes outdoors, while out of combat.

Exterior-to-exterior cell boundaries do not create checkpoints.

No checkpoint is updated while the local player is defeated or dead.

## Network protocol

v0.2.0 publishes:

```text
AT2|<revision>|<acheron-state>|<dead>
```

where `dead` is `0` or `1`.

The old `AT1` receive format is still accepted as a non-dead state for diagnostics, but v0.2.0 transmits `AT2` only.

## First validation pass

1. Normal defeat on Player 1 while Player 2 is alive: no respawn; Player 2 should be able to remain active.
2. Rescue Player 1: both clients converge to normal.
3. Lethal killmove on Player 1: Player 1 should publish dead and respawn at their checkpoint.
4. Defeat both players: both should detect a party wipe and return to their local checkpoints.
5. Player 1 dead + Player 2 defeated: both should converge through the wipe path.
6. Press F5, move away, force a true death, and verify the return position.
7. Enter a dungeon, wait for the delayed checkpoint update, move deeper, then force a true death.
8. Disconnect/reconnect one client and verify stale remote state is removed by ProxyResolver events.

This implementation is intentionally experimental until the two-client test matrix has been completed.