# Crime — Design Requirements

> Context file for the criminal / wanted system. Written against
> `docs/design.md` (in-world first).

## Intent

Crime happens **at the scene**. You rob someone by pointing a weapon at them
in the world; the act, the getaway, and the police response are physical. The
wanted system is the state that makes crime consequential.

## Interaction pattern

Rung 1. Robbery actions (`LF_RobAction`, `LF_RobWeaponAction`,
`LF_RobVehicleAction`) are interact actions with cooldown + civilian-only +
min-police-on-duty gates; they raise wanted and notify police.

Target: extend the physical loop — the victim can resist, bystanders report,
police arrive at the scene. The wanted bump must be reliable per crime (it
isn't today — see current state).

## V1 (shippable)

1. Wanted-bump reliability — DONE (verified 2026-08-30): the account cache
   churn was fixed (`SaveAndReleaseAccount` keeps the account resident), so
   every robbery escalates wanted. Pin tests `roadmap/fuel` (clamp at 5) and
   `crime/rob-weapon-vehicle` (mixed robberies clamp at 5).
2. Robbery payout pays **cash**, never the bank (verified 2026-08-30) — the
   `LF_ATMManager.Deposit` in `LF_RobAction` is gone; the haul is
   `LF_MoneyUtils.AddCash`. Pin test `crime/rob-reward` asserts the bank
   balance stays 0.
3. Stop the police alert firing from inside the counting predicate (it spams
   on every offer refresh) — alert once, at the decision — pending.
4. Fix `#EL-Stole_Money` (no `%1`, amount dropped) and the hard-coded
   "ALERTA POLICIAL" in `LF_RobWeaponAction.AlertPolice` — pending.

## Iteration path

- **V2** — robbery minigame tension (victim response time), per-type wanted
  severity already defined (+1/+2/+3).
- **V3** — organized crime (gang territory, fences), bank robberies.

## Current state

- `LF_RobAction` / `LF_RobWeaponAction` / `LF_RobVehicleAction`
  (`Feature/Crime/`) — cash/weapon/vehicle robbery. ✅ cash payout + wanted
  bump work; ⚠️ near-duplicated guard logic across all three; alert spam;
  localization broken.
- Wanted state lives on `Account`.
- `LF_Murder` (M1-27): a player who kills a player or an NPC is wanted for
  murder (config amount, default 3) with a description report and a police
  alert. `LF_Zones` (Moderation, #242): the town square is a SAFE zone, no player-on-player damage,
  refused in `SCR_CharacterDamageManagerComponent.HijackDamageHandling`; the
  `safe_zone` notice boards say so.

## Dependencies

- `Account` (wanted), `Police` (response/arrest), `Money` (stolen cash),
  `Weapon`s (rob-with and rob-of).
## Wiring facts (moved from AGENTS.md, verified 2026-11-07)

- Breach: `LF_BreachArmAction` (`LF_BreachAction.c`), `LF_Bank`, and
  `LF_Hospital` all call `LF_Breach.Arm(targetId, playerUid)`. Wanted and the
  police notify live inside `Arm`, so a refreshed action offer cannot fire them.
- Cash till: `Roadmap_Fuel_Till` in `Street_Fuel.layer` (on `Roadmap_Fuel_Counter` at the kiosk service window, about 283.6, 237.65, standing on the kiosk floor (0.345 m) with its back edge 2.5 cm inside the window wall; the kiosk interior is closed, #729) is
  `{9D3A51E060EFE982}Prefabs/Props/Commercial/LF_FuelTill_Attended.et`, a meshless child of
  `{4E4E1620C6B02E1F}Prefabs/Props/Commercial/EL_FuelTill.et` (the kiosk's own baked register 3 m back is the one players see) (parent
  `{51D31AB07AE3C1C7}Prefabs/Props/Commercial/CashierShop_01.et`) and carries
  `LF_RobAction` as an additional action.
- The haul is MoneyStack cash
  (`{5439738849229352}Prefabs/Items/Currencies/MoneyStack.et`). The ATM is the
  only cash-to-account boundary, so a robbery leaves `LF_BankAccount` at 0
  (pinned by `crime/rob-reward`).
- Doors: `{99D61E9EDBDF3CC8}Prefabs/Structures/BuildingParts/Doors/EL_Door_Base.et`
  inherits `Door_HouseWooden_E_A_LEFT_EXT_COV_B.et` and overrides the inherited
  `ActionsManagerComponent "{F32B667C3C823800}"`. Add door actions to that
  override. A second ActionsManagerComponent drops the lock actions.

## Vault fence tools (#273)

- LF_BankVaultComponent.m_sRequiredTools (drill,charge, any one) gates LF_Bank.Rob for a player robber. None held: 
ob refused logged, offer reads #LF-Bank_NeedTool.
- While the drill works (#340, `LF_DrillScene.c`): the drill LeaveTool laid down is held in front of the robber at the push animation's hands and switched on (`LF_AssetAnimComponent`: spinning chuck, sparks, motor and grinding loops), then switched off and laid back when `LF_WorkAnim` ends for any reason. A jammed drill is only laid down.
- Drill: kept, jams at m_fJamChance, and is left at the vault as scene evidence either way (its serial names the buyer at the locker). Charge: consumed by LF_Breach.Arm, which also accepts the fence LF_Charge. Drill time is the vault fuse.
- Every attempt publishes LF_OnVaultBreach (opened, jammed, refused). Test crime/vault-tool; proof session.mjs vault-proof.


## Cash van (#274)

- The van box is a `LF_HeistVaultComponent` (a `LF_BankVaultComponent` whose haul is spilled money bags, `LF_BankLoot.SpillBags`). Same gates, tool, jam and evidence path as the bank vault; the offence reads `#LF-Offence_CashVan` (`LF_Breach.SetAlarm`).
- Route: `LF_CashVanRouteComponent` moves the van and its box between stop markers on a timer when no player is within `m_fClearMetres` and calls `LF_Bank.Restock`. Parked on a schedule, not AI-driven (base-game AI driving is unreliable for this).
- Opening publishes `LF_OnHeistLoot`. Test `crime/cash-van`; proof `session.mjs cash-van-proof`.

## Police armory heist (#275)

- The armory store is a `LF_HeistVaultComponent` with weapons loot (`LF_BankLoot.IssueWeapons`), a keycard-or-charge tool list, a loud alarm, a stock and a refill. Weapons are `LF_Armory*` prefabs carrying `LF_SerialComponent`; each serial is booked to the thief in the fence book.
- Test `crime/armory`; proof `session.mjs armory-proof`.

## Lockpicking (#411)

- `LF_Lockpick` (server) runs a timed attempt on a locked `LF_LockComponent` door that carries `LF_LockpickTargetComponent`; `LF_LockpickRules` holds the pure rules (time, odds, wear, noise, gate). The pick is `LF_Lockpick.et`, a stack whose count is its uses; skill is the character level, so nothing new is saved.
- A success always files `LF_WantedReports.Report` (offence `#LF-Offence_Lockpick`); a failure only when its noise roll is heard. Wanted is not raised and dispatch is not called: a witness has to report it.
- Test `lockpick/door`, `e2e/lockpick-door`. Log `[LFDebug:Lockpick]`.

## Vehicle pick and hotwire (#412)

- `LF_Lockpick` works a locked `LF_VehicleLockComponent` the same way as a door. `LF_Hotwire` (server) runs the timed ignition attempt; `LF_HotwireRules` holds the pure rules. The start gate is `SCR_CarControllerComponent.OnBeforeEngineStart`, active only where `m_bIgnitionLocked` is on.
- Hotwire success: `SetHotwiredBy(playerId)` plus `SetStolen(true)` on the lock, raises the thief wanted by 1 (`LF_Hotwire.ApplyTheft`, #495), and an `#LF-Offence_VehicleTheft` report if heard. Nothing is saved.
- Test `lockpick/vehicle`, `e2e/lockpick-vehicle`; proof `session.mjs hotwire-proof`. Log `[LFDebug:Hotwire]`.

## Chop shop (#415)

- `LF_ChopShopComponent` (a bench) with `LF_ChopAction` in three modes (load, work, collect), `LF_StationProgress` for the work. `LF_ChopShopRules` is the pure rule set (payout, check, cooldown); `LF_ChopShopConfig` reads `Configs/Crime/LF_ChopShop.conf`.
- Collect: cash through `LF_MoneyUtils.AddCash` (`chopshop`), `LF_CarParts` items, `LF_StolenVehicles.Clear`, `LF_VehicleOwnership.Scrap`, the effect (`LF_BroadcastChopEffect`, a local particle on every rendering machine), then `LF_WantedReports.Report(... OFFENCE_CHOP)` for the evidence and `LF_OnVehicleChopped`.
- Session state; test `chopshop/payout`, `e2e/chopshop-yard`; proof `session.mjs chop-proof`. Log `[LFDebug:ChopShop]`.

## Stolen vehicles (#414)

- `LF_StolenVehicles` (server, session state, nothing saved: the car entity does not survive a restart, an owned car left out is impounded at load). `Report(vehicle, owner)` files an `LF_StolenReport` when the reporter owns the vehicle record and the lock carries the stolen flag; `CheckPlate(plate, out report)` is the plate-check seam (STOLEN only from a filed report; CLEAR for a registered plate; UNKNOWN otherwise) that stops and searches (#419) call; `Recover(vehicle, officer)` stores the car in the owner's garage (`LF_VehicleOwnership.Store`); `Clear(vehicleId, how)` closes a report when the car leaves another way (the chop shop).
- Actions on the S105 prefab: `LF_ReportStolenAction` (key holder), `LF_CheckPlateAction` and `LF_RecoverVehicleAction` (on-duty officer). Board: `V;plate;nameKey;place` entries ahead of the dispatch log (`LF_WantedBoard`).
- Bus: `LF_OnVehicleReportedStolen`, `LF_OnVehicleRecovered`. Test `stolen/report`. Log `[LFDebug:StolenVehicle]`.
- Working animation (#513, `LF_WorkAnim.c`): the lockpick attempt and the vault drill show a working pose on every client through a custom loiter: `Anims/Workspaces/Work/LF_WorkPick.agr` (base-game repair clips) for the pick, `LF_DrillPush.agr` (our own push clips, generated by `tools\cli asset anim drill`, #340) for the drill. The server owns the session and its stops; fast exit only. Tests `work/anim`, `work/anim-targets` (the car pick and the cash van box drill start it too). Log `[LFDebug:WorkAnim]`.

## Crime catalog (#685a)

What a crime adds to wanted, its ticket fine, a jail-time floor and its repeat rule live in `Configs/Crime/LF_CrimeCatalog.conf` (`LF_CrimeCatalogConfig`, `LF_CrimeCatalog.c`), keyed by the offence id (`#LF-Offence_*`, the same string the report, the board and the saved `{uid, offence, count}` crime record carry, so no save change). Call sites call `LF_CrimeCatalog.ApplyWanted(account, uid, crimeId)` before the report that counts the crime and never keep a number of their own. An id the catalog lacks adds 1 wanted and warns once. The owner section is `crime.crimes.<id>.{wantedAdd, fine, sentenceSeconds, repeatThreshold, repeatWantedAdd}`. Licence seizure and bail are #685b (M5). The carjack and the hotwire theft are two ids (`#LF-Offence_Carjacking`, `#LF-Offence_VehicleTheft`) because they add different wanted.
