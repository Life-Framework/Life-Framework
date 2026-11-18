# Starter pack: town fit / Everon pipeline

Putting the 1989 town onto the real Everon map (`docs/architecture/everon-world.md` section 3). One place, five steps,
one file each, so many agents run at once without touching each other.

```
scout <place> ──> town map <place> (lead approves) ──> kits ──> town fit <place> <slot> ──> town check ──> draft PR
dossier.json      mapping.json                          tools/town/kits   Place_<place>_<slot>.layer
```
`tools\cli help` is the truth about which step exists; a step ships with its card (T1 to T5, #2202 to #2207).

## Steps and commands
```
tools\cli scout <place>                      # dossier: docs/everon/places/<slug>/dossier.{json,md} and img/
tools\cli scout label <slug>                 # img/overview-top-labelled.jpg: mapped buildings (b01, b14) and slots on the overhead shot; attach it to the mapping PR
tools\cli town map <slug> [--template town|village|districts] [--force]   # proposes slot -> building; `--check` refuses an incomplete one
tools\cli town kit extract <layer> <group>   # a kit from a building that already works; no absolute positions
tools\cli town fit <place> <slot> --no-look  # iterate with this: writes MainWorld_Layers/Place_<place>_<slot>.layer + a claim, audits; seconds
tools\cli town fit <place> <slot>            # the same, then TWO island looks (outside and --interior auto): once, at the end
tools\cli town fit <place> <slot> --refit   # re-fit a slot already claimed on main (its own claim); another PR's or slot's claim still refuses
tools\cli town fit <place> <slot> --check    # per-row seat corrections (floor, wall, clip) against the layer on disk; writes nothing
tools\cli town check <place>                 # static kinds and claims; then audit placement --world MainWorld
tools\cli town ready <state>                 # every place of the state passes, systems present, arrival route walks
tools\cli look place:<place>/<slot>          # the slot on the island (MainWorld): wide, obliques, top, kit close-ups
tools\cli look place:<place>/<slot> --interior auto   # its ground floor from inside
```
- **Pictures come from `look place:<place>/<slot>`** (docs/visual-testing.md "look place:"): it opens the pack's MainWorld
  and reads the viewpoint from the dossier and mapping. A plain area, `town:<area>` or `x,z` opens DebugWorld, where an
  island slot is sky. The "before" is the same command on main (the slot unfitted), the "after" on the draft branch.
  The work is about a minute; the queue for the island's RAM can be 30 minutes, so run it once per draft, in the foreground.
- **Iterate without pictures.** Fix kit rows with `town fit --no-look` and `--check` (seconds each). A plain `town fit`
  already takes both looks, so never follow it with `look place:` for the same slot. On 2026-11-18 a police draft re-ran
  a plain fit four times at about 10 min each.
- **A run past 600 s moves to the background.** Wait for its notification (or `t3_background_wait`). Never
  `Wait-Process`, `tail -F` or a second copy of the same look: that morning agents wrote 23 such waits.
- The mapping is a PR that holds only `mapping.json`; nothing is placed before the lead merges it.
- A fit runs in a worktree named `town-draft-<place>-<slot>`. Only that name writes an island layer.
- Its PR is a **draft** with before and after pictures. The maintainer reviews it before any merge.
- Claims live in `tools/town/claims/<place>-<slot>.json`; a claim `origin/main` or an open PR holds is refused.
  `--refit` ignores this slot's own claim on `origin/main` (re-fitting a slot already merged) and still refuses an open PR, a sibling worktree or another slot holding it.

## Known traps
- **Never touch the maintainer's worktree** (`ws/town-maintainer-edits`) or the playtest town's layers. The lead
  ships those.
- No step types a coordinate: every number comes from the dossier. A kit holds none.
- Rows are seated on the island host's own floors and walls (`tools/town/seat.mjs`); a row that still cuts the shell
  refuses the fit. Fix the kit row instead of reaching for `--allow-clip`. A socket-hung row (a fascia) is left out of the layer.
- Core never reads the mapping; what reaches the game is the fitted layer with `LF_PlaceComponent` and feature
  components. Core text stays neutral (place names are pack strings).
- A fitted vanilla building gets its destruction override (D15), or a fitted floor falls in.
- Towns are checked by audit and pictures, never by layout tests.
- A place waiting for its state stays `dark`: no spawn, interactable or NPC.
- Use Workbench `look`, not sessions: two island sessions do not fit the pool's RAM.
- `validate` checks only your slot (layer and claim agree, footprint, branch guard). The other slots of the place being
  unfitted is fine; `town check <place>` lists them as "not fitted yet". `town check <place> --complete` (and
  `town ready`) is the completeness gate, for the lead, not for a draft.
- Building interiors: `brief building` is the fit-out recipe, run in the debug grid first.

## Environment
No Workbench on the maintainer's PC (Gaming-Box); on lf-testbox (PC1) it is allowed through the pool (`toolscli status` says which you are on). Never pin a machine. Run in the foreground (timeout up to 600000 ms) or
`t3_background_wait`; never end the turn with a run pending. Never `--force`, `rm -rf` or `Remove-Item -Recurse` a
worktree (`wt prune`). Never admin-merge.
