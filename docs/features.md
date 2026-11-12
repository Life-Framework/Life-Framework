# Life Framework — Implemented Features & Contracts

> Ground truth of what the codebase currently contains and what each system is
> supposed to do. First written from a read-only sweep of
> `addons/LifeFramework/Scripts/Game/` (2026-08-29); rows marked **verified
> 2026-08-30** were re-audited against current code during the P0 ship-blocker
> pass. Every claim carries a file path. This is the contract test cases must
> prove.
>
> **Keep this file current.** It is the doc every agent reads to know what a
> system does. Update the affected feature's rows in the SAME change that
> alters the system — a stale row reads as "broken" and wastes the next
> agent's whole session (the 2026-08-29 sweep claimed several already-fixed
> exploits were live; that gaslight is what this note prevents).
> `tools\cli validate` runs `validate-docs-sync.ps1`, which warns when feature
> code changes without this file changing.
>
> **Status legend per feature:** ✅ implemented (what "working" means), ⚠️
> known-fragile/untested contract, ❌ known bug or broken path.

## Core

### `LF_GameModeRoleplay` — `Scripts/Game/Core/LF_GameModeRoleplay.c`
- ✅ Lazy-creates `LF_CharacterCreationManager`, `LF_ATMManager`,
  `LF_JobManager` on first player connect and drives character creation.
- ✅ **World-end static reset** (2026-11-08): the game-mode
  destructor calls `LF_StaticReset.ResetAll`, which drops `LF_Breach`, `LF_Jail`,
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
  whitelists and staff actions use `LF_Utils.GetAccountUID`. Wanted level, police
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
  and opens `LF_IdCardView` on the viewer's machine. No card: not performable,
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
  (`79636E668EA37AC9`), empty folders, and a README. Nothing loads it; every
  launch passes `-addons LifeFramework` only. Workbench opened with the pack
  `.gproj` mounts and compiles both addons. `validate-addon-boundary.mjs` fails
  when the core names a pack GUID, path, class, or the addon ID, or when the
  pack holds script; `addon-boundary-selftest.mjs` proves each case. The rule
  is `docs/architecture/core-vs-pack.md`; the tree is classified in
  `docs/architecture/core-pack-inventory.md`.

