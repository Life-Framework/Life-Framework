# Crime — Design Requirements

> Context file for the criminal / wanted system. Written against
> `docs/design-philosophy.md` (in-world first).

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

1. Fix the wanted-bump reliability: `GetPlayerAccount` is cache-only, so after
   the first robbery the wanted increase is skipped (account cache churn in
   `Account`). The most repeatable crime must still escalate.
2. Stop the police alert firing from inside the counting predicate (it spams
   on every offer refresh) — alert once, at the decision.
3. Fix `#EL-Stole_Money` (no `%1`, amount dropped) and the hard-coded
   "ALERTA POLICIAL" in `EL_RobWeaponAction.AlertPolice`.

## Iteration path

- **V2** — robbery minigame tension (victim response time), per-type wanted
  severity already defined (+1/+2/+3).
- **V3** — organized crime (gang territory, fences), bank robberies.

## Current state

- `EL_RobAction` / `EL_RobWeaponAction` / `EL_RobVehicleAction`
  (`Feature/Crime/`) — cash/weapon/vehicle robbery. ⚠️ near-duplicated guard
  logic across all three; alert spam; localization broken.
- Wanted state lives on `Account`.

## Dependencies

- `Account` (wanted), `Police` (response/arrest), `Money` (stolen cash),
  `Weapon`s (rob-with and rob-of).