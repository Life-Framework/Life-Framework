# Crime — Design Requirements

> Context file for the criminal / wanted system. Written against
> `docs/design.md` (in-world first).

## Intent

Crime happens **at the scene**. You rob someone by pointing a weapon at them
in the world; the act, the getaway, and the police response are physical. The
wanted system is the state that makes crime consequential.

## Interaction pattern

Rung 1. Robbery actions (`EL_RobAction`, `EL_RobWeaponAction`,
`EL_RobVehicleAction`) are interact actions with cooldown + civilian-only +
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
   `EL_ATMManager.Deposit` in `EL_RobAction` is gone; the haul is
   `EL_MoneyUtils.AddCash`. Pin test `crime/rob-reward` asserts the bank
   balance stays 0.
3. Stop the police alert firing from inside the counting predicate (it spams
   on every offer refresh) — alert once, at the decision — pending.
4. Fix `#EL-Stole_Money` (no `%1`, amount dropped) and the hard-coded
   "ALERTA POLICIAL" in `EL_RobWeaponAction.AlertPolice` — pending.

## Iteration path

- **V2** — robbery minigame tension (victim response time), per-type wanted
  severity already defined (+1/+2/+3).
- **V3** — organized crime (gang territory, fences), bank robberies.

## Current state

- `EL_RobAction` / `EL_RobWeaponAction` / `EL_RobVehicleAction`
  (`Feature/Crime/`) — cash/weapon/vehicle robbery. ✅ cash payout + wanted
  bump work; ⚠️ near-duplicated guard logic across all three; alert spam;
  localization broken.
- Wanted state lives on `Account`.

## Dependencies

- `Account` (wanted), `Police` (response/arrest), `Money` (stolen cash),
  `Weapon`s (rob-with and rob-of).
## Wiring facts (moved from AGENTS.md, verified 2026-11-07)

- Breach: `LF_BreachArmAction` (`LF_BreachAction.c`), `LF_Bank`, and
  `LF_Hospital` all call `LF_Breach.Arm(targetId, playerUid)`. Wanted and the
  police notify live inside `Arm`, so a refreshed action offer cannot fire them.
- Cash till: `Roadmap_Fuel_Till` in `Roadmap.layer` is
  `{4E4E1620C6B02E1F}Prefabs/Props/Commercial/EL_FuelTill.et` (parent
  `{51D31AB07AE3C1C7}Prefabs/Props/Commercial/CashierShop_01.et`) and carries
  `EL_RobAction` as an additional action.
- The haul is MoneyStack cash
  (`{5439738849229352}Prefabs/Items/Currencies/MoneyStack.et`). The ATM is the
  only cash-to-account boundary, so a robbery leaves `EL_BankAccount` at 0
  (pinned by `crime/rob-reward`).
- Doors: `{99D61E9EDBDF3CC8}Prefabs/Structures/BuildingParts/Doors/EL_Door_Base.et`
  inherits `Door_HouseWooden_E_A_LEFT_EXT_COV_B.et` and overrides the inherited
  `ActionsManagerComponent "{F32B667C3C823800}"`. Add door actions to that
  override. A second ActionsManagerComponent drops the lock actions.