### Config service — `Scripts/Game/Core/Config/LF_ConfigService.c` (#278)
- ✅ (2026-11-12) The server reads the owner's
  `$profile:LifeFramework/config.json` at game start and merges it over the
  shipped `.conf` defaults field by field: owner, then pack, then core. Records
  merge by a stable key (an object keyed by the key, not a list); a new key is
  appended. Shops are the first section (`shops`: default prices,
  `priceRules`, `extraItemPrefabs`). A missing file boots on defaults and
  writes `config.example.json` beside it, never over a file. A bad file,
  section, record or field logs `[LFDebug:Config]` and keeps the lower value;
  nothing crashes the server. Pack overlays come through `LF_ConfigPackSource`,
  the seam D4 decides; the default supplies none. Detail and the
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
- ✅ (2026-11-12, #279 #302) Per-field reject: a price outside its attribute's
  `params: "min max"` (0 to 1000000) is refused with the value, limit and kept
  value logged, and the rest still applies; an unknown key (top level, section
  or record) warns and is ignored; `schemaVersion` above the build's logs an
  error (known fields still apply), below it logs an info line, a bad one reads
  as 1. No migrations (#303). Tests `config/reject`, `config/unknown-keys`,
  `config/schema-version`.
- ⚠️ Owner values are server-side: a client's price list shows shipped prices
  on a dedicated server until #285 streams them. Unknown keys are not warned
  yet (#279); `schemaVersion` is read but not acted on (#302).

## Character & Survival

### `LF_SurvivalStats` — `Feature/Survival/LF_SurvivalStats.c`
- ✅ Hunger and thirst clamped `[0,100]`. `Eat`/`Drink` are additive through
  clamped setters. `Create(id)` starts both at 100.
- ✅ (2026-11-08, M1-28) `UpdateStats(dt, config)` drains needs at the
  `LF_SurvivalConfig` rates. Flagship: hunger `0.01/s` (about 2.8 h from
  full), thirst `0.0125/s` (about 2.2 h). `dt <= 0` is a no-op.
- ✅ (2026-11-08) The separate survival health stat is gone. Needs act on the
  character's real health (`LF_SurvivalHealth`). The save record keeps its
  `m_fHealth` slot, written as 100 and ignored on load, so the save format
  does not change.

### `LF_SurvivalHealth` — `Feature/Survival/LF_SurvivalHealth.c`
- ✅ (2026-11-08) **Needs never kill.** On the server tick, with hunger or
  thirst below 20, the damage manager's default `Health` hit zone falls at
  `0.0005` of max per second (about 8 min) to `0.75` of max, then stops. An
  injury already below the cap is not touched. Survival tracks what it took;
  once needs are met it gives back only that, at `0.001`/s. Vanilla healing
  above the cap is never pulled down while fed. A dead body is skipped.
  Tests: `survival/light` (math), `survival/real-health` (WORLD, spawned
  character).
- ✅ The consumable `m_fHealthRestore` heals real health (percent of max).

### `LF_SurvivalConfig` — `Feature/Survival/LF_SurvivalConfig.c`
- ✅ (2026-11-08) Framework knobs: hunger and thirst drain, the low-needs
  threshold, the real-health cap, drain and give-back (fractions of max), the
  stamina drain multiplier, the tick interval and the max tick delta. A
  missing `.conf` logs `[LFDebug:Survival]` once and falls back to the same
  built-in defaults.

### `LF_CharacterSurvivalComponent` — `Feature/Survival/LF_CharacterSurvivalComponent.c`
- ✅ (2026-11-08) Server-authoritative: stats tick on the server only, on a
  1 s call-queue timer. The delta is clamped to 5 s so a hitch does not drop a
  chunk. The owner gets whole-point hunger and thirst and the low-needs flag as
  owner-only RplProps; `BumpMe` fires only when one of them changes (about
  once per 100 s at flagship rates), and right after eating or drinking. The
  low-needs flip logs `[LFDebug:Survival] <id> needs low=...`. Tests:
  `survival/sync`.
- ✅ The old per-frame `EOnFrame` driver is gone. It had no event mask and
  treated world-time milliseconds as seconds.

### `LF_SurvivalStaminaPenalty` — `Feature/Survival/LF_SurvivalStaminaPenalty.c`
- ✅ (2026-11-08) Modded `SCR_CharacterStaminaComponent.OnStaminaDrain`: with
  low needs, exertion drains stamina `1.5x` (config), so a hungry player
  sprints for less time and breathes hard sooner. It lifts on the next sync
  after eating and drinking. Same reentrancy pattern as the base hydration
  penalty.
- ⚠️ Not yet felt in game: set survival to 10 in the DebugMenu and sprint.

### `LF_BetaHud` survival gauges — `HUD/LF_BetaHud.c`
- ✅ (2026-11-08) Hunger and thirst gauges read the replicated whole points.
  The HUD stays second to the body cue. The health gauge already reads the
  damage manager, so the needs cap now shows there too.

### `LF_LawlessHud` lawless area cue (#591) - `HUD/LF_LawlessHud.c`
- Client side, no RPC: the zone entities exist in every client's world, so the HUD asks
  `LF_Zones.RuleAt` for the controlled character every 0.25 s. A red "LAWLESS AREA" tag
  (`UI/Layouts/HUD/LF_LawlessTag.layout`) stays up while inside, and a toast fires on enter and on
  exit. A late joiner or respawn already inside reads as an enter on the first tick. The rule is
  `LF_LawlessState.Transition`, proven by LOGIC test `lawless-hud/state-rule`. Strings: `LF-LawlessHud_*`.
- Not yet seen in game: tag position and the toasts need a rendering client (see the PR's manual steps).

### `LF_ConsumableEffectSurvival` — `Feature/Survival/LF_ConsumableEffectSurvival.c`
- ✅ Applies configured hunger/thirst/health restore on item use; only applies
  values `> 0`.

### `LF_SurvivalStatsSaveData` — `Feature/Survival/Persistence/`
- ✅ Save/apply round-trip re-clamps through setters (corrupt save sanitized).
- ⚠️ `Equals` uses exact float `==` (no epsilon).

### Downed state — `Feature/Medic/` (M2 #255, 2026-11-09)
- ✅ **Siren and lights on service vehicles (#263).** Two placed service vehicles carry the existing driver-seat siren (`LF_SirenManagerComponent`, `Feature/SirenLights/`): the police car `LF_UAZ469_Police.et` (a covered UAZ469 with the blue bar seated on its roof, #595; it was an open M151A2 jeep whose bar floated) at the station (`Roadmap_Police_Car`) and the new `Prefabs/Vehicles/Wheeled/M997/LF_M997_Ambulance.et` (vanilla `M997_maxi_ambulance` plus the red `LightBarLED_Red.et` bar and the mod siren sounds) at the hospital (`Roadmap_Hospital_Ambulance`). The driver cycles modes with the `LF_CycleSirenModes*` keys (mouse button 2 or `gamepad0:thumb_left`, single and double click); the server decides in `ApplyRequest` through the pure `LF_SirenRules.Judge`: a mode out of range, the mode already running, or a change within `MIN_TOGGLE_MS` (300 ms) of the last is refused and broadcasts nothing, so a toggle replicates once per change. The mode is an `RplProp` (`OnModeReplicated`, `BumpMe` on each accepted change) plus `RplSave`/`RplLoad`, so a client that streams the car in later or joins late applies it, not only clients that had the car streamed in at the toggle (#471); `session.mjs siren-late-proof` (`up --clients 1`) proves a late joiner. Test `vehicles/siren`; `session.mjs siren-proof` (harness verb `siren <Entity> <mode>`, dev gate) drives it and checks the second client. Logs `[LFDebug:Siren]`. On duty only (#464): `RequestBy` refuses (and logs `[LFDebug:Siren]`) anyone who is not an on-duty member of the vehicle's `m_eService` (police jeep: police, ambulance: medic), so a thief in a stolen police car drives it with no siren; test `vehicles/siren-duty`. The base game has no vehicle siren, only `SCR_SoundEvent.SOUND_SIREN` for structures.
- ✅ **Treatment bill on paper, paid at the hospital desk (#260).** A revive (`LF_Revive.Apply`) and a hospital wake after a death each write one `LF_TreatmentBillRecord` in `LF_TreatmentBills` (mirrors the police ticket: number, patient, issuing medic, amount, in-game time) and hand the patient a printed `HospitalBill.et`. Amounts are config: `m_iReviveBillAmount` (100) and `m_fBillMedicShare` (0.5) in `LF_Medic.conf`, `m_iBillAmount` (150, wake) in `LF_Respawn.conf`; the bank is never charged. The patient pays the oldest unpaid bill in carried cash at the hospital desk (`LF_TreatmentBillPayAction`, ledger reason `treatment-bill`, medic as counterparty); the paper is stamped paid. The medic's share of a paid revive bill is paid once in cash at clock-out (`LF_DutyLocker.ClockOut`, ledger reason `bill-share`). An unpaid bill has no consequence in the flagship; it is handed again with every new body. Records persist as the `bills` payload (save version 19); a carried bill comes back blank after a restart and is printed again on spawn. Tests `medic/bill`, `medic/bill-persist`, `respawn/bill-once`. Logs `[LFDebug:Bill]`. **Debt (#464):** a bill unpaid `m_iBillDeadlineHours` (48 in-game h, `LF_Medic.conf`) after it was written becomes debt; `LF_TreatmentBills.Sweep` takes it from the bank as soon as the bank holds money (on every bank credit, on spawn, and a 60 s check), partially if needed, one ledger entry per take with reason `hospital-debt`; the ATM terminal shows an UNPAID DEBT line. The debt lives in the same `bills` record (`m_sPaidAt` = `debt:<left>` while unpaid), so the save layout and version are unchanged; test `medic/bill-debt`.
- ✅ **A medic revives a downed patient (#259).** `LF_ReviveAction` ("Revive", on a player character beside Introduce / Show ID) is shown to an on-duty medic (`LF_MedicDuty`) facing a downed player within `m_fReviveRange` (2.5 m, `LF_Medic.conf`). `LF_Revive.Perform` re-checks on the server (medic on duty, patient downed, in range; `LF_Revive.Decide` is the pure rule), then `LF_Downed.Clear`: the flag, the bleed-out clock and the unconsciousness end, the patient keeps their inventory and the sliver of health, and `LF_OnPlayerRevived` is published on the bus. A civilian, an out-of-range medic and a standing patient are refused with a toast and a log line. Tests `medic/revive`; `session.mjs downed-proof` revives the downed client. A revive uses up one field dressing (`m_sReviveItemPrefab`, `m_iReviveItemCount` in `LF_Medic.conf`; empty = free; refused with `#LF-Revive_NoItem`), stops the bleeding and lifts health to `m_fReviveHealthScaled` (0.25): the downed sliver of health used to bleed away seconds after the wake and down the patient again (#427, found by `session.mjs revive-proof`, 10 of 10 stay awake). **CPR animation (#512):** the revive is a 4 s hold (`Duration 4` on the action). `OnActionStart` asks the server (`LF_AskReviveAnim`, patient as RplId, medic taken from the controller); `LF_ReviveAnim.Begin` re-runs `LF_Revive.Check` and tells the medic's owner (`LF_BroadcastReviveAnim`) to start a custom loiter on `Anims/Workspaces/Medic/LF_ReviveCPR.agr`, an attachment graph on the player graph's `NPC` binding that plays the base-game `p_cro_revive_CPR_in/loop/out` clips (`Gestures` = 560). The base game replicates the loiter to every client, late joiners included; each rendering client logs `revive animation playing` only when it sees the medic loitering with the head bone under 1.3 m (#512; `PlayPostGraphAnimation` never played because the player character has no `CharacterAnimGraphComponent`). The stop is always the fast exit: the CPR out clip has no `Gesture_End` event, so a soft exit would never finish. The server stops it on release (`OnActionCanceled`), completion, the medic leaving reach, the patient no longer downed, or a 7 s overrun (`LF_ReviveAnim.Step`, 400 ms watch). Test `medic/revive-anim`; harness verb `cpr <player> start|stop`, `session.mjs revive-anim-proof`. Logs `[LFDebug:Medic] revive animation ...`. First aid (#257) still buys time for anyone.
- ✅ **A medic revives a downed patient (#259).** `LF_ReviveAction` ("Revive", on a player character beside Introduce / Show ID) is shown to an on-duty medic (`LF_MedicDuty`) facing a downed player within `m_fReviveRange` (2.5 m, `LF_Medic.conf`). `LF_Revive.Perform` re-checks on the server (medic on duty, patient downed, in range; `LF_Revive.Decide` is the pure rule), then `LF_Downed.Clear`: the flag, the bleed-out clock and the unconsciousness end, the patient keeps their inventory and the sliver of health, and `LF_OnPlayerRevived` is published on the bus. A civilian, an out-of-range medic and a standing patient are refused with a toast and a log line. Tests `medic/revive`; `session.mjs downed-proof` revives the downed client. A revive uses up one field dressing (`m_sReviveItemPrefab`, `m_iReviveItemCount` in `LF_Medic.conf`; empty = free; refused with `#LF-Revive_NoItem`), stops the bleeding and lifts health to `m_fReviveHealthScaled` (0.25): the downed sliver of health used to bleed away seconds after the wake and down the patient again (#427, found by `session.mjs revive-proof`, 10 of 10 stay awake). ⚠️ No treatment animation (deferred: no animation asset; the revive is instant); first aid (#257) still buys time for anyone.
- ✅ **Medic job at the hospital duty desk (#258).** `LF_DutyAction` takes a role attribute (`m_eRole`, POLICE or MEDIC); `LF_DutyLocker.ClockIn/ClockOut(account, body, role)` swaps inventories through the role's own slot (`police_duty` / `medic_duty`) the same way for both. The medic role is open (no roster); the account is on duty as `LF_Faction.MEDIC` (the persisted faction int, no save format change). The kit is the MEDIC `LF_FactionLoadout` on the game mode (uniform, medical kit, dressings, saline, morphine, tourniquet, `LF_Pager`). Routing reads an on-duty medic as the medic job (`LF_ServiceCalls.JobFor`), so the hospital board, taking a call and the pager reach them. Duty pay: `m_fMedicDutyPayPerSecond` in `LF_Jobs.conf` (0.4), paid in cash at clock-out; medic seconds never count toward police rank. The desk `Prefabs/Medic/LF_MedicDutyDesk.et` is placed at the playtest hospital (`Roadmap_Hospital_Duty_Desk`). Tests `medic/duty-swap`, `medic/duty-desk`; `session.mjs medic-proof`. ⚠️ A medic clocked in at the police desk (or the reverse) is refused (`#LF-Duty_WrongDesk`).
- ✅ **First aid slows the bleed-out (#257).** `LF_FirstAid`: the vanilla bandage, tourniquet and saline bag (`SCR_ConsumableBandage/Tourniquet/SalineBag.ApplyEffect`, so the vanilla animation and healing are unchanged) add `m_fBandageSeconds` (30), `m_fTourniquetSeconds` (45), `m_fSalineSeconds` (60) to a downed player's clock on the server, never above `m_fFirstAidCapSeconds` (300); morphine adds nothing. `m_bFirstAidAnyone` (flagship on; off = only a player whose job is medic). The same item is counted once. Publishes `LF_OnFirstAid`. Tests `medic/first-aid`, `medic/first-aid-rules`. ⚠️ Not driven in a session: the vanilla bandage may refuse a target with no bleeding wound (manual check in the PR).
- ✅ **Drag and carry a downed player (#261).** `LF_DragAction`, `LF_CarryAction`, `LF_DropAction` and `LF_LoadPatientAction` ("Drag", "Carry", "Put down", "Load into vehicle") on a player character, context LF_ID. `LF_Carry` is server-owned: a shared 250 ms tick moves the body to a point behind the carrier (`m_fDragDistance` 1.4 m, `m_fCarryDistance` 0.4 m) through the jail escort's owner-aware move; the hold ends when the body is revived or gone, the carrier goes down, boards a vehicle or leaves, or the body is `m_fCarryReleaseDistance` (60 m) away. Load seats the body in the nearest free cargo seat within `m_fLoadRange` (5 m) and ends the hold. Who may is config: `m_bCarryAnyone` (flagship on; off = on-duty medics). The hold is an RplProp pair on the character (carrier player id, mode) so every client and a late joiner reads it; every start, drop and load publishes `LF_OnPlayerCarried`. Vanilla has no player carry or drag; ACE Carrying is GPL and a Workshop dependency, so the movement is ours and **there is no carry animation yet** (the body is moved, not animated; deferred until the ACE animation grant, `docs/ecosystem.md`). Tests `medic/carry`; `session.mjs carry-proof`. ⚠️ A load needs a free passenger seat; the driver seat is never used.
- ✅ **Bleed-out timer and give-up (#256).** `LF_BleedOut`: a downed player has `m_fBleedOutSeconds` (180) left on a server-owned clock; one shared 1 s tick counts every downed character down and at zero calls `LF_Downed.Finish` (the real death), attributing it to whoever downed them while they are online. The client reads the time left through an RplProp whole-second copy moved only when it drifts 5 s (no per-frame `BumpMe`). Give-up: hold `LF_GiveUp` (X / pad Y) after `m_fGiveUpDelaySeconds` (30); the request is an RPC on the player controller and `LF_BleedOut.GiveUp` refuses unless the character is downed and the delay has passed. Disconnecting while downed is death (`m_bDisconnectWhileDownedKills`). Tests `medic/bleed-out`, `medic/bleed-out-rules`; `session.mjs downed-proof` covers give-up and a full bleed-out. ⚠️ The dimming screen and heartbeat are vanilla unconsciousness only; no custom effect yet.
- ✅ **Lethal damage downs a player instead of killing them** (config `LF_Medic.conf`
  `m_bDownedEnabled`, flagship on; tests `medic/downed`, `medic/downed-rules`).
  `SCR_CharacterHealthHitZone` (every route to death ends at it: a bullet through the limb
  pass rules, `Kill`, bleeding out) cuts lethal damage to what leaves `m_fDownedHealthScaled`
  (0.1) of the health, then `LF_Downed.Down` marks the character and publishes `LF_OnPlayerDowned`.
  Players only (NPCs die as in vanilla); with config off, or a game mode that disallows
  unconsciousness, lethal damage kills as before.
- ✅ **Vanilla unconsciousness, held.** `UpdateConsciousness` puts the character in the vanilla
  `INCAPACITATED` life state (engine-replicated); `ShouldBeUnconscious` stays true while
  downed, so resilience regeneration cannot wake the player. The downed flag is an RplProp on
  the character (`SCR_ChimeraCharacter.LF_IsDowned`), so a late joiner reads it with the
  snapshot. A downed player takes no further damage (hits are absorbed and logged).
- ✅ **Death is a separate, later event.** The murder hook and the hospital wake run on the real
  death, which is `LF_Downed.Finish` (the bleed-out timer and give-up, #256). Vanilla keeps the
  instigator of the knockout as the killer, so the murder lands on whoever downed them.
  `LF_Downed.Clear` ends the state (revive, #259). Neither caller exists yet; the dev `die`
  command uses `Finish`.
- ✅ **Search and robbery.** `m_bDownedCanBeSearched` (flagship on) lets other players open a
  downed body's inventory; off, `HandleAccessValidation` refuses everyone but the owner. A dead
  body is always lootable. The body keeps its inventory either way.
- ✅ Logs `[LFDebug:Medic]` (downed, absorbed hit, cleared, killed for real, search refused).
  The incident log records each downing as kind `downed` (victim, downer, position).
- ⚠️ Not built (own issues): bleed-out timer and give-up (#256), revive (#259), screen effects for
  the downed client, disconnect while downed. A downed player who disconnects loses the state
  with the body. The suite cannot see the controller's life state (it changes a frame later);
  `session.mjs downed-proof` proves it on a 2-client session plus a late joiner.

### Death & Respawn — `Feature/Character/Spawning/`
- ✅ **Death is defined** (verified 2026-08-30): the dead body stays in the
  world with everything it carried — it is never deleted by the spawn flow and
  is only removed by a server restart or world cleanup.
- ✅ **Respawn via the death screen**: `LF_SpawnLogic.OnPlayerKilled_S` no
  longer auto-respawns; it calls `NotifyReadyForSpawn_S`, the client opens
  `LF_DeathScreen` (workspace widget), and its Respawn button runs
  `SCR_RespawnComponent.RpcAsk_LF_Respawn` → `LF_SpawnLogic.RespawnPlayer_S`
  (account-aware, re-applies the faction loadout).
- ⚠️ Body persistence until restart is default-engine behavior (no explicit
  corpse-cleanup control in this mod yet) — verify on a dedicated server.
- ✅ **You wake up in hospital** (M1-26, #121; tests `respawn/bill-once`,
  `respawn/bed-pick`, `respawn/ward`). A death marks the
  player's uid with a pending wake (`LF_HospitalRespawn.MarkDied`). The next
  spawn of that player (death-screen Respawn, or a reconnect while dead) takes
  it once: the body spawns at a free `LF_HospitalBedComponent` bed (rotating,
  occupied beds skipped), a paper treatment bill is written (#260: no bank charge)
  and `HospitalBill.et` (number, amount, in-game date) goes in the pockets,
  and a toast names the amount. The owner plays the base game's
  bed wake loiter (the tutorial intro's, type 7) with the spawn sound, gets up
  after 4 s and steps down to the bed's stand point on the floor,
  then reads the bill: the note "You don't remember how you got here." and the
  New Life Rule window (a printed server rule, nothing enforces it). A bill
  lying in the world has Read. Server-decided; the client never sends an
  amount or a bed. Every charge and debt logs `[LFDebug:Respawn]`.
- ✅ Debt: `LF_BankAccount.Charge` may go below zero when debt is allowed; a
  negative balance is kept through save and load, deposits pay it off first,
  withdrawals are refused while in debt.
- ✅ Config `Configs/Spawning/LF_Respawn.conf` (`LF_RespawnConfig`): wake in
  hospital (off, or no bed placed: the spawn plaza), bill amount (flagship
  150), debt allowed, New Life Rule minutes (15), wake animation. The beds are
  the respawn point list: put `LF_HospitalBedComponent` on any bed (wake offset
  and yaw are attributes). DebugWorld: three `BedMedical_01` in the front west
  room of the hospital (`Street_Hospital.layer`); the hospital building is now
  the unfurnished `House_Town_E_2I03_Base`.
- ⚠️ The pending wake is session state: a restart between death and respawn
  forgets it (plaza, no bill). A carried bill comes back blank after a restart
  and hides Read.

### Carried inventory — `Feature/Character/Persistence/LF_StorageSnapshot.c`
- ✅ **Carried cash and items survive a restart** (2026-11-08, M1-04, tests
  `persist/carried-roundtrip`, `persist/carried-death-no-dupe`, and
  `e2e/pass2-verify-load` across a real two-boot restart): the whole storage
  tree (prefab, stack quantity, storage index, slot id) is the `carried` payload
  of `LF_PersistenceComponentSerializer`, appended at version 6. Keyed by
  character persistence id plus a slot name, so one owner can hold several named
  snapshots (`carried` now; police duty / civilian stash later, M1-11).
- ✅ Captured at every save point (autosave, manual, shutdown) for each living
  player and on disconnect before the character is deleted. On spawn a snapshot
  replaces the starter kit; without one the starter kit is granted.
- ✅ Apply reconciles instead of adding: a matching occupant is kept, an occupant
  no record claims is deleted, a missing record is spawned. A second apply
  spawns nothing. A prefab that no longer loads logs `LF_Debug.Error` and is
  skipped with its contents.
- ✅ Death drops what you carry: `OnPlayerKilled_S` drops the snapshot and a dead
  body is never captured, so the body keeps the items and a restart grants the
  starter kit.
- ✅ No ground copy: `LifeFramework.conf` saves a stack with its parent and no
  longer self-spawns the player character, so a pocket `MoneyStack` does not
  come back loose on the ground next to the restored one (the two-boot e2e
  counts it).
- ✅ **Placed storage** (2026-11-08, M1-15): `LF_PersistentStorageComponent`
  (`Scripts/Game/Persistence/`) saves a placed container's tree in the same store
  under its `m_sStorageKey` and slot `placed`, so it rides the `carried` payload
  with no new save version. Captured on every item added or removed and at every
  save point; restored a frame after the entity initializes and a frame after a
  load. Restore reconciles, so a second restore spawns nothing. The prefab carries
  no native `Persistence` component, so the engine does not save the contents a
  second time (the two-boot e2e finds the 151 cash in the stash and no loose
  copy in the world).
- ⚠️ Ammo count and weapon state are not in the snapshot. Attachments and
  magazines are kept as items.

### Progression — `Feature/Account/LF_Progression.c`
- ✅ **Progression survives a restart** (2026-11-08, M1-10, tests
  `persist/progression-roundtrip`, `persist/progression-old-save`, and
  `e2e/pass2-verify-load` across a real two-boot restart). The `progression`
  payload, appended at save version 7, holds one `LF_ProgressionRecord` per
  player uid: the civilian delivery career (level, XP, paid total), police rank,
  on-duty seconds and unpaid duty seconds, seconds since the last benefit
  collection, the player level (level, XP, skill points), licences by enum name,
  and turned-in quest ids. A courier at level 2 still opens fuel delivery and a
  senior officer stays senior.
- ✅ Level, licences, and turn-ins live on the character, so their owners write
  through to the store and `LF_SpawnLogic` applies the record to every new body.
  They now survive death as well; only carried items drop.
- ✅ Tolerant load: an unknown licence name is skipped with a warning, an unknown
  quest id is kept with a warning, a rank above the ladder clamps to senior. A
  version 6 save leaves live rank and duty totals alone (the account apply
  carries them over). A running duty clock is settled into the save.
- ✅ **The delivery sign-up survives a restart** (2026-11-08, test
  `persist/job-signup-roundtrip`). The `jobsignups` payload, appended at save
  version 11, holds one `LF_JobSignUpRecord` (player uid, job by enum name) per
  signed-up player. It loads after `progression`, so the career level that gates
  the job is back first. `LF_SpawnLogic` puts the job on every new body after the
  hand-over, so the sign-up also survives death. An unknown name, a job that is
  not delivery work, or one above the player's level stays unemployed with a
  `[LFDebug:Jobs]` warning.
- ⚠️ An active run and the open duty clock are session state; after a restart the
  courier is still signed up and starts a new run. `LF_LicenseManagerComponent` is not on
  `Character_Roleplay.et`, so licences persist only once a prefab carries it.

## Account & Persistence

### `LF_PlayerAccount` — `Feature/Account/LF_PlayerAccount.c`
- ✅ Persistent per-player state: faction, on-duty flag, wanted level
  (clamped `[0,5]`), character roster. `AddCharacter`/`RemoveCharacter`,
  `GetActiveCharacter`, `SetActiveCharacter`, `IncreaseWantedLevel`.
- ⚠️ `RemoveCharacter` does not repair `m_iActiveCharacterIdx`; a stale index
  is an out-of-bounds read. `SetActiveCharacter` with a non-member sets idx
  to -1 with no guard. Fields are effectively public (workaround for BIS
  ticket T174113).

### `LF_PlayerCharacter` — `Feature/Account/LF_PlayerAccount.c`
- ✅ Immutable record (id, prefab, name, age), `GetFullName`, `Create`
  generates a persistence id.

### `LF_PlayerAccountManager` — `Feature/Account/LF_PlayerAccountManager.c`
- ✅ Async load + cache + `SaveAndReleaseAccount`. Verified 2026-08-30:
  `SaveAndReleaseAccount` now keeps the account resident, so consecutive
  wanted bumps and police-menu reads do not lose the account mid-session
  (the old evict-on-write churn is gone).

### `LF_PlayerAccountRecord` — `Feature/Account/Persistence/` (verified 2026-11-07)
- ✅ Explicit persisted form of an account and its characters (faction,
  duty, wanted, roster, active index). `LF_PlayerAccountManager.ExportAll` /
  `ApplyAll` convert it; `LF_PersistenceComponentSerializer` writes it after
  its `version` field. `account/save-roundtrip` covers the
  export → reset → apply path. EPF is gone; there is no `ApplyTo`.

### `LF_Playtime` — `Feature/Account/LF_Playtime.c` (#692)
- ✅ Playtime per account and per job or service, counted on the server and never
  replicated. Each connected player has a clock; every 15 s, on disconnect and
  before each save the seconds since its last credit go to the account's job now
  (`JobFor`: on-duty `POLICE`/`MEDIC`, else the signed-up delivery job such as
  `PARCEL`, else `CIVILIAN`), read through `LF_PlayerAccount` and
  `LF_CivilianJob.GetSignedUpJob` only. One credit is capped at 300 s (a wall-clock
  jump is not playtime). Keyed by the account uid, so gates read the person across
  characters. Read with `Total(uid)`, `JobSeconds(uid, job)`, `CollectJobs`.
  Persisted as the `playtime` payload (save version 26, `LF_PlaytimeRecord`).
  `account/playtime` and the `e2e/pass*` pair cover it.

### `LF_PersistenceManagerComponent` — `Persistence/` (verified 2026-08-30)
- ✅ Server-side save orchestration on the game-mode entity: autosave timer
  (one AUTO slot per playthrough via `LF_SaveSelection`), manual saves,
  save-cache scan, load/re-apply paths, wipe. Every path passes
  `PassesSaveGates()` (manager present, saving allowed, persistence system
  active) and logs `LF_Debug` under `Persistence`.
- ✅ A continued save that never reaches the store read (written before the 2026-11-09 EL_ to LF_ rename, pre-release save reset) logs one `LF_Debug.Warn` "save predates the LF_ rename; starting fresh" at `ACTIVE` and runs fresh, no partial load (verified 2026-11-09 with an old save on the test server; docs/persistence.md "Save reset").
- ✅ Shutdown save is engine-owned: the engine requests the SHUTDOWN save point
  on graceful exit; the manager observes the result via `OnAfterSave` and logs
  an `LF_Debug.Error` if a session ends with no shutdown save. The old manual
  `RequestSavePoint(SHUTDOWN)` from `OnGameEnd` always lost the race against
  the persistence teardown on the Workbench (fixed 2026-08-30). The engine
  writes it only under a mission header; a bare `DebugWorld.ent` boot writes
  none. `wt e2e` pass 1 ends through the host stop file on
  `EL_DebugTest.conf`, asserts the SHUTDOWN save landed before the statics
  reset, and pass 2 loads it (test `e2e/shutdown-save-loaded`, #830).
- ✅ Persistence system resolution is stable across reloads: the systems config
  is now `Configs/Systems/LifeFrameworkSystems.conf` (unique GUID, referenced
  explicitly from both missions), replacing the GUID-colliding
  `ChimeraSystemsConfig.conf` override. Resolution retries at game start and
  logs an `LF_Debug.Error` when the entry is truly missing.
- ✅ **Continuing a saved world needs `-loadSessionSave`** (verified 2026-11-07 on
  the dedicated test server: fresh, `-loadSessionSave`, and kept-saves-without-flag
  boots each logged their line): the engine loads the latest save point at launch only with
  that flag (vanilla behavior). Script does not auto-load: `LoadLatestSave()` is a
  game-state transition that boots the world twice and drops connected players.
  At game start on a dedicated server that was not launched from a save, the
  manager scans the mission's saves and logs `LF_Debug.Error` ("started a NEW
  session while N save point(s) exist") when any exist; with none it logs a
  fresh-session info line. See `docs/persistence.md`.
- ✅ **Playtest host** (`toolscli host start|stop|status|backup`, `tools/host/host.mjs`;
  verified 2026-11-08 on the dedicated server): `start` runs `LF_PlaytestTown.conf`
  from a profile outside every worktree (`%LOCALAPPDATA%LifeFrameworkhost`,
  `LF_HOST_DIR`), passes `-loadSessionSave` when a save exists, and backs the saves
  up first. `stop` writes `$profile:lf-host-stop`; `LF_HostStop` closes the game
  normally so the engine writes the SHUTDOWN save, then the CLI backs the saves up.
  The server owns the machine-wide `server-2001` lock while it runs. The
  no-shutdown-save error now fires at teardown, not on the SHUTDOWN state change
  that comes before the save. Selftest: `tools/validation/host-selftest.mjs`.
  Joining: the host runs `-noBackend`, so every player has the NULL platform
  identity. Until 2026-11-08 that made `GetPlayerUID` empty and the faction pick
  failed with `#EL-CC_Error_NoAccount` (nobody could join).
  `LF_Utils.ChooseIdentityFallback` now keys a player by Steam name on a
  `-noBackend` dedicated server: stable across restarts, one character per name
  (`spawn/identity-fallback`; join, restart, rejoin seen live).
- ✅ **One-command playtest** (`tools\cli playtest [stop|status|shortcut]`,
  `Play Life Framework.cmd`, `tools/host/playtest.mjs`): fast-forwards the detached
  checkout `..\Life-Framework-playtest` to `origin/main` (only while no host runs),
  runs its `host start --wait`, then opens `ArmaReforgerSteam.exe` visibly with the
  persistent profile `%LOCALAPPDATA%\LifeFramework\player` (`LF_PLAYER_DIR`), waits
  for the host's `join allowed` line, and prints friend join lines. A running host
  is reused and joined with its own addon. `stop` closes the game, then `host stop`.
- ✅ **Playtest download** (2026-11-10, `tools/release/`, `.github/workflows/playtest-release.yml`):
  `LifeFramework-Playtest.zip` on the latest GitHub release holds the addon (tracked files only, plus a
  fresh `resourceDatabase.rdb`: `tools\cli playtest release` refreshes it and the workflow packs) and
  `Play Life Framework.cmd`, a PowerShell launcher that finds Arma Reforger through the Steam registry and
  `libraryfolders.vdf`, then offers *Play on my own* (`-server LF_PlaytestTown.conf -addr 127.0.0.1:2302`)
  or *Join the community server* (`-client` the address in `website/public/playtest-server.json`, set with
  `tools\cli playtest server <address>|off`). The homepage and /playtest link the stable
  `releases/latest/download/` URL. Selftest `tools/validation/release-selftest.mjs`. ⚠️ Windows only; the
  zip is not signed, so SmartScreen may warn on first run.

### `LF_AccountReplication` — `Feature/Account/LF_AccountReplication.c`
- ✅ **Client copy of the account** (verified 2026-11-07: dedicated/* tests, server side; client receipt is manual): `LF_PlayerAccountManager` is
  server-only, so client-side offers read a replicated copy. Faction, duty,
  wanted, and police rank pack into one int (`LF_AccountStateBits`) carried by
  an `[RplProp]` on the player's `SCR_ChimeraCharacter`; the on-duty police
  count is an `[RplProp]` on `LF_GameModeRoleplay`. The server writes both
  from the account setters (`SetFaction`/`SetOnDuty`/`SetWantedLevel`/
  `SetPoliceRank`), on `OnPlayerSpawned`, and on a 10 s self-healing pass;
  `BumpMe` only on change. Late joiners get the values with the snapshot.
  `GetState(entity)` answers from the account on the server and from the
  replica on a client. Consumers: `LF_RobAction`, `LF_DutyAction`,
  `LF_ConfiscateAction`, `LF_OpenPoliceMenuAction`, `LF_ArmoryDrawAction`,
  `LF_RankPromoteAction`, `LF_BreachRepairAction`, and (since 2026-11-08) the
  jail actions: arrest, escort, escort-arrest, put in and take out of a
  vehicle. Before that their offers read the server-only account, so on a
  dedicated client an on-duty officer was never offered an arrest. Every
  server path still re-checks the real account; the jail cell and escort holds
  are checked on the server only. `tools/validation/validate-offer-checks.mjs`
  fails a `CanBeShownScript` / `CanBePerformedScript` that reads
  `LF_PoliceUtils.GetAccountForEntity` or `LF_PlayerAccountManager`.
- ✅ **Armory actions hidden from non-police** (2026-11-08, #157): Draw and
  Promote at `Roadmap_Police_Armory` are shown only to on-duty police
  (`LF_ArmoryDrawAction.ShownTo`, replicated state); a civilian no longer sees
  them greyed. The server still refuses in `PerformAction`. Test:
  `playtest/offers`.
- ⚠️ `LF_RobWeaponAction`, `LF_RobVehicleAction`, and `LF_Bank` still count
  police and read accounts directly in their offers; move them to
  `LF_AccountReplication` next.

## Money & Economy

### `LF_MoneyUtils` — `Feature/Money/LF_MoneyUtils.c`
- ✅ Cash is the `MoneyStack` inventory item. `GetCash` (‑1 on fault),
  `AddCash`/`RemoveCash` (actual amount, 0 on fault).
- ⚠️ Inconsistent sentinels (`-1` vs `0`); doc comments wrong on
  Remove/Take.

### `LF_Ledger` — `Feature/Money/LF_Ledger.c` (#330, 2026-11-09)
- ✅ Append-only log of every cash and bank change: id, unix time, character uid, account
  (`cash`/`bank`), signed change, reason key, counterparty uid, balance after. Features never
  call it: `LF_MoneyUtils.AddCash`/`RemoveCash` (cash, player targets only) and
  `LF_BankAccount.Deposit`/`Withdraw`/`Charge` (bank) publish `LF_OnMoneyMoved`, and the ledger
  subscribes (`LF_Ledger.EnsureSubscribed`, game-mode start). The reason is an optional last
  argument at the call site (`shop-buy`, `atm-deposit`, `give-cash`, `ticket-fine`, ...);
  none given records `cash` / `bank`. A hand-over is two lines that sum to zero and name each
  other. Persisted as the `ledger` payload (save version 14), trimmed by `Configs/Money/LF_Ledger.conf`
  (`m_iMaxEntries`, `m_iMaxAgeDays`). `Last(uid, account, n)` lists newest first (what the
  incident log reads); `RebuildBalance`/`VerifyBank` rebuild the bank from the log and compare.
  Cash lines are a trail, not a chain: picking up or dropping a note outside `LF_MoneyUtils` is
  not logged. Cash moved into a bag or an AI is not a player ledger line. `validate` rule
  `validate-ledger-writes` fails direct `SetBalance`/`PREFAB_CASH` writes. Tests `money/ledger-records`,
  `money/ledger-persist`, `e2e/pass2-verify-load`.

### `LF_BankAccount` + `LF_ATMManager` — `Feature/ATM/`
- ✅ **The canonical bank** (verified 2026-08-30): every payout in the game pays
  cash; `LF_ATMManager` + `LF_BankAccount` (per-player, keyed by UID, persisted)
  is the only bank, and the ATM is the only cash↔account boundary.
  `Deposit` only if `>0`, `Withdraw` only if sufficient; balance never negative
  through these paths. Manager is a session map registry + async loader.
- ✅ `SetBalance` clamps corrupt negative persistence values, and
  `CreateAccount` is idempotent and rejects empty IDs (verified 2026-08-30).
- ✅ **No bank wipe on the first connect after a restart** (2026-11-07, test
  `restart/bank-survives-connect`, verified 2026-11-07): the game mode used to
  `new LF_ATMManager()` on the first connect, and the constructor stole the
  singleton the save load had filled. `LF_ATMManager`, `LF_JobManager`, and
  `LF_CharacterCreationManager` now share one pattern: `static ref s_Instance`,
  created only by `GetInstance()`, the constructor never registers itself, and
  the destructor only clears the singleton when it is that instance. The game
  mode keeps no manager fields; `LF_GameModeRoleplay.ResolveConnectManagers()`
  only resolves them.

### ATM terminal — `Feature/ATM/LF_ATMTerminal.c` (was `LF_ATMMenu`)
- ✅ (2026-11-08, M1-13) **Terminal screen** on `LF_UIScreen` with the
  `LF_UISkin.Terminal()` skin: green phosphor (vanilla `robotomono` face) in a
  bezel, balance and carried cash from the server, an `AMOUNT >` entry line, a
  status line, a 3x4 keypad (CLR, 0, DEL), DEPOSIT, WITHDRAW, EXIT. Keys: mouse,
  pad focus that moves like a grid (MenuUp/Down keep the column, MenuLeft/Right
  walk a row), keyboard digits and numpad (`LF_Keypad0..9`), Backspace or pad X
  (`LF_KeypadBack`), Delete or pad Y (`LF_KeypadClear`). The entry cannot pass
  `MAX_TRANSACTION_AMOUNT`. Closes on EXIT, MenuBack, death, or 3.5 m from the
  machine. `LF_ATMAction` runs PerformAction only on the acting machine
  (`HasLocalEffectOnlyScript`) and opens only when the user is this machine's
  character (`LF_CounterAccess.IsLocalActor`).
- ✅ (2026-11-08) **Refusals say why, on the screen**: the server replies
  `LF_EATMRefusal` (amount, cash short, balance short, pockets full, not at an
  ATM, no account) with the new balance and carried cash; no toast (the old
  away-from-ATM toast plus the menu toast was the duplicate). No reply in 5 s
  prints `NO REPLY FROM THE BANK`. Sounds: key click, accept and refuse are UI
  events set on the `LF_ATMAction` attributes; an accepted deposit or
  withdrawal plays `SOUND_PICK_UP` on the character for everyone nearby.
  Tests: `atm/keypad`, `atm/refusal-reasons`, `counter/refusal-text`,
  `counter/local-only`, `counter/screen-layout`.
- ✅ (2026-11-10, #590) **1989 bank teller replaces the placed ATM.** `LF_BankTeller.et` is a clerk NPC
  (Edith) whose chest context carries `LF_ATMAction` labelled "Talk to the teller", so deposit and
  withdraw use the same terminal, server seams, range check (`LF_ActionSite` finds the teller as the
  counter), ledger reasons (`atm-deposit`, `atm-withdraw`), quest signal and ATM code. The playtest town
  places her behind the west bank counter (176.3, 259.5, centred on the counter); the ATM entity `ATM_Debug` stays, hidden
  (no mesh, inside the bank), for tests and flows. Test `atm/bank-teller`; demo
  `session.mjs bank-teller-proof`.
- ✅ **Cash moves** (verified 2026-08-30): the menu forwards through
  `LF_CharacterATMComponent.RequestDeposit/RequestWithdraw`, and the server
  RPC handlers `RpcAsk_Deposit`/`RpcAsk_Withdraw` remove cash on deposit
  (partial removals rolled back) and pay cash out on withdraw (all-or-nothing
  with balance restore), guarded by `LF_ATMManager.IsValidAmount`. No money
  from thin air.
- ✅ The money-moving bodies are the static seams
  `LF_CharacterATMComponent.ServeDeposit/ServeWithdraw` (extracted 2026-11-07,
  no behavior change; the RPC handlers keep the range check and reply);
  `atm/request-cash-boundary` (WORLD) drives them with a real character
  (red-proof observed 2026-11-08). The withdraw rollback case pays into an
  entity with no inventory: a bare `Character_Roleplay` still holds MoneyStack.
- ✅ **At the ATM only** (2026-11-07): the server refuses a
  deposit or withdraw unless an entity carrying `LF_ATMAction` stands within
  `LF_CharacterATMComponent.ATM_RANGE` (5 m) of the character
  (`LF_ActionSite`, server-side search; the client names no site). A refusal
  logs `deposit refused: not at an ATM` / `withdraw refused: ...` and the
  terminal prints `REFUSED: STAND AT THE MACHINE` (2026-11-08; was a toast). On a listen host the requests and the replies call their
  handlers directly, like the shop bridge. Test: `security/service-range`.

### `LF_BankAccountComponent` — `Feature/Banking/` (deleted 2026-08-30)
- ❌ The second, unintegrated entity bank is **gone** (file deleted, no
  references remain). The $20k grant, the persistence race, and the duplicate
  money path died with it.

### `LF_ShopComponent` — `Feature/Shop/LF_ShopComponent.c`
- ✅ Catalog-driven buy: `BuyItem(item, qty, buyer)` guards null/qty, server-
  only, price × quantity, `LF_MoneyUtils.RemoveAmount`, refund on
  inventory-full. A sell or buy of a stack pays or gives one unit per
  quantity through `LF_ShopRules.StackTradeTotal` (verified 2026-10-11).
  The menu sells at most 99 of a held stack per click, the server cap
  `LF_CharacterShopComponent.MAX_TRANSACTION_QUANTITY`; a request outside
  1..99 is refused and logged. (2026-11-07)
- ✅ **Money correctness** (verified 2026-08-30): the partial-funds exploit is
  closed — `BuyItem` compares the actual removed amount against the total and
  refunds only what was removed before failing (a $50 buyer cannot buy a $100
  item), and a partial delivery refunds only the undelivered quantity.
  `SellItem` pays cash only after the goods are confirmed removable and
  claws back overpayments. `IsItemSoldHere` gates buy and sell.
- ⚠️ A shop with no `LF_StoreComponent` has no stock (infinite supply); the physical general store below does.
- ✅ **Physical store (#202).** The DebugWorld general store (`Shop_GeneralStore_Debug`, store key `general_store`) is also a physical store: `LF_StoreComponent` beside the price list. The goods stand on the counter as real world items (apple, cabbage, water, apple juice, field dressing). The price list stays and is the convenient way to buy. **Stock is one number per line** (`min(stock, slots)` display copies stand on the shelf, the rest waits in the back room): a copy taken by hand leaves the stock once and is tagged **unpaid** (`LF_Stores`, a record per item instance on the server); a price-list sale (`LF_ShopComponent.Buy`) checks stock, takes the cash, reserves the units and deletes the surplus display copy; every stock operation first looks for items already taken, so a pickup and a sale can never count one unit twice, and an empty line refuses with `#LF-Shop_RefuseOutOfStock`. **Pay for goods** (`LF_StoreTillAction` on the cash register itself, #582: `Shop_GeneralStore_Till`, the station register prefab `EL_FuelTill.et` standing on the counter and naming its store by `m_sStoreKey`, its action context on the customer side; the server refuses a payer more than `LF_StoreRules.TILL_REACH` (3.5 m) from the register) charges exactly what the user carries unpaid in cash (reason `store-sale`, an `LF_OnMoneyMoved`), all or nothing, clears the tags and hands over an `LF_Receipt` (Read toasts the number, items and total; the printed fields are session state, so a receipt kept over a restart reads blank). An unpaid item eaten or used up stays on the bill. **Shoplifting:** a character holding unpaid goods of a store while outside its boundary box (`m_vBoundaryCenter`/`m_vBoundaryHalf`, flat) is caught by the 500 ms store tick: the goods close as theirs, `m_iWantedIncrease` (1) wanted levels, `LF_Dispatch.Report` with `#LF-Offence_Shoplifting` (the alarm sounds at the counter and police are radioed) and `LF_WantedReports.Identify` so the poster names them (being seen walking out of a shop counts as identified). The shelf refills from config over time: `m_iRestockSeconds` (120) adds each line's `m_iRestockAmount` up to `m_iMaxStock` and respawns the missing copies. **Stock persists** (`storestock` payload, save version 21, `LF_StoreStockRow` by store key and prefab); the shelf is rebuilt from it after a restart. The engine's own persistence also saves a loose shelf item (it carries `LF_QuantityComponent`) and restores it where it stood, so on a fill or a load the store adopts a loose copy sitting on a shelf point (or deletes it when the slot is already filled) instead of spawning a second one; `wt e2e` counts the loose apples around the store after the restart. Tests `store/rules`, `store/shelf-till`, `store/shoplift`, `persist/store-stock`; `session.mjs store-proof` (two clients: A pays, B walks out). ⚠️ Follow-ups: the market and the car lot (cars on display, each with its own buy interaction, key at the desk) are not on this pattern yet; a shelf or unpaid item is kept out of stack merges (`LF_Stores.HoldsApart`, test `store/stack-merge`), so it stays a separate stack until it is moved again after paying; receipt text does not survive a restart. **Unpaid tags survive a restart and a logout (#479):** the `storeunpaid` payload (save version 25, `LF_StoreUnpaidRow` by character record key, store key and prefab, never an entity id) saves the tag of every item a living player carries or used up, and `LF_Stores.ParkCarrier` parks them on disconnect; when that character's body spawns with its carried inventory back (`LF_SpawnLogic.OnCharacterCreated`), `LF_Stores.ReattachUnpaid` puts each tag on an untagged item of that prefab it carries (a used-up one comes back as the same debt). A tag whose item or store is gone is dropped with a warning; a tag on the floor or a dead body is not saved. A restored tag is not swept while its carrier has the store's grace (`m_iRestoredGraceSeconds`, 300 s; toast `#LF-Store_UnpaidRestored` at spawn) to come back and pay; once the carrier has stood in the store, or the grace has run out, the sweep applies as usual, so logging out inside a store is no way to leave with the goods. The till charges a restored tag as before. Tests `persist/store-unpaid`, `e2e/pass*` (a juice taken off the shelf before the save is owed and paid after the restart).
- ✅ **The general store sells a short list** (2026-11-08, M1 RC QA): `Shop_GeneralStore_Debug`
  carries a curated `m_aShopItems` (apple, cabbage, water, apple juice, field dressing,
  ALICE backpack). Without it the price list showed the whole 1312-line base-game catalog,
  starting with an SVD magazine pouch and two "!BASE KIT!" rows. Prices still come from
  `ShopPrices.conf`. The UI kit status row gives its action button more width (label 1.5,
  status 1, button 1.5), so "Order" is no longer cut to "Orde".

### Shop price list — `Feature/Shop/LF_ShopPriceList.c` (was `LF_ShopMenu`)
- ✅ (2026-11-08, M1-13) Typed price list on `LF_UIScreen` with the
  `LF_UISkin.PriceList()` skin (paper, mono face, ink buttons): carried cash, a
  "Look for" field that narrows the list as you type, up to 40 lines of item
  and buy price with an Order button, an order slip (quantity with One less /
  One more, 1..99), Pay <total> cash, Sell these, Leave. Buy and sell are the
  existing `LF_CharacterShopComponent` RPCs; every request now gets a reply
  with `LF_EShopRefusal` (not sold here, unpriced, cash short, pockets full,
  nothing to sell, quantity, too far, no shop), the money moved and the carried
  cash, printed on the slip. The shop no longer raises toasts. An accepted
  trade plays `SOUND_PICK_UP` on the character; UI sounds are `LF_ShopAction`
  attributes. `LF_ShopAction` opens only on the acting machine for its own
  character. Closes on Leave, MenuBack, death, or 3.5 m from the counter.
  Tests: `shop/refusal-reasons`, `counter/local-only`, `counter/screen-layout`.
- ✅ (2026-11-12, #278) `ShopPrices.conf` is the core layer of
  the config service: `LF_ShopManagerComponent.LoadConfig` applies pack
  overlays and the owner's `shops` section before it builds the catalog
  (`LF_ShopPricesConfig.ApplyLayers`). The server charges and pays the owner's
  prices.

### `LF_TraderManagerComponent` + `LF_InventoryStorageManagerComponent` — `Components/InventorySystem/`
- ✅ Sell-to-trader pays **cash** (verified 2026-10-11): inserting into a
  trader-owned storage pays one cash unit per stack quantity
  (`LF_ShopRules.StackTradeTotal`) via `LF_MoneyUtils.AddCash`
  (pay-before-delete, clawed back if the item cannot be removed); black-market
  rejects non-civilian factions. The old "payout to ATM bank" and the
  `Print("testtesttest")` debug line are gone.
- ✅ The value lookup and the black-market decision are the static seams
  `LF_TraderManagerComponent.GetUnitValue` / `CanAccessBlackMarket`
  (extracted 2026-11-07; the lookup now skips a null row instead of crashing),
  pinned by `trader/sell-rules` (LOGIC; red-proof observed 2026-11-08).
- ⚠️ Unguarded `pStorageFrom.GetOwner()` (null crash); null `m_aTradableItems`
  foreach crash; delete-then-pay not atomic; `RETCODE_ITEM_TOO_BIG` used for
  "not tradable"; black-market check fails closed but is only cache-keyed.

## Gathering / Processing / Resources

### `LF_GatherAction` — `Feature/Gathering/LF_GatherAction.c`
- ✅ **Carried tools count (pickaxe-count).** `LF_InventoryUtils.GetAmount`/`RemoveAmount` now also search the equipped-weapon storage (a pickaxe is a weapon-slot item the deposit count skips), each carried item once; `LF_GatherAction.HasToolIn` is the shared tool check (GUID compare). No shipped prefab sets the inventory mode and ore is mined by damage with the pickaxe in hand, so play was not affected. Test `tool/pickaxe-count`.
- ✅ Interact action granting `m_GatherAmount` of `m_GatherItemPrefab` into
  the user inventory; tool requirement (right hand / left gadget / anywhere
  in inventory); per-resource gather counter + cooldown restock.
- ✅ Server re-validates tool, crop stage, cooldown, and malformed item/amount
  configuration in `PerformAction` (verified 2026-08-30). Remaining: timeout
  attribute says "ms" but is compared as seconds-compatible — fragile.
- ✅ The server applies a gather when no peer owns the prop, and a grant that
  adds nothing is an error log (verified 2026-10-10). A sandbag the inventory
  rejects, or a nugget with no usable slot, used to look like a successful use.
- ✅ A node id copies tool, yield, and cooldown from `ResourceChains.conf`.
- ✅ **Crop growth runs on the server only** (2026-11-08, #157): `LF_BaseCrop`
  starts its growth clock only on the server (`Replication.IsServer()` and
  `RplSession.Mode()` is not `Client`), not on
  `RplComponent.IsOwner()`, which is still true on a client while the world
  loads. Every stage read goes through `LF_BaseCrop.StageAt`, which returns
  null for an index outside the list. Before this each game client logged a
  `NULL pointer to instance` VME in `LF_BaseCrop.EOnFrame` for every crop.
  Each crop logs `crop <name> init: grows here <bool> (rpl mode <mode>)` once.
  A missing row or a prefab that does not resolve logs `LF_Debug.Error` and
  grants nothing (verified 2026-10-11). The action stays on the node.

### `LF_FarmPlotComponent` + `LF_FarmPlotAction` + `LF_FarmPlots` — `Feature/Farming/`
- ✅ **Seed to harvest in dirt plots (#614).** `Prefabs/Vegetation/Crops/FarmPlot_Cabbage.et` is a vanilla small vegetable bed with `LF_FarmPlotComponent`: an empty plot offers *Plant Cabbage seeds* (`LF_FarmPlotAction` PLANT, needs a `Prefabs/Items/Seeds/CabbageSeeds.et` packet, one is used), the crop grows on the server through the reused `LF_CropStage` list (sprouts, half grown, ripe; one minute a stage in the prefab, so DebugWorld ripens in two minutes; the last stage holds, a ripe plot waits), the prompt reads *Growing: <stage>* until it is ripe, then *Harvest Cabbage* (HARVEST) gives `m_iHarvestAmount` (3) cabbages, the rest drops at the farmer's feet when the pockets are full, and the plot is empty again. The harvest reports `LF_JobManager.OnGatherCompleted` like a gather. Every machine but a dedicated server draws the crop as local, physics-free `FarmPlot_CabbageVisual.et` copies on `m_sSlots` (six on a bed), scaled per stage from the replicated stage index (`RplProp`, bumped only on a stage change). The server checks reach (3 m), the packet and the stage; refusals toast `#LF-Farm_*` and log `[LFDebug:Farming] plot <key> refused`. The growth clock is one 1 s `LF_FarmPlots.Tick` over the live plots (world-time delta), stopped with the last plot. Seeds come from `Prefabs/Props/Commercial/LF_SeedCrate_Cabbage.et` (`LF_GatherAction` *Take Cabbage seeds*, 4 then a 2 min refill; the gather action gained `m_sActionFormat` for the prompt). DebugWorld: `FarmPlot_1..3` at (145/148.5/152, 299.5), `Farm_SeedCrate`, and the `farm_plots` chalkboard `Sign_FarmPlots` replace the freestanding cabbages (`CabbageCrop_Debug`, `CabbageCrop_Bed`), which stopped being gatherable for good 1.4 min after boot. Tests `farm/plot-rules` (LOGIC), `farm/plot-loop` (WORLD), `persist/farm-plots-roundtrip`; proof `session.mjs farm-proof`.
- ✅ **Growth survives a restart** (save version 23, payload `farmplots`, `LF_FarmPlotRow`: plot key, crop id, whole seconds grown). Only planted plots are saved; a row whose plot is not in the world waits and is saved again; a row for a plot that now grows another crop is dropped with a warning. Growth counts only while a server runs. `wt e2e` pass 2 reads `plot farm_plot_3 restored: cabbage grown 81 s, stage 1`.
- ⚠️ Follow-ups: one crop (cabbage); seeds are free from the crate (no seed shop line yet); the seed packet wears the personal-papers mesh (placeholder in `docs/asset-inventory.md`); a client that joins late relies on the stage `RplProp` callback to draw the crop (manual check).

### `LF_ProcessAction` — `Feature/Processing/LF_ProcessAction.c`
- ✅ Recipe: consume all `m_aProcessingInputs`, produce all
  `m_aProcessingOutputs` (into inventory or force-dropped).
- ✅ **Free-output exploit closed** (verified 2026-08-30): malformed recipes,
  empty input/output lists, missing inputs, and non-positive amounts are
  rejected before inventory mutation. Regression coverage is in
  `LF_Test_ActionGuards`.
- ✅ A machine id copies inputs and outputs from `ResourceChains.conf`. An
  empty recipe or a prefab that does not resolve logs `LF_Debug.Error` and
  returns before inventory changes (verified 2026-10-11).
- ✅ Cocaine, jam, and beer are one-input, one-output recipe prefabs under
  `Prefabs/Processing/Chains/`, each parented to `ItemProcessor_Base`
  (verified 2026-10-11). Cocaine consumes coca leaves and produces a cocaine
  pouch. Jam consumes rowan berries and produces berry jam. Beer consumes an
  empty beer bottle and produces a beer bottle. `LF_Test_RecipeChains` checks
  that each input prefab and output prefab is set and distinct.

### `LF_ResourceChainCatalog` — `Feature/Resources/LF_ResourceChainCatalog.c`
- ✅ `Configs/Resources/ResourceChains.conf` names each gather node's tool,
  yield, and cooldown, and each machine's inputs and outputs (verified
  2026-10-11). Yields are item prefabs. Gather and process do not mint cash.
  The playable iron, copper, and gold rows name the pickaxe. The log row does
  not: the axe stays on `CuttableTree_Base`, and `LogPile_Debug` grants a log
  with no held tool so the sawmill still runs before the axe pickup.
- ✅ **Tomato chain removed.** The tomato crop, item, seed, `TomatoChain.layer` and its trader are gone (poor custom asset). The DebugWorld farm keeps apples and plums plus a cabbage bed (`CabbageCrop_Debug`). `EHarvestJobType` keeps stable values (1 was TOMATOES); license value 1 is now `FARMER_FIELD`. Saved tomato items load skipped with a `restore skipped missing prefab` error, no VME (test `persist/removed-tomato-skipped`).
- ✅ MainWorld's St Philippe farm (`Area_StPhillipe_AppleFarm.layer`) places
  the apple trees, the juicer, and a juice trader (15 cash, matching the
  DebugWorld juice crate), plus iron ore, a pickaxe, the furnace, and an
  iron-bar trader (45 cash, the ShopPrices iron-bar sell price). That iron
  node requires the pickaxe in hand. DebugWorld keeps Apple, Plum,
  Logging, Mining, Cement, Gravel, and Sand. `IronOre_Debug` uses
  `iron-debug` (4 nuggets per piece) so one piece still feeds both furnaces.
  The furnace (`Prefabs/Processor/Furnace.et`) wears the base-game `BoilerLarge_02` mesh instead of the sawmill chipper (#593); its recipes are unchanged.
  `GoldOre_Debug` and `CopperOre_Debug` use the debug rows with the prefab
  yields. Every ore rock still needs the pickaxe (the hit zone's tool list).

### `LF_DestructibleResourceComponent` + `LF_DestructibleResourceHitZone` — `Feature/Resources/`
- ✅ Damage gated to configured tools; tool → per-hit damage; FX on hit. The
  tool match compares prefab GUIDs, so path casing no longer matters. A melee
  hit from a tool the resource does not list logs `[LFDebug:Resources]` and
  deals 0 (2026-11-08).

### `LF_OreNodeComponent` + `LF_OreYield` — `Feature/Mining/` (2026-11-08)
- ✅ You mine ore by swinging the pickaxe at the rock; there is no gather
  prompt. The hit goes through `LF_DestructibleResourceHitZone` (pickaxe 20
  damage, the axe 0). Every 60 damage (three swings) a piece breaks off and the
  node row's yield drops as real nugget items beside the rock, toward the
  miner. After the row's `m_iMaxGathers` pieces the rock crumbles (hidden, no
  collision) and grows back after the row's `m_fCooldownMs`. Server-
  authoritative; clients get the hit dust, a shake and the vanilla rock-break
  sound by broadcast, and the spent state by RplProp (JIP-safe). The spent
  state is not saved. Ported from thebonbon `LF_ToolHitZone` and
  `LF_RandomPrefabSpawnable` with permission. Tests: LOGIC `mining/ore-yield`,
  WORLD `mining/pickaxe-hits`.
  2026-11-10 (#592): a rock shrinks one equal step per piece (`m_fMinScale`, default 0.45, replicated float
  `m_fRockScale` so a late joiner sees a half-mined rock), vanishes when spent and grows back after the cooldown.
  Dropped nuggets are placed by a trace straight down (`FindGroundY`) and rest on the ground. Nugget and pickaxe
  inventory preview cameras were refit (nuggets match the bars; pickaxe distance scaled from the axe by mesh length).
  `LF_MiningArea` no longer logs `spawned 0/0 ores` every minute when the area is full.
  Proof (#596, #598): `agent-box/session.mjs mining-proof [--regrow]` mines `IronOre_Debug` to nothing with real pickaxe swings, then smelts four nuggets (the harness `nuggets` verb) at the furnace.
- ✅ Nuggets and bars render textured: `LF_QuantityComponent.UpdateStackModel`
  no longer re-sets the base mesh, which dropped the prefab's material
  overrides and rendered the items white. Cash, pickaxe, axe, ore and bar
  inventory previews use the Front preset with a narrow FOV.
- ⚠️ Inventory previews draw the mesh's baked material and ignore the prefab's
  material override, so nuggets preview as grey stone and bars as red brick
  (re-applying the remap to `ResolvePreviewEntityForPrefab` did not change the
  render, 2026-11-08). The fix is a mesh that bakes the ore material (Workbench
  FBX import). The pickaxe uses the pistol rig and is gripped at the handle end;
  `HandAdditiveTranslation` has no effect on a weapon, so moving the grip also
  needs a re-exported mesh with the pivot up the handle.

### `LF_MiningSpawn` + `LF_MiningForge` — `Feature/Mining/`
- ✅ Weighted ore spawn. A roll in `[0, total)` maps onto the first bin, including
  a roll of 0. The spawn count is the shortfall to the configured amount, and
  an over-full area spawns nothing. An empty list logs `LF_Debug.Error`.
- ✅ Timed forge. One iron nugget
  (`{6853E2E2AE878611}Prefabs/Resources/Mining/IronNugget.et`) becomes one iron
  bar (`{BCD1E8CD7F975074}Prefabs/Resources/Mining/IronBar.et`) after 5 seconds
  (5000 ms). The nugget stays until the delay elapses. A missing nugget, a
  second start, or a nugget removed before the end logs `LF_Debug.Error` and
  yields no bar. A zero delay finishes inside the start.

## Illicit chains

### `LF_ChainCatalog` — `Feature/Chains/` (#416, M2 Underworld)
- ✅ A chain is config: `m_aChains` in `Configs/Resources/ResourceChains.conf` (the M1
  catalog, extended). A chain has a tier (1 to 5), steps in order, equipment, and heat settings.
  A step names the station type it needs, its inputs and outputs (the `LF_ProcessAction`
  recipe rows), the hands-on work units one batch takes (`m_iWork`), a licence
  (`LF_ELicenseType`, `UNEMPLOYED` = no gate), a minimum player level, and an equipment id.
  A pack adds a chain by overriding the `.conf`; there is no script.
- ✅ **A bad chain disables only itself.** `LF_ChainCatalog.Build` validates each row (no id,
  tier out of range, no steps, no station, no work, a malformed recipe, unlisted equipment,
  duplicate ids, negative heat, a prefab that does not load); a failure logs
  `[LFDebug:Chains] chain <id> disabled: <reason>` and the other chains load. Lookup is
  `FindChain` / `FindStep`; a disabled chain returns null.
- ✅ **Gates.** `LF_ChainGate.Evaluate` (pure) and `ForPlayer` (server) refuse a worker without
  the licence, below the level, or not carrying the equipment, in that order, with the keys
  `#LF-Chain_NeedLicense`, `#LF-Chain_NeedLevel`, `#LF-Chain_NeedEquipment`. Equipment is an item
  the worker carries (priced for a shop), so owning it persists with the inventory.
- ✅ **Heat settings** per chain: `m_fPerWork`, `m_fDecayPerSecond`, `m_fMax`, `m_fRangeM`.
  `LF_ChainHeat` does the arithmetic (`AfterWork`, `AfterDecay`, `RangeAt`); the per-site number
  and the trace that reads it come with the stations (#417) and the smell (#418).
- ✅ The core ships one neutral example, `cider` (tier 1, one `barrel` step, apples to juice).
- Test: `chains/config` (LOGIC).

### Hands-on stations — `Feature/Chains/` (#417, M2 Underworld)
- ✅ **A station is a component, not a timer.** `LF_ChainStationComponent` is one shape for a barrel,
  a still, a press or a cooker; the step it runs comes from config (`m_sChainId`, `m_sStepId`).
  A worker `Load`s a batch (the step's inputs leave their inventory, all or none), `Work`s it
  with a held action that adds one unit of work, and `Collect`s the product. No menu and no
  progress bar: the offer changes with the station state, and the work prompt says in words how
  far along it is (`#LF-Chain_Work_Fresh`, `_Mid`, `_Late`).
- ✅ **Progress belongs to the station.** Every accepted unit adds to the same number, whoever does
  it, so two workers on one barrel finish it in half the time, and three workers on three barrels
  clear three batches in the time one worker clears one. The server rate-limits each worker per
  station (`m_iMinTickMs`, 1200 ms against a 2 s hold), checks reach (`m_fReach`, 3.5 m), the
  hideout access when the barrel carries `LF_HideoutMachineComponent`, and the licence, level and
  equipment gates. Refusals toast `#LF-Chain_*` keys and log `[LFDebug:Chains]`.
- ✅ **Site heat** (`LF_ChainSites`): stations with the same `m_sSiteId` share a number that rises
  with accepted work (`m_fPerWork`), fades with time (`m_fDecayPerSecond`, computed when read) and
  caps at `m_fMax`. `GetHeat`, `GetRange` and `GetWarmSites` are what the smell and smoke trace
  (#418) reads. Heat is not saved; a restart starts every site cold.
- ✅ **Bus events** `LF_OnChainStep` (load, ready, collect) and `LF_OnChainWork` (each accepted
  unit, with the site heat and smell range). See `docs/architecture/events.md`.
- ✅ **A half-finished batch survives a restart.** Payload `chainstations`, save version 20,
  matched by the station's `m_sStationKey` (unique and stable, not the entity id). A saved batch
  that arrives before its station waits and is applied when the station registers; a batch for a
  chain or step the station no longer runs is dropped and logged. Loading twice converges.
- ✅ The core ships `Prefabs/Chains/LF_ChainBarrel.et` (the vanilla open metal barrel with the
  station component, the sound data and the three actions) and three of them on DebugWorld at
  `300 0 322`, `304 0 322` and `308 0 322` (`Street_Chains.layer`, site `Chains_Site`, chain
  `cider`: 4 apples to 1 apple juice, 8 units of work). Work plays the vanilla pick-up gesture and
  `SOUND_PICK_UP`; better animation and sound follow the animation pipeline (#340).
- ✅ `LF_ProcessAction.TryConsumeInputs` / `RestoreInputs` are the all-or-nothing input removal the
  instant processors already did, extracted so the stations share it.
- Tests: `stations/progress` (LOGIC), `stations/world` (WORLD), `persist/chain-stations-roundtrip`
  (PERSISTENCE). The suite's version checks now read 20.
- ✅ **Moonshine (#420, tier 1, config only).** `moonshine` in `ResourceChains.conf`: `mash` at a
  barrel (4 plums and 2 cabbages to 2 water bottles of wash, 12 work), then `distill` at a still
  (2 bottles of wash to 1 `LF_ContrabandCrate`, 16 work). No gate; heat 3 per work, range 60 m.
  The crate is what the existing convoy (barn to pub) pays for. `Prefabs/Chains/LF_ChainStill.et`
  is the barrel prefab with the boiler mesh and `distill`. The hidden DebugWorld site
  (`Street_Moonshine.layer`, site `Moonshine_Site`) has three barrels at `380/384/388 0 300`
  and a still at `384 0 305`, about 150 m from the police station. Test: `chains/moonshine`
  (WORLD). No save change.
- ✅ **Smoke and smell (#418).** `LF_ChainTrace` (server): the first accepted work wakes one 5 s
  timer that runs only while a site is warm or a station still smokes; nothing is checked per frame.
  Each sweep sets every station's smoke level from its site heat (`LF_ChainTrace.SmokeLevel`:
  none, then thirds of `m_fMax`; the level is a replicated int on `LF_ChainStationComponent`, so it
  changes only at a threshold and a late joiner sees it) and lets every player inside the
  heat-scaled range (`LF_ChainSites.GetRange`) notice the smell: on-duty officers get
  `#LF-Chain_Smell_Officer`, everyone else `#LF-Chain_Smell_Witness`, once per listener and site per
  60 s. Smoke is the vanilla `Smoke_Smokestack_Medium` (levels 1 and 2, scaled 0.5 and 1) or
  `_Big` (level 3), a local particle on every non-dedicated machine. Each notice is an
  `LF_OnChainSmell` on the bus; `LF_ChainSmellLead` turns it into a police service call
  (`#LF-Chain_SmellLead`, one per site per 5 min) so dispatch logs and radios it and the service
  board pins it. NPC witnesses do not smell yet. Test: `chains/trace` (LOGIC). Heat is still not
  saved; no save change.
- ⚠️ Not built: nothing in this section. Manual: three
  players on three barrels, see the PR.

## Jobs / Level / Licenses / Whitelist

### `LF_PlayerJobComponent` — `Feature/Jobs/LF_PlayerJobComponent.c`
- ✅ Per-player job state, per-job level/XP maps, license gates (POLICE→POLICE_ACCESS,
  MEDIC→MEDIC_ACCESS), client→server `RpcAsk_SetJob` re-validates licenses.
- ✅ **Fruit-catcher mint closed** (verified 2026-08-30): the server clamps any
  claimed score through `LF_GetFruitCatcherRewardCount` to
  `LF_FRUIT_CATCHER_MAX_SCORE` (100); pin test `security/fruitcatcher-reward-clamp`.
- ✅ **Fruit-catcher claim loop closed** (verified 2026-11-07): a claim
  pays only a server-opened round. The minigame calls `AskStartFruitCatcherRound`
  when a round starts; the server records the round id and start time
  (`LF_FruitCatcherRound`) and `AskClaimFruitCatcherReward` sends that id back.
  One claim per round, same fruit, at least `LF_FruitCatcherRound.MIN_ROUND_SECONDS`
  (30 s) after the start, and the caller must hold FARMER. The minigame screen
  is not in this repo; when it lands it must call the start ask. Pin tests
  `security/fruitcatcher-rounds`, `security/job-workplace-and-seed`.
- ✅ **Job switch and kit gates** (verified 2026-11-07): a
  client-requested switch (`RequestJobFromClient`, behind `RpcAsk_SetJob`) needs
  an `LF_JobSignUpAction` for that job within `LF_JobGuards.WORKPLACE_RADIUS`
  (5 m) and `SWITCH_COOLDOWN_SECONDS` (30 s) since the last switch; going
  UNEMPLOYED needs no workplace. Only delivery jobs have a placed workplace today,
  so a client cannot switch itself to MINER/LUMBERJACK/FARMER/MEDIC/POLICE by RPC.
  `GiveJobEquipment` hands each job's kit once per account per server session
  (`LF_JobGuards.ClaimEquipmentGrant`). Pin test `security/job-switch-guards`.
- ✅ **Debug-license backdoor removed** (verified 2026-08-30): `RpcAsk_DebugGrantLicense`
  no longer exists anywhere in `Scripts/Game`.
- ✅ **No passive paycheck** (verified 2026-11-08, M1-07): the timer paycheck
  (700-1000 cash every 300 s to every job, unemployed included) is deleted; the
  component has no frame event. `LF_JobConfig` has no salary fields. Money comes
  from completed runs, trader sales, police duty pay (`LF_DutyPay`), and the
  unemployment benefit (`LF_UnemploymentBenefit`). Pin test `jobs/no-passive-paycheck`.
- ✅ **Civilian run XP** (verified 2026-10-11): `AddExperience` forwards to
  `LF_CivilianJob`. A completed courier, fuel, or taxi run adds
  `m_iJobXpPerRun` XP and `PullCareer` copies the level onto this component,
  so `OnLevelUp` fires. Level 1 may take courier work, level 2 opens fuel
  delivery, level 3 opens taxi. Under-level signup refuses and is logged.
- ✅ **Job pacing is config and hidden** (2026-11-08, #156): `LF_Jobs.conf`
  sets `m_iJobXpPerRun` (100), `m_iJobXpFirstLevel` (500), and
  `m_fJobXpLevelGrowth` (2): 5 runs open fuel delivery, 15 open the taxi
  (`LF_CivilianJob.RunsToReach`). No level text reaches the player: the
  level-up hint ("grade N, +N%") and the grade in the hire hint are gone
  (`#LF-Job_HiredHidden`). When a run opens new work the drop-off shows
  `#LF-Job_NewWork` titled with the job name (`LF_CivilianJob.NewlyOpened`).
  Pay per run is unchanged (40/60/80): 5 parcels then 5 fuel runs is about
  500 cash, so a new player can buy the S105 (1000, from the bank) after about
  10 runs. Test: `jobs/pacing`.
- ✅ **Job tool grant** (2026-11-08): miner gets
  `{23341E2840F5ACB4}Prefabs/Tools/Pickaxe.et` (the old `Prefabs/Tools/Axe/Pico.et`
  never existed), lumberjack gets `{9CAC68BA551417A4}Prefabs/Tools/Axe/Axe.et`
  (the old GUID was wrong). Spawn goes through `LF_Utils.SpawnEntityPrefab`; a
  tool that does not fit is logged and left at the player's feet.
- ⚠️ Whitelist branch in `SetJob` is dead (only POLICE is restricted and it is
  special-cased). Hard-coded Spanish strings (not localized). Per-job level/XP
  persistence is convention-only, no SaveData pair.

### `LF_CivilianJob` — `Feature/Jobs/LF_CivilianJob.c`
- ✅ Workplace signup, one active run, cash once, abandon pays nothing
  (verified 2026-10-11). The bank balance stays 0. Parcel cargo is
  `{9CA41609F6C806AE}Prefabs/Items/Roleplay/LF_Parcel.et`, a cardboard box carried
  in both hands (#155: the bare `HandCarryItem_Base.et` had no mesh, so the
  hands were empty on a client). Test: `items/carry-meshes`. Fuel cargo is
  `{12D5AD21E383B768}Prefabs/Items/Fuel/Jerrycan_01/Jerrycan_01_item.et` and
  the drop consumes it. The taxi fare uses the rank vehicle
  `{54C3CC22DEBD57BE}Prefabs/Vehicles/Wheeled/S105/S105_beige.et` and does
  not need a second client. A missing cargo prefab or a full inventory logs
  `LF_Debug.Error` and that run does not pay. DebugWorld places
  `Roadmap_Job_Depot` (post counter
  `{28F9F4DC40089B51}Prefabs/Props/Services/Post/CounterPostOffice_01.et`),
  `Roadmap_Job_Drop`, `Roadmap_Job_TaxiPad`, and `Roadmap_Job_FuelCan` on
  `Street_Jobs.layer`: the counter on the post office porch, the drop on a
  house porch at the east end, the taxi pad at the bus stop by the square, the
  fuel can at the fuel station.

- ✅ **Sign-up offers on a dedicated client** (2026-11-08, #157): the greyed reason reads
  `#LF-Job_MoreRunsFirst` ("Do more parcel runs first"), not "Your work level is too low".
  A client reads the level replicated on `LF_PlayerJobComponent` (the career map is
  server-only), so fuel and taxi open on the client once the server levels the career.
- ✅ **The toasts say what to do next** (2026-11-08, M1 RC QA): signing up shows the job as the
  title and the next step as the body (`LF_CivilianJob.SignedUpKey`: "Pick up a parcel at this
  counter"), not the job name twice. Picking up a parcel toasts where the drop is
  (`RunStartedKey`, `#LF-Job_RunStarted_Courier`, 8 s).
- ✅ **Pick up, Complete, and Abandon work on a dedicated client** (2026-11-08, LOGIC
  `jobs/client-offers`): the offers read the server-only sign-up and run maps, so on a hosted
  server "Pick up parcel" stayed greyed ("Sign up at the workplace first") after a good sign-up.
  A client now reads the job replicated on `LF_PlayerJobComponent` (set at sign-up and when a
  restored sign-up is put on a spawned body) and, for Complete and Abandon, the cargo in its own
  inventory (`LF_JobStartRunAction.ClientRefusal`, `LF_CivilianJob.RunOfferOpen`). The server
  re-checks everything when the action runs.
- ✅ (#619) The parcel's inventory description reads "Deliver to: <place>" (`LF_ParcelComponent` sets it from `#LF-Item_Parcel_Deliver` and `#LF-Job_ParcelDest` at spawn; test `jobs/parcel-description`). Demo: `session.mjs parcel-proof` (carry pose and inventory shots). Carry pose (#619): the parcel uses the vanilla jerrycan carry (`Assets/Items/Fuel/workspaces/Jerrycan.agr` / `Jerrycan_player.asi`, both arms low in front, box at the waist) instead of `player_carry`, which put the arms across the chest with the box floating beside the character.

### `LF_JobManager` — `Feature/Jobs/LF_JobManager.c`
- ✅ `GetGatherReward` and `GetProcessReward` stay 0 unless a job scope calls
  `SetScopeReward` for that prefab (verified 2026-10-11). Gather and process
  mint nothing on their own. The player sells the product to a trader. A
  scope payout, when one exists, is cash via `LF_MoneyUtils.GiveCash`. The
  ATM stays the only cash-to-account boundary.
- ✅ `#EL-Job_Earned` is `Earned $%1 for work!` and `GiveReward` passes the
  paid amount through `WidgetManager.Translate` (verified 2026-10-11).

### `LF_PlayerLevelComponent` — `Feature/Level/LF_PlayerLevelComponent.c`
- ✅ Level/XP/skill-point state; `GetExperienceForNextLevel = level * 100`;
  passive bonuses (`gatheringSpeed 1+(level-1)*0.1`, `gatheringAmount` same,
  `saleBonus 1+(level-1)*0.05`); `CheckLevelUp` drains XP cascades, +1 SP and
  +1 total SP per level.
- ⚠️ `SetLevel`/`SetExperience`/`SetSkillPoints`/`SetTotalSkillPointsEarned`
  have no server guard, no validation, no recompute (inconsistent state
  possible). Saves on every XP gain (DB-write hazard). `GetBonusesText`
  returns a Spanish literal.

### `LF_LicenseManagerComponent` — `Feature/License/LF_LicenseManagerComponent.c`
- ✅ License catalog (26 entries), `CanUnlockLicense` (owned/config/whitelist/
  level), `CanAffordLicense` (SP cost), `PurchaseLicense` (spends SP +
  unlock), `HasLicense`, persistence restore.
- ✅ **Whitelist mapping and purchase ordering fixed** (verified 2026-08-30):
  `GetLicenseWhitelistJob` maps MEDIC access to MEDIC and police licenses to
  POLICE; `PurchaseLicense` validates requirements before `UnlockLicense`
  spends skill points. `SetUnlockedLicenses` is the persistence restore seam,
  not a player purchase path. Remaining: not localized; `m_bIsInitialLicense`
  is used for initial grants.
- ✅ **Replicates as a bitmask** (verified 2026-11-07: dedicated/* tests, server side; client receipt is manual): the old `[RplProp] ref array` broke the
  simple-types rule. The server list stays local; `m_iUnlockedBits` (an int
  `RplProp`, `LF_LicenseBits`) is written on every change. On a client
  `HasLicense` and `GetUnlockedLicenses` decode the mask. Bit order is
  `LF_LicenseBits.Order()`: append new types, never reorder.

### `LF_WhitelistManager` — `Feature/Whitelist/LF_WhitelistManager.c`
- ✅ **The playtest host is police from the first join** (2026-11-08, LOGIC
  `whitelist/first-player-police`): with `LF_MissionHeader.m_bLF_FirstPlayerPolice` (on in
  `LF_PlaytestTown.conf`, off by default), the first player to join while the police roster is
  empty is added to it (`$profile:LifeFramework/whitelist.txt`), so the maintainer can clock in
  on his own playtest without writing the file. That file is the one roster: the duty desk and
  the join gate read it. The unused `Whitelists/Police.txt` and `Whitelists/Medic.txt` lists on
  `LF_WhitelistRespawnHandlerComponent` (nothing read their flags) are gone from
  `GameMode_Roleplay.et`; the component keeps the CONNECT join gate.
- ✅ Pure static in-memory whitelists; `IsFactionRestricted` (POLICE,
  MILITARY, MAFIA), `IsJobRestricted` (POLICE only), add/remove/get.
- ✅ **Compiles** (verified 2026-08-30): `enum LF_EFactionType` is now defined
  in this file (CIVILIAN/POLICE/MILITARY/MAFIA).
- ✅ **Restart save and police gate** (verified 2026-10-11): the manager
  writes `$profile:LifeFramework/whitelist.txt` itself, version first, and
  `Load` replaces the cache. A second load does not duplicate entries. A
  missing file logs `LF_Debug.Error` and leaves the police faction closed.
  Civilian stays open. `MayJoinFaction` refuses a uid that is not on the
  police list and logs that refusal. The server calls it in
  `LF_DutyLocker.RefuseClockIn` when a player clocks in at the duty desk (M1-11;
  the faction screen is gone). A client has no whitelist; the desk offer reads
  the roster bit of `LF_AccountReplication`. A uid
  that was not in the file is still refused after load. This file is not
  `LF_PersistenceComponentSerializer`.
- ✅ **An unreadable file is never overwritten** (2026-11-07, test
  `restart/whitelist-unreadable-file-kept`, verified 2026-11-07): a file that exists
  but cannot be opened, has no or a wrong version, or fails to parse sets
  `IsLoadFailed()`. `Save()` then refuses with `LF_Debug.Error` and returns false,
  so the next admin add cannot rewrite the file with only the new entry. A
  missing file is a first run and saving stays allowed. A later good `Load`
  clears the flag.
- ⚠️ Getter returns the live internal array (callers can mutate without logging).

### `AdminChatChannel` — `Feature/Chat/AdminChatChannel.c`
- ✅ **Server re-checks the admin role** (verified 2026-11-07):
  `ProcessMessage` on the authority refuses any sender without
  `EPlayerRole.ADMINISTRATOR` (`IsAdminSender`) before parsing `!wadd`, so a
  modded client cannot whitelist itself for POLICE. Pin test
  `security/admin-chat-role`.

### `LF_BuildingManagerComponent` — `Components/BuildingSystem/LF_BuildingManagerComponent.c`
- ✅ **Placement gated and paid** (verified 2026-11-07): the server
  (`PlaceHeldBuilding`, behind `RpcAsk_SpawnEntityPrefab`) requires the held
  performer for that prefab, a position within `MAX_PLACEMENT_DISTANCE` (5 m)
  of the character, and consumes the held item (`ConsumeBuildingItem`: one off
  a quantity stack, else the item is deleted) before spawning. The client
  preview stops offering placements past that range. Pin tests
  `security/building-placement-range`, `security/job-workplace-and-seed`.

### `LF_UnemploymentBenefit` — `Feature/Jobs/LF_UnemploymentBenefit.c`
- ✅ "Collect benefit" (`LF_CollectBenefitAction`) at `Roadmap_Job_Depot`
  (verified 2026-11-08: LOGIC test `jobs/unemployment-benefit`; in-world collection
  is manual). The server reads the bank balance (`LF_ATMManager`) and pays
  `m_iBenefitAmount` cash when the balance is strictly below
  `m_iBenefitBalanceThreshold`, the player is not on duty as police and has no
  active delivery run, and `m_fBenefitCooldownSeconds` passed since that
  account's last collection. Values in `Configs/Jobs/LF_Jobs.conf` (flagship
  $100, below $500, 1200 s). Pick-up sound, then a `#EL-Benefit_Paid` toast; a
  refusal toasts its reason. A full inventory pays nothing and does not start
  the cooldown.
- ✅ The cooldown survives a restart (M1-10): the save holds seconds since the
  last collection, because world time starts over.

## Crime & Police

### `LF_Breach` — `Feature/Crime/LF_Breach.c`
- ✅ Configured charge opens an `LF_LockComponent` after the fuse and leaves it
  open until repair (verified 2026-10-11). `Arm(targetId, playerUid)` is the
  call later robberies use. Fuse 0 opens in that call. Wanted and the police
  notify fire once, at arm time, not when the offer refreshes. A second charge
  on an open target refuses. Police can always repair; the owning organization
  can repair its own door. `Export` / `Apply` put the open lock back in memory
  but are not wired into the save serializer, so a restart loses it. The
  vanilla charge is not detonated. DebugWorld places `Roadmap_Breach_Door` and
  `Roadmap_Breach_Charge` on `Street_Breach.layer`. The charge prefab is
  `{33CBDE73AB48172A}Prefabs/Weapons/Explosives/DemoBlock_M112/DemoBlock_M112.et`.
  **Charge consumed** (verified 2026-11-07): `Arm(targetId, playerUid,
  chargeCarrier)` takes one charge from `chargeCarrier` before the wanted bump and
  refuses (`arm refused: ... carries no breach charge`) when there is none. The
  door, bank vault, and hospital cabinet actions pass the acting player; the
  door action shows `#EL-Breach_NeedCharge`. A null carrier is the
  server-scripted arm used by tests. Pin test `security/breach-needs-charge`.
  `EL_Door_Base` inherits
  `{776E0766BBDB17A3}Prefabs/Structures/BuildingParts/Doors/Door_HouseWooden_E_A_LEFT_EXT_COV_B.et`.
- ✅ **Client offers** (verified 2026-11-07: dedicated/* tests, server side; client receipt is manual): the target table is server-only, so
  `LF_BreachTargetComponent` mirrors open/armed as `RplProp`s
  (`SetReplicatedState`, written from `Open`/`Repair`/`Apply` and the fuse
  schedule). `CanArmForOffer` / `CanRepairForOffer` ask `LF_Breach` on the
  server and the mirror on a client. A client offers repair to police and, on an
  organization door, to anyone; the server decides membership.
- ✅ **Charge lifetime** (2026-11-08): a charge whose fuse cannot
  fire is dropped with `LF_Debug.Error` (`CancelCharge`) instead of leaving the
  target armed forever: no world, a placed door without
  `LF_BreachTargetComponent`, a timer that could not be scheduled, or the door
  deleted before the fuse fired (`LF_BreachTargetComponent.OnDelete` also
  removes the callqueue entry). The cancel also clears the replicated armed
  mirror. An unplaced, script-driven target keeps its charge and opens through
  `Advance`. `Configure` clears the armed flag and elapsed time; the open flag
  is kept. Test `robustness/lifetime`.

### `LF_Bank` — `Feature/Crime/LF_Bank.c`
- ✅ A vault calls `LF_Breach.Arm`, then spills a config number of bags of
  MoneyStack cash (verified 2026-10-11). The haul never deposits or withdraws
  `LF_BankAccount`. Civilian-only, cooldown, and minimum police on duty use
  the same comparisons as `LF_RobAction`. A second breach inside the cooldown
  spawns nothing. Over-capacity requests are cut to the configured bag count.
  Carry leaves the cash on the bag; dropping it leaves that cash in the world.
  `Export` / `Apply` restore the ground bags and do not write a bank balance;
  they are not wired into the save serializer, so a restart loses the bags.
  DebugWorld places `Roadmap_Bank_Vault` at 193.09 0.18 265 (flush on the bank's east
  wall, standing on the plinth; it stood 1 m out on the grass until 2026-11-11), the bell box
  `Roadmap_Bank_AlarmBox` above it, `Roadmap_Bank_Bag` at the wall foot north of the door
  (193.42 0.03 266.4) and the rob kit chest against the wall south of it (193.5 0 262.3),
  all on `Street_Bank.layer`. The bag is
  `{65DE48DCC1D4DD01}Prefabs/Items/Roleplay/LF_MoneyBag.et` (the vanilla `SackUniversal_01`
  sack at 0.4, about 0.54 m tall). Cash is
  `{5439738849229352}Prefabs/Items/Currencies/MoneyStack.et`. The shell is
  `{D1859197E1AB4E8A}Prefabs/Structures/Services/EL_Bank.et`, which inherits
  `{363A616E6C64BB7F}Prefabs/Structures/Services/Government/MunicipalOffice_E_01/MunicipalOffice_E_01.et`.
  That building is capture-only and is not placed in the debug row.

### `LF_Hospital` — `Feature/Crime/LF_Hospital.c`
- ✅ Hospital till pays `{5439738849229352}Prefabs/Items/Currencies/MoneyStack.et`
  through the civilian, cooldown, and minimum-police gates and does not deposit
  to the bank (verified 2026-10-11). The placed action is `LF_RobAction` on the
  till's single `ActionsManagerComponent`. Wanted rises by 1. One police alert per
  successful rob; an offer refresh does not alert. DebugWorld places
  `Roadmap_Hospital_Till` at `257 0 256.4` (hospital porch) on `Street_Hospital.layer`.
- ✅ The cabinet is a forced-entry container. `BreachCabinet` calls
  `LF_Breach.Arm`. When that lock opens it spawns the configured medicine
  prefabs: field dressing, morphine, saline, and tourniquet. A missing prefab
  logs and that line is skipped. A second breach while the cabinet is open
  spawns nothing and does not alert again. `Roadmap_Hospital_Cabinet` is at
  `198 0 120`. Items stay ordinary; confiscation of a contraband flag belongs
  to the contraband scope.

### `LF_Murder` / `LF_Zones` — `Feature/Crime/`, `Feature/Moderation/` (M1-27, #125; zones #242)
- ✅ **Murder makes you wanted** (test `crime/murder`). Every character death
  reaches `LF_GameModeRoleplay.OnControllableDestroyedEx` on the server. A
  player who kills another player or an NPC gets wanted +`m_iMurderWanted`
  (`LF_Police.conf`, default 3), one `LF_WantedReports` report with the
  `#LF-Offence_Murder` offence and their description (no name), a radio
  dispatch to every on-duty officer (`LF_Dispatch.Report`, no site alarm), and a toast. Not murder: a death with no player behind
  it, a self-kill, a destroyed vehicle, and (config
  `m_bOfficerKillOfWantedExempt`, default on) an on-duty officer killing a
  wanted suspect. Each decision logs `[LFDebug:Murder]`. No automatic ban.
- ✅ **Zone rule sets as data** (#755; tests `zones/rules`, `zones/ruleset-backcompat`). `LF_Zones.conf`
  holds a per-rule table `m_aRuleSets` of `LF_ZoneRuleSet` (rule, blocksPlayerDamage, suppressesWanted,
  allowsRobbery, allowsVehicleTheft, allowsWeaponDraw, fineMultiplier, copsEnter). Shipped: SAFE blocks
  player damage, LAWLESS and NORMAL change nothing. A rule with no entry falls back to the old
  `m_bSafeBlocksPlayerDamage` / `m_bLawlessSuppressesWanted` flags. Callers ask
  `LF_Zones.RuleSetAt(pos)`; today only blocksPlayerDamage and suppressesWanted have consumers (hit
  refusal, `LF_Murder`). The `EVENT` rule waits on D5 (#757); ids stay NORMAL 0, SAFE 1, LAWLESS 2.
- ✅ **Spawn safe zone** (test `safezone/blocks`). `LF_ZoneComponent` (rule SAFE) on
  `LF_SafeZone_Square` (`Gamemode.layer`, fountain 115 0 250, radius 30 m as
  the component attribute) registers a flat circle. The modded
  `SCR_CharacterDamageManagerComponent.HijackDamageHandling` swallows a hit
  when a player hurts a different player and either stands inside: no health,
  bleeding, or ragdoll. NPC, world, and self damage pass. Weapons still raise
  and fire. No police exemption: arrest is an interact action. Each refusal
  logs `[LFDebug:SafeZone] refused ...`; the shooter gets a toast at most every
  8 s. Two `safe_zone` notice boards (`Sign_SafeZone_Spawn`,
  `Sign_SafeZone_East`) say so in the world.
- ✅ **Zone service** (#242, tests `zones/rules`, `safezone/blocks`). The safe zone
  code is now `LF_Zones` (`Feature/Moderation/LF_Zones.c`): a zone is an entity with
  `LF_ZoneComponent`, a shape (circle, box turned by entity yaw, polyline corridor
  with a half-width) and a rule set: SAFE (the damage block above), LAWLESS, NORMAL.
  `LF_Zones.RuleAt(pos)` answers for every caller; overlaps resolve safe, then
  lawless, then normal; shapes are flat (XZ). What each rule set does lives in
  `Configs/Moderation/LF_Zones.conf`: `m_bSafeBlocksPlayerDamage` (on) and
  `m_bLawlessSuppressesWanted` (off: lawless alone does not switch off wanted; on,
  `LF_Murder` skips a murder in a lawless zone and logs the reason). A zone added,
  changed or removed logs `[LFDebug:Zones]` and publishes `LF_OnZoneChanged`. The
  incident log stamps `m_sZone` (safe/lawless/normal) on each record and in its log
  line; the stamp is not saved and a loaded record is stamped again from its position.
  #243 (quarry lawless) is one entity: an `LF_ZoneComponent` with rule LAWLESS.
- ✅ **Lawless quarry** (#243, test `zones/quarry`). `LF_LawlessZone_Quarry` (`Gamemode.layer`, circle 32 m at 178 0 172 over the mining site) is the one lawless area; the plaza zone stays safe and the town normal. Two `lawless_zone` signs (`Sign_Lawless_Entry` at the lane, just outside; `Sign_Lawless_Pit` at the east edge) say so; the catalog row `lawless_zone` carries the rule text. The harness `state` line now ends its zone as `zone=safe|lawless|normal`; `session.mjs quarry-proof` reads it at both signs and the plaza. No fence or gate yet; no save change.
- ✅ Both civilian spawn points stand on the square (the second moved from
  170 0 199, by the quarry, to 121 0 253); `safezone/blocks` asserts every
  civilian spawn point is inside a zone. The hospital wake (M1-26) is not a
  spawn and stays outside.

### `LF_RobAction` / `LF_RobWeaponAction` / `LF_RobVehicleAction` — `Feature/Crime/`
- ✅ Cash/weapon/vehicle robbery; cooldown + civilian-only + min-police-on-duty
  gates; raises wanted level (+1/+2/+3); notifies police. DebugWorld's three
  points set the minimum to 0 so one civilian can run them (verified 2026-10-10).
  The action classes still default to 2 / 1 / 3.
- ✅ **Cash payout + reliable wanted bump** (verified 2026-08-30): the haul is
  `LF_MoneyUtils.AddCash` (never the bank — pin test `crime/rob-reward`
  asserts the bank balance stays 0), and the account cache churn is fixed so
  every robbery escalates wanted.
- ✅ **Fuel-station till** (verified 2026-10-11): `Roadmap_Fuel_Till` on
  `Street_Fuel.layer` at `259 1.04 241.2` (on a counter at the fuel station) is
  `{4E4E1620C6B02E1F}Prefabs/Props/Commercial/EL_FuelTill.et`, which inherits
  `{51D31AB07AE3C1C7}Prefabs/Props/Commercial/CashierShop_01.et`,
  with `LF_RobAction`. Payout is a config range (`m_iMinPayout`..`m_iMaxPayout`);
  the server rolls it, and min equal to max pays that fixed amount. The debug
  till pays 150 MoneyStack cash. The bank balance stays 0. Wanted rises by
  `m_iWantedIncrease` (default 1) and still clamps at 5. A second attempt
  inside the cooldown is refused and logged with `LF_Debug.Error`. The police
  alert fires once, inside `CommitRobbery`, not while counting on-duty police
  and not when the offer refreshes. The fuel station building
  `{9269BD1B6BF3A441}Prefabs/Structures/Commercial/FuelStations/FuelStation_E_01/FuelStation_E_01_V1.et`
  is the capture prefab and is not placed in the row. Player strings:
  `#EL-Fuel_Till_Rob`, `#EL-Rob_Cooldown`, `#EL-Rob_CiviliansOnly`,
  `#EL-Rob_NeedPolice`.
- ✅ **Tests call the real actions** (2026-11-07; red-proofs observed
  2026-11-08). `crime/rob-reward` (WORLD) runs `LF_RobAction.CommitRobbery` with a
  real character: MoneyStack cash, bank 0, wanted +1, cooldown, civilian gate.
  The weapon and vehicle grant/spawn, wanted, and alert moved into
  `LF_RobWeaponAction.CommitWeaponRobbery` and
  `LF_RobVehicleAction.CommitVehicleRobbery` (wanted `WANTED_INCREASE` 2 / 3;
  `PerformAction` keeps the server-only guard), and their civilian check into
  `LF_RobAction.IsFactionAllowedToRob`; `crime/rob-weapon-vehicle` (WORLD)
  drives them. The earlier `crime/rob-reward` and `crime/rob-wanted-clamp`
  never called an action and are gone; the clamp is pinned by `roadmap/fuel`.
- ✅ **No account, no robbery** (verified 2026-11-07: dedicated/* tests, server side; client receipt is manual): `CommitRobbery` refuses and logs
  `robbery refused: no account` instead of paying without the civilian gate and
  wanted bump. `PerformAction` runs on the server only; offers read
  `LF_AccountReplication`, so the till is offered on a dedicated client and the
  min-police gate uses the replicated count. The success toast goes to the robber
  only (`LF_Utils.NotifyEntity`), the alert to on-duty police only.
- ✅ **Weapon/vehicle cooldown in seconds** (2026-11-08, test `robustness/lifetime`): both
  compare through `LF_RobCooldown.IsActive`, which converts the seconds attribute
  to world-time milliseconds (before, 300 s was 0.3 s). A never-robbed action
  has no cooldown. `PerformAction` grants/spawns, raises wanted, and alerts on
  the server only. `LF_RobVehicleAction` no longer notifies from its on-duty
  count (that ran every offer refresh) and refuses an empty vehicle prefab with
  `LF_Debug.Error`; the spawn goes through `LF_Utils.SpawnEntityPrefab`.
- ⚠️ Near-duplicated guard logic across all three.
  Police alerts go through `LF_Dispatch.Report` (M1-20).

### `LF_Jail` — `Feature/Police/LF_Jail.c`
- ✅ Placed cell and timed sentence (verified 2026-10-11). `GetJailPosition()`
  reads the cell origin. Sentence length is wanted level times the config
  step, clamped to the config max. Arrest of a clean player refuses. Expiry
  reduces wanted by the configured amount. Early release refuses. A locked
  jail door returns a prisoner who leaves the volume; an open door (the shared
  `LF_Breach.Arm` on `Roadmap_Jail_Door`) does not, and the remaining time
  stays. `Export` / `Apply` put the remaining seconds back, and `ExportAll` /
  `ApplyAll` carry every serving sentence through the save. DebugWorld places
  `Roadmap_Jail_Door`, `Roadmap_Jail_Cell`, and `Roadmap_Police_Desk` on
  `Street_Jail.layer` (the desk on `Street_Police.layer`). The placed door is
  `{787617D5B683FE41}Prefabs/Structures/BuildingParts/Doors/EL_JailDoor.et`, which
  inherits
  `{B597177F09247CE9}Prefabs/Structures/BuildingParts/Doors/Door_PrisonCell_E_A_Left_INT_ST_B.et`
  and adds `LF_LockComponent`, so the prison mesh and the shared breach actions
  stay together. That vanilla door is the prefab the world test loads.
  The officer action `LF_JailArrestAction` is on the suspect. `LF_JailEscort`
  is the vehicle path: an on-duty officer loads a wanted player into the nearest
  free non-driver seat of a base-game vehicle. The sentence starts when they
  arrive at the cell, not when they enter the car. A player who is not wanted
  is refused and logged. Early release stays refused. A reconnect or a
  restart keeps the remaining sentence. DebugWorld places
  `Roadmap_Escort_Car` at `247.5 0 238` (beside the police station) on `Street_Jail.layer`, the police M151 with
  `LF_JailPutInVehicleAction`. The action is listed at the car. Performing it
  still requires an on-duty officer and a prisoner on foot. (verified 2026-10-11)
- ✅ **Sentences survive a restart** (2026-11-07, tests
  `restart/jail-serializer-roundtrip` and `e2e/pass2-verify-load` across a real
  restart, verified 2026-11-07): `LF_Jail.ExportAll` / `ApplyAll` are the `jail` payload of
  `LF_PersistenceComponentSerializer`, appended at version 5. Only serving
  sentences with time left are saved; `ApplyAll` replaces the live set, so a
  second apply converges. A version 1-4 save stops before the payload and leaves
  live sentences alone.
- ✅ **One jail tick** (2026-11-08, test `robustness/single-jail-tick`): the first placed
  `LF_JailCellComponent` owns the jail config and the repeating `LF_Jail.Tick`;
  a second cell logs `LF_Debug.Error` and neither reconfigures nor ticks (two
  cells used to run sentences at double speed). `OnDelete` removes the tick.
  `LF_JailArrestableComponent` hosts `LF_JailEscort.Tick` on one live instance
  and hands it to the next registered one when that host is deleted.
- ✅ **Jail work shortens the sentence (#127, merged, in-dev).** Two job spots in the DebugWorld cell: *Scrub the floor* (`Roadmap_Jail_ScrubBucket`, 30 s off, 6 s of work, 45 s cooldown) and *Sort the laundry* (`Roadmap_Jail_LaundryBasket`, 20 s off, 5 s, 30 s). `LF_JailWorkAction` carries the job id, seconds off, cooldown and work time as config; the client sends nothing. `LF_JailWork.Begin` (server) refuses anyone who is not serving, is already working or is inside that job's cooldown (cooldown toast shows the time left), then bends the prisoner into the existing work pose (`LF_WorkAnim`: PICK for the scrub, DRILL for the laundry). When the work time is up and the prisoner is alive and within reach, `LF_JailWork.Complete` calls `LF_Jail.Credit` once, which takes the seconds off the remaining time (a task past the remaining time ends the sentence like any release), refreshes the sentence slip, and toasts "Job done. Time left: m:ss". Nothing new is saved: the reduction lands on the sentence's remaining seconds that the `jail` payload already carries; the cooldown is session-only. Test `jail/work` (LOGIC); demo `node agent-box/session.mjs jail-work-proof` (needs `session.mjs roster 2` before `up --clients 2`). Localized `LF-JailWork_*`. The work poses are reused, so the scrub and laundry both show the existing bend; a mop or laundry clip is not made.
- ⚠️ Moving a connected prisoner to the cell, or pulling them back into it,
  goes through `SCR_Global.TeleportPlayer` (the vanilla server-side player
  teleport) instead of a server-only `SetOrigin` that the owning client can
  overwrite; a stand-in with no player is still set directly (2026-11-07). A
  prisoner seated in a vehicle is dismounted straight to the target first
  (`GetOutVehicle_NoDoor`; a client-owned player through the vanilla Game Master
  `SCR_EditableCharacterComponent.SetTransform`), so the vehicle stays put (M1-21,
  test `jail/vehicle-arrest`). Not yet checked on a dedicated server with a remote client.
- ✅ **Online time** (M1-21, 2026-11-08, tests `jail/online-time`, `e2e/pass2-verify-load`): the
  sentence counts down only for connected players (`LF_Jail.Tick` reads
  `PlayerManager.GetPlayers`; `TickOnline` is the testable seam). An offline prisoner keeps
  the time they had at logoff and logs `sentence paused` / `sentence resumed`. A prisoner who
  spawns with time left (rejoin, restart) goes back to the cell with their slip (`LF_SpawnLogic`).
- ✅ **Sentence slip** (M1-21, test `jail/slip`): sentencing hands the prisoner
  `Prefabs/Items/Roleplay/SentenceSlip.et` (vanilla personal-papers mesh). `LF_JailSlipComponent`
  prints name, offence (wanted level), and online minutes left; the minutes replicate only when
  the whole minute changes. Hovering it in the inventory reads it (`LF_JailSlipUIInfo`). A
  respawn or a second arrest reprints the carried slip instead of adding one. No new save data.
- ✅ **Visible release** (M1-21, tests `jail/slip`, `jail/online-time`): release opens the cell
  door (`DoorComponent.SetControlValue`, the vanilla door motion and sound; it closes after
  5 s), places the prisoner at the cell release offset (`m_vReleaseOffset`; DebugWorld `0.15 0 10`, outside
  the door), stamps the slip, then shows the toast 1.5 s later. Release runs once per sentence.
  The DebugWorld cell now sentences 60 s per wanted level, max 300 s (it was zero-length).
- ✅ **Jailbreak** (#235, LOGIC `roadmap/jailbreak`, WORLD `e2e/roadmap-jailbreak`, `session.mjs jailbreak-proof`). A serving prisoner who is outside the cell volume while `Roadmap_Jail_Door` is open has escaped: `LF_Jail.NoteEscape` (called each cell tick) reports it once through `LF_Dispatch.Report` as `#LF-Offence_JailBreak` (so `LF_OnCrimeCommitted` is on the bus, the board and pager carry it) and leaves the sentence and wanted level alone; going back inside re-arms it. The door reuses `LF_Breach` (arm, stay open until police repair). **Fixed:** `m_bDebugLocked` was never applied (the component asked for no init event), so the placed jail door started unlocked; `LF_LockComponent` now sets the INIT mask and applies it, and the per-component `m_bStartLocked` workarounds (jail, breach, lockpick) are gone. Harness verb `charge` hands the player a breach charge (dev gate). No save change.


### `LF_PoliceMenu` — `Feature/Police/LF_PoliceMenu.c`
- ✅ `ArrestPlayer` is the client-side entry point. The wanted list
  left the menu in M1-11: wanted people are on the station wanted board. The
  remote fine (`RpcAsk_LF_PoliceFine`, `FinePlayer`, the Fine button) is gone
  (M1-19): a fine is a paper ticket written face to face (`LF_FineTicket*`).
- ✅ **Client→server bridge works** (verified 2026-08-30): the menu routes
  through `SCR_ChimeraCharacter.LF_AskPoliceArrest`, and the server
  handler `RpcAsk_LF_PoliceArrest` validates on-duty officer and wanted
  target before acting. Arrest also requires the configured
  arrest rank (patrol on the flagship, #165). `LF_PoliceUtils` (pure) holds the
  sentence-length math and the reach check for tests; `MAX_FINE` is the
  senior ticket cap.
  `LF_AskPoliceSelfPromote` always refuses.
- ✅ **Arrest starts a sentence** (verified 2026-10-11): the arrest RPC calls
  `LF_Jail.Arrest` after the rank gate and no longer clears wanted or teleports
  to `"127 1 137"`.
- ✅ **Hands-on only** (2026-11-07): arrest refuses a
  target that is not connected or stands beyond `LF_PoliceUtils.POLICE_REACH`
  (5 m, the in-world arrest range) and shows `#EL-Police_TooFar`. Neither the
  framework nor vanilla has a restraint state, so reach is the requirement.
  Test: `security/police-abuse`.

### `LF_FineTickets` — `Feature/Police/LF_FineTicket*.c`
- ✅ **A fine is a paper ticket paid at the police desk** (M1-19, #104;
  decision 2026-11-08). An on-duty officer facing a player picks **Write ticket**
  (the `fine` slot of the player interaction menu, `LF_FineTicketRole`, no
  prefab edit). The server opens the ticket pad (`LF_FineTicketPad`, paper
  `LF_UIScreen`) on the officer's machine: one "Write: <offence>, <amount>" button per offence from
  `LF_Police.conf` `m_aTicketOffences` at its fixed amount; offences above the
  officer's rank cap (`LF_PoliceRanks.FineCap`: patrol 1000, senior 5000) print
  as a plain "senior officers only" line. The server re-checks duty,
  reach (2 m), the cap, and at most 3 unpaid tickets per holder, then hands
  over `Prefabs/Items/Roleplay/FineTicket.et`: number, date, holder, offence,
  amount, deadline, what it rises to, where to pay, officer rank and name. Cues:
  the officer's point gesture, the paper sound from the ticket, the holder's
  pick-up gesture, and the ticket card on the holder's screen. Inventory hover
  and Read (on the ground) show the same text.
- ✅ **Pay ticket** on the punch clock of `LF_PoliceDutyDesk.et`: carried cash
  only (`LF_EntityCashWallet`), never the bank, all or nothing, oldest unpaid
  first. The clock's punch sound stamps it for everyone nearby; the carried
  paper turns into a receipt ("PAID <date>").
- ✅ **Deadlines in online time** (like the jail sentence): unpaid after
  `m_iTicketFirstDeadline` (1800 s) the amount is multiplied once by
  `m_fTicketLateMultiplier` (x2) and the paper updates; after a further
  `m_iTicketSecondDeadline` (1800 s) the holder gets `m_iTicketLateWanted` (+1)
  wanted and a named "Unpaid fine" poster on the wanted board. Still payable
  after; paying does not lift the warrant. Logs: `[LFDebug:Ticket]`.
- ✅ **Persisted** (save version 12, payload `tickets`). A restart's blank
  copy is reprinted on spawn; a respawn hands an unpaid ticket back. Up to 3
  paid receipts per holder are kept for reprinting. Tests:
  `police/fine-ticket-rules`, `police/fine-ticket`,
  `police/fine-ticket-serializer`.

### `LF_DutyAction` — `Feature/Police/LF_DutyAction.c`
- ✅ **The police kit carries the station car key** (2026-11-08, WORLD `police/car-key`):
  the starter police kit adds `Prefabs/Items/Roleplay/LF_PoliceCarKey.et`, a vehicle key bound
  to the shared identifier `POLICE`; `Roadmap_Escort_Car`'s lock is bound to the same, so every
  officer's key opens every station car. The key is part of the duty inventory (clock-out stores
  it with the uniform). The car is a world entity, not an owned record: no garage, no impound,
  back at the station after a restart. `LF_VehicleKeyComponent` now asks for its INIT event, so
  an editor-bound key actually takes its identifier (before, it stayed unbound).
- ✅ **Clock in / Clock out at the duty desk** (M1-11, 2026-11-08; WORLD
  `police/duty-swap`, `police/death-on-duty`, `police/station`; restart via
  `wt e2e`). `Roadmap_Police_Desk` is `Prefabs/Police/LF_PoliceDutyDesk.et`
  (desk + vanilla `ClockPunch_01`). The offer reads the roster bit of
  `LF_AccountReplication`; the server re-checks `LF_WhitelistManager` and
  `LF_DutyLocker` swaps the whole carried inventory, clothes included:
  clock-in stores the civilian tree as `civilian_stash` and restores
  `police_duty` (or the starter police kit, the POLICE entry of
  `LF_SpawnLogic.m_aFactionCharacterItems`); clock-out does the reverse and pays
  duty pay. The faction is the duty role: POLICE only while clocked in.
  Feedback: the vanilla pick-up gesture on the acting client, the range control
  panel button click (`SOUND_RANGECP_STARTBUTTON`) broadcast from the desk, then
  a toast. Death on duty ends the shift: the body keeps the police gear, the
  civilian stash becomes the carried snapshot the respawn restores, and the
  next shift starts from the starter kit. Going on duty starts the rank clock;
  going off duty settles it. `PoliceStation_Debug` and `PoliceDutyNPC` no
  longer offer duty. The debug menu's police button still sets the role without
  a swap (dev only).
- ✅ **Duty pay at the desk** (verified 2026-11-08: LOGIC test `jobs/duty-pay`;
  the 2-client cash check is manual). Going off duty pays the shift's on-duty
  seconds times `m_fPoliceDutyPayPerSecond` (`Configs/Jobs/LF_Jobs.conf`, flagship
  0.5) as cash through `LF_DutyPay.PayAtClockOut`, with the pick-up sound and a
  `#EL-Duty_Paid` toast. Seconds settled by an armory draw mid-shift are kept
  for the clock-out. A second clock-out, a clock-out with no clock-in, and a
  non-police account pay nothing. Unpaid seconds are session state: a restart
  during a shift drops that shift's pay, it never pays twice.
- ✅ Toggling keeps the account resident: `SaveAndReleaseAccount` re-caches
  the account instead of evicting it, so the officer can reopen the police
  menu and re-toggle at once.
- ✅ **Dedicated client** (verified 2026-11-07: dedicated/* tests, server side; client receipt is manual): the toggle
  runs on the server only; the offer and the name (`#LF-Duty_ClockIn` /
  `#LF-Duty_ClockOut`) read `LF_AccountReplication`.

### `LF_WantedReports` / `LF_WantedBoard` — `Feature/Police/`
- ✅ **Crime reports** (M1-11, LOGIC `police/wanted-board`): each crime that
  raises wanted (till, vehicle and weapon robbery, breach, hospital till) calls
  `LF_Dispatch.Report` (M1-20), which calls `LF_WantedReports.Report`. It records the offence and a description of the
  suspect at that moment: clothing display names, the vehicle they sit in, and
  the nearest sign title within `m_fPlaceRadius` (else a 100 m grid square).
  An on-duty officer within `m_fCatchInActRadius` (`Configs/Police/LF_Police.conf`)
  catches it in the act and the report is identified at once. Showing your own
  ID card to an on-duty officer (`LF_ShowIDAction`) identifies every open
  report. Reports come down when wanted returns to 0, and persist as the
  `wanted` payload (save version 10).
- ✅ **Wanted board** `Roadmap_Police_WantedBoard` (`Prefabs/Police/LF_WantedBoard.et`,
  a cork `LF_Sign_Board` printing WANTED): "Read the wanted posters" asks the
  server per read (`RpcAsk_LF_WantedBoard`, the server finds the board near
  the reader). On-duty police get offence, stars, clothing, vehicle, and place;
  everyone else gets the public poster without the place. A name appears only
  on identified reports. The card is an `LF_UIScreen` paper sheet that closes
  4 m from the board; reading plays the map pick-up sound and gesture.
  On-duty police also see the dispatch log under the posters (M1-20).

### `LF_Dispatch` / `LF_SiteAlarmComponent` — `Feature/Police/` (M1-20, #105)
- ✅ **One crime entry point** (LOGIC `dispatch/once`): `LF_Dispatch.Report`
  is what every crime calls (till, vehicle and weapon robbery, breach and bank
  vault, hospital cabinet, murder with no site alarm). It records the crime report, sounds the site alarm,
  radios on-duty officers, and appends a line to the dispatch log. A refused
  crime reports nothing; no crime refuses for lack of police (the per-crime
  minimum stays a framework attribute, 0 on the flagship). `LF_Utils.AlertPolice`
  is gone.
- ✅ **Site alarm** (WORLD `dispatch/site-alarm`): a replicated
  `Prefabs/Police/LF_SiteAlarm.et` spawns at the site, so only players the
  site streams to get it. It loops the vanilla siren
  (`Structures_Siren.acp`, `SOUND_SIREN_LP`) and flashes an orange light for
  `m_fAlarmSeconds` (30). With nobody on duty when the crime starts it is
  `LF_SiteAlarm_Loud.et`: the vanilla loud siren (`Structures_Siren_Loud.acp`,
  600 m), a brighter, faster flash, `m_fLoudAlarmSeconds` (60). A newer alarm
  at the same site replaces the old one. Durations, prefabs, and the radio
  sound are in `Configs/Police/LF_Police.conf`; the sound, hear range, and
  light are attributes on the prefabs. The alarm is a small rotating beacon (vanilla `HazardBeacon_01` at 0.4,
  0.21 m tall, no collision). It stands on the nearest `LF_AlarmMountComponent` within the mount's reach
  (`LF_Dispatch.AlarmSpot`, WORLD `dispatch/alarm-mount`): the bell box on the bank's east wall above the
  vault door (`Roadmap_Bank_AlarmBox`, vanilla `ElectricityBox_small_01`) and the cash van roof. A site with
  no mount (the till counter) keeps the first surface under it (`LF_Dispatch.RestingSpot`, WORLD
  `dispatch/alarm-rests`). It used to be a 1.6 m beacon (3x) on the ground by the vault door and on the road
  behind the van (#571 maintainer review, 2026-11-11). The flash is LV 9 over 3 m, 0.3 m above the beacon
  (loud: LV 10 over 3.5 m); it was LV 12 over 10 m, which lit up the whole van rear on the trailer.
- ✅ **Radio dispatch**: each on-duty officer's own client (Owner RPC on the
  controller) hears a vanilla radio chirp from their body
  (`Props_Radios_Signals.acp`, `SOUND_RADIO_SIGNAL_2`), then a toast
  "All units: <offence> at <place>. Suspect: <clothing>; on foot/driving ...".
  The line is built from the report's description snapshot; it has no name field.
- ✅ **Dispatch log on the wanted board**: the newest `m_iDispatchLogLines` (5)
  calls, "HH:MM <offence> at <place>: N on duty, radioed" or "nobody on duty,
  loud alarm". Session-local (the posters are the persisted record).
- Harness: `state` adds `dispatch=<n> lastdispatch=<offence@place> alarm=none|normal|loud`;
  `node agent-box/session.mjs dispatch-proof` drives the two-client run.
- Manual: listening check (the siren loop and the radio chirp) and the light at
  night; payphones, pagers, and AI police are later.

### `LF_PoliceRanks` — `Feature/Police/LF_PoliceRanks.c`
- ✅ Two ranks (verified 2026-10-11). Patrol officer draws
  `{1353C6EAD1DCFE43}Prefabs/Weapons/Handguns/M9/Handgun_M9.et`, fines up to
  1000 (`LF_PoliceConfig.PatrolFineCap`). Senior officer draws
  `{3E413771E1834D2F}Prefabs/Weapons/Rifles/M16/Rifle_M16A2.et` and fines up
  to the existing 5000 cap. The resolved kit is the intersection
  of that rank's list and the station catalog; a prefab that does not load is
  logged and skipped, and the rank stays usable. An on-duty senior promotes a
  lower officer. 3600 seconds on duty promotes patrol. `RequestSelfPromotion`
  is the self-promotion RPC body and always refuses. Rank does not change
  faction and does not write the police whitelist. Lower ranks still open the
  wanted list. DebugWorld places `Roadmap_Police_Armory` at `241.2 0 244.2`
  (police station forecourt) on `Street_Police.layer`.
- ✅ **One armory draw per duty shift** (2026-11-07): a draw
  that hands out any kit line marks the account; a second draw logs
  `draw refused: <uid> already drew a kit this shift` and shows
  `#EL-Rank_AlreadyDrawn`. `OnDutyChanged` clears the mark. Test:
  `security/police-abuse`.
- ✅ Rank, on-duty seconds, and unpaid duty seconds survive a restart (M1-10,
  `progression` payload, save version 7).
- ⚠️ The shift draw mark is session state, so a restart allows one more draw.
- ✅ **Arrest and escort are config, open to every officer** (2026-11-08, #165):
  `LF_PoliceConfig` (`{916BDC092C01B7D1}Configs/Police/LF_Police.conf`) sets the
  lowest rank that may arrest (`m_iArrestMinRank`) and escort
  (`m_iEscortMinRank`), and the patrol fine cap. Flagship: 0, 0, 1000, so a
  patrol officer arrests on a fresh server. `LF_Jail.Arrest` and
  `LF_JailEscort` re-check the rank on the server for every path (action,
  menu, escort to the cell); the offers read the replicated rank and show
  `#EL-Rank_ArrestDenied` / `#LF-Rank_EscortDenied` when a server raises the
  minimum. A file that does not resolve logs once and uses the same defaults.
  Test: `police/rank-permissions`.

### `LF_Contraband` — `Feature/Crime/LF_Contraband.c` (verified 2026-11-08, M1-14)
- ✅ One convoy. A member of the organization that owns the hideout performs
  `Start convoy` at `Roadmap_Convoy_Start`, a pallet of wooden crates by the barn
  out of town (`466 0 331`), and is handed a crate of spirits:
  `{968892A9535FE6A4}Prefabs/Items/Contraband/LF_ContrabandCrate.et`, a
  hand-carried item on `HandCarryItem_Base.et` with the vanilla
  `BoxWooden_01.xob` mesh (carry pose `player_carry`, `SOUND_PICK_UP`). A member
  already holding one keeps it; full hands log `pickup refused: hands full` and
  toast `#LF-Contraband_HandsFull`. `Deliver contraband` at `Roadmap_Convoy_Drop`,
  an empty crate cage behind the pub on the square (`131.5 0 284`, off the farm lane), takes
  the crate and pays 200 MoneyStack cash in hand once (`SOUND_PICK_UP`). A second
  delivery on that run pays nothing. A non-member is refused and the refusal is
  logged. The hideout machine also outputs the crate. Dressing on
  `Street_Contraband.layer`: crates and a kerosene lamp on the pallet, a parked
  vanilla `S1203_cargo_brown` van (`Convoy_Barn_Van`) at the barn; beer kegs,
  empty crates, bottles, and a lamp at the pub's back door. Signs
  `Sign_ConvoyPickup` (barn front wall) and `Sign_ConvoyDrop` (pub east wall) on
  `Signs.layer` explain both ends. Both action contexts sit 0.6 m up, on the cage.
  Player strings: `#EL-Contraband_Start`, `#EL-Contraband_Deliver`,
  `#EL-Contraband_NotMember`, `#EL-Contraband_Paid`, `#EL-Contraband_Failed`,
  `#LF-Contraband_Loaded`, `#LF-Contraband_HandsFull`, `#LF-Contraband_Crate_Name`.
- ✅ The vehicle the member is in at `Start convoy`, or the first one they get
  into afterwards (`SCR_CompartmentAccessComponent.GetOnCompartmentEntered`), is
  watched. Getting out of it within `LF_Contraband.DROP_RADIUS_M` (25 m) of the
  drop ends the drive and keeps the run for the delivery (`convoy reached the
  drop`); getting out anywhere else, or dying, fails the run and logs it; a
  failed run pays nothing. WORLD `e2e/roadmap-contraband` covers the crate
  (mesh, hand-carry, flagged) and both outcomes of getting out.
- ✅ **Debug drop is close to the barn** (#616). `LF_GameModeRoleplay.m_bConvoyNearDrop`
  (on in `GameMode_Roleplay_Debug.et`, off on the flagship prefab) picks the drop:
  on, the run uses `Roadmap_Convoy_Drop_Near` (`466 0 304`, 27 m from the barn pallet, a
  second crate cage in `Street_Contraband.layer`); off, the pub's `Roadmap_Convoy_Drop`.
  `LF_Contraband.DropIdFor(bool)` / `ActiveDropId()` choose it; the inactive cage hides its
  Deliver prompt (`LF_ConvoyDeliverAction`) and refuses. Pickup sign and loaded toast now say
  "the convoy drop", not "the pub". Test: `roadmap/contraband`, `e2e/roadmap-contraband`.
- ⚠️ Not driven end to end by automation: whether a player can board with the
  crate in hand or must stow it in the vehicle's cargo is a manual check.
- ✅ **Vehicle watch lifetime** (2026-11-08): payment, failure, and
  `Reset` unregister the `OnCompartmentLeft` handler; a restarted run drops the
  old watch and watches the vehicle the player is in now. A disconnect fails the
  running convoy.
- ✅ **The refusal reaches the player** (2026-11-08, #157): a start refused for
  membership shows a toast with the reason, `#LF-Contraband_JoinOrg` (no
  organization: join or found one at the organization desk) or
  `#LF-Contraband_NotHideoutOwner` (your organization does not hold the
  hideout). `LF_Contraband.StartRefusalKey` picks it. Before this the server
  logged `convoy refused: not a member` and the player saw nothing.
- ⚠️ The crate still offers "Start convoy" to everyone: a client cannot see
  organization membership, so the reason arrives after the press, not on the
  offer.

### `LF_ConfiscateAction` — `Feature/Police/LF_ConfiscateAction.c`
- ✅ (2026-11-08, M1-12) Search in the player interaction menu: shown to on-duty
  police within the 2 m reach; deletes the weapons of the wanted player in
  front of them (the action owner), not the nearest wanted player. Weapons are
  collected before deletion. Earlier contract: on-duty police delete weapons
  from a wanted player.
  The offer reads replicated officer and target state; the deletion runs on the
  server only and re-checks the officer's account (verified 2026-11-07: dedicated/* tests, server side; client receipt is manual).
- ⚠️ No compensation or wanted reduction.

## Ownership

### `LF_OrganizationManager` — `Feature/Organizations/` (verified 2026-10-11; ledger and invites 2026-11-08)
- ✅ A character founds one organization. The id comes from `PersistenceIdUtils`,
  never an entity id. The founder invites and kicks. A member can leave. The
  founder cannot leave or be kicked. A second membership is refused.
- ✅ The shared fund is a balance keyed by organization id, the same shape as
  `LF_BankAccount`. Deposit credits it. Withdraw requires the withdraw rank and
  takes the whole amount or nothing. Buying an upgrade (extra storage,
  processing slot, parked vehicle) debits the configured price in one step or
  leaves the fund and the flag untouched. The price and the new flag are logged.
- ✅ Cash moves only as MoneyStack through `LF_OrganizationPlayer`. A short
  removal is returned to the character. A short payout puts the fund back. The
  personal ATM balance is not read or written. The desk is
  `Roadmap_Org_Desk` on `Street_Org.layer` at `288.9 0 255.7` (org registry porch), using
  `{6B7B7CCC74AACA67}Prefabs/Props/Furniture/Desk_01_short.et`.
  The desk mesh origin is its back-right corner (bounds x -1.70..0, y 0..0.907, z 0..0.747), so the top is at y 0.907 and the props sit at x 287.6..288.6 (`Roadmap_Org_Desk_Phone/Files/Notebooks/Clock`, `_Chair`).
- ✅ (2026-11-08, M1-17) The desk opens the registry ledger `LF_OrgLedger`, a
  1989 ledger page on the UI kit (the old `OrganizationMenu.layout` is gone).
  No organization: name field and "Sign and pay $250"; the fee
  (`LF_OrgRules.FOUNDING_FEE`) leaves carried MoneyStack and comes back if the
  registration is refused. Names: 3 to 32 letters, digits, spaces, `- ' & .`,
  unique ignoring case and outer spaces (`LF_OrganizationManager.IsNameTaken`).
  A member's page: name, rank, fund with deposit (and withdraw for the rank
  that may), the processing upgrade, the alley yard lease
  (`LF_OrgHideoutLease`, free, founder takes it), the member list (offline
  members by their record name), and Leave (not for the founder). Found,
  deposit, withdraw, buy, lease, and leave are refused unless an
  `LF_OrganizationDeskAction` stands within `LF_OrganizationPlayer.DESK_RANGE`
  (5 m) (`#EL-Org_TooFar`; test `security/service-range`). The page closes at
  4 m from the desk, on death, and on MenuBack. Sounds: page turn on open,
  pencil on Sign, map close on close. LOGIC `org/registry`, `org/invite-role`.
- ✅ (2026-11-08, M1-17) Invite and remove are face to face, in the `org_invite`
  slot of the player interaction menu (`LF_OrgInviteRole`, registered in
  `LF_GameModeRoleplay.OnGameStart`). A leader facing someone with no
  organization sees "Invite to <org>" (offer, `Gesture_Point`); the invitee
  facing that leader sees "Join <org>" and joins with one press (raised hand,
  hand-over sound); a leader facing a member sees "Remove from <org>"; nobody
  can remove the founder. An offer is session state (`LF_OrgInvites`, not
  saved) and lapses after 2 minutes. The offer side reads the organization
  replicated on each character (`LF_OrgReplication`: id, name, rights bits,
  held offer; RplProps written on spawn, on change, and every 10 s); the
  server decides from the registry. Two-client proof:
  `node agent-box/session.mjs org [--shot]`.
- ✅ `ExportAll` / `ApplyAll` replace the cache, so a second apply does not
  compound the fund. The game-mode serializer writes these records at version 2,
  after survival. A version 1 save has no organization payload and does not
  clear a live fund. Version 3 appends hideout ownership after these records.
  Version 4 appends owned vehicles after the hideout records. Version 5 appends
  serving jail sentences after the vehicles.

### `LF_Hideout` — `Feature/Hideouts/` (verified 2026-10-11)
- ✅ One building owned by one organization. Members unlock
  `Roadmap_Hideout_Door` at `213 0 120`. A non-member is refused and the
  refusal is logged. The door is `EL_Door_Base` with the inherited
  `LF_LockComponent`. The organization id is written onto that lock. The door
  does not call `LF_Breach` and does not add a second `ActionsManagerComponent`.
- ✅ (2026-11-08, M1-17) An organization takes the yard (`LF_Hideout.HIDEOUT_ID`,
  gate `302.4 0 288` since M1-25) with "Take the lease" on the registry ledger; taking it locks the gate.
  The gate prompt on a client compares the organization replicated on the
  character with the owner on the lock and shows `#EL-Hideout_NotMember` to a
  non-member; the server still decides from the registry.
- ✅ `Roadmap_Hideout_Machine` at `216 0 120` uses the existing
  `LF_ProcessAction` on `ItemProcessor_Base`. It runs only when the player's
  organization owns the hideout and has the processing upgrade. Otherwise it
  refuses and logs. The recipe turns one cabbage into one RowanBerries stack,
  the configured contraband prefab. A chain machine with no hideout component
  still reads `ResourceChains.conf`.
- ✅ **Hideout stash holds items** (2026-11-08, M1-15, tests `hideout/stash`,
  `hideout/stash-save`, and `e2e/pass2-verify-load` across a real two-boot
  restart). `Roadmap_Hideout_Stash` (now at `312 0 296` in the village) is an instance of the reusable
  prefab `Prefabs/Props/Hideouts/LF_HideoutStash.et` (the base-game
  `Chest_01.xob` chest, a crate was too small to notice): a real container (`LF_HideoutStashStorageComponent`,
  200 kg) whose items are entities. Open storage asks the server
  (`SCR_PlayerController.LF_RequestOpenStash`); a member of the organization that
  owns the hideout (`m_sHideoutId` on `LF_HideoutStashStorageManagerComponent`)
  gets the inventory, anyone else hears a refusal sound and gets the
  `#LF-HideoutStash_NotMember` toast, and the refusal is logged
  (`stash refused: not a member`). The server also refuses a non-member moving an
  item out (`HandleAccessValidation`) or in (`CanStoreItem`). Membership is the
  only key: an open or breached door does not open the stash.
- ✅ Stash contents survive a restart through `LF_PersistentStorageComponent`
  keyed `hideout:Roadmap_Hideout` (not an entity id), so the stash keeps its
  items wherever the layer places it. See the placed-storage row under Carried
  inventory.
- ✅ `ExportAll` / `ApplyAll` put the organization id and the lock flag back,
  and a second apply does not change them. The game-mode serializer writes
  these records at version 3. A version 2 save has no hideout payload and does
  not clear a live hideout. Player strings: `#EL-Hideout_NotMember`,
  `#EL-Hideout_NoUpgrade`, `#EL-Hideout_Process`, `#LF-HideoutStash_Name`,
  `#LF-HideoutStash_Desc`, `#LF-HideoutStash_Title`, `#LF-HideoutStash_NotMember`.

## Notifications

### `LF_NotificationManagerComponent` — `Feature/Notifications/`
- ✅ Server→client notification broadcast with toast renderer; `SendToPlayer`/
  `SendToJob`/`SendToAll` + static `NotifyPlayer`/`NotifyJob`/`NotifyAll`.
  Before 2026-11-07 the toast layout
  `{4C38FD68D45325D2}UI/Layouts/Notifications/EL_NotificationToast.layout` did not
  exist, so every toast (fines, arrests, rank denials, lot, lock, jail) failed silently. It
  now exists: a paper slip in the UI kit palette with a stamp-ink type strip (`Background`,
  `ColorStrip`, `Icon`, `Title`, `Message`, `LF_NotificationToastUI` on the root).
  Slips that overlap in time stack downwards. Timing reads the real clock
  (`System.GetTickCount`), not an assumed 16 ms frame. `notifications/toast-layout` (WORLD)
  instantiates the layout on the dedicated server and checks every named widget;
  `notifications/toast-timing` (LOGIC) checks show/fade/remove. On-screen look is
  visual-only: manual check pending (verified 2026-11-07)
- ✅ **Toasts are visible on a game client** (2026-11-08, #157): until now no toast
  was ever drawn. The slip's frame slot had a height of -48 (`OffsetBottom -92` under
  `OffsetTop 140`), the fills were `PanelWidget`s (near-transparent) nested inside
  an `ImageWidget` (which draws no children), and the text column's negative
  stretch offsets ran the message past the slip. Now: 420x120 slip, sibling image
  fills, wrapped message, fade driven from `Tick` (`LF_NotificationToastUI.OpacityAt`).
  Seen at 1280x720 on a dedicated client (contraband refusal). New
  `tools/validation/validate-layout-slots.mjs` fails a point-anchored slot whose
  `Offset*` and `Size*` disagree (it also fixed `EL_SurvivalHUD.layout`).
- ✅ **Targeted delivery fixed** (verified 2026-08-30): `RpcDo_ShowNotification`
  is `RplRcver.Broadcast` with a client-side `playerId` filter (`-1` = all),
  plus a direct call for the listen-server host so the authority sees its own
  toast. `SendToJob` duty semantics now read the account's `IsOnDuty()`, not a
  `job != UNEMPLOYED` tautology.
- ✅ **Private targeted delivery** (verified 2026-11-07: dedicated/* tests, server side; client receipt is manual): `SendToPlayer` sends an Owner RPC on
  the target's `SCR_PlayerController` (`LF_DeliverNotification`), so only that
  client receives it; the listen host's own controller calls the handler
  directly. Broadcast is left for `SendToAll` only. `SendToPolice` /
  `NotifyPolice` reach connected on-duty police by account faction.
  `LF_Utils.NotifyEntity` (shop, robbery, duty,
  confiscate toasts) route through it; `LF_Utils.Notify` still shows a hint on
  the calling machine only. Since 2026-11-08 the courier job (sign-up, paid,
  abandoned), gather/process rewards, weapon and vehicle robbery, and the
  contraband payout toast through `NotifyEntity`; on a dedicated server they
  had reached nobody. enforce-lint L8 fails an `LF_Utils.Notify` after an
  `if (!Replication.IsServer()) return;` guard.
- ⚠️ Hard-coded Spanish prefixes, emoji in logs, `m_Color` never transmitted.

## Inventory / Quantity

### `LF_QuantityComponent` — `Feature/Quantity/LF_QuantityComponent.c`
- ✅ Virtual stack system: `AddQuantity`/`RemoveQuantity` clamp to
  `[0, max]` (0 = unlimited), `SetQuantity(0)` deletes the entity,
  `CanCombine`/`Combine` (conservation: source decrease == dest increase ==
  transferred), `CombineQuantities` (2 and 3 yield 5), `Split`,
  `QuantityRemainingAfterSplit` (5 into 2 leaves 3), `SortByQuantity`
  (descending), `GetRemainingCapacity` (verified 2026-10-11).
- ✅ **In-inventory split mint closed** (verified 2026-11-07): the
  split stack is sized and marked keep-separate before `TryInsertItemInStorage`,
  so `HandleOnItemAdded` no longer merges the spawned init quantity (100 for
  MoneyStack) into the source. Split 1 used to add 99. Pin test
  `security/inventory-split-no-mint`.
- ✅ Stack tier meshes hold their `Resource` while applied, so a tier swap no longer logs
  `Usage of released BaseResourceObject` (2026-11-08, `quantity/operations` visual path).
- ✅ **Transfer intent lifetime** (2026-11-08, test `robustness/lifetime`): `OnDelete` removes
  the entity's intent (no dead keys); an intent older than `INTENT_TTL_MS` (5 s)
  belonged to a failed move and is ignored by the next add.
  `LF_DefaultLoadoutItemQuantityComponent` logs `LF_Debug.Error` instead of
  crashing on a prefab without `LF_QuantityComponent`.
- ⚠️ `Split` with `splitSize` 0 spawns an item then deletes it via
  `SetQuantity(0)`; `ExtractQuantityComponents` inserts null components with
  no check; `HandleOnItemAdded` may refresh UI on a just-deleted entity.

### `LF_InventoryUtils` (Quantity) — `Feature/Quantity/LF_InventoryUtils.c`
- ✅ Quantity-aware `GetAmount`/`AddAmount`/`RemoveAmount` returning *actual*
  amounts; AddAmount fills stacks then spawns; RemoveAmount drains smallest
  stacks first (ascending sort).
- ⚠️ `AddAmount` fills **largest** stacks first (descending) while Remove
  drains smallest — asymmetric by design, untested. Non-quantity
  `RemoveAmount` permanently deletes entities. `GetAmount` returns −1 on
  null target.

### `ScriptedInventoryStorageManagerComponent` (Quantity) — `Feature/Quantity/ScriptedInventoryStorageManagerComponent.c`
- ✅ RPC endpoints for split/transfer/intent (`RPC_LF_RequestQuantitySplit`,
  `RPC_LF_QuantityTransfer`, `RPC_LF_SetTransferIntent`), all server-side,
  range-gated.
- ✅ **Ownership gate added** (verified 2026-08-30): `LF_CanManipulateOwned`
  rejects another living player's carried items (only your own storage or
  unowned containers/ground loot); the old range-only anti-cheat is closed.
- ✅ **Locked owners refused** (verified 2026-11-07):
  `LF_CanManipulateOwned` also refuses a storage whose owner has a locked
  `LF_VehicleLockComponent` or `LF_LockComponent` (`LF_IsStorageOwnerLocked`).
  Pin test `security/locked-storage-refused`.

### Hand-Carry — `Feature/Character/Inventory/HandCarry/`
- ✅ `LF_HandInventoryStorageComponent` state machine
  (`NONE→READY→AWAIT_HOLSTER→ACTIVE→AWAIT_SWAP→NONE`), hands-only storage,
  modded vanilla SCR inventory/gadget wiring.
- ✅ Missing `CharacterControllerComponent`, weapon manager, or inventory
  manager logs `LF_Debug.Error("HandCarry", ...)` and skips that step instead of
  a null dereference (2026-11-07).
- ⚠️ Weapon-holster flow is client-only (server never sees the transition);
  `OnGadgetModeSet` lacks the `HasLocalControl` guard its siblings have;
  `SCR_CharacterControllerComponent` destructor touches `GetOwner()` at
  teardown.

## Vehicles & License Plates

### `LF_VehicleLockComponent` / `LF_VehicleKeyComponent` — `Feature/VehicleLock/`
- ✅ Server-assigned vehicle and debug-key identifiers explicitly bump their replicated state during initialization, so clients receive the identifier after picking up a key.
- ✅ The lock identifier, key id, owner, and lock flag are stored by `LF_VehicleOwnership`. A restart writes that identifier back with `SetVehicleIdentifier`, so the key still matches. Purchased vehicles replace the vanilla `SCR_BaseLockComponent` slot `{54D6FBC0EA806FE5}`, so get-in reads this lock. (verified 2026-10-11)

### `LF_LicensePlateManager` + generators — `Feature/LicensePlate/`
- ✅ `LF_LicensePlateGeneratorGeneric.GenerateLicensePlate()` is `"AA BB NNNN"`.
- ✅ The alphabet includes `Z` (`LetterAt(25)`) and the number range includes `9999`. `Math.RandomInt` is exclusive at the top, so the generator rolls 26 letters and a number end of 10000. (verified 2026-10-11)
- ✅ `LF_LicensePlateRegistry` keeps live plates. A second issue of a live plate is refused, so two vehicles do not share one by construction. (verified 2026-10-11)
- ✅ The registry is session memory, so `LF_VehicleOwnership.ApplyAll` re-reserves every saved plate on load; a purchase after a restart cannot be issued a plate already on a saved vehicle (2026-11-07, test `restart/vehicle-plates-reserved`, verified 2026-11-07).

### Vehicle lot and garage — `Feature/Vehicles/`
- ✅ **Playtest: two cars from the start** (2026-11-08, WORLD `vehicles/test-cars`): with `LF_SpawnLogic.m_bGrantTestCars` (on in `GameMode_Roleplay_Debug.et`, so DebugWorld and the playtest; off in the flagship mode) a newly registered character owns the lot's first two cars as records with their keys in hand: the S105 stored in the garage and the UAZ at the impound lot (`LF_VehicleOwnership.GrantTestCars`). Nothing spawns; retrieve and collect work at once. Granted only to an owner with no vehicle yet.
- ✅ `Roadmap_Vehicle_Lot` sells vanilla S105, UAZ469, S1203, UAZ452, and M923. Each spawn is a thin child of that vanilla prefab with `LF_VehicleLockComponent` and `{9D94B834AF514B4A}Prefabs/Items/Roleplay/VehicleKey.et`. (verified 2026-10-11)
- ✅ A price at or below the lot cash ceiling (500) debits MoneyStack. A higher price debits the ATM balance and does not convert cash. A short payment is refused and charges nothing. Above the ceiling the refusal reads `#LF-Vehicle_BankShort` (pay from the bank; deposit at an ATM), so a buyer holding enough cash knows why (`vehicles/bank-refusal`).
- ✅ Purchase spawns the locked child, sets the buyer as owner, locks it, issues a unique plate, and gives one key. Extra keys are an owner action at the configured cash price.
- ✅ The vehicle record is keyed by the lock identifier. `Export`/`Apply` restore the owner and the key id. `LF_PersistenceComponentSerializer` version 4 writes those records after the hideout records; a version 3 save has hideouts and no vehicle payload. Store keeps the prefab and the lock flag; retrieve spawns on the garage's own pad (`LF_VehicleGarageComponent.m_sPadId` names an `LF_VehicleSpawnPointComponent`), not at the last world coordinates.
- ✅ **Garage it to keep it** (M1-16, #101, decision 2026-11-07; tests `vehicles/impound`, `restart/vehicle-street-impounded`, `garage/ledger-card`): a vehicle survives a restart only when it is stored in a garage. After a load, `LF_VehicleOwnership.ImpoundStreetVehicles` leaves every owned vehicle that was not stored unspawned: it is at the impound lot (`IsImpounded` = owned, not stored, no live entity; derived, so no save field and no version bump). No key is spawned at load. `Roadmap_Impound_Desk` (`246 0 269`, at the gate of the net-fenced yard behind the garage, x 236..256, z 271..283) releases the owner's latest impounded vehicle for `LF_GarageConfig.m_iImpoundFee` (`Configs/Vehicles/LF_Garage.conf`, 250) in carried cash onto a yard pad, once; short cash, a stranger, a full yard, or a second collect is refused and charges nothing. A collect or retrieve hands the owner a key bound to the car when they hold no live one (a restart unbinds carried keys). A purchase is followed by a toast that teaches the rule (`#LF-Garage_BuyHint`); the garage sign and a new `Sign_Impound` board at the desk explain store, retrieve, the ledger, and the fee.
- ✅ `Garage_E_01` (`{A548C1C5370D0734}Prefabs/Structures/Houses/Garage/Garage_E_01/Garage_E_01.et`) is the garage shell. DebugWorld places the use point `Roadmap_Garage` at `238 0 256.6` on `Street_Vehicles.layer`, with store, retrieve, and read the ledger. A retrieved car is parked along the kerb in front of the doors (`254.6`, then 7 m west). Store takes the owner's car parked within 15 m of that garage; the lot is no longer a garage. The shell is its child 5.9 m north, turned so its doors face the street; the player stands in front of the doors. The lot counter is `Roadmap_Vehicle_Lot`: bought cars land on a pad 5 m west and 5 m south of it, then 8 m steps north across the concrete lot. The showroom body is `Roadmap_Vehicle_S105`. (verified 2026-10-11; moved with M1-25)
- ✅ `LF_Garage` holds the park rules and the ledger. Park refuses a seated player or a missing garage, and a vehicle that is not within the same garage's radius as the player (`LF_VehicleGarageComponent.SharesGarage`, `#LF-Garage_VehicleTooFar`, test `security/garage-store-range`), then stops the engine and teleports occupants out before store. Reading the garage or the impound desk sends the reader's rows from the server (`SCR_PlayerController.LF_ShowGarageLedger`) and opens `LF_GarageLedger`, a read-only paper card: model, plate, and where each vehicle is (in the garage, at the impound lot), plus the release fee when one is impounded. The old catalog screen (buy, paint, preview, stats), unreachable from the world and a duplicate of the lot's buy path, is deleted with `UI/Layouts/Garage/LF_GarageVehicleRow.layout`. (2026-11-08)

## UI / Layouts / Localization

### UI kit — `Scripts/Game/Feature/UIKit/`
- Post-release polish. `LF_UITheme` holds the 1989 type, padding, and paper/metal colors. `LF_UIScreen` assembles a frame, title, body line, WLib edit box (ink, no text outline or shadow; the box is not a `TextWidget`), list row, status row, quantity slider, and pause-menu button with `WorkspaceWidget.CreateWidgets`, wires `SCR_ButtonTextComponent`, and activates `MenuContext` every frame (`MenuTextEditContext` while an edit box is focused, `MenuWithEditorContext` so a pad can move). Close removes the widgets and that call. It does not use MenuManager.
- Input (verified 2026-11-07): focus lands on the first non-field control at open, so MenuSelect works without MenuDown first. MenuBack closes. `GetOnButton()` hands the feature the part name of a pressed BUTTON, STATUS action, or list row action. `GetOnClosed()` fires once for any close. A LIST part (`LF_UIList.layout`, `AddListRow`/`ClearList`) scrolls to the focused row. Close defers the removal of its per-frame call by one tick, because it can run inside that call. `ui-kit/initial-focus` and `ui-kit/button-invoker` (LOGIC) cover the pure parts; the on-screen focus ring is visual-only.
- `LF_UIMenuInput` gives the remaining MenuManager menus (ATM, shop, quest, police) and the workspace pickers (faction, spawn location, death, organization) the same input: MenuContext plus MenuWithEditorContext every frame, MenuUp/MenuDown focus ring, MenuSelect on the focused button, MenuBack closes (not on the mandatory faction, spawn and death pickers), and a 150 ms debounce so a click and the poll do not double-fire. The ATM has a Close button and labelled Deposit/Withdraw buttons, tells the player when the amount is blank or zero, and names deposit and withdraw refusals separately. The shop and quest Close buttons work. Shop and ATM moved onto the kit in M1-13 (2026-11-08); their MenuManager presets are gone. The shop, quest, organization desk, and police menu actions are `HasLocalEffectOnlyScript`, so the menu opens only on the acting client.
- A line can be rewritten after open. A status row has one action that hides when the action is empty. A quantity splits one stack into keep and take. The sheet can close on death, on losing the character, or past a distance from an anchor. Nothing in the world opens the sample. `node agent-box/harness.mjs kit` screenshots the sheet, a rewritten balance, a hidden action, both split faces, and focus on the field. It reuses a play session that is already up. Faction, character creation, death, quest, police, and spawn menus were not migrated.
- (2026-11-08, M1-13) Skins: `LF_UIScreen.Open(parts, skin)` paints from an `LF_UISkin` (`Paper()` is the old look and the default, `Terminal()` the ATM, `PriceList()` the shop): frame colours, face, sheet width, blur on or off, and, for skins that ask, the vanilla button text colours (and its arrow glyph hidden). A KEYS part (`LF_UIKeyRow.layout`, `LF_UIPart.CreateKeys`) lays buttons side by side; MenuLeft/MenuRight walk the row and MenuUp/MenuDown keep the column in the next row. `BindHotkey(action, name)` reports an input action as a button press while no field is focused. `SetButtonText`, `SetListHeight`. `LF_UISound.Play` plays a UI sound event. Seen on a dedicated client (agent-box session, 640x360): the frame `Paper` panel draws nearly transparent and the blur darkens it, so paper sheets read dark; the price list turns the blur off to stay legible. The registration form shows the same on that client.
- ✅ (2026-11-08) Opaque paper. The frame's `Metal`, `Paper`, `Rule` and the row plates were `PanelWidget`s, which draw the engine default style's panel image under their colour, so the sheet read dark and see-through. They are untextured `ImageWidget`s now (the vanilla solid-fill pattern, `DialogBackground.layout`), the paper is opaque warm off-white (238, 230, 212), and a full-screen `BackgroundBlur` behind the `Sheet` dims and softens the world. Paper skins paint button labels in ink with a red focus. A field gets an ink caption above it (`<Part>Caption`) because the vanilla placeholder draws light grey. `LF_UIEditBox.layout` now overrides the base `SCR_EditBoxComponent` by its GUID, which removes the stray "Editbox" label and applies the paper-coloured well. The sheet is 900 tall so the price list's Leave and the ATM's EXIT fit. Seen on a session client (WORLD `ui-kit/paper-sheet`, flow `ui-paper`).
- ✅ (2026-11-09) List rows span the sheet. A row sized to its text and sat centred in the scroll, so on the 900-wide price list the car lot cut "needs $2,500" to "needs $2," and "$1,000" to "$1,00". `AddListRow` sets each row to the sheet width less `LIST_ROW_INSET` (list and row padding, scrollbar). Seen on a laptop session client at 1920x1080: the lot and the general store.

### Signs — `Scripts/Game/Feature/Signs/`
- ✅ **Picture on a sign card** (#579 spike, LOGIC `signs/image`). `LF_SignDef.m_sImage` (an imageset) and `m_sImageName` (default `full`) add an `LF_EUIPartKind.IMAGE` part under the title; `LF_UIScreen` loads it into an `ImageWidget` (`UI/Layouts/UIKit/LF_UIImage.layout`) with `LoadImageFromSet`, and logs `[LFDebug:UIKit]` when the imageset does not load. The hospital card shows `Assets/UI/Illustrations/medic_flow.imageset`. `tools\cli ui-image <svg|png> <name>` makes the texture and imageset headless; route and limits in `docs/research/ui-illustration-pipeline.md`.
- ✅ Diegetic signs (M1-01). `LF_SignComponent.m_sSignId` names a row in `Configs/Signs/LF_Signs.conf` (id, title, what, how, needs, rules, next). A value starting with `#` is a localization key; any other value is shown as written. `LF_SignReadAction` is local-effect only and names itself `Read: <title>` (`#LF-Sign_Read`). Perform opens `LF_SignCard` on the reading client: a UI kit sheet with the title, one line per filled section (What, How, Needs, Rules, Next; empty ones omitted), and Close. Close, MenuBack, death, losing the character, or walking 4 m from the sign closes it. No RPC. An unknown or empty id, or a row with no title, logs `LF_Debug.Error("Signs", ...)` once and the action hides. LOGIC `signs/catalog`, `signs/missing-id`, `signs/localization`; WORLD `signs/prefabs`.
- ✅ Prefabs `Prefabs/Props/Signs/LF_Sign_Board` (cork board), `LF_Sign_Pub` (shop sign), `LF_Sign_Notice` (A4 notice), `LF_Sign_Chalkboard` (standing chalkboard; the vanilla `CardboardSign_01` lives in the Rally addon and does not resolve for this mod) inherit the vanilla props (no new meshes) and add the component, an `LF_Sign` action context on the face, and the Read action. A placed instance sets the id. The title is also printed on the prop on rendering clients: `LF_SignComponent` draws `UI/Layouts/Signs/LF_SignFace.layout` into an `RTTextureWidget` bound (`SetRenderTarget`) to a mesh whose material `Assets/Signs/Data/LF_SignFace.emat` samples `$rendertarget`. `LF_Sign_Board` spawns a local `LF_SignFace` sheet (vanilla A4 poster mesh) on its face; `LF_Sign_Notice` retargets its own poster material. A dedicated server skips the face. `LF_Sign_Pub` and `LF_Sign_Chalkboard` print it too (#116): a painted sheet (`LF_SignBoardFace.layout`, slate paint, chalk-white lettering) tuned on the prefab. The chalkboard sheet is turned 90 degrees (`m_vFaceAngles 0 0 90`, scale 6.2, offset 0 1.47 0.3); the A-frame sheet leans with its face (`0 -15 0`, scale 2.6, offset 0 0.62 0.26), sits a little high and does not fill the lower board. Shots in `website/public/docs-images/116/`.
- ✅ The printed title fits the shown part of the render target at any window size. The target is the layout size times the screen DPI scale, but the widgets inside lay out one reference unit per pixel, so a 720p Workbench viewport printed "MINI / ARE". `LF_SignComponent.FaceTitleRect` places the title box and font by `DPIScale(1)`. Board and notice photographed reading MINING AREA, not mirrored, in a Workbench play session (2026-11-08). LOGIC `signs/face-fit`.
- ✅ **Game client face size** (M1-09, 2026-11-08). On a 640x360 dedicated-server client the DPI-scaled title printed tiny in the sheet's top-left third. Without the DPI scaling, the full 432-unit box at font 96 cut WELCOME to WELCO at 640x360 and at 1920x1080 alike: only about 328 of the 512 units across reach the paper. The DPI compensation is now Workbench only (`#ifdef WORKBENCH`); a game client uses `LF_SignComponent.ClientTitleRect` (box x 20..310, font 96 or smaller so the longest word fits). All 18 board titles photographed whole and unmirrored at 1920x1080 on a dedicated-server client. LOGIC `signs/face-client`.
- ✅ **Placed in DebugWorld** (M1-09, 2026-11-08). `DebugWorld_Layers/Signs.layer`
  holds one sign per town manifest site, within 8 m of it, facing the street:
  `Sign_TownMap` at the spawn square (`town_map`, "Welcome to Everon": the
  loop in order and where each place stands, left and right walking east),
  then post office, ATM, bank, general store, market, vehicle lot, garage,
  police station, jail (inside the yard), fuel station, hospital, organization
  office, hideout and convoy drop (in the yard), convoy pickup at the barn, and
  farm, all `LF_Sign_Board`. The back room is an `LF_Sign_Chalkboard` with no
  printed title. `Sign_PoliceWhitelist` places `EL_WhitelistSign` beside the
  whitelist station. Cards are `#LF-Sign_<Site>_<Field>` keys: what, how with
  the exact action names, needs, rules, next (where to go after this place,
  so a stranger can follow the loop from sign to sign; LOGIC `signs/next`), with the DebugWorld numbers (prices,
  pay, cooldowns). WORLD `signs/coverage`: every placed sign resolves a titled
  catalog row, every catalog row is placed, and each title and What resolves.
- ✅ `EL_MiningSign` reads `mining` (`Read: Mining area`) and `EL_WhitelistSign` reads `police_whitelist` beside its whitelist action. `LF_TextLabelComponent` (floating debug text) is deleted.
- ✅ **Signs belong to the building** (M1 polish, 2026-11-08). Nothing official stands on the grass any more. Building signs are vanilla sign bodies with the title painted on their own face: `LF_Sign_Plate` (enamel wall plate, `SignWall_01_115x67`: police by the porch, post office beside the post box, ATM, garage, jail, hospital beside a red cross cloth, organization office), `LF_Sign_PlateSmall` (`SignWall_01_90x45`, spare), `LF_Sign_Fascia` (shop fascia, `SignCommercial_01`, over the bank door and the general store front), `LF_Sign_Posts` (large board on two posts, `SignLarge_01`: town map, market, vehicle lot) and `LF_Sign_PolePlate` (plate on a pole: safe zones, impound, fuel, farm). Each carries `LF_SignComponent` with `m_bFaceOnOwner` (or `m_bFaceOnChild` for the pole plate) and `UI/Layouts/Signs/LF_SignPaintFace.layout`: Rim and Paint images sized to `m_vFaceArea` (the part of the target the mesh UVs show, measured with a quadrant test card on a 1080p client) and coloured per placement (`m_vRimColor`, `m_vPaintColor`, `m_vTextColor`). `PaintedTitleFit` picks the largest font at which the title, broken at spaces, fits the title box. A-frame pavement boards (`LF_Sign_Pub`) stand at the general store and the market; the contraband barn and the back room carry a plain chalkboard; the pub wears the vanilla HOSTINEC sign. The duty desk and the armory locker stand under the police porch (the empty weapon-rack frame on the lawn is gone). The cork `LF_Sign_Board` keeps notices (wanted board under the police porch, convoy): its A4 sheet is turned on its side (`m_vFaceAngles 0 0 90`, scale 4.7) and the title turns 90 degrees with it; a turned text is clipped to its unturned rectangle, so the widget is a square around the box. Panel widgets do not draw inside a render target; the faces use image widgets. Harness `stand` puts the reader 1.4 m in front of a sign face (`LF_AgentHarness.SIGN_STAND_FRONT`), since beside a wall plate is inside the wall. LOGIC `signs/board-face`.
- ✅ **Sign card is a printed notice (2026-11-10).** `LF_SignCard` opens `LF_UISkin.Notice` (640 x 920 sheet): a large bold title, a thin accent rule under it, and for each filled section a small upper-case label (`#LF-Sign_What` "What it is", How, "You need", Rules, Next; an empty section is left off) over a `RICH` body part (`RichTextWidget`, `LF_UIRich.layout`; new UI kit part kinds `HEADING`, `RULE`, `RICH`). Strings keep working as plain prose; `|` or a newline between items, or lines starting `- `, make a hyphen list (`LF_SignCatalog.ParseItems`); `*word*` is drawn in the accent colour (`<color rgba=...>`, a typed `<` or `>` is dropped). A painted sign (`m_vFaceArea` set: plates, fascia, pub sign, chalkboard) frames the card in its paint colour and takes the darkest of paint, lettering and rim as the accent (never lighter than 0.3 luminance); the cork board and A4 notice keep the steel frame and an oxblood accent. `LF_SignCatalog.FitScale` shrinks the type (down to 0.7) so a long card stays on the sheet. The town board, post office, police station and whitelist, hospital, bank, vehicle lot, general store and hideout texts moved to the list style. LOGIC `signs/card-format` (also: every sign fits the sheet), `signs/catalog`, `signs/next`, `signs/localization`.
- ✅ **Props match what they give** (M1 polish, 2026-11-08). The pub-terrace survival points were a hand pump that gave plums ("Tomar Ciruela") and a wooden box that gave apples ("Tomar Manzana"); they are now a plum tree (`SurvivalPoint_Water`) and an apple tree (`SurvivalPoint_Food`) whose gather prompt is the localized `Gather <fruit>`. `LogPile_Debug` had no mesh; it is a vanilla wood pile (`WoodPile_02_A`). The ATM prompt reads "ATM" (`#EL-ATM_Title`), not "ATM Machine".
- ⚠️ MenuBack closes the card through the UI kit. The printed face, the card render, the prompt, and the walk-away close are not yet seen in a rendering client (manual steps in the M1-01 PR).

## Quests

### `LF_QuestTrackerComponent` — `Scripts/Game/Feature/Quests/`
- ✅ Server-authoritative quest definitions, acceptance, objective progress,
  timed expiry, collect recounts across managed nested storage, rewards, and
  client quest-log replication.
- ✅ `QuestGiver_Start` and `DebugWorld` provide a world-facing onboarding
  example with localized menu layouts and registered logic/world tests.
- ✅ **Turn-in loop closed** (verified 2026-11-07): turn-in removes each
  `COLLECT_REPORT` objective's target amount through `LF_InventoryUtils` before
  the reward, all or nothing (a short removal puts the items back and refuses).
  Client open/accept/turn-in (`RequestOpenGiver`/`RequestAccept`/`RequestTurnIn`)
  require the registered giver that offers the quest within
  `LF_QuestManager.GIVER_RADIUS` (5 m); an unknown giver id is refused. Server
  scripts may still call `AcceptQuest`/`TurnInQuest` directly. Pin test
  `security/quest-turnin-takes-items`.
- ✅ **Guided arrival chain** (M1-18, 2026-11-08): `QuestGiver_Clerk.et`
  (`Quest_Clerk` on the square, 134 0 243.5) runs five steps: `arrival_register`
  (talk; hands over the ID card), `arrival_board` (`SIGNAL sign.read:town_map`),
  `arrival_work` (`SIGNAL job.run_complete:parcel`), `arrival_bank`
  (`SIGNAL atm.deposit`), `arrival_done` (talk; $250 and "the town is yours" with
  pointers to the rest of town). Steps 2-4 start and finish on their own
  (`m_bAutoAccept`, `m_bAutoTurnIn`), so the player never walks back between
  errands; each new step's errand shows as a 12 s hint. A fresh spawn without the
  card is pointed at the clerk. A giver's menu lists one chain step at a time and
  never re-lists a one-off quest already turned in (`LF_QuestDefinition.IsListed`),
  so a returning player is not offered the chain; a player who left mid-chain
  gets the next step back on spawn (`SettleChain` after `RestoreTurnedIn`).
  Accepting at a giver counts that giver's talk objective at once. No waypoints.
  The clerk is a converse giver (`m_bConverse`): "Talk to the clerk" opens no menu; one talk
  turns in what is done, takes on the next quest, and the clerk's lines show as slips
  (`RequestConverse`); mid-errand he repeats the errand, after the chain his idle line
  (`m_sIdleKey`). A second NPC with a placed giver's id no longer takes the giver over.
  Harness: `agent-box/flows/arrival-chain.txt`; `state` adds `idcard=` and `quests=`;
  `stand`/`use` reach a replicated NPC (no layer name on a client).
- ✅ `SIGNAL` event type with a string key (`LF_QuestManager.ReportSignal`).
  Hooks in `LF_QuestHooks.c` (modded classes; the job, ATM, and sign files are
  untouched): `LF_JobDropoffAction` reports `job.run_complete:<job>` for a paid
  run, `LF_CharacterATMComponent.ReplyResult` reports `atm.deposit` for a
  successful deposit, and `LF_SignReadAction` asks the server (`RpcAsk_SignRead`)
  to report `sign.read:<id>`, counted only with that sign within 6 m.
  `QuestGiver_Start` (test quests, "Test quest board") moved to the test annex
  (`Quest_TestGiver`, 104 0 70). Tests: `quests/signal-event` (LOGIC),
  `quests/arrival-chain` (WORLD).

### Layouts (loaded by `LF_Test_Data`)
- ✅ `LF_SplitQuantityDialog`, `LF_CharacterCreationMenu`, `LF_ATMMenu`,
  `EL_SurvivalHUD`, `ShopMenu`, `PoliceMenu`,
  `DeathScreen` load (WORLD tier). The `SurvivalHUD` menu preset and its
  `EL_SurvivalHUD` class were never opened and are removed (2026-11-07); `LF_BetaHud`
  draws the gauges. The layout stays only as this load fixture. The unused
  falling-fruit widget (`LF_FallingFruit`, which loaded a layout that never existed) is
  removed; `EHarvestJobType` moved to `Feature/Jobs/LF_HarvestJobType.c`.
- ✅ `LF_DebugMenu` is a development menu opened with F10 (or its rebind),
  with server-routed controls for cash, wanted state, faction, jobs, survival,
  XP, and skill points. Actions affect only the requesting player's state
  (verified 2026-08-30). `LF_DevGate` (`Core/LF_DevGate.c`) checks every
  action on the server. It logs `[LFDebug:DebugMenu] debug action refused: ...`
  and refuses unless the game mode sets `m_bDevToolsEnabled` or the sender
  holds `EPlayerRole.ADMINISTRATOR`. Only `GameMode_Roleplay_Debug.et`
  (DebugWorld) sets the flag. The production `GameMode_Roleplay.et` leaves it
  off. The open key runs the same check on the client and logs
  `open refused` instead of opening. `LF_DevGate.IsAllowed` has the LOGIC test
  `debug-menu/gate` (2026-11-07).
- ✅ The playtest mission `Missions/LF_PlaytestTown.conf` (`LF_MissionHeader`,
  `Core/LF_MissionHeader.c`) runs DebugWorld with `m_bLF_DevToolsOff 1`: the
  header overrides the DebugWorld game mode opt-in, so only administrators
  pass `LF_DevGate` there. A bare world (Workbench play, `toolscli test`) has
  no LF header and keeps the opt-in. LOGIC test `host/playtest-mission`
  (2026-11-08).
- ✅ **Workshop scenario (2026-11-10).** `LF_PlaytestTown.conf` is the Workshop item's scenario
  "Life Framework: Playtest Town": plain-text name and description (the Workshop backend copies
  them as text), the town picture `Missions/LF_PlaytestTown_Preview.edds` as icon, preview and
  loading screen, 16 players, details from `#LF-Mission_PlaytestTown_Details`. `EL_DebugTest.conf`
  and `EveronLifeGameMode.conf` set `m_bShowInScenarioMenu 0`. `tools\cli pack` packs the addon
  as Workbench Publish bundles it. Proven from the packed addon in a clean profile: Scenarios
  lists it, Play loads the town, form R-89 signs, top-up and test cars apply. Asserted in
  `host/playtest-mission`. Publish steps: `docs/playtest/workshop-publish.md`.
- ⚠️ Localization keys `#EL-*` are frequently passed through `string.Format`
  (corrupting keys with `%1`) and/or handed to
  `SCR_HintManagerComponent.ShowCustomHint`, which does not localize `#Key`
  — the player sees raw keys. Spanish literals in notifications remain. The job,
  level and licence texts, the duty status words, the robbery, gather and process action
  texts, and the DebugWorld shop names moved to `#LF-*` keys on 2026-11-07, worded as
  employment rather than XP. Violates the AGENTS.md localization contract until the
  notification prefixes move too.

## DebugWorld coverage

 - ℹ️ **`Worlds/DebugWorld/DebugWorld.ent` is intentionally 0 bytes.** All
  DebugWorld content lives in `Worlds/DebugWorld/DebugWorld_Layers/*.layer`;
  `default.layer` carries the `GenericWorldEntity world` BSP and terrain
  entity. This layer loads before gameplay layers. Placing the world entity in
  `Terrain.layer` left earlier props without physics bodies even when they had
  `RigidBody` and `TRACEABLE` configured. Do not repair the empty `.ent`.
  `MainWorld.ent` differs because it
  is a `SubScene` wrapper over `worlds/Eden/Eden.ent`.
- ℹ️ **Direct collision fixtures need `Flags 0x403 0`.** This includes the
  `Traceable` flag. Without it, a `MeshObject` + `RigidBody` can render while
  character movement and projectile traces pass through it. Prefab fixtures
  such as `LF_TestRunner` inherit the flag; hand-authored `GenericEntity`
  fixtures must set it explicitly.
- ✅ DebugWorld is a one-street 1989 Everon village (M1-25, 2026-11-08;
  `docs/roadmap/m1-25-village.md` has the map). The main street runs east-west
  at z = 250 on the flat 512 m terrain (world 0..512, `TERRAIN_MAX` in
  `tools/street/manifest.mjs`; every heightmap sample is 0, so ground props sit
  at Y = 0). Sites stand in arrival order in front of vanilla buildings: the
  square (spawn, clerk, pub, church), post office, bank, general store and
  market, vehicle lot and garage, police station with a jail yard, hospital and
  fuel station, org registry, and the hideout down a side alley; the contraband
  barn is out of town on the east track. The farm, quarry, builders' yard and
  forest edge are on the outskirts. `Village_Roads`, `Village_Buildings` and
  `Village_Dressing` hold the roads (road generator splines saved as
  `RoadEntity`), vanilla buildings, walls, lamps and trees; `Annex.layer` is
  the net fence of the test annex at x 20..112, z 20..112, which holds the test
  runner, the PR fixtures, and the legacy debug stations (with their
  boundaries) on the ground where world tests spawn their characters. Sites
  keep their names; chain layers moved as blocks. The terrain material
  (`Assets/World/GridSurface.emat`, name kept so the terrain reference holds)
  is the vanilla lawn surface without clutter, so grass does not grow through
  roads and floors.
  `tools\cli validate` runs
  `validate-debugworld.ps1` to guard the layer set.
- ✅ Town tidy (M1-22, 2026-11-08): the street carries no debug artefacts. The
  old police duty table (`PoliceStation`, `PoliceStation_Debug`) and the
  whitelist table (`WhitelistStation`) moved into the annex (100 0 40 and
  100 0 52, names kept). The daylight blue kerosene lamps left the market,
  quarry, forest, and pub tables. The pub's survival points are an apple tree
  (`SurvivalPoint_Food`) and a plum tree (`SurvivalPoint_Water`) instead of a
  fridge and a US water trailer. `Village_Dressing` adds 98 vanilla props:
  bins, planters and flower boxes, a poster column and notice boards, the bus
  stop sign, hydrants, benches, shop crates, a garbage container, yards with a
  clothesline, firewood and a dog house, a fenced cabbage plot, trees behind
  both rows, and four parked cars. The cars are the mod's lockable S105, S1203
  and UAZ-469 with the vanilla `m_bIsLocked` set, so no key opens them.
- ✅ The WORLD-tier `e2e/narrative-loop` exercises the live Apple gather →
  trader cash payout → shop purchase → ATM deposit path. The robbery step calls
  `LF_RobAction.CommitRobbery` (2026-11-07; it used to hand-write the payout);
  its proceeds are asserted as cash and are not added to the bank.
- ✅ `debugworld/entity-collision` checks live physics bodies, geometry, and
  sphere sweeps through both apple trade crates and all three trader tables.
  Flags alone cannot prove collision. World initialization runs first in
  `default.layer` (verified 2026-09-19).
- Manual collision smoke after integrating the change: reopen DebugWorld,
  start a fresh play session, spawn as Civilian, and walk into each of the
  three apple trader tables from both sides. The character must stop at the
  tables, and the apple trade actions must remain usable. Repeat on a client
  joining a running dedicated server. These client checks are not automated.

## Agent box

### `LF_AgentHarness` — `Feature/AgentHarness/`
- ✅ (2026-11-11, placement tool) `tools\cli place on|against|ground|arrange` places world props without typed coordinates: mesh bounds from the `.xob`, the surface top, wall face and ground from probe rays on the test server (`LF_PlacementAudit.RayLine`, answered in the same boot as the audit), offline refusals (does not fit, overlap, carriageway, door swing and walk-up, scale, tall simulated body), the layer entry written by the tool, then the audit on what it wrote with a rollback when flagged. `place bounds|probe|verify` inspect. Probed tops are cached in `tools/place/surface-cache.json`. Tests: `place-tool-selftest` (validate), `agent-harness/placement-probe`. Docs: `docs/placement-tool.md`.
- ✅ (2026-11-11, placement audit) `tools\cli audit placement` ranks every badly placed entity in the playtest town: floating, sunk, hanging off a surface edge, clipping walls, interpenetrating other props, in a door's swing, on a carriageway, odd scale, tilt, reach. `LF_PlacementAudit` (traces, oriented boxes, doors) measures on the dedicated test server through `LF_Test_PlacementAuditTown`, so it needs no session (about 10 s); `agent-box/place-audit.mjs` pairs, exempts (`agent-box/place-audit-allow.json`) and ranks. The harness `probe ent` verb uses the same class. Red-proof `agent-harness/placement-audit` (WORLD: grounded, stacked, floating, scaled and half-off-the-table crates). `shots area --eye-only` shoots an area's eye-level and close-up shots without the wide (about 75 s). `docs/placement-audit.md`; a pre-filming step in `docs/marketing/trailer/quality-gate.md`.
- ✅ (2026-11-10, M2 QA) Harness verbs `pager` (opens the carried pager's card, as `LF_ReadPagerAction` does) and `report` (the `/report` chat command's ask). `use LF_PayphoneAction__3` picks the fourth action of that class on the host (a payphone carries one per service preset; the plain name finds the first). `agent-box/qa-m2.mjs` drives the comms loop (payphone, board, take, caller hears back, pager), `/report`, the square payphone, and `normalize` (clock officers and medics out between proofs).
- ✅ (2026-11-10, M2 QA) `LF_LockpickAction.CanBeShownScript` reads `LF_Lockpick.ShowsPick` again, so a locked car lists the pick action (a merge had put the door-only check back). Test: `e2e/lockpick-vehicle`.
- ✅ (2026-11-10, #348) The `state` line also prints `jail=` (seconds left on a sentence), `garage=` (cars stored), `parked=` (plate of the car in the world) and `stash=` (items in `Roadmap_Hideout_Stash`, -1 with none placed). The verbs `stash` (moves an apple from the character into the stash: the member check in `CanStoreItem` decides) and `park` (sets the owner's car down beside `Roadmap_Garage`) stand in for an inventory drag and for driving; with `apples` they pass the harness gate of a `LF_HARNESS` build, so the playtest journey needs no debug menu. `agent-box/journey.mjs` runs the police and org-C steps and a wider restart check on 3 clients.
- ✅ Dev-only. A rendering play session polls `$profile:agent-box/command.txt`.
  `press F` calls `SCR_InteractionHandlerComponent.ActionPerform`. Wheel scroll
  calls `ActionScroll`. `click` invokes the same button invoker or `OnClick` a
  mouse click does. `use` still checks `CanBeShownScript` and
  `CanBePerformedScript` before `PerformAction`. Menu keys are an allowlist
  (`MenuUp`, `MenuDown`, `MenuLeft`, `MenuRight`, `MenuSelect`, `MenuBack`,
  `LF_OpenDebugMenu`); movement actions are refused.
- ✅ `stand` is the placement exception: the server moves the local character
  beside a named entity. The poll does not start on a dedicated server
  (`RplMode.Dedicated`). The server checks the `stand`, `pose`, and `use` RPCs
  with `LF_DevGate`, the same gate as the debug menu. It refuses and logs
  (`[LFDebug:AgentHarness] <request> refused: ...`) unless the game mode sets
  `m_bDevToolsEnabled` or the sender is an administrator. On a production or
  listen-hosted server a client cannot teleport or reach past action range
  (2026-11-07). On a client it retries until a local controller exists,
  including while faction and spawn menus are up.
- ✅ `previews show <prefab,...>|hide` draws up to 24 prefabs through `ItemPreviewWidget` (`LF_ItemPreviewSheet`), the widget the inventory slots use, hiding the HUD and notification toasts; `toolscli shots previews` pages every mod item onto contact sheets (#612).
- ✅ Screenshots go through `LF_ScreenshotCapture`. UI shots leave the camera
  alone. A world shot teleports the character to the camera offset (default
  `10 3 10`, or `ox oy oz` for the row), faces the target, and screenshots the
  player view. That view faces -Z, so a picture stands on the +Z side of
  its target. The pose is applied again 700 ms later so gravity does not drop
  the body before the picture. `SetCamera` does not move that view. (verified 2026-10-11) On a
  dedicated server the server also sends the pose to the owning client
  (`RpcDo_LF_HarnessPose`), which would otherwise overwrite it; session
  clients take world shots this way (verified 2026-11-08, M1-25). A filtered row's wide shot
  stands just north of that row when the street is further north. If the new PNG matches the
  previous PNG, `report roadmap` records `camera_stuck` once and stops.
  `Roadmap_Jail_Door` and `Roadmap_Jail_Cell` are painted-light when look
  returns an action. Any other white mesh fails. Menu shots use `shot ui`; the
  report judges `classify` and the widget list. Names after `roadmap` walk
  only that row. `node agent-box/harness.mjs kit` photographs the UI kit
  sample in the current play session when play is already up, and enters play
  only when it is not. `node agent-box/harness.mjs report roadmap` writes
  `agent-box/reports/roadmap.html` from a play session. The Node harness, MCP
  tools, layers, and named flows live in `agent-box/`. See `agent-box/README.md`.
  (verified 2026-10-11)
- ✅ A world-shot pose turns off damage handling on the posed character for
  8 s, so the fall from the camera height no longer kills it (a 4.5 m pose
  ended on the death screen; verified 2026-11-08 with an 18 m row shot).
  `shot world <name> x y z ox oy oz` takes a close camera offset. The poll
  restarts when a new game mode appears, so a second Play in the same
  Workbench no longer answers `no_session`. The command file is shared by
  every default-profile session on the machine; the `agent-box-play` lock
  ties it to one checkout's Workbench or game process.
- (2026-11-08, #108) Screens: `click` accepts every UI kit button by the name
  the screen gave it (`LF_UIButtonRelay`: ATM keys, `IdClose`, the sign card's
  `Close`, the garage's `Buy`). `ui text` reads a sheet as `kind:name=text`
  (button labels, typed fields, printed lines, `#keys` translated);
  `wait-text` polls it; `ui-shot` is `shot ui`. `stand` turns the character to
  face the entity. `session.mjs up --mission playtest` runs `LF_PlaytestTown`
  with `-scrDefine LF_HARNESS` on server and clients: harness RPCs pass
  `LF_DevGate.CheckHarnessRequest` in that build only, the debug menu stays
  closed, a client without the define is refused by the script checksum, and
  `host start` never passes it. `session.mjs restart` (playtest) stops with the
  host stop file (SHUTDOWN save), loads, and rejoins the same profiles.
- (2026-11-11, #571) The stand-in verbs `drill`, `keycard`, `charge`, `shelf`, `crate`, `keys`,
  `scanner`, `shoot`, `shootnpc`, `siren` and `die` pass `LF_DevGate.CheckHarnessRequest` too, so a
  playtest-mission session can film the heists and the CPR; without `LF_HARNESS` they still need an
  administrator or dev tools. The launch trailer is shot and cut with `video/trailer-v1/shoot.mjs` and
  `video/trailer-v1/cut.mjs` (takes and cuts in the git-ignored `tmp/trailer-v1/`).
- (2026-11-08, #108) `node agent-box/journey.mjs` plays the M1 journey on a
  two-client session from the player's view (job, parcel run, ATM keypad, price
  list, car, give cash, till robbery and alert, org desk, garage, and on the
  playtest mission a restart), judges `state` before and after each step, and
  writes `agent-box/reports/journey.html` with the client screenshots.
  `tools/validation/agent-box-selftest.mjs` runs the harness, session, and
  journey selftests in `validate`.
  Every step ends with a picture from each client it touched. Clients default to 1280x720 (`session.mjs up --res 1920x1080` for evidence) on the hidden desktop. `journey.mjs --film` records each step inside the game client (`LF_FilmRecorder`), one mp4 per step plus the joined run, linked from the report; it works on the hidden desktop.

### `LF_AgentHarness` — `Feature/AgentHarness/`
- ✅ Dev-only. A rendering play session polls `$profile:agent-box/command.txt`.
  `press F` calls `SCR_InteractionHandlerComponent.ActionPerform`. Wheel scroll
  calls `ActionScroll`. `click` invokes the same button invoker or `OnClick` a
  mouse click does. `use` still checks `CanBeShownScript` and
  `CanBePerformedScript` before `PerformAction`. Menu keys are an allowlist
  (`MenuUp`, `MenuDown`, `MenuLeft`, `MenuRight`, `MenuSelect`, `MenuBack`,
  `LF_OpenDebugMenu`); movement actions are refused.
- ✅ `stand` is the placement exception: the server moves the local character
  beside a named entity. The poll does not start on a dedicated server
  (`RplMode.Dedicated`). The server checks the `stand`, `pose`, and `use` RPCs
  with `LF_DevGate`, the same gate as the debug menu. It refuses and logs
  (`[LFDebug:AgentHarness] <request> refused: ...`) unless the game mode sets
  `m_bDevToolsEnabled` or the sender is an administrator. On a production or
  listen-hosted server a client cannot teleport or reach past action range
  (2026-11-07). On a client it retries until a local controller exists,
  including while faction and spawn menus are up.
- ✅ Screenshots go through `LF_ScreenshotCapture`. UI shots leave the camera
  alone. A world shot teleports the character to the camera offset (default
  `10 3 10`, or `ox oy oz` for the row), faces the target, and screenshots the
  player view. That view faces -Z, so a picture stands on the +Z side of
  its target. The pose is applied again 700 ms later so gravity does not drop
  the body before the picture. `SetCamera` does not move that view. (verified 2026-10-11) A filtered row's wide shot
  stands just north of that row when the street is further north. If the new PNG matches the
  previous PNG, `report roadmap` records `camera_stuck` once and stops.
  `Roadmap_Jail_Door` and `Roadmap_Jail_Cell` are painted-light when look
  returns an action. Any other white mesh fails. Menu shots use `shot ui`; the
  report judges `classify` and the widget list. Names after `roadmap` walk
  only that row. `node agent-box/harness.mjs kit` photographs the UI kit
  sample in the current play session when play is already up, and enters play
  only when it is not. `node agent-box/harness.mjs report roadmap` writes
  `agent-box/reports/roadmap.html` from a play session. The Node harness, MCP
  tools, layers, and named flows live in `agent-box/`. See `agent-box/README.md`.
  (verified 2026-10-11)
- ✅ Town manifest (`agent-box/town.mjs`): every M1 site by entity name (or
  prefab, for the unnamed spawn points), the actions look must offer, its sign
  id, and its journey step. No positions: sites and signs resolve against the
  layers, so a site can move layers without a manifest edit.
  `node agent-box/harness.mjs town` and `validate-town-manifest.mjs` check it
  offline; `report town [site-id...]` walks it in play mode and writes
  `agent-box/reports/town.html`. Sign problems (none within 8 m, blank Read
  card) fail the row: `requireSigns` is true since M1-09 placed the signs.
  The spawn site is the named civilian town spawn `Spawn_Town`; the clerk
  (`QuestGiver_Start`, prefab mode) is a site so the square meets the 30 m
  neighbour rule.
  Play-mode run not yet recorded.

### Directed camera rig and storyboards — `Feature/Film/LF_FilmCameraPath.c`, `tools/film/storyboard.mjs` (dev tooling, #392, #395)
- ✅ Harness verb `camera orbit x y z radius seconds [azimuth arc height]` (an arc round a point, centred on any azimuth: 0 = +z, 90 = +x, 180 = -z, 270 = -x; the old +z sweep is the default), `camera follow x y z distance seconds`, `camera static px py pz lx ly lz seconds [fov]`, `camera drone p l q m seconds [fov]` (eased move between two poses), `camera hud off|on` (HUD and hint pop-ups hidden for a first-person shot; `LF_FilmClean` keeps hints down every 400 ms), `camera stop`. An orbit or follow (many keys) eases in and out once for the whole pass and runs a Catmull-Rom curve through its keys at a steady speed; it used to ease every segment and so stopped at each key, a pulse once a second (fixed 2026-11-11, #571 trailer). Covered by `film/camera-path` (LOGIC). Proven 2026-11-11 on a 1920x1080 session: five 4 s shots of one scene (first person, follow, orbit, drone, static), no HUD (`video/camera-test.json`, internal).
- ✅ Scene presets (#394): light-only scenes for the trailer locations (`scene start trailer-square`, `trailer-bank`, `trailer-quarry` ... : fixed clock, clear weather, day frozen, no actors; `LF_FilmSceneEnv.EnvOnlyHour`) beside `street`. Covered by `film/scene-env`.
- ✅ Storyboard format `lf-storyboard/1` (`video/storyboard.schema.json`, checks in `tools/film/storyboard.mjs`): output size, fps and aspect (16:9 and 9:16), scenes, shots with camera, duration, captions and voice. `toolscli film-make video/<name>.json` validates (a bad file fails naming the field, e.g. `shots[2].camera.radius`), shoots each shot on the session through the engine recorder, deletes the frames after encoding, and hands the takes to post-production. Flags `--shot ids`, `--group name`, `--preview` (half size, 10 fps), `--client n`, `--no-shoot`, `--shoot-only`. The trailer storyboard is `video/trailer-571.json`; its `spawn-square` group (drone, orbit, static) shoots today, the rest are marked `pending` and skipped. `validate-storyboards` checks every `video/*.json`.
- ⚠️ A first-person shot films the client as it stands: the actor setup for each trailer beat (`shot.setup`) is not written yet, and a title card between shots is not supported by post-production.

### `film-make` post-production — `tools/film/post.mjs` (dev tooling, #397)
- ✅ `tools\cli film-make video/<name>.json` joins clips into one mp4: logo title and end cards, crossfades, burned-in captions from cue files plus an `.srt`, offline Piper voice-over, and a licensed music bed ducked under game sound and voice (about -14 LUFS). Covered by `node tools/film/film.mjs selftest`; proven on `video/tooling-test.json` (not for posting).

### `LF_FilmCameraPath` — `Feature/Film/` (filming harness)
- ✅ Dev-only, like the agent harness. `LF_FilmCameraPath` holds timed camera
  keys (position, look-at, FOV); `Sample(t)` eases between keys with
  smoothstep and clamps at both ends. Out-of-order keys are rejected and
  logged (`[LFDebug:Film]`). Covered by `film/camera-path` (LOGIC).
- ✅ `LF_FilmCameraRunner.Play(path)` spawns a manual camera with input off,
  hides the HUD, moves the camera each frame, then restores the previous
  camera and HUD. Harness verb `camera` plays it (directed camera rig, #392,
  next section).
- ✅ `tools/film/film.mjs` records a window or region to mp4 (ddagrab +
  NVENC; gdigrab when ddagrab delivers no frame in 5 s), makes a GIF preview,
  films an agent-box flow, and publishes a clip to the `film-evidence`
  prerelease for a PR comment. Proven in game 2026-11-08: a game client on
  DebugWorld, flow `film-debugworld`, 20.7 s at 1280x720, 2.8 MB (PR #111).
  Screen capture records a frozen frame when the window is unfocused and
  cannot see the hidden `LF-Agents` desktop (#91).
- ✅ `LF_FilmRecorder` records inside the game client: a per-frame callqueue
  tick calls `System.MakeScreenshot` at a fixed rate (default 15 fps) into
  `$logs:el-film/<name>/`, with HUD and menus, and writes `index.txt` with
  each shot's real time and frame-time stats (`[LFDebug:Film]`). Harness verb
  `film start <name> <fps> [max s]` / `film stop`; a dedicated server refuses
  (`error no_renderer`). `tools/film/engine.mjs` encodes the BMPs at true speed
  (concat with per-frame durations, NVENC) and deletes them. Wired as
  `film.mjs engine|encode`, `film.mjs flow <flow> --engine`,
  `journey.mjs --film`, and `session.mjs film` / `proof --film`. Covered by
  `film/recorder` (LOGIC). Proven 2026-11-08 on the hidden desktop, 1280x720,
  client capped at 30 fps: a 64.5 s journey clip (arrive, job, earn, fund, atm,
  shop) and a 27.9 s `world-atm` clip, capture 14.7-15.1 fps, game 29.5-30
  fps, frame after a shot 33.3-33.5 ms vs 33.2-34.5 ms otherwise, one 243 ms
  stall in 64 s (shop menu, not a shot frame).

- ✅ Game audio (#396): the engine cannot record sound, so `tools/film/lf-procaudio.cs`
  (Windows process loopback, `AUDCLNT_PROCESS_LOOPBACK`) captures only the game client's
  process into a 48 kHz float WAV (other host sounds never enter it; the WAV is the separate
  audio track). Harness verb `film slate` plays a UI beep and logs `# slate=<ms>` in
  `index.txt`; `tools/film/audio.mjs` finds the beep in the WAV, lines audio up with the
  frames and muxes an mp4 (`street-clip.mjs --audio`, session started with `--sound`). A start
  and an end slate give the offset and the drift (the mux uses their mean; one unrecorded warm-up beep plays first because the first play is heard about 0.5 s late). The recorder refuses a silent capture
  (`mean_volume` under -60 dB, or no slate heard) and warns when the default output is muted
  (#482). Covered by `node tools/film/film.mjs selftest` (pure parts). Proven on this PC 2026-11-10: three takes read start-to-end beep drift of -96, +119 and +52 ms; the laptop run waits on #482 and a free lane.

### `LF_FilmActor` / `LF_FilmDirector` — `Feature/Film/` (scripted actors, #393)
- ✅ Dev-only, harness verbs `scene start <name> [hour|-] [weather]`, `scene stop|reset|status` (server, through the
  character RPC and `LF_DevGate`) and `camera orbit|follow x y z size seconds`
  (client). An actor is a spawned `Character_Roleplay` on a timed step list:
  `walk`, `run`, `enter`, `drive`, `exit`, `use <action class>`, `wait`. Waypoints
  are authored (no navmesh). No progress for 6 s logs `[LFDebug:FilmActor] ... stuck`
  and stops that actor. A `camera follow` low-passes the followed entity's position (time constant 0.25 s, `LF_FilmCameraPath.SmoothAlpha`), because a car driven in 100 ms velocity steps made the camera shake. Pure maths (steer, pedals, stuck detection, follow smoothing) is covered by
  `film/actor-math` (LOGIC).
- ✅ Scene setup and reset (#394, `LF_FilmScene.c`): a scene is a clock hour, a weather state, the day frozen,
  its actors and vehicles and its camera cues. `scene start street` snapshots the world's local clock hour, weather state, weather looping and day advance, sets the scene's defaults (street: 16:00, Clear, frozen), then
  spawns; `scene start street 21.5 Overcast` overrides one take (`-` keeps a value; an hour outside 0..24 returns
  `error:bad_hour`). `scene stop` (alias `reset`) despawns every actor and vehicle the scene made and writes the snapshot
  back, so every take starts from the same light. Logs `[LFDebug:FilmScene]`. A world with no time and weather manager
  logs an error and runs the scene without a clock. Camera cues stay in the take script (`tools/film/street-clip.mjs`).
  Covered by `film/scene-env` (LOGIC: defaults, override, bad hours, clock conversion); the world write is proved
  in game (PR). Not done: scenes other than `street`.
- ✅ Walk, run and `use` proven 2026-11-10 (the store till action ran for an actor,
  `canPerform=1 uid='film-walker'`). Actors read a stand-in player uid from
  `LF_FilmIdentity` in a `LF_HARNESS` build only (`LF_Utils.GetPlayerUID`).
- ⚠️ Driving: an actor gets into a base-game car (`MoveInVehicle` + `StartEngine`
  worked). Throttle, steering and brake set on `VehicleWheeledSimulation` did NOT move
  it (speed stayed 0 for 9 s, also with clutch 0, gear set and `ForceEnableSimulation`).
  What works: setting the physics body's velocity and angular velocity each tick
  (`Physics.SetVelocity`) along authored waypoints at a steady 30 km/h, wheels
  turning, driver visible; a final taper and zero velocity stop it. No real
  throttle, braking or gear model, and a steering model that only turns the body.
  Clip 2026-11-10 (`agent-box/reports/film/`, PR for #393): walker uses the store till
  while a car drives down the main street.
- ⚠️ Actors in DebugWorld are underwear characters (no loadout). `camera follow`
  tracks the vehicle nearest the given point, client side.

## Cross-cutting risk summary (re-audited 2026-08-30; see per-feature rows)

1. **Money — one canonical path, all payouts cash.** The entity bank
   `LF_BankAccountComponent` is **deleted**; `LF_ATMManager`/`LF_BankAccount`
   is the only bank and the ATM is the only cash↔account boundary. Duty pay, benefits,
   trader sells, shop sells, robberies and job rewards all pay `LF_MoneyUtils`
   cash. Shop buy/sell correctness closed (partial refunds), ATM deposit/
   withdraw moves real cash with rollback. Remaining: no shop stock / per-item
   max quantity.
2. **Security — mostly closed.** Fruit-catcher score is server-clamped
   (pin test); `RpcAsk_DebugGrantLicense` removed; quantity RPCs have an
   ownership gate; gather/process actions re-validate server-side and reject
   malformed configurations. Remaining: multiplayer proof for the RPC edges.
3. **Persistence — improved.** Cache-eviction churn fixed (account stays
   resident). The serializer writes a `version` header
   first (4) and `validate-persistence-version.ps1` checks it against the
   payload order. Remaining: jail, breach, and bank-vault state has
   `Export` / `Apply` but is not in the save serializer, so it is lost on
   restart.
4. **Compile risk — resolved.** `LF_EFactionType` is defined in
   `LF_WhitelistManager.c`.
5. **Client/server breaks — mostly closed.** Police arrest/fine bridge and
   notification targeting are fixed; the death screen (client) → respawn RPC
   (server) bridge is new and needs a manual multiplay pass. Remaining:
   hand-carry holster flow is client-only.

## Toys

### `LF_JumpPad` — `Feature/Toys/LF_JumpPad.c`
- ✅ A trigger sets the entering player's velocity upward once for that enter.
  A second callback before they leave applies the zero vector. Leaving clears
  the enter, so the next entry launches again. A disabled pad applies the zero
  vector and does not call `SetVelocity`. Ported from EveronLife
  `EL_JumpTrigger` (MIT, Copyright (c) 2022 EveronLife). (verified 2026-10-11)

### `LF_SpikeStrip` — `Feature/Toys/LF_SpikeStrip.c`
- ✅ A trigger. A wheeled vehicle that enters has each wheel inside the strip
  box set to zero health. Ported from EveronLife `EL_SpikeTriggerComponent`
  (MIT, Copyright (c) 2022 EveronLife). (verified 2026-10-11)

## Service calls (#248)

- ✅ `LF_ServiceCalls` routes a call to any service in `Configs/Comms/LF_ServiceCalls.conf` (police, medic, taxi; receiver is on-duty police or a current job). An unknown service is rejected with an error, nobody on duty logs "no answer", otherwise `LF_OnServiceCall` is published on the event bus. No caller yet; the payphone is #247.

## Hidden crime record (#270)

- ✅ `LF_CrimeRecord` (`Feature/Crime/LF_CrimeRecord.c`) subscribes to `LF_OnCrimeCommitted` (`EnsureSubscribed`, game-mode start) and counts completed crimes per account uid and offence key, so one robbery counts once. An unpaid ticket is not counted. Server only, never replicated, no UI. Read with `LF_CrimeRecord.Count(uid, offence)` / `Total(uid)` (the fence #271 and serials #272 use these). Persisted as the `crimerecord` payload (save version 15). Tests `crime/record`, `crime/record-persist`. Crime paths wired to the event today: robbery, vehicle and weapon theft, break-in, hospital, murder; gas station and contraband run count once their success path reports through `LF_Dispatch.Report`.
- ✅ Fence (#271): an NPC (`Prefabs/Characters/Npcs/LF_Fence.et`, DebugWorld `FenceNpc`) with an `LF_ShopComponent` set to `m_bFence`. Each `LF_ShopItem.m_iMinCrimes` is the completed crimes the buyer needs on the hidden record (`LF_CrimeRecord.Total`): drill 3, charge 5, keycard 8. `LF_ShopComponent.Buy` asks `LF_FenceRules.Check` before any cash moves; a refusal logs `[LFDebug:Fence] refused ...` and prints `#LF-Shop_RefuseNotTrusted` on the order slip. Prices are the per-prefab `ShopPrices.conf` rules (`m_Prefab`, matched by GUID). Each sale publishes `LF_OnFenceSale`. Test `crime/fence-gate`.
- ✅ Fence serials (#272): each tool the fence sells carries an `LF_SerialComponent` (`Prefabs/Items/Fence/`; saved with the item by `LF_SerialComponentSerializer`, readable through the Read serial action). `LF_FenceSerials` subscribes to `LF_OnFenceSale`, stamps the delivered tools and books each serial to the buyer's account (server only). `LF_FenceSerials.Identify(serial)` / `IdentifyItem(item)` mark the buyer's wanted reports identified; the evidence locker (#269) calls it when it checks a bagged tool (not wired until #269 lands). Persisted as the `fenceserials` payload (save version 16). Carried tools keep their serial across a restart (#470): `LF_StorageSnapshot` captures it per snapshot item and saves it as the `itemserials` payload (save version 22), restored onto the re-spawned item on apply; a stale row is skipped and the item comes back unstamped. Tests `investigation/serial`, `investigation/serial-persist`, `persist/carried-serial`.

## Payphone (#247)

- ✅ `LF_Payphone` (prefab `Prefabs/Comms/LF_Payphone.et`, the base-game phone booth): one action per preset: report a crime, an emergency or someone suspicious (police), ring for a doctor (medic), ring for a cab (taxi; mechanic waits for a mechanic job). The coin comes from carried cash (`m_iPayphoneCost` in `LF_ServiceCalls.conf`), per-player and per-phone cooldowns apply, and nothing is charged when nobody is on duty. The call is `LF_OnServiceCall` on the bus; `LF_ServiceCallDispatch` turns a police call into a radio line to on-duty officers with the phone's place (`LF_Dispatch.Receive`), and the dispatch log shows it. Coin and dial-tone sounds play from the booth. Not done: free text, handset animation (the booth has no handset mesh). Placed in DebugWorld at the square, the fuel station, the hospital and the police station (#254), with a police dispatch board (`Prefabs/Comms/LF_DispatchBoard_Police.et`) beside the wanted board.

## Dispatch board (#250)

- ✅ `LF_ServiceBoard` (LOGIC-ish WORLD `comms/board`): calls whose service lists the `board` channel are pinned to that service's board and expire after `m_iBoardExpirySeconds` (900). A medic call shows on the medic board and not the police board; only the service's on-duty staff read the details, anyone else gets a closed board. World board: `Prefabs/Comms/LF_DispatchBoard_Medic.et` (the wanted board with `m_sService`), at the DebugWorld hospital. Crime reports now go out as police service calls (`LF_Dispatch.Report` publishes `LF_OnServiceCall`), so they and payphone calls share one path to the radio, the dispatch log, and the board. Late joiners read current calls on demand. Not done: teleprinter clatter, taxi board prefab.
- ✅ Taking a call (#251, WORLD `comms/take`): the medic dispatch board has a "Take the call" action (`LF_ServiceTakeAction`). The server checks the taker is on duty for the service and takes the oldest open call with room (`m_iMaxTakers` per service in `LF_ServiceCalls.conf`: 1, police 2), publishes `LF_OnServiceCallTaken`, marks the call taken on the board, and tells the caller "A unit is on the way" by an owner RPC to their controller (found by player id). Calls that expire untaken log a warning. Not done: take from a radio entry or the pager (#249), the place marked on the taker's paper map, a take action on the police wanted board.
- ✅ Pager (#249, WORLD `comms/pager`): `Prefabs/Items/Roleplay/LF_Pager.et` (`LF_PagerComponent`, the base-game ANPRC68 radio mesh) beeps and stores the call when a call whose service lists the `pager` channel reaches its carrier (police, medic, taxi in `LF_ServiceCalls.conf`; receivers are the people on duty or in the job right now). It keeps the last `m_iPagerMessages` (5) calls, newest first; "Read the pager" opens the card. The police duty kit (DebugWorld game mode) includes one, so it stays in the duty inventory on clock-out. The beep is one broadcast from the item, heard by the carrier and anyone beside them. Not done: a small RT-texture screen, the medic and taxi kits (no duty desk yet), a purpose-made pager mesh and beep sound.
- ✅ Service radios (#252, WORLD `comms/radio-preset`): the police and medic duty kits carry a vanilla ANPRC68 handheld; the duty swap (and a reconnect on duty) tunes its first transceiver to the service frequency, `m_iFrequency` in `LF_ServiceCalls.conf` (kHz; police 41000, medic 42000, on the radio's 200 kHz grid). Vanilla radio rules apply: range-limited voice, nothing heard off-frequency. A radio off duty stays on its factory channel; every clock-in retunes (a restored inventory is a prefab snapshot). `LF_ServiceRadio`; harness verb `radio`, `session.mjs radio-proof`. Not done: vehicle radios (no service vehicle prefab yet) and the dispatch chirp on the radio net (it is still a local sound; the scanner, #253, builds on the call bus). Manual: two officers talk at range, a civilian off-frequency hears nothing.
- ✅ Police scanner (#253, LOGIC `comms/scanner-illegal`, WORLD `comms/scanner-hears`): `Prefabs/Items/Fence/LF_Scanner.et`, sold by the fence (2 completed crimes, $900, serial-stamped like the other tools). A carrier who is not a receiver hears every police and medic call (services with a radio frequency in `LF_ServiceCalls.conf`): chirp, a toast with the summary, and `LF_OnScannerHeard` on the bus. Receive only. It is illegal (`m_bScannerIllegal`): the officer's search (`LF_ConfiscateAction`) takes it and files `#LF-Offence_Scanner` once, which the crime record counts. Not done: searching a suspect who is not wanted. Manual: a scanner holder hears a dispatch chirp while an officer on duty hears it too (`session.mjs scanner-proof`).
- ✅ Scene evidence (#268, WORLD `investigation/casing`): a crime report opens a scene (`LF_Evidence`, `Configs/Investigation/LF_Evidence.conf`: 300 s window, 30 m radius). A player shot inside it leaves one casing at the shooter (6 per scene, 40 in the world); an item dropped inside it gets a marker. An on-duty officer's "Bag as evidence" makes an evidence bag (`LF_EvidenceBag.et`) that lists the report, place and minutes lying; the piece rides inside as a hidden child. Untouched evidence expires after 900 s. Not saved across a restart. The casing hook is the `OnProjectileShot` event on the body; confirm it fires for a client's shots on a dedicated server.
- ✅ Vault fence tools (#273, WORLD `crime/vault-tool`): `LF_BankVaultComponent.m_sRequiredTools` ("drill,charge", any one) gates `LF_Bank.Rob` for a player robber; none held logs `rob refused` and the offer reads `#LF-Bank_NeedTool`. A drill is kept (a charge is consumed by `LF_Breach.Arm`, which now also accepts the fence `LF_Charge`); the drill jams with `m_fJamChance` (0.15) and either way is left at the vault, where the scene-evidence hook marks it. It rests 1 m to the robber's right (`LF_Bank.ToolRestSpot`, LOGIC `crime/drill-left-beside`); it used to spawn at the robber's feet, so they stood inside the bench drill while they worked (fixed 2026-11-11, seen on the #571 trailer take). The drill is the vanilla bench drill at 0.4 (a 0.47 m safe drill, standing upright): an item out of an inventory is back at scale 1, so `LF_Bank.SetDown` lays down a fresh copy with the same serial (WORLD `crime/drill-left-size`). Its serial names the buyer at the locker. Drill time is the vault fuse. Every attempt publishes `LF_OnVaultBreach` (opened, jammed, refused). Not done: drill animation and loop sound. A script-driven `Rob` with no robber entity skips the tool check.
- ✅ Evidence locker (#269, PERSISTENCE `investigation/locker`): `Prefabs/Investigation/LF_EvidenceLocker.et` at `Roadmap_Police_EvidenceLocker` (police station, DebugWorld town). "Log evidence bags" (on-duty police) opens each carried stamped bag, calls `LF_FenceSerials.IdentifyItem` on every piece, records a row against the crime report and consumes the bag; the locker accepts nothing else. Rows are the `evidencelocker` payload (save version 18). A forced-open locker (`LF_Breach`, target `LF_EvidenceLocker`; the break-in is reported by the charge) can be burned by anyone but on-duty police. Also fixes `LF_Drill`/`LF_Keycard`, which spawned with no components (fresh `ActionsManagerComponent` GUID shadowed the inherited one). Not done: the wanted board does not show "evidence on file".
- ✅ Witness description from the payphone (#266, LOGIC `investigation/witness`): a crime report records every player in sight (`LF_Witness`, `Configs/Investigation/LF_Witness.conf`: 60 m). The payphone carries "Describe their clothing / vehicle / Give a plate fragment"; the server answers from its own snapshot of the suspect (the report's clothing and vehicle, the plate from `LF_LicensePlateManagerComponent.m_Registration`), limited by the caller's distance: clothing within 25 m, vehicle within 45 m, plate characters fall by one per 8 m and stop at 45 m. A thing is said once; anonymous by default (`m_bAnonymous`). Statements publish `LF_OnWitnessStatement` and show on the wanted board under the dispatch log (police view). Session-local, not saved. Not done: sight is range only (no line-of-sight trace).
- ✅ Officers question witnesses in person (#267, LOGIC `investigation/npc-witness`): a crime report also records every town NPC (`LF_NpcBaseEntity`) within `m_fNpcSightRange` (30 m). On-duty police get "Ask what you saw" on every NPC (`LF_WitnessAskAction`, on `Npc_Base.et`) and on players (`witness` role slot, `LF_WitnessRole`). An NPC witness recalls each thing it could see with chance `m_fNpcAccuracy` (0.6), so statements are partial; a player says everything within its distance limits; an NPC that stood out of range says it saw nothing. Statements land on the wanted board as for the payphone (`LF_OnWitnessStatement`, channel `officer`). DebugWorld gets `Roadmap_Fuel_Clerk` beside the fuel till. Not saved.
- ✅ **The ambulance delivers a patient to a hospital bed for the reduced bill (#262).** A medic carries a downed patient (#261) to a bed, or loads them into the ambulance (`m_aStretcherVehicles` in `LF_Medic.conf`, flagship `LF_M997_Ambulance`) and drives to the hospital. "Put patient in bed" (`LF_BedDeliveryAction`, on `Prefabs/Medic/LF_HospitalBed.et`, placed at the playtest hospital) is shown to an on-duty medic; `LF_BedDelivery.Perform` finds the held body or the downed occupant of a stretcher vehicle within `m_fBedVehicleRange` (14 m) of the bed, re-checks medic on duty, patient downed and `m_fBedRange` (3 m), moves the body onto the mattress, `LF_Downed.Clear`, writes the revive bill (`m_iReviveBillAmount`, 100, not the 150 wake bill; the medic share applies) and publishes `LF_OnPlayerRevived` then `LF_OnPatientDelivered`. A patient seated in a stretcher vehicle does not bleed out. The patient wakes on their own client too: the cleared downed flag makes the owning client recompute vanilla unconsciousness (#442; a long ride left nothing else to trigger it). Test `medic/bed-delivery`; `session.mjs bed-proof`. ⚠️ No stretcher-load sound; the seat is the vanilla cargo seat, not a stretcher.
- ✅ **New Life Rule wristband and reminder (#264).** A real death records the death scene (`LF_NewLifeRule.RecordDeath`, from `LF_SpawnLogic.OnPlayerKilled_S`); the hospital wake gives the wearer a wristband (session state plus a toast; no item) for `m_iNewLifeMinutes` (15, `LF_Medic.conf`, also printed on the bill). A revive never dies, so it gives none. While a wristband lasts, a server check every `m_fNewLifeCheckSeconds` (5; the timer runs only while a wristband exists) watches the wearer: coming back within `m_fNewLifeRadius` (75 m) of the scene after having been outside, inside the window, gives one quiet toast and one incident row (kind `nlr`, amount seconds since waking). Nothing blocks the player. Bus events `LF_OnNewLifeWristband`, `LF_OnNewLifeReturn`. Test `medic/nlr`; `session.mjs nlr-proof`. Logs `[LFDebug:NewLife]`. Not persisted: a restart forgets the wristband (no save version change). `m_iNewLifeMinutes` moved from `LF_Respawn.conf` to `LF_Medic.conf`.
- ✅ **Village hospital placement (#265).** `Street_Hospital.layer` now reads as a 1989 Everon village hospital: the medic duty desk, dispatch board, payphone, three beds, till, cabinet and the ambulance (`Roadmap_Hospital_Ambulance`) were already placed; this adds the ambulance bay (four `ConeTraffic_01` cones, a pole sign `Sign_HospitalBay` with catalog row `hospital_bay` in `LF_Signs.conf`) and waiting-room dressing (two flowerpots, a bench). The `hospital` sign card now describes the desk, beds and bills. No scripts, no save change; bill prices stay in `LF_Medic.conf` / `LF_Respawn.conf`. Shots in `website/public/docs-images/265/`; `session.mjs bed-proof` drives the two-client run. ⚠️ The patient's wake-up after a bed delivery was still INCAPACITATED at the proof's timeout (ambulance/bed path, #442).

### Cash van (#274) — `Feature/Crime/LF_HeistVault.c`, `LF_CashVanRoute.c`, `LF_BankLoot.c`
- ✅ A cash van (`Roadmap_Cash_Van`, the base-game `S1203_cargo_brown`) carries a cash box (`LF_CashVanBox.et`, a 0.8 m strongbox, its own entity; a replicated child under the replicated van made clients disagree on the item table). The box stands on the ground at the van's kerb-side door (`m_vBoxOffset` 1.35 0 -1.4, `m_fBoxYaw` 90, set on the surface by `LF_Dispatch.RestingSpot`); it used to hang off the rear step at 1.4x over the road. The van roof carries the alarm mount. The box is a bank vault (`LF_HeistVaultComponent : LF_BankVaultComponent`): it needs a fence tool (drill or charge, as the bank), reports as `#LF-Offence_CashVan` through the same crime report, dispatch and witnesses, and a drill is left as evidence. Opening it spills `m_iBagCount` money bags of `m_iCashPerBag` in a tidy row beside the box, on the side away from the robber, each on the surface under it (`LF_BankLoot.BagOffset`/`BagSide`, WORLD `crime/haul-tidy`). The bag is `SackUniversal_01` at 0.4 (0.54 m); spawned bags were 1.36 m because `LF_Utils.SpawnEntityPrefab` dropped the prefab's root scale, which it now keeps (the spawn matrix carries `LF_Utils.PrefabScale`). Every scaled prefab the addon spawns is pinned by WORLD `spawn/prefab-scale` (gunpowder 0.012, a 0.15 m heap; linen 0.3, a 0.2 m cloth; the bag; the drill); the scaled vanilla gravel piles, bush and poplar are layer-placed only. The street lamp that stood 1 m behind the box on the verge (196, 255.4) is moved along its row to 204.5, 255.4. The haul is cash in a carried bag, never a bank balance. Test `crime/cash-van`.
- The van is **parked on a schedule, not driven**: base-game AI driving is not reliable enough to carry a robbery. `LF_CashVanRouteComponent` moves the van to the next stop marker (`Roadmap_Cash_Stop_Bank`, `_Store`, `_Fuel` on the main street, `Street_CashVan.layer`) every `m_fDwellSeconds` (300) unless a player is within `m_fClearMetres` (40), then stocks the box afresh (bags the last robbery left lying are removed, the lock closes). The move is a transform set of the van and the box, so the van never drives off a player. A van a player drives away leaves the box behind.
- Every opening publishes `LF_OnHeistLoot` (kind, cash, target id) on the bus. Not saved: the bags and the route position reset on a restart.
- ✅ Lockpicking a door (#411, LOGIC `lockpick/door`, WORLD `e2e/lockpick-door`): `Prefabs/Items/Fence/LF_Lockpick.et`, a stack whose count is its uses (fence, no record needed, $250, serial-stamped). A locked `LF_LockComponent` door that carries `LF_LockpickTargetComponent` (`m_bPickable`; off for a vault or the jail door, which refuse with a logged reason) offers `LF_LockpickAction`. `LF_Lockpick.Begin` re-checks on the server, then runs a timed attempt (20 s at skill 0 down to 6 s at skill 10; skill is the character level minus 1, so no new save data), dropped if the picker walks off, goes down, no longer carries the pick (WORLD `lockpick/keeps-pick`) or the door is opened meanwhile. A success unlocks the door and wears one use; a failure may snap the pick (one use) or just slip; a plain failure wears nothing. A success always files an `LF_WantedReports.Report` (`#LF-Offence_Lockpick`: event bus, crime record, evidence scene, witnesses); a failure only when its noise roll is heard. It never raises wanted or radios the police on its own. Placed at `Roadmap_Lockpick_Door` (lock-up, `Street_Lockpick.layer`). Harness: `lockpick` verb gives five uses. Manual: the held-action animation and a witness reaction.
- ✅ **Fence in the village** (#276). `FenceNpc` (Vince, `Crimes.layer`) moved from the test annex to the barn lot at 186 0 311, north-west of the bank; `session.mjs village-proof` drives the fence, vault, cash van, serial and evidence-locker chain with two clients (the debug police kit now carries a `Backpack_ALICE_Medium` so the officer can hold the bagged drill; the harness `pack` verb is gone). No save change.
- ✅ **Bank robbery kit stand (#605, playtest only).** A chest stand (`Roadmap_Bank_RobKit`, `Props/Debug/LF_BankRobKitStand.et`) beside the bank at 198.6, 259.5 offers *Take bank robbery kit* (`LF_BankRobKitAction`). `LF_BankRobKit.Grant` tops the character up to one drill, one keycard and one fence charge, built from the prefab constants `LF_Bank.PickTool` and `LF_Breach.HasCharge` read; a second press adds nothing, and an item with no room is left at the feet with a logged warning. Gated like the money top-up: the server refuses with a logged warning unless `LF_SpawnLogic.m_bDebugMoneyTopUp` is on (the playtest game mode, not the flagship). No save change. Test `bank/rob-kit`. Logs `[LFDebug:BankKit]`.

### Police armory heist (#275) — `Prefabs/Police/LF_ArmoryStore.et`, `LF_Bank.c`
- ✅ `Roadmap_Police_ArmoryStore` (`Street_Police.layer`, beside the duty armory) is a heist target: a locked cabinet that needs the fence **keycard** (kept, not consumed) or a charge, config on its `LF_HeistVaultComponent` (`m_aLoot` weapons, `m_iStock` hauls, `m_fRefillSeconds`, `m_fCooldownSeconds`). Breaching it files `#LF-Offence_ArmoryHeist` through the normal crime report with the **loud** alarm even with officers on duty (`LF_Dispatch.Report` `forceLoud`), so every on-duty officer is radioed. Without the keycard the action is refused and nothing is filed. Test `crime/armory`.
- Loot lies in front of the store as `LF_ArmoryM9` / `LF_ArmoryM16` (vanilla weapons plus `LF_SerialComponent`). Each is stamped with a fence-book serial booked to the thief, so a weapon found at a scene and logged at the evidence locker identifies them like a fence tool. `LF_OnHeistLoot` carries the serials. It pays again only after a police repair, the loot cooldown and stock; an empty store refuses (`#LF-Heist_Empty`) until the refill adds a kit back.
- Not saved: the stock and cooldown reset on a restart.
- ✅ Pick a vehicle lock and hotwire (#412, LOGIC `lockpick/vehicle`, WORLD `e2e/lockpick-vehicle`): a car that carries `LF_LockpickTargetComponent` and `LF_LockpickAction` can be picked like a door (the same timed, noisy, pick-wearing attempt; the vehicle lock opens and its saved lock flag follows). `LF_VehicleLockComponent.m_bIgnitionLocked` (off by default, so every other vehicle starts as before) makes the engine need the vehicle key: `SCR_CarControllerComponent.OnBeforeEngineStart` (modded in `LF_Hotwire.c`) refuses a start unless the driver holds the key or is the player who hotwired it. `LF_HotwireAction` at the wheel runs a server-timed attempt (15 s at skill 0 down to 6 s; success 50 percent at skill 0 up to 90 percent; a failure may be heard). A success lets that player start the engine, marks the vehicle stolen on the server (`LF_VehicleLockComponent.IsStolen`, replicated, session only) and raises the thief wanted by 1 (#495) and files a `#LF-Offence_VehicleTheft` report if heard; re-locking the car clears the hotwire. A start with the key never marks it stolen. Placed at `Roadmap_Lockpick_Car` (`Street_Lockpick.layer`, a beige S105). Harness: `engine [entity]` seats the character and starts the engine. Not done: the stolen flag is not read by anything yet (#414 reports it), it is lost on restart, and other prefabs opt in by adding the target component, the two actions and `m_bIgnitionLocked`.
- ✅ Stolen vehicles: report, board, plate check, recovery (#414, LOGIC `stolen/report`): a car taken without its key carries the replicated stolen flag (#412); the owner (the key holder whose uid owns the vehicle record) uses *Report the car stolen* on it and the server files an `LF_StolenReport` (vehicle id, plate, name, place) and publishes `LF_OnVehicleReportedStolen`. The report shows on the police wanted board as `STOLEN CAR: <model>, plate <plate>, reported at <place>`, above the dispatch log, for on-duty officers only. An on-duty officer uses *Check the plate* on any vehicle: `LF_StolenVehicles.CheckPlate(plate, report)` answers STOLEN (arrest case, a toast), CLEAR (registered, no report) or UNKNOWN; it reads reports only, never the flag, so an unreported theft is not known to police. That seam is the plate-check API stops and searches (#419) reuse. *Recover stolen vehicle* (officer, car empty) sends the car to its owner's garage through `LF_VehicleOwnership.Store`, closes the report and publishes `LF_OnVehicleRecovered`. Placed on the beige S105 prefab (`EL_S105_beige.et`: ignition lock, pick target, pick, hotwire, report, check, recover), so a bought S105 can be stolen from its owner; the lot display car overrides the ignition lock off. Not saved, by design: the car entity does not survive a restart either (an owned car left on the street is impounded at load), so no payload or version bump. Not done: reporting by payphone, other prefabs, an arrest on the plate itself.
- ✅ Chop shop yard (#415, LOGIC `chopshop/payout`, WORLD `e2e/chopshop-yard`): a bench (`Prefabs/Crime/LF_ChopBench.et`, an engine crane) placed in a walled back lot at (322, 300) of the playtest town (`Street_ChopShop.layer`, `Roadmap_ChopShop`). Hands-on, the chain-station pattern (#417) with `LF_StationProgress`: *Take this car apart* starts the nearest car within 10 m of the bench (engine off, doors locked), *Strip it down* is a held action that adds one unit of work for whoever holds it (6 units; the prompt says how far along it is in words), *Take the cash and parts* pays out. Only a stolen car is taken (the #412 flag); a worker who holds the key of an owned car is refused; a car with someone inside is refused; the yard takes no new car for 300 s after a payout. A refused car says why (`#LF-Chop_NotStolen`: only stolen cars, lockpick or hotwire a car that is not yours; `#LF-Chop_KeyedOwner` for your own) and is left untouched (nothing is locked, stopped or stripped before the checks pass); the load action reads *Take this stolen car apart* and a refusal logs the flags (#601). "Hot" is the #412 flag, set only by a keyless start of a locked-ignition car; a found or road car without an ignition lock is not hot (test `chopshop/steal-and-chop`). The server pays `LF_ChopShopRules.Payout`: the vehicle value (`Configs/Crime/LF_ChopShop.conf`, per spawned prefab, capped at 3000) times 30 percent, in carried cash through the ledger (reason `chopshop`), plus 2 `LF_CarParts` items (a trader buys them: `ShopPrices.conf`). Collecting deletes the car with a particle effect, scraps its ownership record (`LF_VehicleOwnership.Scrap`: no impound lot or garage gives it back, its plate is freed), closes any stolen report (#414), files a `#LF-Offence_ChopShop` crime report (an evidence scene at the yard, witnesses, the wanted board) and publishes `LF_OnVehicleChopped`. Session state: a batch in progress and the cooldown are not saved. Not done: reporting a car that was chopped to its owner, a wreck mesh, the owner organization of the yard.
- ✅ **Officer searches for chain goods** (#419, LOGIC `chains/search`, WORLD `chains/search-world`). `LF_ConfiscateAction` now works on any player (weapons and scanner still need a wanted suspect); a new `LF_VehicleSearchAction` sits on the lockable vehicle prefabs. Goods are the last-step outputs of a chain with `m_bContraband` (moonshine: the crate, `m_iBulk 2`); the server rolls, the find chance rises with load, a find seizes the goods and files `#LF-Offence_ChainGoods` on the bus. Vehicle finds charge the occupants; the plate owner (#414) hooks in at `LF_ChainSearch.SuspectsOf`. No save change. Cooldown (#472, LOGIC `chains/search-cooldown`): the server keeps the last search time per player uid or vehicle `RplId` (`LF_ChainSearch.TryBeginSearch`, pure `MaySearch`); a repeat inside `LF_PoliceConfig.m_iSearchCooldownSeconds` (120 s, 0 disables) rolls nothing and reads clean, so repeating cannot beat the find chance. Weapon and scanner confiscation of a wanted suspect is not gated. Not saved: memory only.
- ✅ Car lot polish (area 4): the four display cars (S105, UAZ, van, UAZ 452) stand in an even row of 4.5 m nose to the street at z 261, each with a chalkboard price board (`lot_price_*` sign ids, text from the lot price list) in front; the vehicle-lot sign moved clear of the cars. No script change.
- ✅ **General store and market polish (area 3, 2026-11-10).** The market tables no longer overlap (`TableRecreation_01_3` moved 1.5 m west), two tables stand under the east umbrella stands, and the site alarm prefab (`LF_SiteAlarm`, and so the loud one) carries a 3x vanilla `HazardBeacon_01` at 1.1 m so the alarm is readable in daylight where the point light was not. Navmesh "unreachable" flags on the market signs and NPCs are AI-navigation only; a player walks to all of them.
- ✅ Working animation for the drill and the lockpick (#513, WORLD `work/anim`): a player drilling the bank vault (#273) or picking a lock (#411, #412) bends to the work on every client. `LF_WorkAnim` (server, `Feature/Crime/LF_WorkAnim.c`) keeps one session per worker: the lockpick attempt starts it with its own timer (`LF_Lockpick.BeginAs`) and ends it on finish, drop or cancel; the drill starts a self-ending 6 s one (`LF_WorkAnim.DRILL_SECONDS`) when `LF_Bank.Rob` opens the vault with it. The server also stops it when the worker goes down, leaves 5 m or overruns. The worker's owner starts a custom loiter (#512 route) on `Anims/Workspaces/Work/LF_WorkPick.agr` (base-game `p_erc_supplies_repair_low_*`, `Gestures` 561) or `LF_WorkDrill.agr` (`p_erc_supplies_demolitionUS_low_*`, 562); the base game replicates it to every client and late joiners. Stops use the fast exit. Every rendering client logs `work animation playing`. Demo: `node agent-box/session.mjs work-anim-proof`. Logs `[LFDebug:WorkAnim] ...`. The locked-car pick and the cash van box drill use the same two paths (`LF_Lockpick.BeginAs` for any pick target, `LF_Bank.Rob` for the `LF_HeistRobAction` box), pinned by WORLD `work/anim-targets`; the proof drives all four and the watching client logs `work animation playing` four times. The worker's own check can fire after a 6 s drill already ended on a lagging machine; it then logs `work animation finished before the check`, not an error.
- ✅ **Action-context anchors (dev tool).** `tools/actions/place-action-contexts.mjs --anchor center|front|top|custom <entity|prefab>` places a `UserActionContext` point and radius from the `.xob` mesh bounds instead of by eye; `--batch` replays `tools/actions/anchors.json`; `--check` (run by `validate-action-reach`) flags points over 0.5 m off the mesh or a radius that misses a player 0.5 m away, and fails over 3 m. First users: the fuel station till, the police duty desk clock. See `docs/action-anchors.md`.
- ✅ (car polish) Vehicle lock action: "Get in" lists first, then a state-aware Lock/Unlock row (`Sort Priority` 5 on the lock action, `LF_VehicleLockComponent.ACTION_SORT_PRIORITY`), then "Store in garage" (`LF_VehicleStoreHereAction`, owner on foot, car and player within one garage radius, same server path as the counter). Occupants with the key can lock from the interior door context. U (`LF_VehicleLockToggle`, gamepad right-stick click hold) toggles the seated or nearest keyed vehicle; the server checks key and 6 m reach and plays a placeholder click (`SOUND_PICK_UP`). The garage counter mesh `Roadmap_Garage` now stands at the doors with a 3.5 m context so Store/Retrieve/ledger show from the doorway. The S105 lot price board data (`lot_price_s105`, 214.8 0 259.2) matches the other three; not reproduced statically.
- ✅ Keys are created at their holder (#620, WORLD `vehicles/key-inventory`): `LF_VehicleOwnership.KeySpawnOrigin` puts a purchased, collected or retrieved key at the holder's feet before the inventory insert, never at the lot or impound pad. A key made at a far pad was unresolved on the holder's client, and the key and ID card moves hung on the loading throbber. A key that does not fit logs a warning.
- Debug money top-up (LF_DebugMoneyTopUp, `LF_SpawnLogic.m_bDebugMoneyTopUp`, on only in GameMode_Roleplay_Debug): on spawn and every 60 s, bank under 100k fills to 1M and carried cash under 5k fills to 20k, ledger reason `debug-topup`; opening bank is 1M.
- Debug "Send me to jail" (LF_DebugJailMe, `LF_SpawnLogic.m_bDebugJailMe`, on only in GameMode_Roleplay_Debug): a red-rimmed board (Sign_DebugJailMe, about 235.5, 228.5) outside the jail whose action jails you for 60 s through the real sentence path (`LF_Jail.Imprison`, server side; the same call Arrest now uses). Refused with a log line when the flag is off, no cell is placed or you already serve.
- Spawn hub (#607, LOGIC/WORLD `spawnhub/travel`, `Feature/Debug/LF_SpawnHub.c`): the globe on the spawn-square table opens an LF_UIScreen list of every area in `Configs/Debug/LF_SpawnHub.conf` (one `LF_SpawnHubPoint` row per area, name `LF-SpawnHub_Pt_<id>`); a pick sends `RpcAsk_LF_SpawnHubTravel(id)` on the owned character and the server moves the body with `LF_Jail.MoveBody` (dismounts a seated player). Gate: `LF_SpawnLogic.m_bPlaytestHub` (on only in GameMode_Roleplay_Debug); a disabled mode, unknown id or missing body is refused with an `LF_Debug.Warn`. Since #693b the server also requires the body within 6 m of a travel globe (`LF_SpawnHub.MayTravelFrom`, found with `LF_ActionSite`; administrators and `LF_HARNESS` builds are exempt), refuses with `TOO_FAR` plus a toast (`LF-SpawnHub_TooFar`), and rate-limits travel to 2 per 10 s. The time and weather actions of the old annex globe now live on the same globe (`Street_SpawnHub.layer`). Proof: `spawn-hub-proof`.
- ✅ NPC damage and witness dialogue (#610, #615; WORLD `npc/damage`, LOGIC `investigation/witness-dialogue`): town NPCs are scripted entities with no character damage manager, so bullets did nothing. Only a prefab that carries an `LF_NpcHitZone` takes damage: ambient civilians (`Npc_Civilian_01.et`, `Roadmap_Civilian_1`/`_2` in `Street_Fuel.layer`) die to lethal damage (`LF_NpcDamage`, removed after 1.5 s), and the kill is murder through `LF_Murder` for a player killer. Protected on purpose, no hit zone: shop clerks and every `Npc_Default_01.et` child (quest givers, the fence, the police sergeant), because players need them. Root cause of the empty answers: `LF_Witness.CollectNpc` skipped every NPC as not alive (`IsAlive` is false for a non-character), so no town NPC had ever been recorded as a witness; fixed. Asking a witness (`LF_WitnessAskAction`, `LF_WitnessRole`) now names the NPC (`LF_WitnessDialogue.NameOf`, stable from its position) and one that saw nothing answers with one of four noncommittal lines (`#LF-Witness_Unsure1..4`: no confirm, no deny) instead of "You didn't see anything". Any NPC can be asked, clerks included. Harness: `shootnpc <EntityName>` (dev gate). Demo: `node agent-box/session.mjs npc-witness-proof`. Manual: a real bullet on a civilian.
- ✅ **Fenced crime site regroup (#608, #609, 2026-11-10).** The safe, weapon rack, tyre pile (steal a vehicle), evidence table with the wanted suspect, and the practice police desk now stand in one row at z 50 in the test annex (x 30, 34, 38, 42, 48), with four chalk info boards at the first, second, third and fifth (`crime_safe`, `crime_rack`, `crime_car`, `crime_police` in `LF_Signs.conf`: what it is, how, what you need, what next). The evidence table no longer clips the safe, the police desk table is spaced off its panel, and the PR-feature table moved out from under the lock-test police car. The robbery and apple-gather actions in the layers were hard-coded Spanish (`Robar Dinero`); they are now `#LF-Crime_Rob*` and `#LF-Job_GatherApples_*` keys. `validate-en-text.mjs` fails a Spanish-looking `Target_en_us` or literal layer/prefab `Name`/`Description`. No save change.
- ✅ **Quarry regroup (#618, area 11 follow-up).** The cement, gravel and sand chains and the logging chain (stand, sawmill, log pile, three cuttable pines) moved into the quarry pit; the cement plant no longer overlaps the east cliff and the cement NPC no longer stands in rock. Overlapping stones, spoil heaps, tyres, pallets and gravel dressing were cleared from the work spots. New `quarry_info` sign (`Sign_Quarry_Info`, `LF-Sign_QuarryInfo_*`) at the gate. Layer-only change (`CementChain`, `GravelChain`, `SandChain`, `LoggingChain`, `MiningChain`, `Signs`); mining (#596) and smelter (#598) code untouched.
- ✅ **Police search a back room (#600, LOGIC `shop/backroom-police`).** A shop with `LF_ShopComponent.m_bBackRoom` (the DebugWorld black market, `Shop_BlackMarket_Debug`) shows an on-duty officer `LF_BackRoomSearchAction` ("Search back room") beside the normal price sheet; civilians and off-duty officers never see it. The server (`LF_ShopComponent.Raid`, rules in `LF_BackRoomRules`) re-checks duty, reach (4 m) and the shutter, logs each stock line in the evidence locker as a `backroom` row through `LF_EvidenceLocker.LogSeizure` (no crime report; kept on load without one, no save format change), and shuts the room for 10 min of world time. A shut room refuses every buy and sell with `#LF-Shop_RefuseClosed`. The shutter is server-only state and the price sheet is stateless, so cop, owner and a late joiner all get the same refusal; it is not saved, so a restart reopens the room. `LF_Debug` lines: `[LFDebug:Shop] back room ... searched by`, `[LFDebug:EvidenceLocker] back room seizure`. Proof verb `backroom-police-proof`.
- ✅ **Playtest town: duty rows, dispatch area, test till (refs #146, #147, #202).** (a) The hospital duty desk `LF_MedicDutyDesk.et` restates its `LF_DutyClock` context and every action on the clock itself (the derived prefab used to replace the base list, so Clock in lost its context and floated off the device); `Sort Priority` puts the state-aware Clock in / Clock out row first, then the bill, then the fine ticket; the police desk got the same order. (b) New dispatch area (`Street_Dispatch.layer`, about 249 to 257, 229 to 233): `Roadmap_Dispatch_PoliceBoard` and `Roadmap_Dispatch_MedicBoard` (read and take a call) and `Roadmap_Dispatch_Console` (`LF_DispatchConsole.et`). The console carries `LF_DebugDispatchAction` rows *Test police call* (from the fuel till) and *Test medic call* (from the fuel payphone), which go through `LF_ServiceCalls.Place` with no coin; they work only where `LF_SpawnLogic.m_bDebugTestDispatch` is on (GameMode_Roleplay_Debug; the server refuses otherwise) and need a receiver on duty (`LF_DebugDispatch.Fire`). Tests `comms/debug-dispatch`, `debugworld/dispatch-area`. (c) The fuel till's *Rob the till* action sits 0.3 m above the till (was 0.8 m, floating), and `Roadmap_Fuel_Till` and `Roadmap_Fuel_Clerk` now stand at the fuel station building's own interior `CashierShop_01` (about 284.3, 240.2; the stand-alone counter at 263.2, 240.4 is removed); `Street_TillTest.layer` adds `Roadmap_Test_Till` on a counter at (244.5, 230.5) with a 5 s cooldown and no police minimum for a solo tester. Placed boards and the console are core-side debug-town props (`core-pack-inventory`: core).
- ✅ **Car lot price boards read the lot's price list (#540).** The `lot_price_*` chalkboards carry `LF_SignComponent.m_sPriceLotShopId` and `m_PriceVehicle`; the title in `LF_Signs.conf` is one template key (`#LF-Sign_LotPrice_Title`, "%1 %2") filled at read time with the offer's name key and `LF_MoneyFormat.FormatMoney(price)` from the `LF_VehicleLotComponent` offer (`LF_VehicleLotComponent.FindLot`, a per-machine registry). A changed `m_iPrice` changes the board and its Read card; a missing lot or offer logs `LF_Debug.Error` and hides the sign. Test `signs/price-board`.
- ✅ **Paper items stay storable in any test order (#699).** The receipt, fine ticket and hospital bill prefabs carry an `SCR_GadgetComponent`, hand-pose attributes copied from `SentenceSlip.et`, and `SizeSetupStrategy Volume`. Without the gadget component the engine let only the first of those prefabs spawned in a process be inserted into an inventory; later ones were refused (`TryInsertItem` false, tickets "lie at their feet") even on fresh characters, so test results depended on registration order. Test `inventory/paper-items`.
- ✅ **Every small mod item is storable on any spawn (#708).** The same engine quirk hit the pager, seed packet, keycard, lockpick, evidence bag and marker, vehicle and house keys, ID card and sentence slip: without `SizeSetupStrategy Volume` a later spawn read a mesh volume (440, 1073) instead of its `ItemVolume`, and the pager, seed packet and scanner also needed an `SCR_GadgetComponent` to fit (evidence, keycard and lockpick got one too, so room no longer depends on order). All carry them now; `inventory/paper-items` spawns each twice and checks volume and room, and `validate-item-volume` flags a new item prefab without them unless it is allow-listed with a reason. The farm loop test no longer needs a backpack or a registration-order workaround.
- ✅ **128-player cost budget and benchmark gate (#335).** `LF_Perf` (`Core/LF_Perf.c`) holds server counters, timers and gauges behind `-lfPerf` (or `LF_Perf.SetEnabled`), printed as `[LFDebug:Perf] count/time/gauge` every minute and read by tests; sites today: account refresh and publish, account-state bumps, ledger record, bleed-out advance, save point. `Configs/Perf/LF_PerfBudget.conf` (`LF_PerfBudget`) holds the limits. PERSISTENCE test `perf/budget-128` builds 128 synthetic characters (accounts, banks, crime, a money log at its 20000-row cap) through the managers, bus and serializer and fails when a cost passes a limit; `tools\cli perf` prints the table and writes `tmp/perf/report.md`. Validate: `validate-perf-budget.mjs`, `perf-report-selftest.mjs`. Numbers and what a synthetic run cannot show: `docs/architecture/performance.md`. ⚠️ No network or live-player measure yet; that needs a multi-client run (steps in the doc).
- ✅ **Police application (#128).** Duty desk action `LF_PoliceApplyAction` opens a paper form (`LF_PoliceApplicationForm`: name, why, availability) that `RpcAsk_LF_PoliceApplication` sends to the server. `LF_PoliceApplications` validates through `LF_PoliceApplicationRules` (one open application per player, not for an officer, field length), appends `STATUS|uid|name|why|availability|unix` to `$profile:LifeFramework/police_applications.txt` (not part of the save, no persistence bump), logs `[LFDebug:Police]` and toasts a receipt. Approval writes the police roster through `LF_WhitelistManager.AddFactionWhitelist`: an administrator picks `LF_PoliceReviewAction` (oldest open first; server re-checks the role), or a mission with `m_bLF_InstantPoliceApproval` (the playtest town) approves on submit. Test `police/application`; `session.mjs police-application-proof`. Not done from the issue: reject with cooldown, a letter at the desk, chief-rank approval.
- ✅ **Fuel till reachable (#729).** The till moved from inside the closed kiosk to `Roadmap_Fuel_Counter` (a `CounterShop_01` at the south service window, 283.6 237.2); the clerk stays behind the window mesh. The rob action faces -Z (anchors.json), so a robber standing south of the counter gets "Rob the till" with the till and clerk in view. `harness.mjs stand Roadmap_Fuel_Till s`; `crime/rob-reward` unchanged.
- ✅ **Props on counters (validate-prop-on-surface).** The general store register, the police and hospital duty-desk punch clock, the dispatch console radio and a market crate are placed wholly on their surface; `tools/validation/validate-prop-on-surface.mjs` fails a prop whose footprint hangs over 0.1 m past an edge (docs/agent-area-polish.md).

- ✅ **Police rank ladder is config (#754a, M3).** `m_aRanks` in `Configs/Police/LF_Police.conf` is the ladder: per rank `m_sId`, `m_sTitleKey`, `m_iFineCap`, `m_fPayPerSecond` (negative defers to `m_fPoliceDutyPayPerSecond`), `m_iDutySecondsForNext` and `m_aKit`. A rank is its index and a saved index never changes meaning, so `PATROL` 0 and `SENIOR` 1 stay first and a pack appends. `LF_PoliceRanks.FineCap/TitleKey/DutySecondsForNext/CollectKit` and `LF_DutyPay.RateForAccount` read the row; an unknown index clamps to the nearest rung, and a saved rank above the ladder loads as the top rank. A missing config falls back to the built-in two ranks. Test `police/rank-table`. Qualifications are #754b.
- ✅ (2026-11-12, #323 batch 0a) Annex twins for the crime and police fixtures. `Annex.layer` now holds `Annex_*` copies of the bank vault, bag, bell box and rob kit, the cash van and its box, the breach and lockpick doors and car, the chop bench, the S105 and the police escort car, the police desk, armory, wanted board and evidence locker, the hideout door, machine and stash, and the jail door and a bare jail cell marker, all placed with `tools\cli place` in the fixture yard at x 70..100, z 35..60 and each with its own breach, vault, hideout and shop id (`Annex_Hideout`, `annex-chop`). `LF_TestAnnex` (Tests) names them. 21 test files stopped quoting `Roadmap_*` names, so the village can move to the pack without them; `validate-annex-fixtures` baseline 45 files / 73 pairs to 24 / 40. Tests only, no gameplay or save change. The convoy drop ids and the placed jail cell lookup in `LF_Jail.SyncPlacedCell` still name the town (production lookups by name, a later batch).

- ✅ **Vehicle prices: one base price, per-action multipliers (#688, M3).** `Configs/Vehicles/LF_VehiclePrices.conf` (`LF_VehiclePricesConfig`, `Feature/Vehicles/LF_VehiclePrices.c`) lists each vehicle once (`m_Prefab` the lot's vanilla car, `m_SpawnPrefab` its locked child, found by GUID) with `m_iBasePrice`; buy, sell-back, chop, impound and garage are percent multipliers (`m_iBuyPercent` 100, `m_iSellBackPercent` 50, `m_iChopPercent` 30 with `m_iChopValueCap` 3000, `m_iImpoundPercent` 10, `m_iGaragePercent` 0) and an optional per-vehicle `m_i<Action>Override` (-1 derives it; 0 is free) wins. `LF_VehiclePricesConfig.PriceFor` returns -1 for an unlisted vehicle. Shipped numbers equal today's: lot 1000/2500/4000/3500/8000, chop 30 percent capped at 3000, impound 250 (an override per car). ⚠️ Not yet consumed: the lot offers, `LF_ChopShopConfig` and `LF_GarageConfig` still read their own values (test `vehicles/price-model` pins them equal); the consumers and the `vehicles` config.json section move over in #283. Sell-back has no desk yet.

- ✅ **Two-addon tooling (M3, #322).** Every repo launcher passes the addon folders as one comma-separated `-addonsDir` (`tools/lib/addons.mjs addonsDirArgs`; Workbench keeps only the last repeated flag, so the pack could not find the core), and `validate-addonsdir` rejects a repeat. `tools\cli build [addon]` and `tools\cli pack [addon] [--out dir]` take `LifeFramework` (default) or `LF_Everon1989` by name or GUID; `pack` now honours `--out` and defaults to `tmp/workshop-pack/<addon>`, so a second pack no longer replaces the first. The rdb refresh, `validate-rdb-fresh`, `copyResourceDatabase` and the playtest release rdb check cover a list of addons: the core always, another addon once it declares a resource, so an empty pack changes nothing. Offline selftest in `validate-rdb-fresh` and `validate-addonsdir`; build and pack of both addons proven headless.
- ✅ (2026-11-12, #323 batch 0b) Annex twins for the jobs, chains, vehicles and organization fixtures. `Annex.layer` gains `Annex_Job_Depot/Drop/TaxiPad/FuelCan`, `Annex_Org_Desk`, `Annex_ATM`, `Annex_Furnace`, `Annex_Vehicle_Lot` with `Annex_Garage` and `Annex_Impound_Desk` (pad ids `annex-lot`, `annex-garage`, `annex-impound`), `Annex_Chains_Barrel_1..3` (site `Annex_Chains_Site`) and `Annex_AppleTrader` with its tables and containers plus `Annex_AppleGatherTree`, in the fixture yard at x 70..104, z 66..100. A mesh-less twin is placed by putting a crate with `tools\cli place`, copying its ground coords and removing the crate. 13 test files stopped quoting village names; `validate-annex-fixtures` baseline 24 files / 40 pairs to 11 / 18. Tests only. `LF_Test_Roadmap_Fuel` keeps its row: `LF_RobAction.GetActionNameScript` only answers for the entity named `Roadmap_Fuel_Till`.
- ✅ (2026-11-12, #323 batch 0c) Annex twins for the medic, dispatch, carry, sign and ATM fixtures, which retires the last `validate-annex-fixtures` baseline rows (batch 0 total: 45 test files / 73 pairs to none; the baseline is `{}` and the check only fails on a new village name). `Annex.layer` gains `Annex_Hospital_Bed_1/Ambulance/Duty_Desk`, `Annex_Dispatch_Console/PoliceBoard/MedicBoard`, `Annex_Test_Till`, `Annex_Fuel_Till`, and the four zone notices `Annex_Sign_SafeZone_Spawn/East` and `Annex_Sign_Lawless_Entry/Pit`, placed with `tools\cli place` at x 56..70, z 36..62. The safe-zone and quarry tests now check that a notice carries the `safe_zone` / `lawless_zone` sign id instead of that a town sign exists. `LF_RobAction` shows the fuel till action name from the new `m_bTillActionName` attribute (set on `Roadmap_Fuel_Till` and its twin) in place of testing the entity name; behaviour in the town is unchanged. Tests, one attribute, no save change.
- ✅ (2026-11-12, #323 batch 0d) Core scripts stop naming village entities. The convoy start and drop are found through `LF_ConvoyMarkerComponent` (`m_bDrop`, new `m_bNearDrop`), the playtest test-car lot is the `LF_VehicleLotComponent` with `m_bTestCarLot`, `LF_Jail.SyncPlacedCell` re-applies the placed `LF_JailCellComponent` (its `m_sDoorTargetId` now defaults empty and the village cell sets it), the cash van route names its stops and van on the placed box instead of in the prefab and script, and the agent harness finds the stash (`LF_PersistentStorageComponent.GetPlaced`) and the nearest garage by component. The hospital till and cabinet and the convoy markers moved onto annex twins (`Annex_Hospital_Till/Cabinet`, `Annex_Convoy_*`), and the safe and quarry zone tests read the placed zone back or use fixture zones instead of town coordinates. `validate-annex-fixtures` now reads entities inside a `$grp` or children (`Shops` counts as a core layer), and the new `validate-core-village-names` fails when a core script quotes a village entity name (baseline `{}`). No save change; the village entities keep working through the new attributes.
