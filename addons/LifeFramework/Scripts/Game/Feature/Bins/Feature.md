# Bins

> Slice #3323 (R1), parent #1019. Player docs: `docs/features/world-reset.md` (Bins).

## Intent

A bin is a container that anyone may open, that takes items while the server runs, and that is empty after
every restart. It is a real item sink and a dead drop. A server owner makes any prop a bin by giving it
`LF_BinComponent`, a vanilla storage component and a vanilla open-storage action;
`Prefabs/Props/Bins/LF_StreetBin.et` (over the vanilla `BinStreet_01`) is the first one.

## Shape

- `LF_BinComponent` registers each live bin by `EntityID`; `Empty()` deletes what is left inside through the
  bin's storage manager (server only). `EmptyAll()` does every bin.
- A bin is never saved: no `LF_PersistentStorageComponent`, no save section names it. A restart brings it back
  empty. In the debug and playtest world the world reset empties it (`LF_WorldReset_Bins`, step `bins`).
- Removal is always through the storage manager, never by deleting the prefab, so a bin stands and nothing is
  duplicated.
- Garbage round (R4) reads a bin's fill through `LF_BinComponent.GetItems()`.

## Tests

`LF_Test_BinEmptiesOnRestart` (WORLD).
