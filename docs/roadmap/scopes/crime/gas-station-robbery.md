# Gas station robbery

Phase: Release
Extends: `Crime`, `Money`

## Player loop

A civilian holds up the till at a fuel station and leaves with cash. No breach. This is the small, fast robbery.

## In scope

- A till action on a fuel-station building, reusing `EL_RobAction` gates: civilian only, cooldown, minimum police on duty, wanted increase, one police alert.
- Payout is MoneyStack cash in a config range. The server rolls it.
- The site can sit on an Everon fuel station or a building override dropped in its place.

## Out of scope

- A new robbery action distinct from the cash rob path.
- Blowing the pumps, custom station meshes, or a fuel-price simulation.
- Vehicle fuel theft. Fuel delivery is a [legal job](../work/civilian-jobs.md).

## Assets

Building: `Prefabs/Structures/Commercial/FuelStations/FuelStation_E_01/`. That building is the capture prefab and is not placed in the debug row. The debug till is `{4E4E1620C6B02E1F}Prefabs/Props/Commercial/EL_FuelTill.et`, which inherits `Prefabs/Props/Commercial/CashierShop_01.et`. Payout: MoneyStack via `EL_RobAction`. The fuel can `Prefabs/Items/Fuel/Jerrycan_01/Jerrycan_01_item.et` is the delivery job, not this till. LNGC has no till robbery. See [sources](../../sources.md).

## First slice

One DebugWorld till. Cash increases, bank balance does not, wanted increases by the configured amount, a second attempt inside the cooldown is refused and logged.

## Depends on

- `Crime` robbery gates
- `Money`
- `Police` on-duty count

## Proof

- Logic: same pin style as `crime/rob-reward` and `crime/rob-wanted-clamp`.
- World: the till on a station prefab pays once per cooldown.
