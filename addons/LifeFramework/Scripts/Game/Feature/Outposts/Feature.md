# Outposts - Design Requirements

> Context file for the rebel outposts (0.3.0 gangs O1, decision N28). Written against `docs/design.md` (in-world first).

## Intent

A rebel outpost is a safe place far from town. Nobody fights inside; police may walk in, but the
outpost's traders do not serve an on-duty officer. A player who wants the stock walks in off duty.

## Rules

- SAFE: `LF_OutpostComponent` registers a SAFE circle (`LF_Zones`, name `outpost:<id>`) and the place
  under kind `outpost` (`LF_Places`). Radius from `Configs/Outposts/LF_Outposts.conf` unless the component sets one.
- Police enter freely (the SAFE rule never stops them).
- A trader asks `LF_Outpost.RefusesService(customer, traderPos)` before it serves. An on-duty officer is
  refused with a toast in words (`LF-Outpost_TraderRefusesOfficer`); off duty is served.
- `m_bTradersRefuseOnDutyPolice` in the conf turns the refusal off.

## Current state

- Rule, zone registration, config, refusal text: done (#4217).
- The quartermaster crate (`LF_OutpostStock.c`, #4223): stock rows, one Buy action per row index, the tier gate in words, the officer refusal on every Buy, cash then item with a refund. Default north stock: gang radio $60, pistol $500 (tier 1), rifle $2,000 (tier 2).
- Outpost guns carry no outlet serial: the vanilla PM and AK-74 have no `LF_SerialComponent`, so a seized outpost gun does not trace back to a counter (N29: the rebels sell what the legal trail does not). The pack stock tables (O6) may pick serial-stamped prefabs and call `LF_FenceSerials.OnWeaponSale` then.
- Not yet wired: the trader sale and shop buy paths (the outpost crate does) call `RefusesService` (a hook in
  `Trader/LF_TraderSale.c` and the shop buy path, outside this card); the placed outpost prefab and the
  north site come from later cards.

## Dependencies

`Moderation` (`LF_Zones`), `Places` (`LF_Places`), `Police` (`LF_PoliceUtils.IsOnDutyCharacter`), `Trader`, `Shop`.
