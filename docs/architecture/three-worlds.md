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
| `test --tier all`, `--only`/`--red` with any WORLD or PERSISTENCE test | 2. Tests named in `tools/test/world-pins.json` boot 3 in a second run until that list is empty |
| `ci`, `wt gate`, `wt ship` suite | as `--tier all` |
| `e2e`, `--pre-move`, `host` | 3 (the playtest mission) |
| `session up` (default `--mission debug`) | 3, as today. `--mission grid` boots 2 |
| `grid place`, `grid list`, `grid free`, `place --layer Grid_*` | 2 |
| `look grid:<slug>`, `look grid:all` | 2 |
| `look <area>`, `look town:<area>` | 3 |
| `audit placement` | 3. `--grid` measures 2 at the grid bar |
| `validate-world-zones` | grid rules on 2, the town guard on 3 |
| Pass cache | the world is already in the key (`--world`), so a run on world 2 never stands in for a run on world 3 |

PERSISTENCE goes to world 2 with WORLD: its tests read annex fixtures, which stop existing in world 3
when the move ends. The real restart proof (`e2e`) stays on the playtest mission.

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
- The world keeps the annex coordinates (x 20..112, z 20..112) on the same flat terrain, so the
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
