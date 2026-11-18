# Property

### `LF_Properties` — `Feature/Property/` (2026-11-17, #2646, spec #756)
- 🟡 The server keeps one `LF_PropertyRecord` per house or plot, keyed by its stable id (the house
  identifier or the plot key, never coordinates): owner, organisation, layout, build stage, work done,
  deposited materials (`prefab=count;...`), purchase time, rent paid until (0 = owned outright) and
  whether the owner spawns there. `Create`, `SetStage`, `SetOwner`, `GetOwnerKey`, `ExportAll`, `ApplyAll`.
- 🟡 A house's owner of record stays in the `houses` payload (`LF_Houses`); `GetOwnerKey` reads it from
  there and `SetOwner` sets it on a house in the world. A plot's owner is the property row's.
- 🟡 Saved as the `properties` sidecar payload (save version 51, `docs/persistence.md` item 38). A row
  whose property is not in the world waits and is saved again. A version 1-50 save loads an empty book.
  Proof: `property/record`, `property/persist`, `wt e2e`. No gameplay uses the book yet (later M8 slices).

### `LF_BuildStageRules` — `Feature/Property/` (2026-11-18, #2648, spec #756)
- 🟡 Pure rules for a build stage (`LF_PropertyStageDef` plus the record's `prefab=count;...` string and
  work done): `Missing` and `MissingText` per item, `ItemsMet`, `WorkRemaining`, `WorkAfter` (clamped, work
  is the site's so every worker's units add), `RoundsLeft` (reuses `LF_StationProgress.Rounds`), and
  `IsDone`, true only when items and work are both met.
- 🟡 `Deposit` accepts only the owner and invited key holders (`MayContribute`, grief rule #807). A deposit
  past the need is refused whole and names the item and the overflow; a refused try changes nothing.
- 🟡 Deposited items are counts, never entities; `Parse`, `Format` and `CountOf` read and write the string.
  Nothing calls the rules yet (later M8 slices). Proof: `property/build-stage-rules`.

### Plot purchase — `Feature/Property/LF_PlotComponent.c` (2026-11-18, #2649, spec #756)
- 🟡 A for-sale plot sign (`Prefabs/Props/Property/LF_PlotSign.et`, an `LF_PlotComponent` naming a
  `LF_Property.conf` plot key) shows "Buy plot ($price)". The request carries no plot id: the server takes
  the nearest plot sign within 5 m of the caller's character.
- 🟡 Refused, nothing taken: no sign in reach, a plot with no price, an owned plot, short carried cash, a
  cuffed buyer. Bought: exactly the price leaves the buyer's carried cash (ledger reason `property_buy`),
  the buyer's character key becomes the owner in `LF_Properties` with the purchase time, and a toast says
  so. The row rides the existing `properties` payload. Proof: `property/plot-buy`.
