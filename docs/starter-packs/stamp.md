# Starter pack: author a shop in the grid, stamp it onto the island

The placement program's one path for shops and fitted buildings (`docs/architecture/placement-system.md` "Author in the
grid, stamp into the world", card #3881). Nobody edits a shop on the island: it is built around a copy of the same vanilla
building in a pack grid cell, lifted as a building-local kit, and stamped onto every island copy.

## Commands, in order

```
tools\cli town author <kit> --from <place>/<slot> --dry   # which building prefab, which authoring bay (author-<kit>)
tools\cli town author <kit> --from <place>/<slot>         # grid place <slug> <prefab> --pack --bay, then the kit's rows at the building
tools\cli look grid:author-<kit> --interior auto          # look, fix the rows in the cell's layer, look again
tools\cli audit placement --grid --strict                 # clean in the grid first
tools\cli town author extract <the cell's Grid_*.layer> '<group>' --kit <kit> [--clear {GUID}path,...]   # also releases the bay
tools\cli town author --reopen <kit>                       # edit it again: a bay is rebuilt from the kit
tools\cli town stamp <kit> --dry                          # every mapped island copy, writes nothing
tools\cli town stamp <kit>                                # writes Place_<place>_<slot>.layer + claim, then the island audit
```

The bay is scratch space (six 2x2 bays, `docs/architecture/world-zones.md` "Authoring bays"): extract saves the kit, archives the layer as `tools/town/kits/sources/<kit>.layer` and frees the bay so the next shop can use it; `--keep-bay` keeps it. No free bay means another author has not extracted yet.

`town stamp --all` re-stamps every kit that carries a `stamp` block (after a kit change).

## What the kit holds

- Rows in the building's own frame: `offset` (x, y, z relative to the building entity; the same prefab has the same floors, so
  the height is the shell's, never the terrain's), `yaw`, and `attach` (`floor`, `wall`, `shelf:<row>`, `counter:<row>`,
  `table:<row>`).
- `stamp.prefab`: the vanilla building it was authored around. A stamp onto any other prefab is refused.
- `stamp.lanes`: entrance to every till, counter and shelf face, 1.2 m (measured by the audit card #3883).
- `stamp.clear`: vanilla clutter the building brings that the shop does not want; `town stamp` writes one `Fit_<slot>_clear` carrier per copy (`LF_BaseWorldClearComponent`) that hides it at load, on the
  server and every client, nothing saved (`docs/architecture/base-world-clear.md`). A fitted (not stamped) kit may carry a top-level `clear`.

## Rules the stamp enforces

- Same bytes twice: rows sorted by kit row id, mm rounding, the claim keeps its date while the layer is unchanged.
- The island audit is the verdict: a `floating`, `sunk`, `blocks-door`, `clips` or `overlaps` finding on a stamped row puts
  the previous layer and claim back and exits 1. Fix it in the grid, extract, stamp again.
- Island layer branches are `ws/town-draft-<place>-<slot>-...` (`validate-island-zones`).

## Traps

- `kit-grid put` (the seed) writes the kit's offsets on the cell's ground; run `place settle` or fix heights in the grid before
  extracting, since extract keeps heights relative to the building.
- A grid layer with more than one Structures prefab (two houses) needs `--building <EntityName>` on extract.
- Selftest: `node tools/validation/town-stamp-selftest.mjs` (fixture `tools/town/fixtures/stamp/`).
