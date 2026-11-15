# DebugWorld coverage

- 🧪 **Maintainer world edits: `toolscli world diff|ship`** (2026-11-16; proof `tools/validation/world-diff-selftest.mjs`, `gate-tier-selftest.mjs`). `world diff` reads a worktree's `.layer` changes against its merge base as MOVED / ADDED / REMOVED / PROP CHANGED per entity (by name, or prefab plus parent path) and marks what Workbench rewrites on save as NOISE (the `default.layer` BSP line, re-ordered blocks, `angleY` vs `angles`, a dropped class prefix, whitespace). `world ship` reverts noise-only layers, commits, runs validate, the placement audit on the touched layers (allowlisting the maintainer's flagged placements) and the layer-named tests (a failing test is a warning listed under `## Test warnings (maintainer layout)` in the PR body, never a block; `world-test-judge-selftest.mjs`), then `wt ship`s; a `town-maintainer-*` branch with only such edits gates as T1w (no build, no full suite). `docs/worktrees.md` "Maintainer world edits".
- 🧪 **Maintainer gate: warn-only placement audit, own-change tier, `--body-file` PRs** (2026-11-15; proof `tools/validation/maintainer-gate-selftest.mjs`). On a `town-maintainer-*` branch the `ci` placement audit and the layout checks are warn-only at every gate tier (T1w, T1 and the full gate): they print `WARN`, write `tmp/gate-warnings.json`, and the ship queue lists them in the PR body under `## Gate warnings (maintainer layout)`. The tier is judged on the branch's own change (the newest main it merged), so a stale local `origin/main` no longer leaks main's files into a synced branch's diff; a refused hand edit now says why in the tier line. `world ship` caps its PR body at 60 KB with a `+N more, see world diff` line and `createPrGh` always passes `--body-file` (a 33 KB body failed with `ENAMETOOLONG`). Merged, in-dev.
 - ℹ️ **`Worlds/DebugWorld/DebugWorld.ent` is intentionally 0 bytes.** All
  DebugWorld content lives in `Worlds/DebugWorld/DebugWorld_Layers/*.layer`;
  `default.layer` carries the `GenericWorldEntity world` BSP and terrain
  entity. This layer loads before gameplay layers. Placing the world entity in
  `Terrain.layer` left earlier props without physics bodies even when they had
  `RigidBody` and `TRACEABLE` configured. Do not repair the empty `.ent`.
  `MainWorld.ent` differs because it
  is a `SubScene` wrapper over `worlds/Eden/Eden.ent`.
- ℹ️ **Direct collision fixtures need `Flags 0x403 0`.** This includes the
  `Traceable` flag. Without it, a `MeshObject` + `RigidBody` can render while
  character movement and projectile traces pass through it. Prefab fixtures
  such as `LF_TestRunner` inherit the flag; hand-authored `GenericEntity`
  fixtures must set it explicitly.
- ✅ DebugWorld is a one-street 1989 Everon village (M1-25, 2026-11-08;
  `docs/roadmap/m1-25-village.md` has the map). The main street runs east-west
  at z = 250 on the flat 512 m terrain (world 0..512, `TERRAIN_MAX` in
  `tools/street/manifest.mjs`; every heightmap sample is 0, so ground props sit
  at Y = 0). Sites stand in arrival order in front of vanilla buildings: the
  square (spawn, clerk, pub, church), post office, bank, general store and
  market, vehicle lot and garage, police station with a jail yard, hospital and
  fuel station, org registry, and the hideout down a side alley; the contraband
  barn is out of town on the east track. The farm, quarry, builders' yard and
  forest edge are on the outskirts. `Village_Roads`, `Village_Buildings` and
  `Village_Dressing` (in the pack world `LF_Everon1989/Worlds/Everon1989Town` since #323 batch 1) hold the roads (road generator splines saved as
  `RoadEntity`), vanilla buildings, walls, lamps and trees; `Annex.layer` is
  the net fence of the test annex at x 20..112, z 20..112, which holds the test
  runner, the PR fixtures, and the legacy debug stations (with their
  boundaries) on the ground where world tests spawn their characters. Sites
  keep their names; chain layers moved as blocks. The terrain material
  (`Assets/World/GridSurface.emat`, name kept so the terrain reference holds)
  is the vanilla lawn surface without clutter, so grass does not grow through
  roads and floors.
  `tools\cli validate` runs
  `validate-debugworld.ps1` to guard the layer set.
- ✅ Town tidy (M1-22, 2026-11-08): the street carries no debug artefacts. The
  old police duty table (`PoliceStation`, `PoliceStation_Debug`) and the
  whitelist table (`WhitelistStation`) moved into the annex (100 0 40 and
  100 0 52, names kept). The daylight blue kerosene lamps left the market,
  quarry, forest, and pub tables. The pub's survival points are an apple tree
  (`SurvivalPoint_Food`) and a plum tree (`SurvivalPoint_Water`) instead of a
  fridge and a US water trailer. `Village_Dressing` adds 98 vanilla props:
  bins, planters and flower boxes, a poster column and notice boards, the bus
  stop sign, hydrants, benches, shop crates, a garbage container, yards with a
  clothesline, firewood and a dog house, a fenced cabbage plot, trees behind
  both rows, and four parked cars. The cars are the mod's lockable S105, S1203
  and UAZ-469 with the vanilla `m_bIsLocked` set, so no key opens them.
- ✅ The WORLD-tier `e2e/narrative-loop` exercises the live Apple gather →
  trader cash payout → shop purchase → ATM deposit path. The robbery step calls
  `LF_RobAction.CommitRobbery` (2026-11-07; it used to hand-write the payout);
  its proceeds are asserted as cash and are not added to the bank.
- ✅ `debugworld/entity-collision` checks live physics bodies, geometry, and
  sphere sweeps through both apple trade crates and all three trader tables.
  Flags alone cannot prove collision. World initialization runs first in
  `default.layer` (verified 2026-09-19).
- Manual collision smoke after integrating the change: reopen DebugWorld,
  start a fresh play session, spawn as Civilian, and walk into each of the
  three apple trader tables from both sides. The character must stop at the
  tables, and the apple trade actions must remain usable. Repeat on a client
  joining a running dedicated server. These client checks are not automated.

