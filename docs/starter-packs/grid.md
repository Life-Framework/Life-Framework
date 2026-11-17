# Starter pack: grid feature proof

A world feature is built and shown in its own debug-grid cell in GridWorld: low look bar, one feature per cell
(`docs/architecture/world-zones.md`). Never a town layer.

## Commands
```
tools\cli wt new <slug> --wait
tools\cli grid list                                        # claims, free cells, a map (north up)
tools\cli grid place <slug> [<prefab>...] [--cell cXrY] [--span WxH]   # yard, label sign, props placed and audited
tools\cli place on|against|ground|arrange ...              # more props; never typed coordinates (docs/placement-tool.md)
node tools/actions/place-action-contexts.mjs               # a new world interactable
node tools/grid/lattice.mjs write                          # after claiming or freeing: the frame plates
tools\cli look grid:<slug> --close 0                       # the picture; one per change, from the cell's gap
tools\cli audit placement --grid --strict                  # must be clean
tools\cli evidence <issue> after grid-<slug>               # the folder `look` wrote in tmp\look\; `evidence <issue> table` prints the PR markdown
tools\cli grid free <slug>                                 # undo a wrong claim
```
You own `GridWorld_Layers/Grid_<slug>.layer` (dashes become `_`) and `tools/grid/cells/<slug>.json`. `--span 3x2`
takes a block of cells for a big item; `--cell` is its south-west corner.

## Known traps
- A claim is read from this worktree and `origin/main` and open PRs; two PRs can still take one cell. If
  `validate-world-zones` names a clash, `grid free <slug>` and `grid place` again.
- A sibling worktree's uncommitted claim is invisible until #2221 lands; run `grid list` last before you ship.
- A pack prefab cannot be named by a core grid layer (`validate-addon-boundary`): `grid place` routes a cell with pack content to
  the pack grid world on its own (`--pack` forces it; `world-zones.md` "Pack cells").
- A placement meant to look odd goes in `agent-box/place-audit-allow.json` with a reason.
- `look` needs a Workbench on a pool machine; use a session only for UI, motion, a second player, sound or filming.
- Core tests keep annex names; never assert a town coordinate. Tests run on the grid, not the town.
- A grid `GridWorld` look shows the real prop on flat ground: judge "sits on its surface" and "reachable on foot", not
  decor.

## Environment
No Workbench on this PC and never pin a machine. Run in the foreground (timeout up to 600000 ms) or
`t3_background_wait`; never end the turn with a run pending. Never `--force`, `rm -rf` or `Remove-Item -Recurse` a
worktree (`wt prune`).
