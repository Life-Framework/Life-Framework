# Three worlds: script test, debug grid, playtest

Maintainer decision, 2026-11-15, narrowed on 2026-11-16. Each test or session boots the
smallest world that holds what it needs, and every world stays on one topic. The epic and its
slice cards are on GitHub (label `slice`); this page covers the design and the order of the work.

| | 1. Script test | 2. Debug grid | 3. Playtest |
|---|---|---|---|
| World | `Worlds/TestLogic/TestLogic.ent` (exists, #1254) | `Worlds/GridWorld/GridWorld.ent` (new) | `Worlds/DebugWorld/DebugWorld.ent` and its pack SubScene `LF_Everon1989/Worlds/Everon1989Town` (unchanged paths) |
| Holds | World entity, time manager, game mode, destruction manager, `LF_TestRunner`. No terrain | The flat debug terrain, sky and light, game mode, one spawn per cell, the fenced annex and its legacy stations, every `Grid_<slug>.layer` | The village and the pack town, with no annex once the move ends |
| Addon | Core | Core | Core shell (terrain, game mode) plus pack town (`core-vs-pack.md`) |
| Who writes | Nobody (fixed) | Agents through `grid place` and the grid-refactor cards | The maintainer by hand in Workbench, and `town-` passes |
| Look bar | none | Low (`world-zones.md`) | Full polish |
| Boot target, server start to ready | about 2 s less than DebugWorld (measured, #1254) | no more than 60% of DebugWorld's time, measured in slice W2 | unchanged |

## Who boots which world

| User | World |
|---|---|
| `test --tier fast`, `--only`/`--red` of LOGIC tests | 1 (as today, `tools/lib/test-world.mjs`) |
| `test --tier all`, `--only`/`--red` with any WORLD or PERSISTENCE test | 2. Tests named in `tools/test/world-pins.json` boot 3 in a second run until that list is empty. The town tests (`tools/test/town-tests.json`) are in no tier run |
| `test --town` | 3, one boot, the 12 town tests only (below). `--only <town test>` boots 3 too |
| Pack tests (`--tier all`, `--tier persistence`, or named with `--only`/`--red`) | 2 under the pack test header, one more boot (below). Never under `--tier fast`, `--core-only`, `--world` or `--town` |
| `ci`, `wt gate`, `wt ship` suite | as `--tier all`, without the town tests; the pack leg runs after the shards |
| `e2e` | one pair of boots per world (#1993): 2 for the grid-fixture persistence tests, 3 for the town tests (below), 2 under the pack test header for PERSISTENCE pack tests. `--pre-move` keeps the town pair only |
| `host` | 3 (the playtest mission) |
| `session up` (default `--mission debug`) | 3, as today. `--mission grid` boots 2 |
| `grid place`, `grid list`, `grid free`, `place --layer Grid_*` | 2 |
| `look grid:<slug>`, `look grid:all` | 2 |
| `look <area>`, `look town:<area>` | 3 |
| `audit placement` | 3. `--grid` measures 2 at the grid bar |
| `validate-world-zones` | grid rules on 2, the town guard on 3 |
| Pass cache | the world is already in the key (`--world`), so a run on world 2 never stands in for a run on world 3 |

PERSISTENCE goes to world 2 with WORLD: its tests read annex fixtures, which stop existing in world 3
when the move ends. The real restart proof (`e2e`) therefore boots twice, save and reload, on each world
that holds PERSISTENCE tests: the `LF_GridWorld` mission for every test that is not a town test or a pin, the
playtest mission (the suite's `EL_DebugTest` header, or `--mission playtest`) for the town tests and the pins.
Each pair runs only its tests (`-Only`), so a test never boots a world without its fixtures. The plan is
`planE2eLegs` (`tools/lib/e2e-legs.mjs`, `e2e-legs-selftest`); `e2e/pass2-verify-load` and
`e2e/shutdown-save-loaded` follow `e2e/pass1-write-save` into its pair (the shutdown marker is deposited after that
test's manual save). `LF_TEST_GRID_WORLD=0` is one playtest pair of the whole tier, as before.

## Town tests: out of the suite, in one playtest run (#1895)

A test about the town itself needs no fixture, only the town, so it does not belong in a gate that boots
the grid. Twelve are named in `tools/test/town-tests.json`: `signs/coverage` and the roadmap and e2e
scenarios (`roadmap/contraband|escort|hideout|jailbreak|jailing|jailing-save`, `e2e/narrative-loop`,
`e2e/pass1-write-save`, `e2e/roadmap-contraband|jailbreak|jailing`). They stay registered (`regen-tests`
is unchanged); only where they run changes:

- `tools\cli test --no-build --town` boots the playtest world once (DebugWorld, mission 3) and runs
  exactly that list. `--tier all`, `--tier persistence`, `ci`, `wt gate` and `wt ship` skip it:
  `planLegs` leaves the tests out of the GridWorld leg, and `readRegistry` (`tools/lib/test-shards.mjs`)
  leaves them out of every shard plan. `LF_TEST_GRID_WORLD=0` is still the rollback: one DebugWorld boot
  of everything.
- **Who runs it.** The main guard runs `test --town` as its own warn-only stage after the suite goes
  green and posts a `main-guard/town` commit status; a red town stage never reverts and never changes
  `main-guard/suite` (`docs/worktrees.md` "Main guard"). The lead also runs it before `promote`. The
  guard wiring is a follow-up (#1901); until it lands the lead's run is the only one.
- **`e2e/pass1-write-save` and `roadmap/jailing-save` are PERSISTENCE tests.** `wt e2e` (and `e2e`) runs
  the whole persistence tier on the playtest mission through its own two-boot script, not through `test`,
  so it still runs them (and the reload boot checks `e2e/pass2-verify-load`). The `--town` entry is the
  single-boot run. The WORLD tests, `e2e/narrative-loop` among them, are covered only by `--town`.
- **Pins.** Their 12 entries left `tools/test/world-pins.json`; a test is a pin or a town test, never both
  (`test-world-selftest`). Every other pin ends in the grid-fixture cards P1 to P4; the list is empty
  when the last of them lands, and this page then says so.

## Pack test leg: pack data proved in game

A core test cannot name a pack file (`validate-addon-boundary`), and the grid boots no pack header, so
the pack's config overlays never applied in a test run. Pack data (a race course, a drink's values, a
shop's prices) was checked by node selftests reading `.conf` text, which shows nothing about what the
game loads. The pack test leg closes that gap:

- **Where the tests live.** `addons/LF_Everon1989/Scripts/Game/Tests/LF_Test_*.c`, one class per file
  extending `LF_Test`, with `// tier:` and `// red-proof:` lines as in the core. They may name pack files.
  `validate-addon-boundary` allows these files and nothing else in the pack: a pack test file declares
  only `LF_Test_*` classes, and the core still never names a pack class.
- **Registration.** `tools\cli regen-tests` writes `LF_PackTestRegistrations.generated.c` beside them: a
  `modded class LF_TestManager` that overrides `RegisterPackTests()` and calls `RegisterPack()` for each.
  The core registry is untouched. The runner skips a pack test unless it is named (`[LFTEST] SKIP ...
  pack test`), so no core leg runs one by accident.
- **The header.** `Missions/LF_Everon1989_Test.conf` inherits `LF_Everon1989.conf` (the same overlay list,
  manifest and flags), boots GridWorld (its test runner) and is hidden from Scenarios. `worlds.mjs`
  `PACK_TEST_MISSION` names it.
- **The leg.** `planLegs` (`packLeg`, `tools/lib/test-world.mjs`) adds one boot on that header for the pack
  tests of `--tier all` (every one but PERF) or `--tier persistence`, and for those an `--only`/`--red`
  names; a mixed `--only` splits into the core legs plus the pack leg, and each leg runs only the names
  that belong to it (a core leg never falls back to the whole tier, #2276); a name that matches no test
  stops the run with exit 2 (`splitSelection`, `test-selection-selftest`). A sharded run (`ci`, `wt gate`)
  runs the pack leg after the shards. `planE2eLegs` gives PERSISTENCE pack tests a save and reload pair
  on the same header. `pack-test-leg-selftest` proves the plan, the boundary and the header.
- **The first one.** `everon/drinks-impair` (`LF_Test_EveronDrinksImpair`, #2003) asserts that the moonshine
  and beer rows apply over the neutral core table under the pack header. It replaced
  `pack-drinks-impair-selftest.mjs`. The townsfolk test of #1960 waits for a pack-side grid layer: its
  NPCs are pack prefabs, which the core GridWorld cannot place.

One table of world paths, `tools/lib/worlds.mjs` (`TEST_WORLD`, `GRID_WORLD`, `PLAYTEST_WORLD`,
`gridLayersDir()`), replaces the paths each tool spells out on its own today (`addons.mjs`
`CORE_WORLD`, `world-zones.mjs` `DEBUG_LAYERS`, `place.mjs`, `place-audit.mjs`, `look.mjs`).

## How the grid moves: copy the layer files, then delete them from DebugWorld

**Copy, not reference.** A `.layer` file has no `.meta` and no GUID. A world loads every file in its
own `<World>_Layers/` folder, so nothing anywhere names a layer by `{GUID}path`. The only way for one
world to load another world's layers is a SubScene `Parent`, which loads all of them, the town
included (that is how `Everon1989Town` works). So the grid layers are copied verbatim into
`GridWorld_Layers/`:

- The entities keep their names. Tests look fixtures up by name, and `LF_Places` ids and saved keys
  default to the entity name, so tests and saves resolve the same in either world.
- The `{GUID}path` prefab references inside the layers are copied unchanged and stay valid.
- New GUIDs go only to the new world's own files: `GridWorld.ent.meta`, the mission header
  `Missions/LF_GridWorld.conf(.meta)` and, only if the terrain cannot be shared (below), the copied
  terrain's `.terr.meta`.
- The world keeps the annex coordinates on the same flat terrain (x 20..112, z 20..112 at the split; the fence now runs to x 160 after the lattice grew east, world-zones.md "The grid"), so the
  lattice maths in `world-zones.mjs` and every claim stay valid without changes.

**Terrain.** `default.layer` holds `GenericTerrainEntity DebugTerrain`, whose data lives in
`Worlds/DebugWorld/DebugTerrain/`. W1 first copies `default.layer` as it is. If the terrain does not
load in GridWorld (a fixture falls through, or `Unable to find` appears in the log), W1 copies
`DebugTerrain/` to `GridWorld/GridTerrain/` and gives the new world's terrain its own `.meta` GUID.
Either way, DebugWorld's terrain is never moved.

**What counts as grid.** An entity is grid when it stands inside the annex rectangle (x 18..114,
z 18..114) on a legacy annex layer or a `Grid_*` layer. Counted from the layers on 2026-11-16:
`Annex` 130, `Crimes` 2 of 9 (the other 7 are in town), `HousesKeys`, `Jobs`, `NPCs`, `PRFeatures`,
`Police`, `Tests`, `WhitelistChat`, all fully inside. `Shops` is entirely in town and stays.
No `Grid_*` layer or claim exists yet. `tools/grid/split-annex.mjs` does the split in both directions:
`--copy` writes the inside entities into GridWorld, and `--strip` removes them from DebugWorld. It
writes LF and keeps each entity's text byte for byte. `Gamemode`, `default` and `Factions` are
written new for GridWorld: its game mode, sky, terrain, and one spawn point at the annex gate, with
no town spawns and no town safe or lawless zones.

**The order.** The copy comes first. The tooling then switches to GridWorld, and the pinned tests
gain grid fixtures. The delete from DebugWorld happens last, in a single slice. Until the delete,
the annex exists in both worlds. To stop the two copies from drifting apart, `validate-world-zones`
refuses any change to an annex entity or a new `Grid_*` layer in DebugWorld from the moment W3 lands.

## Parallel work after the move

After the move, the grid-refactor cards (G1 to G5) edit only `Worlds/GridWorld/**`, their claims and
grid tooling. In the same period, the maintainer edits the town by hand in DebugWorld
(`ws/town-maintainer-edits`). No card except W6 touches a DebugWorld file. W6 deletes the annex from
DebugWorld: whole layer files, plus the 2 annex entities in `Crimes.layer`. It runs in a
`town-grid-removal` worktree, the marker the zone guard reads, and the maintainer's `world sync`
absorbs it. It is labelled `hold` until the maintainer says the town branch is synced.

## Rollback

- W1 to W5 and G1 to G5 only add files or move tool defaults. `LF_TEST_GRID_WORLD=0` sends WORLD
  and PERSISTENCE runs back to DebugWorld, and `--world <path>` always wins, so a broken GridWorld
  never blocks a gate. Reverting the PR restores the old default.
- W6 is one commit of deletions, and `git revert` restores the layers byte for byte. Saves made on
  the playtest world after W6 that touched annex entities load as "not there", which persistence
  already tolerates. W6 proves that with `wt e2e --pre-move <main before W6>`.
- If GridWorld cannot boot with the shared terrain, the fallback is the copied terrain (above), not
  a SubScene of DebugWorld.

## Not in scope

- `tools/commands/world.mjs`, `tools/lib/world-diff.mjs` and `tools/lib/world-sync.mjs` belong to
  the world diff, ship and sync work. No card here owns them.
- The playtest town's content and its cleanup backlog (`world-zones.md`).
- Renaming or moving DebugWorld. `Everon1989Town` names it as its SubScene `Parent` by path, and the
  core missions `EL_DebugTest` and `LF_HostedTest` name it by `{GUID}path`.
