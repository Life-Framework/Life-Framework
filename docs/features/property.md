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
