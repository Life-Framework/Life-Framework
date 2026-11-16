# World zones: the debug grid and the town

Maintainer decision, 2026-11-15 (`docs/decisions.md`). The grid and the town have separate look bars
and writers, and since the three-worlds move (`docs/architecture/three-worlds.md`) separate worlds:
the grid lives in GridWorld, the town in DebugWorld and the pack world. The code is `tools/lib/world-zones.mjs`; the CLI is `tools\cli grid`;
the guard is `tools/validation/validate-world-zones.mjs`.

| | Debug grid | Town |
|---|---|---|
| Where | **GridWorld** (`Worlds/GridWorld/GridWorld.ent`): the net-fenced annex, x 20..328, z 20..112 (`Annex.layer` holds the fence; the gate is the gap in the north fence at x 69..79). Until slice W6 deletes it, the legacy annex also stands south-west of the village in DebugWorld, frozen (see "The annex freeze") | Everything else the playtest mission loads: the DebugWorld village layers (`Street_*`, the chain layers, `Signs`, `Survival`, `Roadmap`, ...) and every layer of the pack world `LF_Everon1989/Worlds/Everon1989Town` |
| Layers | `GridWorld_Layers/Grid_<slug>.layer`, one per feature (dashes in the slug become `_`), and `Annex`, which now holds only the perimeter fence (the stations moved one system to a cell, #2133) | Every other `.layer` in those two folders. `default`, `Gamemode` and `Factions` hold no props and belong to neither zone |
| For | Testing a system: one feature per cell, easy to find, easy to film for a proof | Players and videos: a real 1989 village |
| Look bar | Low. Debug-looking props, stand-ins and a test sign are fine. Things still stand on the ground and are reachable | Full polish bar (`.claude/skills/polish/SKILL.md`, `docs/style-guide.md`): NPCs dressed for their job and standing inside their building, furniture under a roof, boards on walls, nothing debug-looking |
| Who writes | Any slice, through `tools\cli grid place` only, inside its own cell | Only a town-placement pass: a worktree whose slug starts with `town-` |

## The grid

The grid is a 25 x 7 lattice of 12 m cells from x 22, z 22 (2 m inside the annex fence), ids `c0r0`
(south-west) to `c24r6` (north-east). It started 7 x 7 and grew east by four columns when the old square ran out of
free cells (2026-11-16): `c0r0` to `c6r6` keep their ids and coordinates, so every claim stands, and
`GRID_CENTRE` stays at 66,66 so no old cell's gap flips. The east wall of `Annex.layer` moved from x 112 to
x 160 and the north and south walls were extended. It grew east by four columns again (11 to 15, east wall x 208) when the first 24 m buildings (a town hall, a bank) found no free 2 x 2 or 3 x 2 block (2026-11-16); the new cells are `c11r0` to `c14r6`, and once more (15 to 25, east wall x 328, 2026-11-16) so that buildings of any size and the whole town pipeline find room for months: `c15r0` to `c24r6` are free, x 202..322, and the north gate and its track did not move. The margin is 2 m west and south of the lattice and 6 m east
and north (the gate and its track live in that margin). Terrain bounds: `GridTerrain` is 257 x 257 points at 2 m,
512 m square, so the grid is far inside it; `world-zones-selftest` asserts the fence stays
inside 0..512 (`TERRAIN_SIZE` in `tools/grid/lattice.mjs`). To grow again: raise `CELL.cols` or `CELL.rows`,
move the matching `Annex.layer` wall, run `node tools/grid/lattice.mjs write`. A cell is usable when none of these is in it:

- a legacy station: any entity of a non-grid layer (the annex fence runs excepted);
- the ground the world tests spawn their fixtures on, x 60..90, z 60..91, and the spot 48,64
  (`docs/roadmap/m1-25-village.md` "Test annex");
- the track in through the north gate, x 66..82, z 100..112.

Each legacy station a town pass retires, or moves into a cell of its own, frees ground. After the
2026-11-16 growth the east columns (`c7` to `c10`) are free of stations and reserved ground, so `grid list`
shows 28 more free cells.

**Big items take several cells.** A claim may cover a rectangle of cells: `grid place <slug> --span 3x2`
(`cols x rows`) takes the first free 3 x 2 block, or a block from `--cell` (its south-west corner). The claim
file records `"span": [3, 2]` beside `"cell"` (the corner); no `span` is one cell. One yard goes round the whole
block (fence runs of about 5 m, two left out for the gap on a side of 20 m or more), the entities may stand
anywhere inside the block, and every cell of it counts as claimed for `grid list`, the frame plates, the
open-PR and `origin/main` clash checks and `validate-world-zones`. A 3 x 2 block is 36 x 24 m: a 33 x 19 m hotel fits.

```
toolscli grid place <slug> <prefab>  [--dry] [--margin M] [--front +z] [--cell cXrY] [--span WxH] [--centre]   one building, sized
toolscli grid place <slug> [<prefab>...] [--name N] [--rows] [--dry] [--no-verify]                          rows of props
tools\cli grid list                    claims, free cells, and a map (north up)
tools\cli grid free <slug>             delete the feature's grid layer and its claim
tools\cli look grid:<slug>             the look pictures of that cell, from its gap
```

**One command for a building: `grid place <slug> <prefab>`.** Name a vanilla or addon prefab (`MunicipalOffice_E_01`,
`Villa_E_2I01`, a path or a `{GUID}path`) and the command does the sizing, the claim and the placement
(`tools/grid/autosize.mjs`; no coordinates, no `sed`):

1. **Bounds.** The same reader `toolscli place` uses (`prefabInfo`: the measured list, else the mesh's `.xob` header from
   the paks, offline, no Workbench): footprint, height, the footprint centre's offset from the pivot (a MunicipalOffice's
   is 0.36, 0.56 m; a Villa's pivot is 1.4 m off its centre), and the entity class read from the prefab's root
   (`SCR_DestructibleBuildingEntity` for the vanilla buildings), never hard-coded.
2. **Span.** `ceil((footprint + margin) / 12)` per axis, margin 4 m by default (2 m each side from the block's edge, 1 m
   inside the yard fence; `--margin M`). It tries yaw 0/180 and yaw 90/270 and takes the smaller block; of two blocks of the same
   size it takes the one whose front can face the yard's gap. A 15.3 x 12.7 m villa is 2x2, a 1 m crate 1x1, a 24 x 17.8 m
   municipal office 3x2 or 2x3, a 30 x 6 m long house 3x1 or 1x3. Height does not change the span.
3. **Claim.** The first free block of that size, from the slug's hash, against this worktree, `origin/main`, open PRs and
   sibling worktrees (spans included). No fit prints what it tried and the largest free block (`no free 3x3 block ... the
   largest free block is 2x2 cells (24 x 24 m) at c10r0`) and writes nothing. It does **not** grow the lattice
   (`tools/grid/lattice.mjs` `CELL.cols` is a separate change).
4. **Place.** A block of two cells or more (or `--centre`) gets the building: the footprint centred in the block, the yaw (0, 90,
   180 or 270) whose front (`+z`, `--front -z|+x|-x` for a mesh with another front) faces the gap, the ground height from
   `place ground`'s probe rays, then the placement audit of the whole cell (`audit placement --grid --strict --layer Grid_<slug>.layer`).
   If the place tool refuses the building (not level, overlap), the claim this call made and its yard are removed so no half-claimed
   block is left; `--force`, `--margin`, `--span` or `--cell` and run it again. A block of one cell keeps the row of props below
   (a crate is not a building); `--rows` forces the rows for any prefab.
5. **`--dry`** prints the bounds, the sizes the two orientations need, the block, the yaw, the pivot and the entity class, and writes nothing.
   `--span` and `--cell` still override the sizing (a span too small for the prefab is refused with what it needs).

```
grid: MunicipalOffice_E_01 {363A616E6C64BB7F}Prefabs/.../MunicipalOffice_E_01.et (SCR_DestructibleBuildingEntity): footprint 24.06 x 17.76 m, height 20.61 m (base -5.8 m from the pivot), footprint centre 0.36, 0.56 m (x, z) from the pivot
  sizing with a 4 m walk-around margin: 3x2 at yaw 0/180, 2x3 at yaw 90/270
grid: town-hall -> cell c11r4 +2x3 (x 154..178, z 70..106, gap west) (new)
  would place SCR_DestructibleBuildingEntity Grid_town_hall_Main (MunicipalOffice_E_01): footprint centre 166 88, pivot x 166.558 z 87.642, angleY 270; front (+z) faces the west gap; y from the ground probe
```

`grid place` for a new slug (the rows of props; a building takes the path above):

1. Picks the cell (with one prefab it first sizes the block as above): the first usable, unclaimed cell in lattice order, starting from a hash of the slug
   (FNV-1a modulo the 77 cells), so the same slug gets the same cell on every machine. `--cell` asks for a
   particular free cell. "Unclaimed" is read from this worktree **and from `origin/main`**: `place`
   fetches `origin/main` and treats its `tools/grid/cells/` claims as taken, so a slice cut before
   another slice merged still avoids that slice's cell (#1905 and #1906 both took c2r4 when each only
   looked at its own tree). It refuses a slug `origin/main` already claims. `--no-fetch` (also on
   `grid list`) reads the `origin/main` the clone already has; a failed fetch prints a note and goes on.
   Claims in flight count as taken too: `place` and `grid list` read the claim files of every OPEN PR
   (`gh pr list`, cached 60 s in the OS temp dir; #2014 and #2016 both took c0r6 because a PR's claim is
   not on `origin/main` until it merges). `list` shows them with their PR numbers and flags two open
   PRs on one cell. No `gh` or token prints a warning and goes on; `--no-fetch` skips it (offline).
   Claims not yet committed count too: a claim file still untracked in a sibling local worktree is on no
   remote and in no PR (`ws/police-station-kit` and `ws/building-bank` both took c3r1 for a 2x2, 2026-11-16).
   `place` and `list` read `tools/grid/cells/*.json` of every other worktree of the clone
   (`git worktree list --porcelain`: no network, so `--no-fetch` keeps it), treat those cells as taken and name
   the worktree in the clash (`claimed by <slug> in worktree <folder> (<branch>), not pushed yet`). A missing,
   pruned or locked folder and an unreadable file are skipped; no `git` prints a warning. A stale claim file
   left in an old worktree still blocks its cell until that worktree is pruned or runs `grid free`.
   A claim made in a worktree that is gone, or before the other slice's PR is open on another machine, can still clash;
   `validate-world-zones` catches that (below).
