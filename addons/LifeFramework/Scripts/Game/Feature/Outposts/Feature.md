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
- Trader sale and shop till call `RefusesService` (#4224).
- Standing (#4224, N27): a completed trade inside an outpost books the `fence_sale` award on the one
  underworld track and sets a per-outpost "known here" flag (`LF_OutpostStanding`, save section
  `outpoststanding`). Nothing reads the flag yet.
- Not yet: the placed outpost prefab and the north site (later cards).

## Dependencies

`Moderation` (`LF_Zones`), `Places` (`LF_Places`), `Police` (`LF_PoliceUtils.IsOnDutyCharacter`), `Trader`, `Shop`.
