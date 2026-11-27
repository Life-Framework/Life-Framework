# Character & Survival

> **In short.** A lethal hit knocks you down instead of killing you, and you have a few minutes to be bandaged, revived by a medic or loaded into a car for the clinic. When a medic is on duty, hurt residents go down at work sites (the mine, the timber yard, the farm, the dock, a roadside stop) and show up as **medic calls** on the clinic's dispatch board, on the medic's pager and as marks on the map. A medic drives out, holds Treat the patient or carries the casualty to a clinic bed, and earns pay and medic experience for it; an untreated call leaves the board and the resident does not survive. This is built and in the current build; it is still being playtested. The rest of this page is the engineering record.

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
- ✅ (2026-11-18, #3359) **A corpse stops draining.** A respawned body shares the character's stats object with
  its corpse; the corpse's tick kept running until cleanup (10+ min), so hunger and thirst fell twice as fast.
  `Tick` now ends the body's own tick and drops its stats reference once the body is dead. Test
  `survival/corpse-no-drain` (drives `TickAt(nowMs)` on both bodies).
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

### `LF_BetaHud` subtle HUD — `HUD/LF_BetaHud.c`, `HUD/LF_HudLogic.c`, `HUD/LF_HudBankMirror.c`
- ✅ (2026-11-22) The bottom-centre site URL is gone. Small brand mark top-left (`LF_HudBrandMark.imageset`, 32 %
  opacity); cash and bank stacked top-right with a sliding "+$100"/"-$18" delta, dimmed to 40 % after 5 s idle and full
  while the inventory or map is open; up to five wanted badges (`LF_HudWantedBadge.imageset`, our own shield glyph)
  under the money, red flash and a "Wanted" caption on a rise, faded out at 0; food and drink as label + number at the
  bottom centre, hidden while both are at 50 or more, shown below 50 or for 4 s after a rise, red below 25. Health and
  stamina keep their bottom-right gauges.
- Reads only values already replicated to the owner: cash from the inventory invoker, wanted from the packed account
  state, hunger and thirst from the survival RplProps, and the bank from `LF_HudBankMirror`: an owner-only RplProp on
  `SCR_ChimeraCharacter` the server writes on spawn and on every bank `LF_OnMoneyMoved` (display only, nothing trusts it).
- Replicated values are polled every 0.25 s; widgets change only when a value or a target does. Hidden behind
  engine menus (pause, settings, dialogs) and during dialogue; a UI kit sheet is not an engine menu, so the HUD stays up beside one (`LF_DialogueScene`, `LF_DialogueCamera`).
- Bank shows a debt as `-$150`; the mirror refreshes on the 10 s account refresh as well. Harness verb `hud <wanted> <hunger> <thirst> [bankDeposit] [hold]` (dev gate) sets up the states for pictures.
- `/hud <brand|money|wanted|needs|all> <on|off>` toggles a part; kept per client in `$profile:LF_HudPrefs.txt`.
- Test `hud/subtle-rules` (LOGIC): show and low thresholds, dimming, delta and bank text, pip clamp and rise, /hud
  parsing and the saved line.

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

### Eat, drink and throw — `LF_ConsumableEffectEat.c`, `LF_FoodThrow.c` (2026-11-19, play report: "no eating or drinking action or animation")
- 🧪 `LF_ConsumableEffectEat` is the one shared base of `LF_ConsumableEffectSurvival` and `LF_ConsumableEffectImpair`; the
  settings live on `ConsumeableItem_Base`, `Food_Base` and `Drink_Base`, so every food and drink gets them (an inventory
  Use entry, Use when held, the use animation) and a new item sets only its numbers. The animation is the vanilla canteen's
  (`Canteen_Soviet` item graph, `CMD_Item_Action`, hand to mouth) for food and drink alike.
- 🧪 Hunger or thirst is restored on the server when the use animation completes, once per item (`LF_ConsumeRules.Claim`); a
  stack of food gives up one unit. Refused while dead, downed or unconscious, cuffed, seated in a vehicle or swimming
  (`LF_ConsumeRules.RefusalFor`, checked at the start on the client and again on the server), with a toast. The server log
  line `consumed <prefab>: hunger a -> b, thirst c -> d` is the proof (`session.mjs food-proof`).
- 🧪 The engine can clear the gadget's owner before it reports the use finished, which made vanilla drop the effect without a
  word (the food was gone, nothing restored); the owner recorded at `ModeSwitch` is restored for these items.
- 🧪 Throw: the `LF_ThrowHeld` input (G, gamepad right shoulder plus B) with food or a drink in the hands. The server takes the
  item out of the inventory and launches the item's own rigid body; the launch goes to every machine through the item's own
  RPC (a velocity set on the server alone left each client's copy lying at the thrower's feet). It lands as a plain world item,
  no damage, and can be picked up again. Tests: `LF_Test_FoodConsume`.

### `LF_Impairment` and `LF_ConsumableEffectImpair` — `Feature/Survival/` (#1919)
- ✅ Any consumable can carry `LF_ConsumableEffectImpair` (strength, duration,
  `LINEAR` or `EXPONENTIAL` curve, stack cap). Use on the server adds a dose to the
  drinker's level (0-100), held in memory per character id by `LF_Impairment`.
  Linear falls to zero over the duration; exponential halves every fifth of it.
  Doses stack up to the cap. The survival tick decays it; death or the body being
  deleted clears it, and so does a restart (persisting is not built, off by default).
  The owner reads `LF_CharacterSurvivalComponent.GetReplicatedImpairment()`, an
  owner-only RplProp bumped on a 5-point change, start or end. Logs
  `[LFDebug:Impairment]`. Test: `survival/impairment`.
- ✅ A drink's numbers come from a table (#2002): `LF_DrinkImpairmentTable` reads
  `Configs/Survival/LF_DrinkImpairment.conf`, rows keyed by the drink's prefab GUID
  (strength, duration, curve, cap), and a pack overlays rows with the override verbs
  (`m_sOp` add, replace, patch, remove) like any other list. The core file ships no
  rows; `LF_ConsumableEffectImpair` uses the row for the consumed item's exact prefab
  and its own attributes when there is none. A row naming a prefab that does not load
  logs `LF_Debug.Error` (`[LFDebug:Impairment]`) and is skipped. Pack-only: no owner
  `config.json` section yet. Test: `survival/drink-impairment-table`.
- ✅ `BeerBottle.et` and `Moonshine.et` (`Prefabs/Items/Drinks/`, #2001) are `Drink_Base` drinks that carry the
  effect at its defaults (no table row yet). Test: `drinks/alcohol`.
- ✅ A drink quenches and impairs in one sip (#2159): the engine takes one consumable effect per item, so
  `LF_ConsumableEffectImpair` carries `m_fThirstRestore` and calls `LF_CharacterSurvivalComponent.Drink` itself
  (nothing when the Survival system is off). The amount lives in the prefab, not the impairment table: beer 10,
  moonshine 3 (water restores 25). Test: `drinks/quenches`.
- ✅ 1989 Everon drinks impair (#2003): the pack overlay `LF_Everon1989/Configs/Survival/LF_DrinkImpairment_Everon1989.conf`
  (listed in the pack mission header) adds a row per drink prefab GUID. Beer: 12 points, linear over 10 min, cap 45.
  Moonshine, the strongest: 40 points (one bottle reaches the full sway of #1920), exponential over 20 min, cap 90.
  Pack data only; the core table still ships no rows. Test: `everon/drinks-impair` (pack test, run under the pack header); the overlay mechanics: `survival/drink-impairment-table`.

### `LF_ImpairmentView` — `Feature/Survival/LF_ImpairmentView.c` (#1920)
- ✅ While the owner's replicated impairment level is above zero their view sways and
  the aim drifts: each frame the change in a slow two-sine yaw/pitch offset is added to
  the local character's `CharacterInputContext` aim change, so view and aim move
  together and stay where the player left them once the level is gone. Client only: the
  owner-only level is 0 on every other body, and the frame skips any body that is not
  the local controlled entity.
- Amplitude is `max * (level / fullLevel)^power`, zero at level 0, capped at the maximum
  (`LF_ImpairmentViewConfig`: full level 40, power 1.5, max 1.6 degrees yaw, pitch 60%).
  Script defaults, no `.conf` yet. Test: `survival/impairment-view`.
- ✅ Cost at 128 players (#2404): the frame is armed only on the local controlled body, and only while it
  sways or has an offset left to unwind. A level change (the owner-only RplProp callback, or the
  server on a listen host), a control change (`SCR_PlayerController.OnControlledEntityChanged`) or the
  harness pin arms it; the frame clears itself once the sway is unwound. Before, every survival
  component armed `EOnFrame` on every rendering client, proxies included (127 early-return calls a
  frame per client at 128 players). Tests `survival/impairment-view-frame`, `perf/life-128`.
- Harness `impair <0-100|off>` pins the local view level for a session proof.

### `LF_SurvivalStatsSaveData` — `Feature/Survival/Persistence/`
- ✅ Save/apply round-trip re-clamps through setters (corrupt save sanitized).
- ⚠️ `Equals` uses exact float `==` (no epsilon).

### Downed state — `Feature/Medic/` (M2 #255, 2026-11-09)
- ✅ **Siren and lights on service vehicles (#263).** Two placed service vehicles carry the existing driver-seat siren (`LF_SirenManagerComponent`, `Feature/SirenLights/`): the police car `LF_UAZ469_Police.et` (a covered UAZ469 with the blue bar seated on its roof, #595; it was an open M151A2 jeep whose bar floated) at the station (`Roadmap_Police_Car`) and the new `Prefabs/Vehicles/Wheeled/M997/LF_M997_Ambulance.et` (vanilla `M997_maxi_ambulance` plus the red `LightBarLED_Red.et` bar and the mod siren sounds) at the hospital (`Roadmap_Hospital_Ambulance`). The driver cycles modes with the `LF_CycleSirenModes*` keys (mouse button 2 or `gamepad0:thumb_left`, single and double click); the server decides in `ApplyRequest` through the pure `LF_SirenRules.Judge`: a mode out of range, the mode already running, or a change within `MIN_TOGGLE_MS` (300 ms) of the last is refused and broadcasts nothing, so a toggle replicates once per change. The mode is an `RplProp` (`OnModeReplicated`, `BumpMe` on each accepted change) plus `RplSave`/`RplLoad`, so a client that streams the car in later or joins late applies it, not only clients that had the car streamed in at the toggle (#471); `session.mjs siren-late-proof` (`up --clients 1`) proves a late joiner. Test `vehicles/siren`; `session.mjs siren-proof` (harness verb `siren <Entity> <mode>`, dev gate) drives it and checks the second client. Logs `[LFDebug:Siren]`. On duty only (#464): `RequestBy` refuses (and logs `[LFDebug:Siren]`) anyone who is not an on-duty member of the vehicle's `m_eService` (police jeep: police, ambulance: medic), so a thief in a stolen police car drives it with no siren; test `vehicles/siren-duty`. The base game has no vehicle siren, only `SCR_SoundEvent.SOUND_SIREN` for structures.
- ✅ **Police siren heard in multiplayer (0.2.0 live fix).** The police knob rows (`LF_SirenModeAction`) used to set the mode only on the machine that ran them, the dedicated server, so the menu showed and no client heard or saw anything. Every siren row now asks the server (`RequestModeStr`: duty rule, throttle, `BumpMe`) and is not broadcast, so each client applies the replicated mode (driver, passenger, bystander, late joiner); an off-duty driver sees the knob rows greyed; a destroyed vehicle turns its siren off for everyone. Test `sirenlights/police-knob`.
- ✅ **Treatment bill on paper, paid at the hospital desk (#260).** A revive (`LF_Revive.Apply`) and a hospital wake after a death each write one `LF_TreatmentBillRecord` in `LF_TreatmentBills` (mirrors the police ticket: number, patient, issuing medic, amount, in-game time) and hand the patient a printed `HospitalBill.et`. Amounts are config: `m_iReviveBillAmount` (100) and `m_fBillMedicShare` (0.5) in `LF_Medic.conf`, `m_iBillAmount` (150, wake) in `LF_Respawn.conf`; the bank is never charged. The patient pays the oldest unpaid bill in carried cash at the hospital desk (`LF_TreatmentBillPayAction`, ledger reason `treatment-bill`, medic as counterparty); the paper is stamped paid. The medic's share of a paid revive bill is paid once in cash at clock-out (`LF_DutyLocker.ClockOut`, ledger reason `bill-share`). An unpaid bill has no consequence in the flagship; it is handed again with every new body. Records persist as the `bills` payload (save version 19); a carried bill comes back blank after a restart and is printed again on spawn. Tests `medic/bill`, `medic/bill-persist`, `respawn/bill-once`. Logs `[LFDebug:Bill]`. **Debt (#464):** a bill unpaid `m_iBillDeadlineHours` (48 in-game h, `LF_Medic.conf`) after it was written becomes debt; `LF_TreatmentBills.Sweep` takes it from the bank as soon as the bank holds money (on every bank credit, on spawn, and a 60 s check), partially if needed, one ledger entry per take with reason `hospital-debt`; the ATM terminal shows an UNPAID DEBT line. The debt lives in the same `bills` record (`m_sPaidAt` = `debt:<left>` while unpaid), so the save layout and version are unchanged; test `medic/bill-debt`.
- ✅ **A medic revives a downed patient (#259).** `LF_ReviveAction` ("Revive", on a player character beside Introduce / Show ID) is shown to an on-duty medic (`LF_MedicDuty`) facing a downed player within `m_fReviveRange` (2.5 m, `LF_Medic.conf`). `LF_Revive.Perform` re-checks on the server (medic on duty, patient downed, in range; `LF_Revive.Decide` is the pure rule), then `LF_Downed.Clear`: the flag, the bleed-out clock and the unconsciousness end, the patient keeps their inventory and the sliver of health, and `LF_OnPlayerRevived` is published on the bus. A civilian, an out-of-range medic and a standing patient are refused with a toast and a log line. Tests `medic/revive`; `session.mjs downed-proof` revives the downed client. A revive uses up one field dressing (`m_sReviveItemPrefab`, `m_iReviveItemCount` in `LF_Medic.conf`; empty = free; refused with `#LF-Revive_NoItem`), stops the bleeding and lifts health to `m_fReviveHealthScaled` (0.25): the downed sliver of health used to bleed away seconds after the wake and down the patient again (#427, found by `session.mjs revive-proof`, 10 of 10 stay awake). **CPR animation (#512):** the revive is a 4 s hold (`Duration 4` on the action). `OnActionStart` asks the server (`LF_AskReviveAnim`, patient as RplId, medic taken from the controller); `LF_ReviveAnim.Begin` re-runs `LF_Revive.Check` and tells the medic's owner (`LF_BroadcastReviveAnim`) to start a custom loiter on `Anims/Workspaces/Medic/LF_ReviveCPR.agr`, an attachment graph on the player graph's `NPC` binding that plays the base-game `p_cro_revive_CPR_in/loop/out` clips (`Gestures` = 560). The base game replicates the loiter to every client, late joiners included; each rendering client logs `revive animation playing` only when it sees the medic loitering with the head bone under 1.3 m (#512; `PlayPostGraphAnimation` never played because the player character has no `CharacterAnimGraphComponent`). The stop is always the fast exit: the CPR out clip has no `Gesture_End` event, so a soft exit would never finish. The server stops it on release (`OnActionCanceled`), completion, the medic leaving reach, the patient no longer downed, or a 7 s overrun (`LF_ReviveAnim.Step`, 400 ms watch). Test `medic/revive-anim`; harness verb `cpr <player> start|stop`, `session.mjs revive-anim-proof`. Logs `[LFDebug:Medic] revive animation ...`. **Medic state and CPR throttle (#2624):** `LF_Revive.Check` also looks at the medic: a medic who is downed or not alive is refused with `MEDIC_DOWN` (toast `#LF-Revive_MedicDown`), in the revive, the CPR start and the 400 ms watch that stops a running CPR. A CPR start from the same medic inside `LF_ReviveAnim.START_COOLDOWN_SECONDS` (0.5 s) of their last one is refused and the first refusal of each window is logged; a repeat start while the CPR runs was already free. Test `medic/revive-medic-state`. First aid (#257) still buys time for anyone.
- ✅ **A medic revives a downed patient (#259).** `LF_ReviveAction` ("Revive", on a player character beside Introduce / Show ID) is shown to an on-duty medic (`LF_MedicDuty`) facing a downed player within `m_fReviveRange` (2.5 m, `LF_Medic.conf`). `LF_Revive.Perform` re-checks on the server (medic on duty, patient downed, in range; `LF_Revive.Decide` is the pure rule), then `LF_Downed.Clear`: the flag, the bleed-out clock and the unconsciousness end, the patient keeps their inventory and the sliver of health, and `LF_OnPlayerRevived` is published on the bus. A civilian, an out-of-range medic and a standing patient are refused with a toast and a log line. Tests `medic/revive`; `session.mjs downed-proof` revives the downed client. A revive uses up one field dressing (`m_sReviveItemPrefab`, `m_iReviveItemCount` in `LF_Medic.conf`; empty = free; refused with `#LF-Revive_NoItem`), stops the bleeding and lifts health to `m_fReviveHealthScaled` (0.25): the downed sliver of health used to bleed away seconds after the wake and down the patient again (#427, found by `session.mjs revive-proof`, 10 of 10 stay awake). ⚠️ No treatment animation (deferred: no animation asset; the revive is instant); first aid (#257) still buys time for anyone.
- ✅ **Revive and Load patient show wherever the medic aims at the body (#5155 follow-up).** The in-game menu is the actions of the one context nearest the view ray. `LF_ID` sits 1.3 m above the character origin, so on a body lying on the ground a medic looking straight down aimed the vanilla `LowerTorso` context (Check injuries, Load casualty, Search) and saw no Revive; only a glance above the body reached `LF_ID`. `LF_ReviveAction` and `LF_LoadPatientAction` are now registered in `LF_ID` plus the eight vanilla body-part contexts (`default`, `Head`, `Chest`, `LowerTorso`, `LeftLeg`, `RightLeg`, `LeftArm`, `RightArm`) in `Character_Roleplay.et`, and Sort Priority -10 / -9 (lower sorts first; vanilla loot is 999) puts them on top in each. Aimed at the clinic doctor or the till the menu is theirs, as for any prop. Test `medic/revive-contexts`; proof `downed-flow-proof` reads the harness `aim` (the real menu, `GetFilteredActions`), not `look` (declaration order, capped at 12 hits).
- ✅ **Medic job at the hospital duty desk (#258).** `LF_DutyAction` takes a role attribute (`m_eRole`, POLICE or MEDIC); `LF_DutyLocker.ClockIn/ClockOut(account, body, role)` swaps inventories through the role's own slot (`police_duty` / `medic_duty`) the same way for both. The medic role is open (no roster); the account is on duty as `LF_Faction.MEDIC` (the persisted faction int, no save format change). The kit is the MEDIC `LF_FactionLoadout` on the game mode (uniform, medical kit, dressings, saline, morphine, tourniquet, `LF_Pager`). Routing reads an on-duty medic as the medic job (`LF_ServiceCalls.JobFor`), so the hospital board, taking a call and the pager reach them. Duty pay: `m_fMedicDutyPayPerSecond` in `LF_Jobs.conf` (0.4), paid in cash at clock-out; medic seconds never count toward police rank. The desk `Prefabs/Medic/LF_MedicDutyDesk.et` is placed at the playtest hospital (`Roadmap_Hospital_Duty_Desk`). Tests `medic/duty-swap`, `medic/duty-desk`; `session.mjs medic-proof`. ⚠️ A medic clocked in at the police desk (or the reverse) is refused (`#LF-Duty_WrongDesk`).
- 🧪 **Medic ranks: rank gates kit and pay (#2708).** `LF_MedicRanks` mirrors the police ladder as its own class (not a shared base, #652). Three built-in rungs, trainee, paramedic, doctor (`LF-MedicRank_*`), or the `m_aRanks` list in `LF_Medic.conf` (`LF_MedicRank`: id, title key, pay per second, duty seconds for the next rung, kit). A rung draws its own kit and every lower rung's (trainee: dressing, tourniquet; paramedic: saline; doctor: morphine); `CanDrawItem` / `CanDraw` refuse a higher rung's item with `#LF-MedicRank_KitRefused` and a `[LFDebug:MedicRanks]` line naming the rank it needs. Duty pay: `LF_DutyPay.RateForAccount` uses the rung's rate (built-in -1 / 0.5 / 0.6; -1 keeps `m_fMedicDutyPayPerSecond`). Promotion: the duty clock (`LF_PoliceRanks.SettleDutyAt`) credits a medic's on-duty seconds to `LF_MedicRanks.AccrueDuty`, one rung at 3600 s and the next at 7200 s total. Rank and medic seconds are per character (`LF_CharacterStanding.m_iMedicRank`, `m_iMedicDutySeconds`), saved by the `medicranks` save section (`LF_MedicRanksSavePayload`, section version 1); a save without the section loads every medic at rank 0. Tests `medic/ranks` (LOGIC), `medic/ranks-persist` (PERSISTENCE), `wt e2e`. ⚠️ No medic kit locker reads `CanDraw` yet: the duty desk still hands out the MEDIC loadout whole.
- ✅ **First aid slows the bleed-out (#257).** `LF_FirstAid`: the vanilla bandage, tourniquet and saline bag (`SCR_ConsumableBandage/Tourniquet/SalineBag.ApplyEffect`, so the vanilla animation and healing are unchanged) add `m_fBandageSeconds` (30), `m_fTourniquetSeconds` (45), `m_fSalineSeconds` (60) to a downed player's clock on the server, never above `m_fFirstAidCapSeconds` (300); morphine adds nothing. `m_bFirstAidAnyone` (flagship on; off = only a player whose job is medic). The same item is counted once. Publishes `LF_OnFirstAid`. Tests `medic/first-aid`, `medic/first-aid-rules`. ⚠️ Not driven in a session: the vanilla bandage may refuse a target with no bleeding wound (manual check in the PR). **Container attribute (modded classes):** each `modded class` of a base-game config class repeats the base's `[BaseContainerProps()]`; without it the config entry fails with `Unknown class` and the vanilla bandage did nothing. `validate-modded-container-props` reads the base through the MCP tools and fails a copy that drops one (skipped in CI); test `medic/consumable-effect-loads`.
- ✅ **Load a downed player into a vehicle (#261; drag and carry cut 2026-11-15).** `LF_LoadPatientAction` ("Load into vehicle") on a player character, context LF_ID, offered to anyone (`m_bCarryAnyone`, flagship on; off = on-duty medics) standing within `m_fCarryRange` of a downed body when a vehicle with a free cargo seat is within `m_fLoadRange` (5 m). The server (`LF_Carry.Load`) re-checks and seats the body through `SCR_CompartmentAccessComponent.MoveInVehicle`, the vanilla casualty path (Game Master and `SCR_LoadCasualtySupportStationComponent` use it), re-asking up to four times while the owning client takes the seat; every load publishes `LF_OnPlayerCarried` ("load"). The bed (#262) takes the downed body lying beside the medic or the downed occupant of a stretcher vehicle. **Cut:** Drag, Carry and Put down, the server hold whose 250 ms tick moved a client-owned body with the Game Master teleport (`SCR_EditableCharacterComponent.SetTransform`): the maintainer saw the body teleport around with no animation. Vanilla has no carry or drag animation and no attach for a player character, and ACE's clips are GPL (`docs/research/animation-pipeline.md`), so there is no smooth path to put back; the actions, the hold, its RplProp pair on the character and the follow tick are removed. The police escort (`LF_JailEscort`) is bookkeeping only and never moved anyone. Tests `medic/carry`; `session.mjs carry-proof` (load only). ⚠️ A load needs a free passenger seat; the driver seat is never used.
- ✅ **Bleed-out timer and give-up (#256).** `LF_BleedOut`: a downed player has `m_fBleedOutSeconds` (180) left on a server-owned clock; one shared 1 s tick counts every downed character down and at zero calls `LF_Downed.Finish` (the real death), attributing it to whoever downed them while they are online. The client reads the time left through an RplProp whole-second copy moved only when it drifts 5 s (no per-frame `BumpMe`). Give-up: hold `LF_GiveUp` (X / pad Y) after `m_fGiveUpDelaySeconds` (30); the request is an RPC on the player controller and `LF_BleedOut.GiveUp` refuses unless the character is downed and the delay has passed. The going-down toast (`#LF-Medic_Downed`) says to wait for help or give up after the delay; a too-early give-up toast (`#LF-Medic_GiveUpTooEarly`) names the seconds still to wait (`LF_BleedOut.SecondsUntilGiveUp`, never under 1; it named the whole delay until the 2026-11-27 medic playtest). **Downed HUD line (#5155):** a small line near the bottom of the screen (`LF_DownedHud`, `UI/Layouts/HUD/LF_DownedClock.layout`) shows the replicated bleed-out time and the wait before give-up (`#LF-DownedHud_Wait`, then `#LF-DownedHud_Ready` once it is open); the wait counts from the replicated seconds downed (`SCR_ChimeraCharacter.LF_GetDownedForShown`, an RplProp moved every 5 s with the bleed copy) plus the client's own time since it arrived, hidden for a stun, a revive and a death. It is asked for from three places through `LF_DownedHud.OnLocal`: the flag reaching the owner, the server setting it on a listen host's own character (`OnServerChanged`; the host gets no replication callback for its own change), and the machine taking control of a body that is already downed (`SCR_PlayerController.OnControlledEntityChanged`; a rejoin into a downed logout body brings no flag change). `session.mjs downed-rejoin-proof` covers the rejoin. Harness `seen` token `downedhud=`. **Action order (#5155):** `Sort Priority` -10 on `LF_ReviveAction` and -9 on `LF_LoadPatientAction` (lower sorts first; vanilla loot is 999 and sits last) in `Character_Roleplay.et` put them above Introduce, Show ID and Give cash for the medic who is offered them. Test `medic/downed-hud-rules`. Disconnecting while downed is death (owner `disconnect.downedDies`, #691). Tests `medic/bleed-out`, `medic/bleed-out-rules`; `session.mjs downed-proof` covers give-up and a full bleed-out. ⚠️ The dimming screen and heartbeat are vanilla unconsciousness only; no custom effect yet.
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
- ✅ Logs `[LFDebug:Medic]` (downed, absorbed hit, cleared, killed for real, search refused). A body under fire logs one absorbed-hit line per 5 s carrying the hit count; a revive logs one wake line (the +1/+4/+9 s follow-ups of #427 are `LF_Downed.SetWakeTrace(true)`, off by default; test `medic/downed-log-rate`, #2582).
  The incident log records each downing as kind `downed` (victim, downer, position).
- ⚠️ Not built (own issues): bleed-out timer and give-up (#256), revive (#259), screen effects for
  the downed client, disconnect while downed. A downed player who disconnects loses the state
  with the body. The suite cannot see the controller's life state (it changes a frame later);
  `session.mjs downed-proof` proves it on a 2-client session plus a late joiner.

### Death & Respawn — `Feature/Character/Spawning/`
- 🔧 (2026-11-16, #1767; merged, in-dev) **A fresh civilian can spawn in a catalogue starting kit.**
  When the item catalogue has offers under the `starting_kit` outlet, `LF_SpawnLogic.GetFactionLoadout(CIVILIAN)`
  returns a loadout built from them (`LF_FactionLoadout.WithStartingKit`, `LF_StartingKit.Resolve`):
  `clothing` items go to loadout slots, every other category is carried. It replaces the game mode's
  civilian loadout whole, including its starting cash; with no such outlet the game mode's loadout
  applies unchanged. Other roles (the police kit) never read it. Proof: `catalogue/starting-kit` (LOGIC).
- ✅ **Death is defined** (verified 2026-08-30): the dead body stays in the
  world with everything it carried — it is never deleted by the spawn flow and
  is only removed by a server restart or world cleanup.
- ✅ **Respawn via the death screen**: `LF_SpawnLogic.OnPlayerKilled_S` no
  longer auto-respawns; it calls `NotifyReadyForSpawn_S`, the client opens
  `LF_DeathScreen` (workspace widget), and its Respawn button runs
  `SCR_RespawnComponent.RpcAsk_LF_Respawn` → `LF_SpawnLogic.RespawnPlayer_S`
  (account-aware, re-applies the faction loadout).
- ✅ **The death screen names the price** (#3152; test `ui/death-screen-cost`): above the Respawn button a
  kit body line reads "You wake at the hospital. The bill is $150. What you carried stays with your body."
  `LF_DeathScreen.CostText` builds it from `LF_RespawnConfig.m_iBillAmount` (the client already has the
  config; a pack's bill shows), a bill of 0 drops the bill sentence, and the currency symbol is the
  `{currency:sym}` term.
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
  player uid: the civilian delivery career pay total, police rank,
  on-duty seconds and unpaid duty seconds, seconds since the last benefit
  collection, licences by enum name, and turned-in quest ids. Since #749 its
  career level and XP and its single player level are a mirror of the DELIVERY
  and CIVILIAN jobs (`LF_JobLevels`, own payload), written in the pre-#749 curves; a
  version 7-27 save's job XP comes from them. A courier at level 2 still opens fuel
  delivery and a senior officer stays senior.
- ✅ Licences and turn-ins live on the character, so their owners write
  through to the store and `LF_SpawnLogic` applies the record to every new body.
  They now survive death as well; only carried items drop. Job levels are keyed by
  the character, not the body.
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

### Logged-out tag — `Feature/Character/LF_LogoutBodyTag.c` (#1974, 2026-11-16)
- A logged-out player's body (#1973) shows a small "Logged out" label over it to everyone within 12 m. The server flags the body (`SCR_ChimeraCharacter.LF_IsLoggedOut`, an RplProp), so a late joiner sees it too; the label goes when the body dies, despawns or its player rejoins (a rejoin that takes the body back clears the flag, #2104, so no client keeps tracking it). The session proof (2 clients, then client 1 as a late joiner, #2104) aims the camera at the body: a late joiner tracks and draws the label from the entity snapshot. Tests: `character/logout-body-tag` (LOGIC); two-client and late-joiner session proof pending.

### Clinic building kit — `LF_Everon1989/Prefabs/Buildings/Clinic/`, `tools/buildings/clinic/` (#2286, epic #2201, 2026-11-17)
- 🧪 The Saint-Philippe `clinic` slot (the converted town house b39, vanilla `House_Town_E_2I02_v2`) fitted as a village practice, a pack kit placed in the pack grid world (`Grid_clinic.layer`, cell c23r0, 2x2): the east wing is the waiting room (benches, the medic dispatch board) closed by the reception counter (the medic duty desk, so clock-in and the treatment bill pay, the hospital till and the doctor `LF_Npc_Medic_1`), the old bedroom is the treatment room (exam couch, the hospital medicine cabinet with its breach flow), the tiled west room is the ward with two `LF_HospitalBed`s (respawn and put-patient-in-bed), the kitchen stays the staff kitchen and the first floor the doctor's flat. The site alarm mount is a bell box on the front wall; a fascia sign (card `hospital`) and a facade glow light the entrance. Every interactable is an existing system (`Feature/Medic/Feature.md`, `LF_Hospital`); the dressing is static base-game props, 6 point lights without shadows. `tools/buildings/clinic/` (`spec.json`, `layout.mjs`, `shell.mjs`, `layer.mjs`, `proof.json`, `looks.mjs`) rebuilds and re-shoots it.
- 🔧 (2026-11-24, #4905) **The clinic fascia reads from the square.** The street board said "Everon Village Hospital", 23 letters that shrank to a thin line beside "CLOTHING" and the pharmacy board. The `clinic.json` fascia row now carries sign `clinic` (pack file `Configs/Signs/LF_Signs_Everon1989/LF_Signs_Clinic.conf`, title `LF-Sign_Clinic_Title` = "Clinic"); its sign card reuses the hospital text and picture. The `hospital` sign stays for the grid world. Merged, in-dev.

### Medic and hospital statics reset at world end (life review, 2026-11-17)
- ✅ `LF_TreatmentBills`, `LF_ReviveAnim` and `LF_HospitalRespawn` (with `LF_NewLifeRule`) register an `LF_StaticResetHook`. Their reset functions had been called only from tests, so a Workbench play restart or a mission change kept the last world's bills, pending wakes and wristbands. It also left the debt-sweep, wristband and CPR-watch flags set, so those timers never started again. Test: `medic/reset-hooks` (LOGIC). Review: `docs/reviews/2026-11-17-life.md`.

### Loading a downed player is not for bystanders (#2405, life review 2026-11-17)
- ✅ `m_bCarryAnyone` now defaults to off: only an on-duty medic or on-duty police may load a downed player into a vehicle (`LF_Carry.MayCarry`, server and the client's offer). A stretcher vehicle (`m_aStretcherVehicles`, its seat pauses the bleed-out, #262) takes an on-duty medic only, whatever the config (`LF_Carry.MayUseVehicle`). In any other vehicle the patient bleeds out as normal. On a stretcher the bleed-out stays paused but the give-up clock runs (`LF_BleedOut.Advance` -> `LF_AdvanceDownedFor`), so a seated player can always give up. Refusal text `LF-Carry_NotAllowedDuty`. Tests `medic/carry`, `medic/bleed-out`. ⚠️ Not covered: an on-duty medic who leaves a patient on a stretcher keeps the bleed-out paused.

### Medic ranks read pack overlays; the 1989 ladder (#3047)
- 🧪 `LF_MedicConfig.Get()` merges every pack overlay the mission header lists over `LF_Medic.conf`: `m_aRanks` is a list keyed by rank id (`LF_MedicRankList`, `LF_MedicRank` is now an `LF_ConfigRecord`), so a pack adds, patches, replaces or removes a rung (`docs/config/override-model.md`). The core file ships no rung, so the pack's rungs are the whole ladder and the built-in three stand only when none is given. A saved rank is the rung's index, so a remove is refused on every rung but the last; a rung with no title key or a pay below -1 is refused. No owner `config.json` section. Test `medic/pack-overlay` (LOGIC).
- 🧪 The 1989 Everon pack (`LF_Everon1989/Configs/Medic/LF_Medic_Everon1989.conf`, listed in the header) sets a four-rung ladder: orderly (0.1333 cash per on-duty second, $480 an hour; dressing, tourniquet), EMT (0.1389, $500; saline bag), paramedic (0.1472, $530; morphine), doctor (0.1556, $560; surgical gauze): the mid-tier rung of the 0.3.0 pay ladder since 2026-11-22 (#4178; it was 0.22 to 0.34 below). Promotion after 1, 2 and 4 hours of total duty; the top rung never promotes. Titles: pack rows `LF-MedicRank_Orderly` and `LF-MedicRank_Emt`, the core rows for paramedic and doctor. The core built-in paid 0.4 (jobs config rate), 0.5 and 0.6, so no rung paid less than before (lead decision on #3051). Maintainer decision 2026-11-18 narrowed the duty-versus-civilian pay gap: the ladder is now 0.22 / 0.26 / 0.30 / 0.34 (about 790 to 1220 an hour against 400 to 480 for a civilian run) and `server/flagship/config.json` sets `medicDutyPayPerSecond` 0.22; the core defaults are unchanged. Test `everon/medic-ranks` (pack leg).
- 🧪 (2026-11-21, #3888 baseline) **The clinic's "Everon Village Hospital" fascia hangs on the street wall.** The kit row stood at host z 5.82, inside the 40 cm front wall, so the stamp found no wall to seat it on and it sat over a window. It now stands outside the wall plane (z 6.2, seated flush by the stamp), 3.5 m up where the wall is solid across its whole 4.3 m width, centred over the door at x -1.5 (at -2.4 and -2.0 the audit finds no standing room on the sign's floor). Kit `clinic` only; no script change.
- 🔧 **HUD (fix-hud-atm, 0.2.1, in-dev):** the needs line at the bottom shows a fork icon and a glass icon (`food`, `drink` in `LF_UIIcons.imageset`, drawn by `tools/ui-icons/build-ui-icons.mjs`) with the figure once either need is under 50 %; the need still above 50 % fades to 40 % (`LF_HudLogic.NeedOpacity`), red under 25 %. The cash and bank block sits on a soft dark plate (`m_moneyPlate`) and the bank line is bold and brighter, so it reads on bright walls and sky. The brand mark is off while the map is open (`LF_HudLogic.BrandVisible`). Test LOGIC `hud/subtle-rules`.

### Casualty calls — `Feature/Medic/LF_CasualtyCall.c`, `LF_CasualtyCallConfig.c`, `Configs/Medic/LF_CasualtyCalls.conf` (#4827, 0.3.0)
- 🧪 Work for a medic on a quiet server. While at least one medic is on duty, every `m_iIntervalSeconds` the server picks a row of `LF_CasualtyCalls.conf` by weight (an `LF_Places` kind, a summary key, a weight), finds a place of that kind, spawns a resident (`m_sPrefab`, `Character_Roleplay` by default) lying hurt within `m_fSpotRadius` of it and places a medic service call through `LF_ServiceCalls.Place` with the grid place and the position: the clinic's medic board, the pager and the dispatch map mark all come from that call. No medic on duty: nothing spawns, and the roll stays due so the first medic to clock in gets one at the next 10 s check.
- 🧪 The casualty goes through the medic downed state (`LF_Downed.Down`), so revive, carry and the bed take them, their inventory is locked while downed, and the bleed-out runs: an untreated casualty dies. It is server-spawned with an `RplComponent`; `LF_IsDowned` is an RplProp and the unconscious state is vanilla's, so clients and late joiners see the body lying down.
- 🧪 Cap: `m_iPerMedic` open casualties per on-duty medic, `m_iMaxOpen` in all (1 and 2). When the call leaves the board (`m_iBoardExpirySeconds`) the body is deleted and an `LF_OnServiceCallClosed` closes its map mark; a body in a seat or being carried waits 60 s more. The world reset clears them (`LF_WorldReset_Casualties`). Nothing is saved: a restart clears casualties like it clears the board.
- 🧪 The core conf has no rows and the generator off; the pack fills it (#4829). `LF_CasualtyCallConfig.ApplyLayers` merges each pack overlay over the core file: a field the overlay writes directly replaces the core value, and `m_aRows` replaces the row list whole. The Everon pack's `Configs/Medic/LF_CasualtyCalls_Everon1989.conf` turns it on, rolls every 1200 s (about three calls an hour with one medic on duty, cap one open per medic, two in all) and lists five rows by place kind, each with a US English summary key in `LF_Everon1989/Language/`: `mine` (Eastrise and Highstone mines) "Miner down at the mine, crushed hand" w3, `LogPile` (the woodlot) w2, `FarmCompound` w2, `Dock` w2, `RoadFuel` (the roadside fuel stop) w1. Coalman's Brow, the sawmill yard and the Meaux road register no kind of their own yet (only the shared `Workplace`), so they are not rows. Test `everon/casualty-rows` (`LF_Test_EveronCasualtyRows`, pack leg).
- 🧪 **Treat the patient (#4828, `LF_CasualtyTreat.c`).** On a casualty the Revive hold is labelled "Treat the patient" and takes twice the hold (about 8 s). The server re-checks duty, range, the medic's state and the revive item (one field dressing, used up), wakes the patient, takes the casualty off the open list and closes its map mark (`LF_OnServiceCallClosed`); the resident leaves 20 s later unless they are in a seat or downed again. Pay goes through `LF_OnPlayerRevived` with the casualty's caller uid, so `LF_MedicDutyBonus` books the $10 revive bonus (rank cap per hour, paid at clock-out) and `LF_JobLevelSources` the MEDIC XP. Farming guards: a casualty pays once (a second treat finds no open casualty), a casualty the treating medic downed pays nothing, and the hourly cap holds. The medic board line leaves with the call (`LF_ServiceBoard.Close` on `LF_OnServiceCallClosed`, #4852). The client labels from a replicated flag on the body (`SCR_ChimeraCharacter.LF_IsCasualty`, an RplProp the server sets when the generator spawns the casualty and clears when the call closes, #4848), so only a real casualty reads "Treat the patient" and any other downed resident keeps the plain revive. A bed delivery closes the call (`CloseDelivered`, #4863). ⚠️ Untested by ear or eye: the label on a second client is read from the flag, not yet from a two-client session.
- 🧪 **Casualty expiry (#4852).** A casualty lasts `m_iExpirySeconds` (core default and Everon overlay 600 s, 10 minutes; floor 30 s, never past the shared board expiry `m_iBoardExpirySeconds`, 900 s) from its roll: the call leaves the board and the body is cleared then, and the body's bleed-out is set to the same time (`LF_StartBleeding`) instead of the player default 180 s. The first played hour (#4830) lost its call after 106 s with the clinic about 300 m from the dock, too short to read the board, draw an ambulance and drive. The roll line logs `expires in N s`.
- 🧪 **Dev roll and stand (#4853, harness build only).** `casualty roll [kind]` rolls one casualty now (`LF_CasualtyCalls.RollNow`: skips the interval and the open cap, needs the generator on and a medic on duty, `kind` filters the rows by place kind) and replies with its caller uid, position and seconds left; `stand Casualty` (the newest) or `stand casualty:<n>` walks the character beside that body (`LF_CasualtyHarness.Named`, called by `LF_AgentHarness.ResolveNamed`). Proof `session.mjs casualty-call` plays the hour with both, then carries a second casualty to a clinic bed.
- 🧪 **Dressed, on clear ground (#4863).** A casualty row names a work outfit (`m_sOutfit`, an `LF_NpcOutfitPreset` conf under `Configs/NpcOutfits/`) and `LF_NpcOutfitDresser.Dress` puts its garments on the resident before they go down (a downed body's inventory is locked), so they no longer lie in underwear. The Everon rows: the mine a quarry worker, the woodlot a denim local, the farm a farmer, the dock a docker, the roadside stop a mechanic. The body no longer lies at a random point: `LF_CasualtySpot.Find` tries up to 14 points around the place (then 14 more at twice the radius) and takes the first that is open ground: not on a prop or floor (the first played hour put one on a hay bale in the farm barn), not under a roof, not in water, level within 0.3 m over a body's length and with no wall or prop within 1.1 m. A row with `m_bBuilt` (the dock's quay) may lie on a built surface and under a roof but must still be level and uncrowded. A roll that finds nothing tries another row (3 picks) and logs `no clear ground`.
- 🧪 **Bed delivery closes the call (#4849).** A casualty a medic puts in a clinic bed (`LF_BedDelivery.Complete`) leaves the open list, its board line and map mark close (`LF_OnServiceCallClosed`) and the woken resident walks off like a treated one (`LF_CasualtyTreat.CloseDelivered`). The $15 delivery bonus is the bed's own and nothing more is paid. A patient who is not a casualty changes nothing.
- 🧪 **A downed player pages the medic (#5152, `Feature/Medic/LF_DownedCall.c`).** `LF_OnPlayerDowned` for a player (not an NPC body: a casualty places its own call) places a medic service call through `LF_ServiceCalls.Place` with the grid place, the summary "Person down, bleeding out" (`#LF-DownedCall_Summary`) and the body's position, so the on-duty medics get the medic board row, the pager card and toast and the dispatch map mark, as for a casualty. The downed player is the caller (a medic who takes it tells them help is on the way) and is never a receiver: a lone medic who goes down pages nobody. A revive or a bed delivery (`LF_OnPlayerRevived`) or the death (`LF_OnPlayerKilled`: bleed-out, give-up) closes it (`LF_OnServiceCallClosed`: board row and map mark go). No medic on duty: no call. Test `medic/downed-call` (`LF_Test_DownedCall`); proof `session.mjs downed-call` (`up --mission island --clients 2`: the medic's client pager, board row and map mark, closed by the revive).
- Test: `medic/casualty-call` (`LF_Test_CasualtyCall`), `medic/casualty-treat` (`LF_Test_CasualtyTreat`); `medic/casualty-spot` (`LF_Test_CasualtySpot`), `medic/casualty-dressed` (`LF_Test_CasualtyDressed`), `medic/casualty-bed` (`LF_Test_CasualtyBed`); node selftest `casualty-call-selftest`.
