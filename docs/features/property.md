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
  `LF_Property.conf` plot key) shows "Buy plot ($price)". The request carries the used sign's plot key: the server sells
  it only when a live sign with that key is within 5 m of the caller's character, so a nearer neighbouring sign
  never sells instead (#2777 review). Rate-limited as a purchase; a dead or downed buyer is refused.
- 🟡 Refused, nothing taken: no sign in reach, a plot with no price, an owned plot, short carried cash, a
  cuffed buyer. Bought: exactly the price leaves the buyer's carried cash (ledger reason `property_buy`),
  the buyer's character key becomes the owner in `LF_Properties` with the purchase time, and a toast says
  so. The row rides the existing `properties` payload. Proof: `property/plot-buy`.
- 🟡 (#2780) The plot sign reads "For sale" (core sign `plot_for_sale` in `LF_Signs.conf`, neutral strings; a pack words it
  by overriding the same keys). A client has no property book, so each sign replicates one bool, `m_bOwned`
  (`RplProp` + `BumpMe` on change; a late joiner gets it with the entity). `LF_Properties.SetOwner` and `ApplyAll` refresh the
  live signs, and a sign that enters the world after a sale starts owned. `LF_PlotBuyAction` shows only when
  `LF_PlotComponent.IsForSale()` (a price and not owned), so an owned plot no longer offers Buy; the server still refuses a
  forged buy. The owner's key is never replicated. Proof: `property/plot-owned-state` (the flag drives the action with the
  book emptied, as on a client); the late-joiner view is a manual step.

### Spawn house — `Feature/Property/LF_SetHomeAction.c` (2026-11-18, #2651, spec #811)
- 🟡 The owner of a house sees "Make this my spawn" at its door (`EL_Door_Base.et`, beside the lock action).
  The server takes the house from the door used; the client sends no id. A key holder or a stranger does not see
  it, and a forged perform or a direct `LF_Properties.SetSpawnHouse` by them is refused with a logged reason.
- 🟡 The choice is `m_bOwnerSpawnsHere` on the house's `properties` row (no new payload, no version change). One
  per owner: setting a second house clears the first. `LF_Properties.SpawnHouseOf(ownerKey)` returns it while the
  owner who set it still owns the house; a sold house is never the buyer's spawn, in session or after a save. Any owner change
  (`LF_HouseManagerComponent.SetOwner`, whatever calls it) clears the flag, so buying the house back does not restore it.
- 🟡 Applies at login only (the spawn flow reads `SpawnHouseOf`); deaths still wake at hospital. Proof: `property/set-home`.
