# Jobs / Level / Licenses / Whitelist

### `LF_PlayerJobComponent` — `Feature/Jobs/LF_PlayerJobComponent.c`
- ✅ Per-player job state, per-job level/XP maps, license gates (POLICE→POLICE_ACCESS,
  MEDIC→MEDIC_ACCESS), client→server `RpcAsk_SetJob` re-validates licenses.
- ✅ **Fruit-catcher reward removed** (#2408, 2026-11-17): the minigame never landed and nothing in a world
  makes a player FARMER, so `AskStartFruitCatcherRound`, `AskClaimFruitCatcherReward`, their RPCs,
  `LF_FruitCatcherRound` and `EHarvestJobType` are deleted. A fruit reward comes back with its own
  location gate. The `fruit_pick` XP award stays in the jobs config. Pin test
  `security/job-workplace-and-seed` still covers the workplace gate.
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
  (verified 2026-10-11). Run rules below (#2408). The bank balance stays 0. Parcel cargo is
  `{9CA41609F6C806AE}Prefabs/Items/Roleplay/LF_Parcel.et`, a cardboard box carried
  in both hands (#155: the bare `HandCarryItem_Base.et` had no mesh, so the
  hands were empty on a client). The mesh is `PostParcel.xob`, 0.26 x 0.14 x 0.38 m (a shoebox; every vanilla
  cardboard box is about 0.4 x 0.43 x 0.67 m, and a script spawn overrides a prefab `scale`, so none can be
  shrunk). Its body sits on the `PropFireView` layer, the layer of a vanilla box prop (the old
  `ItemFireView` body let a walking player through it); test `jobs/parcel-collision` compares the
  parcel's layer with a spawned `BoxWooden_01` and its size. Pickup and drop use the paper sound set
  (`Items_PickUp_Map.acp`) and the hold is the wrench graph, not the jerrycan graph, so no canister
  sounds. (#1846) The parcel is a core hand tool, not a hand-carry gadget: `LF_Parcel.et` inherits `RoleplayItem_Base` and carries `LF_HandToolComponent` (tool id `parcel`, `m_bEquipOnPickup`), so it takes the whole hand like the vault drill, sits in a 1-0 quick slot (the weapon bar) and, with `m_bEquipOnPickup`, is taken into the hands once when it first enters the carrier's inventory (the courier job gives it; also after a restart load). It has an item volume (3000) so it must fit the carrier's storage; `LF_CivilianJob.GiveCargo` fails and the run does not start if it does not. Drawing a weapon puts it away (the vanilla hand swap) instead of dropping it; the player takes it back with its slot key. Test `jobs/parcel-hand-tool` (the job gives a hand tool with id `parcel` that goes into a quick slot and the hands). The mesh's own collider (`UBX_PostParcel`, `ItemFireView` in `PostParcel.xob.meta`) still
  needs a Workbench reimport to match. Test: `items/carry-meshes`. A hand-carried item and the parcel stay in a living carrier's hands: `SCR_CharacterControllerComponent.ShouldGadgetBeDropped` refuses the engine drop (sprint, stance) for a hand-carry gadget or a `LF_ParcelComponent`, so only a drop action, a drawn weapon or a downed or dead carrier lets go (#920; the hand-carry storage also takes a carry gadget back to the hands when the engine lowers it). A parcel that leaves a slot for the ground (`LF_ParcelComponent` listens to its own `InventoryItemComponent` slot change, so it works as a hand tool or in the pack) gets a small faint warm marker light at its foot (`LF_ParcelComponent`, every machine; visible at night, barely by day), the dropper gets a toast, and the server lifts it onto the probed ground if it sank. Hold: the parcel borrows the vanilla supply-box carry set (`MedicalKit_supplies`, the mortar ammo box's) with `HandAdditiveTranslation 0 0.22 0.08` and `HandAdditiveRotation 0 90 0`, so the fist grips the top edge of the box and it hangs by the left leg, front to back; with the old wrench set and no offset the hand went through the box (`docs/tooling/hold-tuner.md`). Tests: `jobs/parcel-carry`, `jobs/parcel-collision`, `jobs/parcel-hand-tool`, `jobs/parcel-hold`. Fuel cargo is
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

- ✅ **Parcel drops on the island** (2026-11-18, #2959; draft PR): no building of Saint-Philippe's own survey stands
  300 m from the depot kiosk (b21), so the run had no stop that pays. `Place_saint-philippe_parcel-drops.layer` holds five
  `LF_ParcelDrop` mailboxes (a pack prefab over the vanilla `Mailbox_02`: `LF_JobSiteComponent` DROP plus `LF_JobDropoffAction`, so
  `LF_JobMapMarks` finds them by place, not by name) on the entrance walls of the east districts' wooden house and the
  dock's metal shed, cabin, garage and barracks (318 to 480 m out; `runMinDistance` stays 300). `town drops saint-philippe
  parcel-drops` writes them from the dossiers and seats each on its host's real wall. The island depot counter offers
  parcel, fuel delivery and taxi sign-ups again (#3373; they were dropped on 2026-11-18 while the island had no fuel or
  taxi sites). The fuel pickup and drop props (#3370) and the taxi rank (#3114) are the sites those two jobs run from.

- **Fuel drops at the farms** (2026-11-19, #3377): a fuel run now ends at a tank in a yard, not a letter box.
  `Place_saint-philippe_fuel-drops.layer` holds four `LF_FuelDrop` drums (the DROP site and drop-off action of #3370): beside
  the henhouse and the greenhouse on the village's far edge (296 m from the fuel station) and under the two open sheds above
  the quay (560 and 600 m). `town drops saint-philippe fuel-drops` writes them: the slot's kit now picks the prop
  (`DROP_KITS` in `tools/town/drops.mjs`; `parcel-drops` keeps its mailbox). Each stands within 1 cm of the ground
  (`look contact`). `look place:<place>/<slot>@<building>` now pictures one drop of an outposts slot.

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
  restored sign-up is put on a spawned body); Complete and Abandon read the same job
  (`LF_JobStartRunAction.ClientRefusal`, `LF_CivilianJob.RunOfferOpen`). The server
  re-checks everything when the action runs.
- ✅ **Dying or losing the cargo mid-run does not lock a player out of delivery work** (#3356, merged, in-dev; LOGIC
  `jobs/run-cargo-lost`): the cargo is no longer read for the client offer (a downed carrier drops the parcel, which
  greyed Abandon while "Pick up parcel" failed silently as "already has a run" until a restart), so Abandon shows for
  a delivery job on the body. The player's death (`LF_OnPlayerKilled`, subscribed by `StartRun`) ends the run as an
  abandon: no pay, the deposit and the contract stake stay forfeited, the map marks clear
  (`LF_CivilianJob.OnCarrierDied`). A refused start now toasts (`LF_JobStartRunAction.StartRefusalKey`: run still open,
  no room for the cargo, other refusal) and a drop-off without the cargo says so (`#LF-Job_NoCargo`). The harbour
  crates contract starts through `BeginRun`, so it ends the same way.
- ✅ (#619) The parcel's inventory description reads "Deliver to: <place>" (`LF_ParcelComponent` sets it from `#LF-Item_Parcel_Deliver` and `#LF-Job_ParcelDest` at spawn; test `jobs/parcel-description`). Demo: `session.mjs parcel-proof` (carry pose and inventory shots). Carry pose (#619, reworked in the parcel fix): the jerrycan graph hung the parcel 0.44 m below the hand (its grip point is at the canister's handle, the parcel mesh is centred on its origin), so it dangled at knee height through the legs, and it played canister sounds. The parcel was put back on `player_carry` for a while (box beside the arms, a stopgap); since #1846 it is a core hand tool held with the wrench graph (see the cargo row).

### `LF_JobManager` — `Feature/Jobs/LF_JobManager.c`
- ✅ `GetGatherReward` and `GetProcessReward` stay 0 unless a job scope calls
  `SetScopeReward` for that prefab (verified 2026-10-11). Gather and process
  mint nothing on their own. The player sells the product to a trader. A
  scope payout, when one exists, is cash via `LF_MoneyUtils.GiveCash`. The
  ATM stays the only cash-to-account boundary.
- ✅ `#EL-Job_Earned` is `Earned $%1 for work!` and `GiveReward` passes the
  paid amount through `WidgetManager.Translate` (verified 2026-10-11).

### `LF_JobLevels` — `Feature/Level/LF_JobLevels.c` (#749, D12)
- ✅ (2026-11-12) **Per-job XP replaces the single player level.** One XP track per job
  (CIVILIAN, DELIVERY, FARMER, MINER, LUMBERJACK, MEDIC, POLICE in the core config), keyed by
  the character record key. The store holds total XP; the level is 1 plus the job's
  thresholds reached (`LF_JobsConfig.m_aTracks`, owner key `jobs.tracks`), so a rebalance
  reaches old saves. `LF_PlayerLevelComponent`, its skill points and its unused passive
  bonuses are gone (component removed from `Character_Roleplay.et`; the debug menu's skill
  point button too). Tests `jobs/job-levels`, `jobs/job-level-gates`, `config/jobs-owner`,
  `persist/job-levels`, `e2e/pass2-verify-load`.
- ✅ XP is added only by the server: `AddXp(uid, job, amount, reason)` (quest reward XP to the
  quest's `m_sJob`, debug grants to CIVILIAN) and `Award(uid, action, units)` for in-world
  actions whose XP, job and cooldown are config (`jobs.xpAwards`: `delivery_run` 100 DELIVERY,
  `fruit_pick` 5 FARMER a fruit, `gather` 2 FARMER). An action inside its cooldown earns
  nothing; a job's `dailyXpCap` (FARMER 2000) drops the part over the cap per day of server
  time. Both clocks are session state. An unknown job or action earns nothing and logs
  `[LFDebug:JobLevels]`.
- ✅ Gates read job levels: delivery sign-up (DELIVERY: fuel level 2, taxi level 3, the old
  pacing), licence purchase (`LF_LicenseGate`: the licence family's job at the catalog level,
  or a `jobs.licenceGates` row), chain steps and quests (`m_sLevelJob` / `m_sJob`, CIVILIAN by
  default), lockpicking skill (CIVILIAN).
- ✅ (2026-11-17, #2700) **A job-level gate refuses in words, never a number.**
  `LF_JobLevelWords.Refusal(job, have, need)` returns a `#LF-Level_Near_<Family>` key one level
  short and `#LF-Level_Far_<Family>` further (Farmer, Miner, Lumberjack, Medic, Police, Delivery,
  `Any` for other jobs; "Do more runs first" for DELIVERY). `LF_ChainGate.LevelKey/ReasonKey` and
  `LF_LicenseGate.RefusalKey` take their keys from it, and a refused paid licence unlock toasts
  the words. Test `level/job-level-words`. Job sign-up and run-start refusals (`LF_JobAction`)
  and the chain station's `RefusalKey` still show the old line until #R4 / a follow-up wire them.
- ✅ **Saved as the `joblevels` payload (save version 28)**: one `LF_JobXpRow` per (character,
  job). A version 7-27 save migrates: the single level goes onto CIVILIAN and the delivery
  career onto DELIVERY at the same levels; every other job starts at 0. Load is idempotent.
- ✅ A level rise from any grant publishes `LF_OnJobLevelChanged`; `LF_JobLevelSync` refreshes
  the replicated DELIVERY level on the character's live body, so a quest's DELIVERY XP opens fuel
  delivery on the client too (`jobs/job-level-sync`). A capped or refused award does not start
  its cooldown. The pre-#749 career curve stays config (`jobXpFirstLevel`, `jobXpLevelGrowth`,
  read only to migrate), and the single level migrates whole at any level.
- ⚠️ Only the DELIVERY level reaches a client (`LF_PlayerJobComponent`'s replicated level, for
  the sign-up offer); no client reads another job's level yet, so no level is shown. There is
  no XP source yet for MINER, LUMBERJACK, MEDIC or POLICE, and no player-facing licence
  purchase, so those licence gates are reachable only through config and tests.

### `LF_LicenseManagerComponent` — `Feature/License/LF_LicenseManagerComponent.c`
- ✅ License catalog (26 entries), `CanUnlockLicense` (owned/config/whitelist/
  job level), `PurchaseLicense` (checks, then unlocks; nothing is spent since #749),
  `HasLicense`, persistence restore. The level gate is `LF_LicenseGate` (#749): the
  licence family's job (FARMER_* in FARMER, ..., criminal licences in CIVILIAN) at the
  catalog's `m_iMinLevelRequired`, unless the jobs config has a `licenceGates` row.
- ✅ **Whitelist mapping and purchase ordering fixed** (verified 2026-08-30):
  `GetLicenseWhitelistJob` maps MEDIC access to MEDIC and police licenses to
  POLICE; `PurchaseLicense` validates requirements before `UnlockLicense`. `SetUnlockedLicenses` is the persistence restore seam,
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
- ✅ **A remove before the first load still removes** (2026-11-17 property review, LOGIC
  `whitelist/remove-before-load`): `RemoveFactionWhitelist` and `RemoveJobWhitelist` load the
  file first. They returned false on an empty cache, so a police trial that expired on the first
  duty desk use after a restart was marked expired while the officer stayed on the roster.
- ✅ **`!wadd POLICE <uid>` writes the roster duty reads** (2026-11-17, LOGIC
  `chat/admin-wadd-grant`, #2548): `AdminChatChannel.ApplyWhitelistAdd` calls
  `LF_WhitelistManager.AddFactionWhitelist` and logs and reports to staff only when that stored
  a new entry. `MEDIC`, `CONNECT`, `NONE` (no roster; there is no medic faction type) and a uid
  already on the roster log an `LF_Debug.Warn` and report nothing.
- ✅ **The job whitelist reads the faction roster by account uid** (2026-11-17, WORLD `whitelist/job-roster`, #2577): `IsWhitelistedForJob(accountUid, job)` passes when the job's faction roster (POLICE job, POLICE faction) holds the account uid, or the old per-job list does (read only, never written). `LF_PlayerJobComponent` and `LF_LicenseManagerComponent` pass `LF_Utils.GetAccountUID`, so a second character of an approved officer passes. Manual: see the PR.
- ✅ **The dead legacy whitelist sign action, client flags and GM context actions are gone**
  (2026-11-17, WORLD `whitelist/legacy-gone`, #2576): `LF_WhitelistAction` (it needed
  `LF_PlayerWhitelistComponent`, which no character carries, and its flags were set only on the
  owning client), that component, and `LF_Enable/DisableWhitelistContextAction` (in no editor
  config) are deleted. `EL_WhitelistSign` keeps `LF_SignReadAction`, so the sign is still readable and
  no layer changed. `LF_WhitelistRespawnHandlerComponent` keeps the start migration and the CONNECT
  join gate; its `OnPlayerSpawned` is an empty hook. Police duty reads `LF_WhitelistManager` and is
  unaffected.
- ✅ **Legacy whitelist files move into the roster once** (2026-11-17, WORLD
  `whitelist/legacy-migrate`, #2575): on game-mode start `LF_WhitelistRespawnHandlerComponent`
  passes each configured `LF_Whitelist` to `LF_WhitelistLegacyMigration.Migrate`. POLICE uids go to
  `AddFactionWhitelist(uid, POLICE)`, repeats and rostered uids skipped, and log
  `migrated <n> legacy POLICE uids from <file>`. CONNECT and MEDIC have no account roster: their
  uids are counted and logged, not migrated. The legacy file is only read; a line in
  `$profile:LifeFramework/whitelist-legacy-migrated.txt`, written after the roster saved, stops a
  later start re-adding a uid an admin removed. An unreadable roster or failed save leaves the
  file unmarked, so the next start retries. Nothing writes the legacy file (`AddNewUuidToFile` and
  `RemoveUUIDFromFile` are deleted). To re-run it after restoring an older roster, delete the marker
  (`docs/owner/rented-host.md` "Backups").
- ✅ **A migrated legacy file that still changes warns once per start** (2026-11-17, WORLD
  `whitelist/legacy-ignored-count`, `whitelist/legacy-failure-paths`, #2607): when a POLICE legacy
  file is already marked, `Migrate` counts its uids that are not on the roster
  (`LF_WhitelistLegacyMigration.CountIgnored`: added to the file after the move, or removed from the
  roster since) and logs `LF_Debug.Warn` `<n> uids in the legacy POLICE file are not on the roster and
  are ignored`. An unreadable roster (adds nothing, no marker) and a failed roster save (no marker)
  are tested, each followed by the next start migrating in full.

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
  is manual). The server reads the bank balance (`LF_ATMManager`) plus the cash the
  player carries (economy review 2026-11-17, test `jobs/benefit-counts-cash`) and pays
  `m_iBenefitAmount` cash when the balance is strictly below
  `m_iBenefitBalanceThreshold`, the player is not on duty as police and has no
  active delivery run, and `m_fBenefitCooldownSeconds` passed since that
  account's last collection. Values in `Configs/Jobs/LF_Jobs.conf` (flagship
  $100, below $500, 1200 s). Pick-up sound, then a `#EL-Benefit_Paid` toast; a
  refusal toasts its reason. A full inventory pays nothing and does not start
  the cooldown.
- ✅ The cooldown survives a restart (M1-10): the save holds seconds since the
  last collection, because world time starts over.

### `LF_LicenceDesk` — `Feature/License/LF_LicenceDesk.c` (#1766)
- ✅ The firearm permit is sold at a licence desk: fee in carried cash (`licences.permitFee`), a clean
  record (wanted level 0, no open crime report, no conviction in the last `licences.permitConvictionDays`
  in-game days, default 7, 0 is off; #1995) and the licence's gate at application and collection,
  issued at the desk one in-game day later (`licences.permitWaitDays`). On-duty police revoke it with
  a player interaction, but only once the holder is cuffed (`LF_Restraint`; #1916): the
  interaction stays hidden and the server's `Revoke` refuses and logs for a free player. Test `licence/revoke-cuffs`. The gun counter (#1903) then serves the holder. Tests `licence/desk`,
  `e2e/licence-desk` (the open application, a revoke and a conviction that still blocks the permit across `wt e2e`).
- 🧪 (2026-11-18) **The firearm permit is got at the gun store** (maintainer 2026-11-17; follows draft #2061). A catalogue
  offer with `m_sGrantsLicence` (owner key `grantsLicence`) is a licence row: the counter's price list shows the licence
  with the permit fee and, on Pay, runs the same `LF_LicenceDesk.VisitFor` the desk ran (fee in carried cash through
  `LF_EntityCashWallet` with ledger reason `licence-fee`, clean record, the licence's gate, the in-game-day wait, the
  `permit_due:` flag), so the player applies and later collects at that counter (`LF_ShopComponent.ApplyForLicence`,
  `LF_CharacterShopComponent.RpcAsk_Licence`; the server re-checks range, rate limit, a raided shop and that the outlet has
  the row). The slip prints the answer (`#LF-LicenceCounter_*`, else the desk's refusal keys). A row for a licence the
  office does not issue (anything but `FIREARM_PERMIT`) is dropped at load and logged. The core ships no row, so the desk
  still issues the permit in a core-only world; the pack's `LF_Catalogue_GunStore.conf` puts the row on `gun_counter`,
  and while any outlet has the row the desk hides and refuses its permit action (`LF_LicenceDesk.DeskIssuesPermit`).
  An application already open at the desk is collected at the counter. Selling a gun still needs the permit. The client
  reads the server's fee from the catalogue stream (`fee`). Tests `licence/counter`, `everon/gun-store-permit` (pack leg).
- 🧪 (2026-11-18, #3128) **The licence desk sells a driving licence** (`DRIVING_LICENCE`, enum value 301, replication bit 29).
  The desk's second action "Buy a driving licence" (`LF_LicenceDeskDrivingAction`) issues it at once for
  `licences.driverLicenceFee` (core default 100) in carried cash: no record check, no wait (`LF_LicenceDesk.BuyDrivingFor`).
  Short cash is refused with nothing taken, a holder is refused as already held, and the fee is given back if the grant
  fails. It is saved in the progression licence list like the permit. The action also shows where the gun store sells the
  permit. No car lot asks for it yet, so a newcomer's first car needs none. Test `licence/desk-driving`.

- ✅ **Run rules: deposit, distance, time, passenger** (#2408, merged, in-dev). All numbers are the jobs config's
  (`Configs/Jobs/LF_Jobs.conf`, owner section `jobs`):
  - *Deposit.* `LF_CivilianJob.BeginRun` takes a deposit from the player's cash when a run starts: the cargo's shop sell price
    (`LF_ShopManagerComponent.GetSellPrice`) times `cargoDepositPercent` (100), or `cargoDepositFallback` (50) when the
    shop list has no price. A player short of it gets no run and no cargo (toast `#LF-Job_NeedDeposit`). A delivery returns it
    once; abandoning forfeits it, so start, abandon, start, abandon costs the cargo's value each loop (the cargo is not taken
    back). A taxi fare has no cargo and no deposit. A restart ends the run, and the deposit it held is given back to the
    player's bank account on the load (save version 49, payload `jobruns`, one `LF_JobRunRow` per run holding a deposit: uid, amount and
    job; `LF_CivilianJob.ExportDeposits` / `ApplyDeposits`, tests `jobs/deposit-restart` and `jobs/deposit-cargo`, proof `wt e2e`), so a
    restart never takes a player's money. The run's cargo goes with it (#2481): before the refund the load takes one parcel or fuel can
    (the job's own) out of the player's saved carried snapshot (`LF_CivilianJob.StripSavedCargo`), so the deposit and the cargo are
    never both kept; a cargo no longer carried (dropped, lost at death) is just refunded. The log says which
    (`[LFDebug:Jobs] restart ended the run of ...`). The bank takes the money because the player may be offline when the
    server loads. A run still live in the session (a re-applied load) keeps its own deposit and gets no second one. Ledger reasons
    `job-deposit`, `job-deposit-back`.
  - *Distance and time.* `BeginRun` records where and when the run started. `LF_CivilianJob.Dropoff` (the one call behind
    `LF_JobDropoffAction`) pays only at a placed drop (`LF_JobSiteComponent` site DROP), at least `runMinDistance` (300 m,
    straight line) from the start, after at least `runMinTimeFraction` (0.6) of the expected driving time, which is the
    distance over `runRoadSpeed` (10 m/s). A refused drop pays nothing, keeps the run, the cargo and the deposit, and shows
    `#LF-Job_TooClose`, `#LF-Job_TooFast` or `#LF-Job_NotADrop`. A run with no start on record pays nothing.
  - *Taxi.* A fare also needs another player seated in the driver's vehicle when the fare is dropped off
    (`LF_CivilianJob.CountPassengers`; `#LF-Job_NoPassenger`).
  - *Map stop.* `LF_JobMapMarks.OnRunStarted` marks the nearest drop at least `runMinDistance` from the start, so the stop
    always pays; when none is that far it marks the nearest.
  - `CompleteRun` is the payout only and `AbandonRun` the forfeit; production code reaches a payout through `Dropoff`.
    Tests `jobs/run-rules` (LOGIC: the checks, the deposit state, the config) and `jobs/run-cargo-deposit` (WORLD: a real
    character, cash and cargo at the placed drop). Manual: a taxi fare with a second player is in the PR.
- **Map marks (#2239):** a fare destination and a delivery stop become a shared mark plus the waypoint for their player, cleared when the run completes (`Feature/Jobs/LF_JobMapMarks.c`, `docs/features/map.md`; test `jobs/job-map-marks`). The marks are driven by `OnFareAccepted`/`OnStop`; nothing in the run code calls them until fares and stops exist.
- 🧪 (2026-11-17) **A body the save restored gets its saved licences (#2413).** A player who takes control of a living body no spawn made (`LF_SpawnLogic.CreateCharacter`: "already controls a character") never went through `BindAccountComponents`, so the body held only its starter licences and `LF_LicenceDesk.Holds` charged a permit holder again. `LF_SpawnLogic.AdoptLivingBody` now runs `LF_Progression.ApplyToCharacter` on that body (idempotent; a missing body or key logs `[LFDebug:Spawn]` and applies nothing). Test `licence/restored-body` (WORLD). Manual: grant a permit, restart the server, rejoin: the licence desk and the gun counter see the permit and charge nothing.
- 🧪 (2026-11-18) **A restored body keeps its delivery job (#3353).** The saved sign-ups load while nobody is connected, so `LF_CivilianJob.ApplySignUpToLiveBody` found no body, and the "already controls a character" branch only ran `AdoptLivingBody`: a signed-up courier, fuel driver or taxi driver came back UNEMPLOYED at level 1 after every restart and the job actions greyed out. `LF_SpawnLogic.AdoptLivingBody` now also runs `LF_CivilianJob.ApplySignUpToCharacter` and `LF_PlayerJobComponent.PullCareerFor` on the body (both idempotent). Test `jobs/signup-restored-body` (WORLD). Manual: sign up as a courier on a dedicated server, restart it, rejoin: "Pick up parcel" is offered at once.
- 🧪 (2026-11-17) **A taxi stand is a place (#2691).** `Prefabs/Jobs/LF_TaxiStand.et` is a pole sign carrying `LF_TaxiStandComponent` (`Feature/Jobs/LF_TaxiStandComponent.c`), which registers in `LF_Places` as kind `taxi` (`LF_TaxiStand.PLACE_KIND`; place id = the component's id, else the entity name, else `taxi_<entity id>`). `LF_TaxiStandComponent.Nearest(pos)` returns the registered stand closest on the ground plane (or null), and `GetLabel()` names it for a fare and the map: the prefab's label, else the nearest map location (`LF_NamedPlaces`), else the place id. The core holds no coordinates or place names. Test `jobs/taxi-stand-places` (WORLD). Grid cell `taxi-stand` (`tools\cli look grid:taxi-stand`). No fare or map row reads the kind yet (#2690, #2239).
- 🧪 (2026-11-17) **A taxi fare is priced by the distance driven (#2690).** A taxi drop-off pays `LF_TaxiFare.Price(pickup, dropoff)` instead of the flat 80: the pickup is the run's server-recorded start (`m_vRunStart`), the distance is the straight line to the drop site. A drop within `shortHopMetres` (100) of the pickup pays `floor` (80); beyond it the fare is `floor` plus `ratePerMetre` (0.04) per metre, never above `cap` (300), so Saint-Philippe to Montignac (3.7 km) pays about 228. The numbers are `Configs/Jobs/LF_TaxiFares.conf` with the owner's `taxiFares` section over it (`floor`, `ratePerMetre`, `cap`, `shortHopMetres`; its own section, not under `jobs`); a zero or negative number, or a cap below the floor, logs `LF_Debug.Error` and prices with the shipped numbers. `LF_CivilianJob.Dropoff` passes the fare to `CompleteRun`; the ledger reason and the XP are unchanged, and `CompleteRun` with no fare still pays the old flat `PAY_TAXI`. Test `jobs/taxi-fare` (LOGIC).
- 🧪 (2026-11-18) **Mining, logging and medic work earn job-level XP (#2707).** `LF_JobLevelSources` (`Feature/Level/LF_JobLevelSources.c`, subscribed at game start) turns bus events into `jobs.xpAwards` rows through `LF_JobLevels.Award`: `LF_OnGathered` of family `MINING` pays `gather_mining` (10 MINER), family `LOGGING` pays `gather_logging` (10 LUMBERJACK); `LF_OnPlayerRevived` pays `medic_revive` and `LF_OnPatientDelivered` pays `medic_delivery` (25 MEDIC each, to the medic; a self-revive or a medic with no player id earns nothing). Gather cooldown 8 s, medic cooldown 60 s, and the MINER, LUMBERJACK and MEDIC tracks carry a daily cap of 2000. DIGGING and FORAGING gathers pay nothing here. Test `jobs/level-sources` (LOGIC). Manual: mine, log and revive in a session, then read the XP in `[LFDebug:JobLevels]`.
- 🧪 (2026-11-18) **The island's parcel depot is fitted (#2325, draft).** The Saint-Philippe parcel slot (newspaper kiosk b21, 	ools/town/kits/parcel.json, Place_saint-philippe_parcel.layer) holds the depot counter (CounterPostOffice_01 with the sign-up, start-run, abandon and benefit actions and an LF_JobSiteComponent DEPOT) inside the booth and a post_office sign on its wall, so the town board's post-office card has a sign to find. No drop-offs yet: a drop must stand at least unMinDistance (300 m) from the depot, so they are a separate card. Manual: see the PR.
- 🧪 (2026-11-18) **Fishing earns FISHER job-level XP (#2995).** `Configs/Jobs/LF_Jobs.conf` has a `FISHER` track (the 19-level ladder of the other trades, daily cap 2000) and the `fish_catch` row (10 XP, no cooldown: a cast takes at least 15 s). `LF_FishingSession.Land` awards it after the fish is given, so a strike that gives nothing earns nothing; before the track existed the award logged a warning and paid 0, and the rod's FISHER level read 1. The catch is also published as `LF_OnFishCaught` (`Feature/Fishing/LF_FishingEvents.c`) for later readers. Test `fishing/cast`.
- 🧪 (2026-11-18, #2710, slice of #1018) **The 1989 pack gives FARMER, MINER, LUMBERJACK and MEDIC a level table with one unlock per level.** `LF_Everon1989/Configs/Jobs/LF_Jobs_Everon1989.conf` (header overlay) sets each job's thresholds to levels 2..N and one `licenceGates` row per level: FARMER apple 2, water 3, plum 4, speed 5; MINER coal 2, copper 3, iron 4, speed 5, gold 6; LUMBERJACK basic 2, process 3, speed 4; MEDIC basic 2, advanced 3, revive 4. POLICE and DELIVERY keep the core tables (POLICE has no XP source yet, DELIVERY unlocks are fixed in `LF_CivilianJob`). Refusal words for those four jobs are pack rows `LF-Level_Near_*` / `LF-Level_Far_*` that override the core rows by Id. Tools: the pickaxe is sold at the `miners_supply` catalogue outlet only to a MINER at level 2 or more (`LF_Catalogue_MinerTools_Everon1989.conf`), and `LF_ShopStock_Everon1989.conf` gains a `general_store` stock (the vanilla entrenching tool as the shovel, plus the four house furniture items at the general store overlay prices). ⚠️ No counter names the `miners_supply` outlet or the `general_store` stock yet: placing them is a town pass (shelf stock rests on boards by measured contact, #2961). Medic ranks are a separate pack overlay: `LF_MedicConfig` reads pack overlays since #3047 and the pack's four-rung ladder is in `docs/features/character-survival.md`. Test `everon/job-tables` (pack leg).
- 🧪 (2026-11-18) **Run pay is config (#3147).** A parcel, fuel or flat taxi run pays its row in the jobs config's `runPays` list (key `PARCEL`, `FUEL_DELIVERY` or `TAXI`, field `pay`; `Configs/Jobs/LF_Jobs.conf` holds the neutral 40, 60 and 80, a pack's `LF_JobsConfig` overlay or the owner's `jobs.runPays` section replaces a row). `LF_CivilianJob.PayFor` reads it through `LF_JobsConfig.RunPayFor`; a missing row or one below 1 pays the `PAY_` constant and logs `LF_Debug.Error`. The 1989 pack pays the parcel 40, the fuel run 75 and the flat taxi run 110. A measured taxi fare still pays `LF_TaxiFare`, and `LF_TaxiFareConfig` now takes pack overlays field by field: the 1989 pack ships `LF_TaxiFares_Everon1989.conf` (floor 90, 0.005 per metre, cap 150), so a 2000 m fare pays $100 (about $600/h, under police entry duty pay) and a short hop pays more than a fuel run (#3164). `tools\cli economy` reads the rows. Tests `jobs/civilian-run-pay-config` (LOGIC) and `everon/run-pay` (pack leg).
- 🧪 (2026-11-18) **Saint-Philippe taxi rank (draft, #2318).** The `taxi` slot of the real island is fitted on the kerb in front of b39 (the townhouse beside the clinic): the pad is an `LF_JobSiteComponent` of site TAXI with the "start a run" action (`LF_JobStartRunAction`, TAXI), and a beige S105 stands at the kerb as the cab. The kit is `tools/town/kits/taxi.json` (extracted from `Street_Jobs.layer`; the cab is its own row because `town fit` writes no child entities), the layer `LF_Everon1989/Worlds/MainWorld/MainWorld_Layers/Place_saint-philippe_taxi.layer`, the claim `tools/town/claims/saint-philippe-taxi.json`. Taxi runs now have a real site on the island, so the depot sign-up for the taxi (removed on #2958) can come back. `town check` warns that the kind `TaxiRank` is in no system's `m_aPlaces`; the job code finds the pad through its component, not by place kind.
- 🧪 (2026-11-18, #3150) **Every licence refusal says where to get the licence and what it costs.** `LF_LicenceWhere` (`Feature/License/LF_LicenceWhere.c`) builds one sentence per licence: the firearm permit and the driving licence read "<place> sells it for <fee>." (place is the string row `LF-LicenceWhere_<TYPE>`, neutral core text "The licence desk"; the pack row of the same Id says "The gun store counter" for the permit; fee is `licences.permitFee` / `licences.driverLicenceFee`), a job licence reads "Earn it through <job> work." (never a level), and a licence with no where text reads "Ask at the licence desk.". The shop refusal (`LF_ShopRefusal.Text`, a gated outlet's "Not without a %1.") and the chain station refusal (`LF-Chain_NeedLicenseWhere`, toast from `LF_ChainStationAction` via `LF_ChainGate.LicenceWhere`) both carry it. The shop line is composed on the client, which reads the fee from its own licence config, so a dedicated client of a server with a non-default fee shows the default fee until the fee rides the reply (follow-up). Test `licence/where` (LOGIC).

- ✅ **Fuel pickup and fuel drop props** (2026-11-18, #3370): the island had no FUEL site, so the fuel delivery run could
  not start there. Two pack prefabs in `LF_Everon1989/Prefabs/Props/Roleplay/`: `LF_FuelPickup` (a vanilla pallet with four
  jerrycans: `LF_JobSiteComponent` FUEL plus `LF_JobStartRunAction` FUEL_DELIVERY) and `LF_FuelDrop` (a vanilla fuel drum:
  `LF_JobSiteComponent` DROP plus `LF_JobDropoffAction`, shaped like `LF_ParcelDrop`; a drop also takes parcels). Destruction
  is off on both so a broken prop cannot strand a run. Not placed yet (placement cards #3376-#3381); the depot sign-up
  returns with #3373. Test: `everon/fuel-props` (pack leg).
- ✅ Route job rules (#3324, #1019, LOGIC `jobs/route-rules`): `LF_RouteRules` (`Feature/Jobs/LF_RouteRules.c`) is the pure rule set for a route job: an ordered list of stops. `Create(stops, ratePerStop, roundBonus)` returns null (logged) for zero stops or a negative rate or bonus. `NextStop()` is the index to reach, -1 once done; `CompleteStop(i)` refuses (logged, no change) a stop out of order, repeated, out of range or after the end; `PayAtEnd()` is stops x rate + the bonus once every stop is done; `AbandonPay()` is half the rate per done stop, rounded down, no bonus. Int math, no world, no statics, no config file. Nothing calls it yet; the route job and the bus driver job reuse it. Not saved.
- 🧪 (2026-11-19, #3186) **The driving licence holds through a rejoin, a late joiner and a restart (dedicated server, island).** Proof `session.mjs licence-late-proof` (`agent-box/proofs/licence-late-proof.mjs`, `up --mission island --clients 1`): client 1 buys the licence at the town hall desk, rejoins (same account, new player id) and buys a car at the lot with no refusal while the desk's action is hidden from it (its client reads `m_iUnlockedBits`); client 2 joins late without a licence and gets the lot's refusal toast `#LF-Lot_NeedsDrivingLicence` (the server logs `purchase refused for <uid>: no driving licence`); client 2 then buys the licence, the server restarts (`session restart`) and client 2's car buy goes through, client 1's desk action stays hidden. Passed with no code change; `--selftest` checks the log judges without a game.
- ✅ Licence seizure rules (#3428, #1035, LOGIC `licence/seizure-rules`): `LF_LicenceSeizureRules` (`Feature/License/LF_LicenceSeizureRules.c`, `Configs/Crime/LF_LicenceSeizure.conf`) is the rule table for which crime takes which licence and for how many in-game days. A row is `{m_sCrimeId, m_aLicences (LF_ELicenseType names), m_iBanDays}`; `BansFor(crimeIds, bans)` returns one `LF_LicenceBan` per licence the conviction's codes take, the longest ban when two codes take the same licence, nothing for an unmapped code, and a licence name that is not an `LF_ELicenseType` is logged and skipped. Core ships brandishing, weapon theft and murder taking the firearm permit, reckless driving and drunk driving (`#LF-Offence_DrunkDriving`, not in the crime catalog yet) the driving licence; a pack changes rows with an overlay of the same class (add, replace, patch, remove by crime id). Server only, not saved. Nothing takes a licence from a character yet: the conviction hook calls `BansFor`.
- 🧪 (2026-11-19, #3430) **Licence bans survive a restart** (save section `licencebans`, PERSISTENCE `licence/bans-restart`). `LF_LicenceBans.Ban(uid, licence, days, holder)` (`Feature/License/LF_LicenceBans.c`) takes the licence from the character's saved licences (`LF_Progression`) and from the live body when given, and stamps the in-game minute the ban ends (the game clock's date, `LF_LicenceDesk.NowMinutes`; a second ban keeps the later end; 0 days takes the licence with no ban). `LF_LicenseManagerComponent.CanUnlockLicense` refuses a banned licence, so the licence desk, a shop counter and `PurchaseLicense` all refuse it until the ban ends; an admin `/licence` grant still works. The section (`LF_LicenceBansSave.c`) saves each ban still in force by record key, licence name and end minute; a load replaces the live bans, lifts one that ended while the server was down (`[LFDebug:License] ... ended while the server was down ...; lifted on load`) and takes each banned licence from the saved record again. A save from before the section has no bans. The arrest that hands out a ban is #3432.
- 🧪 (2026-11-19, #3432) **An arrest seizes the licence, and the desk says when it comes back** (PERSISTENCE `licence/seizure`). `LF_LicenceSeizure` (`Feature/License/LF_LicenceSeizure.c`) subscribes to `LF_OnArrested` (game mode start, `LF_LicenceSeizure.EnsureSubscribed`) and reads the suspect's open crime reports (`LF_WantedReports`) as the conviction's crime codes. Each code the seizure rules (#3428) map takes its licences through `LF_LicenceBans.Ban` (#3430), passing the arrested body so it does not keep the licence until the next respawn; the arrested player gets "Your firearm permit has been seized for 7 days." (`LF-LicenceSeized_*`). A conviction with no mapped code changes nothing (`[LFDebug:License] conviction of <uid> maps to no licence ban`). An arrest that seizes the firearm permit also drops an open permit application (`LF_LicenceDesk.RevokeFor`); the fee is not refunded. The licence desk, a shop counter's licence row and the driving licence request answer a banned buyer with the new outcome `LF_ELicenceDesk.BANNED`: "Refused: your firearm permit was seized. Come back in N days." (`LF-LicenceDesk_Banned`, `_DrivingBanned`; days rounded up). The ban rides the `licencebans` section, so a restart keeps it. Manual: get a brandishing conviction, check the toast, ask the desk for the permit (refused with the days left), restart, ask again.
- ✅ Garbage round (#3325, #1019, WORLD `jobs/garbage-round`): the steady, no-buy-in route job. `Prefabs/Jobs/LF_GarbageDepot.et` (a desk, `LF_GarbageSiteComponent`, action "Take the garbage round") opens a round and spawns the company truck (`LF_GarbageRound.Start`): the route is the `m_iStops` bins nearest the depot, found by component through `LF_BinComponent.FindAllBins`, never by name, nearest first. "Empty bin" on `LF_StreetBin` (shown only on a round) must hit the next bin in order (`LF_RouteRules`, #3324): it deletes the bin's contents server-side and puts a bag on the truck; an out-of-order or off-route bin is refused with a toast and changes nothing. "Tip the load" at `Prefabs/Jobs/LF_GarbageDump.et` needs every bin done and the truck within `m_fTipRadius` (30 m) of the dump, then pays stops x rate + bonus once through `LF_CivilianJob.HandCash` (reason `garbage-round`), closes the round and deletes the company truck. A truck with someone in its seats is not deleted under them: it is marked for removal (logged), a 2 s sweep that runs only while one is marked deletes it once empty, and while marked it cannot be locked or chopped (`LF_GarbageRound.IsPendingRemoval`), so a friend sitting in it at the tip never keeps a free truck. A dead, downed or cuffed worker is refused at every step (`UNFIT`, logged). Numbers: `Configs/Jobs/LF_GarbageRound.conf` (5 bins, $15 a bin, $25 bonus = $100 a round, about $400 an hour) with each pack overlay of `LF_GarbageRoundConfig` layered over it. Starting again closes the open round (no pay, old truck removed) so a dead round never locks the player out. Not saved: a restart ends the round. No `LF_EJobType` row and no SANITATION XP track yet (needs the job enum and levels config, outside this slice).
- 🔧 (2026-11-21, review of #3862) **Each open garbage round has its own bins.** Two players doing rounds at once got the same nearest bins and one bin counted for both. `LF_GarbageRound.Start` now leaves out every bin another player's round holds (`IsClaimedByOther`), so the second round takes the next free bins (fewer stops if few are free, "no bins" if none). A round whose truck is gone, or that has run longer than `CLAIM_SECONDS` (an hour, an owner who left), holds nothing, so an abandoned round never keeps bins for good. Test `jobs/garbage-round`.
- 🧪 (2026-11-19, #3148) **Night runs pay a night rate and the pay toast names each part.** A parcel, fuel or taxi run finished inside the night window of the world clock (`TimeAndWeatherManagerEntity.GetTimeOfTheDay`, read on the server at the drop-off; a world with no manager pays the base and logs `[LFDebug:Jobs] no world clock` once) pays its base plus `round(base x (rate - 1))`. The rate is `nightRate` on the job's `runPays` row (`LF_JobRunPayDef.m_fNightRate`; the core ships none, the 1989 pack sets 1.25 on all three rows, so a $75 fuel run pays $94); the window is `nightRunStartHour` and `nightRunEndHour` on `LF_JobsConfig` (21 to 5, wraps midnight; both 0 reads as the shipped 21 to 5; equal non-zero hours make no night). The night is its own part, a rate on the base only: it never compounds the harbour contract's multiple or the standing bonus (#2703). `LF_CivilianJob.Dropoff` splits the pay into `LF_JobPayParts` (run, night, bonus), books the night as the `night-rate` ledger reason and the bonus as `standing-bonus`, and `LF_JobDropoffAction` sends a toast that names the parts: `LF-Job_Paid_Night` "Paid $106: run $85, night +$21", `LF-Job_Paid_Bonus` (the contract and standing bonus, which the player never saw before) and `LF-Job_Paid_NightBonus`; a run with no extras keeps `EL-Job_Paid`. The parts ride the toast as one packed argument `~total|run|night|bonus` that the notification client splits into `%1` to `%4`. Tests `jobs/night-run-rate` (WORLD: the clock at 12:00 and 23:00, the contract run's parts, the toast key and argument) and `everon/night-run-rate` (pack leg). Manual: finish a fuel run after 21:00, then one at noon, read the toast.
- 🔧 (2026-11-22, Coalman's Brow mine) **A new miner can buy the pickaxe.** The pickaxe offer at the `miners_supply` outlet (`LF_Catalogue_MinerTools_Everon1989.conf`) asked for MINER level 2, but MINER XP comes only from pickaxe gathers (`LF_JobLevelSources`: DIGGING pays none), so no player could ever reach it. The offer now opens at level 1 (the job is still required); `everon/job-tables` checks `m_iMinRank <= 1`.
- 🧪 (2026-11-22) **Postal van run: a beginner delivery job with a lent van** (WORLD `jobs/postal-run`). `LF_PostalRun` (`Feature/Jobs/LF_PostalRun.c`, numbers in `Configs/Jobs/LF_PostalRun.conf`) runs it on the server; the offices are `LF_PostalPointComponent` stands (`Prefabs/Jobs/LF_PostalPoint.et`, the PO-box stand with one `LF_PostalAction` per step), registered in `LF_Places` as `PostalPoint`, never found by name. **Take the postal van run** needs a driving licence (`m_bNeedsDrivingLicence`), refuses a second run (one van per player), waits `m_iStartCooldownSeconds` (60) after the last run, and goes through `LF_RateLimit` SPAWN. The destination is the nearest other office at least `m_fMinDistance` (800 m) away (`PickDestination`). The van (the office's `m_Van`, else the config's) is spawned on a clear spot off the road: rings of 7 to 22 m round the stand, the AI road network check (`LF_PlacementZones.IsOnRoad`), level ground and a van-sized box trace. It leaves the world save (`LF_VehicleWorldSave.Hold`, no ownership record), is locked, and its key (`LF_VehicleOwnership.SpawnKey`/`GiveKey`, now public) goes in the player's pocket; the pack van has `m_bIgnitionLocked`, so only the key holder starts it. **Load or unload a mail sack** counts `m_iSacks` (4) in at the home stand and out at the far one, each with the van within `m_fVanReach` (18 m); the map mark and waypoint (`LF_JobMapMarks.OnStop`) move home, far, home. **Hand in** pays once at home: the jobs config's run pay row `POSTAL` (core $80, pack $80 with a 1.25 night rate; fallback `LF_CivilianJob.PAY_POSTAL`) through `LF_CivilianJob.HandCash`, plus the `postal_run` award (120 DELIVERY XP), never under `m_iMinRunSeconds` (420). A watch every 5 s ends a run with no pay when the van is destroyed or gone, the driver is off the server or over `m_fAbandonMetres` (350 m) from it for `m_iAbandonSeconds` (180), or the run passes `m_iMaxRunSeconds`. Every end takes the key and the van back (`LF_GarbageRound.ReturnTruck`, now public: an occupied van waits for its seats to empty). Not saved: a restart ends the run and the van with it, and nothing was paid. Pack: `LF_PostalVan.et` (vanilla S1203 cargo van in yellow, the LF lock with the ignition locked, painted English lettering "POSTAL SERVICE" on the sides and "MAIL" on the rear doors, no real logo), `LF_PostalPoint_Everon.et` (lends that van), the `postal-drops` outposts slot (`town drops saint-philippe postal-drops`: sp:b21, the parcel kiosk, and meaux:b01, the Meaux square kiosk, ~1.3 km), map badge kind `PostalPoint` (`LF_MapPois_PostalPoint.conf`), and Anselme's voiced "van run" reply. `toolscli economy` lists POSTAL: $80, 10 min, $480/h (police $900 = 1.9x, medic $792 = 1.65x). Merged, in-dev.

- 🧪 **Take the job at a gathering desk (2026-11-22).** Every `LF_Workplace.et` desk whose job is MINER, LUMBERJACK or FARMER (Coalman's Brow mine, the two woodlot desks, the orchard) offers **Take the job: <Job>** on its clock, and **Quit the job: <Job>** to a player who holds it (`LF_WorkplaceJobAction`, `LF_WorkplaceJob.Toggle`): server-side, the same standing gate as the counter and the job dialogue, then `LF_PlayerJobComponent.SetJob`, every outcome logged. Before it nothing in play signed a player up for those jobs, so the pickaxe and axe tills (which need the job) never sold. Duty desks keep the punch clock; delivery jobs keep their counters (all three have one on the island). The job lives on the body, as before: after a respawn the player takes it again at the desk. Test: `jobs/workplace-job-desk` (WORLD).

- 🧪 **30-level trade tables and tier words (2026-11-22, #4181, W1 of the 0.3.0 work scope).** FARMER, MINER, LUMBERJACK, FISHER and MEDIC level to 30 on one curve in the core `LF_Jobs.conf` and the pack overlay `LF_Jobs_Everon1989.conf` (29 thresholds each; the pack's four short tables are replaced, FISHER inherits the core one). Total XP: level 2 100, 5 1,000, 6 1,500 (unchanged, `MINER_GOLD` still opens at 1,500), 10 5,000, 15 11,000, 20 20,000, 25 30,000, 30 42,000. At about 700 XP an hour, level 5 is about 1.5 h and level 10 about 7 h. DELIVERY, CIVILIAN and POLICE keep their tables (the legacy mapping and the police ranks read them). The five tier words the UI shows instead of a number (decision N1) are pack rows `LF-Everon_TradeTier_1` to `_5`: Apprentice, Hand, Journeyman, Foreman, Master (bands 1-5, 6-11, 12-19, 20-27, 28-30; the cards that show them, W3/W6, pick the band). Licence gates still sit on levels 2 to 6 only; W4 adds one unlock per level. Test `everon/trade-tables` (pack leg); `everon/job-tables` now allows levels above 4 to have no gate. Merged, in-dev.
