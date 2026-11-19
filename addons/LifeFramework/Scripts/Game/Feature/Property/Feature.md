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
- Owned state on clients (#2780): `LF_PlotComponent.m_bOwned` is an `RplProp` (the server `BumpMe`s it from `RefreshOwned`, called by
  `LF_Properties.SetOwner` / `ApplyAll` and when a sign appears); the Buy action reads `IsForSale()` from it, never the book.
  The owner key is never sent. The face is the core sign `plot_for_sale` (`Prefabs/Props/Property/LF_PlotSign.et`).
- Finding plots (#2984): a sign for sale is an `LF_Places` place of kind `PlotForSale` (id = plot key) until the plot is
  owned; the core POI row draws it on the map and a town board card with place id `*` points at the nearest.
- House purchase (#3207): `LF_HouseSaleComponent` on `Prefabs/Props/Property/LF_HouseSaleSign.et` names a house id; the price
  and tier label are `LF_PropertyHouseDef` rows in `m_aHouses` (a pack prices houses by overlay, like plots). The request
  (`RpcAsk_LF_BuyHouse`, `LF_HouseSaleRequest.c`) carries the used sign's house id; the server sells it only for a free,
  conscious buyer with a sign of that id within 5 m, takes exactly the price (ledger `property_buy_house`), sets the owner
  through `LF_Properties.SetOwner` and hands over a `HouseKeyItem` bound to the id. The sign replicates `m_bOwned` like
  the plot sign and shows "Sold". A house id in `m_aHouses` is never a plot key (`RefuseHouseCollisions`).
- Spawn house (#2651): "Make this my spawn" on the house door, owner only; `m_bOwnerSpawnsHere` on the row, one
  per owner, read by `LF_Properties.SpawnHouseOf` at login only.

## Rules

- Never coordinates, never an EntityID in a row. A row whose property is absent waits and is saved again.
- New record fields go at the end and raise VERSION (docs/persistence.md "Record shapes").
- A broken or missing config logs `LF_Debug.Error("Property", ...)` and the getters return 0, empty or null.
- The core holds generic example plots only; a pack overrides the file with its own plots and prices (#808).
- No rent in wave 1: `m_iRentIntervalHours` and `m_iGraceHours` of 0 mean owned outright.
