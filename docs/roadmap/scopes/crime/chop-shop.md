# Chop shops

Milestone: M2
Extends: [vehicles](../ownership/vehicles.md) (plates, keys, impound), [vehicle robbery](../../../features.md), investigation plates and evidence, `Money`

## Player loop

A player steals a car, drives it to a hidden yard, and sells it for scrap and parts instead of keeping it. The yard pays less than the car is worth and wants no questions. The police can trace stolen plates, so the longer a stolen car is on the road the riskier it is.

## In scope

- A placed chop shop site with a drop area. The owner is an NPC or an organization, set in config.
- A **hotwire** step to drive a lockpicked or otherwise unkeyed vehicle: a held action at the ignition with a short failure risk and a noise cost. Without it a picked car cannot be driven away.
- **Dismantle:** the player parks a vehicle in the yard and the shop takes it apart. The vehicle is removed and the player gets cash by a fraction of the vehicle's value, plus optionally spare parts (items) for sale to a trader.
- **Stolen state:** a vehicle taken without its key is marked stolen on the server. The owner can report it (a payphone or an interaction) and the plate goes on the police wanted board. A police stop or an ID check on a stolen plate is an arrest case.
- A cooldown and a per-vehicle value cap so a chop shop is not an endless income.
- The server decides the payout, the stolen flag and the removal.

## Out of scope

- Car boosting contracts and delivery lists with random targets (later).
- VIN scratching and re-plating (later).
- Custom wreck meshes. The vehicle is deleted with an effect.

## Assets

Vehicles are base-game prefabs from the vehicle catalogue. The yard uses base-game props. Payout fractions and cooldowns are config.

## First slice

One DebugWorld yard. A player picks a parked car, hotwires it, drives it into the yard, dismantles it, gets paid, and the owner's payphone report puts the plate on the board.

## Depends on

- [Lockpicking](lockpicking.md)
- Vehicle ownership, plates (`LF_LicensePlateManager`) and the wanted board (#96)
- Investigation plates and the payphone (M2, epic #148)

## Proof

- Logic: the payout is a fraction of the value and capped; a stolen flag is set by an unkeyed start; an owned, keyed vehicle cannot be dismantled by a thief.
- World: the full loop on DebugWorld.
- Persistence: the stolen flag and the cooldown survive a restart.
- Manual: the yard animation and a police stop on a stolen plate.
