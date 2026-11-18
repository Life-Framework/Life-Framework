# Property — Design Requirements

> Context file for owned plots and houses (#747, spec #756). Player docs: `docs/features/property.md`.

## Intent

A property is a house or a plot a character (or an organisation) owns, rents or builds on, in stages.
The server keeps one record per property by a stable id so building progress, rent and the spawn
choice survive a restart.

## Shape

- Config (#2647, P2): `Configs/Property/LF_Property.conf` read through `LF_PropertyConfig`. Plots, tiers,
  four stages per tier, per-stage item requirements, work units and shell prefabs are all config.
- A plot key equal to a house identifier is refused (#2783): `RefuseHouseCollisions` at config load,
  `RefuseHouseId` when a house registers later; `validate-plot-house-ids` checks the confs and layers.
- Record (#2646): `LF_PropertyRecord`, one row, append-only, VERSION 1. Stable id = house identifier or
  plot key. `LF_Properties` is the server book (static map), `ExportAll` / `ApplyAll` for the
  `properties` payload.
- A house's owner of record is in `houses` (`LF_Houses`); this book mirrors it. Contents of a house ride
  `LF_PersistentStorageComponent` under `property:<id>`, not this record.
- Plot purchase (#2649): `LF_PlotComponent` on a plot sign; `RpcAsk_LF_BuyProperty` on the player
  controller sends the used sign's plot key; the server sells it only when a sign with that key is within 5 m,
  rate-limited (PURCHASE), for a living conscious buyer, paid with carried cash. Nothing
  builds yet.
- Spawn house (#2651): "Make this my spawn" on the house door, owner only; `m_bOwnerSpawnsHere` on the row, one
  per owner, read by `LF_Properties.SpawnHouseOf` at login only.

## Rules

- Never coordinates, never an EntityID in a row. A row whose property is absent waits and is saved again.
- New record fields go at the end and raise VERSION (docs/persistence.md "Record shapes").
- A broken or missing config logs `LF_Debug.Error("Property", ...)` and the getters return 0, empty or null.
- The core holds generic example plots only; a pack overrides the file with its own plots and prices (#808).
- No rent in wave 1: `m_iRentIntervalHours` and `m_iGraceHours` of 0 mean owned outright.
