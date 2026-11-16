# Character & Survival

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
- ⚠️ No drink prefab carries the effect yet: the beer wiring is a later #1011 slice.

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
- Harness `impair <0-100|off>` pins the local view level for a session proof.

### `LF_SurvivalStatsSaveData` — `Feature/Survival/Persistence/`
- ✅ Save/apply round-trip re-clamps through setters (corrupt save sanitized).
- ⚠️ `Equals` uses exact float `==` (no epsilon).

### Downed state — `Feature/Medic/` (M2 #255, 2026-11-09)
- ✅ **Siren and lights on service vehicles (#263).** Two placed service vehicles carry the existing driver-seat siren (`LF_SirenManagerComponent`, `Feature/SirenLights/`): the police car `LF_UAZ469_Police.et` (a covered UAZ469 with the blue bar seated on its roof, #595; it was an open M151A2 jeep whose bar floated) at the station (`Roadmap_Police_Car`) and the new `Prefabs/Vehicles/Wheeled/M997/LF_M997_Ambulance.et` (vanilla `M997_maxi_ambulance` plus the red `LightBarLED_Red.et` bar and the mod siren sounds) at the hospital (`Roadmap_Hospital_Ambulance`). The driver cycles modes with the `LF_CycleSirenModes*` keys (mouse button 2 or `gamepad0:thumb_left`, single and double click); the server decides in `ApplyRequest` through the pure `LF_SirenRules.Judge`: a mode out of range, the mode already running, or a change within `MIN_TOGGLE_MS` (300 ms) of the last is refused and broadcasts nothing, so a toggle replicates once per change. The mode is an `RplProp` (`OnModeReplicated`, `BumpMe` on each accepted change) plus `RplSave`/`RplLoad`, so a client that streams the car in later or joins late applies it, not only clients that had the car streamed in at the toggle (#471); `session.mjs siren-late-proof` (`up --clients 1`) proves a late joiner. Test `vehicles/siren`; `session.mjs siren-proof` (harness verb `siren <Entity> <mode>`, dev gate) drives it and checks the second client. Logs `[LFDebug:Siren]`. On duty only (#464): `RequestBy` refuses (and logs `[LFDebug:Siren]`) anyone who is not an on-duty member of the vehicle's `m_eService` (police jeep: police, ambulance: medic), so a thief in a stolen police car drives it with no siren; test `vehicles/siren-duty`. The base game has no vehicle siren, only `SCR_SoundEvent.SOUND_SIREN` for structures.
- ✅ **Treatment bill on paper, paid at the hospital desk (#260).** A revive (`LF_Revive.Apply`) and a hospital wake after a death each write one `LF_TreatmentBillRecord` in `LF_TreatmentBills` (mirrors the police ticket: number, patient, issuing medic, amount, in-game time) and hand the patient a printed `HospitalBill.et`. Amounts are config: `m_iReviveBillAmount` (100) and `m_fBillMedicShare` (0.5) in `LF_Medic.conf`, `m_iBillAmount` (150, wake) in `LF_Respawn.conf`; the bank is never charged. The patient pays the oldest unpaid bill in carried cash at the hospital desk (`LF_TreatmentBillPayAction`, ledger reason `treatment-bill`, medic as counterparty); the paper is stamped paid. The medic's share of a paid revive bill is paid once in cash at clock-out (`LF_DutyLocker.ClockOut`, ledger reason `bill-share`). An unpaid bill has no consequence in the flagship; it is handed again with every new body. Records persist as the `bills` payload (save version 19); a carried bill comes back blank after a restart and is printed again on spawn. Tests `medic/bill`, `medic/bill-persist`, `respawn/bill-once`. Logs `[LFDebug:Bill]`. **Debt (#464):** a bill unpaid `m_iBillDeadlineHours` (48 in-game h, `LF_Medic.conf`) after it was written becomes debt; `LF_TreatmentBills.Sweep` takes it from the bank as soon as the bank holds money (on every bank credit, on spawn, and a 60 s check), partially if needed, one ledger entry per take with reason `hospital-debt`; the ATM terminal shows an UNPAID DEBT line. The debt lives in the same `bills` record (`m_sPaidAt` = `debt:<left>` while unpaid), so the save layout and version are unchanged; test `medic/bill-debt`.
- ✅ **A medic revives a downed patient (#259).** `LF_ReviveAction` ("Revive", on a player character beside Introduce / Show ID) is shown to an on-duty medic (`LF_MedicDuty`) facing a downed player within `m_fReviveRange` (2.5 m, `LF_Medic.conf`). `LF_Revive.Perform` re-checks on the server (medic on duty, patient downed, in range; `LF_Revive.Decide` is the pure rule), then `LF_Downed.Clear`: the flag, the bleed-out clock and the unconsciousness end, the patient keeps their inventory and the sliver of health, and `LF_OnPlayerRevived` is published on the bus. A civilian, an out-of-range medic and a standing patient are refused with a toast and a log line. Tests `medic/revive`; `session.mjs downed-proof` revives the downed client. A revive uses up one field dressing (`m_sReviveItemPrefab`, `m_iReviveItemCount` in `LF_Medic.conf`; empty = free; refused with `#LF-Revive_NoItem`), stops the bleeding and lifts health to `m_fReviveHealthScaled` (0.25): the downed sliver of health used to bleed away seconds after the wake and down the patient again (#427, found by `session.mjs revive-proof`, 10 of 10 stay awake). **CPR animation (#512):** the revive is a 4 s hold (`Duration 4` on the action). `OnActionStart` asks the server (`LF_AskReviveAnim`, patient as RplId, medic taken from the controller); `LF_ReviveAnim.Begin` re-runs `LF_Revive.Check` and tells the medic's owner (`LF_BroadcastReviveAnim`) to start a custom loiter on `Anims/Workspaces/Medic/LF_ReviveCPR.agr`, an attachment graph on the player graph's `NPC` binding that plays the base-game `p_cro_revive_CPR_in/loop/out` clips (`Gestures` = 560). The base game replicates the loiter to every client, late joiners included; each rendering client logs `revive animation playing` only when it sees the medic loitering with the head bone under 1.3 m (#512; `PlayPostGraphAnimation` never played because the player character has no `CharacterAnimGraphComponent`). The stop is always the fast exit: the CPR out clip has no `Gesture_End` event, so a soft exit would never finish. The server stops it on release (`OnActionCanceled`), completion, the medic leaving reach, the patient no longer downed, or a 7 s overrun (`LF_ReviveAnim.Step`, 400 ms watch). Test `medic/revive-anim`; harness verb `cpr <player> start|stop`, `session.mjs revive-anim-proof`. Logs `[LFDebug:Medic] revive animation ...`. First aid (#257) still buys time for anyone.
- ✅ **A medic revives a downed patient (#259).** `LF_ReviveAction` ("Revive", on a player character beside Introduce / Show ID) is shown to an on-duty medic (`LF_MedicDuty`) facing a downed player within `m_fReviveRange` (2.5 m, `LF_Medic.conf`). `LF_Revive.Perform` re-checks on the server (medic on duty, patient downed, in range; `LF_Revive.Decide` is the pure rule), then `LF_Downed.Clear`: the flag, the bleed-out clock and the unconsciousness end, the patient keeps their inventory and the sliver of health, and `LF_OnPlayerRevived` is published on the bus. A civilian, an out-of-range medic and a standing patient are refused with a toast and a log line. Tests `medic/revive`; `session.mjs downed-proof` revives the downed client. A revive uses up one field dressing (`m_sReviveItemPrefab`, `m_iReviveItemCount` in `LF_Medic.conf`; empty = free; refused with `#LF-Revive_NoItem`), stops the bleeding and lifts health to `m_fReviveHealthScaled` (0.25): the downed sliver of health used to bleed away seconds after the wake and down the patient again (#427, found by `session.mjs revive-proof`, 10 of 10 stay awake). ⚠️ No treatment animation (deferred: no animation asset; the revive is instant); first aid (#257) still buys time for anyone.
- ✅ **Medic job at the hospital duty desk (#258).** `LF_DutyAction` takes a role attribute (`m_eRole`, POLICE or MEDIC); `LF_DutyLocker.ClockIn/ClockOut(account, body, role)` swaps inventories through the role's own slot (`police_duty` / `medic_duty`) the same way for both. The medic role is open (no roster); the account is on duty as `LF_Faction.MEDIC` (the persisted faction int, no save format change). The kit is the MEDIC `LF_FactionLoadout` on the game mode (uniform, medical kit, dressings, saline, morphine, tourniquet, `LF_Pager`). Routing reads an on-duty medic as the medic job (`LF_ServiceCalls.JobFor`), so the hospital board, taking a call and the pager reach them. Duty pay: `m_fMedicDutyPayPerSecond` in `LF_Jobs.conf` (0.4), paid in cash at clock-out; medic seconds never count toward police rank. The desk `Prefabs/Medic/LF_MedicDutyDesk.et` is placed at the playtest hospital (`Roadmap_Hospital_Duty_Desk`). Tests `medic/duty-swap`, `medic/duty-desk`; `session.mjs medic-proof`. ⚠️ A medic clocked in at the police desk (or the reverse) is refused (`#LF-Duty_WrongDesk`).
- ✅ **First aid slows the bleed-out (#257).** `LF_FirstAid`: the vanilla bandage, tourniquet and saline bag (`SCR_ConsumableBandage/Tourniquet/SalineBag.ApplyEffect`, so the vanilla animation and healing are unchanged) add `m_fBandageSeconds` (30), `m_fTourniquetSeconds` (45), `m_fSalineSeconds` (60) to a downed player's clock on the server, never above `m_fFirstAidCapSeconds` (300); morphine adds nothing. `m_bFirstAidAnyone` (flagship on; off = only a player whose job is medic). The same item is counted once. Publishes `LF_OnFirstAid`. Tests `medic/first-aid`, `medic/first-aid-rules`. ⚠️ Not driven in a session: the vanilla bandage may refuse a target with no bleeding wound (manual check in the PR). **Container attribute (modded classes):** each `modded class` of a base-game config class repeats the base's `[BaseContainerProps()]`; without it the config entry fails with `Unknown class` and the vanilla bandage did nothing. `validate-modded-container-props` reads the base through the MCP tools and fails a copy that drops one (skipped in CI); test `medic/consumable-effect-loads`.
- ✅ **Load a downed player into a vehicle (#261; drag and carry cut 2026-11-15).** `LF_LoadPatientAction` ("Load into vehicle") on a player character, context LF_ID, offered to anyone (`m_bCarryAnyone`, flagship on; off = on-duty medics) standing within `m_fCarryRange` of a downed body when a vehicle with a free cargo seat is within `m_fLoadRange` (5 m). The server (`LF_Carry.Load`) re-checks and seats the body through `SCR_CompartmentAccessComponent.MoveInVehicle`, the vanilla casualty path (Game Master and `SCR_LoadCasualtySupportStationComponent` use it), re-asking up to four times while the owning client takes the seat; every load publishes `LF_OnPlayerCarried` ("load"). The bed (#262) takes the downed body lying beside the medic or the downed occupant of a stretcher vehicle. **Cut:** Drag, Carry and Put down, the server hold whose 250 ms tick moved a client-owned body with the Game Master teleport (`SCR_EditableCharacterComponent.SetTransform`): the maintainer saw the body teleport around with no animation. Vanilla has no carry or drag animation and no attach for a player character, and ACE's clips are GPL (`docs/research/animation-pipeline.md`), so there is no smooth path to put back; the actions, the hold, its RplProp pair on the character and the follow tick are removed. The police escort (`LF_JailEscort`) is bookkeeping only and never moved anyone. Tests `medic/carry`; `session.mjs carry-proof` (load only). ⚠️ A load needs a free passenger seat; the driver seat is never used.
- ✅ **Bleed-out timer and give-up (#256).** `LF_BleedOut`: a downed player has `m_fBleedOutSeconds` (180) left on a server-owned clock; one shared 1 s tick counts every downed character down and at zero calls `LF_Downed.Finish` (the real death), attributing it to whoever downed them while they are online. The client reads the time left through an RplProp whole-second copy moved only when it drifts 5 s (no per-frame `BumpMe`). Give-up: hold `LF_GiveUp` (X / pad Y) after `m_fGiveUpDelaySeconds` (30); the request is an RPC on the player controller and `LF_BleedOut.GiveUp` refuses unless the character is downed and the delay has passed. Disconnecting while downed is death (owner `disconnect.downedDies`, #691). Tests `medic/bleed-out`, `medic/bleed-out-rules`; `session.mjs downed-proof` covers give-up and a full bleed-out. ⚠️ The dimming screen and heartbeat are vanilla unconsciousness only; no custom effect yet.
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

