# Debug log triage

What every line in a DebugWorld boot log means, and what to do about it. Read
this BEFORE investigating any log message — most lines are either fixed,
benign vanilla noise, or by design. Re-proving that wastes a session.

**Keep this file current in the same change that alters a feature.** A message
that leaves this table without a verdict reads as an open bug to the next
agent. When a new message shows up in a boot log: grep this file first; if
absent, classify it, fix or file it, and add the row here.

## Where logs live

- Workbench play: `Documents/My Games/ArmaReforger/logs/<date>/console.log`
  and `error.log` (or the session list via the MCP `logs_list` / `logs_tail`).
- Headless server (`cli dev` / `cli test`): the CLI parses the ELTEST markers
  and dumps `[ELDebug:*]` lines itself.
- Grep for real failures: `( E )`, `SCRIPT (E)`, `VME:`, `Unable to find`,
  `Missing string ID`, `Virtual Machine Exception`. Judge by specific patterns,
  never by total error counts (vanilla noise below produces dozens of lines
  that are harmless).

## Triage table

Verdict legend: **FIXED** = was a mod bug, now resolved; **OPEN** = known mod
bug, still to fix; **VANILLA** = base-game behavior/noise, do not touch;
**BY DESIGN** = intended mod behavior; **INFO** = informational.

