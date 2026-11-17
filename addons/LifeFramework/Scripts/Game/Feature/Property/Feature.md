# Property — Design Requirements

> Context file for the property book (#2646, spec #756). Player docs: `docs/features/property.md`.

## Intent

A property is a house or a plot a character (or an organisation) owns, rents or builds on. The server
keeps one record per property by a stable id so building progress, rent and the spawn choice survive a
restart.

## Shape

- `LF_PropertyRecord`: one row, append-only, VERSION 1. Stable id = house identifier or plot key.
- `LF_Properties`: the server book (static map), `ExportAll` / `ApplyAll` for the `properties` payload.
- A house's owner of record is in `houses` (`LF_Houses`); this book mirrors it. Contents of a house ride
  `LF_PersistentStorageComponent` under `property:<id>`, not this record.

## Rules

- Never coordinates, never an EntityID in a row. A row whose property is absent waits and is saved again.
- New fields go at the end of the record and raise VERSION (docs/persistence.md "Record shapes").
