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
- ✅ **Live map states** (2026-11-16, #2208, D14): `LF_MissionHeader.m_aLF_LiveStates` lists the
  live map states (plain numbers; the pack says what each means) and an `LF_PlaceComponent` has
  `m_iLiveState` (default -1, always live). A place whose state is not listed does not register in
  `LF_Places`, so its systems stay dark and players see vanilla terrain, and it logs one
  `[LFDebug:Places] dark: ...` info line. An empty list, or no `LF_MissionHeader`, makes every
  state live, so the playtest town and the test worlds are unchanged. Logic in
  `Core/LF_PlaceLiveState.c`. Test `places/live-state`.
- 🧪 **Everon 1989 declares its live states; 2a is dark** (2026-11-18, #2694, W5 of #2644): the pack header
  `LF_Everon1989.conf` now lists `m_aLF_LiveStates { 0 }` (the square). State 2a (Saint-Philippe dock and districts, the
  road to Meaux, Meaux) is its own id, 21 (2b to 2d are 22 to 24), and is not listed, so a place tagged 21 does not
  register in `LF_Places` until the cutover adds 21 to the list. Test `everon/live-state-2a` (pack leg).

### `LF_GameModeRoleplay` — `Scripts/Game/Core/LF_GameModeRoleplay.c`
- ✅ Lazy-creates `LF_CharacterCreationManager`, `LF_ATMManager`,
  `LF_JobManager` on first player connect and drives character creation.
- ✅ **World-end static reset** (2026-11-08): the game-mode
  destructor calls `LF_StaticReset.ResetAll`, which runs every class's own
  `LF_StaticResetHook` subclass, found by reflection into a flat array sorted by owner
  (2026-11-17, #2755: no static initializer, which ate the size-capped static-init function; 2026-11-16: no central list; #2034 retired the `modded class LF_StaticResetHooks` chain, whose
  nested `super.Run` overflowed the script stack at about 123 layers; `LF_StaticReset.Registered()`
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
  PURCHASE, TRAVEL, COMMS, SPAWN, INVENTORY, CRIME, CHESS; chess 6 per 5 s, its own bucket, #2405): a fixed window admits a small burst
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
  Damage held back inside a sample window is published when the window ends (a 1 s tick) and
  before the victim's death or downing record, so the end of a short fight is logged (#2414; test
  `incident/damage-flush`).
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
- 🔧 **Ban record** (#1285, test `moderation/ban-record`). Every `#ban create` and `#ban remove` (the unban) the backend confirms (held at PENDING, written from `BanCommand.OnUpdate` only on OK), from chat or RCON, appends a `V1|BAN or UNBAN|account uid|unix|staff uid|duration|incident ids|reason` row to `$profile:LifeFramework/ban_record.txt`; the incident ids are the newest 10 `LF_IncidentLog` numbers (as either party) the log still keeps under any record key of the account: its uid and each character's key, since incidents are stored by character (#2414; test `moderation/ban-record-characters`). `#banrecord <uid or player id>` (administrators, chat or RCON) lists one account's rows. Legacy and malformed lines are skipped with a warning. Enforcement stays vanilla; the file is append-only and outside the save (no persistence version).
- 🔧 **Admin money grant** (`Feature/Moderation/LF_AdminMoney.c`, test `admin/money-grant`). `#lfmoney <player id or name> <amount> [cash|bank]` gives (or, negative, takes) money. A vanilla server command like `#kick`: the engine runs it for an administrator in chat (after `#login <passwordAdmin>`, or an id in `game.admins`) and for RCON with the admin permission; the handler checks the caller's ADMINISTRATOR role again (`AdminChatChannel.IsAdminPlayer`) and a non-admin gets a refusal line. The amount is a whole number, not 0, at most 1,000,000 either way (a bank deposit also stops at a 2,000,000,000 balance). Cash goes through `LF_MoneyUtils.AddCash`/`RemoveCash` (cash that does not fit lands at the target's feet; the default account) and bank through `LF_BankAccount.Deposit`/`Charge` (a take never pushes the bank into debt); both record ledger reason `admin-grant` and save as any balance change. The target must be connected: a player id, or a name matched ignoring case (two players with the name are refused: use the id). Each grant logs `[LFDebug:Admin]` and publishes `LF_OnAdminAction` (the incident log and the staff relay). It is a plain server answer in the chat or RCON reply, not a localized string. The RCON route is the same command object (`RequiredRCONPermission` = admin, as `#kick`); it is written, not yet run against a live RCON.
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
- ✅ `/report` in chat flags the reporter's incidents (either party) from the last `m_iReportWindowMinutes` (10) in `LF_Incident.conf`; online admins get a toast; a per-player cooldown `m_iReportCooldownSeconds` (120) refuses spam, and a report with nothing to flag starts it too (#2414; test `incident/report-spam`). Flagged incidents head the Game Master list (sent even outside the camera radius, marker "!!") and persist (`incidentflags` payload, save version 17) until an admin types `!handled <number>` in admin chat, which is logged `[LFDebug:IncidentReport]`. Tests `incident/report`, `persist/incident-flags-roundtrip`.
- ⚠️ The payphone staff line waits for comms routing.
- ✅ (2026-11-15, #1284) Staff relay, `Feature/Moderation/LF_StaffRelay.c`: `LF_OnAdminAction` and the new `LF_OnIncidentFlagged` (published by `LF_IncidentFlags.Report` after the rows exist) each send one short line through `LF_StaffWebhook.Send`. Throttled per kind: events collect for 5 s from the first, then one line goes out (its own line for one event, a count for more), so ten flags in a window are one message. Tests `incident/flagged-event`, `moderation/staff-relay` (captured sender). `LF_StaffRelay.Install()` runs from a `modded class LF_GameModeRoleplay.OnGameStart` in the same file (#1284 follow-up): server only, a no-op on a client, safe to call twice, and `LF_StaticReset.ResetAll` calls `Uninstall()` at world end (subscribers and tick). Test `moderation/staff-relay-install`. (2026-11-17, #2414) The webhook keeps each POST's `RestCallback` in a bounded list until its answer arrives (a local one could be freed before success or error fired), cuts a long line on a UTF-8 character boundary and escapes every control character as JSON requires; test `moderation/webhook-escape`.

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
  (`LF_CharacterConfig.m_iMaxPerAccount`, 1 in the core, 3 in the Everon pack) sets how many characters an
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
- ✅ **Changing character at the clerk (merged, in-dev).** The Everon pack allows 3 characters per account
  (pack overlay `LF_Characters_Everon1989.conf`; `LF_CharacterConfig.ApplyLayers` reads its fields over the core file,
  which still ships 1). The town clerk's "About my papers" (condition `PapersOffered`) holds the lost-card reply and
  "I need another of my files" (`CharacterSwitchOffered` / `CharacterSwitch`). The clerk checks
  `LF_CharacterSwitch.Refuse` and grants a 30 s token; after the talk the client asks for the sheet
  (`RpcAsk_LF_CharacterSwitch`, refused without the token) and `LF_CharacterSelectSheet` opens in switch mode: the
  character being played marked "On file now", the others, "register a new resident" while a slot is free, and "Stay
  as you are" (MenuBack too, `RpcAsk_LF_CancelCharacterSwitch`). The body stays in the world until the pick. The pick
  counts only within 3 minutes of the sheet and 10 m of where it was sent (`LF_CharacterSwitch.OfferHolds`, test
  `character/switch-offer`), so a client that skips the cancel cannot pick later elsewhere. The pick
  (`ServerSwitchPick`) checks again, then `StoreAndRemoveBody` stores the character the way a disconnect does
  (playtime, phone call, carried snapshot, unpaid store tags, last position, rejoin mark, dialogue and town-board
  state), takes and deletes the body, reopens the arrival flow and spawns the pick or opens form R-89. Refused, logged
  `[LFDebug:CharacterSwitch]` and toasted with the reason (`#LF-CharSwitch_*`) while dead, jailed, cuffed or escorted,
  downed, wanted, on police or medic duty, within `m_iSwitchCombatSeconds` (120) of player-on-player damage, carrying
  contraband or hauling a convoy, on a delivery job run, in a race or in a vehicle. Debt stays on its character. The
  police whitelist and roster stay per account (an approved officer may clock in as any of their characters); a switch
  is refused on duty, so only the character who clocked in carries the duty kit. A client forgets the names its
  character learned when its body is taken away (`LF_KnownNames.ForgetAll`), and a name learned for a player id is not
  shown over another character of that player. Tests `character/switch-rules`, `character/isolation`,
  `everon/characters-per-account`.
- ✅ **Arrival-only flow (#2413, merged, in-dev).** The server keeps a set of players whose arrival
  flow is open (`m_FlowOpen`: filled on connect, cleared in `SpawnPlayer`, forgotten on disconnect).
  `ContinueAfterQuiz` (the quiz's `RpcAsk_LF_QuizContinue`), `ServerCreateCharacter` (form R-89) and
  `ContinueFlow` itself refuse anyone not in it (`LF_CharacterCreationRules.RefuseFlowRequest`): a
  dead, downed, cuffed, jailed or combat-logout-held player, all of whom are already past arrival,
  can no longer restart the flow to reach the character pick or an instant respawn. A refusal toasts
  `#LF-CharSel_Refused` and logs `[LFDebug:CharacterCreation] ... refused: not arriving`. Test
  `character/flow-gate` (LOGIC).

- 🧪 **Barber's chair (#3234, merged, in-dev).** `Prefabs/Props/Roleplay/LF_BarberChair.et` is a seat
  (`LF_SitAction`) with `LF_BarberAction` ("Ask for a haircut", shown only to the player sitting on that
  chair; `m_iFee`, 40 shipped). It opens `LF_BarberSheet` (UI kit): previous and next look try each look on
  the seated body as a local preview, pay sends `RpcAsk_LF_BarberCut` on the player's controller, and leaving
  unpaid puts the old head back. The server re-checks the seat (`IsLoiteringOnEntity` on a chair with the
  barber action), the look and the cash, takes the fee (`LF_MoneyUtils.RemoveCash`, reason `barber`, nothing
  charged on a refusal), and sets the head with `VisualIdentity.SetHead` + `CommitChanges`, the base game's
  camo-head path, which replicates to every client and late joiner. The engine bakes hair and beard into each
  head mesh (no hair, beard or hair-colour part exists), so a look is one of ten vanilla civilian heads
  (`LF_Barber.Looks`) and changes the face with the hair. The chosen head is kept per character record key in
  the `barber` save section (`LF_BarberSavePayload`) and put back on every spawn (`LF_BarberSpawnSubscriber`),
  so it survives death, respawn and restart. A listen host puts its preview back before paying, so its server does not refuse the look as already worn. `LF_WantedLook.Capture` records the head (`|h=HEADGUID`), so wanted posters and photo IDs show the paid look. Tests `barber/changes-hair`, `barber/persist`, `barber/host-and-portrait`. Placing the chair
  in the Saint-Philippe barber (b43) is the shop card.

- 🧪 **The Saint-Philippe barber slot (#3236, in-dev).** Town house b43 (`House_Town_E_2I02_yellow`, on the square row
  beside the clinic) holds the `barber` slot: `LF_PlaceComponent` kind `BarberChair` (`saint-philippe.barber`; the map
  POI comes from the template, #3518) plus the kit `tools/town/kits/barber.json`. The salon is the empty south end of the
  tiled carriage hall (the flat's furnished rooms stay behind their doors): one wide wall mirror
  (`MirrorBathroom_02`) on the east wall, two `LF_BarberChair` seats 1.4 m in front of it, a basin on the same wall, a
  `CounterShop_01` against the west wall, and a barber-red `LF_Sign_Fascia` over the carriage doors reading COIFFEUR
  (pack sign `barber`, `LF-Sign_Barber_*` strings). Floor rows carry a `lift` of -0.045 m (counter -0.055): the hall's
  walkable floor lies below its model's top surface, so `look contact` reads 0 cm. The fascia stands 0.1 m off the
  collider wall because the plaster is proud of it (text hidden behind it otherwise). `seat.mjs` now takes the root
  transform off the host's furniture so a row standing in the flat's sofa is flagged. Layer
  `Place_saint-philippe_barber.layer`, claim `saint-philippe-barber.json`.

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

### `LF_Utils` / `LF_FormatUtils` / `LF_Component` — `Core/`
- ✅ `MaxInt`/`MinInt`/`ClampInt`, `AbbreviateNumber` (999→raw, 1000→`1.0K`,
  1e6→`M`, 1e9→`B`), typed component finders.
- ✅ (2026-11-18, #2787) Enforce `Math.Max`/`Math.Min`/`Math.Clamp` work in floats, so an int above 2^24
  (unix seconds, money, XP, counters) loses its low bits: 1800000480 came back as 1800000512. Every int time,
  money, XP and running-counter site uses `LF_Utils.MaxInt`/`MinInt`/`ClampInt`; `Math.ClampInt` (native int)
  is also exact. `validate-int-time-math` fails a time-named, non-float argument to `Math.Max/Min/Clamp`
  (shrink-only `tools/validation/int-time-math-baseline.json`; `// lf-float: <why>` marks real float time math).
  Test `core/int-time-math`.
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
  `DropItem` tolerates an
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
- 🧪 (2026-11-14, #689) Persistence switches: `persistence.keepCash`, `keepCarriedGear`, `keepSurvival`, `keepWanted`, `keepJail`, `keepLicences` in `config.json` (all on by default) choose what survives a restart. The save still holds every field (no version bump, old saves load); `LF_PersistenceKeep.ApplyAfterLoad` resets each switched-off field after the last payload is read, idempotently, and logs `[LFDebug:Persistence] keep<Field> off: N ...`. Cash off strips cash stacks from carried snapshots and the duty-desk slots (#2547; house lockers and the bank keep theirs); wanted off clears every character and its posters; jail off frees every prisoner, #691 logout sentences included. Owner page `docs/owner/persistence-switches.md`, detail `docs/persistence.md`. Tests `persistence/switch-config`, `persistence/switches` (and the `wt e2e` restart with keepLicences off).
- 🧪 (2026-11-17, #2547) Persistence switches reach the duty desk: with `keepCarriedGear` off the civilian stash (`civilian_stash`) is dropped with the carried snapshot, so an officer saved on duty gets the civilian kit at clock-out, not pre-wipe gear; with `keepCash` off cash is also stripped from `civilian_stash`, `police_duty` and `medic_duty`. Duty kits keep their gear (armory book). Test: `persistence/keep-stash`.
- 🧪 (2026-11-17, #2621) A gear wipe hands the armory book back its outstanding kits: with `keepCarriedGear` off, `LF_PersistenceKeep.ApplyAfterLoad` calls `LF_ArmoryKits.ClearAllOutstanding`, so an officer whose issued kit the wipe took is not refused a new one (a kit kept in the `police_duty` locker stays on the book); logs `[LFDebug:Persistence] keepCarriedGear off: N outstanding armory serial(s) cleared`. The 2 h issue cooldown stays as saved. No save change. Test: `persistence/wipe-armory-book`.
- 🧪 (2026-11-14, #964) Prefab slots (`Core/Config/LF_PrefabSlots.c`): a system asks `LF_PrefabSlots.Get("<slot>")` for the prefab it spawns or hands out instead of naming one. `Configs/PrefabSlots/LF_PrefabSlots.conf` maps each slot id to its default prefab and the component classes the system needs; a pack overlay patches `m_Prefab` by slot id through the override model, and the required components stay the core file's. A pack prefab that does not load or lacks a required component is rejected at first use with the reason and the slot keeps the core default; a slot with no usable prefab logs `LF_Debug.Error` and returns empty, so only that feature stops. Core and packs only (no `config.json` layer), so client and server resolve the same prefab; a pack change needs a restart. Slots today: `cash`, `money_bag`, `id_card`, `handcuffs_worn`, `evidence_marker`, and (#1128) `hospital_bill`, `fine_ticket`, `jail_slip`, `store_receipt`, `evidence_casing`, `evidence_bag`, `evidence_footprint`, `evidence_statement`, `phone_handset`, `notice_board_face`, `sign_face`, `courier_parcel`, and (#4223) `gang_radio` (the pack sets `LF_GangRadio`). Prefabs that a shop price, resource chain, fence NPC or other prefab names by path (drill, keycard, fence charge, lockpick, car parts, contraband crate, scanner, service radio, vehicle key, iron nugget and bar) stay in script until their other references move too. `validate-prefab-slots` checks every slot GUID resolves and every script slot id is a core slot. Test `prefabs/slots`. `LF_PrefabSlotsConfig` and `LF_SoundSlotsConfig` are on the pack public surface (#1005), so `pack check` accepts a pack slot overlay; test `prefabs/slot-swap-spawn` swaps `evidence_marker` to another prefab, spawns it, and spawns the core handset after a bad `phone_handset` swap. Cash saved before a pack swapped the `cash` slot still counts (#1130): the core cash and any prefab the save alias table leads to the slot prefab are counted and spent first, new cash is the slot prefab, the save format is unchanged. Tests `prefabs/cash-slot-swap`, `e2e/cash-slot-swap` (`tools\cli e2e --cash-swap`).
- 🧪 (2026-11-14, #323) Sound slots (`Core/Config/LF_SoundSlots.c`), the sound twin of prefab slots: a prop asks for its sound by slot id (`m_sSoundSlot` on `LF_AmbientEmitterComponent`, or `LF_SoundSlots.Get("<slot>")` in script) instead of naming a sound project, so a pack swaps the sound by overriding the slot. `Configs/SoundSlots/LF_SoundSlots.conf` maps each slot id to its default project; a pack overlay of the same class patches `m_Project` by id through the override model. A pack project that does not load is rejected at first use with the reason and the slot keeps the core default; a slot with no usable project logs `LF_Debug.Error` once and the prop stays silent, nothing else stops. An `m_sSoundProject` set on an instance still wins over the slot (for a pack's own placement). Core and packs only, so client and server agree; a pack change needs a restart. Slots today are the sixteen atmosphere sounds: `atmo_store_radio`, `atmo_pub_radio`, `atmo_furnace`, `atmo_sawmill`, `atmo_quarry`, `atmo_farm`, `atmo_still`, `atmo_police_desk`, `atmo_chop_shop`, `atmo_bank_murmur`, `atmo_bank_till`, `atmo_hospital_equipment`, `atmo_hospital_ward`, `atmo_payphone_line`, `atmo_fuel_pump`, `atmo_van_idle`; every default is the sound the prop played before, so nothing sounds different. 12 core prefabs and 3 DebugWorld layers (`ATM`, `MiningChain`, `Street_CashVan`) now name a slot. `validate-sound-slots` checks every slot GUID resolves, every slot id asked for is a core slot, and no core prefab, layer, script or config (the slots file and core tests aside) names a `Sounds/Atmosphere/` sound; `atmosphere-selftest` resolves slots when it checks emitter radii. Test `sounds/slots`. The Everon pack's own layers still name their sounds directly (they are the pack's).
- 🧪 (2026-11-14, #323 batch 4) The atmosphere sounds moved into the pack: `Sounds/Atmosphere/` (seventeen projects and their loops) is now `addons/LF_Everon1989/Sounds/Atmosphere/`, GUIDs unchanged. The sixteen core `atmo_*` slots keep their ids and have no default; the pack overlay `Configs/SoundSlots/LF_SoundSlots_Everon1989.conf` (listed in the `LF_Everon1989` and `LF_PlaytestTown` headers) fills each with the project it had. A slot with no core sound that no pack fills plays nothing and logs one `[LFDebug:SoundSlots]` Info line, not an error, so a core-only server is silent there; a pack-filled slot logs `slot '<id>' -> <project> (pack)`. `validate-sound-slots` now fails a core default that names a `Sounds/Atmosphere/` sound; `atmosphere-selftest` reads the projects from the pack and the slots from core plus the pack overlay; `tools\cli asset ambience` writes to the pack (`ambience.json` `addon`). Test `sounds/slots` runs on base game sounds, so it passes core-only.
- 🧪 (2026-11-14, #962) One override model for list sections (`LF_ConfigOverlay.c`): each record has an id and every layer (packs in load order, then the owner file, then the stream) adds, replaces, patches or removes it by id (`"op"` in JSON, `m_sOp` in a pack `.conf`); no verb patches an existing id and adds a new one, as before. A verb naming an id no lower layer has, an add of an id that exists, or an unknown verb is refused with the source and id while the rest applies; two packs on one id warn naming both and the later wins. On it: `shops.priceRules`, `catalogue.items`/`offers` (the catalogue's own merge is gone; a client replaces its lists with the streamed ones) and `crime.crimes` (which now reads pack overlays). The schema gains `op` on those records. Docs `docs/config/override-model.md`. Test `config/overlay-ids`.
- 🧪 (2026-11-14, #1109, #957 part) Vehicle prices and dialogue patches on the override model (#962). `vehicles` is an `LF_ConfigList` (`LF_VehiclePriceList`): the id is the lot prefab's GUID (a key naming the locked child's GUID addresses the same entry), the owner file takes `"op"`, a pack `.conf` of `LF_VehiclePricesConfig` now layers (percents and entries by `m_sOp`), and a new or replaced vehicle needs a base price above 0; the hand-written upsert in `ApplyOwner` is gone. Dialogue nodes and lines are `LF_ConfigRecord`s: a pack `.conf` of `LF_DialogueConfig` listed as an overlay of the NPC's core file adds, replaces, patches or removes them by id, sets the start node, and a node patch replaces its reply list (a reply has no id). `LF_DialogueConfig.Layered` (used by `LF_DialogueComponent`) drops a patch that leaves the conversation broken (unknown node, unreachable reply, hook with no class) and plays the core one. #957 keeps: dialogue by id instead of prefab path, `asset dialogue --addon`, text-only lines, the neutral core clerk with the Everon one as a pack patch, and the `validate` rule for patches. Tests `config/vehicle-override-verbs`, `dialogue/override-verbs`, `config/pack-patch-overlays`; `reload/dialogue-file` still green. Docs `docs/config/override-model.md`.
- 🧪 (2026-11-14, #957) Dialogue by id, text-only lines, pack patch tool and the Everon clerk as a pack patch. `LF_DialogueComponent.m_sDialogueId` (`clerk`) is resolved by `LF_DialogueRegistry` from the core index `Configs/Dialogue/LF_DialogueIndex.conf`, which a pack overlay of `LF_DialogueIndexConfig` adds to, replaces or removes by id; the clerk and fuel clerk prefabs name ids, a path in `m_sDialogue` still works. A line with no event and no length is text only (caption for reading time, no sound); an event without a length is a half-made recording and drops the patch. `tools\cli asset dialogue <npc> --addon <pack>` with `"patches": "<core npc>"` writes a text-only patch `.conf` and strings with no ElevenLabs call; `validate-dialogue-patch.mjs` reads every pack dialogue patch against its core. The 1989 pack ships `LF_Dialogue_ClerkEveron.conf` (listed in its mission header): the clerk's greet moves to a pack string key with the same words, so nothing changes on screen. LOGIC `dialogue/pack-overlay`; docs `docs/config/override-model.md`, `docs/asset-pipeline/dialogue.md`. The clerk's voice split (#323, 2026-11-14) is below.
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
  `JsonLoadContext` probe results: `Core/Config/Feature.md`. Tests
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
  `config/migration-file`. Stamping `schemaVersion` joins pieces under 8 KB, so a config past the
  8191-char Enforce string cap keeps its tail (#2671, test `config/stamp-long`).
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
- 🧪 (2026-11-16, #1803) `Worlds/GridWorld/GridWorld.ent` is the second of the three worlds (`docs/architecture/three-worlds.md`): the flat debug terrain (its own copy, `GridWorld/GridTerrain/`, because the DebugWorld terrain does not load from another world), sky and light, the debug game mode, one spawn `Spawn_Grid_Gate` just inside the annex north gate, the test runner, and every annex entity copied byte for byte from DebugWorld by `node tools/grid/split-annex.mjs --copy` (Annex, HousesKeys, Jobs, NPCs, PRFeatures, Police, Tests, WhitelistChat whole; 2 of the 9 `Crimes` entities). No town spawns, safe or lawless zones. Mission header `Missions/LF_GridWorld.conf`. DebugWorld is unchanged and nothing boots GridWorld by default yet: `tools\cli test --world Worlds/GridWorld/GridWorld.ent` does. `tools/lib/worlds.mjs` names the three worlds. Test `world/grid-boot`; `split-annex-selftest`.

- 🧪 (2026-11-16, #1808, W7) Grid cell `test-fixtures` (`Grid_test_fixtures.layer`, c1r3) gives the tests that read the town their own fixtures: one placed `EL_JailCell` (door target `Annex_Jail_Door`) and `farm_plot_1..3` (`FarmPlot_Cabbage` with `LF_FarmPlotComponent` keys). 11 pins leave `tools/test/world-pins.json` (the jail cluster: `disconnect/cuffed`, `disconnect/jail`, `jail/online-time`, `jail/placed-cell`, `jail/slip`, `jail/work`, `robustness/single-jail-tick`; the plots: `farm/plot-loop`, `persist/farm-plots-roundtrip`, `systems/keep-data`, `systems/world-gates`); they run in the GridWorld boot. 41 pins stay with a note in the file: shops and tills, zones, spawn points, signs, quest givers, hideouts, the car lot and impound desk, the roadmap scenarios.
- 🧪 (2026-11-16, #1760, chapter #1009) The Everon pack carries the weapons, clothing and starting-kit catalogue overlays (`Configs/Catalogue/LF_Catalogue_{Weapons,Clothing,Kit}_Everon1989.conf`, `LF_ItemCatalogueConfig`) and the police ranks overlay (`Configs/Police/LF_Police_Everon1989.conf`, `LF_PoliceConfig`), all empty stubs listed last in the mission header `m_aLF_ConfigOverlays` so later cards fill one file each. `validate-everon-config` merges the core catalogue and police files with the header's overlays and runs the expectation fixtures in `tools/validation/fixtures/everon/*.json` (kinds `outlets` and `ranks`, README there); `everon-config-selftest` is its red proof.

### `LF_NamedPlaces` — `Scripts/Game/Core/LF_NamedPlaces.c` (#1943, chapter #1014)
- 🧪 (2026-11-16, #1943) `LF_NamedPlaces.Nearest(pos, maxRadius = 600)` returns the display name of the nearest map location (the editor comment entities every map places for its town, village and landmark labels; a pack adds a name by placing one), or empty when none is within the radius or the world has no editable-entity core. The name is the map's own, usually a `#` localization key for a widget's SetText. The core holds no town list. Test `world/named-place`. (2026-11-18) The scan is our own over `SCR_EditableEntityCore.GetAllEntities`, not the base `FindNearestEntity`: that one reads its private entity set unguarded, and the set stays null until a first label registers, so a world with no labels crashed `Nearest` with a VME (`LF_Test_ChainTrace` once tests ran name-sorted). Test `world/named-place-no-labels` (LOGIC, `LF_Test_ANamedPlacesNoLabels`, named to sort first). The 1989 pack places "Saint Philippe" (`Place_SaintPhilippe` in `Village_Dressing.layer`, key `LF-Place_SaintPhilippe`, town pass #1947).
- 🔧 (2026-11-16, #1771; merged, in-dev) **The 1989 Everon pack fills the starting kit.** `LF_Catalogue_Kit_Everon1989.conf` offers under `starting_kit`: a brown denim jacket (its shirt is part of the prefab; shirts and jackets share the one upper slot), dark-brown trousers, the dirty work boots, a flat cap and `cash_start` (the `MoneyStack` default of 100, carried in a pocket, the same cash the game mode's M88 jacket held). No M88 or M70 pieces, Soviet panama, ALICE pack, police kit or weapon. Fixture `tools/validation/fixtures/everon/starting-kit.json` (kind `outlets`, `present:false` for the military pieces) is read by `validate-everon-config`. Vanilla has no civilian shoe, so the boots are the pack's existing `boots_work`; a civilian shoe asset is a follow-up.
- 🔧 (2026-11-18, fix-newcomer-room; merged, in-dev) **A newcomer has room for the first-hour buys.** The kit above wears the upper slot a shirt needs, and its jacket and trouser pockets take items up to 10x10x10 (600 total) while a folded shirt is 15x15x15, so every clothing buy on a fresh character was refused `buy failed (inventory full)` and refunded. The kit now also wears `kit_canvas_sack`, a plain black canvas sack with leather straps (vanilla `Backpack_Veshmeshok_Black.et`, 20x20x20 items, 6000 total: what someone moving to the island carries their things in; the Swiss M70 rucksack was looked at and left out for its Alpenflage camo), category `clothing` so it goes to the back slot, price 0 and no shop outlet, so it cannot be sold back for cash. The shop's give path is unchanged: a buy that truly does not fit is still refused and refunded. Test `everon/newcomer-room` (pack leg) gives a bare body the real civilian kit and lands a shirt and a water bottle through `LF_InventoryUtils.AddAmount`.

### `LF_FittedBuildings` — `Scripts/Game/Core/LF_FittedBuildings.c`, `LF_FittedBuildingComponent.c` (D15)
- 🔧 (2026-11-17; merged, in-dev) **Fitted vanilla buildings are indestructible.** `town fit` puts
  `LF_FittedBuildingComponent` on the slot's place marker (beside `LF_PlaceComponent`, inside the
  building) and records the claim's `destruction.mode: "hooked"`; a slot with no marker stays
  `needs-hook`. On the server, one frame after load, the component runs one sphere query (radius
  `m_fSearchRadius`, default 25 m) for entities with `SCR_DestructibleBuildingComponent`, takes the one
  whose bounds hold the marker (else the closest within 3 m) and calls
  `DamageManagerComponent.EnableDamageHandling(false)` on it and its child destructibles. Every
  vanilla hit (explosive, shell, vehicle contact) goes through `HandleDamage`, so the building never
  reaches DESTROYED, never sends the destroy RPC and has no JIP state to replicate. Logs
  `[LFDebug:FittedBuildings] <place id>: building ... is indestructible`; no building found is an
  `Error` and the building stays destructible. No coordinates in core; no per-frame cost. A game
  master `SetHealth(0)` still bypasses it (vanilla's own invulnerability rule). Test
  `world/fitted-building-indestructible` (DebugWorld spawns two garages; on CoreOnEveron it uses two
  real buildings near the map's middle).

- **World-end resets (#2415):** the dev Game Master, character-creation manager and offers, pick/registration/ID-request/ledger sheets, vehicle records and plates, garage and organization config, and the whitelist now register `LF_StaticResetHook`s. The dev Game Master hook takes its handler off `SCR_EditorManagerCore` and a grant re-checks the dev-tools gate, so a production world loaded after a dev world grants no EDIT. A disconnect forgets the player's pick/register offers. Tests `moderation/dev-gm-reset`, `character/creation-reset`, `vehicle/state-reset`, `whitelist/static-reset`, `organization/static-reset`, `ui/sheet-static-reset`.
- **Open sheets close at world end, HUD money handler leaves the old body (#2512):** the registration form and the character pick hide their open screen before the world-end reset drops the static, so the old world's sheet no longer stays drawn (and holding the interaction prompt) into the next session. `LF_BetaHud` keeps the inventory manager its `OnMoneyChange` handler is subscribed to and removes it in `OnStopDraw` and before subscribing again, so a respawn leaves one cash update per move. Test `character-creation/sheets-world-end-reset` (WORLD, through a seam; the HUD path is manual).

- **Host path for client requests (#2511):** a listen host's own `Rpc(RpcAsk_...)` never reaches its handler, so the debug menu's actions and the spawn-hub travel did nothing for the host. `LF_AskDebugAction` and `LF_AskSpawnHubTravel` now call their handler directly when `Replication.IsServer()` (the world reset request already did). `validate-rpc-host-path` is retired (#2715): in-game tests (#2641, `docs/architecture/rpc-routing.md`) show a bare `Rpc(RpcAsk_...)` runs its handler at once on the server, so the check enforced a redundant branch; its baseline and selftest are deleted, and the existing host branches come out in #2716–#2718. Before the retirement the baseline was empty: the organization ledger asks (#2550), `LF_AskPoliceArrest` and the quest tracker `AskRequestLog` (#2534) call their handler directly on a listen host.

- **RPC routing settled by tests (#2641):** the premise of the row above is wrong. On the server, a bare `Rpc()` with `RplRcver.Server` runs its handler at once inside the call; so does `Owner` on an entity the server owns. `Broadcast` never runs on the server, and an entity with no `RplComponent` drops every Rpc (`RPL (E): ... unregistred item`). The tests are `LF_Test_RpcRoutingOwned`, `LF_Test_RpcRoutingNotOwned` and `LF_Test_RpcRoutingNoRplId`, using the probe `LF_TestRpcProbeComponent` on `Prefabs/Debug/LF_TestRpcProbe*.et`. Findings, verdicts and the follow-up slices: `docs/architecture/rpc-routing.md`. The host branches stay until those slices remove them: they are redundant but harmless.
- 🧪 (2026-11-17, #2667, W2 of #1022) Organization war rules come from config: `LF_OrgWarConfig` loads `Configs/Organizations/LF_OrgWars.conf` (fee 5000 from the fund, answer window 600 s, war length 7200 s, pair cooldown 86400 s, 3 members a side, `warKillsAreMurder` off, `tagWarKills` on), layers each pack overlay of it, then the owner keys `organizations.wars.*`, and `LF_OrgWarConfig.Rules()` hands out a copy as `LF_OrgWarRules`. A bad owner value is refused with an error line and the value below it stays. The schema generator reads a dotted `SECTION` ("organizations.wars") as an object nested in its parent section. Test `organizations/war-config`.
- 🧪 (2026-11-18, #2669, W4 of #1022) Organization wars run on a clock and are announced: `LF_OrgWarSweep.Start()` (game mode, server) calls `LF_OrgWars.Tick(now)` every 30 s, so an unanswered declaration lapses and a war past `endsAt` ends without anyone touching the desk. A subscriber on `LF_OnOrganizationEmptied` ends that organization's open wars with reason `forfeit` (`ForfeitOf`; an accepted war ends and starts the cooldown, a waiting declaration lapses). `LF_OrgWarAnnounce` subscribes to the three war events: one `NotifyAll` toast per event (declared, accepted, ended, lapsed, forfeit; the two names ride as `Declarer / Target`) and a longer `NotifyPlayer` notice to each online member of both sides. Keys `LF-OrgWarAnnounce_*` are core-neutral; the pack words them. Test `org/war-sweep`.
- 🧪 (2026-11-18, #2769, from the #2668 review) Org war: the shipped officer rung holds `DECLARE_WAR` (founder and officer declare and answer; the member does not; an org with a saved rank mask keeps it, owners still edit `organizations.rankDefaults`). A LAPSED declaration starts a same-pair cooldown of `organizations.wars.lapseCooldownSeconds` (default 3600 s, 0 = none), shorter than the 86400 s war cooldown an ENDED war starts, and it holds both directions, so a fund cannot be drained by re-declaring on a loop. The lapse cooldown is in `Configs/Organizations/LF_OrgWars.conf` and the owner file. Tests `org/war-lapse-cooldown`, `organizations/war-config`.
- 🧪 (2026-11-18, #2670, W5 of #1022) War kills are tagged: `LF_OrgWarKills` (`Feature/Organizations/`) subscribes to `LF_OnPlayerKilled` (`Watch()`, called by `LF_Murder` before each publish) and, when `tagWarKills` is on and the killer and victim belong to two organizations at war, appends one incident of kind `war_kill` through `LF_IncidentLog.Append` (victim as subject, killer as other party, weapon as `how`, `murder` as `LF_Murder` decided; the record shape is unchanged). The ordinary `death` record stays. With `warKillsAreMurder` on the kill is murder and is still tagged. An owner who narrows `incidents.kinds` must list `war_kill`. Test `org/war-kill-tag`.
- 🧪 (2026-11-18, #2672, W6 of #1022) Organization wars survive a restart: payload `orgwars` (save version 54) keeps every declared or accepted war and every closed one still inside its pair cooldown, with its id, state and unix-second times, so a war fee paid before a restart no longer loses its war. A war whose organization is gone is dropped on load with a `[LFDebug:OrgWars]` warning; a deadline passed during downtime closes on the first sweep, at the deadline. A save from version 53 or older loads with no wars. Test `org/war-persist`; proved across a real restart with `wt e2e`.
- 🧪 (2026-11-18, #2677, W9 of #1022) A war notice board (`Feature/Organizations/LF_OrgWarBoard.c`, prefab `Prefabs/Organizations/LF_OrgWarBoard.et`): a board kind (`orgwars`, `LF_BoardProviders`, the wanted board's read path) that lists declared, running and ended wars as typed notices, declared first, then accepted, then ended, newest first inside each. Declared and accepted notices are read from the war table; `LF_OrgWarBoardSubscriber` listens to `LF_OnOrgWarEnded` and keeps an ended or lapsed notice for one hour (`ENDED_KEEP_SECONDS`, memory only, 16 at most). A read closes the wars the clock has passed (`LF_OrgWars.Tick`), then packs three texts and two numbers per notice; times cross as seconds, so the client needs no server clock. Strings `LF-OrgWarBoard_*`. The pack hangs the prefab beside the town board; the core places it only in a grid cell. Test: `org/war-board-text` (LOGIC). Manual: read the board at the grid cell with two orgs at war.
- 🧪 (2026-11-18, #2675, W7 of #1022) A war sheet at the registry desk: a second desk action, `LF_OrgWarDeskAction` ("Wars"), opens `LF_OrgWarSheet` (`Feature/Organizations/`), a sheet of UI kit parts with a scrolling body. It lists the organizations on the register with a Declare button (the fee and the member minimum are in the intro line, in words), the declarations made against the viewer's organization with Accept and the time left in the answer window, and the viewer's active wars with the time until they end. Each register row says whether the pair is open, has a declaration waiting, is at war, is on cooldown or is under the member minimum, and carries Declare only when it is open. A viewer with no organization, or a rank without `DECLARE_WAR`, or an organization under the minimum, sees the same rows with no buttons and an intro that says why. The server builds the page (`LF_OrgWarSheet.BuildPage`) and sends it in one RPC (`RpcAsk_LF_OrgWarSheet` / `RpcDo_LF_OrgWarSheet`, the war desk guards); the buttons send the W3 asks (`LF_AskOrgWarDeclare`, `LF_AskOrgWarAnswer`) then ask for a fresh page, and the server re-checks every one. Test `org/war-sheet` (the row model). A debug-grid cell `slice-2675` holds a desk with both actions; the town desk gets the action in the town card.
- 🧪 (2026-11-23, #4455, gangs A2) Prospect rank: a member who joins face to face (`LF_OrgInvites.Accept`, "Join <org>") is a prospect; a direct `LF_OrganizationManager.Invite` (fixtures, tools) makes a full member for 48 hours (`LF_OrgProspect`, `Feature/Organizations/LF_OrgProspect.c`). A prospect keeps every other right of the entry rung, but `LF_Organization.Allows` and `RightsOf` drop `WITHDRAW`, `SPEND_FUND` and `USE_STASH` (`LF_OrgRights.ForProspect`), so the fund withdrawal, the strongbox, the stash and the catalogue all refuse on the server with no call site changed; the refusal words are `#LF-OrgProspect_Refused` and the rank reads "Prospect" (`#LF-OrgProspect_Rank`, never a countdown). A leader's promote, or 48 hours from the join (unix seconds), ends it; a playtest direct join is a full member. The join time is in save section `org_prospects` (v1); a save from before it has no prospects, so old members keep full rights. Test `org/prospect` (PERSISTENCE; `wt e2e` keeps a prospect across the restart).
- 🧪 (2026-11-23, #4546) A prospect refused at the stash or the ledger withdrawal is told `#LF-OrgProspect_Refused` ("Prospects can't touch the gang fund or the stash yet."), not "not a member" or the clerk's generic refusal: `LF_Hideout.StashRefusalKey` (stash toast, `LF_HideoutStashPlayerController.RpcAsk_LF_OpenStash`) and `LF_OrganizationPlayer.WithdrawRefusalKey` (ledger notice, `RpcAsk_LF_OrganizationWithdraw`) ask `LF_OrgProspect.RefusalFor`. Strangers keep "not a member" and an unowned stash keeps "no owner". Test `org/stash-prospect-refusal` (WORLD).

### Save sections — `Persistence/LF_SavePayload.c` (#2841, 2026-11-18)
- 🧪 **Per-payload save versions** (#2841): a new save payload is an `LF_SavePayload` subclass in its feature folder (its own name and `Version()`, found by reflection), written as a named object after the 58 frozen positional payloads; the save writes format 1000 (`SECTIONED_FORMAT`). Two PRs adding payloads touch no shared line or number. Positional saves (v1-58) load as before with every section absent (`OnAbsent()`); an unknown or newer section is skipped and pauses autosave; one failed section does not stop the rest. Locks per section in `tools/validation/record-shapes/<name>.json`. Detail `docs/persistence.md` "Sections". Tests `persist/save-sections` (JSON and the engine's binary context), `e2e/save-sections` (v32, v55, v58 loads; the real restart in `wt e2e`); `tools/validation/save-sections-merge-selftest.mjs`.
- 🧪 (2026-11-18, #2620) **Listen-host check for broadcasts:** `validate-rpc-owner-host-path` flags a method that calls `Rpc(<handler>, ...)` where the handler is `RplRcver.Broadcast` and the method has neither a direct call of the handler nor a host guard (`RplSession.Mode()`, `System.IsConsoleApp()`, `m_bClient`); a broadcast never runs on the server, so the host's own player would see nothing. It is the reshaped #2620 (an owner-reply check was dropped by #2641: an owner Rpc on a server-owned entity already runs its handler, `docs/architecture/rpc-routing.md`). Four sites are baselined with reasons in `tools/validation/rpc-owner-host-path-baseline.json` (the config stream, three plant broadcasts: the host reads or draws those without the broadcast); a stale entry fails. `Tests/` files are skipped. Selftest: `rpc-owner-host-path-selftest.mjs`.
- 🧪 (2026-11-22, 0.2.0 playtest, merged in-dev) `LF_CharacterConfig` has a fourth overlay field, `m_iStartingBank` (-1 keeps the game mode's `LF_SpawnLogic.m_iStartingBank`; 0 or more replaces it, `LF_CharacterConfig.StartingBank`). The Everon pack sets 2000 (1250 on 2026-11-22, then raised the same day). See money-economy.md; test `LF_Test_EveronStartingBank`.
- 🧪 (2026-11-23) **Rebel repaints (#4471, WO-L7 of 0.3.0).** `tools\cli livery build` writes `LF_UAZ469_Rebel` and `LF_Ural4320_Rebel` into the pack (`Prefabs/Vehicles/Liveries/`, `Assets/Liveries/`): an olive body over the vanilla mud and dirt layers, no lettering. The UAZ469 carries a hand-painted number `14` on each front door (a pack plate, `LF-Sign_Livery_UAZ469_Rebel_Title`); the Ural4320 carries `27` on both cab doors (#4544: its doors are animated body parts, so the plates sit on the body beside the door and do not swing with it) and two vanilla camo nets (`CamoNet_PalletMRE`, `CamoNet_PalletAmmo_Soviet`, forest materials) draped over a load in the cargo bed. Both come from a hand-written kit prefab, `LF_Ural4320_RebelKit` (pack, `Prefabs/Vehicles/`), that the generated `LF_Ural4320_Rebel` inherits; `rebel-kit-selftest` keeps the chain whole. The Ural4320 joined the generator as `tools/livery/vehicles/Ural4320.mjs` (WO-L6 reuses it). The cabs keep the vanilla blue interior. Credit rows are in `docs/credits.md` "Liveries". No spawn point or shop uses the variants yet.
- 🧪 (2026-11-23) **Company repaints (#4472, WO-L6 of 0.3.0).** `tools/livery/rows/company.mjs` adds five variants to `tools\cli livery build`, in the pack: `LF_S105_Taxi` (cab yellow; TAXI on both rear quarter panels and a back-to-back roof sign), `LF_S1203_CompanyBakery` (cream, EVERON BAKERY), `LF_S1203_CompanyFactory` (factory blue, EVERON WORKS), `LF_UAZ452_CompanyHardware` (red-orange, EVERON HARDWARE, on the cargo-van body) and `LF_Ural4320_CompanyTimber` (forest green, EVERON TIMBER on both cab doors). The names are invented, US English capitals, plates written as fixed-panel children by the build (`panels`, #4559). Each plate's sign row (`livery_company_*`, `Configs/Signs/LF_Signs_Everon1989/LF_Signs_Livery_*.conf`) and string (`LF-Sign_Livery_*_Title`) is hand-written like the ambulance's. Credit rows are in `docs/credits.md` "Liveries". No spawn point or shop uses the variants yet; the UAZ452 van keeps a vanilla red star on its grille, and lettering size on the road is still to be judged in a full-profile session.
- ✅ (2026-11-27) A prefab with user actions needs an `RplComponent`, or the actions work for a host and do nothing for a client on a dedicated server (the engine sends a performed action to the server over the entity's replication; with no `RplComponent` it is dropped, `docs/architecture/rpc-routing.md`). `validate-actions-have-rpl.mjs` fails a prefab whose addon chain has an `ActionsManagerComponent` with actions and no `RplComponent`, unless every action class overrides `HasLocalEffectOnlyScript()` (a local mesh that asks the server some other way). It found the three jail work props (`LF_JailWorkAction`) and the hospital bed (`LF_BedDeliveryAction`), which both refuse to run on a client and so never ran; each now carries an `RplComponent`. The tree-fell helper entity is local by design and now says so. Selftest `actions-have-rpl-selftest.mjs`. The `LF_PoliceCarPoolBoard` no longer carries a `RigidBody` on its collision-less notice board model (an engine error per spawn).
- ✅ (2026-11-27) A player-facing amount is formatted by `LF_MoneyFormat.FormatMoney`, never by hand: `validate-money-format` fails a production script line that writes `"$" + amount` or `string.Format("$%1", ...)` (log lines exempt, `// lf-money-format-ok: <why>` for a real exception). A string row may write `{currency:sym}%1`: `LF_Terms.Text` groups that slot. Test `money/format-edges`.
