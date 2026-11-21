# Starter pack: author a shop in the grid, stamp it onto the island

The placement program's one path for shops and fitted buildings (`docs/architecture/placement-system.md` "Author in the
grid, stamp into the world", card #3881). Nobody edits a shop on the island: it is built around a copy of the same vanilla
building in a pack grid cell, lifted as a building-local kit, and stamped onto every island copy.

## Commands, in order

```
tools\cli town author <kit> --from <place>/<slot> --dry   # which building prefab, which cell (author-<kit>)
tools\cli town author <kit> --from <place>/<slot> [--margin 1.5]   # grid place <slug> <prefab> --pack [--margin m], then the kit's rows
tools\cli look grid:author-<kit> --interior auto          # look, fix the rows in the cell's layer, look again
tools\cli audit placement --grid --strict                 # clean in the grid first
tools\cli town author extract <the cell's Grid_*.layer> '<group>' --kit <kit> [--clear {GUID}path,...]
tools\cli town stamp <kit> --dry                          # every mapped island copy, writes nothing
tools\cli town stamp <kit>                                # writes Place_<place>_<slot>.layer + claim, then the island audit
```

`town stamp --all` re-stamps every kit that carries a `stamp` block (after a kit change).

## What the kit holds

- Rows in the building's own frame: `offset` (x, y, z relative to the building entity; the same prefab has the same floors, so
  the height is the shell's, never the terrain's), `yaw`, and `attach` (`floor`, `wall`, `shelf:<row>`, `counter:<row>`,
  `table:<row>`).
- `config` per row: the component overrides (till stock id, sign def, clerk and host settings) with the component GUID. Extract
  reads them from the grid row's components; a row the grid holds as a bare prop takes the config of the same row of the kit on
  disk (same id, a shared id suffix, or the one prefab; the command prints who carried and who is bare), and that kit's source
  files stay in `source.also` so nested blocks (stock lines) are still found. `extract --fresh` carries nothing.
- `stamp.prefab`: the vanilla building it was authored around. A stamp onto any other prefab is refused.
- `stamp.lanes`: entrance to every till, counter and shelf face, 1.2 m (measured by the audit card #3883).
- `stamp.clear`: vanilla clutter the building brings that the shop does not want; it stays an obstacle until #3882 can hide it.

## Rules the stamp enforces

- Same bytes twice: rows sorted by kit row id, mm rounding, the claim keeps its date while the layer is unchanged.
- The island audit is the verdict: a `floating`, `sunk`, `blocks-door`, `clips` or `overlaps` finding on a stamped row puts
  the previous layer and claim back and exits 1. Fix it in the grid, extract, stamp again.
- Wall-mounted rows (role `sign`, a hung kind, `mount: wall`) are seated on the island building's real wall, not carried by the
  grid offset: the stamp reads the host model, finds the wall plane under the plate (nine points across it), and where it
  steps (a door recess, a lintel) slides the row along the wall in 10 cm steps to the nearest flat run that cuts nothing, at
  most 3 m each way (`slide: <m>` on the kit row changes the reach). No flat run in reach: it stays seated at its centre and the
  stamp warns; the island audit decides. Floor rows are never moved by the stamp.
- Island layer branches are `ws/town-draft-<place>-<slot>-...` (`validate-island-zones`).

## Traps

- `kit-grid put` (the seed) writes the kit's offsets on the cell's ground; run `place settle` or fix heights in the grid before
  extracting, since extract keeps heights relative to the building.
- A grid layer with more than one Structures prefab (two houses) needs `--building <EntityName>` on extract.
- Selftest: `node tools/validation/town-stamp-selftest.mjs` (fixture `tools/town/fixtures/stamp/`).
