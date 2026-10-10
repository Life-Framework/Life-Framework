# Police ranks

Phase: Release
Extends: `License`, `Whitelist`, `Police` duty

## Player loop

A police officer starts at the bottom rank and is promoted for time on duty. Rank decides which vanilla kit they can draw at the station and which police actions they can use.

## In scope

- A config ladder of a few ranks (starting rank, then senior ranks). Titles stay period-plain: patrol officer up through a senior officer. The exact names live in localization and config.
- Promotion is server-side, from on-duty time or an explicit promotion action by a higher rank. A client cannot set its own rank.
- Rank gates station kit: each rank lists vanilla clothing and weapon prefabs. The armory gives those items and nothing else.
- Higher ranks can raise the fine cap and authorize arrest. Lower ranks can still go on duty and see the wanted list.
- Who may join the police faction stays on the whitelist. Rank is progression after that, not a way around the whitelist.

## Out of scope

- New weapons, tactical gear, or vehicles per rank.
- A skill-point purchase that skips the ladder.
- Detective minigames. Investigations are [police missions](../later/police-missions.md), after release.

## Assets

LNGC has no rank ladder. The kit is config pointing at gear that already exists:

- Sidearm `{1353C6EAD1DCFE43}Prefabs/Weapons/Handguns/M9/Handgun_M9.et` (already in `ShopPrices.conf`)
- Long gun `Prefabs/Weapons/Rifles/M16/Rifle_M16A2.et`
- Uniforms recolored from vanilla clothes in LNGC: `Hat_PeakedCap_police.et`, `Shirt_Turtleneck_01_police.et`, `Pants_Suit_01_police.et`
- Station `Prefabs/Structures/Houses/Villa/Villa_E_2I01/Villa_E_2I01_Police.et`
- Cars `{C03B70B228FC49BA}Prefabs/Vehicles/Wheeled/M151A2/M151A2_Police_Base.et` and LNGC `Prefabs/Vehicles/Wheeled/S105/S105_police.et`

There is no shotgun in the base game. A missing prefab logs and that kit line is skipped. See [sources](../../sources.md).

## First slice

Two ranks. The lower rank draws a sidearm at the station. The higher rank draws a long gun and can arrest. A lower rank's arrest is refused and logged.

## Depends on

- `Police` duty and arrest
- `Whitelist` for faction entry
- `License` only if a rank requires an existing police license; rank itself is not a second license currency

## Proof

- Logic: kit list is the intersection of rank and config; self-promotion RPC refuses.
- World: station armory gives the rank's items to the on-duty officer.
