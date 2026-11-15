# World zones: the debug grid and the town

Maintainer decision, 2026-11-15 (`docs/decisions.md`). DebugWorld has two zones, each with its own
look bar and its own writers. The code is `tools/lib/world-zones.mjs`; the CLI is `tools\cli grid`;
the guard is `tools/validation/validate-world-zones.mjs`.

| | Debug grid | Town |
|---|---|---|
| Where | The net-fenced annex south-west of the village, x 20..112, z 20..112 (`Annex.layer` holds the fence; the gate is the gap in the north fence at x 69..79) | Everything else the playtest mission loads: the DebugWorld village layers (`Street_*`, the chain layers, `Signs`, `Survival`, `Roadmap`, ...) and every layer of the pack world `LF_Everon1989/Worlds/Everon1989Town` |
| Layers | `DebugWorld_Layers/Grid_<slug>.layer`, one per feature (dashes in the slug become `_`), and the legacy annex layers (`Annex`, `Crimes`, `HousesKeys`, `Jobs`, `NPCs`, `PRFeatures`, `Police`, `Tests`, `WhitelistChat`, `Shops`) | Every other `.layer` in those two folders. `default`, `Gamemode` and `Factions` hold no props and belong to neither zone |
| For | Testing a system: one feature per cell, easy to find, easy to film for a proof | Players and videos: a real 1989 village |
| Look bar | Low. Debug-looking props, stand-ins and a test sign are fine. Things still stand on the ground and are reachable | Full polish bar (`.claude/skills/polish/SKILL.md`, `docs/style-guide.md`): NPCs dressed for their job and standing inside their building, furniture under a roof, boards on walls, nothing debug-looking |
| Who writes | Any slice, through `tools\cli grid place` only, inside its own cell | Only a town-placement pass: a worktree whose slug starts with `town-` |

## The grid

The grid is a 7 x 7 lattice of 12 m cells from x 22, z 22 (2 m inside the annex fence), ids `c0r0`
(south-west) to `c6r6` (north-east). A cell is usable when none of these is in it:

- a legacy station: any entity of a non-grid layer (the annex fence runs excepted);
- the ground the world tests spawn their fixtures on, x 60..90, z 60..91, and the spot 48,64
  (`docs/roadmap/m1-25-village.md` "Test annex");
- the track in through the north gate, x 66..82, z 100..112.

On 2026-11-15 that leaves 15 free cells. Each legacy station a town pass retires, or moves into a cell
of its own, frees ground.

```
tools\cli grid place <slug> [<prefab>...] [--name N] [--cell cXrY] [--dry] [--no-verify]
tools\cli grid list                    claims, free cells, and a map (north up)
tools\cli grid free <slug>             delete the feature's grid layer and its claim
tools\cli look grid:<slug>             the look pictures of that cell, from its gap
```

`grid place` for a new slug:

1. Picks the cell: the first usable, unclaimed cell in lattice order, starting from a hash of the slug
   (FNV-1a modulo 49), so the same slug gets the same cell on every machine and two slices running
   at once rarely collide. `--cell` asks for a particular free cell.
2. Writes `Grid_<slug>.layer` with the cell's yard: a 10 m net-fence square, 1 m in from the cell's
   edges, built from seven 5 m runs. Half the side facing the annex centre is left open as the way
   in, and a pole-plate sign stands just outside that gap. The sign id is `grid:<slug>`, and
   `LF_SignCatalog.GridLabel` prints the slug on the plate (`police-gates` reads POLICE GATES). It is
   not a string-table key, because the grid is a test zone.
3. Records the claim in `tools/grid/cells/<slug>.json` (slug, cell, layer, rows used, date): one file
   per feature, never a central list (golden rule 10).
4. Puts the prefabs in a row inside the yard, facing the gap, through `tools\cli place arrange row`:
   the same offline checks, ground probe and audit as any placement. A layer needs no registration;
   DebugWorld loads every file in `DebugWorld_Layers/`.

Each later `grid place <slug> <prefab>...` adds the next row. A cell has three rows, from the back
towards the gap. To put a prop on a table, against the yard fence, and so on, use the `place`
primitives with `--layer Grid_<slug>.layer`. Whatever the command, an entity in a grid layer must
stand inside its own cell.

`validate-world-zones` checks the claims on every validate. It fails when:

- two claims name one cell (the second slice runs `grid free` and `grid place` again);
- a claim has no layer, or a `Grid_*.layer` has no claim;
- a grid layer places anything outside its cell.

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
| `tools\cli audit placement --strict` | Fails on any flagged entity | Not measured by default. With `--annex` its rows are listed, and `--strict` ignores them |
| `validate-player-props` | Fails on a new `*_Debug` name or `/Debug/` prefab, in the DebugWorld village and the pack town world (the baseline is today's debt and never grows) | Not checked |
| `polish` checklist | Every line holds (`.claude/skills/polish/SKILL.md`) | Only "It sits on its surface" and "can be reached on foot"; one `look grid:<slug>` picture in the PR |
| `tools\cli place` | The full set of checks | The full set of checks (they measure physics, not taste) |
| `look` | `look <area>` or `look town:<area>`; `town:` refuses an area inside the grid | `look grid:<slug>` |

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
  Its general store (`Shop_GeneralStore_Debug`, the till and radio on it, near 179, 244) stands in
  town. The town pass either moves the store into a real shop building and renames the layer's town
  part, or moves the debug store into a grid cell.
- The farm stand, the market stalls and the job stands outdoors are where such things belong; the
  scan listed them and they are not on the backlog.