2. Writes `Grid_<slug>.layer` with the cell's yard: a 10 m net-fence square, 1 m in from the cell's
   edges, built from seven 5 m runs. Half the side facing the annex centre is left open as the way
   in, and a pole-plate sign stands just outside that gap. The sign id is `grid:<slug>`, and
   `LF_SignCatalog.GridLabel` prints the slug on the plate (`police-gates` reads POLICE GATES). It is
   not a string-table key, because the grid is a test zone.
3. Adds `Grid_<slug>_Spawn` (#1813, `tools/grid/cell-spawn.mjs`): a `LF_PlaceComponent` of kind `GridCell`, id = slug, in the corridor at the gap facing the yard. `/grid <slug>` and `teleport grid:<slug>` move a player there; `node tools/grid/cell-spawn.mjs --backfill` adds it to a cell that lacks one.
4. Records the claim in `tools/grid/cells/<slug>.json` (slug, cell, layer, rows used, date): one file
   per feature, never a central list (golden rule 10).
5. Puts the prefabs in a row inside the yard, facing the gap, through `tools\cli place arrange row`:
   the same offline checks, ground probe and audit as any placement. A layer needs no registration;
   GridWorld loads every file in `GridWorld_Layers/`. `grid list` and the blocked-cell check read
   GridWorld's layers, and `place --layer Grid_*` and `place on|against|ground` find and write
   `Grid_*` layers there (`GRID_LAYERS` in `tools/lib/world-zones.mjs`).

Each later `grid place <slug> <prefab>...` adds the next row. A cell has three rows, from the back
towards the gap. To put a prop on a table, against the yard fence, and so on, use the `place`
primitives with `--layer Grid_<slug>.layer`. Whatever the command, an entity in a grid layer must
stand inside its own cell.

`validate-world-zones` checks the claims on every validate. It fails when:

- two claims name one cell (the second slice runs `grid free` and `grid place` again). Two grid layers
  cannot share a cell: each layer's entities must stand inside its own claim's cell, and claims are unique;
- a claim names a cell `origin/main` gives to another slug (read from git; CI fetches `origin/main`, a
  local `validate` uses the one the clone has);
  (a cell another OPEN PR claims is only a warning, read with `gh` in CI: a flake never blocks a merge);
- a claim has no layer in `GridWorld_Layers/`, or a `Grid_*.layer` there has no claim;
- a grid layer places anything outside its cell;
- a `Grid_*.layer` is in `DebugWorld_Layers/` (the grid lives in GridWorld).

`validate-grid-world` adds one rule: every entity in a GridWorld legacy layer stands in a claimed cell. Police, Crimes, Jobs and NPCs (#1811) each moved into their own cell (`police`, `crimes`, `jobs`, `npcs`) with their entity names unchanged, and their free-standing layers are deleted. `PENDING` in that file is empty since #2133 moved the last stations out of `Annex.layer` (the annex fence runs are the grid wall and stay): 20 systems took cells `bank-heist` (with the breach and lockpick doors), `hideout`, `jail`, `chopshop`, `lockpick-cars`, `cash-van`, `police-desk`, `dispatch` (with the two test tills), `convoy`, `job-depot`, `zone-signs`, `apple-chain`, `org-desk`, `atm`, `furnace`, `gun-counter`, `hospital`, `chain-barrels`, `vehicle-lot` and `impound`, each shifted as one rigid group so its relative geometry is unchanged. The frame's gravel path stops at a claim spanning two rows, so a station may stand on the line between them.

## The frame

`Grid_frame.layer` (claim `tools/grid/cells/frame.json`, cell `lattice`) is the one layer that spans the whole lattice. It is written by `node tools/grid/lattice.mjs write`, not by `grid place`:

- `GridMarker_cXrY`, a plate at each cell's west edge reading its id (`c3r1`), and `GridPlate_cXrY` above it reading the claim's slug, FREE, or BLOCKED (legacy stations or reserved ground);
- `GridPath_rR_N`, gravel mounds end to end along the south edge of each row;
- `GridMapBoard`, 3.5 m in front of the gate spawn and facing it, titled with the claimed cells (cell to system). `grid list` prints the same map.

The plates are a snapshot of the claims: run `node tools/grid/lattice.mjs write` after claiming or freeing a cell (`check` exits 1 when the file is out of date). `validate-world-zones` checks only that the frame stays inside the lattice rectangle. `look grid:frame` shows the gate end and the board. The lattice constants (`CELL`, cell maths) live in `tools/grid/lattice.mjs`; `world-zones.mjs` re-exports them.

## The annex freeze

The legacy annex exists in both worlds until slice W6 deletes it from DebugWorld. To keep the two
copies from drifting, `validate-world-zones` also fails a branch that adds or changes an entity inside
the annex rectangle (x 18..114, z 18..114) in any DebugWorld layer. It compares each entity's text
with the merge base with `origin/main`; deleting is allowed, which is what W6 does. The edit belongs
in the copy in `GridWorld_Layers/`. The freeze applies to every branch, `town-` passes included; the
town guard below is unchanged.

## The guard

A slice never edits a town layer. `validate-world-zones` takes the change set against the merge base
with `origin/main` (untracked files included). It fails when that set contains a town layer and the
branch is neither of these:

- **a town-placement pass**: the worktree slug starts with `town-` (`tools\cli wt new town-<chapter>`,
  branch `ws/town-<chapter>`). The marker is the branch name because CI and the local validate both
  see it without asking GitHub (`GITHUB_HEAD_REF` in CI, else the checked-out branch);
- **a branch from before the guard**: listed in `tools/validation/zone-guard-baseline.json`. Every
  `ws/*` branch that existed locally, on origin or as an open PR when the guard landed is listed, so
  running slices keep working. The list never grows.

`main` and a detached HEAD outside CI (the push build of `main`) are not checked. A change to a town
entity that is not a placement (a sign id, a component value) also counts as a town change; it rides
with the next town pass, or the slice asks the lead for a `town-` worktree.

## How each check treats the zones

| Check | Town | Grid |
|---|---|---|
| `tools\cli audit placement --strict` | A flagged entity is a **warning on every branch**, never a failure | Fails on a flagged entity in a non-town layer the branch adds or moves (`--annex --hard-layer <layers>`, which `wt gate` passes from the diff); a grid layer the branch did not touch only warns |
| `validate-prop-on-surface` | A prop overhanging a surface in a town layer is a **warning on every branch** | A finding in a grid layer or a prefab still fails |
| `validate-town-manifest`, `validate-road-ids` | Warn on every branch (they read only town layers; a normal PR cannot change one) | Not checked |
| `validate-player-props` | Warns on a new `*_Debug` name or `/Debug/` prefab, in the DebugWorld village and the pack town world (the baseline is today's debt and never grows) | Not checked |
| `polish` checklist | Every line holds (`.claude/skills/polish/SKILL.md`) | Only "It sits on its surface" and "can be reached on foot"; one `look grid:<slug>` picture in the PR |
| `tools\cli place` | The full set of checks | The full set of checks (they measure physics, not taste) |
| `look` | `look <area>` or `look town:<area>`; `town:` refuses an area inside the grid | `look grid:<slug>` |

## Tests do not read the town

Maintainer direction, 2026-11-15: tests must not fail because he moved things in the town; most of
where things stand is arbitrary, so it is not written down. A test, harness proof or flow, the town
manifest, the harness selftest or a validator never quotes a town entity name and never writes a
position on the town. `validate-no-town-layout-asserts` enforces it (rules and escape hatch in its
header); `tools/validation/town-layout-baseline.json` is the debt and only shrinks. What to write
instead:

- a fixture in the grid (`toolscli grid place`) or a prefab the test spawns and deletes;
- a test-owned stand-in for a town fact (`LF_Zones.RegisterCircle` for a safe zone);
- a contract that must hold wherever things stand: find it by component, `LF_Places`, a sign id or
  the actions it carries, and measure from what was found.

The town manifest (`agent-box/town.mjs`) keeps its contracts (each site offers its actions, has a
sign with a card) and only warns about distances, on every branch.

**The town never blocks anyone** (maintainer rule, 2026-11-15; #1867 left the chop shop's `Workbench_Vice_01` 0.41 m
above its bench and failed every other PR). The town is hand-edited, so every layout finding on a town entity
is a warning on every branch: `tools/lib/town-findings.mjs` splits a finding by the layer it blames, and
`tools/lib/world-checks.mjs` (`TOWN_LAYOUT_CHECKS`) lists the checks that read only town layers.
`validate-world-zones` stays hard: a normal branch that changes a town layer still fails. Proof:
`tools/validation/town-warn-selftest.mjs`.

## The town-placement pass at chapter close

When every slice of a chapter has merged, the planner adds one **town placement** slice before the
integration run (`docs/process/roadmap-pipeline.md` §4). One agent places that chapter's systems in
town to the polish bar: it moves each new system from its grid cell into the building or spot where
it belongs, dresses and places its NPCs, and works through the cleanup backlog rows the chapter
touches. Its card (`docs/process/slice-card.md` "Town placement card"):

- type `town`, tier T1, model sonnet, machines `full-profile`;
- owns the town layers it names, plus `agent-box/place-audit-allow.json`;
- depends on every slice of the chapter;
- acceptance: `proof look` of each area it touched, before and after, `audit placement --area <area>
  --strict` clean, and the polish checklist ticked in the PR;
- worktree slug `town-<chapter>` (`tools\cli wt new town-<chapter>`), which is what the guard reads.

A grid cell stays after its system moves to town: the tests and proofs keep using it. `grid free`
retires a cell only when the system no longer needs a test spot.

## Town cleanup backlog

These town placements look like grid dumps, read from the layers on 2026-11-15 (furniture standing
outside any building) and the pictures under `website/public/docs-images/`. The first town pass works
through them. None of them moves in the PR that set up the zones.

| # | Entities (layer) | At x,z | What is wrong | Picture |
|---|---|---|---|---|
| 1 | `Roadmap_Hospital_Duty_Desk` (Street_Hospital) | 259.4, 256.4 | A small desk with the duty clock on the lawn in front of the hospital door. It belongs at reception, inside | `323/hospital-after.jpg`, `265/hospital-desk-frontage.png` |
| 2 | `Roadmap_Hospital_Till` (Street_Hospital) | 268.3, 257.3 | A till on its own outside the hospital | `323/hospital-after.jpg` |
| 3 | `Roadmap_Hospital_DispatchBoard` (Street_Hospital) | 264.0, 256.4 | A free-standing DISPATCH board on the lawn; it belongs on a wall inside | `323/hospital-after.jpg` |
| 4 | `Hospital_SupplyCrate_1`, `_2`, `Hospital_WaitBench` (Street_Hospital) | 271.6, 256.3 | Supply crates and a waiting bench outside the door; they belong in the ward and the waiting room | `323/hospital-after.jpg` |
| 5 | `Roadmap_Org_Desk` with `_Phone`, `_Files`, `_Notebooks`, `_Clock`, `_Chair` (Street_Org) | 288.9, 255.7 | An office desk set on the grass verge beside the ORGANIZATION OFFICE | `619/org-desk-wide.png` |
| 6 | `Roadmap_Dispatch_Console`, `_Phone`, `_PoliceBoard`, `_MedicBoard` (Street_Dispatch) | 249..257, 229..233 | The dispatch test area (a console with a debug test call, two boards) stands in the street; the boards belong in the police station and the hospital, and the test console in a grid cell | `269/dispatch-after-robbery.png` |
| 7 | `Roadmap_Test_Till`, `Roadmap_Test_Till_Counter` (Street_TillTest) | 244.5, 230.5 | A test till on a shop counter in the open: a test spot, which belongs in a grid cell | none |
| 8 | `Roadmap_Impound_Desk` (Street_Vehicles) | 246.0, 269.0 | A desk in the car lot; it belongs in a lot office or the police station | none |
| 9 | `Roadmap_Job_Depot` (Street_Jobs) | 155.2, 256.3 | A post-office counter in the street; it belongs in a depot or the post office | none |
| 10 | ~~`SquareClerkDesk` (pack Village_Dressing)~~ | 136.0, 243.5 | Done (town-arrival-desk): the desk and chair are gone; `Quest_Clerk` stands behind `SquareRegistryCounter` under `SquareRegistryParasol`, with the `Sign_Registry` board (REGISTRY OFFICE) beside it, facing the spawn | `arrival-desk/arrival-desk-after.png` |
| 11 | `Roadmap_Police_WantedBoard`, `Roadmap_Police_DispatchBoard` (Street_Police) | 235..238, 242.9 | Free-standing boards in front of the police villa; they belong on a wall inside | none |
| 12 | `Roadmap_Bank_RobKit` (Street_Bank) | 196.7, 258.5 | A debug-looking kit stand by the bank (baselined in `player-props-baseline.json`) | none |
| 13 | `Roadmap_ChopShop` (Street_ChopShop) | 322.0, 300.0 | The chop bench in the open; it belongs in the garage | none |
| 14 | `Roadmap_Hideout_Workbench` with `_BenchBox`, `_BenchJerrycan`, `_BenchCarton`, and `Roadmap_Hideout_Machine` (Street_Hideout) | 298..308, 302..309 | Workbench and machine out in the yard; they belong in the shed | none |
| 15 | The other 17 rows of `player-props-baseline.json`: the `*_Debug` entities of CementChain, CrimeChain, GravelChain, LoggingChain, MiningChain, SandChain and Survival, the mining sign in the pack's Village_Signs, and `Sign_DebugJailMe` (Street_Jail) | the quarry, the farm, the forest edge, the jail | Debug-named stand-ins in town | none |

Two more notes for the first pass:

- `Shops.layer` is a legacy annex layer, so the guard and `validate-player-props` treat it as grid.
  Its general store (`Shop_GeneralStore_Debug`, the till, radio, clerk and alarm box) stands in town:
  since town-general-store (2026-11-15) inside the village shop house (`Village_GeneralStore`, pack
  `GeneralStore.et`, near 180.6, 233.6). Renaming the layer's town part is still open; the store
  stays a core entity because the suite boots the core world.
- The farm stand, the market stalls and the job stands outdoors are where such things belong; the
  scan listed them and they are not on the backlog.
