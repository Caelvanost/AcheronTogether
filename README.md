# Acheron Together

Acheron Together is an experimental compatibility plugin that synchronizes Acheron defeat state between players in Skyrim Together Reborn.

Current development version: **v0.1.0**.

## v0.1.0 scope

The first milestone synchronizes the local player's Acheron state to the corresponding STR remote-player proxy on the other client.

States currently mirrored:

- normal / rescued
- pacified
- defeated

The plugin intentionally synchronizes only player state. Consequence quests, teleport/resolution selection, inventory transfer, Hunter Pride actions and other Acheron systems are not synchronized yet.

## Architecture

```text
Acheron local player state
        ↓
AcheronTogether.dll
        ↓
STRPluginMessagingAPI.dll
        ↓
official Skyrim Together Reborn connection
        ↓
STRPM ProxyResolver
        ↓
remote player's local STR proxy
        ↓
Acheron Papyrus API
```

Acheron Together is **STRPM-only**. There is no custom UDP transport and no fallback network stack.

The plugin reads Acheron's runtime keywords from `Acheron.esm`:

```text
0x801  Defeated
0x802  Pacified
```

Remote state is applied through Acheron's public native Papyrus functions (`DefeatActor`, `RescueActor`, `PacifyActor`, `ReleaseActor`) instead of directly mutating Acheron's internal data structures.

## Networking behavior

The STRPM channel is:

```text
acherontogether
```

Wire payload v1:

```text
AT1|<revision>|<state>
```

State values:

```text
0 = normal
1 = pacified
2 = defeated
```

Messages use reliable + ordered STRPM delivery. A five-second heartbeat resends the current local state so late joins/reconnects can converge even when the player state has not changed recently.

Incoming messages are keyed by the authenticated STRPM sender `ConnectionID`. The STRPM ProxyResolver converts that identity to the current local proxy FormID. Messages received before a proxy mapping exists are retained and applied when the mapping becomes available.

All Skyrim object lookup and Acheron calls are dispatched on the game thread.

## Requirements

Development target:

- Skyrim Special Edition / Anniversary Edition 1.6.1170
- SKSE64 2.2.6
- Skyrim Together Reborn 1.8.0
- STRPluginMessagingAPI 0.8.x with ProxyResolver
- Acheron (current 1.11.x line)

Acheron and STRPluginMessagingAPI must be installed on every client.

## Build

The project uses CommonLibSSE-NG through vcpkg.

```powershell
.\build_release.bat
```

The script reads the version from `VERSION` and creates:

```text
dist/AcheronTogether-v0.1.0.zip
```

Archive layout:

```text
SKSE/
└─ Plugins/
   └─ AcheronTogether.dll
```

## First two-client test pass

1. Install the same Acheron version, STRPM version and Acheron Together build on both clients.
2. Start STR and join the same server.
3. Confirm both `AcheronTogether.log` files contain `ACHNET STRPM READY` and a `ACHNET PROXY` line for the other player.
4. Defeat Player 1 and verify Player 2 sees Player 1's proxy enter Acheron's defeated/bleedout state.
5. Rescue Player 1 and verify the remote proxy returns to normal.
6. Repeat Player 2 -> Player 1.
7. Test a pacified-only transition if the current Acheron setup exposes one.
8. Disconnect/reconnect one client while the other player is defeated and verify state converges after proxy resolution/heartbeat.

For initial testing, keep Acheron consequence teleport/resolution features conservative. v0.1.0 synchronizes defeat state only; it does not coordinate consequence quest selection between clients.

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
```

## Development direction

Likely next milestones after the state baseline is validated:

- authoritative handling of player defeat/rescue transitions
- shared all-players-defeated detection
- consequence ownership (host/leader authority)
- synchronized consequence/resolution selection
- safe teleport coordination
- recovery/revive rules compatible with STR
