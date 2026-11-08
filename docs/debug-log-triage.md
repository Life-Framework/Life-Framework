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
| `Unknown class 'Flags' at offset ...` while loading `IronOre.et`, `GoldOre.et`, or `CopperOre.et` | FIXED | The mining prefabs left the `components` block open before the entity `Flags` property. The parser treated `Flags` as a component class. The block is now closed in all three prefabs. |
| `Unknown keyword/data 'WeaponAnimationComponent' at offset ...` while loading `Axe.et`, `Pickaxe.et`, or `Tool_Base.et` | FIXED | The tool prefabs placed `WeaponAnimationComponent` directly in `WeaponComponent`; the base-game weapon shape nests it under `WeaponComponent.components`. The three tool prefabs now match that shape. |
| `INVENTORY (W): "Parent Node From Parent Entity" property on RplComponent ... set to true` | FIXED | `Prefabs/Items/Seeds/SeedTomato.et` was a standalone GenericEntity whose RplComponent defaulted the flag to true; vanilla `Item_Base` sets `"Parent Node From Parent Entity" 0`. SeedTomato now sets it. If `Cabbage.et` / `BaseCrop` ever warn the same way, apply the same one-line fix. |
| `RPL (E): RplCreationError: Number of items mismatch. expected=N, created=M ... node='Assets/Characters/Basebody/Basebody_Male_01.xob'`, then `Kicked from game. group=1 'REPLICATION'` | FIXED (guarded) | A placed NPC (`Npc_Base.et`, Runtime replication since #150) had a component that a layer adds and the prefab lacks. The client builds the NPC from the prefab, so it is short one replicated item. Move the component into a prefab. `validate-npc-layer-overrides.mjs` refuses such a layer. |
| `GUI (W): Missing string ID = <key>` | FIXED | Runtime `.conf` tables drifted from the `.st` source. Regenerate: `tools\cli regen-localization` (check mode runs in `cli validate`). The `.st` is source of truth; `everonlife_localization.<lang>.conf` are derived. |
| `[Chat] <N> chat channel styles are not configured.` | FIXED | `ChatPanel.layout` was missing `m_AdminStyle`. Now sets all nine styles including `m_AdminStyle SCR_ChatMessageStyle "{5CC55B49D3D54EDC}" : "{5980F59BAAC4DDDB}Configs/Chat/AdminChannel.conf"`. |
| `EditBoxFilterComponent used on invalid widget type` | FIXED | `ChatHud.layout` had a nested override block duplicating the EditBox + filter tree that never merged. Dead block removed; the filter now lives once, on the correctly-typed `EditBoxWidgetClass` in `ChatPanel.layout`. |
| `Slot_<Name> Has no Content!` (WeaponInfo, Chat, VON, AvailableActions, Vehicle, GameVersion, Notifications, Hints) | FIXED | `PlayerControllerRoleplayMP.et` cleared `InfoDisplays` but left the vanilla `HUDManager_Root.layout` wired, whose slots warn when empty. Now the controller's HUD handler points at the mod's empty `EL_HUDManagerRoot.layout` (`{F7EE326F6B1B4101}`). |
| `Notification data in 'SCR_NotificationsLogDisplay' has duplicate notification info key: EDITOR_PERCEIVED_FACTION_PUNISHMENT_KILLING_SET / EDITOR_PERCEIVED_FACTION_TYPE_DISABLED` | VANILLA | Duplicate entries are a copy-paste bug in the base game's `Configs/Notifications/Notifications.conf` (mislabeled vehicle-salvage notifications). Fires in any session, any controller. Do not ship a 123 KB override to delete two entries. |
| `Unknown keyword/data 'SlidingTrackMaterial'` while loading vanilla `M151A2.et` | VANILLA | The message comes from the packaged base-game vehicle resource, not `M151A2_Police_Base.et`. Do not override the vanilla vehicle to suppress it. |
| `Unknown keyword/data 'EL_DebugAreaBoundary'` while loading DebugWorld layers | INFO | Workbench's world parser reports the custom entity class while opening the layer, but the runtime immediately creates the boundary and logs `spawned 4/4 corner poles`. The class is loaded and functional; the message is editor parser noise. |
| `ImageWidget 'm_Outline': Can't load mask ...` / `OverlayWidget Frame0 has invalid slot GridWidgetSlot ...` in `InventoryMain.layout` | VANILLA | These messages originate in the base-game inventory layout and imageset while the menu opens. Life Framework does not override that layout or imageset. |
| `Leaked 'BlockListApi' script instance (1x)!` after an application hang and script reload | VANILLA | This occurs during Workbench online-service/script-reload teardown after the 63-second application hang. It is not emitted by Life Framework code and does not occur during normal DebugWorld initialization. |
| `Unable to register a script command due keyword duplicity: ban / kick` | VANILLA | Engine command registration on game reload. The mod registers no `ban`/`kick` commands. Harmless reload noise. |
| `No SCR_FactionManager found in the world, SCR_RespawnSystemComponent might not work as intended!` / `No SCR_LoadoutManager found ...` | BY DESIGN | DebugWorld has no vanilla faction/loadout managers; the mod uses `EL_SpawnLogic` + its own character-creation flow, not the vanilla respawn system. |
| `ScenarioFramework: Available tasks are empty, no new tasks will be generated.` | VANILLA | Scenario Framework finds no SF tasks in the world. Harmless for the mod's roleplay flow. |
| `SCR_PlaceableEntitiesRegistryFromCatalog cannot find SCR_EntityCatalogManagerComponent` | VANILLA | An editor-mode entity was instantiated in play mode (DebugWorld boots). Harmless in debug. |
| `The default minimap texture ('$LifeFramework:Worlds/DebugWorld/DebugWorldRasterized.edds') not found ... Skipping...` | BY DESIGN | DebugWorld has no satellite minimap. The engine derives the path from the world name and skips when absent; world + tests are unaffected. To add one: World Editor → Export Map Data → Type: Rasterization → convert to `.edds` at `Worlds/DebugWorld/DebugWorldRasterized.edds`. |
| `[ELDebug:Whitelist] no whitelist file yet: $profile:LifeFramework/Whitelists/<X>.txt` | BY DESIGN | First boot before an admin created the whitelist file; the feature degrades to allow (CONNECT-gate file-missing path). Create the file to restrict. |
| `Usage of released BaseResourceObject` with `GetInfo() Scripts/Game/Core/EL_UIInfoUtils.c` under `ResolveDisplayName` / `BuildCatalog` | FIXED | `EL_UIInfoUtils.GetInfo` passed `Resource.Load(prefab)` straight into `FindEntitySource`, so the resource was released before the source was read: about 16,000 error lines per DebugWorld boot, one stack per catalog item. The resource is now held in a local (2026-11-07). The same message from `EL_QuantityComponent.UpdateStackModel` came from the tier mesh's `Resource` being released when the method returned, so the next `SetObject` touched a released mesh. The component now holds that `Resource` in a `ref` member (2026-11-08): 1 line per `--tier all` boot before, 0 after. |
| `[ELDebug:Siren] mode -> default (index 0)` | INFO | Siren state machine initializing. |
| `[ELDebug:DebugWorld] boundary spawned 4/4 corner poles` | INFO | Debug boundary poles placed. |
| `[ELDebug:Mining] spawned 5/5 ores` / `respawn scheduled in 60000 ms` | INFO | Mining feature state. |
| `[ELDebug:Mining] ore phase` / `ore destroyed` / `forge started` / `forge finished` | INFO | Phased ore mesh and the timed nugget-to-bar forge. |
| `[ELDebug:Mining] strike ignored` / `forge rejected` / `forge failed` / `ore spawn skipped` | BY DESIGN | A mismatched tool, a forge with no nugget, or a spawn list with no weight logs and that step does nothing. |
| `[ELDebug:Quantity] stack visual {GUID} at quantity N` | INFO | Stack quantity visual applied. |
| `[ELDebug:VehicleLock] identifier assigned:` | INFO | Vehicle lock registration. |
| `[ELDebug:LicensePlate] generated plate ...` | INFO | License plate generation. |
| `[ELDebug:Quests] registered quest '<id>' from giver '<name>'` | INFO | Quest registry population. |
| `[ELDebug:Ranks] ... refused` / `kit line skipped` | BY DESIGN | Rank gates log the refusal and leave the rank usable. A kit prefab that does not load drops that line only. `RequestSelfPromotion` always refuses and does not change rank. |
| `[ELDebug:Jobs] job request refused` / `equipment refused` / `fruit round refused` / `fruit claim refused`; `[ELDebug:Quests] open giver refused` / `accept refused` / `turn-in refused`; `[ELDebug:Breach] arm refused: ... carries no breach charge`; `[ELDebug:Farming] placement refused`; `[ELDebug:Chat] admin message refused`; `[ELDebug:Quantity] manipulation refused` / `split refused` | BY DESIGN | Server-side exploit guards (exploits set A). Each refuses a client request that names a job away from its workplace, re-claims a kit or fruit round, talks to a giver out of reach, arms without a charge, places out of range, sends `!wadd` without the admin role, or touches a locked vehicle/house storage. The `security/*` tests trigger several of these on purpose during a full-tier boot. A refusal in live play is a client asking for something it may not have; investigate only if a legitimate action is refused. |
| `[ELDebug:Ranks] promoted` / `kit drawn` / `duty clock started` | INFO | Rank promotion, armory draw, and the on-duty clock. |
| `[ELDebug:CharacterCreation] ...` / `[ELDebug:Spawn] ...` / `[ELDebug:FactionMenu] ...` | INFO | Spawn/character-creation flow state. |
| `[EL_NotificationManagerComponent] ✓ ... / ... shut down` | INFO | Notification manager lifecycle. |
| `[ELDebug:Signs] card open: <id>` | INFO | A player read a sign; the card opened on that client. |
| `[ELDebug:Signs] sign id '<id>' has no titled entry in LF_Signs.conf; Read hidden at <pos>` | ERROR | A placed sign names an id that is missing, empty, or untitled in `Configs/Signs/LF_Signs.conf`. Logged once per sign; that sign's Read action hides. Fix the id on the placed instance or add the row. |
| `[ELDebug:Signs] sign config does not resolve` / `unexpected root class` | ERROR | `LF_Signs.conf` is missing or broken. Every sign hides its Read action. |
| `[ELTEST] ... AppleTradeContainer ... has physics` / `blocks a sphere sweep` | FIXED | The world entity lived in `Terrain.layer`, after earlier gameplay layers had initialized. Their meshes rendered but their physics bodies were absent. Moving world setup to first-loaded `default.layer` restored automatic body creation. The regression test checks both apple crates and three tables. `TRACEABLE` alone did not fix this. |
| `[ELDebug:CollisionTest] ... sweep fraction=...` | INFO | Collision regression probe. A fraction strictly between 0 and 1 with the expected hit entity proves the obstacle blocks the sweep. Fraction 1 means no hit. |
| `[ELDebug:Persistence] ...` (state transitions, autosave schedule, save scan) | INFO | Persistence lifecycle logging. |
| `[ELDebug:Persistence] started a NEW session while N save point(s) exist` | BY DESIGN (ops) | A dedicated server booted without `-loadSessionSave` while the mission has saves, so the saved world was not loaded. Relaunch with `-loadSessionSave` (`docs/persistence.md`). Not expected on the test server, whose profile starts empty. |
| `[ELDebug:Whitelist] save refused: the whitelist file could not be read at load` | BY DESIGN | The whitelist file exists but failed to load (wrong version, unreadable). Saving is blocked so the file is not overwritten; fix or remove the file and restart. The `restart/whitelist-unreadable-file-kept` test prints it on purpose. |
| `[ELDebug:Persistence] save point created` | INFO | A manual/autosave completed (expected on normal saves). |
| `[ELDebug:Persistence] system state ... -> SHUTDOWN` | INFO | Persistence session teardown. |
| `Starting RPL server, listening on address 0.0.0.0:2001` | BY DESIGN | Every `-server` boot listens on 2001; `-bindPort` is ignored in that mode. The CLI's `server-2001` lock keeps boots one at a time. |
| `Unable to start replication` / `Unable to initialize the game` | not a feature bug | Another server already holds 2001: a boot that bypassed the CLI lock (a manual launch, an old worktree CLI) or a leftover `ArmaReforgerServer.exe`. End it and rerun. |
| `Wrong GUID` on localization | not a feature bug | The gitignored `resourceDatabase.rdb` copy is missing. `tools\cli wt new`, `test`, `dev`, `serve`, and `wt e2e` copy it from the main checkout; the `all` and `persistence` tiers stop with `ERROR resourceDatabase.rdb missing` when it is absent. Do not commit it. |
| RPL invalid prefab GUID | not a feature bug | Same missing resource database. Copy it. Wrong GUID and invalid prefab are this, not a feature defect. |
| inventory `not found in replication` together with a cash read of 0 | not a feature bug | The resource database copy is missing. `GetCash` returning 0 is the same gap. These four tests fail only for that reason: `e2e/narrative-loop`, `e2e/inventory-storage`, `money/cash-roundtrip`, `vehiclelock/lock-key-access`. |
| harness `Cannot index into a null array` | fixed bug | The old parser indexed `$matches` while the log was still open. The harness now prints its own `SUMMARY`/`FAIL` on a harness error, so this line should not appear any more. |
| `[ELDebug:DebugMenu] debug action refused: player <id> ...`, `[ELDebug:DebugMenu] open refused: ...`, `[ELDebug:AgentHarness] stand/pose/use refused: player <id> ...` | BY DESIGN | `EL_DevGate` refused a dev-tool request. Dev tools run only when the game mode sets `m_bDevToolsEnabled` (`GameMode_Roleplay_Debug.et`, DebugWorld) or the sender is an administrator. A non-admin on a production server sees this. In the DebugWorld it means the mode prefab lost the flag. |

| `[ELDebug:Crime] robbery refused: no account` | BY DESIGN | A robbery with no server account is refused (no civilian gate or wanted bump could apply, so it must not pay). In a test it is a spawned body that is not a player. On a live server it means the robber's account was not created: check the `[ELDebug:CharacterCreation] flow start` line for that player. |
| `[ELDebug:Notify] notify player <id>: <message>` / `police alert sent to <n> officer(s)` | INFO | Server-side record of a targeted toast or police alert. `0 officer(s)` is normal when no police are on duty. |
| `[ELDebug:Notify] notify skipped: <entity> is not a player` | BY DESIGN | A server feature (shop, robbery, duty) toasted an entity no player controls, e.g. a test-spawned body. Nothing to show. |
| `[ELDebug:Notify] notify to player <id> failed: no player controller` | OPEN if seen live | The target disconnected between the action and the toast, or the id is wrong. Benign once; repeated lines for a connected player are a bug. |
| `[ELDebug:Notify] toast dropped: no EL_NotificationManagerComponent on this client` | OPEN | The game mode prefab lost its `EL_NotificationManagerComponent`. Re-add it to the game mode prefab. |
| `[ELDebug:CharacterCreation] faction <n> refused for player <id>: #EL-CC_Error_FactionDenied` | BY DESIGN | The server whitelist refused a police pick (or an unknown faction value). The player gets a toast and the faction menu again. `#EL-CC_Error_FactionLocked` means the faction was already chosen. |
| `[ELDebug:CharacterCreation] spawn location '<key>' refused ...` / `character refused ...` | BY DESIGN | Server re-validation of a creation RPC failed (location of another faction, unknown key, no accepted faction, bad name/age, second character). The reason key is the toast. |
| `[ELDebug:IdCard] grant refused: character <id> already carries their card` | BY DESIGN | A spawn found the character's own card already in the inventory (carried inventory came back), so no second card. |
| `[ELDebug:IdCard] card not granted: no room in the inventory of character <id>` | OPEN | The spawned body had no pocket for the card (loadout without clothing storage). The player has no ID until a grant succeeds. |
| `[ELDebug:IdCard] show refused: player <id> carries no ID card` | BY DESIGN | Show ID with no printed card carried; the player gets `#LF-ID_NoCard`. |
| `[ELDebug:CharacterCreation] registration form put away unsigned, opening it again` | BY DESIGN | MenuBack on form R-89; the form returns until signed. |
| `[ELDebug:CharacterCreation] faction menu not opened: no workspace on this machine` | OPEN | The owner RPC landed on a machine without UI (a dedicated server owning a controller should never happen). |
| `[ELDebug:Police] wanted list refused: ...` / `wanted list sent: <n> entries` / `wanted list received: <n> entries` | INFO | Server and client halves of the police menu wanted-list RPC. A refusal means the requester was not an on-duty officer. |
| `[ELDebug:License] license <type> has no replication bit` | OPEN | A new `EL_ELicenseType` was not appended to `EL_LicenseBits.Order()`; clients will not see it. Append it. |
| `[ELDebug:StaticReset] feature statics reset (run N)` | BY DESIGN | `LF_StaticReset.ResetAll` from the `EL_GameModeRoleplay` destructor at world teardown. The reset is idempotent. |
| `[ELDebug:Session] disconnect cleanup player=N escorts=M` | BY DESIGN | `EL_GameModeRoleplay.OnPlayerDisconnected` released that player's escort holds and convoy run. `disconnect cleanup skipped: player N has no uid and no entity` (Warn) means the identity was already gone; nothing to release. |
| `[ELDebug:CharacterCreation] pending faction dropped for player N: disconnected` | BY DESIGN | The player picked a faction but left before picking a spawn location; `EL_CharacterCreationManager.ReleasePlayer` dropped the stage-one pick. |
| `[ELDebug:Jail] second jail cell ignored at <pos>` | BY DESIGN | Only the first placed `LF_JailCellComponent` owns the jail config and `LF_Jail.Tick`. A second cell is a world-authoring error; remove it. The `robustness/single-jail-tick` WORLD test spawns one on purpose, so one such line in a full-suite log is expected. |
| `[ELDebug:Jail] cell tick removed` | INFO | The tick-owning jail cell was deleted (world teardown or test). |
| `[ELDebug:CarriedInventory] restore skipped missing prefab '<prefab>' (and anything inside it)` | BY DESIGN | A saved carried inventory names a prefab that no longer loads (removed or moved). That item and its contents are skipped; the rest is restored. `persist/carried-roundtrip` injects `Missing.et` on purpose, so two such lines in a full-suite log are expected. Anywhere else: find the renamed prefab. |
| `[ELDebug:CarriedInventory] restore could not insert '<prefab>' into <owner> - deleting` | OPEN | A restored item fits no storage of its saved parent or the character. The item is lost. Check whether the parent prefab's storage changed. |
| `[ELDebug:Breach] charge cancelled on <id>: <reason>` | BY DESIGN | A charge whose fuse can no longer fire (no world, no `LF_BreachTargetComponent` on the door, timer not scheduled, or the door was deleted before the fuse fired) is dropped so the target is not armed forever. The `robustness/lifetime` LOGIC test emits one with reason `robustness test`. |
| `[ELDebug:Breach] fuse timer not scheduled: <id> is not a placed entity; Advance drives it` | BY DESIGN | Script-driven (unplaced) breach targets, e.g. the LOGIC tests' `logic-breach-fuse`, advance only through `LF_Breach.Advance`. |
| `[ELDebug:Spawn] spawn abandoned for playerId N: UID/account did not resolve after 60 retries` | OPEN if seen | The ~1 s registration retry is capped (`EL_SpawnLogic.MAX_REGISTER_RETRIES`). Seeing it means the backend never returned an identity or the account never loaded: check the persistence/identity lines above it. |
| `[ELDebug:Quantity] stale transfer intent ignored for <entity>` | INFO | A keep-separate/merge intent older than `EL_QuantityComponent.INTENT_TTL_MS` (5 s) belonged to a move that failed; the later add ignores it. |
| `[ELDebug:Jobs] job equipment ... did not fit the inventory; left at the player's feet` | BY DESIGN | Job tool grant (pickaxe/axe) found no room. The tool stays on the ground at the player. |
| `[ELDebug:Host] watching $profile:lf-host-stop` / `stop requested, closing the game` | INFO | The playtest mission (`LF_PlaytestTown.conf`) listens for `toolscli host stop`, which writes that file; the server then closes normally and the engine writes its SHUTDOWN save. |
| `[ELDebug:Host] removed a stale stop file from an earlier run` | BY DESIGN | A stop file survived a crash or a force kill. It is deleted at start so the new run does not close at once. |
| `[ELDebug:Persistence] session ended without a shutdown save` logged right after `system state ACTIVE -> SHUTDOWN` and before `save completed (SHUTDOWN)` | FIXED | The check ran on the SHUTDOWN state change, which the engine enters before it writes the SHUTDOWN save. It now runs at component teardown (`OnDelete`), after the save. Seen on every `host stop` before the fix (2026-11-08). |
| `RplConnection::ValidationError remote script source code checksum does not match!` then `Disconnecting identity=...` | BY DESIGN | The client compiled different script than the server, usually a `-scrDefine` on one side only. `agent-box/session.mjs` passes `LF_SESSION_IDENTITY` to the server and every client. A friend joining a host must run the same commit. |
| `[ELDebug:Spawn] persistent UID not available yet for playerId N, retrying` repeating on a `-noBackend` dedicated server | BY DESIGN | Without the backend every client has the NULL identity, and a dedicated server keeps the strict identity path. The multi-client session sets `LF_SESSION_IDENTITY` so each player id gets its own identity. A hosted server needs the backend. |
| `NETWORK (E): Trying to register connection of possibly unauthorized client.` on a `-noBackend` join | INFO | The client has no backend identity. The join continues. Seen on every multi-client session client (2026-11-08). |
| `RESOURCES : Out of memory. Free system <N> MB` on the session server | INFO | The OS had little free RAM while two game clients loaded (16 GB PC, about 2.1 GB private each plus the server and Workbench). The session kept running. Close Workbench before `session.mjs up` if it repeats. |
| VME `NULL pointer to instance` in `SCR_CharacterHelper.GetCharacterControlType` right after `[ELDebug:AgentHarness] ... died` | HARNESS ONLY | The harness `die` verb kills through `DamageManager.Kill` with a bare self instigator; vanilla `SCR_InstigatorContextData` reads a control type that instigator lacks. The death, the death screen, and the hospital respawn still run. A real death does not take this path. |
| `[ELDebug:Respawn] death recorded for <uid>` then `wakes in hospital bed N of M` and `hospital bill <amount> charged` | BY DESIGN | M1-26. One bill per death; a respawn or reconnect with no new death logs nothing. `is in debt` follows when the balance went negative. |
| `RESOURCES (E): Wrong GUID/name` for a resource added on your branch, and `INVENTORY (W): Item ... not found in replication` when it is spawned | not a feature bug | The worktree `resourceDatabase.rdb` came from main and does not know the new `.meta`. Open Workbench once on the worktree (`-gproj` plus `-addonsDir <wt>addons` and the base game) to rewrite it; `copyResourceDatabase` keeps a copy newer than main's. |

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
