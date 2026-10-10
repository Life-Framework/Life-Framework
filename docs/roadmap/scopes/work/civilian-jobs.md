# Civilian jobs

Phase: Release
Extends: `Jobs`, `Level`, `License`, `Money`

## Player loop

A civilian takes a job at a workplace, does the work in the world, and is paid in cash. Doing the work advances that job. Three jobs ship as the examples: taxi, fuel delivery, and parcel delivery.

## In scope

- Sign-up is at the workplace (a desk or board), not a menu from anywhere.
- **Taxi.** Take a base-game vehicle from the rank, pick up at a point, drop at a point, earn a config fare in cash.
- **Fuel delivery.** Carry a base-game fuel can from the depot to a vehicle or station marker. Delivery consumes the can and pays cash.
- **Parcel delivery.** Pick up a crate at the depot (hand-carry) and drop it at the marker.
- Job experience actually increases on a completed run. Level gates the next workplace the way licenses already gate police and medic.
- Paychecks for salaried jobs stay cash, on the existing clock.
- Each run pays once. Abandoning the run pays nothing and is logged.

## Out of scope

- A phone or map-app dispatcher. Callers are placed markers.
- New cars for the taxi. The rank uses [vehicles](../ownership/vehicles.md) from the base game.
- Joke job titles. Names read like 1989 work.

## Assets

Jobs already configured in `EL_PlayerJobComponent`: unemployed, farmer, miner, lumberjack, medic, police. Taxi, fuel delivery, and parcels are new loops on that component.

- Taxi and fuel runs use `Prefabs/Vehicles/Wheeled/S105/S105_beige.et` or `UAZ469_covered_CIV_blue.et`
- Fuel can: `Prefabs/Items/Fuel/Jerrycan_01/Jerrycan_01_item.et`
- Parcel crate: `HandCarryItem_Base.et`. Depot counter: `Prefabs/Props/Services/Post/CounterPostOffice_01.et`
- Heavier delivery, if a run needs a truck: `S1203_transport_beige.et` or `M923A1_transport.et`

LNGC lists job-specific income as not built. Do not port its unemployed stipend in place of these runs. See [sources](../../sources.md).

## First slice

Fix job XP so a completed run levels the job. Ship parcel delivery end to end (no vehicle required). Taxi and fuel delivery follow on that same pay and XP path.

## Depends on

- `EL_PlayerJobComponent` (XP is still a no-op today)
- `Money` cash payout
- `Level` and `License` for gates
- Hand-carry for parcels
- [Vehicles](../ownership/vehicles.md) for the taxi

## Proof

- Logic: a run pays once; XP increases; a player under the level is refused at the workplace.
- World: parcel depot to drop-off on DebugWorld.
- Manual: taxi with two clients is not required for the first slice; one driver and a marker fare is enough.
