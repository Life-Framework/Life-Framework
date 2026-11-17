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
  needs a Workbench reimport to match. Test: `items/carry-meshes`. A hand-carried item and the parcel stay in a living carrier's hands: `SCR_CharacterControllerComponent.ShouldGadgetBeDropped` refuses the engine drop (sprint, stance) for a hand-carry gadget or a `LF_ParcelComponent`, so only a drop action, a drawn weapon or a downed or dead carrier lets go (#920; the hand-carry storage also takes a carry gadget back to the hands when the engine lowers it). A parcel that leaves a slot for the ground (`LF_ParcelComponent` listens to its own `InventoryItemComponent` slot change, so it works as a hand tool or in the pack) gets a small faint warm marker light at its foot (`LF_ParcelComponent`, every machine; visible at night, barely by day), the dropper gets a toast, and the server lifts it onto the probed ground if it sank. Tests: `jobs/parcel-carry`, `jobs/parcel-collision`, `jobs/parcel-hand-tool`. Fuel cargo is
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
- ⚠️ `!wadd` stores nothing since the Police/Medic lists left `GameMode_Roleplay.et` but still
  reports the grant; the job whitelist is checked by character key and never written (#2412).

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

- ✅ **Run rules: deposit, distance, time, passenger** (#2408, merged, in-dev). All numbers are the jobs config's
  (`Configs/Jobs/LF_Jobs.conf`, owner section `jobs`):
  - *Deposit.* `LF_CivilianJob.BeginRun` takes a deposit from the player's cash when a run starts: the cargo's shop sell price
    (`LF_ShopManagerComponent.GetSellPrice`) times `cargoDepositPercent` (100), or `cargoDepositFallback` (50) when the
    shop list has no price. A player short of it gets no run and no cargo (toast `#LF-Job_NeedDeposit`). A delivery returns it
    once; abandoning forfeits it, so start, abandon, start, abandon costs the cargo's value each loop (the cargo is not taken
    back). A taxi fare has no cargo and no deposit. A restart ends the run and the held deposit is not returned (the cargo item
    stays in the inventory and sells for about the deposit). Ledger reasons `job-deposit`, `job-deposit-back`.
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
