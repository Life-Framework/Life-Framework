# Read your server log: success lines and the fix for each error

The server writes `console.log` under `<profile>\logs\logs_<date>_<time>\` on every start. This page is the
short version of what to look for: the lines that mean the server booted with the framework and the 1989
Everon pack, then the fifteen error lines owners meet most, each with what it means and what to do.
[Install a server from zero](install.md) gets you to the first boot; the full line-by-line list, including the
ones only a developer sees, is [`docs/debug-log-triage.md`](../debug-log-triage.md).

Search the log for the exact text in backticks below. Numbers, ids and names in a line change from server to
server; `N`, `X` and `<id>` stand for them. Judge by the line, never by a count of `(E)` lines: the base game
prints dozens of harmless ones.

## The lines of a good boot

These come in this order, with other lines between them. The mission line proves the pack's header loaded, the
`ACTIVE` line proves the framework came up, and `Entered online game state.` proves the server is open (about
10 to 15 seconds after the engine starts, once both mods have downloaded). The last line appears when the
first player gets to character creation.

```text
Config load @"{C638C03B775C31BB}Missions/LF_Everon1989.conf"
[LFDebug:Config] no owner config at $profile:LifeFramework/config.json; every section uses the shipped defaults
[LFDebug:Persistence] system state SETUP -> ACTIVE
Entered online game state.
[LFDebug:Persistence] autosave scheduled every 600 seconds
[LFDebug:Persistence] fresh session: no save point exists for this mission
[LFDebug:CharacterCreation] flow start for player 1
```

- `no owner config` is right until you create `config.json` ([where it goes](install.md#6-where-configjson-goes)).
- `fresh session: no save point exists for this mission` is right for a first boot. After a restart with
  `-loadSessionSave` it becomes `session continued from a save point (-loadSessionSave)`, then
  `save loaded: version <N>, game build <x>, running <y>`.
- A log with none of these, or without `Entered online game state.`, did not boot. A line with `SCRIPT (E)`,
  `VME` or `Virtual Machine Exception` is a failure, with one expected exception: row 7 below, on a fresh
  profile.
- `tools\cli server-doctor` reads a log and checks all of this for you ([install, step 9](install.md#9-check-a-log-with-server-doctor)).

## The fifteen commonest error lines

| Line | What it means | Fix |
|---|---|---|
| `[LFDebug:Persistence] started a NEW session while N save point(s) exist` | The server booted without `-loadSessionSave`, so your saved world was not loaded and players start from scratch. | Stop the server and start it again with `-loadSessionSave` on the launch line. Add it to your start script so every restart has it. |
| `[LFDebug:Persistence] continued save never reached the LF store (game build X) ...` | A `-loadSessionSave` boot came up with none of the framework's data loaded: the save is from before the 2026-11-09 `EL_` to `LF_` rename, or a game update made the engine drop it. Autosave is paused so the save stays as it is. | Stop the server, restore a backup of the profile (`tools\cli host restore`), and read `docs/ops/game-updates.md`. A save from before the rename is deleted once and started fresh. |
| `[LFDebug:Persistence] save was written by game build X (save version N); this server runs Y` | The save comes from another game build. Expected once after you update the server. | Nothing, if the next lines show `save loaded`. If they show `save load damaged`, see the next row. Run `host preflight` before each game update. |
| `[LFDebug:Persistence] save load damaged: N payload(s) failed to read ...` | The load lost data. Autosave is paused so it does not overwrite what is left. | Stop the server and restore the last good backup with `tools\cli host restore`; do not let it run on. |
| `[LFDebug:Persistence] saving disabled for this session: the boot load was damaged ...` | Same cause as the row above: every save is off, the shutdown save too, so `session ended without a shutdown save` follows at stop. | Restore a backup (`docs/ops/game-updates.md`) and start again. Whatever players do before the restart is not kept. |
| `[LFDebug:Persistence] autosave tick skipped: the save load was damaged` | Repeats every autosave interval after one of the two rows above, until a restart on a good save. | Fix the row above it; this line stops by itself after that restart. |
| `[LFDebug:Whitelist] whitelist file missing, police faction closed: $profile:LifeFramework/whitelist.txt` | The police roster does not exist yet. It is logged at error level on purpose, so `SCRIPT (E)` shows on a fresh profile. | Expected until the first player joins: they are put on the roster and the file is written. Nothing to fix; the police faction stays closed until the roster has a name. |
| `[LFDebug:CharacterCreation] player N has no platform identity after 60 s, so no account can open` | The server's online service never told it who the player is, so their registration form cannot save. The form shows `#LF-CC_Error_NoIdentity`. | Check the server's `publicAddress` and that the backend is reachable; the player signs the form again to retry. |
| `[LFDebug:Spawn] spawn abandoned for playerId N: UID/account did not resolve after 60 retries` | The player never got an identity or their account never loaded, so no character spawned. | Read the persistence and identity lines just above it; usually the same backend problem as the row above. The player reconnects after it is fixed. |
| `[LFDebug:Config] stream: the server sends format N, this build reads M` | A client refused the server's price list because it runs a different build, so what it shows may differ from what the server charges. | Make the server and players run the same Workshop version: update the server, restart it, let the clients update. |
| `[LFDebug:Notify] toast dropped: no LF_NotificationManagerComponent on this client` | The game mode prefab lost its notification component, so players see no toasts. | Re-add `LF_NotificationManagerComponent` to the game mode prefab. |
| `[LFDebug:Signs] sign config does not resolve` / `unexpected root class` | `LF_Signs.conf` is missing or broken, so every sign hides its Read action. | Put back a working `Configs/Signs/LF_Signs.conf`: reinstall the mod from the Workshop, or fix the file if your pack ships its own. |
| `[LFDebug:Store] store X: <item> taken off the shelf has no price, so it is free` | A shelf holds an item with no buy price, so players take it for nothing. | Give the item a buy price in `ShopPrices.conf`, or set it with [`priceRules` in the owner config](../config/owner-config.md). |
| `[LFDebug:Store] store key X is used twice` / `has no store key` | Two stores share a key, or one has none, so the second store's stock is not saved. | Give each `LF_StoreComponent` its own `m_sStoreKey` on the placed shop. A shop you built from [add a shop](add-shop.md) is one of these. |
| `Unable to start replication` / `Unable to initialize the game` | Another server is already using the port, usually a leftover `ArmaReforgerServer.exe` from an earlier run. | End the other process, or move `bindPort`, `publicPort` and `a2s.port` in `server.json` and forward the new ports, then start again. |

Anything not on this page: search `docs/debug-log-triage.md` for the line first. If it is not there either,
run [`server-doctor`](install.md#9-check-a-log-with-server-doctor) and report the line on the issue tracker.
