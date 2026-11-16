# Crime & Police

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
- 🧪 **Evidence at the detonation, not the placement** (2026-11-16): placing a charge (`Arm`) files the crime
  report, opens the scene and sounds the alarm, but the footprint card the report would leave is only
  planned (`LF_ScenePieces.PlanFootprint`, held on the target as `LF_PendingFootprint`; `deferPieces` on
  `LF_Dispatch.Report` and `LF_WantedReports.Report`). `Open` lays it (`LF_ScenePieces.LeaveFootprint`):
  exactly one card per report, where the robber stood at the charge. `CancelCharge`, `Repair`, `Reset`,
  `Apply` and `Configure` drop the plan, so a defused charge leaves nothing. Bank vault, hospital cabinet
  and door charges all go through `Arm`, so they follow the same rule. Test `crime/breach-evidence`.
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
  The pack layer `Village_Buildings` (`LF_Everon1989`, #323 batch 3) places `Roadmap_Bank_Vault` as the whole vault kit (`LF_BankVault.et`) at 194.3 0.007 259.6 facing the street,
  built onto the bank's east wall (see "Bank vault in the bank" below), the bell box
  `Roadmap_Bank_AlarmBox` on the bank wall above its roof, `Roadmap_Bank_Bag` on the vault floor
  (194.3 0.5 261.6) and the rob kit chest on the grass beside the door (196.7 0 258.5),
  all on `Street_Bank.layer`. The bag is
  `{65DE48DCC1D4DD01}Prefabs/Items/Roleplay/LF_MoneyBag.et` (a generated grey cloth sack tied with rope,
  `Assets/Items/Roleplay/MoneyBag/MoneyBag.xob`, 0.46 m tall, origin at its base). Cash is
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
  player who kills another player or an NPC gets wanted +the catalog's amount
  (`#LF-Offence_Murder` in `LF_CrimeCatalog.conf`, default 3), one `LF_WantedReports` report with the
  `#LF-Offence_Murder` offence and their description (no name), a radio
  dispatch to every on-duty officer (`LF_Dispatch.Report`, no site alarm), and a toast. Not murder: a death with no player behind
  it, a self-kill, a destroyed vehicle, and (config
  `m_bOfficerKillOfWantedExempt`, default on) an on-duty officer killing a
  wanted suspect. Each decision logs `[LFDebug:Murder]`. No automatic ban.
- ✅ **Zone rule sets as data** (#755, #1077; tests `zones/rules`, `zones/ruleset-backcompat`, `zones/conf-off`). `LF_ZoneRuleSet` and `LF_ZoneConfig` members carry no initializers, so a `.conf` value of 0 is read as 0 (an initializer used to win); script builds one with `CreateDefault`. `LF_Zones.conf`
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
- 🔧 **Fuel till robbed through its attendant** (#936, merged, in-dev): the till is `LF_FuelTill_Attended.et` (its world action is retired) and the voiced `LF_FuelClerk` stands behind it. The robbery reply in the `fuel_clerk` dialogue is offered only while a weapon is drawn (`RobArmed` hook, `LF_RobDialogue.c`). Picking it calls `LF_RobHold.Start` on the server (armed, in range, free till, cooldown, `LF_RateLimit` CRIME), starts a hold timer with a bar and radius warning (`LF_RobHoldHud`), and pays through `PayHeld` when the hold is kept; leaving, weapon away, police arriving or going down each end it with a message and a cry from the attendant. Alarm, dispatch, witness and evidence fire as before. Tests `crime/rob-hold`, `crime/fuel-clerk-dialogue`. Demo: `session.mjs fuel-robbery-proof`.
- ✅ **Fuel-station till** (verified 2026-10-11): `Roadmap_Fuel_Till` on
  `Street_Fuel.layer` at `259 1.04 241.2` (on a counter at the fuel station) is
  `{4E4E1620C6B02E1F}Prefabs/Props/Commercial/EL_FuelTill.et`, which inherits
  `{51D31AB07AE3C1C7}Prefabs/Props/Commercial/CashierShop_01.et`,
  with `LF_RobAction`. Payout is a config range (`m_iMinPayout`..`m_iMaxPayout`);
  the server rolls it, and min equal to max pays that fixed amount. The debug
  till pays 150 MoneyStack cash. The bank balance stays 0. Wanted rises by
  the catalog's robbery amount (default 1) and still clamps at 5. A second attempt
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
  `LF_RobVehicleAction.CommitVehicleRobbery` (wanted from the catalog, weapon theft 2 and carjacking 3;
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
  the crime catalog's offences with a fine (`LF_CrimeCatalog.conf`) at their fixed amount; offences above the
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
  `m_iTicketSecondDeadline` (1800 s) the holder gets the catalog's `#LF-Offence_UnpaidFine` amount (+1)
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
  the nearest sign title within `m_fPlaceRadius`, else the nearest map town or
  landmark (`LF_NamedPlaces.Nearest`, #1944, WORLD `police/dispatch-names-place`:
  "bank robbery, Saint Philippe"), else a 100 m grid square. The place is stored as a
  localization key and localized where each board and radio line is built; the
  core holds no place names.
  An on-duty officer within `m_fCatchInActRadius` (`Configs/Police/LF_Police.conf`)
  catches it in the act and the report is identified at once. Showing your own
  ID card to an on-duty officer (`LF_ShowIDAction`) identifies every open
  report. Reports come down when wanted returns to 0, and persist as the
  `wanted` payload (save version 10).
- ✅ **Wanted board** `Roadmap_Police_WantedBoard` (`Prefabs/Police/LF_WantedBoard.et`,
  a cork `LF_Sign_Board` printing WANTED): "Read the wanted posters" asks the
  server per read (`RpcAsk_LF_Board` with kind `wanted`, the server finds the board near
  the reader). On-duty police get offence, stars, clothing, vehicle, and place;
  everyone else gets the public poster without the place. A name appears only
  on identified reports. The card is an `LF_UIScreen` paper sheet that closes
  4 m from the board; reading plays the map pick-up sound and gesture.
  On-duty police also see the dispatch log under the posters (M1-20).
- ✅ **Posters on the wanted board's face** (#758, `docs/architecture/visual-boards-spike.md`
  issue 1, budget in `docs/architecture/visual-boards-budget.md`): every player near the
  board sees a paper poster per wanted suspect pinned on the cork, without reading it:
  WANTED, the name once identified (else UNKNOWN PERSON), the offence, what was seen
  (`Wearing ...; driving ...`, the line left off when nothing was seen) and the wanted
  level. `LF_WantedPosters` builds the public list on the server (one poster per suspect,
  newest first, no place) on bus events (`LF_OnCrimeCommitted`, the new `LF_OnWantedChanged`
  and `LF_OnSuspectIdentified`) and after a load, coalesced to one broadcast; the board's
  `LF_WantedPosterBoardComponent` sends it by `RplSave`/`RplLoad` (late join, stream-in)
  and a reliable broadcast. Each client draws one render target over the cork
  (`Prefabs/Props/Signs/LF_BoardFace.et`, a 16:9 quad, the vanilla cork texture under the
  posters) within `drawRadius` (30 m) and releases it 5 m beyond; the face renders live for
  3 s after a change and then freezes, which the budget measured as nearly free. Owner
  config section `wantedBoard`: `posters` (4, at most 8 in two rows) and `drawRadius`.
  The Read card and the police-only lines are unchanged; the dispatch boards keep their
  painted face (`m_bDrawPosters 0`; decision 2026-11-13). Readable to about 4 m at noon;
  at night it has no light of its own, like every other sign (`session.mjs
  wanted-posters-look-proof` takes the pictures). Tests `police/wanted-posters-feed`,
  `police/wanted-poster-face`; proof `session.mjs wanted-posters-proof`.
- ✅ **Stolen cars on the wanted board's face** (#758, spike issue 5): a car its owner
  reported stolen (`LF_ReportStolenAction`) takes one of the last places (`wantedBoard.
  stolenPosters`, 1) as a STOLEN poster: a picture of the model (the prefab's own preview,
  in its default colour), the plate on a white strip, the car's name and "Seen it? Tell the
  police." The report carries the vehicle prefab's GUID; no place or owner reaches the face.
  Rebuilt on `LF_OnVehicleReportedStolen`, `LF_OnVehicleRecovered`, `LF_OnVehicleChopped`
  and any closed report. Test `police/wanted-posters-stolen`; proof
  `session.mjs stolen-poster-proof` (`up --clients 2 --mission playtest`).
- ✅ **Portraits on the wanted posters** (#758, spike issue 4): the server records the
  suspect's look at the crime (`LF_WantedLook.Capture`: the character prefab and each
  loadout slot's item as prefab GUIDs, no paths) on the report and in the poster entry.
  Each client dresses a dummy of that character in the preview world and the poster's
  `ItemPreviewWidget` draws it as a head-and-shoulders mugshot (a preview three frames
  wide, clipped by the photo frame), the same framing for a named and an unknown suspect.
  A report without a look keeps the empty photo frame. The face re-renders at 10 s and
  30 s after a change so streamed textures land, then the dummies are deleted. The look
  is saved as the `wantedlooks` payload (save version 29, keyed by report sequence), so
  the portraits come back after a restart; a version 28 save loads its reports without a
  picture. Tests `police/wanted-look`, `persist/wanted-looks`; `wt e2e` checks the look
  across a real restart.

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
- 🔨 The flagship ladder (#1769, merged, in-dev; `LF_Police_Everon1989.conf`): five ranks, Cadet, Patrol,
  Senior Patrol, Sergeant, Lieutenant. Fine caps 500, 1000, 2500, 4000, 5000; pay 0.3, 0.5, 0.65, 0.8, 1
  per on-duty second; duty to next 1h, 2h, 4h, 8h. A saved rank is the rung's index and a shipped rung cannot move, so
  the pack patches the core's `patrol` (index 0) into Cadet and `senior` (index 1) into Patrol and appends the other three.
  Titles are the pack's `LF-Rank_*` rows. A saved rank above the ladder clamps to the top on load.
- 🔨 The flagship kit (#1942, merged, in-dev; `LF_PoliceKit_Everon1989.conf`, listed after the ladder in the
  mission header): the pack overlay restates each rung's `m_aKit`. Patrol (index 0) draws the M9 and the spike strip;
  senior and every rung above draw the M16, the spike strip and the roadblock kit (`LF_Roadblocks.KIT`, added to the
  station catalog in `LF_PoliceRanks.Catalog`, since a kit line outside it is not issued). A civilian cannot draw.
  Test: `police/everon-kit`.
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

### `LF_Stun` and the taser — `Feature/Police/LF_Stun.c`, `Prefabs/Weapons/Handguns/Taser/` (#341 spike, 2026-11-13)
- ✅ **A stun weapon downs a player for a few seconds and wounds nobody.** A weapon prefab that carries `LF_StunWeaponComponent` (`m_fStunSeconds`, `m_fRange`) is a stun weapon. On the server, `SCR_CharacterDamageManagerComponent.HijackDamageHandling` hands every kinetic hit to `LF_Stun.TryHit`; when the shooter holds a stun weapon the damage is discarded, and inside the reach the victim enters the downed state (`LF_Downed`: the same replicated flag and vanilla unconsciousness, so a late joiner sees it) marked as a stun. A stun starts no murder attribution, the player cannot give up out of it (`LF_BleedOut.GiveUp` refuses), a disconnect while stunned is not a death, and the server wakes them after `m_fStunSeconds` with the health they had. A stale wake does nothing (token), a revive ends a stun early, an already downed player is not stunned, and a hit out of reach or on an NPC is absorbed with a log line. `LF_Stun.Decide` is the pure rule. Toast `#LF-Police_Stunned`. Logs `[LFDebug:Police] player <uid> is stunned for <n> s`.
- ✅ **The owner now recomputes consciousness when the downed flag sets, too** (`LF_OnDownedReplicated`): a stun damages no hit zone, so without it the stunned player's own client kept walking.
- ✅ **Taser** (`Handgun_Taser.et` on the vanilla `Handgun_M9_base.et`, 8 s, 10 m): its own magazine well (`LF_MagazineWellTaser`), so a pistol magazine does not fit, a one-round cartridge (`Magazine_Taser.et`), and a 1-damage projectile (`Ammo_Taser.et`, listed in the `AmmoList.conf` override the magazine system needs; test `resources/ammo_list` and `validate-ammo-list` fail when a round an addon magazine uses is missing from it, #1643). One lies on the DebugWorld police desk (`Roadmap_Police_Taser`). Test `police/stun`; `session.mjs taser-proof` on two clients: taken from the desk, the target is stunned and seen stunned by both, give-up is refused, and they get up.
- ⚠️ The model (v2, 2026-11-13, a boxy yellow and black body with a slim grip the hand closes around) is a generated placeholder (`docs/asset-inventory.md`); the cartridge wears the vanilla magazine mesh; the muzzle flash is still the M9's. No agent has fired a dart at a player: the proof enters at the damage handler (`shoot` verb). Any kinetic hit by a player holding the taser counts as its hit. The officer gets no taser from the armory or duty kit yet. Manual steps in the PR.
- ✅ **Taser sounds** (2026-11-13, asset pipeline sound stage): the taser lists its own sound projects in place of the M9's shot and handling (`Sounds/Weapons/Handguns/Taser/`): `SOUND_SHOT` is a pop and a second of electric crackle, `SOUND_DRY` a plastic click (three takes), `SOUND_RELOAD_MAG_REMOVE` and `SOUND_RELOAD_MAG_INSERT` the cartridge out and in. The M9's slide sounds are gone with its handling project; melee, pick-up and drop stay vanilla. All generated with ElevenLabs (`tools/assets/taser/sounds.json`, `docs/credits.md`). Test `police/taser_sound`.
- ✅ **The stunned body falls, stays down and gets up** (#1857, 2026-11-15): a stun is a knockout (`Feature/Medic/LF_Knockout.c`, reusable for a punch). The server owns the decision and the timer; the replicated knockout flag makes the machine that owns the body start vanilla's ragdoll (`CharacterControllerComponent.Ragdoll`, owner-only; the engine replicates it to proxies) and hold it with `RefreshRagdoll` for the stun. The wake releases it (`RefreshRagdoll(0)`), keeps the character unconscious 1.5 s while it settles prone, then the vanilla wake-up leaves them lying prone and in control. Refused, and nothing changes, on a dead, medically downed or vanilla-unconscious character; ending one never revives anyone. While stunned the target counts as downed, so an officer can cuff them (`LF_Restraint`). Test `medic/knockout`; `session.mjs taser-ragdoll-proof` (2 clients, full): ragdoll on owner and peer, release at 8.0 s of 8, prone, then ALIVE. Harness `state`/`peers` lines carry `ragdoll=` and `stance=`. Logs `[LFDebug:Medic] knockout ragdoll on <uid> starts|released`.
- ✅ **Stun buzz** (`LF_StunBuzz`, 2026-11-13): while a player is stunned an electric buzz loops on their body (`SOUND_LF_STUN_LP`, heard to about 60 m). The stun flag on the character is now replicated (`m_bLF_Stunned`); each client starts the loop when it sets and ends it when it clears, a host included, with a 30 s cap. `session.mjs taser-sound-proof` on two clients with `--sound`: the taser knows and plays its four events and not the M9's `SOUND_RELOAD_BOLT_PULL`, both clients start the buzz with the stun and stop it at its end, and client 1's audio is recorded.
- ⚠️ No ear has judged the sounds: an agent read spectrograms and a recording of the client. A fired shot and a real reload (the animation's own events) are manual steps. A client that joins during a stun was not tested. A client out of earshot when the stun starts does not get the buzz by walking closer.

### `LF_ConfiscateAction` — `Feature/Police/LF_ConfiscateAction.c`
- ✅ (2026-11-08, M1-12) Search in the player interaction menu: shown to on-duty
  police within the 2 m reach; deletes the weapons of the wanted player in
  front of them (the action owner), not the nearest wanted player. Weapons are
  collected before deletion. Earlier contract: on-duty police delete weapons
  from a wanted player.
  The offer reads replicated officer and target state; the deletion runs on the
  server only and re-checks the officer's account (verified 2026-11-07: dedicated/* tests, server side; client receipt is manual).
- ⚠️ No compensation or wanted reduction.
- ✅ **The search reads item legality** (#1764, `Feature/Catalogue/LF_ItemSearch.c`): every search, wanted or not, checks each carried item (inventory and equipped weapons) against the item catalogue (`LF_ItemCatalogue.OffenceFor` with the carrier's licences). An item that is an offence for that carrier is listed, dropped and deleted on the server, and each offence kind is filed once through `LF_WantedReports.Report` (`#LF-Offence_UnlicensedItem`, `_RestrictedItem`, `_IllegalItem`); the officer sees "Contraband found and seized". A pistol with `FIREARM_PERMIT` is left alone. An on-duty officer is not checked, because the police kit issues the catalogue's M9 without the civilian permit. Test `catalogue/search-legality`. Logs `[LFDebug:Catalogue] search of <uid> flags <prefab>: <offence>`.
- ⚠️ An off-duty officer still carrying the service M9 would lose it to a search; the kit needs its own prefab. The officer gets a count, not an itemised list on screen. A seized weapon's serial is not traced to its outlet yet (#1067).

### `LF_Restraint` and the handcuffs — `Feature/Police/LF_Restraint.c`, `Prefabs/Items/Roleplay/Handcuffs.et` (#664, 2026-11-13)
- ✅ **An on-duty officer holding handcuffs cuffs a downed, stunned, or held-up and unarmed player.** Two entries in the player interaction menu (`Character_Roleplay.et`, context `LF_ID`): **Cuff** (`LF_RestrainAction`), offered to an on-duty officer whose left hand holds an item with `LF_HandcuffsComponent`, and **Uncuff** (`LF_ReleaseRestraintAction`), offered to any on-duty officer facing a cuffed player. The server decides (`LF_Restraint.Restrain` / `Release`): the request goes through `LF_RateLimit` (`SPAWN`), the officer's real account must be on duty, the cuffs must be in hand (`HeldCuffs`: the gadget manager's held gadget, else the controller's left-hand attachment, both readable on the server), and the target must be downed (`LF_Downed`, a taser stun included) or compliant: held up (`LF_HoldUp.HeldUpAt`) within `m_iCompliantSeconds` (`LF_Police.conf`, default 60, 0 = downed only) and holding no weapon. Every refusal logs and toasts the reason. `DecideRestrain`, `DecideRelease` and `IsCompliant` are the pure rules (test `police/restraint-rules`).
- ✅ **The cuffs are consumed onto the suspect and come back.** Cuffing deletes the officer's pair and records its prefab against the prisoner; release puts a pair in the releasing officer's inventory (at their feet when full). An arrest at the cell (`LF_Jail.Arrest`) takes them off and returns them to the arresting officer; a cuffed body that dies (bleed-out, give-up) returns them to the officer who cuffed it; a prisoner who disconnects returns them too. Logs `[LFDebug:Police] restrained <uid> by <uid> (downed|held up, cuffs <prefab>)` and `restraint off <uid> (<why>); cuffs <where>`.
- ✅ **A cuffed player keeps walking but holds nothing.** The replicated flag `m_bLF_Restrained` on the character reaches every client and a late joiner; the owner's controller disables weapon controls, holsters the weapon and puts away a held gadget, and gives the controls back on release (test `police/handcuffs`). Every client with a renderer shows a cuff on the suspect's right wrist (`Handcuffs_Worn.et`, spawned locally and attached to the `righthandprop` bone the way the vanilla loiter props are, a ring turned onto the forearm; never replicated).
- ✅ **The handcuffs** (`Handcuffs.et`, a held gadget on the vanilla `Item_Base.et`) are the first asset of the pipeline's `gadget` kind: first a Meshy model (generation ids in `tools/assets/handcuffs/meshy.json`), since 2026-11-14 a Blender model with articulated strands (`tools/assets/handcuffs/model.py`, no credits). On-duty police get a pair in the DebugWorld police kit; one lies on the police desk (`Roadmap_Police_Handcuffs`). `session.mjs cuffs-proof` on two clients: clock in, down the suspect, hold the cuffs, cuff, both clients read the suspect cuffed, release, both read them free, the cuffs are back; a cuffed player who dies gives them back.
- ✅ **Held in a closed fist (2026-11-14).** The handcuffs, the lockpick (`LF_Lockpick.et`) and the pager (`LF_Pager.et`) borrow the vanilla MX991 torch's hold pose (`Flashlight_MX991.agr`, `MX991_player.asi`, `m_eAnimVariable MX991`) instead of the gauze graph, whose anim set has no hold clips (the hand stayed open under the item). The cuffs are gripped by their lock boxes with one ring above the fist and one below; the lockpick and pager stand upright in the fist. Dropped, all three lie flat (seen in a session); the lockpick and pager had `RestingUP Forward`, which stood the lockpick's length and the pager's width upright. Values come from each spec's `states` block (`tools\cli asset states <slug>`, `docs/asset-pipeline/states.md`). The cuffs' open state is built (2026-11-14, below).
- ✅ **Open cuffs (2026-11-14, `asset-states-followups`).** The handcuffs model is built in Blender with a swinging strand per cuff on a hinge bone (`strand_r`, `strand_l`, pivots exact, 70 degrees, `spec.anim.bones`). `LF_AssetAnimComponent` on `Handcuffs.et`: `SetActive(true)` opens both strands (replicated, late joiners included), `false` closes them; `LF_HandcuffsComponent.SetOpen(bool)` wraps it (server). The cuffing hold below calls it. Try: `tools\cli asset states handcuffs --shots` (the `state_use_*` pictures) or harness `itemon` / `itemoff <prefab>`.
- ✅ **Cuffing is a hold, and the cuffs open and shut in it (2026-11-14, `cuffing-open-state`).** Cuff and Uncuff are short holds (`Duration 1.8` and `1.5` on the actions in `Character_Roleplay.et`: the vanilla progress bar). `OnActionStart` asks the server (`LF_AskCuffing`, suspect as RplId, officer taken from the controller); `LF_Cuffing.Begin` re-runs reach and the rule, then opens what the hold opens: for Cuff the strands of the cuffs in the officer's left hand (`LF_HandcuffsComponent.SetOpen(true)`, the replicated `LF_AssetAnimComponent` flag, a click, `SOUND_LF_OPEN`); for Uncuff the cuffs on the suspect's wrists (the replicated `m_bLF_CuffsOpen` on the character, which every client turns into the swing of the local worn model, `Handcuffs_Worn.et` now carries the same two hinged strands). The server rechecks reach and the rule every 250 ms: either party walking out of reach, going down or dying, the cuffs leaving the hand, or a suspect no longer held up stops the hold, shuts the cuffs and toasts `#LF-Restrain_Interrupted` to the officer; letting go of the hold (`OnActionCanceled`) does the same. The end of the hold (`PerformAction`) is accepted only for a hold the server saw begin and only after 80 percent of the action's duration (`LF_Cuffing.HoldLongEnough`), so a client cannot cuff in an instant or skip the open phase. A finished Cuff swings the strands shut with a click (`SOUND_LF_CLOSE`, `Sounds/Items/Handcuffs`, generated with the sound stage: `tools/assets/handcuffs/sounds.json`), and 650 ms later the cuffs leave the hand and go onto the suspect (`LF_Restraint.Restrain`, unchanged); a finished Uncuff takes them off and back to the officer (`Release`). The pair is rechecked as the cuffs shut: a suspect who stepped away keeps the cuffs in the officer's closed hand. Tests `police/cuffing-rules`, `police/cuffing-open-state`; harness verb `cuffing <player> cuff|release start|cancel|pin|finish` (`pin` holds without the time-out for a picture, `finish` and `pin` behind the dev gate), `session.mjs cuffs-proof`. Logs `[LFDebug:Police] cuffing hold ...`. Not done: the officer's hands do not move on their own (no animation exists for it, #340), and the returned cuffs go back into the inventory shut, not into the hand.
- ✅ **A lock-picking set (2026-11-14).** `LF_Lockpick.et` keeps its GUID and now shows a stitched leather wallet with a hook pick and a tension wrench under two straps (`tools/assets/lockpick/model.py`, no credits), where Meshy's version read as a pen. Held, it is a bundle in the fist with the tool tips up; on the ground it lies flat.
- ✅ **Pager on the ground (2026-11-14).** Checked in a session: dropped, it lies screen-up with `ground.up Right`; nothing to change.
- ⚠️ **The cuffs do not show in the officer's hand.** Holding them raises the hand in the vanilla gauze pose the other held items use (the lockpick's offsets), but the session shot shows an empty hand; the hand offsets (`gadget.hand` in the spec, `HandAdditive*` on the prefab) need a pass in Workbench. Cuffing works the same.
- ⚠️ **No restrained animation.** The base game has no surrender, hands-up, cuffed or tied animation (`docs/research/animation-pipeline.md`: the ACE captives set is the candidate, #340); a cuffed player stands or lies in the normal pose with the cuffs on one hand. There is no surrender gesture either, so "compliant" means held up and unarmed. Cuffed players can still open their inventory, and the officer cannot lead them by the cuffs (escort is the vehicle escort). Disconnect rejoin rules for cuffed players are #691. The restraint is not saved: a server restart frees every cuffed player and the pairs on them are gone.

### `LF_Roadblocks` — `Feature/Police/LF_Roadblock.c`, `LF_RoadblockActions.c`, `Prefabs/Props/Roleplay/Roadblock/` (#1941, 2026-11-16)
- ✅ Police roadblock kit. An on-duty officer deploys the folded kit (`Roadblock_Kit.et`, 9 kg) as a composition (#1940, `LF_PlacementCompositions`): one barrier (`RoadBlock_01`) and four cones across the road, each piece on the ground, one server-checked confirm, one pick-up that folds all five back into the kit. Only on-duty police deploy or pick up (refusal logged `[LFDebug:Roadblock]`, toast `#LF-Roadblock_PoliceOnly`). A vehicle seen moving near the set that then stops is logged once (`stopped at the roadblock of <uid>`). Ghost of the whole set while Deploy is selected (`LF_RoadblockPreview`, behind the same dev-tools flag as the spike strip ghost). Test `police/roadblock`; look `grid:roadblock`; proof `session.mjs roadblock-proof`.
- 🧪 A roadblock clears itself (#2100). A set stands for `m_iExpiryMinutes` (default 20, `Configs/Police/LF_Roadblock.conf`, `LF_RoadblockConfig`), or until the officer who laid it goes off duty or leaves (their account is gone), checked every server tick. Each removal deletes every piece and logs `[LFDebug:Roadblock] cleared the roadblock of <uid>: <reason>`. Nothing persists. Test `police/roadblock-expiry`.
- ⏳ Not yet: the armory does not hand the kit out (the rank kits in `LF_Police.conf` and the pack conf are a follow-up), the red (blocked) ghost has no picture, and a set does not expire.

### `LF_SpikeStrips` — `Feature/Police/LF_SpikeStrip.c`, `Prefabs/Props/Roleplay/SpikeStrip/` (#646, 2026-11-13)
- ✅ Police spike strip. Every rank's armory kit carries the folded strip (`SpikeStrip_Item.et`, 6 kg).
  On the ground, Deploy spike strip (on-duty police only, `LF_RateLimit` SPAWN; through `LF_PlacementService.Confirm` since #1026, at the spot the officer's ghost showed) lays `SpikeStrip.et`
  (3.5 m) from 1 m in front of the officer: along their facing, or straight across the nearest road
  within 12 m when the AI road network has one. Its three ground points take the higher of terrain and
  what stands there; more than 0.25 m between them refuses (`#LF-Placement_NoGround`). Something standing in
  the footprint (a post, a car) or under it (a strip already laid, anything dynamic) refuses
  (`#LF-Placement_Blocked`, #928). The item is used up.
- ⚠️ Placement preview (#928 prototype, dev-tools worlds only, selecting the action is not automated): while Deploy is selected the officer sees a ghost
  of the strip where it will lie, blue when the server would accept it and red when not (`LF_SpikeStripPreview`).
- ✅ The server samples the vehicles within 9.75 m every 50 ms and tests each wheel's path since the last
  sample against the footprint, so a fast car cannot jump it. A crossing wheel's `SCR_WheelHitZone` (on the
  wheel slot entity) takes COLLISION damage to DESTROYED; vanilla `UpdateWheelState` then cuts the radius to
  80 %, the grip to 20 % and adds drag on every machine, so the car slows and pulls. A flat wheel is not hit
  twice. Logs `[LFDebug:SpikeStrip] punctured wheel N of <vehicle>`.
- ✅ Pick up spike strip (on-duty police) puts the folded item back in the inventory, or at the feet when full.
  The strip goes by itself after `LF_PoliceConfig.m_iSpikeStripLifetimeSeconds` (600). DebugWorld shows one
  on the police station verge (`Roadmap_Police_SpikeStrip`: `m_bArmed 0`, `m_bExpires 0`; an officer can still pick it up).
- Model: one Meshy link repeated by the asset pipeline's deployable kind (`tools/assets/spike-strip`).
  Tests `police/spike-strip`, `police/spike-strip-geometry`; proof `session.mjs spike-strip-proof`.
- ⚠️ Not persisted: a deployed strip is gone after a restart. The road-network alignment did not run in
  DebugWorld (no AI road network there); the strip follows the officer's facing.


### `LF_ConvictionRecord` — `Feature/Police/LF_ConvictionRecord.c` (#1914, 2026-11-16)
- 🧪 Every arrest (`LF_OnArrested` on the bus) stamps the player uid with the in-game day (the game
  clock's date as a day number, `DayOrdinal`) and counts it. `IsRecent(uid, days)` answers "convicted
  in the last N in-game days?" (the stamp day counts as day 0, so a window of 3 is the stamp day and
  the two after it); the firearm permit desk (#1766) refuses an application on it, with the window
  from `LF_LicencesConfig`. Server only, never replicated. Saved as the `convictions` payload (save
  version 36); vanilla persistence restores the clock, so a stamp keeps its age across a restart.
  A stamp dated after today (an admin set the clock back) still counts as recent. Pin test
  `police/conviction-record` (also the `wt e2e` restart proof).
- ✅ **Logging out wanted right after police contact rejoins in jail; downed rejoins at the hospital (#1972).** A suspect identification (ID check, caught in the act, report) stamps police contact on the character (`LF_PoliceContact`, server session state keyed by uid); a stop or search action may stamp it with `LF_PoliceContact.Stamp(uid, source)`. A wanted player who logs out within `disconnect.contactWindowSeconds` (default 120, `0` off) is handled like a cuffed logout by `LF_DisconnectRules.Decide`: the jail rule, the leaving-custody charge, a flagged logout row. Wanted past the window rejoins in place with the warrant. `LF_DisconnectRules.RejoinFor` is the pure rule for where the leaver rejoins: jail, hospital (a downed logout dies and wakes in a bed with the bill) or in place. Each logout logs `[LFDebug:Rejoin] <uid> rejoins <jail|hospital|in place>: <reason>`. The sentence is the existing jail payload, so the jail rejoin survives a restart. The pending hospital wake and the contact stamp are session state (a restart between a downed logout and the rejoin spawns at the plaza with no bill); the combat-logout body is #1973.
- ✅ **A short grace protects a new character (#1780, Arrival #1007, #794).** `LF_Grace` (`Feature/Moderation/LF_Grace.c`, config `Configs/Moderation/LF_Grace.conf`, `m_iMinutes`, core default 0 = off). While the account's persisted playtime (`LF_Playtime.Total`) is under N minutes and the character's crime record (`LF_CrimeRecord.Total`) is empty, the character cannot be held up (`LF_HoldUp.Attempt` returns `GRACE`) or arrested (`LF_JailArrestAction.PerformAction`), and may not rob (`LF_RobAction.Gate` refuses `REFUSED_GRACE`; `LF_RobWeaponAction.CommitWeaponRobbery`; a protected character also cannot hold anyone up). The first completed crime ends it for good. Nothing new is saved: a restart or relog neither restarts nor extends it. The server decides and the refusal toast names the grace (`LF-Grace_Refused`); nothing is replicated, so a client menu may still offer the action and the server refuses it. The clock is per account (playtime is), the end is per character (the crime record is). Pin test `onboarding/grace`.
- 🧪 (2026-11-16) **The flagship turns the grace on at 30 minutes (#2075, from #1780).** `LF_GraceConfig` reads an owner `grace` section (`minutes`, 0 to 600) over `LF_Grace.conf`; `server/flagship/config.json` sets 30, the core default stays 0. Test `config/respawn-grace`; `flagship-config-selftest` pins the live value.
