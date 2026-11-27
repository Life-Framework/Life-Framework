# Crime & Police

### `LF_Breach` — `Feature/Crime/LF_Breach.c`
- ✅ Configured charge opens an `LF_LockComponent` after the fuse and leaves it
  open until repair (verified 2026-10-11). `Arm(targetId, playerUid)` is the
  call later robberies use. Fuse 0 opens in that call. Wanted and the police
  notify fire once, at arm time, not when the offer refreshes. A second charge
  on an open target refuses. Police on duty can always repair (an off-duty police account cannot, #2539); the owning organization
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
- 🧪 (2026-11-23, #4156) **The rob kit chest is the only thing to use on it.** The Workshop photo drill (`PhotoScene_Bank_Drill`, #370) stood on the chest as a live `LF_Drill`: the F key on the chest offered its "Pick up", which a robber already carrying the kit drill got refused as too heavy (`#AR-Inventory_InfoTooHeavy`), and a fresh player got a free drill. It is gone. `validate-town-hand-tools` fails any hand tool (`LF_HandToolComponent` or a prefab inheriting one) placed in a town or island layer; selftest `town-hand-tools-selftest`. On a dedicated server with a remote client the kit drill takes quick slot 5, the slot press holds it and the vault drills 30 s, in the annex, the playtest town and Saint-Philippe (`session.mjs drill-quickslot-proof [--island]`, #4306).
- 🧪 (2026-11-23, #4197) **The robbery kit loadout is held by a test.** `LF_Test_RobberyKitLoadout` (WORLD, `crime/robbery-kit-loadout`): every item `LF_BankRobKit.Items()` lists resolves to a prefab that spawns as an inventory item, the drill is a `LF_HandToolComponent` with tool id `drill` (the one the vault drilling and the quick bar read) that spins and sounds, every tool a vault or store asks for (`LF_SafeTuning.DEFAULT_TOOLS`, the armoury keycard) has a kit item, the vault money bag prefab resolves, and one grant leaves the drill, keycard and charge carried. The live check on main (`drill-quickslot-proof`, 2026-11-23) passed: the kit needed no config change, so no `LF_RobberyKit*.conf` was added.

### Safe types — `Feature/Crime/LF_SafeType.c`
- Three safe tiers are data, not three robbery systems. `Configs/Crime/LF_SafeTypes.conf` holds one `LF_SafeTypeRow` per tier and a pack overlays it through its mission header (`LF_SafeTypes_Everon1989.conf`; add, replace, patch or remove a row by id, `LF_ConfigOverlay`). A vault or heist component names a row with `m_sSafeType`; `LF_BankVaultComponent` builds an `LF_SafeTuning` from its own attributes and `LF_SafeTypes.Apply` lets the row fill every value still at its class default. A value the prefab or layer sets to something else wins. A value set equal to the class default reads as "not set", so to pin such a value on a typed safe, change it by at least one step. An empty `m_sSafeType` is the old behaviour. An unknown id logs `[LFDebug:SafeTypes]` an error and the safe keeps its own numbers.
- The robbery itself is unchanged: `LF_Bank.Rob`, `LF_Breach.Arm`, `LF_VaultDrill`, the fence tools, the alarm and the wanted report all run as before; only where the numbers come from is new.

| Tier | `store_box` | `bank_room` | `reserve_vault` |
|---|---|---|---|
| In the world | small box safe in a shop (`LF_SafeBox.et`, the vanilla `Safe_01` mesh) | a vault room fitted into a bank (`LF_BankVault_RoomDoor.et`) | the big standalone vault (`LF_BankVault.et` and its kit); the later reserve building |
| Haul | 1 bag x 150 = 150 | 1 bag x 500 = 500 | 4 bags x 1000 = 4000 |
| Tool | none | drill or charge | drill or charge |
| Timer | 10 s fuse | opens at once (drill: 30 s held at the door) | opens at once (drill: 90 s held at the door) |
| Jam chance | 0 | 0 | 0.25 |
| Police on duty needed | 0 | 0 | 2 |
| Cooldown | 60 s | 60 s | 600 s |
| Alarm | `store` | `bank` | `bank`, loud |
| Action name | "Crack the box safe" | "Rob the bank" | "Break into the reserve depot" |

- `store_box` matches the placed shop tills: they ship with 0 police needed, 150 cash a haul and a 900 s cooldown (`LF_FuelTill_Attended.et`; it was 60 s, which paid a solo robber 150 a minute, #2418). `LF_RobAction`'s own code default is 2 police, which every placed till overrides to 0, so the tier follows the placed value. With no tool it opens after the fuse. `m_fDrillSeconds` is 10 only because the setter needs a positive value; no drill is accepted. The Everon pack patches the haul to 120 (1989 till takings). It also sets rung 2 of the robbery ladder (#4435): 1 police on duty and a 45 min cooldown (2700 s), so a shop safe never opens with nobody on duty and a second try inside 45 min is refused in words.
- `bank_room` is today's bank: the numbers `LF_BankVault.et` sets (1 bag, 500, 60 s, 0 police, no fuse, drill or charge, bank alarm, no jam, 30 s drill).
- `reserve_vault` is the big vault for a later "Federal Reserve" building (docs only; in-world text says "reserve depot"). The existing `LF_BankVault.et` prefab keeps its GUID, path and its own explicit numbers and sets no type, because it is the playtest town's bank today; a building that uses it as the reserve vault sets `m_sSafeType "reserve_vault"` on its placement and drops the explicit numbers.
- Test `crime/safe-types` (LOGIC) and `everon/safe-types` (pack leg). Log `[LFDebug:SafeTypes]`.
- **Store box in the pack** (#2331): `LF_Everon1989/Prefabs/Props/Safes/LF_StoreBox.et` inherits the core `LF_SafeBox.et` and names `store_box` itself, so a shop fit-out places the pack prefab and the pack's 120 cash takings apply. It swaps the mesh to the vanilla olive `CabinetMetal_01` steel cabinet with its own material, because the core `Safe_01` mesh renders plain white (#2728). The pack grid cell `store-box` (`Grid_store_box.layer`, cell c19r4) stands one till and one box beside it. Test `everon/store-box-safe` (pack leg, `LF_Test_EveronStoreBoxSafe`) asserts the box registers under its entity name with one bag, 120 a bag and a 10 s drill, and that a script-driven robbery with no tool and no police arms it and spills exactly that haul after the fuse. The general store on the island has one behind its counter (kit `general-store`, row `general-store-box-safe`, entity `Shop_GeneralStore_BoxSafe`, the vault id): it holds one haul (`m_iStock 1`) and restores it every 45 min (`m_fRefillSeconds 2700`), so the shop cannot be farmed. The supermarket (b66, slot `supermarket`) has the same box behind its till (kit `supermarket`, row `supermarket-box-safe`, entity `Shop_Supermarket_BoxSafe`, #4436): the same prefab, `store_box` row and vault values, one stand-up metre beside the clerk. The test also asserts the pack row says 1 police and 2700 s, that a robbery with nobody on duty is refused, and that a second try inside the cooldown is refused. Putting a box in each other shop is a later per-shop card.

### `LF_BigJobLock` — `Feature/Crime/LF_BigJobLock.c` (#4193, plan 0.3.0 Crime A6, decision N17)
- 🔧 One big job (robbery rungs 4 to 6: hospital pharmacy, bank vault, reserve depot; the prison break in 0.4)
  runs on the server at a time. `TryBegin(jobKey, holderKey, cooldownSeconds, now)` takes the server-wide lock
  or answers `BUSY` (another big job runs) or `COOLDOWN` (this job ended recently); `RefusalText` gives the
  player the reason in words, with the minutes left. `Finish` releases the lock and starts that job's own
  cooldown; another big job may start meanwhile. A lock never finished expires after 45 min into its cooldown.
  Callers pass the cooldown from their pack config (`COOLDOWN_PHARMACY` 2 h, `COOLDOWN_BANK` 3 h and
  `COOLDOWN_RESERVE` 6 h are the plan defaults).
- 🔧 **The bank heist takes it** (#4437): a configured heist job (`LF_HeistRun.FromRow`) is a big job keyed by its
  row id. Arming its door takes the lock (#4492): `LF_Breach.Arm` asks `LF_HeistRun.ArmLockRefusal` before the
  charge is taken (a refusal costs nothing and names the reason in `[LFDebug:Breach]`) and calls `ClaimAtArm`
  right after, so a second crew is refused inside the first crew's fuse window. The door opening (`DoorOpened`)
  adopts that lock, or calls `TryBegin` for a door opened with no arm; a refused run stays idle. A charge cancelled
  or repaired before its fuse runs out ends the job (`ArmAbandoned`: `Finish`, so the cooldown starts). The run
  ending (getaway or failure) or `Stop` mid-run calls `Finish`. The cooldown is the row's `m_iCooldownSeconds`
  (`LF_HeistRun.CooldownSeconds`; 0 keeps `COOLDOWN_BANK`, 3 h; the Everon bank row sets 10800). `ArmRefusal` (offer and server perform)
  refuses arming the door after the police minimum with `#LF-BigJob_Busy` or the cooldown in minutes, logged
  `[LFDebug:BigJob]` at most every 10 s. The lock lives on the server, so the client offer passes and the server
  perform sends the toast; its log line says the real reason (police, a big job running, or the cooldown; `ArmRefusalWhy`). Runs built from attributes take no lock. Tests `LF_Test_HeistBigJobLock`, `LF_Test_HeistArmLock`.
- 🔧 Save section `bigjob`: cooldowns as seconds left, so a restart never resets one; a job running at the
  save comes back as that job in its full cooldown, with the lock free. Test `LF_Test_BigJobLock`; `wt e2e`
  checks the cooldown across a real restart.

### `LF_Hospital` — `Feature/Crime/LF_Hospital.c`
- ✅ The hospital till is not scripted here: the placed action is `LF_RobAction` on the
  till's single `ActionsManagerComponent` (civilian, cooldown and minimum-police gates, cash
  payout, wanted, dispatch; DebugWorld places `Roadmap_Hospital_Till` at `257 0 256.4`, hospital porch,
  on `Street_Hospital.layer`). `LF_HospitalTillComponent` only registers the till place. The old
  `LF_Hospital.RobTill` and its helpers had only test callers and read a millisecond clock against a
  60-second cooldown; deleted in #2419.
- ✅ The cabinet is a forced-entry container. `BreachCabinet` calls
  `LF_Breach.Arm`. When that lock opens it spawns the configured medicine
  prefabs: field dressing, morphine, saline, and tourniquet. A missing prefab
  logs and that line is skipped. A second breach while the cabinet is open
  spawns nothing and does not alert again. `Roadmap_Hospital_Cabinet` is at
  `198 0 120`. Items stay ordinary; confiscation of a contraband flag belongs
  to the contraband scope.

### `LF_HospitalPresence` — `Feature/Crime/LF_HospitalPresence.c` (#4446, plan 0.3.0 Crime D2, rung 4)
- 🔧 (merged, in-dev) **The pharmacy cabinet needs one medic or two police on duty and a free big-job lock.**
  The cabinet action (a modded `LF_HospitalCabinetAction`) asks `LF_HospitalPresence.RefusalKey` on the
  server: too few on duty (`#LF-HospitalPresence_TooFew`), another big job running (`#LF-BigJob_Busy`) or
  the pharmacy cooling down (`#LF-BigJob_Cooldown` with minutes). The key is the action's disabled reason;
  the perform check says the same as a toast to a dedicated client, whose offer cannot count medics.
  Medics are counted on the server over connected players (`LF_MedicDuty.IsOnDuty`), police through
  `LF_AccountReplication.CountPoliceOnDuty`. A refusal is checked before the charge is taken.
- A breach that arms the cabinet takes `LF_BigJobLock` as `pharmacy`; `EndJob` runs a hold after arming
  (900 s, the fuse plus looting) and starts the 2 h cooldown. A restart in between saves the full cooldown.
- Tuning: `LF_HospitalPresenceConfig` (core defaults; Everon overlay
  `LF_Everon1989/Configs/Crime/LF_HospitalPresence_Everon1989.conf`): `m_iMinMedics 1`, `m_iMinPolice 2`,
  `m_iCooldownSeconds 7200`, `m_iHoldSeconds 900`. `LF_Hospital.CanBreach`/`BreachCabinet` stay the raw
  breach for scripted callers. Test: `LF_Test_HospitalPresence`.

### `LF_ClinicShortage` — `Feature/Crime/LF_ClinicShortage.c` (#4445, plan 0.3.0 Crime D1, decision N18)
- 🔧 (merged, in-dev) **The clinic is short for an hour after its drug cabinet is robbed.** A
  `LF_OnBreachOpened` whose target is a hospital cabinet starts it; a breach of any other target starts
  nothing. While short, the counters named in the config (`pharmacy_counter` by default) charge 150% of
  the buy price, and a revive held within 40 m of a hospital place (till or cabinet) takes 1.5 times as
  long. Then it ends on its own. Never a block on revives.
- Fair to medics: an on-duty medic restocking at the counter pays the normal price (the counter sells
  only to `MEDIC_ACCESS` holders, so the raise would otherwise fall on them); the sell price is unchanged,
  so buying dear and selling back is never a profit; duty pay, revive bills and bonuses do not change.
  A second robbery while short restarts the clock at the full duration and never stacks a second hour.
- `LF_ClinicShortage.IsActive()` and `SecondsLeft()` are the server accessors the door notice (CR-D8)
  reads. One bool replicates on the game mode (`RplProp`, bumped only on change) so a client's revive
  hold and price list match the server.
- Tuning: `LF_ClinicShortageConfig` (duration, price percent capped at 300, revive percent capped at 200,
  clinic radius, outlet ids). The core field defaults are the shipped values; the pack lists
  `LF_ClinicShortage_Everon1989.conf` in its mission header overlays.
- **Not persisted:** a server restart ends the shortage.
- Test: `crime/clinic-shortage` (`LF_Test_ClinicShortage`).
- 🔧 (merged, in-dev) **The notice on the clinic door (#4448, 0.3.0 Crime D8).** The only signal of the shortage: no toast. A paper notice on the clinic front wall (kit row `lf-clinic-shortage-notice`, prefab `LF_ClinicShortageNotice`, sign `clinic_shortage`) prints PHARMACY CLOSED with three rows, Why: Stock stolen, Medicine: Prices raised, Until: Further notice, from the replicated shortage flag (`LF_ClinicShortage.IsActive()`, local row source `clinic_shortage`, `Feature/Signs/LF_ShortageNotice.c`). When the shortage ends `LF_ShortageNoticeComponent` takes the paper off the door (clears the mesh flag, polling every 2 s on a client). The client holds only the flag, not the end time, so the paper says "until further notice". Test `signs/shortage-notice`; harness `clinicshort on|off|state` forces it for a session picture.

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
- 🧪 (2026-11-16) **Killing a logged-out body is murder of that player (#2103).** The body (`LF_LogoutBody`) has no controlling player, so its kill arrives with `victimPlayerId` 0. `LF_Murder.VictimUid` / `VictimIsPlayer` resolve it through `LF_LogoutBody.UidOfBody`: the victim is the account's uid, the `LF_OnPlayerKilled` event is published with it, and the officer exemption reads that account's wanted level. A plain NPC stays an NPC. Test `crime/murder-logout-body`.
- 🧪 (2026-11-18, #2670, W5 of #1022) **A war kill is not murder.** When the killer's and the victim's active characters belong to two organizations at war (`LF_OrgWars.IsAtWar`) and the war rules keep `warKillsAreMurder` off, `LF_Murder.OnCharacterKilled` skips the death with `LF_Murder.SKIP_ORG_WAR`: no wanted, no crime report, no dispatch, `[LFDebug:Murder] death of <uid> is not murder: org war ...`. The `LF_OnPlayerKilled` event carries `m_bMurder` false. With `warKillsAreMurder` on it is murder like any other. A safe zone is untouched: it refuses the damage before any kill, and a war never lifts it. See `docs/features/core.md` for the `war_kill` incident. Test `org/war-kill-tag`.
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
- 🔧 **A held robbery refused by the grace period says so** (#2157, #2075, merged, in-dev): when `LF_RobAction.Gate` refuses a new character (`REFUSED_GRACE`), `LF_RobHold.Start` and the `RobRefused` dialogue hook send the grace toast (`LF_Grace.Notify`) through `LF_RobHold.NoteRefusal`, and the attendant's caption (`LF_RobHoldRules.RefusalKey`) names the grace instead of "no till". Test `crime/rob-hold-grace-toast`.
- 🔧 **Vaults and breaches refuse a character in its grace; only police on duty repair** (#2539, merged, in-dev): `LF_Bank.Rob` (drill and charge start) and `LF_Breach.Arm` (every breach, before the charge is taken) refuse a character `LF_Grace.Refuses` protects and send the grace toast. `LF_Breach.PlayerMayRepair` accepts a police account only while it is on duty, so an off-duty police alt cannot relock a vault to rob it again. `LF_Bank_StaticResetHook` drops the vault table at world end. Test `crime/vault-grace-repair`.
- 🔧 **A fused vault charge opens the vault and pays once** (#2541, merged, in-dev): the fuse timer (`LF_BreachTargetComponent.FireFuse`) advances by the seconds `LF_Breach` scheduled (the safe type's fuse; the component's own `m_fFuseSeconds` is 0 on a vault) and, on a bank vault, goes through `LF_Bank.FuseFired`, which opens the lock and spawns the pending haul once. A second fuse call does nothing. A breach target on a vault entity no longer configures the breach itself, so the vault's safe-type fuse is not reset to 0. Test `crime/vault-charge-fuse`.
- 🔧 **A robber holds one till at a time; a downed or dead attendant ends the hold unpaid** (#2540, merged, in-dev): `LF_RobHold.Refusal` refuses a robber who already keeps a hold running (`REFUSED_HOLDING`, caption `#LF-RobHold_Refused_Holding`). Each tick reads the attendant: down or dead turns a running, paying or police end into `LF_ERobHoldEnd.CLERK` (`LF_RobHoldRules.JudgeClerk`), which pays nothing (only `PAID` opens the till, `Pays`), shows `#LF-RobHold_Fail_Clerk` and leaves the attendant silent. Test `crime/rob-hold-one-per-robber`.
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
- ✅ **Weapon and vehicle robberies are judged on the server (#2538, merged, in-dev).** `CommitVehicleRobbery` and
  `CommitWeaponRobbery` take the police count, the action's minimum, cooldown, last-rob time and now, and refuse (nothing
  delivered, no wanted, `LF_Debug` warn plus a `NotifyEntity` toast) a robber in grace (the vehicle had no grace check),
  a faction that may not rob, too few police on duty, or a robbery inside the cooldown. The pure `Gate` static
  (`LF_RobVehicleAction.Gate`, `LF_RobWeaponAction.Gate`) is what the client's `CanBePerformedScript` and the server both
  ask; the offers now read `LF_AccountReplication.GetPoliceOnDutyCount` (the old per-action counter is gone).
  `PerformAction` judges against the previous rob time and does not start the server's cooldown for a refused robbery.
  `crime/rob-commit-gates` (WORLD) pins it.
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
  `Street_Jail.layer` (the desk on `Street_Police.layer`). The playtest town jail is
  two-layer like the island's (#4336): `Roadmap_Jail_Door` (the yard gate) is the cell
  door, and a short net-fenced pen outside it ends in `Roadmap_Jail_OuterGate` (same
  `EL_JailDoor` prefab, `LF_BreachTargetComponent` target, named by the cell's
  `m_sOuterDoorTargetId`) between two lock-only wing doors. The cells refuse a charge
  until the outer gate is blown, and a released prisoner appears beside the pen. The placed door is
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
  **The escort car works on any police car (#3431, merged, in-dev).** Until then the escort path was
  bookkeeping only: the put-in and take-out actions were on no Everon prefab, the on-foot detain action
  (`LF_JailEscortArrestAction`) was on none either, nothing registered a vehicle, and no one was
  physically seated. Now `LF_UAZ469_Police.et` carries `LF_JailPutInVehicleAction` and
  `LF_JailTakeOutOfVehicleAction` (door context `door_l01`) so every police UAZ, the Everon station kit's
  `PoliceKit_PatrolCar` and `PoliceKit_EscortCar` included, offers them, and `Character_Roleplay.et` carries
  "Detain for escort" on the interaction menu. The officer detains a wanted suspect, stands at the car and
  puts them in: `LF_JailEscortCar.PutIn` (server) checks the hold and that the suspect is within 8 m, then
  moves them into a real cargo seat through `MoveInVehicle` (the owning client answers; a lost request is
  asked again up to 4 times, a refused one rolls the hold back to on foot and the officer is told). The
  car's escort id is its placed name or its entity id, so a car spawned with no name works; seat rows
  are made on demand. Take-out at the placed cell starts the sentence (`LF_JailEscort.Arrive`); anywhere
  else the suspect steps out 2.5 m beside the car and stays held on foot. The hold is server state, so
  neither action refuses from the client: the server judges and toasts the reason. Test `jail/escort-car`
  (LOGIC); proof `session.mjs police-arrest-proof --escort [--car <entity>]` on two clients reads the seat on both.
  A suspect held for escort, cuffed or not, is refused the driver's seat like a cuffed one
  (`LF_CuffEnforcement.HeldFromDriving`), so an uncuffed suspect cannot slide over and drive the police car
  off; a seat retry stops once the hold has left the car. Test `police/escort-no-driving` (LOGIC).
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
- ✅ **Island work stations (#3419, #3754, merged, in-dev).** The Saint-Philippe jail slot (`Place_saint-philippe_jail.layer`, kit
  `tools/town/kits/jail.json`) carries the three 1989 stations from #3418 inside the villa's barred cell: the laundry tub on
  the west side, the kitchen sink beside the cell door and the broom leaned in the north-east corner, all on the ground floor.
  #3419 first put the tub and sink in an upstairs room and the broom behind the villa; a jailed prisoner could reach none of
  them, because `LF_Jail.Confines` pulls them back to the cell origin when they are more than the cell's 2 m zone away (and the
  cell door is locked). The rows now stand within the zone, clear of the villa's own bunk bed, benches and wash basin
  (`town fit --refit`, no terrain row any more; `audit placement --strict` clean, seated on the tile floor). Checked by the
  interior `look place:saint-philippe/jail`; not walked as a booked prisoner (manual step in the PR). No script or save change.
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
  The prisoner's platform uid is not replicated (#2416, test `jail/slip`): the slip keeps it in a plain
  server-side field and replicates the display name (never the uid: a prisoner with no character name reads
  "unknown", `#LF-JailSlip_Unnamed`) and a printed flag.
- ✅ **Visible release** (M1-21, tests `jail/slip`, `jail/online-time`): release opens the cell
  door (`DoorComponent.SetControlValue`, the vanilla door motion and sound; it closes after
  5 s), places the prisoner at the cell release offset (`m_vReleaseOffset`; DebugWorld `0.15 0 10`, outside
  the door), stamps the slip, then shows the toast 1.5 s later. Release runs once per sentence.
  The DebugWorld cell now sentences 60 s per wanted level, max 300 s (it was zero-length).
- ✅ **Jailbreak** (#235, LOGIC `roadmap/jailbreak`, WORLD `e2e/roadmap-jailbreak`, `session.mjs jailbreak-proof`). A serving prisoner who is outside the cell volume while `Roadmap_Jail_Door` is open has escaped: `LF_Jail.NoteEscape` (called each cell tick) reports it once through `LF_Dispatch.Report` as `#LF-Offence_JailBreak` (so `LF_OnCrimeCommitted` is on the bus, the board and pager carry it) and leaves the sentence and wanted level alone; going back inside re-arms it. The door reuses `LF_Breach` (arm, stay open until police repair). **Fixed:** `m_bDebugLocked` was never applied (the component asked for no init event), so the placed jail door started unlocked; `LF_LockComponent` now sets the INIT mask and applies it, and the per-component `m_bStartLocked` workarounds (jail, breach, lockpick) are gone. Harness verb `charge` hands the player a breach charge (dev gate). No save change.

- 🔨 **Booking at the jail (arrest flow review, 2026-11-22, merged, in-dev).** `LF_JailBooking` (`Feature/Police/LF_JailBooking.c`).
  The suspect's `LF_JailArrestAction` was "Send to jail" from anywhere within 5 m of any wanted player, no cuffs; it is now
  two booking options on `Character_Roleplay.et`, **Book into jail: full sentence** (`m_iPercent 100`) and **Book into jail:
  reduced (cooperated)** (`50`, at least 30 s, never above the full one; `LF_Jail.Arrest(..., percent)`), offered only when the suspect
  is cuffed and the officer stands within 15 m (`BOOKING_METERS`) of the placed cell. The cell notes its origin on every machine
  (`LF_JailBooking.NoteCell` in `LF_JailCellComponent.OnPostInit`) so the client offers it there; `LF_JailBooking.Book` (server)
  re-checks place and cuffs (or the officer's escort hold) and toasts the refusal (`#LF-JailBook_NotHere`, `#LF-JailBook_CuffFirst`).
  **Escort needs the cuff:** "Detain for escort" is now **Escort** and refuses an uncuffed suspect (client reason and `LF_JailEscort.Grab`),
  so the hands-up / downed rules of the cuff gate the whole chain. **Take out of vehicle** no longer books silently at the cell: the
  suspect steps out beside the car and the officer books with a chosen option (`#LF-JailBook_AtStation` toast at the station).
  **Waypoint:** cuffing a wanted suspect puts a "Jail booking" waypoint on the officer's client through the existing job waypoint RPC
  (`LF_MapShareComponent.ServerJobWaypoint`, no new RPC); booking takes it off. The officer's first cuff in a session shows the how-to
  note (`#LF-JailBook_FirstArrest`). **Prisoner clock:** `m_iLF_JailSecondsLeft` (RplProp on the character, written from
  `HandSlip`/`RefreshSlip`/`Release`, bumped only every 15 s or on a jump) drives a one-line HUD strip, "Jail: m:ss left"
  (`UI/Layouts/HUD/LF_JailClock.layout`), counted down locally between writes; harness `seen` prints `jailclock=`. Nothing new is saved.
  Test `jail/booking` (WORLD: refusals, reduced length, held in the cell, HUD seconds, release); session proof
  `session.mjs arrest-flow-proof` (clock in, wanted, cuff, waypoint, escort, car, booking at the cell, HUD clock; `--wait <s>` the release).

### `Feature/Police/LF_PoliceMenu.c` (arrest RPC and `LF_PoliceUtils`)
- ✅ The duty NPC has no menu (#2535). The `LF_PoliceMenu` menu class, its layout,
  the `PoliceMenu` preset and `LF_OpenPoliceMenuAction` are gone: duty is the duty
  desk (`LF_DutyAction`), the wanted list is the station board, a fine is a paper
  ticket (`LF_FineTicket*`), and #2509 had already removed the blank Search and
  Arrest buttons, leaving a sheet with only Close. `PoliceDutyNPC.et` stays as a
  DebugWorld prop with no actions; `LF_Test_PoliceDutyNpcNoMenu` holds that. The
  arrest RPC and `LF_PoliceUtils` stay for the planned wanted-list sheet.
- ✅ **Client→server bridge works** (verified 2026-08-30): the arrest request goes
  through `SCR_ChimeraCharacter.LF_AskPoliceArrest`, and the server
  handler `RpcAsk_LF_PoliceArrest` validates on-duty officer and wanted
  target before acting. Arrest also requires the configured
  arrest rank (patrol on the flagship, #165). `LF_PoliceUtils` (pure) holds the
  sentence-length math and the reach check for tests; `MAX_FINE` is the
  senior ticket cap.
  The client-side self-promote RPC is gone (#2416): it only ever refused and logged every call; `LF_PoliceRanks.RequestSelfPromotion` stays as the server rule for a promote that names the caller.
- ✅ **Arrest starts a sentence** (verified 2026-10-11): the arrest RPC calls
  `LF_Jail.Arrest` after the rank gate and no longer clears wanted or teleports
  to `"127 1 137"`.
- ✅ **Hands-on only** (2026-11-07): arrest refuses a
  target that is not connected or stands beyond `LF_PoliceConfig.ArrestReach()`
  (`m_fArrestReach`, owner key `arrestReach`, 5 m by default, the in-world arrest range) and shows `#EL-Police_TooFar`. Neither the
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
- 🔨 **Ticket request limit and a private paper** (#2416, in-dev). `RpcAsk_LF_WriteTicket` counts each request against
  `LF_FineTicketRole.AllowWriteRequest`: `ticketRateBurst` (3) per `ticketRateWindowSeconds` (10, at most 60) per officer (police
  config; `LF_RateLimit.AllowWithAt`, kind `TICKET`). The first refusal in a window logs `[LFDebug:RateLimit]` and toasts
  `#LF-RateLimit_Slow`; the rest are counted into one line when the window rolls over. The paper's replicated fields
  (`LF_FineTicketComponent`) no longer include the holder's platform uid, which every client streaming the item could read;
  the display name stays and the uid lives in the server's `LF_FineTicketRecord`. Test `police/ticket-request-limit`.

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
  a toast. The clock-in toast (`#LF-Duty_ClockedIn`, 6 s, police and medic desks) says the
  clothes, cash and things wait in the duty locker until clock-out: the carried cash goes into the
  stash with the rest, and before 2026-11-27 the readout dropped to $0 with only "You are now on duty"
  (medic playtest). Clock-out still reads "You are now off duty". Death on duty ends the shift: the body keeps the police gear, the
  civilian stash becomes the carried snapshot the respawn restores, and the
  next shift starts from the starter kit. Going on duty starts the rank clock;
  going off duty settles it. `PoliceStation_Debug` and `PoliceDutyNPC` no
  longer offer duty. The debug menu's police button still sets the role without
  a swap (dev only).
- ✅ **Duty pay at the desk** (verified 2026-11-08: LOGIC test `jobs/duty-pay`;
  the 2-client cash check is manual). Going off duty pays the shift's on-duty
  seconds times the rank's `m_fPayPerSecond` (a negative rank rate defers to `m_fPoliceDutyPayPerSecond`, `Configs/Jobs/LF_Jobs.conf`, core 0.5; the flagship's ladder pays 0.0972 for a cadet up to 0.125 since 2026-11-22, see money-economy.md) as cash through `LF_DutyPay.PayAtClockOut`, with the pick-up sound and a
  `#EL-Duty_Paid` toast. Seconds settled by an armory draw mid-shift are kept
  for the clock-out. A second clock-out, a clock-out with no clock-in, and a
  non-police account pay nothing. Unpaid seconds are session state: a restart
  during a shift drops that shift's pay, it never pays twice.
- 🔧 **Clock-out names the locker cash apart from the pay** (#5148, crime playtest 2026-11-27; merged, in-dev). The
  clock-in side is the `#LF-Duty_ClockedIn` toast (#5157: clothes, cash and things wait in the duty locker). At
  clock-out the money HUD flashed the locker cash and the shift pay as one rise; now `#LF-Duty_LockerCashReturned`
  ("Your $100 cash is back from your duty locker.") is its own line beside the `#EL-Duty_Paid` pay toast.
  `LF_DutyLocker.LockerCash(characterId)` counts the cash in `civilian_stash` (legacy cash prefabs too, not marked
  notes); `LF_DutyAction` reads it before clock-out empties the locker. No cash, no toast.
  Pay is unchanged. A restart mid-shift keeps the locker cash once: `wt e2e` saves an on-duty officer with $100 in the
  locker and after the restart finds $100 in `civilian_stash`, none in the carried police gear, and gets it back once at
  clock-out (`keepCash` off still strips it, `persistence/keep-stash`). WORLD `police/duty-locker-cash`.
- 🔨 **Duty pay and rank count only a capped, connected shift** (#2416, in-dev). `LF_PoliceRanks.SettleDutyAt` counts at
  most `dutyShiftMaxSeconds` (police config, 10800 = 3 h; 0 = no cap, #2622) of one clock-in for pay and rank (`ShiftCredit`); the clock runs on
  uncounted until the officer clocks out and in again, so an AFK officer earns at most one capped shift per visit to
  the desk. A disconnect settles and stops the clock (`LF_PoliceRanks.OnPlayerLeft`, from `OnPlayerDisconnected`): the
  officer stays on duty with the body and kit, but the time away is never counted. `OnPlayerReturned` (on spawn) starts
  the clock again inside the same shift cap, which also restarts the clock of an officer whose on-duty flag survived a
  server restart. The cap applies to every duty role (police and medic). Log `[LFDebug:Ranks] shift cap reached`,
  `duty clock stopped`, `duty clock resumed`. A connected officer who stands idle is paused by the idle sampler below. The shift's counted seconds are session
  state: a server restart gives a fresh cap. The cap log fires once per shift. Tests `police/duty-shift-cap`,
  `police/duty-clock-disconnect`.
- 🔨 **A dedicated server sees an officer who only looks around** (#3154, in-dev, proof `duty-idle-proof`). The idle
  sampler reads the server's copy of a remote client's aim, and that copy follows the client: on a dedicated session a
  client on duty that only pitched its view for 360 s kept the clock running, and the same client standing still was
  paused about 330 s later.
- 🔨 **Duty pay pauses while the officer or medic stands idle** (#3120, in-dev, WORLD `police/duty-idle`). `LF_DutyIdle`
  samples every connected on-duty player every 30 s. Moving more than 2 m (on foot or in any vehicle; a seat follows the
  vehicle's position) or turning the view about 10 degrees counts as active, so patrol, a desk, a parked car with the
  officer looking out and driving to a call all keep the clock running. After `dutyIdleSeconds` (jobs config, default
  300; 0 = off) with none of that, the clock is settled at the moment the window ran out and stopped; the idle time earns
  no pay and no rank time. The next sample that sees activity starts it again (at most 30 s late). Rank rates are
  untouched. A logout while paused needs nothing: the spawn restarts the clock (`ResumeDutyClock`). Watches are runtime
  only. Log `[LFDebug:DutyIdle] duty clock paused`, `duty clock resumed`.
- 🔨 **A character switch settles the duty clock first** (#2838, in-dev, LOGIC `police/duty-clock-character-switch`). Every
  change of the account's active character (a pick, a new character made active, the active one removed) runs
  `LF_PoliceRanks.SettleForCharacterSwitch` before it: the running seconds go to the outgoing character's rank time
  and the shift's pay, and the clock stops. Logout already settled at the disconnect. Log `[LFDebug:Ranks] duty clock
  settled for <uid> (character <key>) before a character switch`. Medic rank time follows the same settle once medic
  ranks (#2837) land.
- 🔨 **Administrators set or demote a police rank** (#2838, in-dev, LOGIC `police/rank-admin-set`). Chat
  `/policerank <account uid> <rank>` (`LF_PoliceRankAdmin`). The server checks the caller (engine administrator or
  staff roster level `administrator`, as for application decisions), the account and the rank (0 to the top rung).
  It settles a running duty clock, sets the active character's rank and logs `rank set by administrator <caller>:
  <uid> (character <key>) rank <old> -> <new> (promotion|demotion|unchanged)`. A demotion restarts the character's
  on-duty seconds at the start of the new rank, so the next accrual does not promote them back. Refusals are logged
  and shown to the caller. Medic ranks are not covered yet (#2837 is not merged).
- 🔨 **A rank change settles the shift at the old rank; a demotion takes kit above the new rank** (#2874, in-dev,
  LOGIC `police/duty-pay-rank-change`, WORLD `police/armory-demotion-strip`). Every rank change (a promotion at the
  station, a time promotion, `/policerank` up or down, a medic time promotion) first settles the duty clock, then
  restates the unpaid seconds earned so far at the new rank's rate (`LF_DutyPay.AfterRankChange`, rounded up, logged
  `rank change settled the shift: <s> unpaid s at rate <old> carried as <s2> s at rate <new>`), so clock-out pays each
  part of the shift at the rank held for it. One settle that crosses a time-promotion threshold is split there
  (`LF_PoliceRanks.SecondsToNextRank`). No save change and no money moves before clock-out. A `/policerank`
  demotion takes off the officer every issued armory item the new rank's offer gate refuses and turns its serial in
  (`LF_PoliceRanks.StripKitAboveRank`, logged `kit above rank <r> taken from <uid>`, toast); an offline officer loses
  it at their next spawn. Plain kit lines with no serial (spike strip, roadblock kit) are not tracked and stay.
- 🔨 **Outcome bonuses, capped per hour by rank, paid with the shift** (#4210, in-dev, LOGIC `jobs/duty-bonus-cap`).
  `LF_DutyBonus.Award(account, kind)` adds a fixed amount to an on-duty account's pending bonus: a call resolved
  $15, a booking $10, a stolen car $20 (police), a revive $10, a delivered patient $15 (medic). The hour window opens
  at the first bonus; inside it the total stops at the rank's cap (police $100/$100/$100/$95/$70, medic
  $40/$50/$50/$40; the plan's economy table), logged `bonus +$<n> (<kind>) ...` or `bonus capped for <id>`. Clock-out
  (`LF_DutyPay.PayAtClockOut`) pays the pending bonus as cash beside the shift pay (ledger reason `duty-bonus`); a
  bonus that does not fit is kept for the next clock-out, and clocking out does not reopen the hour's cap. Fines and
  bail never reach this ledger. `LF_MedicDutyBonus.Install` awards revives and deliveries from the bus; a bed delivery's own revive is
  refunded when its delivery follows, so the pair pays $15.
  **Live in play since #4319** (LOGIC `jobs/duty-bonus-wiring`, PERSISTENCE `jobs/duty-bonus-persist`): `LF_GameModeRoleplay`
  calls `LF_MedicDutyBonus.Install()`. `LF_DutyBonus.AwardUid(uid, kind)` is called by `LF_PoliceCallPay.Settle` (a fine at
  the scene completes the call) and `LF_CallOutcome.Close` with RESOLVED (no one found and unfounded earn nothing) for
  the $15, `LF_JailBooking.Book` for the $10 and `LF_StolenVehicles.Recover` for the $20 (not when the officer reported
  the car themselves). The ledger is the `dutybonus` save section (`LF_DutyBonusSavePayload`, rows of account id,
  pending, window start, window earned), so a restart keeps the unpaid bonus and the hour's count.
- 🧪 **Alpha: the station desk approves on submit** (rc 0.2.0, LOGIC `police/alpha-instant-approval`). Owner key
  `police.alphaInstantApproval` (off by default; on in `server/flagship/config.json` for the alpha only) makes
  `LF_PoliceApplicationForm` take the playtest path (`LF_PoliceApplicationRules.ApprovesOnSubmit`): the application is filed
  without the playtime gate or cooldown and `ApproveInstant` puts the account on the roster with no trial, so any player
  can apply at the duty desk and clock in without an administrator. Log `approved on submit: police.alphaInstantApproval
  is on (alpha only)`. Turn it off before the server opens to the public (`server/flagship/README.md` "Alpha pacing").
- 🔨 **An expired trial ends the shift** (#2416, in-dev). `LF_PoliceTrialSweep` runs on the server every
  `trialSweepSeconds` (60): it expires trials (`ExpireTrials`), then clocks out each connected on-duty officer whose
  trial ended unpromoted and who may no longer clock in (`LF_PoliceApplications.TrialExpiredFor`, roster check), through
  the desk's own `LF_DutyLocker.ClockOut` (shift pay, gear swap) and toasts `#LF-Duty_TrialExpired`. The sweep settles
  each connected officer's clock before expiring, so seconds that earn the promotion end the trial first. An officer
  who was offline, or on the spawn screen with no body, is left alone until a sweep finds them with a body; an admin
  who puts them back on the roster keeps them on duty. Log `[LFDebug:Police] trial of <uid> ended on duty`. Tests `police/trial-expiry-duty`, `police/trial-expiry-clock-out`.
- 🧪 **A duty press off the roster is refused by the server, logged and toasted** (rc 0.2.0, LOGIC `police/duty-desk-roster-refusal`): `LF_DutyAction.ClientRefusal` greys only a missing account and the wrong desk; a player off the police roster now reaches `LF_DutyLocker.ClockIn`, which logs `clock-in refused for <uid>: #LF-Duty_NotOnRoster` and the desk toasts "Only officers on the police roster can clock in. Use Apply at this desk to join". Before, the client greyed it and nothing reached the server or the player.
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
  the nearest map settlement (`LF_NamedPlaces.NearestSettlement`, #1944, #5147, WORLD
  `police/dispatch-names-place`: "bank robbery, Saint Philippe"; a site label such as
  "Factory" never names a place, the fuel station beside it read "Factory" on the island),
  joined with the nearest sign title within `m_fPlaceRadius` when there is one ("Fuel
  Station, Saint Philippe": stored `<site>^<town>`, `LF_WantedBoard.ResolvePlace` reads
  it with `#LF-Wanted_PlaceAt`). With no sign the town alone names it (#3092: the bank's
  fascia sign must not replace the town); with no settlement the sign alone, then the
  nearest site label, then a 100 m grid square. The place is stored as a
  localization key and localized where each board and radio line is built; the
  core holds no place names. The evidence bag's read line (`LF_EvidenceBagComponent.ReadLine`) and the
  evidence locker's read card (`LF_EvidenceLocker.DescribeAll`) go through `ResolvePlace` too; before the
  pre-RC review (part 4) they printed the stored `<site>^<town>` as is (test `investigation/evidence-place-text`). In town the hospital till's alarm stands on `Hospital_AlarmBox` (an alarm mount on the hospital front wall, `Street_Hospital.layer`) and the pack names the town "Saint Philippe" (`Place_SaintPhilippe`, town pass #1947).
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
- 🔧 **What the clothes hide** (#2438, merged, in-dev): a witness report reads
  `LF_ItemCatalogue.Concealment` over what the suspect wears (`LF_WantedLook.WornPrefabs`,
  `LF_WantedReports.ApplyConcealment`). A `concealsBuild` item (the green raincoat) adds
  "build hidden" and a `concealsFace` item adds "face hidden" to the clothing list, so the
  dispatch call, witness statements, the wanted board and the posters say so. A hidden face
  leaves no portrait (`LF_WantedLook.Witnessed`: the poster keeps its empty photo frame); a
  hidden build keeps it, since the portrait is head and shoulders and already wears the
  coat. The photo ID still calls `LF_WantedLook.Capture` and keeps the full face. Test
  `police/wanted-look-conceal`.

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
- 🔧 **The alarm shows on everyone's map** (merged, in-dev): a robbery report at a till, vault, clinic cabinet or armoury rings that place's map badge (red pulsing ring while in progress, grey "robbed recently" after); see `docs/features/map.md` "Robbery alarm on the place badges". Read from the dispatch's police call; no new robbery logic.
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
  logged and skipped, and the rank stays usable. An on-duty officer promotes a
  lower one (rule in the promotion bullet below). 3600 seconds on duty promotes patrol. `RequestSelfPromotion`
  is the self-promotion RPC body and always refuses. Rank does not change
  faction and does not write the police whitelist. Lower ranks still open the
  wanted list. DebugWorld places `Roadmap_Police_Armory` at `241.2 0 244.2`
  (police station forecourt) on `Street_Police.layer`.
- 🔨 The flagship ladder (#1769, merged, in-dev; `LF_Police_Everon1989.conf`): five ranks, Cadet, Patrol,
  Senior Patrol, Sergeant, Lieutenant. Fine caps 500, 1000, 2500, 4000, 5000; pay 0.0972, 0.1042, 0.1111, 0.1181, 0.125
  per on-duty second ($350 to $450 an hour since 2026-11-22, #4178: the 0.3.0 pay ladder makes police the lowest steady pay; 0.25 to 0.35 from 2026-11-18, 0.3 to 1 before); duty to next 1h, 2h, 4h, 8h. A saved rank is the rung's index and a shipped rung cannot move, so
  the pack patches the core's `patrol` (index 0) into Cadet and `senior` (index 1) into Patrol and appends the other three.
  Titles are the pack's `LF-Rank_*` rows. A saved rank above the ladder clamps to the top on load.
- 🔨 Only an officer above the new rank promotes (#2095, in-dev). `LF_PoliceRanks.Promote` (server) refuses
  unless the on-duty promoter's rank is above the rung the target moves into: on the five-rung flagship ladder a
  Patrol (1) cannot promote a Cadet (0) into Patrol, a Sergeant (3) promotes a Patrol into Senior Patrol, and a rank
  equal to the new rung is refused. The top rank is the ceiling (`PromoterRankNeeded` = `min(target + 2, top)`), so
  the Lieutenant still promotes into Lieutenant and a one- or two-rung core ladder keeps "the senior promotes the
  patrol". A refusal logs `[LFDebug:Police]` and toasts the promoter `#EL-Rank_PromoteNeedsHigher`. The armory
  action's server check (`CanPromote`) hides Promote from a rank that can promote nobody;
  the nearest-officer pick prefers someone the promoter may promote. Test: `police/promote-rung`.
- 🔨 **The station Promote action finds officers in play** (#2416, in-dev): `FindLowerRankNearby` read only bodies
  tied by `BindOfficer`, which only tests call, so every real promote was refused "no lower-rank officer nearby". It
  now lists the connected players (`LF_PromoteCandidates`: each controlled body's account id and position) within
  `PROMOTE_RADIUS` (5 m). Test: `police/promote-finds-players`.
- 🔨 The flagship kit (#1942, merged, in-dev; `LF_PoliceKit_Everon1989.conf`, listed after the ladder in the
  mission header): the pack overlay restates each rung's `m_aKit`. Patrol (index 0) draws the M9 and the spike strip;
  senior and every rung above draw the M16, the spike strip and the roadblock kit (`LF_Roadblocks.KIT`, added to the
  station catalog in `LF_PoliceRanks.Catalog`, since a kit line outside it is not issued). A civilian cannot draw.
  Test: `police/everon-kit`.
- 🔨 **Radar gun with three speed classes** (#4211, PO-R4, merged, in-dev): `LF_RadarGun.et` (a vanilla binoculars
  mesh until the Blender model lands) carries **Measure speed**, offered to on-duty officers. The server traces the
  officer's aim up to 150 m for a vehicle, reads its velocity in whole km/h and classifies it against the road's limit
  (`LF_RadarRules.Classify`): 1 to 15 over is **minor** (`road.speeding.minor`), 16 to 30 **road**
  (`road.speeding`), over 30 **serious** (`road.speeding.serious`); at or under the limit is clear. A toast shows
  speed/limit and the class. The limit is the 50 km/h default (`LF_RadarLimits`) until the speed signs (PO-R5) give
  per-road limits. The gun is not yet issued in a kit. Test: `police/radar-classes`.
- 🔨 **One issued kit per officer, two hours apart, saved** (#2416, in-dev; replaces the 2026-11-07 per-shift
  draw mark, which clocking out and in reset). `LF_ArmoryKits` books each kit's serialled lines (the M9, the M16)
  to the officer's account (all its characters share one book) with the issue time (`System.GetUnixTime`). While any serial is out, the armory action issues
  nothing: an officer carrying the kit hands it in (the items are removed, `#EL-Rank_KitReturned` or
  `#EL-Rank_KitPartReturned`); with none of it carried the first use warns `#EL-Rank_KitOutstanding` and a second
  use within 15 s reports it lost (each serial logged `[LFDebug:Armory] <uid> reported serial <s> lost`,
  `#EL-Rank_KitReportedLost`). A new kit also waits `armoryCooldownMinutes` (police config, 120) of real time after
  the last issue (`#EL-Rank_DrawCooldown`). The book is the `armorykits` payload (save version 45), so clock-out and
  restart keep both. Plain lines (spike strip, roadblock kit) carry no serial and are not booked. Tests:
  `police/armory-kit-cap`, `police/armory-kit-turn-in`, `police/armory-cooldown-persist` (with a `wt e2e` reload
  check).
- ✅ Rank, on-duty seconds, and unpaid duty seconds survive a restart (M1-10,
  `progression` payload, save version 7).
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
  `LF_Contraband.IsNearDropActive()` and `LF_ConvoyMarkerComponent.FindDrop(bool)` choose it; the inactive cage hides its
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
- ✅ **A taser hit is police contact; the stun statics reset at world end (#2543, 2026-11-17).** `LF_Stun.Apply` calls `LF_Stun.StampContact`, which stamps `LF_PoliceContact` ("stun") on the victim when the shooter's account is an on-duty officer, so a wanted player who is tased and logs out falls under the logout rule, as with cuffing. A stun from anyone else stamps nothing. `LF_Stun` and `LF_StunBuzz` each have an `LF_StaticResetHook` (`LF_Stun_StaticResetHook`, `LF_StunBuzz_StaticResetHook`): the stun counters, the buzz counters and the playing map clear; the audio sources die with the world. Test `police/stun-contact`.
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
- ✅ **The duty M9 is lawful police issue** (#2096, Refs #1979): the armory and the patrol kit hand out `Prefabs/Weapons/Police/Handgun_M9_Service.et` (`LF_PoliceRanks.SIDEARM`), a child of the serialled `LF_ArmoryM9` and so of the base-game M9. The catalogue lists it as `pistol_m9_service`, legality `legal`, so an off-duty officer searched with it keeps it and nothing is filed; the plain `pistol_m9` without `FIREARM_PERMIT` is still seized. The pack's `police_armory/pistol_m9` offer issues the service prefab and the kit confs list it. Test `catalogue/service-pistol-lawful`.
- ✅ **The officer reads what was seized** (#2154, Refs #1978): the search toast is "Seized from <name>: AKS-74U, 2x 30-round magazine", one line per item kind with a count in front when more than one, cut to three kinds and "and N more" (`LF_ItemSearch.SearchAndSeize` out list, `LF_ItemSearch.SeizedText`; test `LF_Test_SearchSeizedList`). A search that took only chain goods keeps "Contraband found and seized".
- ⚠️ A seized weapon's serial names its outlet (#2153).

### `LF_Restraint` and the handcuffs — `Feature/Police/LF_Restraint.c`, `Prefabs/Items/Roleplay/Handcuffs.et` (#664, 2026-11-13)
- 🧪 **The live game mode carries the duty kits** (rc 0.2.0, WORLD `spawn/roleplay-duty-kits`): `GameMode_Roleplay.et` (the island and every core server) now holds the POLICE kit (flat cap, dark turtleneck, black trousers, officer harness, boots, ALICE pack; M9 and two magazines, station car key, pager, handcuffs, radio) and the MEDIC kit, which only `GameMode_Roleplay_Debug.et` had. Before, an island officer clocked in with the generic civilian items (`no kit configured for role POLICE`) and every cuffing was refused `NO_CUFFS`.
- ✅ **An on-duty officer holding handcuffs cuffs a downed, stunned, or held-up and unarmed player.** Two entries in the player interaction menu (`Character_Roleplay.et`, context `LF_ID`): **Cuff** (`LF_RestrainAction`), offered to an on-duty officer whose left hand holds an item with `LF_HandcuffsComponent`, and **Uncuff** (`LF_ReleaseRestraintAction`), offered to any on-duty officer facing a cuffed player. The server decides (`LF_Restraint.Restrain` / `Release`): the request goes through `LF_RateLimit` (`SPAWN`), the officer's real account must be on duty, the cuffs must be in hand (`HeldCuffs`: the gadget manager's held gadget, else the controller's left-hand attachment, both readable on the server), and the target must be downed (`LF_Downed`, a taser stun included) or compliant: held up (`LF_HoldUp.HeldUpAt`) within `m_iCompliantSeconds` (`LF_Police.conf`, default 60, 0 = downed only) and holding no weapon. Every refusal logs and toasts the reason. `DecideRestrain`, `DecideRelease` and `IsCompliant` are the pure rules (test `police/restraint-rules`).
- 🧪 **Hands up counts as compliant for the cuffs** (rc 0.2.0, LOGIC `police/restrain-hands-up`): `LF_Restraint.IsCompliantNow` reads `LF_Restraint.Complies`, which also passes a target with their hands up (`SCR_ChimeraCharacter.LF_IsHandsUp`) and no weapon. Before, a suspect who raised their hands to an officer's aim could be searched (`LF_HandsUpGate`) but not cuffed unless they had been held up in a robbery-style hold-up. Armed never complies; `compliantSeconds` 0 still limits cuffing to downed players.
- ✅ **The cuffs are consumed onto the suspect and come back.** Cuffing deletes the officer's pair and records its prefab against the prisoner; release puts a pair in the releasing officer's inventory (at their feet when full). An arrest at the cell (`LF_Jail.Arrest`) takes them off and returns them to the arresting officer; a cuffed body that dies (bleed-out, give-up) returns them to the officer who cuffed it; a prisoner who disconnects returns them too. Logs `[LFDebug:Police] restrained <uid> by <uid> (downed|held up, cuffs <prefab>)` and `restraint off <uid> (<why>); cuffs <where>`.
- ✅ **A cuffed player keeps walking but holds nothing.** The replicated flag `m_bLF_Restrained` on the character reaches every client and a late joiner; the owner's controller disables weapon controls, holsters the weapon and puts away a held gadget, and gives the controls back on release (test `police/handcuffs`). Every client with a renderer shows a cuff on the suspect's right wrist (`Handcuffs_Worn.et`, spawned locally and attached to the `righthandprop` bone the way the vanilla loiter props are, a ring turned onto the forearm; never replicated).
- ✅ **The server enforces the cuffs (#2417, 2026-11-17, `Feature/Police/LF_CuffEnforcement.c`).** The owner-side weapon block above is UX; a modified client ignores it, so the server refuses a cuffed character's requests itself. Damage whose instigator is cuffed is swallowed in `HijackDamageHandling` on the character damage manager (whose vanilla hook does not call super), on `SCR_DamageManagerComponent` (vehicles, props) and at the top of `LF_Stun.TryHit` (darts); a cuffed character's own fall and bleeding already running are not refused. A cuffed character who reaches a driver's seat (`SCR_CompartmentAccessComponent.OnCompartmentEntered`, which fires on the server) is put out of it the next frame, and a driver who gets cuffed is put out at once; cargo seats stay allowed for prisoner transport. Till (`CommitRobbery`, `REFUSED_CUFFED`), held till (`LF_RobHold.Refusal`/`Start`), vehicle, weapon and vault robbery, `LF_Lockpick.BeginAs`, and the interaction menu's give cash, give item and role actions (`LF_PlayerInteraction.ServerAllows`) are refused; introduce stays open. Each refusal logs `[LFDebug:Police] refused <what> from cuffed <uid>` and toasts `#LF-Restrain_NotWhileCuffed`. Tests `police/cuffs-server-rules`, `police/cuffs-server-refusals`. A cuffed driver is put out beside the vehicle: a body this machine owns here, a remote-owned body through its owner (the `LF_Jail` dismount path), asked again every second up to 5 times; a running lockpick or held robbery ends when the cuffs land. Not covered: vanilla item use (eating, a medkit), opening another storage while cuffed, and damage to mines or destructible buildings (their vanilla hooks never reach the base one).
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
  A spot in a door leaf's swing (1 m both ways) or the 3 m walk-up in front of a ground-floor door is refused the same way, "a door is in the way" (#2445, `LF_PlacementDoors`, for every placement: single, aimed and composition).
  A kind with a zone in `Configs/Placement/LF_PlacementPolicy.conf` is also refused outside it (#2447, `LF_PlacementZones`): roads only, inside a house the player owns or holds a key to (the house nearest the spot, so not the neighbour's), or within a radius of their organization's base (`LF_Places` kind `org_base`, registered by the leased hideout since #2504: `LF_Hideout.SyncOrgBase` on lease, load and release, last lease wins, server only; test `hideout/org-base`). The server's `Confirm` asks `LF_PlacementPolicy.ZoneAt`; core ships no zoned kind. Test `placement/policy-where` (WORLD).
  A kind with a cost in the same `.conf` (#2448, `LF_PlacementCost`) pays it on the server only after the spot is VALID, once: cash through `LF_MoneyUtils` (ledger reasons `placement`, `placement-refund`) or items used up; a player who cannot pay is refused (`#LF-Placement_NeedCash`, `_NeedItem`) and keeps everything, a failed spawn gives it back, and a refundable kind refunds once on pick-up to the payer only (compositions, and spike strips through `LF_SpikeStrips.PickUp` since #2502; another officer folding a paid strip up refunds nobody). Core ships every kind free; a pack sets prices. Tests `placement/policy-cost`, `police/spike-strip-refund` (WORLD).
  A PERMANENT placeable (`LF_PlaceableComponent` lifetime, default temporary) is saved (#1032, `LF_PlacementSave`, payload `placements`, save version 52): prefab, spot, owner key, kind, property, never an EntityID; a restart spawns each row once (a second apply makes no duplicate; a row whose prefab no longer resolves is skipped with an error). The server refuses one past 40 per owner or 25 per property (`m_iOwnerCap`, `m_iPropertyCap` on the policy conf; toasts `#LF-Placement_CapOwner`, `_CapProperty`). The owner within 4 m picks it up (`LF_PlacementPickup.PickUp`): entity and row go, a refundable cost comes back, the item goes into the inventory. A placed prop carries `LF_PlacedOwnerComponent` (the placer's character key, replicated, stamped by `LF_PlacementSave` on record and on load) and `LF_PlacementPickupAction` ("Pick up", `#LF-Placement_PickUp`): shown and performable only for that character, and the server's `PickUp` decides again from the saved row (owner, 4 m), so a client that sends it anyway changes nothing (#2751). Tests `placement/persist` (PERSISTENCE, restart through `wt e2e`), `placement/cap`, `placement/pickup-action` (WORLD).
- **House furniture (#2654, slice of #747):** four neutral carried items (chair, table, shelf, lamp: `Prefabs/Placeables/Furniture/Furniture_*_Item.et`, vanilla props placed) are PERMANENT, need an owned or key-held house (`HOUSE_OWNER_OR_KEY` plus the `OWNED_PROPERTY` zone in the policy conf, kinds `Furniture_*`) and use the `m_eSnap` modes. FLOOR lies like GROUND (the ground probe reads a building floor). WALL puts the shelf flush on the wall face the player aims at (`LF_PlacementFurniture`: face normal is the heading, back edge 1 cm off the face, floor from a probe at the middle and front edge); the server refuses a WALL proposal more than 2 cm off a wall. Three five-line hooks in `LF_PlacementService` (Plan, PlanAimed, Confirm). Test `housing/furniture` (WORLD). Each item places a `Furniture_*_Placed.et` (the vanilla prop plus `RplComponent`, the owner stamp and the "Pick up" action, #2751), so the owner can take it back; a chair saved before #2751 comes back as the plain prop with no action. Prices and the 1989 catalogue are pack work.
  **In a cottage the player built (#3173):** a finished build-site plot counts as a property for both the house rule and the `OWNED_PROPERTY` zone. A spot (or, for the rule, the player) within 8 m of the shell origin (`LF_BuildSiteComponent.GetShellOrigin`, `LF_PlacementZones.PLOT_RADIUS_M`) belongs to the plot, keyed by the plot key; its owner (read from `LF_Properties` on the server on every call, so a sale takes effect at once) and any character carrying a key bound to the plot may place there, a stranger may not. A live house whose origin is nearer the spot than the shell origin wins, so a cottage beside a neighbour's house never claims that house (`LF_PlacementZones.HouseNearerThanPlot`). An unfinished plot is no property. A client, which has no owner key, lets the prompt show over any owned finished cottage and the server's Confirm decides. The property cap counts per plot. Test `placement/on-built-plot` (WORLD).
- **House furniture, home set (vanilla props only, 2026-11-18):** fourteen more carried items furnish a home (`tools/furniture/items.json`, written by `tools/furniture/gen.mjs`): bed, double bed, sofa, armchair, coffee table, rug, television, radio, potted plant, mirror, picture, stove, bookcase, workbench. Each is built on a base-game prop (the footprint comes from the model's measured bounds, the carried item keeps the vanilla prefab's material override such as the poster's print), is PERMANENT, needs an owned or key-held house like the first four, and places a `Furniture_<Id>_Placed.et` with the owner stamp and "Pick up". The radio keeps its vanilla turn-on action. The mirror and the picture are WALL items with `m_fHangHeight` (1.3 m and 1.5 m): `LF_PlacementFurniture.ProbeWall` lifts the origin that far above the floor at the foot of the wall, on the client's ghost and on the server's recomputed spot alike; 0 (the default, the shelf) stands on the floor. A vanilla prop's pivot is often a corner (wardrobe, stove, bookcase), so the ghost extends from the aimed point; the footprint is exact. Pack: price rules (buy 20 to 260, sell about 40 percent of it) in `LF_Everon1989/Configs/Shop/`, and all furniture, the first four included, on a new `furniture` stock in `LF_ShopStock_Everon1989.conf` for Saint-Philippe's furniture store (the building plan picks the building). The general store keeps selling the original four (chair, table, shelf, lamp) until the furniture store's counter lands, so nothing disappears in between; the other fourteen sell only from `furniture`. Proof `everon/furniture-home` (WORLD, pack leg: stocked, priced above sell-back, permanent, floor or wall snap, hang height, house-owner policy, owner stamp and Pick up on the placed prop). Pictures: `website/public/docs-images/2654/furniture-items-after.jpg`, `furniture-showroom-after.png` (grid cell `furniture-vanilla`, flat ground: a placed prop in a real house room and the hung mirror and picture stay manual).
- **Storage furniture (vanilla props only, 2026-11-18, draft: REVIEW-NEEDED):** seven more items with a real inventory, from the same `tools/furniture/items.json` (rows with `storage`): wardrobe (120 L, 60 kg), dresser (80 L, 40 kg), cupboard (90 L, 50 kg), chest (60 L, 40 kg), crate (50 L, 40 kg), fridge (70 L, 30 kg), kitchen unit (100 L, 50 kg). The placed prefab adds `SCR_UniversalInventoryStorageComponent` (capacity and item-size limit), `LF_FurnitureStorageManagerComponent`, `LF_PersistentStorageComponent` with `m_bKeyAssignedLater` and `LF_FurnitureStorageOpenAction` (the vanilla open-storage action, offered only to the placer). Sold from the `furniture` stock like the rest. Contents persist with the placement and add no save section: `LF_PlacementSave` calls `LF_PlacedStorage.Attach` on a fresh placement and when it spawns a saved row, which gives the storage the stable key `furniture:<placement id>` (`LF_PersistentStorageComponent.AssignKey`), and the contents ride the existing carried payload as a placed-slot `LF_StorageSnapshot` record, restored a frame after the spawn and again after a load (idempotent). A placement that still holds items is not picked up (toast `#LF-Placement_EmptyFirst`); an empty one is, and its record is cleared. `LF_FurnitureStorageManagerComponent.HandleAccessValidation` keeps a stranger's client request from moving items out. Tests `housing/furniture-storage` (WORLD: components, item-size limit, ten apples, key, Open for the placer only, refused pick-up), `housing/furniture-storage-save` (PERSISTENCE: cash, apple and water bottle survive delete plus load, a second load adds nothing, an empty one is picked up and its record goes), `everon/furniture-storage-stock` (pack leg: stocked, priced above sell-back, house-owner policy). A real restart, a second player at the open furniture and the inventory screen are manual.
- ⚠️ Placement preview (#928 prototype, dev-tools worlds only, selecting the action is not automated): while Deploy is selected the officer sees a ghost
  of the strip where it will lie, blue when the server would accept it and red when not (`LF_SpikeStripPreview`).
- ✅ The server samples the vehicles within 9.75 m every 50 ms and tests each wheel's path since the last
  sample against the footprint, so a fast car cannot jump it. A crossing wheel's `SCR_WheelHitZone` (on the
  wheel slot entity) takes COLLISION damage to DESTROYED; vanilla `UpdateWheelState` then cuts the radius to
  80 %, the grip to 20 % and adds drag on every machine, so the car slows and pulls. A flat wheel is not hit
  twice. Logs `[LFDebug:SpikeStrip] punctured wheel N of <vehicle>`.
- 🔨 **Spike strip cap** (#2416, in-dev): an officer may have `spikeStripMaxPerOfficer` (police config, 2) of their own
  strips deployed at once, counted by deployer (`LF_SpikeStripComponent.CountDeployedBy`; the strip placed in a world
  layer is never counted). `LF_PlacementService.Confirm` asks `LF_SpikeStrips.CapProblem` before anything is spawned or
  used up: the next strip is refused (`[LFDebug:Placement] refused: <uid> already has N spike strip(s) deployed (cap M)`,
  toast `#LF-SpikeStrip_Cap`) and the item stays. Picking one up or its lifetime running out frees a place. Test
  `police/spike-strip-cap`. The cap keys on the strip prefab (`LF_SpikeStrips.STRIP_PREFAB`).
- ✅ Pick up spike strip (on-duty police) puts the folded item back in the inventory, or at the feet when full.
  The strip goes by itself after `LF_PoliceConfig.m_iSpikeStripLifetimeSeconds` (600). The town has no
  loose strip: the police armory hands out the kit (town pass #1947; the grid cell `roadblock` keeps the film and test spot).
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
- ✅ **Logging out wanted right after police contact rejoins in jail; downed rejoins at the hospital (#1972).** A suspect identification (ID check, caught in the act, report) stamps police contact on the character (`LF_PoliceContact`, server session state keyed by uid); a person search (`LF_ConfiscateAction`, found or clean) and a completed cuffing (`LF_Cuffing.FinishCuff`, only when `LF_Restraint.Restrain` returned OK) stamp it too with `LF_PoliceContact.Stamp(uid, "search"|"cuffing")` (#2102). A wanted player who logs out within `disconnect.contactWindowSeconds` (default 120, `0` off) is handled like a cuffed logout by `LF_DisconnectRules.Decide`: the jail rule, the leaving-custody charge, a flagged logout row. Wanted past the window rejoins in place with the warrant. `LF_DisconnectRules.RejoinFor` is the pure rule for where the leaver rejoins: jail, hospital (a downed logout dies and wakes in a bed with the bill) or in place. Each logout logs `[LFDebug:Rejoin] <uid> rejoins <jail|hospital|in place>: <reason>`. The sentence is the existing jail payload, so the jail rejoin survives a restart. The pending hospital wake and the contact stamp are session state (a restart between a downed logout and the rejoin spawns at the plaza with no bill); the combat-logout body is #1973.
- ✅ **A short grace protects a new character (#1780, Arrival #1007, #794).** `LF_Grace` (`Feature/Moderation/LF_Grace.c`, config `Configs/Moderation/LF_Grace.conf`, `m_iMinutes`, core default 0 = off). While the character's own persisted played time (`LF_GraceCharacterClock`, #2443) is under N minutes and the character's crime record (`LF_CrimeRecord.Total`) is empty, the character cannot be held up (`LF_HoldUp.Attempt` returns `GRACE`) or arrested (`LF_JailArrestAction.PerformAction`), and may not rob (`LF_RobAction.Gate` refuses `REFUSED_GRACE`; `LF_RobWeaponAction.CommitWeaponRobbery`; a protected character also cannot hold anyone up). The first completed crime ends it for good. Nothing new is saved: a restart or relog neither restarts nor extends it. The server decides and the refusal toast names the grace (`LF-Grace_Refused`); nothing is replicated, so a client menu may still offer the action and the server refuses it. Both the clock and the end are per character. Pin test `onboarding/grace`.
- 🧪 (2026-11-17) **Each new character gets its own grace (#2443, #2075 item 3).** The grace clock is the character's own played time: `LF_Playtime.Settle` also credits the account's active character in `LF_GraceCharacterClock` (save version 48, payload `graceclock`), so a second character on an account with hours played is protected for its own first N minutes. An older save seeds each existing character with its account's playtime, so the upgrade gives nobody a fresh grace. Test `onboarding/grace-per-character`.
- 🧪 (2026-11-16) **The flagship turns the grace on at 30 minutes (#2075, from #1780).** `LF_GraceConfig` reads an owner `grace` section (`minutes`, 0 to 600) over `LF_Grace.conf`; `server/flagship/config.json` sets 30, the core default stays 0. Test `config/respawn-grace`; `flagship-config-selftest` pins the live value.
- ✅ **The armory issues from its outlet by rank** (#1763, Refs #1066): an offer of the `police_armory` outlet with `issue` set is handed out free (no money moves) as its serialled twin (`issuePrefab`, `LF_ArmoryM16.et`), and a serial is booked to the officer in the book (`LF_FenceSerials`). `LF_PoliceRanks.IssueItem` and the `Draw` kit lines read the offer's job and rank gate; a patrol officer is refused the rifle and the toast names the senior rank (`#EL-Rank_IssueNeedsRank`). Test `LF_Test_ArmoryOutlet` (`catalogue/armory-rank`). ⏳ `LF_PoliceRank.m_aKit` still lists which lines a rank carries (the spike strip, the roadblock kit, the pistol); retiring it is a follow-up card.
- ✅ **A serial can name the outlet that sold the weapon** (#1765, Refs #1067): `LF_OutletSerial.Mint(outlet, n)` writes `LF-<outlet>-0007` and `LF_OutletSerial.Outlet(serial)` reads the outlet back; `LF_FenceSerials.Issue(uid, prefab, outlet)` books it. The outlet lives inside the serial string, so no record changes shape (save version unchanged) and an old `LF-0001` serial parses to no outlet and still loads. Test `LF_Test_OutletSerial` (`catalogue/outlet-serial`). ⏳ The armory issue path (`LF_PoliceRanks.IssueOffer`) and a shop weapon sale do not pass their outlet yet: a follow-up one-line call each, held back so this slice stays out of the police-issue files.
- ✅ **The armory and the gun counter stamp their outlet into the serial** (#2153, Refs #2123, #1009): `LF_PoliceRanks.IssueOffer` books the issue with the offer's outlet (`LF-police_armory-0007`). A sale at a shop that is not a fence, of a catalogue weapon sold under a licence-gated offer (the gun counter; the black market and back room are unlicensed and stay unserialled), goes through `LF_FenceSerials.OnWeaponSale` and books `LF-gun_counter-0012` to the buyer; the weapon slot is searched as well as the bags, since a bought weapon lands there. A fence sale still publishes `LF_OnFenceSale`, now with `m_sOutlet`; only fence sales take that path. Test `LF_Test_OutletSerialWired` (`catalogue/outlet-serial-wired`). ⚠️ A weapon prefab with no `LF_SerialComponent` cannot carry a serial: the sale stands and `[LFDebug:Serials]` warns. The gun counter's items are still the plain vanilla prefabs, so they need serialled twins in the catalogue before a counter sale reads a serial in play.
- ✅ **The armory's AKS-74U carries a serial** (#2152, Refs #2118): the pack's rank-2 issue offer for `rifle_aks74u` sets `issuePrefab` to `LF_ArmoryAKS74U.et` (the vanilla rifle plus `LF_SerialComponent`), so the issue is booked to the officer instead of logging "untracked". The black-market PM is not issued by the armory, so it has no twin. Test `LF_Test_EveronArmorySerials` (pack leg, `everon/armory-serials`).
- ✅ **The gun counter's PM and M21 carry serials** (#2166, Refs #2153, #1009): the pack's `pistol_pm` and `rifle_m21` catalogue items point at `LF_CounterPM.et` and `LF_CounterM21.et` (`LF_Everon1989/Prefabs/Weapons`, the vanilla weapon plus `LF_SerialComponent`, the `LF_ArmoryM9` pattern), so a licensed counter sale is booked as `LF-gun_counter-NNNN` instead of warning "a weapon prefab needs LF_SerialComponent". The Vz.58 is no live counter offer (the weapons overlay removes it) and has no twin. A plain vanilla PM or M21 a player already carries no longer matches a catalogue row. Test `LF_Test_EveronGunCounterSerials` (pack leg, `everon/gun-counter-serials`).

### Police station kit (#2201 town pipeline)
- 🧪 **The 1989 police station as a reusable kit** (2026-11-16): the vanilla `Villa_E_2I01_Police` fitted out as a working
  post in the GridWorld claim `police-station-kit` (c3r1, 2x2), written by `tools/buildings/police-station/fitout.mjs`
  (building-space rows, three cumulative passes: BUILD, POLISH 1, POLISH 2) and shot by `looks.mjs`; `spec.json` is the
  concept and the rooms, `proof.json` the shot list and the booking walkthrough. The public hall has the licence and
  permit desk (`LF_LicenceDesk`) and the wanted board; the duty room has the duty desk (clock in), the armory locker and
  `LF_ArmoryStore`, the evidence locker and the dispatch board; the records room has the case board; the villa's own barred
  cell is the holding cell (the cell door decision is under POLISH 2 below).
  Patrol cars, a spike strip and the payphone stand in the yard. Budget (maintainer): at most 6 dynamic lights
  per building and 1 shadow caster, 300 entities for the building with its furniture, static vanilla props, no per-frame scripts; `fitout.mjs --report`
  prints the counts. Saint-Philippe placement is later, through `town fit`.
- 🧪 **Police station POLISH 2** (#2275, merged, in-dev). The villa is now the pack prefab `Prefabs/Buildings/PoliceStation/PoliceStation.et`
  (`tools/buildings/police-station/shell.mjs`): the vanilla villa with its furniture cut from 231 props to 143 (the attic's junk,
  16 light switches, repeats) and its ceiling lamps given a lit cover, so the building totals 215 of the 300 entities (MEDIUM).
  The facade carries an `LF_Sign_Fascia` over the loggia with the pack card `police_front` ("Gendarmerie"; the name is in
  the pack table) and two wall lamps whose glass is lit with no light of their own; the sixth light is one warm glow under
  the loggia. The hall has a front counter facing the door, its notices and the benches either side of the duty-room door;
  the cell has its rules pinned by the door; the yard has a bench, a wall bin, planters, cones and the escort car.
  Every prop height is snapped by `surface.mjs`; `tools\cli building check police-station` reads the kit's layer rows too
  (`spec.json` `check_layer`). **The cell door, decided:** the kit always shows the villa's own barred cell and door and
  carries no jail component, because one placed cell is allowed per world and GridWorld has one. `fitout.mjs --with-cell`
  (for `town fit` into a world with no cell) stages only the `LF_JailCell` marker, its zone fitted to the room (2 m) and the
  release point in the hall; no `EL_JailDoor`, which would hang a second door in the villa's doorway.
  **The villa's own cell door is the breach target (#2373, in-dev).** `PoliceStation.et` overrides the cell's door leaf
  (`DoorSet_PrisonCell_L_EXT_B` > `Door_PrisonCell_E_A_Left_INT_ST_B`, written by `shell.mjs`) with `LF_LockComponent`
  (locked at spawn), `LF_BreachTargetComponent` id `PoliceStation_CellDoor` and the arm and repair actions, the same
  parts `Roadmap_Jail_Door` has, so `--with-cell` writes `m_sDoorTargetId "PoliceStation_CellDoor"` and a prisoner is
  held while the door is locked, a charge breaches it and the jailbreak path of #235 applies, and police repair it. A cell
  placed by hand in a world (the island town) sets that same id on its `LF_JailCellComponent`. One station per world:
  two stations would share the id. Test `everon/police-cell-door` (WORLD, pack leg).
- 🧪 **Saint-Philippe jail cell** (#2969, town draft, draft PR, in-dev). The police slot's `JailCell` marker (`LF_PlaceComponent`) is only
  a marker: it switches the Jail system on and leaves `LF_Jail` with no cell, so an arrest had nowhere to send the prisoner. The island
  now has the real cell: the `jail` slot (a street-style slot that rides in the police slot's building, `with: police` in
  `tools/town/templates/town.json`, kit `tools/town/kits/jail.json`, layer `Place_saint-philippe_jail.layer`) puts one
  `LF_JailCellComponent` (prefab `EL_JailCell`, zone 2 m, release point 3 0 2.5 in the cell's frame, no door target) in the villa's own
  barred cell on the ground floor, west. `LF_Jail` finds it by component (`LF_Places` kind `JailCell`, id `cell`; one per world),
  so arrests, sentences, the zone and release run on the island exactly as in DebugWorld. Breach and jailbreak on it still need
  the villa door to carry `LF_BreachTarget` (#2373). `tools\cli town check <place>` now warns (an error under `--complete`) when
  a slot promises `JailCell` and no fitted layer carries `LF_JailCellComponent`, so the marker alone cannot pass again.
- ✅ **Map area data** (#2233, epic #2223, test `map/areas`; node twin `tools/map/rules/areas.mjs`). `LF_MapAreas` (`Feature/Map/LF_MapAreas.c`) turns an area into an outline vertex list: a polygon is its points, a circle a 32-gon, a box its four corners turned by yaw the way `LF_ZoneArea.Contains` turns it, a polyline corridor a mitred strip. `Collect` merges every `LF_Zones` zone that has a style (SAFE and LAWLESS draw; NORMAL draws nothing) with the map-only areas (`LF_MapAreaComponent`: town line, gated, custom; a polygon local to the entity, or a circle). Styles are tokens for the `map-*` theme rows (M6). The outline, centroid, style and rule cases are one table, `Configs/Map/Rules/areas.cases.json`, that the Enforce test and the emulator's node rule both run; `LF_MapAreaCases.generated.c` carries it as script because the game cannot read a loose `.json` from an addon (`node tools/map/rules/areas.mjs --gen` rewrites it; the selftest fails when stale). `LF_Zones.GetAll()` is the one hook in the zone service. Nothing draws yet (M6).
- 🔧 **Crime review fixes** (2026-11-17, `docs/reviews/2026-11-17-crime.md`, merged, in-dev):
  - **Till robbery with no room for the cash.** `LF_RobAction.CommitRobbery` now refuses with `REFUSED_NO_ROOM` when the robber's inventory takes none of the cash. It starts no till cooldown, adds no wanted and calls no dispatch, and the robber gets the toast `#LF-Robbery_NoRoom`. Test `crime/rob-no-room`.
  - **Chop-shop COLLECT.** COLLECT pays first. A short payment is rolled back and refused (`NO_ROOM`, `#LF-Chop_NoRoom`), and the car stays. COLLECT also refuses while someone sits in the car (`LF_ChopShopRules.CheckCollect`). Test `chopshop/collect-guard`.
  - **Escort holds.** `LF_Jail.Imprison` ends the prisoner's escort hold (`LF_JailEscort.Drop`) on every arrest path. Before this, a freed prisoner's next logout counted as escaping custody. Test `jail/escort-drop`.
  - **Self-arrest.** `LF_Jail.Arrest` refuses when the officer is the prisoner. The police-menu arrest RPC now runs the same `LF_PlayerInteraction.CheckPair` and grace gate as the in-world arrest action, so the menu cannot arrest yourself, a dead player or a new character. Test `jail/self-arrest`.
  - **Jail work after a logout.** The jail-work timer carries the prisoner's uid, so a prisoner who logs out mid-task is no longer refused as BUSY after rejoining. Test `jail/work-abandon`.
  - **Jail work refuses a start out of reach (#2580).** `LF_JailWork.Begin` (via `BeginFor`) now refuses a worker beyond `LF_WorkAnim.MAX_RANGE` of the work spot, or a dead one, and logs a `[LFDebug:Jail]` warning; before, only the finish checked range. Test `jail/work-begin-range`.
  - **Escort grab reach and holds** (#2542). Both escort actions (`LF_JailEscortAction`, `LF_JailEscortArrestAction`) run `LF_JailEscort.Grab` on the server: reach first (`LF_PlayerInteraction.CheckPair`, the cuff and arrest rule), then a hold check. `LF_JailEscort.Escort` and `Load` refuse a suspect another officer already holds instead of overwriting the hold (`HeldByOther`; the holder's own repeat grab still works). Each refusal logs through `LF_Debug` ("Jail") and toasts the officer (`#LF-Interact_TooFar`, `#LF-Escort_HeldByOther`). Test `jail/escort-grab-rules`.
- 🔧 **1989 jail work stations** (#3418, pack; merged, in-dev). Three pack prefabs in `LF_Everon1989/Prefabs/Props/Jail/` carry the existing `LF_JailWorkAction` (#127): `LF_JailLaundryTub` (job `laundry_tub`, 10 s off, 6 s work), `LF_JailKitchenSink` (`kitchen_sink`, 8 s off, 5 s work) and `LF_JailYardBroom` (`yard_broom`, 8 s off, 8 s work), each with a 30 s cooldown per job. A full round takes 26 s off, under half of the shortest sentence (one wanted level, 60 s). Strings `#LF-JailWork_LaundryTub`, `_Kitchen`, `_Yard`. Not yet placed in a layer; no script or save change. Pack test `everon/jail-work`.
- ✅ **Fence workshop yard, pass 1 BUILD** (#2302, shell #2301, epic #2201; pack). `FenceYard.et` (Workshop_01 main plus the side annex) stands in pack grid cell c14r4 (2x2, `Grid_fence_yard.layer`). The front is a plain garage; the fence's stall (`LF_FenceStall`, stock `fence`) and the fence (`LF_Fence`) are in the annex back room, written by `tools/buildings/fence-yard/fitout.mjs` (`--check` for drift) with the room lights (`FenceYard_Kit.et`: 3 shadowless lights). No sign, string or map place names the fence (D-M5). No stash (the only stash system is the org hideout's) and no car bay (Crime has none for a fence). No script or save change.
- 🔧 **Crime and policing review fixes** (#2419, merged, in-dev): the evidence bag takes a dropped item only while it still lies loose beside its marker (`LF_Evidence.Bag`: no parent entity, no inventory slot, within `ITEM_MOVED_M` 2 m; otherwise the row is dropped and the item stays with whoever has it). An NPC witness answers an officer once (`LF_WitnessEntry.m_bAsked`; the accuracy roll no longer repeats). The contraband run pays only a driven crate (`LF_ConvoyRun.m_bDriven`, set when the vehicle watch attaches) and a paid character waits `LF_Contraband.COOLDOWN_MS` (10 min) before the next run (`#LF-Contraband_Cooldown`, `#LF-Contraband_NotDriven`); its failure toast and `LF_Hotwire.MayStartEngine` use `LF_Utils.NotifyEntity`. A hideout door a member opened relocks when no member of the owner stands within `LF_Hideout.RELOCK_RADIUS_M` (40 m), checked every `RELOCK_SWEEP_MS`; `ApplyAll` releases a live lease the save does not hold; `LF_Hideout`, `LF_PoliceApplications` have world-end reset hooks; `LF_PoliceRosterNotice.s_aSent` is capped at 64. The chop bench takes the nearest stolen car (`LF_ChopShopRules.PickDropIndex`), so an unstolen car beside it no longer blocks the yard. `LF_WorkAnim.ResetStatics` removes its queued `Watch`. No save format change. Tests `investigation/bag-held`, `investigation/npc-witness`, `crime/contraband-cooldown`, `hideout/relock-and-load`, `crime/chop-drop-pick`, `work/anim-reset`, `police/statics-reset`. ⚠️ Not checked live: the relock sweep with a real member walking away, and the dedicated-server toasts.
- 🧪 **Police calls, list and take** (#2477, slice of #234; merged, in-dev). `LF_PoliceCalls` (server, session only) keeps a list of calls. A player's first wanted report (`LF_OnWantedChanged` 0 to above 0) posts one `FINE` call naming the suspect uid, the site (the newest report's place text, never a coordinate) and a minimum rank (`LF_PoliceRanks.PATROL`); a suspect has at most one open call. `Take(callId, officerUid, accountState)` accepts only an on-duty police state at or above the call's rank, one officer per call; each refusal logs `LF_Debug.Warn("PoliceCalls", ...)`. `Complete(callId, officerUid)` closes it once, for the officer who holds it. The list holds 64 calls and drops the oldest closed one. `LF_GameModeRoleplay` calls `LF_PoliceCalls.Install()` (one line). No UI, pay or save yet. Test `police/call-accept`.
- 🧪 **Police dispatch board: take a call** (#2479, slice of #234; merged, in-dev). The police dispatch board (`LF_DispatchBoard_Police.et`) carries `LF_PoliceCallBoardAction` ("Take a call") beside the radio-call take. Only an on-duty police officer is offered it. On perform the server checks the officer is at the board, takes the oldest open call nobody holds whose minimum rank the officer meets (`LF_PoliceCalls.OldestOpenFor`, then `Take`, which re-checks duty and rank), toasts the site, and sends a waypoint on the site to that officer (the owner-channel waypoint the job stops use). A site filed as a grid square marks the square centre, a site filed as a place sign title marks that sign; a site named only by the map label gets the take and the toast but no waypoint. Refusals log `LF_Debug` `PoliceCalls`. Test `police/call-board`. Manual: an officer on duty takes a call at the board in a session.
- 🧪 **Unit status from the car radio** (#4204, PO-D3; merged, in-dev). The police car (`LF_UAZ469_Police.et`) carries two radio rows, "Radio: available" and "Radio: busy" (`LF_UnitStatusAction`, at the driver door or in the cab), offered only to an officer on duty. The server re-checks the account and the reach, then keeps one status per officer (`LF_UnitStatus`, session state, no save). Every officer reading the dispatch or wanted board gets a "UNITS ON THE RADIO" section, one row per unit ("Unit Dupont: busy"), sorted by name. Clocking out clears the row (`LF_PoliceRanks.OnDutyChanged`), and a unit no longer on duty is dropped at the next read. Refusals log `LF_Debug` `UnitStatus`. Test `police/unit-status`. Manual: two officers on duty; one radios busy from the car, the other reads the dispatch board and sees the row; the first clocks out and the row is gone on the next read.
- 🧪 **Dispatcher voice on the radio** (#4422, PO-D5; merged, in-dev). When a call reaches an on-duty officer, the vanilla radio squelch plays from their body (`radioSoundProject`), then a calm male dispatcher says what happened and where: "Robbery in progress." then "At the general store." (`LF_DispatchVoice`, called once from the radio RPC in `LF_Dispatch` on the officer's client). The recordings are the voiced dialogue `dispatcher` (`tools/assets/dialogue/dispatcher/`, ElevenLabs voice "Eric - Smooth, Trustworthy", used by no other NPC), registered by the Everon pack's own index overlay `LF_DialogueIndex_Dispatcher.conf` (listed in the mission header); 13 call types answer the offence key and 13 places (the town label, and the sign titles) answer the place key the call carries. The core holds no place names. An offence or place with no line logs a `LF_Debug` `Police` warning, the voice says only the line it has, and the call lands as before; the log line `dispatch voice <type> <place>` names the lines picked. Test `LF_Test_DispatchVoice` (LOGIC). Manual: two players, one on duty at the station, the other robs a till at the general store; the officer hears the squelch, "Robbery in progress", "At the general store", then the toast.
- 🧪 **Police call pay: fine at the site completes the call** (#2478, slice of #234; merged, in-dev). `LF_FineTicketRole.Write` publishes `LF_OnFineIssued` (officer and holder uids, the officer's position, ticket number, amount) after a ticket is issued. `LF_PoliceCallPay` (server) completes the suspect's open call when the fine is by the officer who took it, the suspect still has an open wanted report, and the officer stands within `callRadius` (police config, default 50 m, ground plane) of the site. Since #4319 it pays no cash: the completed call earns the $15 resolved-call duty bonus once (`LF_DutyBonus`, paid at clock-out), toast `#LF-PoliceCall_Bonus` (or `#LF-CallOutcome_Resolved` when the hour's cap is reached); the `callReward` config key is gone. The site is a position kept per suspect: the alarm site of the police `LF_OnServiceCall` the crime report publishes, else the suspect's body there; a wanted level rising from or dropping to 0 forgets it. With no kept position a `grid X-Z` place resolves to its square's centre (radius at least 71 m); other place text logs a Warn and completes nothing. Refusals log under `[LFDebug:PoliceCallPay]`. `LF_GameModeRoleplay` calls `LF_PoliceCallPay.Install()` (one line). Test `police/call-pay`.
- 🧪 **Police calls: the report's place, and a call closes with the wanted level** (#2516, fix of #2477; merged, in-dev). Crime paths raise the wanted level before `LF_WantedReports.Report` files the report, so a call posted at the rise had an empty site. `LF_WantedReports.Add` now publishes `LF_OnWantedReportFiled` (uid, sequence, place). A level rising from 0 with a report already filed posts the call at once with the newest report's place; with none filed yet the suspect waits (`call for <uid> waits for the crime report`) and the first report posts the call with its place. A level back at 0 (served time, expiry, another officer's arrest, an admin) drops the wait and closes the suspect's open call, taken or not (`LF_PoliceCalls.CloseFor`, `[LFDebug:PoliceCalls] call #N closed for <uid>: no longer wanted`); a closed call pays nothing and `Complete` refuses it. A report filed after the level cleared posts no call. Test `police/call-lifecycle`.
- 🔧 **A spare drill in the pack never blocks the charge** (#2581, item 13 of #2418; merged, in-dev): `LF_Bank.PickTool` picks the drill only when it is held (`LF_HandTool.HeldTool`); a keycard or charge the robber can use comes next, and a drill only in the pack is the answer last, so a drill-only vault still shows "hold the drill". A robber with a charge and a spare pack drill at a `drill,charge` vault is no longer told to drill. No save change. Test `crime/vault-picktool-spare`.
- 🧪 **Police calls keep the crime's position** (#2578, follow-up of #2516; merged, in-dev). A site named only by a map location label (`LF_NamedPlaces`) resolved to no point, so the officer got no waypoint. `LF_WantedReportRecord` now carries a session-only `m_vPosition` and `m_bHasPosition` (`[NonSerialized]`, in no save payload, no version bump); `LF_WantedReports.Report` files it through `AddAt` and `LF_OnWantedReportFiled` carries it. `LF_PoliceCall` stores it (from the event, or from the newest stored report when the report came first) and `LF_PoliceCallBoardAction.CallOrigin` uses it before the site text, so `SendWaypoint` marks the crime's spot. A report with no position (`Add`, a loaded save) falls back to the site text as before. Test: `LF_Test_PoliceCallPosition` (WORLD).
- 🧪 **The Saint-Philippe `police` slot is fitted on the island (#2319, town draft; merged, in-dev once approved).** The vanilla police villa b32 (`Villa_E_2I01_Police`) gets the `police` kit (`tools/town/kits/police.json`, lifted from the grid police station kit): the duty desk with its clock-in clocks, the armoury store, the evidence locker, the wanted and dispatch boards, the front-desk dressing and the fascia sign. The layer is `Place_saint-philippe_police.layer` with the `PoliceStation`, `PoliceDesk` and `Armoury` markers. Left out of the kit: the licence desk and case board (they cut the vanilla villa's walls), the payphone (floated at roof height), the patrol cars and spike strip, and the six lights (the kit carries no light settings). Cells are not in this slot yet.
- 🧪 **Police Search and Arrest are the player interaction menu on a wanted suspect** (#2494, proof only; merged, in-dev). The duty NPC menu with two blank buttons is gone (#2509, #2535); an on-duty officer stands within 2 m of a wanted player and the vanilla action menu on the body offers Search (`LF_ConfiscateAction`) and Arrest (`LF_JailArrestAction`), the latter sending the suspect to the cell with a sentence slip. `session.mjs police-arrest-proof` (`up --clients 2 --profile full --roster 2`) drives it on two real clients: rob the till, clock in, menu picture, Search, Arrest, `jail=` seconds above 0 (56 s for wanted 1). `LF_AskPoliceArrest` stays server-guarded for a later wanted-list sheet. No game code changed.
- 🧪 **Shop bell for the general store till** (#2727, rescued WIP of #929; merged, in-dev): a till can name the alarm kind `shopbell` (`LF_RobAction.m_sAlarmKind`), an ElevenLabs-generated 1980s shop burglar bell (`Alarm_Robbery_ShopBell.acp`, `LF_SiteAlarm_ShopBell[Loud].et`, kind row in `LF_Police.conf`); the core default kind for a till stays `store`. Core ships the kind and the prefabs; the grid cell `shops-till` mounts a till that rings it (`Grid_shops_till_Till`). The island general store mounts it in a follow-up once its town draft lands. Test `police/store-alarm`.
- 🔧 **The vault prompt works on a dedicated client** (#2418, item 10; merged, in-dev): the vault table (`LF_Bank`) and the drill sessions (`LF_VaultDrill`) live on the server only, so a remote client's rob prompt read "This vault cannot be robbed" on every vault and never showed "Stop drilling"; a host or listen run hid it. On a client `LF_Bank.GateReason` now answers from what replicates (`ClientGateReason`): the faction and police count from `LF_AccountReplication`, open or armed from the door's breach RplProps, the tools and police minimum from the vault's own `LF_SafeTuning`, and the driller from the new `LF_BankVaultComponent` RplProp (`SetDriller`, a player id, set when a drill starts and cleared when it ends). Cooldown, stock and grace stay server-only: `LF_Bank.Rob` now toasts each refusal (`GateRefusal`) instead of a silent no-op. No save change. Test `crime/vault-client-offer`; session proof `vault-offer-proof` (dedicated server, one client).
- 🔧 **The clinic cabinet prompt works on a dedicated client** (#3364; merged, in-dev): `LF_HospitalCabinetAction` asked `LF_Hospital.CanBreach`, which reads the server-only cabinet table, so every client saw "Breach cabinet" greyed as "already open". A client now asks the cabinet's `LF_BreachTargetComponent.CanArmForOffer` (the replicated open and armed flags), as `LF_BreachArmAction` does; the server keeps `CanBreach` and `PerformAction` re-checks through `LF_Hospital.BreachCabinet`. No save change. Test `crime/hospital-cabinet-client-offer`.
- 🧪 (2026-11-18, #3350, X3 of #3339) **A safe zone on the Saint-Philippe arrival square.** The island had no zone, so a newcomer could be shot at the clerk. The pack layer `Place_saint-philippe_safe-zone.layer` (slot `safe-zone`, place kind `SafeZone`, claim `saint-philippe-safe-zone.json`) holds one SAFE `LF_ZoneComponent` circle, 52 m around (4437, 10670): the town hall and its clerk, the forecourt, the residents' house and the square by the pub. It stops short of the bank (nearest wall about 60 m off) and the gun counter, where robberies happen. No lawless zone (D11). Boot log: `[LFDebug:Zones] zone Fit_saint_philippe_safe_zone_Place is safe: shape CIRCLE at <4437,12.7,10670> radius 52 m`. The refusal itself is the shared rule, test `safezone/blocks`. Follow-up: the circle did not cover where newcomers spawn. A brand-new character wakes in a hostel first-spawn bed (4474, 10762, 98 m from the square, found in the 2026-11-18 first-hour run: `pos=4474.4,16.3,10761.4 zone=normal`), and the legacy `MainWorld` spawn (4472, 10535, 140 m south) served every other spawn. So the same layer now holds a second 25 m SAFE circle `Fit_saint_philippe_safe_zone_hostel_Place` (4474, 10762) over the beds and the spawn point `Fit_saint_philippe_safe_zone_spawn` (4426, 10664, 12 m inside the square circle) for a bedless, post-death or later spawn, and the legacy spawn is gone. The slot's footprint margin is 120 m (was 70) so one slot holds both circles. Boot log: `[LFDebug:Zones] zone Fit_saint_philippe_safe_zone_hostel_Place is safe: shape CIRCLE at <4474,12.5,10762> radius 25 m`.
- 🔧 **The evidence locker orders a convoy when it fills** (#3378, merged, in-dev): `LF_EvidenceLockerFill` counts the locker's rows against `LF_EvidenceConfig.m_iConvoyThreshold` (default 12, `LF_Evidence.conf`). The locker calls `Check()` after a bag or a back-room seizure is logged or a raid empties it; reaching the threshold publishes `LF_OnEvidenceConvoyDue` (rows, threshold, reason `full`) once, and dropping below it (a raid, a world reset, a load) re-arms. A break-in at the locker (`LF_Breach.Reset` or `CancelCharge` on target `LF_EvidenceLocker`) publishes it at once with reason `break-in`, once until re-armed. Nothing subscribes yet, and the locker's rows and save are unchanged. Test `investigation/locker-fill`.
- 🔧 (2026-11-21) **A convoy that does not leave re-arms its order.** A start refused for missing route markers, no AI budget or a convoy already under way, and a convoy called off, left the locker "due" until its rows dropped below the threshold or the server restarted, so a full locker ordered nothing more. `LF_EvidenceLockerFill.ScheduleRearm()` (called by `LF_EvidenceConvoySubscriber` on a refused start and by `LF_EvidenceConvoy.TryClear` after a call-off) arms `RearmNow` after `LF_EvidenceConfig.m_iConvoyRearmSeconds` (default 600, `LF_Evidence.conf`); a locker still at the threshold then orders again, one emptied meanwhile orders nothing. Tests `investigation/locker-fill`, `investigation/evidence-convoy`.

- 🔧 **The evidence convoy** (#3382, in-dev): when the locker orders a convoy, `LF_EvidenceConvoy` (subscribed in `LF_GameModeRoleplay`) starts the scene `evidence_convoy`: a Ural truck with a driver and a police UAZ escort spawn at the scene's `event_spawn` marker, drive its `event_waypoint` route to the `event_target` (the central store) through `LF_EventSceneDrive` (the escort is a second drive over the same route, starting 9 m behind), and despawn out of sight; on-duty police get an "Evidence transfer under way" toast when it leaves. A snapshot of the locker's rows rides in the truck's crate (a child prop of the truck). The rows stay in the locker while the truck is on the road and leave it on arrival (`LF_EvidenceLocker.RemoveDelivered`, numbering is not reused, so case board cards keep their row number); a convoy that is lost, times out (900 s) or is dropped by a restart delivers nothing. After a delivery a locker still at the threshold orders the next convoy. No placed markers, no AI budget (two free slots), or an empty locker start nothing and the locker keeps its rows (logged `[LFDebug:EvidenceConvoy]`). The island route markers are not placed yet. Test `investigation/evidence-convoy`.

- 🔧 **Chop yard outside town** (#3403, X29, merged, in-dev): a lone farm yard 1.2 km from the Saint-Philippe square, in the hamlet scouted as `chopyard` (the big barn at its south edge, open ground east of its doors, the road 59 m beyond). `LF_ChopBench` (shop id `chopyard-yard`, drop area 10 m left clear) stands 14 m in front of the barn, dressed with the engine hoist (the bench mesh), a welder, jack, vehicle doors and scrap, tyres, barrels, a workbench and one wreck. No sign and no POI (`poi` none, kind `ChopYard`). Place `chopyard`, template `chopyard`, kit `chop-yard` (lifted from DebugWorld `Street_ChopShop.layer`), layer `Place_chopyard_yard.layer`. `audit placement --strict` clean, `town check chopyard` ok. ⚠️ Not checked live: driving a stolen car in and chopping it (session proof).
- **Furniture offer from a dedicated client (#3361):** the HOUSE_OWNER_OR_KEY rule read `LF_HouseManagerComponent.Live()`, which only the server fills, so a remote client never saw the furniture prompt, even in its own house. A client now reads the house managers streamed in within 30 m (`LF_PlacementPolicy.GatherHouseClient`, a static-entity sphere query) and the replicated `m_sHouseIdentifier` / `m_sOwnerKey`, plus the keys it carries. It only decides whether the prompt shows; the server's `Confirm` still re-checks with the live table. Test `placement/client-offer` (a fixture house outside the live table). ⚠️ Not checked live: a dedicated-server client in an owned house (session proof `placement-furniture-proof` not yet written); the built-plot cottage as a source stays with #3173.
- 🔧 **Furniture offer in an island house (playtest 2026-11-27 round 5; merged, in-dev):** the client's house search above was a `STATIC` sphere query, and every Everon house is a mesh-less `LF_HouseMarker.et` that a static query never returns, so the owner of a bought Saint-Philippe cottage stood in it with a bought chest on the floor and was never offered "Place where I aim" (no house within 30 m on the client). `LF_PlacementPolicy.NearestReplicatedHouse` now queries every entity (`EQueryEntitiesFlags.ALL`). The server's `Confirm` is unchanged. Test `placement/client-offer-marker` (WORLD: owner and key holder offered in a marker house, a stranger not). ⚠️ The place, put in, take out and relog in a real session is a manual step.
- 🧪 **Cottage furniture range complete: stool added (#3194, parent #3169).** Bed, wardrobe and rug were already in the home set; the stool (`Furniture_Stool_Item`/`_Placed`, vanilla `Stool_01`, FLOOR snap, buy 40 / sell 16) is the missing piece, written by `tools/furniture/gen.mjs` from a new `items.json` row (policy row, string `LF-Furniture_Stool_Name`, price rule and `furniture` stock row follow). It sells from the furniture store (#3256), not the general store, like the other fourteen. Test `housing/furniture-range` (`LF_Test_FurnitureRange`, WORLD): each of the four has its `_Item`/`_Placed` pair with the owner stamp, bed, stool and rug land on the floor of the owner's house through the placement service, the rug is a flat mat, the wardrobe is a WALL item; `everon/furniture-home` now lists the stool (stocked, priced, pick-up).
- 🧪 **Prison break rules as pure logic (#3420, parent #1027, #798):** `LF_PrisonBreakRules.TryStart` decides whether a break may start: the feature is on (`Configs/Police/LF_PrisonBreak.conf`, `m_bEnabled` 0, ships off), the starter is free and belongs to an organisation, that organisation has a member inside, at least `m_iMinOfficersOnDuty` (3) officers are on duty, no window is open, and the island cooldown (`m_iCooldownSeconds`, 7200) is spent. A good start opens a window of `m_iWindowSeconds` (90, 10..600) in which prisoners may choose to walk; refusals are machine codes logged as `[LFDebug:PrisonBreak]`. Pure state (last start, window end), no entities and no persistence; nothing calls it yet. `LF_Test_PrisonBreakRules` (LOGIC) covers each refusal, the window length and the cooldown.
- 🔧 **Crew robbery rules as pure logic** (#3401, parent #1024, merged, in-dev): `LF_CrewRobRules` (`Feature/Crime/LF_CrewRobRules.c`) holds the rules of a crew robbery with a holder on the till spot, optional lookouts and a minimum crew (`CrewMeets`). The take grows per second only while the holder is on the spot (`AdvanceSpotSeconds`), pauses off it and is capped (`Take`). The police response window starts with the alarm, not the robbery (`ResponseWindowOpen`). Pay is split evenly among the crew present at the end in int math, the remainder going one cash each to the first members: 301 among 3 is 101, 100, 100 (`Split`). Numbers live in `LF_CrewRobConfig` (`Configs/Crime/LF_CrewRob.conf`: min crew 2, 5 per second, cap 1500, 90 s window). No world wiring yet. Test `LF_Test_CrewRobRules` (LOGIC).
- 🔧 **Rob a till as a crew** (#3404, parent #1024, in-dev): a till whose `LF_RobAction` has `m_bCrewJob` set is robbed by a crew; the instant action is hidden and refused there. Three actions beside it (`LF_CrewRobStartAction`, `LF_CrewRobHoldAction`, `LF_CrewRobTakeAction` in `LF_CrewRobActions.c`). "Start the job" gathers the starter and every armed civilian within 15 m (`LF_CrewRob.GatherCrew`), needs `LF_CrewRobConfig.m_iMinCrew`, runs the single-till gates (cooldown, grace, civilian, officers on duty) for the holder, starts the till cooldown, raises wanted on each member and sounds one alarm through `LF_Dispatch.Report`. "Hold the till" (holder only) makes the take grow each 250 ms tick while the holder stands within the till's hold radius, and it pauses when they step off (`LF_CrewRobRules.AdvanceSpotSeconds`). "Take the money" (holder only, once) ends the job and pays every crew member within 40 m of the till an equal share (`LF_CrewRobRules.Split`) as physical cash, or marked notes when dirty money is on (`LF_RobAction.PayLoot`). A lookout gets a toast when an on-duty officer comes within the till's police radius, once per approach. A member who is down, dead or cuffed at the take gets no share. A holder who goes down, is cuffed or leaves ends the job unpaid, and so does a take that stops growing for `LF_CrewRobConfig.m_fIdleTimeoutSeconds` (120 s; the holder off the spot, not holding, or at the cap), which frees the till. Server-side state only, not saved. Grid cell `crew-till` holds `LF_CrewTill.et` (a counter with the till marked for crew jobs and the three actions); `node agent-box/session.mjs crew-rob-proof` (`up --mission grid --clients 2`) proves it on a dedicated server with two players. Test `LF_Test_CrewRob` (WORLD).
- 🔧 **Heist stages as pure logic** (#3409, parent #1024, merged, in-dev): `LF_HeistStages` (`Feature/Crime/LF_HeistStages.c`) is the state machine a heist runs on: ordered stages, each with a gate (`LF_EHeistGate`: `TARGET_OPENED`, `CREW_IN_AREA`, `TIME_HELD` with a hold time), one clock for the job and a fail state. A signal for any stage but the current one, or with the wrong gate, does nothing. The clock running out (also on a late signal) or `Tick(now, crewAlive)` seeing no crew fails it (`FAIL_CLOCK`, `FAIL_CREW`); a fail and a success are final and the first fail reason stands. Times are world milliseconds passed in; clock 0 means no limit; leaving a hold resets it. Nothing in the world uses it yet (the stages are wired by the heist cards under #1024). Test `crime/heist-stages`. Log `[LFDebug:Heist]`.
- 🧪 **The break: forcing the jail door opens a window (#3421, parent #1027, #798, merged, in-dev):** `LF_PrisonBreak` (`Feature/Police/LF_PrisonBreak.c`) listens to the jail door's `LF_Breach` (hooks in `LF_Breach.Arm` and `Open`). When the rules of #3420 allow it (breaks on, the starter free and in an organisation with a member serving, enough officers, cooldown spent), the window opens for `windowSeconds`, a jailbreak report goes to dispatch (every on-duty officer is radioed; the forced door already sounds its alarm), and a prisoner who walks out of the cell volume while it is open (`LF_Jail.NoteEscape`) gets the `escaped` status: still serving, remaining time kept in the `jail` payload and the sentence clock held while out (`LF_Jail.TickOnline`), no longer pulled back (`LF_Jail.Confines`). At the end of the window the door locks again (`LF_Breach.Reset`); a prisoner who stayed in serves on. A breach the rules refuse is the plain break-in: no window, no status. The escaped list is the `prisonbreak` save section (`LF_PrisonBreakSavePayload`, character key and seconds left; a save without it leaves nobody escaped); the window is session state. Not yet: recapturing an escapee (`LF_Jail.Arrest` still refuses a prisoner who is serving). Test `police/prison-break` (PERSISTENCE; restart check under `wt e2e`).
- 🧪 **The search afterwards: escapees are wanted (#3422, parent #1027, #798, merged, in-dev):** `LF_PrisonEscape` (`Feature/Police/LF_PrisonEscape.c`). A prisoner who walks out in a break window is posted on the wanted board as "Escaped custody" (`#LF-Offence_EscapedCustody`, a crime report with a dispatch call; wanted at least 1; on their record). The member who forced the door has the jailbreak on their record and is wanted (`LF_PrisonEscape.OnOpened`, called from `LF_PrisonBreak.Start`). `LF_Jail.Arrest` no longer refuses an escapee: it calls `LF_Jail.Recapture`, which puts them back in the cell with the rest of the sentence plus the escape term (`LF_PrisonBreakConfig.m_iEscapeTermSeconds`, default 300 s), clears the escaped status and refreshes the slip; the escort (`LF_JailEscort`) uses `LF_Jail.IsInCustody` so a cuffed escapee can be walked back too. A first escape is one dispatch call and one wanted row. A prisoner who stayed inside is still refused. No new save data: the status is the "prisonbreak" section, the sentence is the jail payload. Test: `LF_Test_PrisonEscape`.
- 🔧 **Two-layer jailbreak and bail (merged, in-dev).** `LF_JailBreak` (`Feature/Police/LF_JailBreak.c`): a jail whose cell (`LF_JailCellComponent`) names an outer door (`m_sOuterDoorTargetId`) as well as its cell door is broken in two charges on the existing `LF_Breach.Arm` path. `LF_Breach.Arm` asks `LF_JailBreak.ArmRefusal` before the charge is taken: the cells are refused while the outer door is locked (`#LF-JailBreak_OuterFirst`, also the offer reason from the outer door's replicated open flag), a prisoner in custody cannot charge either door (`#LF-JailBreak_FromInside`), and both doors refuse inside the cooldown after the cells last blew (`#LF-JailBreak_Cooldown`). Both layers ring the loud alarm and dispatch with the door's position; the config fuse replaces the door's own. The cell charge reports the new catalogue row `#LF-Offence_PrisonBreach` (+4 wanted, a 600 s sentence floor). When the cells open (`LF_Breach.Open`), `LF_Jail.FreeInside` ends the sentence of every prisoner in custody whose body is within the cell zone plus 2 m (no release move, no wanted reduction, slip stamped) and `LF_PrisonEscape.OnEscaped` posts each as escaped custody, wanted; an offline prisoner (no body) serves on. The breacher gets the jailbreak on their record (`LF_PrisonEscape.OnOpened`). Each blown door seals itself after `m_iResealSeconds` (`LF_Breach.Reset`), or police repair it with the breach repair action. In two-layer mode `LF_PrisonBreak.OnOpened` opens no break window. **Bail** (`Feature/Police/LF_JailBail.c`, action `LF_JailBailAction` on `Prefabs/Police/LF_JailBailPhone.et`): a prisoner in custody pays `LF_JailBailRules.Quote` (per started minute left, clamped to a minimum and maximum) from their bank (`LF_ATMManager`, `Withdraw` is the check; cash is not used) and `LF_Jail.EndOnBail` ends the sentence like a served one; refused with nothing taken when bail is off, the payer is not in custody, or the bank is short. Releases now land on the floor under the release point (`LF_Jail.GroundedReleasePoint`, a down trace that sees through characters). Numbers in `Configs/Police/LF_JailBreak.conf` (`LF_JailBreakConfig`, pack-overlayable; the Everon pack sets bail 150 per minute, at least 500). Server only; the doors reach a late joiner through `LF_BreachTargetComponent`'s replicated flag; the cooldown is session state. Tests `police/jailbreak-layers`, `police/jailbreak-free` (WORLD), `police/jail-bail` (LOGIC). Logs `[LFDebug:JailBreak]`, `[LFDebug:JailBail]`. On the Saint-Philippe island the jail kit (`tools/town/kits/jail.json`) places it: the cell door (`saint_philippe_jail_cell`, `EL_JailDoor` in the villa's barred doorway, the vanilla leaf hidden by a boxed clear), a barred gate that fills the corridor mouth wall to wall (outer door `saint_philippe_jail_outer` plus two fixed wing leaves, so the hall cannot be walked around), the bail phone inside the cell zone, and the cell row names both doors. Before this the island cell named no door, so a sentenced prisoner was not held at all. A served prisoner is released in the public hall, north of the gate. Both jail layers sound their own **jailbreak siren**: `LF_JailBreak.AlarmKindFor` sets the alarm kind `jail` (`Configs/Police/LF_Police.conf`, prefabs `LF_SiteAlarm_Jail` and `_JailLoud`, orange flashing light, a 196 Hz four-blast klaxon synthesised by `tools/assets/jail-siren/synth.mjs`), a sound no robbery kind shares. Test `alarm/jailbreak-siren`. Proof `island-jail-breach-proof` (held, cells refused first, outer then cells frees B) and bail paid at the cell phone (500 from the bank, released in the hall). **Playtest town (#4247, merged, in-dev):** the town's walled jail yard (`Street_Jail.layer`) gets the bail phone, `Town_Jail_BailPhone` on a vanilla office desk against the yard's north wall inside the cell zone, so a sentenced player can pay bail there; the town's cell, gate and two work props stay as they were. The outer door and wing leaves are not in the town yet (it has one gate and no corridor), so the town jail is the single-door cell.
- 🔧 **Hands-up rules as pure logic** (#3394, parent #1024, #782; merged, in-dev): `LF_HandsUpRules` (`Feature/Crime/LF_HandsUpRules.c`) is the state of one target with their hands up. The target chooses: it starts only on the target's own accepted answer to the prompt (`AnswerStarts`/`Answer`); another player's request or answer, a refusal and an invalid id do nothing, and repeating the yes does not restart the timer. It is never a forced pose. `Tick(pos, armed, nowMs)` ends it with a reason (`LF_HandsUpEndReason`): `ARMED` when a weapon is drawn, `MOVED` past 1.5 m horizontally of where the hands went up, `TIMEOUT` after 30 s; `Lower` ends it as `LOWERED` by the target only. Nothing in the world uses it yet (prompt, pose and rob wiring are separate cards). Test `crime/hands-up-rules` (`LF_Test_HandsUpRules`, LOGIC).

### `LF_BreakInRules` — `Feature/Crime/LF_BreakInRules.c`, `LF_BreakInConfig.c`
- 🧪 **Staged break-in rules, pure logic** (2026-11-19, #3368, parent #1023): a break-in is `LF_BreakInConfig.m_iStageCount` stages (`Configs/Crime/LF_BreakIn.conf`: 3 stages, 20 s each). `LF_BreakInRules.Tick(state, worked, required, dt, config)` advances a stage only while every work point is worked at once; one of two does nothing. Work stopped for longer than `m_fGraceSeconds` (3 s) drops the current stage to zero (finished stages stay), sets `m_bResetPending` once per gap (`ConsumeReset`) for the defenders, and fires only on stopped work that had progress, never on an approach. `ProgressOf` / `WordKey` give the word: barely started, halfway (from 0.34 overall), nearly through (from 0.75), through (keys `#LF-BreakIn_*`). No world clock, no entity: nothing calls it yet. Test `crime/break-in-rules`.
- 🔧 **Heist run: door, vault drill, getaway** (#3411, parent #1024, merged, in-dev): `LF_HeistRun` (`Feature/Crime/LF_HeistRun.c`) binds three `LF_HeistStages` stages to the world through the bus. Stage one is the site's breach door: `LF_Breach.Open` now publishes `LF_OnBreachOpened` (target id, the uid that armed it), and the door's event starts the run and its clock; the arm itself already rang the alarm and radioed dispatch. Stage two is `LF_OnVaultBreach` "opened" for the site's vault (the drill finishing). Stage three is the getaway: every crew member (the uids that opened the door or the vault) more than the radius from the site. A vault opened before the door does nothing; the clock running out, or no live crew body, fails the run. `LF_HeistRunComponent` on a site entity names the door target id, the vault id, the getaway radius (default 40 m) and the clock (default 600 s), ticks once a second while a run is going and starts a fresh run when one ends. Grid cell `grid:heist-run` (GridWorld, `Grid_heist_run.layer`): a breach door, a drill vault and the site. Test `crime/heist-run` (WORLD: real breach arm, real vault drill, getaway, clock fail). Log `[LFDebug:Heist]`. ⚠️ Not checked live: two clients on a dedicated server running the grid site end to end.
- 🔧 **Heist jobs as pack data: the 1989 bank job** (#3414, parent #1024, merged, in-dev): the core reads heist jobs from `Configs/Crime/LF_Heist.conf` (`LF_HeistConfig`, `Feature/Crime/LF_HeistConfig.c`, ships none) and layers each pack overlay over it by site id (`LF_ConfigOverlay`). A job is `LF_HeistSiteRow`: a clock, a police-on-duty minimum and ordered `LF_HeistStageRow`s; a stage kind resolves to its component and gate (`LF_HeistKinds`: door = `LF_BreachTargetComponent`, vault = `LF_BankVaultComponent`, getaway = `LF_HeistRunComponent` radius). The Everon 1989 pack (`LF_HeistBank_Everon1989.conf`, listed in the mission header) ships `bank`: staff door (`bank_staff_door`, the clerk may choose hands up), vault (`bank_vault`), getaway 120 m out, 720 s clock, 2 police on duty. Test `everon/heist-bank` (pack leg). ⚠️ The staff door and the site entity are not placed in the Saint-Philippe bank layer yet (follow-up card).
- 🔧 **Heist site reads its job row; the police minimum gates the door** (#3748, parent #1024, merged, in-dev): `LF_HeistRunComponent.m_sHeistId` names an `LF_HeistConfig` job; set, the site builds its run with `LF_HeistRun.FromRow` (door and vault ids from the stages, getaway radius from the getaway stage or the component's own when the row says 0, clock, police minimum, clerk hands-up flag) and the door/vault/radius/clock attributes are ignored. An id with no row, or a row missing a door, vault or getaway stage, builds no run and logs `[LFDebug:Heist]` an error. Arming the breach on a job's door with fewer police on duty than the job needs is refused (`LF_HeistRun.ArmRefusal`): the offer shows "Not enough police on duty for this job" (replicated count) and the server refuses the arm with the same toast (live count). The clerk hands-up flag is carried on the run (`ClerkMayHandsUp`) and logged when the run starts; nothing acts on it until hands-up is a state a target chooses (#782). Tests `crime/heist-run-row` (LOGIC) and `everon/heist-bank-run` (pack leg).
- 🔧 **Saint-Philippe bank: staff door and heist site placed** (#3752, parent #3748, in-dev): the island bank slot (`Place_saint-philippe_bank.layer`, kit `tools/town/kits/bank.json`) now carries the heist job's two entities. `LF_BankStaffDoor` (`Prefabs/Props/Roleplay/BankVault/`, a steel leaf with `LF_BreachTargetComponent` id `bank_staff_door`) hangs in the building's real west side doorway, in front of the wooden door; `LF_HeistSite_Bank` is the site (`LF_HeistRunComponent`, heist id `bank`, site id `saint_philippe_bank`; the 120 m getaway is measured from it). The job's police minimum, door, vault and getaway come from the pack's `bank` row (#3748). Proof `node agent-box/session.mjs island-bank-heist-proof` (`up --mission island --clients 4 --roster 3,4`): with one officer on duty arming the door is refused, with two the door opens and the run starts, a second robber drills the vault, both robbers reach the police desk (over 120 m away) and the run completes. Log `[LFDebug:Heist]`.
- 🔧 **Chop shop fits a hidden compartment (#3776, parent #1035, merged, in-dev):** `LF_ChopFitAction` on `LF_ChopBench` ("Fit a hidden compartment", shown only where the compartment table has rows, so the bare core offers nothing). `LF_ChopFit.Serve` (server, `Feature/Crime/LF_ChopFit.c`) picks the nearest car in the drop area the worker owns (`LF_ChopFitRules.PickIndex`), checks it (`LF_ChopFitRules.Check`: not the owner, no `LF_VehicleCompartmentConfig` row, already fitted, short purse; each has a `#LF-ChopFit_*` toast), takes the table's `m_iFitPrice` in cash (`chopshop-fit`), runs `LF_VehicleCompartment.Fit` and hands the money back whole if the compartment does not build. The size goes into the car's saved state (`LF_VehicleOwnership.SetState`) and the empty compartment into the vehiclecache ledger (`LF_VehicleCache.Capture`, #3435), so a garage trip or a restart keeps it. No new save section. Tests LOGIC `crime/chop-fit-rules`, pack WORLD `everon/chop-fit`. Log `[LFDebug:ChopShop]`.

### `LF_HandsUp` — `Feature/Crime/LF_HandsUp.c`, `LF_HandsUpPrompt.c` (#3396, slice H2 of #1024)
- 🧪 **Hands up, the target chooses** (2026-11-19, LOGIC `crime/hands-up-state`, proof `hands-up`): a hold-up (`LF_OnHoldUp`)
  or a held aim (`LF_OnAimedAt`) offers the target a prompt, "Put your hands up?" (`UI/Layouts/HUD/Crime/LF_HandsUpPrompt.layout`),
  for 8 s, not again for 20 s. The key (`LF_HandsUp`, F7 / gamepad RB+X: every letter key and bare gamepad X, which is vanilla Reload and Interact, clash with a vanilla action, `validate-input-vanilla-clash`) sends `RpcAsk_LF_HandsUp` on the target's own controller;
  the server takes the player id from that controller, needs a live offer, and raises the hands through `LF_HandsUpRules`
  (#3394: walk past 1.5 m, draw a weapon, lower them with the key, or 30 s ends it). Ignoring the prompt or answering with no
  offer changes nothing. The state is `SCR_ChimeraCharacter.m_bLF_HandsUp` (`RplProp`, `BumpMe` on change, so a late joiner
  reads it); the owner's machine plays the vanilla halt gesture (id 2) until the state ends (no two-hand pose exists in the
  base game). Harness verb `handsup state|choose <0|1|2>|offer <playerId>`. Logs `[LFDebug:HandsUp]`. No save change.
- 🔧 **Search needs a target that has given up: the hands-up gate** (#3399, parent #1024, #782; merged, in-dev): `LF_HandsUpGate`
  (`Feature/Crime/LF_HandsUpGate.c`) is the one gate a search or robbery of a PLAYER goes through. The target has given up when
  their hands are up or they are restrained; otherwise the action refuses with "They haven't given up." (`#LF-HandsUp_NotGivenUp`).
  The hands-up flag counts only from a target that is alive, controlled by a player and not downed (a dead body, a downed player or an
  AI never counts); restraint counts on its own. `LF_ConfiscateAction` (the officer's search) shows the reason on the offer (client,
  replicated flags) and refuses on the server before the search or the police-contact stamp. No action robs a player yet; `LF_RobAction`
  and the weapon and vehicle robberies rob a till, a weapon or a vehicle, so they are unchanged and a future player robbery calls
  `LF_HandsUpGate.Check`. Proof `search-proof` now raises the target's hands first and checks the down-hands refusal. Test
  `crime/hands-up-gate` (`LF_Test_HandsUpGate`, LOGIC). Logs `[LFDebug:HandsUp]`. No save change.
- ✅ **Police station rebuilt through the grid (#3889, merged, in-dev).** The Saint-Philippe police slot (`Place_saint-philippe_police.layer`, kit
  `tools/town/kits/police.json`) was authored in a grid authoring bay around a copy of the villa (`town author police`), walked and
  audited there, extracted and stamped onto b32 (`town stamp police`). It is furnished lightly: the front duty desk with a desk officer
  (`LF_Npc_Police_1`) behind it, the armory cabinet and the armory store, the evidence locker, wanted, dispatch and case boards, the two
  duty clocks, a few seats, a plant and an interview table upstairs. The eight display pieces on the old display desk, the posters, the
  back desk and the street bench, bin and flowerpots (their layer height was read as height above terrain, #3938) are gone. The front fascia
  row is left out: no seat on the villa's front wall cleared the shell audit (follow-up). The audit of the layer reports no floating,
  displaced, clipping, door-blocking or unreachable row. The kit's source layer is archived at `tools/town/kits/sources/police.layer`.
- 🧪 (2026-11-22) **Island jail stations seated on the real floor and out of the door swing (#3888).** The laundry tub was 8.6 cm into the island floor and the kitchen sink 0.4 m into the cell door's swing. The kit row of the tub now carries `keepY` with its island height (`town fit` used to snap every floor row back to the model's top surface), and the sink moved 0.65 m along its wall (z) to stand clear; all three stations are still inside the cell's 2 m zone. Merged, in-dev.
- 🧪 (2026-11-23) **Sentences up to 15 online minutes, work stations scaled (#4207).** The cell's cap is 900 s (`EL_JailCell.et`, `LF_JailCellComponent.MAX_SENTENCE_SECONDS`); a configured cap above that clamps to 900 with a `[LFDebug:Jail]` warning. The cell tells `LF_JailWork` its cap (`SetSentenceCap`) and each finished chore credits its station number scaled to it: 3x at 15 minutes (the 8, 10 and 8 s stations take 24, 30 and 24 s off), never more than 5% of the cap for one chore, and a full shift of all stations stays within 10% (`LF_JailWorkRules.ScaledSecondsOff`). No cap set leaves a chore as written. No persistence change. Test `jail/cap-15`. Merged, in-dev.

### `LF_StashContainer` — `Feature/Crime/LF_StashContainer*.c` (#4195, crime B3)

The short container: a tally clerk leaves one container short on the books, and only a player who carries the clerk's key opens it.

- `LF_StashContainerStock` holds the rules: a key prefab, the stock lines (`LF_StashContainerEntry`: prefab and count) and a refill timer in seconds (default 2700). `Check(player, nowSec)` answers `OK`, `NO_KEY`, `EMPTY`, `NO_STOCK` or `NO_PLAYER`; `Take` (server only) moves the whole stock into the player's inventory in one go, drops what does not fit at their feet, and starts the timer. Until it runs out a second open finds it empty.
- `LF_StashContainerComponent` goes on the container prop. The pack sets the key, the cartons and bottles and the refill time on its prefab. The component replicates only "empty or not" (`m_bEmpty`) so a client's action row can say why it stays shut; the server flips it back with a `CallLater` at the refill.
- `LF_StashContainerAction` is the "Open the container" row. It is refused with "Locked. You need the clerk's key" or "Empty. Come back later".
- The state lives for the session: a restart starts the container full again. Every take and refusal logs `[LFDebug:StashContainer]`.
- Test: `LF_Test_StashContainerRefill` (WORLD).
- 🧪 (2026-11-23) **Marcel the tally clerk: dialogue and fee (#4199, crime B4).** Pack dialogue `LF_Dialogue_TallyClerk.conf` (id `tally_clerk`, speaker Marcel Brun, spec `tools/assets/dialogue/tally_clerk/dialogue.json`), five lines, every one recorded in its own ElevenLabs voice (Antonin; no other NPC uses it): the greeting ("Container nine is short this week"), the fee line (four hundred dollars, it stays short on the books), the key line (brass key on a red string, back row past the crane), a come-back-later line and a goodbye. A player walks greeting, fee, pay. Captions are pack strings (`LF-Dialogue_TallyClerk_*`, `LF-Townsfolk_TallyClerk_Name`). The fee and the key are wired by #4381 (next row). Test `everon/tally-clerk` (pack leg).
- 🧪 (2026-11-23) **Marcel takes the $400 and hands over the container nine key (#4381, follow-up of #4199).** The pay reply ("Here is the four hundred.") runs the core dialogue action `FeeForItem` with `400|{6E6C667869AC0F87}Prefabs/Items/Story/LF_ContainerNineKey.et` and ends the talk. Paid: the $400 leaves the player's carried cash and the key goes into the inventory; Marcel answers with his voiced key line (its `for_key` is `#LF-Dialogue_FeePaid`) and a toast says "You paid $400 in cash." Short of $400: nothing is taken and he says "That is not enough. Come back with the full amount." Already carrying the key: nothing is taken ("You already have one. Keep your money."). If the key cannot go into the inventory, the $400 comes back whole. The key is `LF_ContainerNineKey.et` in the pack ("Container Nine Key", brass, our house-key mesh). Marcel's body is `Configs/Npcs/LF_Npc_Townsfolk_TallyClerk.et` (talk action, dialogue id `tally_clerk`) and his index row is the fragment `Configs/Dialogue/LF_DialogueIndex_OldMan/LF_DialogueIndex_TallyClerk.conf`. **Not placed yet:** the container yard on the dock is the town card CR-B9, which puts Marcel there with the kit `tools/town/kits/tally-clerk.json` and sets container nine's `m_sKeyPrefab` to this key. Tests `dialogue/fee-trade` (core) and `everon/tally-clerk` (pack leg).
- 🧪 (2026-11-23) **The container yard on the dock: container nine and Marcel stand in the world (#4441, crime B9 of 0.3.0).** Town slot `saint-philippe-dock/containers` (hostless plot p5, kit `tools/town/kits/container-yard.json`, layer `Place_saint-philippe-dock_containers.layer`): a small fenced yard on the dock, gate gap on the south side. Marcel the tally clerk stands just inside the gate by the yellow tally hut (the `tally-clerk` kit row, so his talk action offers the $400 key from his standing spot). Container nine is the pack prefab `Prefabs/Props/Crime/LF_ContainerNine_Everon1989.et` (a vanilla 20 ft container with an `LF_StashContainerComponent` and its "Open the container" action): key `LF_ContainerNineKey.et`, stock four cigarette cartons and four vodka bottles, refill 2700 s. A second plain container stands beside it with an aisle between, both lying north to south. The pack prefab, not the kit row, holds the key, stock and timer, so every copy of container nine is the same short container.
- 🧪 (2026-11-22) **Police call outcomes: resolved, no one found, unfounded (#4203, PO-D1 of 0.3.0).** `LF_CallOutcome.Close(callId, officerUid, outcome, officerPosition, messageKey, messageArg)` (server, `Police/LF_CallOutcome.c`) closes a call the officer holds as `LF_CallOutcomeKind.RESOLVED`, `NO_ONE_FOUND` or `UNFOUNDED` and records it on the call (`LF_PoliceCall.m_iOutcome`, added by a `modded class`, session only like the call list). The outcome counts only when the officer stands within `callRadius` (police config, default 50 m, ground plane) of the scene: the call's own crime position, else the pay site or the report's grid square. A far close, a call another officer holds, a closed call, no outcome or a site with no position is refused with a localized reason (`#LF-CallOutcome_TooFar` names the radius; `NotHeld`, `NoCall`, `NoScene`) and the call stays open. Success returns the closing line (`#LF-CallOutcome_Resolved`, `NoOneFound`, `Unfounded`). This slice only records and closes; the action row that calls it, pay, police XP, the caller's unfounded mark, the 30 s stay at the scene and the unfounded-rate flag are later PO-D cards that read `m_iOutcome`. Refusals log under `[LFDebug:CallOutcome]`. Test `police/call-outcomes`.
- 🧪 (2026-11-23) **A stop search finds contraband in a car (#4196, CR-B7 of 0.3.0).** `LF_VehicleSearchGoods` (`Feature/Police/LF_VehicleSearchGoods.c`). An item is classed contraband by the marker `LF_VehicleSearchGoodsComponent` (class tobacco, alcohol, narcotics or untaxed goods; the pack's cigarette carton and vodka bottle, #4198, carry it) or by `LF_VehicleSearchGoods.Register(prefab, class)`. The officer's "Search vehicle" (`LF_VehicleSearchAction`, the quick and the full one) walks the car's storage with the chain-goods find chance (`LF_ChainSearch.FindChance` of the classed units) and, on a find, toasts "Found in the trunk: 2 tobacco, 1 alcohol. Search again to seize it." The next search of that car within 5 minutes seizes exactly the classed goods still there, once, and files the offence against the people sitting in it; legal goods stay. Chain goods keep their own seize-on-find path. The pending find is server session state. Logs `[LFDebug:Police] stop search found ...` and `seized ...`. Test: `LF_Test_VehicleSearchGoods` (WORLD).
- 🧪 (2026-11-23) **Rank board and commendations plaque at the station (#4206, PO-R9 of 0.3.0).** Two printed boards, both board kinds (`LF_BoardProviders`) that anyone may read. `LF_RankBoard` (`Feature/Police/LF_RankBoard.c`, kind `rankboard`, read action `LF_RankBoardReadAction`) lists every account on the police roster that has a character, highest rank first and then by name, one line each: "Senior Officer: Sam Senior" (the rank title from `LF_PoliceRanks.TitleKey`, so a ladder of any length works). The server builds the list per read (`LF_PlayerAccountManager.CollectOfficerRows`, a `modded class` in that file), so a promotion shows on the next read; at most 40 rows print, an empty roster says "Nobody is on the roster yet." `LF_Commendations` / `LF_CommendationPlaque` (`Feature/Police/LF_Commendation.c`, kind `commendations`, read action `LF_CommendationPlaqueReadAction`) is a ledger of name, reason and time kept in `$profile:LifeFramework/police_commendations.txt` (`time|name|reason`, 200 rows, the separator scrubbed from a name or reason); `LF_Commendations.Award(name, reason, nowUnix)` is the server call that engraves one, and the plaque lists the newest 30 first ("Abe Patrol: Ten years of service"). Nothing awards a commendation yet: a later card calls `Award`. The two props exist since #4698: `Prefabs/Props/Police/LF_RankBoard.et` and `LF_CommendationPlaque.et`, each the vanilla `NoticeBoardCork_01` with a read-action row ("Read the rank board", "Read the commendations plaque", radius 1.5 m) and an `RplComponent`; a read opens the board card like the other board kinds. Their placement in the station hall is the town card R10; until then they stand only in the grid cell `slice-4698`. Test `police/rank-board-prefabs` (`LF_Test_RankBoardPrefabs`, WORLD) spawns each prop and checks it carries one read action of the class its board kind names. Logs `[LFDebug:RankBoard]` and `[LFDebug:Commendations]`. Test `police/rank-board-text` (`LF_Test_RankBoardText`, LOGIC).
- 🧪 (2026-11-23) **Rank fleet: the police car pool hands each rank its car (#4205, PO-R1 of 0.3.0).** `LF_RankFleet` (`Feature/Police/LF_RankFleet.c`) reads one car per police rank id from the pack overlay `LF_RankFleet_Everon1989.conf` (root class `LF_RankFleetConfig`; a later overlay row for the same rank replaces an earlier one; the core names no vehicle). The Everon pack gives the Cadet the beige S105 and Patrol, Senior Patrol, Sergeant and Lieutenant the `LF_UAZ469_Police` car. `Request(rank, out prefab, out refusalKey)` is what the pool calls; `MayDraw(rank, prefab, out refusalKey, out neededTitleKey)` lets a rank take its own car or any lower rank's, and refuses a higher car with `#LF-RankFleet_NeedsRank` ("That car is for the rank of %1 and up.", `%1` = the needed rank's title key) or `#LF-RankFleet_NotIssued` for a rank with no row or a car on no row. Refusals log `LF_Debug` `RankFleet`. The cars are the vanilla prefabs for now; the white POLICE livery repaint (WO-L4) swaps the prefab in the conf row. Tests `police/rank-fleet` (core) and `everon/rank-fleet` (pack leg). Not wired to a spawn point yet: the pool action that calls `Request` is a follow-up card.
- 🧪 (2026-11-23) **The cigarette carton and the vodka bottle exist (#4198, CR-B2 of 0.3.0).** Pack prefabs `LF_Everon1989/Prefabs/Items/Contraband/LF_CigaretteCarton.et` (vanilla `LitterInterior_01_Cigarettes_02` mesh, 0.2 kg, 200 ml) and `LF_VodkaBottle.et` (vanilla `S2_Vodka` mesh, 1.1 kg, 900 ml), both on `RoleplayItem_Base` (so a stack counts as one item with a quantity) and each with an `LF_VehicleSearchGoodsComponent` (carton = tobacco, bottle = alcohol), so the stop search (#4196) lists and seizes them without any `Register` call. They go into a field pack's storage. Names are pack strings `LF-Item_CigaretteCarton` and `LF-Item_VodkaBottle` (with `Desc`). No outlet sells them yet (buyer, barkeeper and container cards follow). The vanilla pack and bottle textures print Cyrillic, so each prefab overrides the one material slot that carries the print (`LitterInterior_01_V1`, `Vodka_paper`) with `LF_CigaretteCarton.emat` / `LF_VodkaBottle.emat` (inherit the vanilla material, swap only the colour map) in `LF_Everon1989/Assets/Items/Contraband/`: the vanilla atlas with the pack's and the label's patches repainted in US English for two invented brands, GANNET filter cigarettes and CORVIN vodka (`node tools/assets/contraband-labels/build.mjs`; the vodka preview camera is turned to the label front). Test `everon/contraband-items` (pack leg).
- 🧪 (2026-11-23) **Police car pool board: draw your rank's car into the bay, in the POLICE livery (#4352).** `LF_PoliceCarPoolBoard.et` (core, `Prefabs/Props/Police/`, the vanilla motor-pool cork board mesh) carries `LF_PoliceCarPoolComponent` (the bay: `m_vBayOffset` in the board's frame, default 6 m in front, and `m_fBayYaw`) and four `LF_PoliceCarPoolDrawAction` rows, slot 0 to 3. A slot is the fleet table's cars in ladder order with repeats dropped (`LF_PoliceCarPool.Cars`), named for the lowest rank that draws it ("Take the Cadet car", "Take the Patrol car"); a slot past the table hides, and the rows show only to police on duty (replicated account state). A car above the officer's rank stays on the list greyed with "That car is for the rank of Patrol and up." The server (`LF_PoliceCarPool.Draw`) checks, in order: police on duty (`#LF-CarPool_OffDuty`), within 5 m of the board (`#LF-CarPool_TooFar`), 10 s since this officer's last draw (`#LF-CarPool_TooSoon`), `LF_RankFleet.MayDraw` (the rank refusal), the officer's last pool car empty (`#LF-CarPool_CarOut`), and no other vehicle within 3.5 m of the bay (`#LF-CarPool_BayBlocked`). Then the officer's last pool car is deleted (one car per officer), the new one spawns in the bay as an unlocked service car (only an officer on duty opens it), is held out of the world save (gone at a restart) and settled on the ground; the officer gets "Your car is in the bay." Logs `[LFDebug:CarPool]`. The Everon fleet rows now name the pack liveries built with `tools\cli livery build` (WO-L4's police part): `LF_S105_Police` (the S105 joined the generator: its doors are animated parts of the body mesh, so one body material repaints them; white; POLICE plates sit on both rear quarter panels, which are fixed body, so they stay put when a door opens: two child `LF_SignComponent` plates in `LF_S105_Police.et` reusing the `livery_uaz469_police` sign id, #4372; written by `livery build` from the row's `panels` since #4559) for the Cadet and `LF_UAZ469_Police` (white, navy trim, POLICE plates on the front doors) for Patrol and up. Test `police/car-pool` (WORLD); the board stands with both cars in grid cell `police-car-pool` (Everon1989Grid). On the island the board hangs on the east wall of the Saint-Philippe police villa (`Fit_saint_philippe_police_cars_board` in `Place_saint-philippe_police-cars.layer`, seated with the town-drops wall seat because `toolsi place` reads only DebugWorld and the pack annex), its bay on the lot beside the parked-car markers (#4372).
- 🧪 (2026-11-23) **Police prisoner van: the S1203 in the POLICE repaint (#4469, WO-L4).** `tools\cli livery build S1203_Police` wrote `LF_S1203_Police` (pack `Prefabs/Vehicles/Liveries/`, body material `Assets/Liveries/S1203/LF_S1203_Police.emat`, a credit row in `docs/credits.md`): the vanilla `S1203_transport_beige` with the body material repainted white and layer 2 off. It is the prisoner transport, not a rank car, so the rank fleet table is unchanged (Cadet `LF_S105_Police`, Patrol and up `LF_UAZ469_Police`); nothing spawns the van yet. No door plates on it: the van's doors are not separate parts (`tools/livery/vehicles/S1203.mjs` `doors: null`). Known: in a full-profile session close up the van's sides read dark blue-grey with a white roof, while `look item` shows it white (follow-up).
- 🧪 (2026-11-23) **Josselin the barkeeper: lines, prices and weekly caps (#4439, CR-B5 of 0.3.0; the plan's Josette).** Pack NPC `Configs/Npcs/LF_Npc_Townsfolk_Barkeeper.et` (outfit `LF_Outfit_Barkeeper.conf`: cotton shirt, dark trousers; no vanilla apron exists, so none yet) carries the core `LF_CounterOutletComponent` (buyer id `bar_chotain`) with the "Sell under the counter" action beside his talk action. He is a man because every character body is male and `validate-voice-sex` refuses a woman's name or voice on one. The pack's `LF_BuyerDemand_Bars_Everon1989.conf` (its own file, listed in both mission headers) gives each bar weekly period rows (`m_iPeriodHours 168`): `bar_chotain` takes 2 cartons at $150 and 6 vodka bottles at $45 ($570 a full week), `bar_levie` 2 cartons at $155 and 5 bottles at $52 ($570), inside the $500 to $650 expected-clean hour of crime tier 1 in the plan's economy table. The second bar's barkeeper is placed with `m_sBuyerId bar_levie` (CR-B9). Dialogue `barkeeper` (spec `tools/assets/dialogue/barkeeper/dialogue.json`, five lines, every one recorded in the ElevenLabs voice Hank Harvey, which no other NPC uses): the greeting, what he buys ("Two cartons a week, a few bottles. More than that and the owner notices."), why only two cartons, the law ("If you wear a badge, I sell beer.") and a goodbye; captions `LF-Dialogue_Barkeeper_*`, name `LF-Townsfolk_Barkeeper_Name`. Not voiced: the toasts of the sale action ("Not with that badge on, officer.", "That's enough for this week."), which are core notifications and not dialogue lines. **Not placed yet:** the two bars are CR-B9_3 and CR-B9_4. Test `everon/barkeeper` (pack leg).
- 🧪 (2026-11-24) **Levie bar: its own barkeeper id (#4702, CR-B9 of 0.3.0).** Both bars placed `LF_Npc_Townsfolk_Barkeeper` with the Townsfolk id `barkeeper`, so the server refused the second (`[LFDebug:Places] register refused: Townsfolk 'barkeeper' is already placed`) and one bar had nobody to sell to. The Levie bar now places the pack variant `Configs/Npcs/LF_Npc_Townsfolk_Barkeeper_Levie.et` (inherits Josselin's prefab; place id `barkeeper_levie`, outlet buyer `bar_levie`), so the layer carries no component override. Dialogue, standing and the `bar_levie` demand rows (weekly caps per outlet, #4439) are unchanged. Island session (`session up --mission island`): no `register refused`, both ids log `placed Townsfolk`, and `use LF_DialogueTalkAction` answers at both counters. The sell row reports `cannot_perform` at both with nothing to sell in hand (the same at Chotain). `tools/town/kits/village-bar-1i04.json` still names the old prefab: a `town fit levie pub` refit would undo this until that kit row follows (follow-up).
- 🧪 **Run plate on the car in front** (#4425, PO-hotlist; merged, in-dev). `LF_RunPlateAction` (`Feature/Police/LF_RunPlate.c`; the row sits beside the unit-status rows on `LF_UAZ469_Police.et` (context `default`: the door contexts the unit-status rows use are not offered from the seat), and on the white S105 police car as an `actions` row of `tools/livery/rows/police.mjs` that `livery build` writes, so a rebuild keeps it, test `everon/run-plate-row`) is offered to an on-duty officer in the driver seat of a police car. The server re-checks the real account and the seat, finds the nearest vehicle within 30 m in a forward cone itself (`LF_RunPlate.FindAhead`; a client never names a plate or a car), reads its plate and asks the hotlist (`LF_StolenVehicles.CheckPlate`). The answer is one pager line, "Plate AB CD 1234: STOLEN | CLEAR | UNKNOWN", with the car's place (`LF_Pager.BuildParts` shows it, no window). Refusals are toasts: off duty, not at the wheel, no car ahead. With no pager on the officer the answer is the old plate-check toast. Test `police/run-plate`. Session proof (2026-11-23, island, two players): `session.mjs run-plate-island-proof` (the `parkat` harness verb sets the stolen car 8 m ahead of the police car) reads `Plate <plate>: STOLEN at Saint-Philippe` on the pager. Manual: report a car stolen as its owner, drive a police car behind it on duty, press Run plate, read the pager; a registered car reads CLEAR; off duty the row is gone.
- 🧪 (2026-11-23) **911 abuse: three unfounded calls a day file the charge (#4421, PO-D2 of 0.3.0).** `LF_CallAbuse` (`Police/LF_CallAbuse.c`, server only) counts each call an officer closes as `UNFOUNDED` (a `modded class LF_CallOutcome` hook on `Close`, after the close succeeded) against the call's caller (`LF_PoliceCall.m_sSuspectUid`, the uid the call carries; an officer closing their own call counts nothing). The third unfounded close in one in-game day (`LF_ConvictionRecord.Today()`) files one report through `LF_WantedReports.Add` with the pack law book's emergency line abuse code `#LF-Ticket_OffenceEmergencyLineAbuse` (CRIMINAL, $200, no wanted level; it shows on the charge sheet) and marks the caller; a fourth the same day files no second report. From then on every call posted for that caller (`modded class LF_PoliceCalls.Post`) carries `LF_PoliceCall.m_bLowPriority`; `LF_CallAbuse.BoardTag(call)` returns the board text `#LF-CallAbuse_LowPriority`. The day roll clears counts and marks. Session only: counts and marks are not saved, so a restart starts the day clean. Every count, charge and mark logs under `[LFDebug:Police]`. Not done here: drawing the tag on the dispatch board (`LF_Dispatch.BoardLine`, outside this card), putting marked calls last in `OldestOpenFor`, and the in-world action row that calls `Close`. Test `police/call-abuse` (LOGIC).
- 🧪 (2026-11-23) **Close a call from the car radio; low-priority calls tagged and handed out last (#4585, follows PO-D1 and PO-D2).** The core police UAZ469 (`LF_UAZ469_Police.et`, so the pack's Patrol car too) carries three more radio rows beside the unit status rows: "Radio: call resolved", "Radio: no one found" and "Radio: call unfounded" (`LF_CallCloseAction`, `m_eOutcome`, at the driver door or in the cab). The prompt shows to an officer on duty; the server re-checks the duty bit from the account, the 6 m reach to the car, finds the call the officer took at the board, the nearest to them when they hold several (`LF_PoliceCalls.HeldBy(uid, position)`, keyed by the same character key as the take) and closes it through `LF_CallOutcome.Close`, which refuses a call held by someone else and a close more than `callRadius` from the scene. So the $15 resolved bonus and call XP now run in the world. 911 abuse counts only a call's caller (`LF_PoliceCall.m_sCallerUid`, set with `LF_CallAbuse.SetCaller`): a call posted from a crime report carries the wanted suspect's uid and no caller, so closing it unfounded charges nobody (the #4421 slice counted the suspect). No player 911 call posts a takeable call yet, so the abuse count stays idle until a follow-up card does. No call held reads `#LF-CallOutcome_NoneHeld`. `LF_PoliceCalls.OldestOpenFor` hands out a normal call before any low-priority one, however old. A dispatch log record keeps `m_bLowPriority` when the service call's caller was charged today and the call is not a crime report (`LF_Dispatch.Report` publishes those with the suspect's uid and is skipped); the board entry's third field is now a bit set (`LF_Dispatch.FLAG_LOUD`, `FLAG_LOW_PRIORITY`) and `LF_Dispatch.BoardLine` appends the tag (`#LF-Dispatch_BoardTagged`). The cadet S105 and the S1203 van have no radio rows yet. Tests `police/call-close-radio` (LOGIC), `police/call-close-radio-car` (WORLD).
- 🧪 (2026-11-23) **Player 911 calls become takeable police calls; radio rows on the S105 and S1203 (#4606, follows #4585).** A police service call a player phones in (payphone dial or preset, a carried radio set, the debug test call, every `LF_ServiceCalls.Place`) now posts a `PHONED` call (`LF_PoliceCalls.KIND_PHONED`, `PostPhoned(callerUid, site, hasPosition, position)`) when `LF_Dispatch.Receive` logs it and it is not a crime report. The call has no suspect; its scene is the phone's position, its rank Patrol, and `LF_CallAbuse.SetCaller` names the caller, so the 911-abuse count, the low-priority mark and the board sort from #4421/#4585 now run in play. The chain smell lead is an automatic tip (`Place(..., tip: true)`, `LF_OnServiceCall.m_bTip`): it posts a takeable call that names no caller, so the player who noticed the smell is never charged. One open phoned call per caller (`FindOpenPhonedBy`): calling again while it is open posts nothing new. The caller uid is always the server's key for the user entity at the phone (`LF_Payphone.Use`, `LF_PhoneCalls.ServerDial`, `LF_HandsetCall`, `LF_DebugDispatch`), never a client value. `FindOpenFor("")` finds nothing, so a phoned call never answers a suspect lookup. `LF_CallOutcome.Close` pays no bonus and no XP to an officer closing a call they phoned in themselves. Logs `[LFDebug:PoliceCalls] call #N phoned in by ... is takeable at the board`. The Cadet's S105 and the S1203 van now carry the police car rows (unit status available/busy, "Radio: call resolved / no one found / call unfounded", Run plate) at the driver door: core bases `Prefabs/Vehicles/Wheeled/S105/LF_S105_PoliceBase.et` and `S1203/LF_S1203_PoliceBase.et` hold them, and the livery rows (`tools/livery/rows/police.mjs`) inherit those, so `tools\cli livery build` keeps them. Tests `police/phoned-call` (LOGIC), `everon/police-car-rows` (pack leg).
- 🧪 (2026-11-23) **Medic fleet: the clinic car pool hands each medic rank its ambulance (#4432, S1 of 0.3.0).** `LF_MedicFleet` (`Feature/Medic/LF_MedicFleet.c`, a copy of `LF_RankFleet` keyed by medic rank id, root class `LF_MedicFleetConfig`, pack overlay `LF_MedicFleet_Everon1989.conf`; the core names no vehicle, so the old `LF_M997_Ambulance` is no longer handed out) and the pool (`LF_MedicFleetPool.c`: `LF_MedicCarPoolComponent` bay, `LF_MedicCarPool.Draw`, `LF_MedicCarPoolDrawAction`, slots 0 to 2 on `LF_MedicCarPoolBoard.et`, core `Prefabs/Props/Medic/`). `Draw` checks in order a medic on duty (`#LF-CarPool_OffDuty`, so police and off-duty medics are refused), 5 m from the board, 10 s since the last draw, `LF_MedicFleet.MayDraw` (`#LF-RankFleet_NeedsRank` naming the rank), the last car empty, and the bay clear; one ambulance per medic, held out of the world save, unlocked service car. The medic rank rides in the replicated account state (`LF_AccountStateBits` bits 17 to 24, `GetMedicRank`; `SetMedicRank` publishes it, #4566), so the board offers a medic on duty only the ambulances the rank may draw; the server still checks the real account and refuses a rank that is too low in words. The Everon pack gives Orderly and EMT `LF_UAZ452_AmbulanceFleet` and Paramedic and Doctor `LF_S1203_AmbulanceFleet` (`Prefabs/Vehicles/Medic/`, inheriting the WO-L5 white liveries with lock, store-here, search, unflip, repair kit and the ambulance siren and light bar; pay untouched). `LF_VehicleLockComponent.m_bMedicService` makes a service car open for a medic on duty instead of an officer. Logs `[LFDebug:MedicCarPool]`, `[LFDebug:MedicFleet]`. Tests `medic/fleet` (WORLD) and `everon/medic-fleet` (pack leg). The Everon overlay `LF_Medic_Everon1989.conf` sets `m_aStretcherVehicles` to the two fleet ambulances (a pack list replaces the core list whole, `LF_MedicConfig.ApplyStretcherVehicles`; the core `.conf` keeps the M997 for the debug world), so a downed patient seated in either does not bleed out and `LF_BedDelivery.IsStretcherPrefab` answers for a prefab without spawning it (#4566). On the island the board hangs on the back wall of the Saint-Philippe clinic (`Fit_saint_philippe_clinic_medic_fleet_board` in `Place_saint-philippe_clinic.layer`, seated with the town-drops wall seat through the `medic-fleet-board` entry in `DROP_KITS`, because `tools\cli place` reads only DebugWorld and the pack annex); its bay is moved to local `9 -1 5` (the default 6 m straight ahead lands on the taxi cab parked in front of the clinic, and the bay test reaches 4 m past a car's bounds), 7.9 m from the cab on the open cobbles (#4566). Light bars, checked in a full-profile island session beside the drawn cars (#4566): the UAZ452 bar was buried in the roof at 1.95 m and the S1203 bar floated 0.4 m above it at 2.3 m (the S1203 mesh's 2.69 m bound is not its roof, which stands near 1.89 m on the vehicle origin), so the bars now sit at 2.06 m (UAZ452) and 1.85 m (S1203) with their bases on the roof (`LightBarLED.xob` runs 0.03 to 0.13 m above its origin). The game-mode prefab carries the vanilla `SCR_SupportStationManagerComponent`, so the drawn ambulance's vanilla heal and load-casualty support stations register instead of raising two VMEs at every draw (#4878, test `medic/support-station-manager`). The bay is blocked by a vehicle whose origin is within 4 m of the bay centre, not by one whose bounds reach it, and a refusal logs the blocker's prefab, id and distance (#4878, test `medic/bay-edge`; the police pool still tests bounds).

- 🟡 **Vince speaks at the counter** (#4620, slice of #4440; WORLD `crime/fence-voice`, pack `everon/fence-voice`). `LF_FenceVoice` (`Feature/Crime/`) is called from `LF_ShopComponent.Buy` on a fence: a record refusal (`NOT_TRUSTED`, any gated tool) says `refuse_drill`, a completed drill sale says `sell_drill`, any other sale is silent. The line comes from the fence NPC's own conversation (his role dialogue plus the pack patch), so a server with no recording for it stays quiet and logs it. The server sends the buyer's client one RPC (`LF_FenceSay`: fence entity id, line id) that plays the clip from his head, and a toast under "The Fence" carries the caption for the clip length plus 1.5 s. Only the buyer hears it. No save change.
- 🧪 (2026-11-23) **Speed-limit signs in US English: SPEED LIMIT 30 / 50 / 100 KM/H (#4434, PO-R5 of 0.3.0, first half).** Pack prefabs `LF_Everon1989/Prefabs/Props/Signs/LF_SpeedLimit30.et`, `LF_SpeedLimit50.et` and `LF_SpeedLimit100.et` inherit the core pole plate (`LF_Sign_PolePlate`, a vanilla sign post) and paint a white face with black lettering through the pack sign file `Configs/Signs/LF_Signs_Everon1989/LF_SpeedLimit.conf` (ids `speed_limit_30`, `speed_limit_50`, `speed_limit_100`; title `SPEED LIMIT <n> KM/H`, in km/h because the radar gun measures km/h). The Read card says where each limit applies (30 near markets and squares, 50 through town, 100 on the open road). Strings are pack rows `LF-Sign_SpeedLimit<n>_Title/_What`; `validate-us-english-signs` passes on them. Workbench draws no sign face, so a `look` shows a white plate; the lettering is for a rendering client. The lay-by kit is built (second half of #4434): the pack grid cell `speedtrap` (`c29r0`, 2 x 1 cells, `look grid:speedtrap`) holds the three signs on poles at the far end, a vanilla `RoadBlock_01` barrier as the end stop, four `GravelPile_01_base` piles at 0.5 scale along the back edge and the S105 police car (`LF_S105_Police`) on its patrol spot; no traffic cones. `tools/town/kits/speed-trap.json` (nine rows, origin the lay-by centre) is the kit `town fit` places on a real pull-in. A full-profile session read the three faces (`SPEED LIMIT 30`, `50`, `100 KM/H`). Island road placement is PO-R10.
- 🧪 (2026-11-23) **Pharmacy cabinet loot as chain goods with a narcotics class (#4447, CR-D3 of 0.3.0).** `LF_HospitalLoot` (`Feature/Crime/LF_HospitalLoot.c`, server) picks a cabinet's haul: its own `m_aLootPrefabs` wins, else the pack's `LF_HospitalLootConfig` (`LF_Everon1989/Configs/Crime/LF_HospitalCabinet_Everon1989.conf`, listed in the pack header), else the core `LF_Hospital.FillDefaultLoot`. Each config line is a vanilla prefab, a count (1 to 20) and a narcotic flag; the island cabinet yields 4 morphine ampoules and 4 aspirin bottles (narcotic) plus 2 field dressings, a saline bag and a tourniquet. A narcotic prefab is registered with `LF_VehicleSearchGoods` as `NARCOTICS` when the cabinet configures (a vanilla prefab cannot carry the marker), so a stop search lists it; `HaulClass` maps the narcotic units to `LF_GoodsClass` (this haul: 8 units, class B). Every item goes into a backpack (#4369). No price, buyer or charge yet: the buyers and the law book read the class. No save change. Test `everon/pharmacy-loot` (pack leg).
- 🔧 (2026-11-24, #4180 round 3 G3) **The clinic cabinet lists its own row first.** `LF_HospitalCabinet.et` (and the `Grid_hospital.layer` copy of its action list; the frozen DebugWorld copies keep no priority) give `LF_HospitalCabinetAction` "Breach the cabinet" Sort Priority 1 and `LF_BreachRepairAction` "Repair breach" Sort Priority 9: the action list sorts lowest first and unsorted rows have no promised order, so the greyed repair row led. The island clinic cabinet inherits the prefab. Check: `node tools/validation/hospital-cabinet-action-order-selftest.mjs` (red proof: with the priorities removed it prints 12 FAIL lines). Merged, in-dev.
- 🧪 (2026-11-24) **Speed trap on the Meaux road: the lay-by and the limit signs stand on the island (#4708, PO-R10 of 0.3.0).** `town fit meaux speedtrap` puts the `speed-trap` kit on free plot p5, a grass verge on the east side of the road that runs south out of the Meaux square (hostless, rows on the probed ground; `Place_meaux_speedtrap.layer`, claim `meaux-speedtrap`, mapping slot `speedtrap` and an optional `speedtrap` slot in the village template). The S105 patrol car parks parallel to the road with its nose to a `RoadBlock_01` end stop, clear of the asphalt and a gate's door swing. The three limit signs stand on the verge facing the drivers they speak to: 30 (south end, northbound drivers entering town), 100 (further south on the open-road approach) and 50 (north end, southbound drivers leaving town). The kit now runs north-south along the road instead of end-on as in the grid, and leaves out the four gravel piles, because `town fit` does not carry the grid's 0.5 scale and each pile would stand 8 m across. Workbench draws no sign face, so `look` shows white plates; the lettering needs a rendering client. `audit placement --world MainWorld --layer Place_meaux_speedtrap.layer --strict` is clean. No traffic cones exist in the vanilla set.
- 🧪 **Playtest town jail work stations (#4679, 2026-11-24, merged, in-dev):** `Everon1989Town_Layers/Village_Jail.layer` places the island's three jail stations (`LF_JailLaundryTub`, `LF_JailKitchenSink`, `LF_JailYardBroom`, jobs `laundry_tub`, `kitchen_sink`, `yard_broom`) in the town's walled yard, south of the cell door's walk-up, so a jailed player can shorten the sentence with the island's props. The core `Roadmap_Jail_ScrubBucket` and `Roadmap_Jail_LaundryBasket` stay (core DebugWorld proof `jail-work-proof`). The cell and outer doors already matched the island's (`EL_JailDoor`, #4336). Proof: session, `Sign_DebugJailMe`, work at the tub (`jail` 29 to 19 s), `stand Roadmap_Jail_Door` ends at the cell origin (the locked door keeps the prisoner in).
- 🔧 (2026-11-27, crime playtest, `docs/playtest/agent-playtest-crime-2026-11-27.md`) **Bail no longer ends with "Sentence served".** A prisoner who paid bail read "Bail of $2,250 paid from your bank. You are free to go." and then "Sentence served". `LF_Jail.EndOnBail` completes the sentence with no served toast (`Complete(uid, false)` -> `Release(uid, false)`); a served sentence still says it. `LF_Jail.ReleaseToastsQueued()` counts the queued toasts for tests (LOGIC `police/jail-bail`). Money and release are unchanged.
- 🔧 (2026-11-27, crime playtest) **The searched player is told what the police took.** Only the officer got "Seized from <name>: M9"; the suspect saw nothing. `LF_ConfiscateAction.NotifySeized` now also sends the suspect `LF_ItemSearch.TakenText` ("The police took: M9", `#LF-Search_TakenFromYou`, the same three-names-and-a-count list), or `#LF-Search_TakenPlain` when only chain goods went. WORLD `catalogue/search-seized-list`.
- 🔧 (2026-11-27, crime playtest, #5145) **The search toast names only a suspect the officer was introduced to.** It
  used the account name (`LF_Utils.GetPlayerName`) for a stranger. `LF_ItemSearch.SuspectNameFor(officerUid,
  suspectUid)` gives the character name when `LF_Introductions.Knows(officer, suspect)`, else "an unknown person"
  (`#LF-Search_UnknownSuspect`): "Seized from an unknown person: M9". WORLD `catalogue/search-suspect-name`.
