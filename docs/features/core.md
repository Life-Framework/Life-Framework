# Core

### `LF_Places` — `Scripts/Game/Core/LF_Places.c`
- ✅ **Map independence** (2026-11-14, #961): placed components register their
  owner under a kind and the feature's stable id (`Register`), and systems find
  places with `Find` / `FindOrigin` / `FindAll`, never `FindEntityByName`.
  Entries hold `EntityID`; a read drops an entry whose entity is gone. A second
  live entity under a held id is refused with `[LFDebug:Places] register
  refused`. Dropped at world end (`LF_StaticReset`). Migrated: `LF_Breach`
  (lock, sync, origin, fuse), the jail cell door, `LF_Bank` vault origin and
  heist target, `LF_Hospital` till and cabinet origin, the hideout door (no
  longer needs an entity name). Tests `places/registry`,
  `places/unnamed-door`.

### `LF_GameModeRoleplay` — `Scripts/Game/Core/LF_GameModeRoleplay.c`
- ✅ Lazy-creates `LF_CharacterCreationManager`, `LF_ATMManager`,
  `LF_JobManager` on first player connect and drives character creation.
- ✅ **World-end static reset** (2026-11-08): the game-mode
  destructor calls `LF_StaticReset.ResetAll`, which runs every class's own
  `modded class LF_StaticResetHooks` (2026-11-16: no central list; `LF_StaticReset.Registered()`
  lists them, `LF_Test_StaticResetHooks`), and so drops `LF_Breach`, `LF_Jail`,
  `LF_JailEscort`, `LF_Contraband`, `LF_PoliceRanks`, the jail cell/arrestable
  tick owners, the garage and spawn-point registries, and the quantity transfer
  intents. It is not called at game start: placed components register in
  `OnPostInit`, before the game mode starts.
- ✅ **Disconnect cleanup** (2026-11-08): server
  `OnPlayerDisconnected` releases escort holds where the player is officer or
  prisoner (`LF_JailEscort.ReleasePlayer`) and fails a running convoy, dropping
  its vehicle watch (`LF_Contraband.ReleasePlayer`). Jail sentences stay (they
  resume on reconnect). An officer who leaves on duty stays on duty with the
  police inventory carried; the civilian inventory waits at the desk (M1-11).
- ⚠️ Managers are only created on first connect; a server-side call before any
  player connects gets null.
- ✅ **Base-game managers on a vanilla map (#1104).** The prefab carries
  `LF_EntityCatalogManagerComponent` (`Core/LF_EntityCatalogManagerComponent.c`), the vanilla
  entity catalog manager without its demand for a vanilla faction manager, so vanilla Everon's
  resupply stations find their catalogs. It no longer brings its own `MPDestructionManager`:
  vanilla Eden places one, and `DebugWorld`'s `Gamemode.layer` places its own. Core alone on
  `CoreOnEveron` boots with neither base-game error.
- ✅ **DebugWorld weather is pinned Clear (#602).** `GameMode_Roleplay_Debug.et`
  carries the base-game `SCR_TimeAndWeatherHandlerComponent` with one predefined
  start (Clear, 09:36 game time) and no random changes, so the server calls
  `ForceWeatherTo(true, "Clear")` and the stock state machine never drifts into
  Cloudy. Cloudy's volumetric clouds render as vertical streaks here, and swapping
  in Eden's world, sky, light, fog, post-process, probe and weather prefabs (or
  removing the terrain) did not change that. The harness `time <hour> [WeatherState]`
  keeps the looping flag. Test `world/debugworld-weather-pinned-clear`.

### `LF_EventBus` — `Core/Events/` (#329, 2026-11-09)
- ✅ Server event bus: typed events, `Subscribe`/`Unsubscribe`/`Publish`, subscribers
  called in subscription order, each isolated (a false return logs
  `[LFDebug:Events]` Error and the rest run), list copied so unsubscribing
  mid-publish is safe, server only. Published today: `OnCrimeCommitted`,
  `OnMoneyMoved` (every cash and bank change), `OnPlayerKilled`, `OnArrested`, `OnJobCompleted`,
  `OnVehicleBought`, `OnPlayerDowned` (the downed state, below). Test `core/event-bus`.
  See `docs/architecture/events.md`.

### `LF_RateLimit` — `Core/LF_RateLimit.c` (#693b, 2026-11-12)
- ✅ Server-side request rate limit, per player and per request kind (`LF_ERequestKind`
  PURCHASE, TRAVEL, COMMS, SPAWN, INVENTORY): a fixed window admits a small burst
  (purchases 4 per 2 s, travel 2 per 10 s, comms 3 per 5 s, spawn or move 4 per 1 s,
  inventory 8 per 1 s; named `// lf-fixed:` constants until the #278 config service lands).
  The first refusal in a window logs `[LFDebug:RateLimit]` Warn and toasts the player
  (`LF-RateLimit_Slow`); later refusals in that window are counted and logged once when it
  rolls over. A request with no player (server-local) is not counted. Applied in: shop buy and sell (the slip prints `LF-Shop_RefuseTooFast`, no
  toast), vehicle lot, organization upgrade and founding, spawn hub travel, service board,
  building placement, give cash and give item, arrest, quantity split and transfer. Not
  applied where a state gate already makes spam a no-op (character create and select,
  respawn) or to admin-only reads (incident feed). The `validate-request-audit` lint now
  covers every `RplRcver.Server` handler (59) and fails when a baselined rate limit is
  removed. Test `ratelimit/window`.

### `LF_IncidentLog` — `Feature/Moderation/` (#239, 2026-11-09)
- ✅ Server-only, append-only incident log, a subscriber on `LF_EventBus`
  (`OnPlayerKilled` with killer, weapon, position, murder flag; `OnPlayerDamaged`,
  sampled per victim/attacker pair every `m_fDamageSampleSeconds`; `OnMoneyMoved`
  with its source; `OnPlayerDowned` as kind `downed`, #255). Bounded by `m_iMaxRecords` and `m_iRetentionHours`, kinds from
  `Configs/Moderation/LF_Incident.conf` (`all` or a list of death, damage, cash, downed).
  `Query(center, radius, from, to, kind)`. Saved as the `incidents` payload
  (save version 13); never replicated. Logs `[LFDebug:Incident]`. Tests
  `incident/log`, `persist/incident-log-roundtrip`.
- ✅ **Hold-up** (#244, test `incident/holdup`). A `Hold up` entry (`holdup` role slot of the player interaction
  menu, `Character_Roleplay.et`) is offered while a weapon is in hand. The server (`LF_HoldUp.Attempt`) checks
  weapon, range (`m_fHoldUpRange`, 2 m; the menu itself reaches 2 m) and the actor's cooldown
  (`m_iHoldUpCooldownSeconds`, 30) in `LF_Incident.conf`, then raises the vanilla halt gesture, plays a cue
  at both characters, toasts the target, and publishes `LF_OnHoldUp`. The incident log records kind `holdup`
  (actor, target, position, zone). A refusal logs `[LFDebug:HoldUp] hold-up refused` and records nothing.
- ✅ **Admin actions are logged** (#246, test `incident/admin-action`). `LF_AdminAction` publishes `LF_OnAdminAction` on the server for every Game Master editor action (teleport, heal, spawn, delete, whitelist, kick), the vanilla `#kick` and `#ban` chat commands, and `!wadd` / `!handled`. The incident log records kind `admin`: the admin is the subject, the player acted on the other party, the action name the detail. Shown in the Game Master incident view only (it answers administrators only). No save change (existing record, new kind string).
- 🔧 **Ban record** (#1285, test `moderation/ban-record`). Every `#ban create` and `#ban remove` (the unban) the backend confirms (held at PENDING, written from `BanCommand.OnUpdate` only on OK), from chat or RCON, appends a `V1|BAN or UNBAN|account uid|unix|staff uid|duration|incident ids|reason` row to `$profile:LifeFramework/ban_record.txt`; the incident ids are the target's newest 10 `LF_IncidentLog` numbers (as either party) the log still keeps. `#banrecord <uid or player id>` (administrators, chat or RCON) lists one account's rows. Legacy and malformed lines are skipped with a warning. Enforcement stays vanilla; the file is append-only and outside the save (no persistence version).
- ✅ **Aim intent** (#245, test `incident/aim-throttle`). Server only: `LF_AimIntent` ticks every `m_fAimCheckSeconds` (1 s), looks only at characters with a raised weapon, and when one keeps another player within `m_fAimRange` (15 m) and `m_fAimConeDegrees` (6) for `m_fAimWindowSeconds` (3 s) publishes `LF_OnAimedAt`: one incident row (kind `aim`) per pair per window. `m_bAimAloneCounts` 0 also needs a clear line of sight. No rule changes. No save change.
  It changes no damage rule. No save change (the kind is a string).
- ⚠️ Cash given, robbed, or picked up appear only as `OnMoneyMoved` publish points
  land (#330); today the ATM is wired. No query UI yet: admin tooling reads it.

### Game Master incident view — `Feature/Moderation/LF_IncidentGm.c`, `UI/GM/LF_IncidentGmPanel.c` (#240, 2026-11-09)
- ✅ In vanilla Game Master EDIT mode (modded `SCR_EditModeEditorUIComponent`), a panel plus a marker
  per incident cell (25 m) near the camera, last hour, 400 m. The marker nearest the screen centre is
  selected and its rows are listed (kind, minutes ago, who / whom, amount). The client sends only its
  camera position by RPC; the server checks the ADMINISTRATOR role on every request (radius, window and
  40-row cap are fixed server side). A non-admin gets nothing and `[LFDebug:IncidentGM]` logs the
  refusal. Nothing persists. Test `incident/gm-query`. Manual: see the PR.

### Player report to staff — `Feature/Moderation/LF_IncidentFlags.c` (#241, 2026-11-09)
- ✅ `/report` in chat flags the reporter's incidents (either party) from the last `m_iReportWindowMinutes` (10) in `LF_Incident.conf`; online admins get a toast; a per-player cooldown `m_iReportCooldownSeconds` (120) refuses spam, and a report with nothing to flag does not start it. Flagged incidents head the Game Master list (sent even outside the camera radius, marker "!!") and persist (`incidentflags` payload, save version 17) until an admin types `!handled <number>` in admin chat, which is logged `[LFDebug:IncidentReport]`. Tests `incident/report`, `persist/incident-flags-roundtrip`.
- ⚠️ The payphone staff line waits for comms routing.
- ✅ (2026-11-15, #1284) Staff relay, `Feature/Moderation/LF_StaffRelay.c`: `LF_OnAdminAction` and the new `LF_OnIncidentFlagged` (published by `LF_IncidentFlags.Report` after the rows exist) each send one short line through `LF_StaffWebhook.Send`. Throttled per kind: events collect for 5 s from the first, then one line goes out (its own line for one event, a count for more), so ten flags in a window are one message. Tests `incident/flagged-event`, `moderation/staff-relay` (captured sender). `LF_StaffRelay.Install()` runs from a `modded class LF_GameModeRoleplay.OnGameStart` in the same file (#1284 follow-up): server only, a no-op on a client, safe to call twice, and `LF_StaticReset.ResetAll` calls `Uninstall()` at world end (subscribers and tick). Test `moderation/staff-relay-install`.

### `LF_SpawnLogic` — `Scripts/Game/Feature/Character/Spawning/LF_SpawnLogic.c`
- ✅ Account-aware respawn: loads `LF_PlayerAccount`, spawns the active
  character's prefab at a spawn point for the account's role (CIVILIAN, or
  POLICE for an officer who reconnects on duty; falls back to any point), and
  applies the role's kit (`GiveRoleKit`, recursive, purpose-filtered storage
  fill). The POLICE kit is the starter police kit the duty desk issues (M1-11).
- ✅ `ResolveSpawnPoint` (static) picks `SCR_SpawnPoint` by account role key;
  shared with the character-creation flow so both spawn paths land in the same
  area. Verified 2026-08-30.
- ✅ **No-spawn-point fail-safe** (verified 2026-08-30): `GetCreationPosition`
  returns `bool` and `CreateCharacter` aborts the spawn with an
  `LF_Debug.Error` instead of dropping the player at world origin.
- ✅ **Registration retry cap** (2026-11-08): a UID or account that
  does not resolve retries about once a second up to `MAX_REGISTER_RETRIES` (60),
  then logs `LF_Debug.Error` and stops. A success clears the counter.
- ⚠️ Item is silently dropped if no matching storage; loadout recursion has no
  depth guard.

### `LF_CharacterCreationManager` — `Feature/CharacterCreation/LF_CharacterCreationManager.c`
- ✅ Flow (M1-11, 2026-11-08): no account → create → registration form R-89
  (`LF_RegistrationForm`) → civilian spawn with an ID card. There is no faction
  screen and no spawn-location picker; police clock in at the duty desk.
  A returning account skips the menus and the form. `OnCharacterCreated` persists a character, inits ATM + survival
  components, spawns.
- ✅ **Server-owned on a dedicated server** (verified 2026-11-07: dedicated/* tests, server side; client receipt is manual): the flow runs on the server
  only. The form opens on the joining client through an owner RPC on its
  `SCR_PlayerController` (`LF_ShowRegistrationForm`,
  `LF_CharacterCreationController.c`); the answer comes back as
  `RpcAsk_LF_CreateCharacter` and lands in `ServerCreateCharacter`. The player
  is the controller's id, never a client value. `LF_CharacterCreationRules`
  re-checks it: at most `LF_CharacterConfig.MaxPerAccount()` characters per account (1 shipped), names 1..32 ASCII letters with single inner spaces, hyphens,
  or apostrophes (`LF_RegistrationRules`), age 18..80. Names are not unique. A refusal toasts the reason
  key (`#EL-CC_Error_*`) and re-opens the menu. A listen host's joiners now get
  their own menus instead of the host.
- ✅ Real character prefab (verified 2026-08-30):
  `{9B5BB216CC7FF18E}Prefabs/Characters/Core/Character_Roleplay.et`.
- ✅ (2026-11-08, M1-08) No more "Default Citizen": an account with no
  character opens the registration form over `LF_ShowRegistrationForm`
  (owner RPC). A server refusal sends the form back with the reason written on
  it. The form never picks a faction.
- ✅ **Characters per account (#331, save version 24).** `Configs/Account/LF_Characters.conf`
  (`LF_CharacterConfig.m_iMaxPerAccount`, 1 shipped) sets how many characters an
  account holds. With more than 1, a returning player without a living body and
  not on duty gets the registry sheet `LF_CharacterSelectSheet` (owner RPC
  `LF_ShowCharacterSelect`): one line per character and "register a new
  resident" while a slot is free; `RpcAsk_LF_SelectCharacter` is re-checked by
  `LF_CharacterCreationRules.RefusePick` and only accepted while the server has a
  pick open for that player (a further character likewise needs the pick's "new
  resident" line, `RefuseFurtherCharacter`). Police applications and the roster
  are per account (`LF_PoliceApplications.ApplicantUid`). Every character-scoped record is keyed
  by the character's record key (`LF_Utils.GetPlayerUID`; slot 1 = the account
  uid, later slots their own id; the key is replicated on the body for clients);
  whitelists and staff actions use `LF_Utils.GetAccountUID`. An account change
  replicates to its own player's body only, and the police count once a frame (#716,
  `account/publish-cost`). Wanted level, police
  rank and duty totals belong to the character (`LF_CharacterStanding`). An old
  save loads its one character as slot 1 with nothing rewritten
  (`docs/persistence.md`). Tests `account/characters-per-account`,
  `persist/character-slots`, `e2e/pass*` (`e2e --legacy-save`). ⚠️ The "start a
  new life" form and retiring a character are not built yet; the incident log
  keeps character keys.

### `LF_PlayerInteraction` — `Feature/Interaction/` (M1-12, 2026-11-08)
- ✅ One list on another player's body (`Character_Roleplay.et`, context
  `LF_ID`): Introduce yourself, Show ID, Give cash, Give what you hold; on-duty
  police also Search and Arrest. `LF_PlayerRoleAction` slots `fine` and
  `org_invite` stay hidden until `LF_PlayerInteraction.RegisterRole(slot,
  handler)` is called on every machine. Reach: 2 m between origins, both
  alive, the target a player, not yourself; the server re-checks every action.
- ✅ Give cash: the server opens `LF_GiveCashSheet` on the giver (`GiveCashAmount`,
  `GiveCashButton`), then `RpcAsk_LF_GiveCash` moves MoneyStack cash through
  `LF_CashHandover.Move`: more than carried is refused, what the receiver cannot
  take goes back (no cash created or lost). Gesture `Point` + `SOUND_PICK_UP`.
- ✅ Give item: the giver's machine names the item in hand (gadget, right-hand
  item, or current weapon); the server moves it only when its storage root is
  the giver and the receiver has room.
- ✅ Introduce: the target learns the user's character name (`LF_Introductions`,
  keyed by player uid, one way). Persisted as the `introductions` payload (save
  version 8); names re-sync to clients on every spawn. Gesture `Halt`.
- ✅ `LF_KnownNames` (client): a known name shows under the crosshair only while
  looking at that player (15 degree cone) within 4 m. Strangers show nothing.
- ⏳ Gesture visibility, the name label, and the restart path on a real
  dedicated server are manual (PR #98 steps).

### `LF_ShowIDAction` — `Feature/CharacterCreation/LF_ShowIDAction.c`
- ✅ (2026-11-08) Shows the ID card the user carries (their own first, else
  the first printed one) to the player they face: the server reads the item
  and opens `LF_IdCardView` on the viewer's machine (#902: the photo card,
  closing by itself after `idCard.showSeconds`, 8 s). No card: not performable,
  reason `#LF-ID_NoCard`. Offered by the player interaction reach rule
  (`LF_PlayerInteraction.CanOffer`, 2 m, both alive, a player); the server
  re-checks it. The shower points (`Gesture_Point`) and the viewer hears
  `SOUND_PICK_UP` (M1-12).
- ⏳ Proof on a dedicated server with two clients is manual (PR #93 steps).

### `LF_Utils` / `LF_FormatUtils` / `LF_Component` / `LF_ComponentData` — `Core/`
- ✅ `MaxInt`/`MinInt`, `AbbreviateNumber` (999→raw, 1000→`1.0K`,
  1e6→`M`, 1e9→`B`), typed component finders.
- ⚠️ `LF_ComponentData.Get` strips the last 5 chars of the class name
  (`XxxClass`→`Xxx`); breaks for any data class not ending in `Class`.
- ✅ (2026-11-12, #323 batch 0) `LF_Utils.SamePrefab(a, b)` and `PrefabGuid(name)`: two resource
  names are one prefab when their GUIDs match, whatever path text they carry; only a name without
  a GUID falls back to its path (case-insensitive); empty never matches. A resource moved into the
  pack keeps its GUID but `GetPrefabName()` reports the new path, so every prefab comparison
  against a saved or configured name goes through it: chop shop values, `LF_InventoryUtils`
  (`FindItemsByPrefab` uses `LF_SamePrefabPredicate`, the carried-item counts), stored inventories,
  store lines and unpaid-tag reattachment, shop catalog and curated lists, trader lists, restricted
  storages, gather and ore tools, breach charges, quest counts, chain goods, vehicle offers,
  building placement (the server spawns its own performer prefab, never the client's string),
  police kit lists, contraband, the item-count invokers (every subscribed name), GATHER/PROCESS
  quest filters and job scope rewards. The shop catalog is a map keyed by GUID (build: 1339
  items in about 230 ms). Saves are unchanged;
  a pre-move save matches on load. `ShopPrices.conf` rules for one prefab use `m_Prefab` (GUID);
  `m_sFind` stays for folders and name families and must be updated when those resources move,
  as must the shop's `LooksLikeItemPath` folder filter. Tests `prefab/same-prefab`,
  `prefab/moved-path`.

### `LF_InventoryUtils` (Core) / `LF_BuildingManagerComponent` / `LF_ChatManagerComponent`
- ✅ (2026-11-08) `LF_InventoryUtils.AddItem` spawns through
  `LF_Utils.SpawnEntityPrefab` (unresolved prefab logs and returns false);
  `IsStorageHierachyRoot`, `GetStorageOwner`, and `DropItem` tolerate an
  entity without `InventoryItemComponent` / a character without an inventory
  manager.
- ✅ The building preview is a local (non-replicated) entity on the owning
  client and is deleted in `OnDelete` and the destructor, so a life no longer
  leaves an orphan preview on every client. A null camera manager retries next
  frame; a preview that fails to spawn logs once and stops the frame loop.
- ✅ `LF_ChatManagerComponent` registers only `EntityEvent.INIT`; it has no
  frame handler.
- ✅ Vehicle garage and spawn-point registries remove themselves in `OnDelete`.

### `LF_RegistrationForm` — `Feature/CharacterCreation/LF_RegistrationForm.c`
- ✅ (2026-11-08) Civil registry form R-89 on `LF_UIScreen`: given name,
  surname, age, Sign. Client checks only write the error line; the server
  decides. MenuBack brings the form back unsigned ("sign before you go into
  town"). Harness: `type RegFirstName Ada` ... `click SignButton`.

### `LF_IdCardComponent` / `LF_IdCard` — `Feature/CharacterCreation/LF_IdCardComponent.c`
- ✅ (2026-11-08) `IDCard.et` carries holder character id, first name, last
  name, age (RplProps, server-issued once). Max stack 1, so cards never merge.
- ✅ (M1-18, 2026-11-08) The town clerk hands the card over at arrival step 1
  (a quest with `m_bRewardIdCard`). From then on `LF_SpawnLogic` prints one
  from the account's character record on every spawn of that player
  (`LF_QuestManager.HasEarnedIdCard`, from the persisted turn-ins), so the card
  survives death and restart. `m_bGrantIdAtSpawn` (now default off) gives every
  spawn a card for a mode with no clerk. A character already carrying their own
  card is refused a second; a blank carried card is printed instead of adding
  one. The DebugWorld civilian starter kit no longer carries a blank card.
- ✅ (#902, 2026-11-13) Superseded in part: spawning no longer prints a fresh card
  each time. A card lost at death or stolen stays lost and the clerk replaces it
  for a fee; see `LF_PhotoIdCard` below.

### `LF_NpcBaseEntity` station NPCs — `Prefabs/Characters/Core/Npc_Base.et`
- ✅ **Dressed on every machine** (2026-11-08, #150): `BaseLoadoutManagerComponent`
  spawns the slot clothing (jacket, pants, boots) on the server, and that
  clothing is replicated. `Npc_Base.et` carries an `RplComponent`
  (`"Rpl State Override" Runtime`, `"Parent Node From Parent Entity" 0`) and a
  `Hierarchy`, as vanilla `Character_Base.et` does: the server streams each
  placed NPC with its worn clothes as children. Before this fix, a dedicated client saw
  the NPCs in underwear. Test: `npc/clothes-replicate` (WORLD).
- ✅ **Worn, not at their feet, on every client** (2026-11-11, #328): the loadout
  manager put the clothing into slots that no inventory storage owned, so only
  the parent link reached a client and whether it wore an item depended on
  stream order (a different few NPCs per session stood in underwear with the
  jacket or trousers on the ground). `Npc_Base.et` now carries
  `LF_NpcLoadoutStorageComponent` (an `SCR_EquipedLoadoutStorageComponent`) and a
  plain `InventoryStorageManagerComponent` (`SCR_InventoryStorageManagerComponent`
  assumes a character), the vanilla path that replicates which slot holds which
  item. The storage hides the NPC from a player's inventory Vicinity, so nobody
  can open it and take the clothes or pick up the NPC. Test:
  `npc/wears-clothes` (WORLD).
- ⚠️ A client builds a placed NPC from its prefab, not from the layer. A
  layer may override only `CharacterIdentityComponent` on an NPC. An added
  component kicks every client (`RplCreationError: Number of items mismatch`),
  and other overrides never reach clients. `validate-npc-layer-overrides.mjs`
  enforces this. Names set in the layer do not reach clients either
  (`FindEntityByName` works only on the server).
- ✅ (2026-11-08) An NPC's talk context (clerk, quest giver, police duty) sits on
  its chest: `PivotID "Spine5"`, `Radius 0.6`, as vanilla `Character_Base` does
  for "Chest". It was `Offset 0 0.5 0`, `Radius 2.0`, at the hips. The town
  trader's counter crate (`TraderChainCounter`) stands on a table in front of
  the trader instead of on the ground at his feet. Test:
  `npc/interaction-height` (WORLD).
- ✅ (2026-11-08) The flagship pack `addons/LF_Everon1989/` exists as a
  skeleton: a `.gproj` that depends on the base game and the core
  (`4550A2B656E1B569`), empty folders, and a README. Nothing loads it; every
  launch passes `-addons LifeFramework` only. Workbench opened with the pack
  `.gproj` mounts and compiles both addons. `validate-addon-boundary.mjs` fails
  when the core names a pack GUID, path, class, or the addon ID, or when the
  pack holds script; `addon-boundary-selftest.mjs` proves each case. The rule
  is `docs/architecture/core-vs-pack.md`; the tree is classified in
  `docs/architecture/core-pack-inventory.md`.
- ✅ (2026-11-14, #322) Every launch loads the pack: `build`, `test`, `serve`,
  `host`, `playtest`, `dev`, `wt e2e`, `agent-box/session.mjs`,
  `tools/dedicated/session.mjs` and `listen-host` pass
  `-addons LifeFramework,LF_Everon1989` when the pack's `.gproj` is in the
  checkout and the core alone when it is not (`tools/lib/addons.mjs`
  `launchAddons`, one helper). `build` builds both projects; `validate --full`
  compiles both (red-proof: a broken script in the pack fails the
  `[LF_Everon1989]` pass). The pack has its own header,
  `Missions/LF_Everon1989.conf`, which `host`/`playtest` and
  `session --mission playtest` boot; a host whose newest save is on
  `LF_PlaytestTown.conf` keeps that header so the save continues. Remote pool
  runs carry each addon's `resourceDatabase.rdb`.

### Config service — `Scripts/Game/Core/Config/LF_ConfigService.c` (#278)
- 🧪 (2026-11-14, #689) Persistence switches: `persistence.keepCash`, `keepCarriedGear`, `keepSurvival`, `keepWanted`, `keepJail`, `keepLicences` in `config.json` (all on by default) choose what survives a restart. The save still holds every field (no version bump, old saves load); `LF_PersistenceKeep.ApplyAfterLoad` resets each switched-off field after the last payload is read, idempotently, and logs `[LFDebug:Persistence] keep<Field> off: N ...`. Cash off strips cash stacks from carried snapshots only (lockers and the bank keep theirs); wanted off clears every character and its posters; jail off frees every prisoner, #691 logout sentences included. Owner page `docs/owner/persistence-switches.md`, detail `docs/persistence.md`. Tests `persistence/switch-config`, `persistence/switches` (and the `wt e2e` restart with keepLicences off).
- 🧪 (2026-11-14, #964) Prefab slots (`Core/Config/LF_PrefabSlots.c`): a system asks `LF_PrefabSlots.Get("<slot>")` for the prefab it spawns or hands out instead of naming one. `Configs/PrefabSlots/LF_PrefabSlots.conf` maps each slot id to its default prefab and the component classes the system needs; a pack overlay patches `m_Prefab` by slot id through the override model, and the required components stay the core file's. A pack prefab that does not load or lacks a required component is rejected at first use with the reason and the slot keeps the core default; a slot with no usable prefab logs `LF_Debug.Error` and returns empty, so only that feature stops. Core and packs only (no `config.json` layer), so client and server resolve the same prefab; a pack change needs a restart. Slots today: `cash`, `money_bag`, `id_card`, `handcuffs_worn`, `evidence_marker`, and (#1128) `hospital_bill`, `fine_ticket`, `jail_slip`, `store_receipt`, `evidence_casing`, `evidence_bag`, `evidence_footprint`, `evidence_statement`, `phone_handset`, `notice_board_face`, `sign_face`, `courier_parcel`. Prefabs that a shop price, resource chain, fence NPC or other prefab names by path (drill, keycard, fence charge, lockpick, car parts, contraband crate, scanner, service radio, vehicle key, iron nugget and bar) stay in script until their other references move too. `validate-prefab-slots` checks every slot GUID resolves and every script slot id is a core slot. Test `prefabs/slots`. `LF_PrefabSlotsConfig` and `LF_SoundSlotsConfig` are on the pack public surface (#1005), so `pack check` accepts a pack slot overlay; test `prefabs/slot-swap-spawn` swaps `evidence_marker` to another prefab, spawns it, and spawns the core handset after a bad `phone_handset` swap. Cash saved before a pack swapped the `cash` slot still counts (#1130): the core cash and any prefab the save alias table leads to the slot prefab are counted and spent first, new cash is the slot prefab, the save format is unchanged. Tests `prefabs/cash-slot-swap`, `e2e/cash-slot-swap` (`tools\cli e2e --cash-swap`).
- 🧪 (2026-11-14, #323) Sound slots (`Core/Config/LF_SoundSlots.c`), the sound twin of prefab slots: a prop asks for its sound by slot id (`m_sSoundSlot` on `LF_AmbientEmitterComponent`, or `LF_SoundSlots.Get("<slot>")` in script) instead of naming a sound project, so a pack swaps the sound by overriding the slot. `Configs/SoundSlots/LF_SoundSlots.conf` maps each slot id to its default project; a pack overlay of the same class patches `m_Project` by id through the override model. A pack project that does not load is rejected at first use with the reason and the slot keeps the core default; a slot with no usable project logs `LF_Debug.Error` once and the prop stays silent, nothing else stops. An `m_sSoundProject` set on an instance still wins over the slot (for a pack's own placement). Core and packs only, so client and server agree; a pack change needs a restart. Slots today are the sixteen atmosphere sounds: `atmo_store_radio`, `atmo_pub_radio`, `atmo_furnace`, `atmo_sawmill`, `atmo_quarry`, `atmo_farm`, `atmo_still`, `atmo_police_desk`, `atmo_chop_shop`, `atmo_bank_murmur`, `atmo_bank_till`, `atmo_hospital_equipment`, `atmo_hospital_ward`, `atmo_payphone_line`, `atmo_fuel_pump`, `atmo_van_idle`; every default is the sound the prop played before, so nothing sounds different. 12 core prefabs and 3 DebugWorld layers (`ATM`, `MiningChain`, `Street_CashVan`) now name a slot. `validate-sound-slots` checks every slot GUID resolves, every slot id asked for is a core slot, and no core prefab, layer, script or config (the slots file and core tests aside) names a `Sounds/Atmosphere/` sound; `atmosphere-selftest` resolves slots when it checks emitter radii. Test `sounds/slots`. The Everon pack's own layers still name their sounds directly (they are the pack's).
- 🧪 (2026-11-14, #323 batch 4) The atmosphere sounds moved into the pack: `Sounds/Atmosphere/` (seventeen projects and their loops) is now `addons/LF_Everon1989/Sounds/Atmosphere/`, GUIDs unchanged. The sixteen core `atmo_*` slots keep their ids and have no default; the pack overlay `Configs/SoundSlots/LF_SoundSlots_Everon1989.conf` (listed in the `LF_Everon1989` and `LF_PlaytestTown` headers) fills each with the project it had. A slot with no core sound that no pack fills plays nothing and logs one `[LFDebug:SoundSlots]` Info line, not an error, so a core-only server is silent there; a pack-filled slot logs `slot '<id>' -> <project> (pack)`. `validate-sound-slots` now fails a core default that names a `Sounds/Atmosphere/` sound; `atmosphere-selftest` reads the projects from the pack and the slots from core plus the pack overlay; `toolscli asset ambience` writes to the pack (`ambience.json` `addon`). Test `sounds/slots` runs on base game sounds, so it passes core-only.
- 🧪 (2026-11-14, #962) One override model for list sections (`LF_ConfigOverlay.c`): each record has an id and every layer (packs in load order, then the owner file, then the stream) adds, replaces, patches or removes it by id (`"op"` in JSON, `m_sOp` in a pack `.conf`); no verb patches an existing id and adds a new one, as before. A verb naming an id no lower layer has, an add of an id that exists, or an unknown verb is refused with the source and id while the rest applies; two packs on one id warn naming both and the later wins. On it: `shops.priceRules`, `catalogue.items`/`offers` (the catalogue's own merge is gone; a client replaces its lists with the streamed ones) and `crime.crimes` (which now reads pack overlays). The schema gains `op` on those records. Docs `docs/config/override-model.md`. Test `config/overlay-ids`.
- 🧪 (2026-11-14, #1109, #957 part) Vehicle prices and dialogue patches on the override model (#962). `vehicles` is an `LF_ConfigList` (`LF_VehiclePriceList`): the id is the lot prefab's GUID (a key naming the locked child's GUID addresses the same entry), the owner file takes `"op"`, a pack `.conf` of `LF_VehiclePricesConfig` now layers (percents and entries by `m_sOp`), and a new or replaced vehicle needs a base price above 0; the hand-written upsert in `ApplyOwner` is gone. Dialogue nodes and lines are `LF_ConfigRecord`s: a pack `.conf` of `LF_DialogueConfig` listed as an overlay of the NPC's core file adds, replaces, patches or removes them by id, sets the start node, and a node patch replaces its reply list (a reply has no id). `LF_DialogueConfig.Layered` (used by `LF_DialogueComponent`) drops a patch that leaves the conversation broken (unknown node, unreachable reply, hook with no class) and plays the core one. #957 keeps: dialogue by id instead of prefab path, `asset dialogue --addon`, text-only lines, the neutral core clerk with the Everon one as a pack patch, and the `validate` rule for patches. Tests `config/vehicle-override-verbs`, `dialogue/override-verbs`, `config/pack-patch-overlays`; `reload/dialogue-file` still green. Docs `docs/config/override-model.md`.
- 🧪 (2026-11-14, #957) Dialogue by id, text-only lines, pack patch tool and the Everon clerk as a pack patch. `LF_DialogueComponent.m_sDialogueId` (`clerk`) is resolved by `LF_DialogueRegistry` from the core index `Configs/Dialogue/LF_DialogueIndex.conf`, which a pack overlay of `LF_DialogueIndexConfig` adds to, replaces or removes by id; the clerk and fuel clerk prefabs name ids, a path in `m_sDialogue` still works. A line with no event and no length is text only (caption for reading time, no sound); an event without a length is a half-made recording and drops the patch. `toolscli asset dialogue <npc> --addon <pack>` with `"patches": "<core npc>"` writes a text-only patch `.conf` and strings with no ElevenLabs call; `validate-dialogue-patch.mjs` reads every pack dialogue patch against its core. The 1989 pack ships `LF_Dialogue_ClerkEveron.conf` (listed in its mission header): the clerk's greet moves to a pack string key with the same words, so nothing changes on screen. LOGIC `dialogue/pack-overlay`; docs `docs/config/override-model.md`, `docs/asset-pipeline/dialogue.md`. The clerk's voice split (#323, 2026-11-14) is below.
- 🧪 (2026-11-14, #323 batch 4) The clerk's voice lives in the pack. The core clerk (`Configs/Dialogue/LF_Dialogue_Clerk.conf`) is a neutral, text-only conversation: no recordings, no audio project, `"text_only": true` in `tools/assets/dialogue/clerk/dialogue.json`. The Everon pack's patch (`LF_Dialogue_ClerkEveron.conf`, from `tools/assets/dialogue/clerk_everon/`) now carries the Everon wording (pack string keys `LF-Dialogue_ClerkEveron_*`) and the voiced lines: its `voice` and `budget_chars` make `tools\cli asset dialogue clerk_everon` write the wavs, the audio project `Dialogue_clerk_everon.acp` and the conf's `m_sSoundProject` and per-line events into `LF_Everon1989/Sounds/Dialogue/clerk_everon/`. The ten recordings and their provenance moved with their GUIDs (nothing re-bought). `LF_Test_DialogueClerk` runs against the text-only core clerk and needs no voice; `LF_Test_PackPatchOverlays` covers a voiced pack patch on a core fixture; `dialogue-pipeline-selftest.mjs` covers the text-only spec and a voiced patch end to end. A pack-loaded session still speaks the same Everon lines.
- 🧪 (2026-11-14, #1108) The override model reaches the rest of the content lists: `police.ranks` (a remove is refused on a shipped rung and on any rung that is not the last, so no saved rank index moves; pack overlays now layer ranks too), `organizations.rankDefaults`, `zones.ruleSets` (a remove is refused), `licences.rules` (by licence name; the ids are the catalog's, so no remove) and the system catalog `LF_SystemDef` (pack overlays only; a framework or depended-on system cannot be removed). `LF_ConfigList.RemoveRefusal` is the one hook for what may not go. The schema gains `op` on those records. Docs `docs/config/override-model.md`. Test `config/lists-overlay`.
- 🧪 (2026-11-14, #1106) The `jobs` lists are on the override model: `tracks` (by job id), `xpAwards` (by action id) and `licenceGates` (by licence name) take `add`, `replace`, `patch` and `remove` from the owner file and from a pack overlay of `LF_JobsConfig` (read through `LoadPackOverlays`); the `jobs` merge code in `ApplyOwner` is gone, and a new or replaced record must still be complete (a track needs thresholds, an award a job, a gate a job and a level). Tests `config/jobs-override-model`, `config/jobs-pack-overlay`.
- ✅ (2026-11-13, #282 #283) The rest of the D8 sections are owner-readable: `zones` (rule sets keyed by NORMAL/SAFE/LAWLESS, plus the two legacy flags), `police` (the #875 tunables and the rank ladder keyed by rank id, where an unlisted id appends a rung and a saved rank index never moves), `organizations` (the #875 tunables and the #857 default-rank rights as a bool per permission, applied to organizations founded afterwards), `persistence` (`enableAutosave`, `autosaveInterval`, read once at game start, so a restart applies it; there is no retention setting, the autosave overwrites one AUTO slot) and `licences` (`isInitialLicense` and `requiresWhitelist` per licence name; the level gate is `jobs.licenceGates`). `police` and `organizations` ride the config stream (`BuildPayload` takes them as the fourth and fifth argument; the client applies them with the same `ApplyOwner`; alarm prefabs, rank kits and rank defaults stay on the server); `zones`, `persistence` and `licences` are server-only. `LF_ConfigReader.ReadResource` refuses a `{GUID}path` that does not load. With no `config.json` every value is the one the code carried before (`config/sections-backcompat`). Tests `config/sections-a` (police, licences, police stream), `config/sections-b` (zones, organizations, persistence, organizations stream) and `config/sections-backcompat`.
- ✅ (2026-11-12, #749) The `jobs` section (D8): duty pay, the benefit, and the job XP
  tables (`tracks` by job id with `levelThresholds` and `dailyXpCap`, `xpAwards` by action
  id, `licenceGates` by licence name), read by `LF_JobsConfig.ApplyOwner` with ranges.
  `ReadFloat` takes a whole number or a fraction; `ReadInts` refuses a list holding a
  non-number or a number out of range whole. `vehicles` and `jobs` are known top-level
  keys now (`vehicles` warned as unknown before). Test `config/jobs-owner`.
- ✅ (2026-11-12) The server reads the owner's
  `$profile:LifeFramework/config.json` at game start and merges it over the
  shipped `.conf` defaults field by field: owner, then pack, then core. Records
  merge by a stable key (an object keyed by the key, not a list); a new key is
  appended. Shops are the first section (`shops`: default prices,
  `priceRules`, `extraItemPrefabs`; a rule may set `prefab`, matched by GUID). `validate-config-schema` fails when a schema field and the key list `ApplyOwner` declares for `WarnUnknownKeys` disagree. A missing file boots on defaults and
  writes `config.example.json` beside it, never over a file. A bad file,
  section, record or field logs `[LFDebug:Config]` and keeps the lower value;
  nothing crashes the server. Pack overlays come through `LF_ConfigPackSource`;
  the default, `LF_MissionConfigPackSource`, reads the mission header's
  `m_aLF_ConfigOverlays` list (D4 option b; `config/pack-header`). Detail and the
  `SCR_JsonLoadContext` probe results: `Core/Config/Feature.md`. Tests
  `config/json-probe`, `config/merge-fields`, `config/merge-keyed`,
  `config/merge-reject`, `config/merge-no-owner`, `config/owner-file`,
  `config/pack-overlay`.
- ✅ (2026-11-12) `config.schema.json` and `config.example.json` in
  `docs/config/` are generated from the config classes' `[Attribute]`
  declarations (name, type, default, min and max, description; keys follow the
  Hungarian-prefix rule). `node tools/config/schema.mjs --write` regenerates
  them; `validate-config-schema` fails on a stale copy, validates the example
  and the example the service writes at runtime against the schema, and
  `config-schema-selftest` proves it red on a stale schema and a bad example.
  Tools only, no game code (#280).
- ✅ (2026-11-14) Every owner-config field carries help text in the schema.
  The generator read only a named `desc:` and dropped the description most
  attributes give as the third argument, so 145 fields had none; it now reads
  both. `validate-config-schema` fails when any field, record field or section
  has no description (`descriptionProblems`), and the Config generator page
  shows the text under each field (#286).
- ✅ (2026-11-12, #279 #302) Per-field reject: a price outside its attribute's
  `params: "min max"` (0 to 1000000) is refused with the value, limit and kept
  value logged, and the rest still applies; an unknown key (top level, section
  or record) warns and is ignored; `schemaVersion` above the build's logs an
  error (known fields still apply), below it logs an info line, a bad one reads
  as 1. Tests `config/reject`, `config/unknown-keys`, `config/schema-version`.
- ✅ (2026-11-12, #303) Config migrations at boot: an owner `config.json` older than
  the build's schema is backed up (`config.json.bak-v<N>-<UTC timestamp>`, never over an
  existing backup), stepped through ordered `LF_ConfigMigration` steps, rewritten, and each
  step logged. A failed step, a missing step, or a failed backup keeps the original file,
  logs an error, and the server boots on shipped values. Schema is still v1, so none ship
  yet; the v0 to v1 step is a test fixture. Tests `config/migration-chain`,
  `config/migration-file`.
- ✅ (2026-11-14, #970) Live reload: `reload <what>` (harness verb; `LF_ContentReload`) makes the server read content again in a running session and stream it to the clients. `config` is `/configreload`; `dialogue` reads `$profile:LifeFramework/dev/dialogue.json` (`{"captions": {"#key": "text"}}`) as caption overrides (`LF_DialogueOverrides`) that the conversation scene shows for the caption, the replies and the speaker (text only: the voice and the node structure need the pipeline and a restart). The `dialogue` file is read only where dev tools are open (a dev world or a harness build), never for an administrator alone on a live server. A reloader is the class `LF_ContentReloader_<Name>` (no list; the terms layer, #958, adds its own). The overrides ride the config stream as a `devDialogue` section written only while there are some, so a payload without them is unchanged. Test `reload/dialogue-file`. Proof `agent-box/proofs/live-reload-proof.mjs` (`up --clients 1`). `docs/worktrees.md` "Live reload". Merged, in-dev.
- ✅ (2026-11-12, #285) Config stream: the server serialises the effective
  client-relevant values (the `shops` section: defaults, every rule in walk
  order, extra prefabs) with a format version and a hash. Clients get them on
  join through the game mode's `RplSave`/`RplLoad` and again through a
  broadcast RPC when they change; the client's `LF_ShopPricesConfig.ApplyLayers`
  uses them as its last layer and the catalog re-prices. Another format or
  schema, a hash mismatch or a bad value logs `LF_Debug.Error` and keeps the
  client's values. `/configreload` (administrators; dev worlds and harness
  builds) re-reads `config.json` on a running server and streams the change.
  Only typed client fields are written; the owner file never leaves the server.
  Proof verb `config-stream-proof`. Tests `config/stream-roundtrip`,
  `config/stream-client-only`, `config/stream-reject`, `config/stream-vehicles` (the vehicles section, #285).

### Debug grid world — `Worlds/GridWorld/` (#1803, 2026-11-16)
- 🧪 (2026-11-16, #1803) `Worlds/GridWorld/GridWorld.ent` is the second of the three worlds (`docs/architecture/three-worlds.md`): the flat debug terrain (its own copy, `GridWorld/GridTerrain/`, because the DebugWorld terrain does not load from another world), sky and light, the debug game mode, one spawn `Spawn_Grid_Gate` just inside the annex north gate, the test runner, and every annex entity copied byte for byte from DebugWorld by `node tools/grid/split-annex.mjs --copy` (Annex, HousesKeys, Jobs, NPCs, PRFeatures, Police, Tests, WhitelistChat whole; 2 of the 9 `Crimes` entities). No town spawns, safe or lawless zones. Mission header `Missions/LF_GridWorld.conf`. DebugWorld is unchanged and nothing boots GridWorld by default yet: `toolscli test --world Worlds/GridWorld/GridWorld.ent` does. `tools/lib/worlds.mjs` names the three worlds. Test `world/grid-boot`; `split-annex-selftest`.

- 🧪 (2026-11-16, #1808, W7) Grid cell `test-fixtures` (`Grid_test_fixtures.layer`, c1r3) gives the tests that read the town their own fixtures: one placed `EL_JailCell` (door target `Annex_Jail_Door`) and `farm_plot_1..3` (`FarmPlot_Cabbage` with `LF_FarmPlotComponent` keys). 11 pins leave `tools/test/world-pins.json` (the jail cluster: `disconnect/cuffed`, `disconnect/jail`, `jail/online-time`, `jail/placed-cell`, `jail/slip`, `jail/work`, `robustness/single-jail-tick`; the plots: `farm/plot-loop`, `persist/farm-plots-roundtrip`, `systems/keep-data`, `systems/world-gates`); they run in the GridWorld boot. 41 pins stay with a note in the file: shops and tills, zones, spawn points, signs, quest givers, hideouts, the car lot and impound desk, the roadmap scenarios.