| Message (pattern) | Verdict | What it is / what to do |
|---|---|---|
| `[ELDebug:Persistence] save point FAILED` | FIXED | Mod requested a `SHUTDOWN` save from `OnGameEnd`, but the engine owns that save point and was already tearing down. `EL_PersistenceManagerComponent.OnGameEnd` no longer requests SHUTDOWN; the engine save is observed via `OnAfterSave`. |
| `[ELDebug:Persistence] no SCR_PersistenceSystem for this world` | FIXED | A mod `ChimeraSystemsConfig.conf` shadowed the vanilla GUID `{86E953538A28A98D}`, making system-config resolution flaky between boots. Now `LifeFrameworkSystems.conf` (fresh GUID `{2104C177A245B6D1}`) inherits vanilla by GUID and always declares `SCR_PersistenceSystem`; missions set `SystemsConfig` explicitly. |
| `SCR_DamageManagerComponent ... OnPostInit ... NULL pointer to instance` on M151A2 police vehicle parts (headlights, indicators, wheels, bench, supply boxes) | OPEN | The mod's `M151A2_Police_Base.et` part chain initializes damage managers against a NULL dependency. Same family: `SCR_ResourceEncapsulator Initialize NULL '#return'`, `SCR_WheeledDamageManagerComponent OnPostInit NULL`, `VEHICLE (W): No steering axle`. Investigate the mod prefab's inheritance/component wiring vs the vanilla `M151A2.et`; do not edit vanilla. Deferred 2026-08-30. |
| `Tree component ActionsManagerComponent cannot be combined with component ActionsManagerComponent` | FIXED | `Worlds/DebugWorld/DebugWorld_Layers/PlumChain.layer` carried an `ActionsManagerComponent` override whose GUID belonged to the Apple tree prefab, so the engine treated it as a *second* instance. Override removed; the entity inherits the prefab's single component. Check new gather-tree layers for the same pattern. |
| `INVENTORY (W): "Parent Node From Parent Entity" property on RplComponent ... set to true` | FIXED | `Prefabs/Items/Seeds/SeedTomato.et` was a standalone GenericEntity whose RplComponent defaulted the flag to true; vanilla `Item_Base` sets `"Parent Node From Parent Entity" 0`. SeedTomato now sets it. If `Cabbage.et` / `BaseCrop` ever warn the same way, apply the same one-line fix. |
| `GUI (W): Missing string ID = <key>` | FIXED | Runtime `.conf` tables drifted from the `.st` source. Regenerate: `tools\cli regen-localization` (check mode runs in `cli validate`). The `.st` is source of truth; `everonlife_localization.<lang>.conf` are derived. |
| `[Chat] <N> chat channel styles are not configured.` | FIXED | `ChatPanel.layout` was missing `m_AdminStyle`. Now sets all nine styles including `m_AdminStyle SCR_ChatMessageStyle "{5CC55B49D3D54EDC}" : "{5980F59BAAC4DDDB}Configs/Chat/AdminChannel.conf"`. |
| `EditBoxFilterComponent used on invalid widget type` | FIXED | `ChatHud.layout` had a nested override block duplicating the EditBox + filter tree that never merged. Dead block removed; the filter now lives once, on the correctly-typed `EditBoxWidgetClass` in `ChatPanel.layout`. |
| `Slot_<Name> Has no Content!` (WeaponInfo, Chat, VON, AvailableActions, Vehicle, GameVersion, Notifications, Hints) | FIXED | `PlayerControllerRoleplayMP.et` cleared `InfoDisplays` but left the vanilla `HUDManager_Root.layout` wired, whose slots warn when empty. Now the controller's HUD handler points at the mod's empty `EL_HUDManagerRoot.layout` (`{F7EE326F6B1B4101}`). |
| `Notification data in 'SCR_NotificationsLogDisplay' has duplicate notification info key: EDITOR_PERCEIVED_FACTION_PUNISHMENT_KILLING_SET / EDITOR_PERCEIVED_FACTION_TYPE_DISABLED` | VANILLA | Duplicate entries are a copy-paste bug in the base game's `Configs/Notifications/Notifications.conf` (mislabeled vehicle-salvage notifications). Fires in any session, any controller. Do not ship a 123 KB override to delete two entries. |
| `Unable to register a script command due keyword duplicity: ban / kick` | VANILLA | Engine command registration on game reload. The mod registers no `ban`/`kick` commands. Harmless reload noise. |
| `No SCR_FactionManager found in the world, SCR_RespawnSystemComponent might not work as intended!` / `No SCR_LoadoutManager found ...` | BY DESIGN | DebugWorld has no vanilla faction/loadout managers; the mod uses `EL_SpawnLogic` + its own character-creation flow, not the vanilla respawn system. |
| `ScenarioFramework: Available tasks are empty, no new tasks will be generated.` | VANILLA | Scenario Framework finds no SF tasks in the world. Harmless for the mod's roleplay flow. |
| `SCR_PlaceableEntitiesRegistryFromCatalog cannot find SCR_EntityCatalogManagerComponent` | VANILLA | An editor-mode entity was instantiated in play mode (DebugWorld boots). Harmless in debug. |
| `The default minimap texture ('$LifeFramework:Worlds/DebugWorld/DebugWorldRasterized.edds') not found ... Skipping...` | BY DESIGN | DebugWorld has no satellite minimap. The engine derives the path from the world name and skips when absent; world + tests are unaffected. To add one: World Editor → Export Map Data → Type: Rasterization → convert to `.edds` at `Worlds/DebugWorld/DebugWorldRasterized.edds`. |
| `[ELDebug:Whitelist] no whitelist file yet: $profile:LifeFramework/Whitelists/<X>.txt` | BY DESIGN | First boot before an admin created the whitelist file; the feature degrades to allow (CONNECT-gate file-missing path). Create the file to restrict. |
| `[ELDebug:Siren] mode -> default (index 0)` | INFO | Siren state machine initializing. |
| `[ELDebug:DebugWorld] boundary spawned 4/4 corner poles` | INFO | Debug boundary poles placed. |
| `[ELDebug:Mining] spawned 5/5 ores` / `respawn scheduled in 60000 ms` | INFO | Mining feature state. |
| `[ELDebug:Quantity] stack visual {GUID} at quantity N` | INFO | Stack quantity visual applied. |
| `[ELDebug:VehicleLock] identifier assigned:` | INFO | Vehicle lock registration. |
| `[ELDebug:LicensePlate] generated plate ...` | INFO | License plate generation. |
| `[ELDebug:Quests] registered quest '<id>' from giver '<name>'` | INFO | Quest registry population. |
| `[ELDebug:CharacterCreation] ...` / `[ELDebug:Spawn] ...` / `[ELDebug:FactionMenu] ...` | INFO | Spawn/character-creation flow state. |
| `[EL_NotificationManagerComponent] ✓ ... / ... shut down` | INFO | Notification manager lifecycle. |
| `[ELDebug:TextSign] label ready:` | INFO | Text sign init. |
| `[ELDebug:Persistence] ...` (state transitions, autosave schedule, save scan) | INFO | Persistence lifecycle logging. |
| `[ELDebug:Persistence] save point created` | INFO | A manual/autosave completed (expected on normal saves). |
| `[ELDebug:Persistence] system state ... -> SHUTDOWN` | INFO | Persistence session teardown. |

## How to add a row

When a new message appears:

1. Grep this file; if absent, classify it against the real source:
   - Is the emitting class in `addons/LifeFramework/Scripts/` or a vanilla
     script (read it with `tools\cli call game_read ...`)? Vanilla emitter that
     is not mod-affected → **VANILLA**.
   - Mod emitter that is intentional (whitelist missing file, debug placeable)
     → **BY DESIGN**.
   - Mod emitter that is a defect → fix it, mark **FIXED**, reference the fix.
2. Add the row in the same change.