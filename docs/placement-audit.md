# Placement audit

`tools\cli audit placement` measures every entity the DebugWorld layers place in the playtest town and
prints a ranked list of what is badly placed. It needs no game client and no session: it runs one
test on the dedicated test server (about 10 s on the pool), which loads the same DebugWorld the
playtest mission does. Run it from a worktree, before filming and after moving anything in a layer.
To place or move a prop, use `tools\cli place` (`docs/placement-tool.md`): it computes the transform
from real bounds and probe rays and runs this audit on what it wrote.

```
tools\cli audit placement                       ranked list, top 40; full report in tmp/place-audit/report.txt
tools\cli audit placement --area hospital       one area of agent-box/areas.json
tools\cli audit placement --layer Street_Hospital.layer,Signs.layer
tools\cli audit placement --annex               also the debug annex (the old test stations south-west of town)
tools\cli audit placement --from tmp/place-audit/out.txt   re-rank the last measurements (records.json beside it), no server
tools\cli audit placement --strict              exit 1 when anything is flagged (a pre-filming gate)
tools\cli audit selftest                        the Node checks on fixtures (also in tools\cli validate)
```

Each line is `rank. score  name [layer, area] @x,y,z: problem; problem`. The score adds a weight per
problem (floating 10, door 7, sunk 7, walk-up 4, overhang 6, clips 6, scale 6, unreachable 6, overlap 5, road 5,
tilt 2), grows with how bad it is, and counts 1.5x for an interactable and 1.3x inside a named area.
`report.json` beside the text has the same rows and the thresholds used.

## How it works

1. `agent-box/place-audit.mjs` reads every layer with `agent-box/layers.mjs` (name, prefab, position,
   layer, parent), drops the world, game mode and road layers and the debug annex, and writes one line
   per entity plus the town circle for doors to `tmp/place-audit/in.txt`.
2. It runs `tools\cli test --no-build --only LF_Test_PlacementAuditTown --local` with
   `LF_PLACE_AUDIT_IN` pointing at that file. `tools/test/test-e2e.ps1` copies it into the fresh test
   profile; the test finds each entity (`LF_PlacementAudit.FindPlaced`: by name, else the nearest of the
   same prefab within 1 m) and writes one measurement line each, plus every door leaf, to
   `$profile:place-audit/out.txt`. Without `LF_PLACE_AUDIT_IN` (a normal suite run) the test only checks
   the walk on the police evidence locker, which the audit found half inside the station wall.
3. The Node side adds the checks that pair entities, applies exemptions and the allowlist, and ranks.

## Checks

| Problem | Measured | Threshold |
|---|---|---|
| floating | bounds bottom above the surface under it: the higher of a trace down from 0.5 m above the bottom (every physics layer) and the top of a prop whose bounds stand under the centre | 0.15 m |
| sunk | bounds bottom and pivot both below that surface (a vehicle's wheels below its pivot, or a pivot left on the floor under a raised mesh, are not sunk) | 0.15 m (0.5 m for buildings) |
| overhang | a bottom corner (20% in from each side) with no surface within 0.2 m under it while the centre rests on a prop | any corner |
| clips | another entity's surface inside the shrunk bounds on two or more of 54 rays across them | 0.15 m in |
| overlaps | oriented boxes (local bounds through the transform, scale applied) interpenetrating | 0.08 m |
| blocks a door | an oriented box inside a door leaf's clearance (the leaf pushed out 1 m both ways, 2.2 m high) | 0.1 m |
| in a door's walk-up | the same box pushed out 3 m, for ground-floor doors, outside the door's own building (furniture inside it is not on the street path) | 0.1 m |
| in the road | the box centre on a carriageway of `Village_Roads.layer` (roads under 15 m wide, not the market, square or fuel apron) | half width - 0.3 m |
| scale | entity scale outside 0.5 to 2 (odd: outside 0.75 to 1.6, signs excepted) | |
| tilted | up axis off vertical | 5 deg |
| unreachable | interactables: no standing spot within 1.5 m of the edge (3 m for signs) | |

Exempt by kind (`RX` in `place-audit.mjs`): hung things (a sign, board or poster with a wall or another
entity within 0.3 m behind it, or any thin thing more than 0.5 m up against one) do not float, while a
sign on a pole or a board with nothing behind it must stand on something; sacks, logs, planks, tyres lie tilted; vegetation has no
collision; buildings clip roads and neighbours by design and are reported from the prop side; wall,
fence and rock runs overlap at their joints; something much smaller sitting inside a shelf, stand,
crate or table group is its contents; a layer parent and its child do not overlap each other; road
furniture, vehicles and markings may stand in a road.

## Allowlist

`agent-box/place-audit-allow.json` lists placements that are meant to look that way:
`{ "match": "<regex on name or prefab>", "kind": "<problem>", "layer": "<optional>", "why": "..." }`.
Add an entry only with a reason a reviewer can check in a shot.

## Limits

- Bounds are mesh bounds. A prop with an odd mesh (an open frame counter, a T-posed NPC) has a box
  bigger than what you see, so its overlap can be a false alarm; check the shot.
- A child of a rotated parent is placed where `layers.mjs` sums the coordinates, which ignores the
  parent's rotation; no layer nests that way today. An entity the test cannot find is listed under
  "not found".
- It measures where the layer puts each entity. A dynamic rigid body (a pickup with `RigidBody Active Active`,
  such as `WoodLog_01_Small`) is simulated on clients and can tip off a table that the audit passed: on
  2026-11-11 the logging stand's logs, laid along a 1.2 m table, fell upright in the after shot. Lay such
  items on the ground, or check them in a shot.
- It measures; it does not judge looks. The visual pass (`tools\cli look <area>` with no session, or
  `tools\cli shots area <name> --eye-only`; `docs/visual-testing.md`, `docs/agent-area-polish.md`) still catches the wrong asset, junk strewn about and odd scale that is
  inside the thresholds.
- Fixtures and the red-proof: `LF_Test_PlacementAudit` (WORLD) spawns a grounded, a stacked, a
  floating, a scaled and a half-off-the-table crate; `place-audit.mjs selftest` covers the pair checks.
