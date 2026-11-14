# World

## Intent

The server keeps the map tidy: abandoned dropped items, empty vehicles nobody is near and bodies are swept on a timer, so 40 players over a long session do not pile up entities.

## Interaction pattern

None: it is a server service. Nothing a player owns and keeps (an owned vehicle, a named mission entity) is ever touched.

## Cleanup (#690)

- **Player sees:** old litter, wrecks and bodies vanish once no player is near; nothing owned does.
- **Owner and side:** `LF_Cleanup` runs on the server, started from `LF_GameModeRoleplay.OnGameStart` when the `World` system is on.
- **Data:** `Configs/World/LF_CleanupConfig.conf` (a rule per kind: age without a player near, distance to the nearest player, damage state, exempt prefab parts) with the owner's `cleanup` section over it. The system switch is `World`. Nothing is persisted; a saved vehicle record is never touched.
- **Proof:** `cleanup/sweep` (WORLD), `cleanup/rule` (LOGIC), `cleanup/persist` (PERSISTENCE). Each sweep logs `[LFDebug:Cleanup]` lines.
