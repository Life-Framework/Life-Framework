# Starter pack: author a shop in the grid, stamp it onto the island

The placement program's one path for shops and fitted buildings (`docs/architecture/placement-system.md` "Author in the
grid, stamp into the world", card #3881). Nobody edits a shop on the island: it is built around a copy of the same vanilla
building in a pack grid cell, lifted as a building-local kit, and stamped onto every island copy.

## Commands, in order

```
toolscli town author <kit> --from <place>/<slot> --dry   # which building prefab, which authoring bay (author-<kit>)
toolscli town author <kit> --from <place>/<slot> [--margin 1.5]   # grid place <slug> <prefab> --pack --bay [--margin m], then the kit's rows at the building
tools\cli look grid:author-<kit> --interior auto          # look, fix the rows in the cell's layer, look again
tools\cli audit placement --grid --strict                 # clean in the grid first
tools\cli town author extract <the cell's Grid_*.layer> '<group>' --kit <kit> [--clear {GUID}path,...]   # also releases the bay
tools\cli town author --reopen <kit>                       # edit it again: a bay is rebuilt from the kit
tools\cli town stamp <kit> --dry                          # every mapped island copy, writes nothing
tools\cli town stamp <kit>                                # writes Place_<place>_<slot>.layer + claim, then the island audit
tools\cli town stamp <kit> --terrain                      # a kit with yard rows: probe the island ground first (one test-server boot per slot)
```

The bay is scratch space (six 2x2 bays, `docs/architecture/world-zones.md` "Authoring bays"): extract saves the kit, archives the layer as `tools/town/kits/sources/<kit>.layer` and frees the bay so the next shop can use it; `--keep-bay` keeps it. No free bay means another author has not extracted yet.

`--reopen` is lossless (#3987): the bay holds every row with its till, sign and action blocks and a `KitClear_<kit>` carrier for `stamp.clear`, so the bay's audit and look see the building without the clutter the stamp hides. The next extract keeps what you hand-tuned (role, verb, wall, attach, footprint, front, counter walls, lane widths, system) for every row you did not move more than 10 cm or 5 degrees; a moved row re-reads its verb and wall. Edit the clear list in the carrier, or pass `--clear <res,...>` (`--clear none` empties it); `--fresh` recomputes everything. A first build can hide clutter in its bay too: `town author <kit> --from <place>/<slot> --clear <res,...>`.

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
- `stamp.clear`: vanilla clutter the building brings that the shop does not want; `town stamp` writes one `Fit_<slot>_clear` carrier per copy (`LF_BaseWorldClearComponent`) that hides it at load, on the
  server and every client, nothing saved (`docs/architecture/base-world-clear.md`). A fitted (not stamped) kit may carry a top-level `clear`.

## Rules the stamp enforces

- Same bytes twice: rows sorted by kit row id, mm rounding, the claim keeps its date while the layer is unchanged.
- The island audit is the verdict: a `floating`, `sunk`, `blocks-door`, `clips` or `overlaps` finding on a stamped row puts
  the previous layer and claim back and exits 1. Fix it in the grid, extract, stamp again.
- Wall-mounted rows (role `sign`, a hung kind, `mount: wall`) are seated on the island building's real wall, not carried by the
  grid offset: the stamp reads the host model, finds the wall plane under the plate (nine points across it), and where it
  steps (a door recess, a lintel) slides the row along the wall in 10 cm steps to the nearest flat run that cuts nothing, at
  most 3 m each way (`slide: <m>` on the kit row changes the reach). No flat run in reach: it stays seated at its centre and the
  stamp warns; the island audit decides. Floor rows are never moved by the stamp.
- Yard rows (`mount: terrain` or `ground`, props on open ground round the building) are seated on the ISLAND ground, never on the flat grid
  height (#3961): the row keeps its building-relative x, z and yaw, and its y comes from `tools/town/terrain/<place>-<slot>.json` (the
  pins `town fit --terrain` keeps too), written in the frame its prefab reads (`tools/lib/height-frame.mjs`). A stamp that needs a point
  with no pin is refused with `run town stamp <kit> --terrain`; that boots the island once per slot, keeps the pins, and plans again.
  A prop on a slope rests on the highest of its five footprint points, so its high side never sinks. A kit that stamps without a
  readable host model and has yard rows is refused.
- Island layer branches are `ws/town-draft-<place>-<slot>-...` (`validate-island-zones`).

## Traps

- `kit-grid put` (the seed) writes the kit's offsets on the cell's ground; run `place settle` or fix heights in the grid before
  extracting, since extract keeps heights relative to the building.
- A grid layer with more than one Structures prefab (two houses) needs `--building <EntityName>` on extract.
- Selftest: `node tools/validation/town-stamp-selftest.mjs` (fixture `tools/town/fixtures/stamp/`).
