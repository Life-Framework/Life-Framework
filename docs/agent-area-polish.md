# Area polish pass

This pass goes through the playtest town (DebugWorld, `LF_PlaytestTown`) one area at
a time. For each area it:
1. fixes how the existing things are placed;
2. adds a few props where the chain needs them;
3. retests that area's features;
4. takes the screenshots the website uses.

It runs after M2 QA, one area at a time, so it never competes for the game PC.

## Look (maintainer, 2026-11-10)

**Tidy.** First make everything already there sit right: on the ground or the
surface, upright, not floating, not clipping, not blocking a door or path, and facing
the way a player approaches.

**Then add a few extra props, only in the area each chain uses.** For example, crates
by the fence, barrels and sacks at the moonshine site, cones and a stretcher in the
ambulance bay, a filing cabinet by the evidence locker. Base-game assets only, kept to
1989 Everon. Don't scatter clutter across the town.

## Areas, in order

1. Spawn square and safe zone
2. Bank: ATM and vault
3. General store and market, including the #202 shelves and till
4. Car lot and garage
5. Fuel station and till
6. Police station: duty desk, wanted and dispatch boards, evidence locker, armory, jail
7. Hospital: desk, beds, ambulance bay
8. Payphones (square, fuel, hospital, police)
9. Fence (behind the barn)
10. Cash van route and stops
11. Quarry (lawless) and mining
12. Farm and trader chains
13. Hideout, organization and stash
14. Lock-up: lockpick door and car
15. Chop shop yard
16. Moonshine site (barrels and still)
17. Town signs (one sweep across all signs)

## Per area: builder (`lf-builder`, Sonnet)

1. **Before shots.** Use the daily baseline, no session: `tools\cli shots area <name> --baseline` prints the path
   of that area's `contact.jpg`, `index.json` and `problems.txt`, and how many commits old it is (see Daily baseline).
   Run `git log <stamp>..origin/main -- <the area's layer, prefabs, scripts>`; if that is empty the baseline is
   your before shot. If your area changed since, or there is no baseline, shoot your own once (`shots area <name> --annotate`).
2. **Fix placement.** Read the baseline's `problems.txt` first and fix those: floating,
   sunk, tilted, clipping, unreachable. Then, for every entity in the area: ground snap and surface height,
   rotation, collision, and reachability of each interaction point. Check each one with
   `wb_read_props` or the layer coordinates, and with a close shot.
3. **Check every prefab is the right one.**
   - **Right asset:** each entity in the area must use a base-game or mod asset that
     actually is the thing. A payphone is a phone booth, not a radio. A bed is a
     medical bed. A still is a still-like object.
   - **Wrong asset:** when there's a better base-game match (`asset_search`), swap
     to it.
   - **No real match:** keep the closest stand-in and record it as a placeholder in
     `docs/asset-inventory.md` (next section).
4. **Add props.** A handful per chain area, from `asset_search`. Keep counts modest:
   static props, no lights or particles unless the chain needs them.
5. **Retest the area's features.** Run its `--only` tests plus `debugworld/entity-collision`,
   and that area's `session.mjs` proof verb if it has one.
6. **After shots:** this is the builder's only session: run `shots area` once; `problems:` must be empty or each line explained. Judge the
   look from `contact.jpg` (add `--annotate` to see entity names), then hand the area to the reviewer.
7. **Ship one PR per area:** before and after shots in the body, the final shots in
   `website/public/docs-images/area-<n>-<slug>/`, and a `docs/playtest.md` line if anything
   moved.

## Per area: reviewer (a separate agent, Sonnet)

The reviewer gets only the shots and this checklist, not the builder's reasoning. Each
item passes or fails:

- Nothing floats or sinks. Items rest on the ground, a table or a shelf.
- Nothing clips through walls, other props or vehicles.
- Interactables are reachable and face the approach. Signs are readable and centred.
- The area reads at a glance as what it is: the wide shot tells you "this is the hospital".
- Every object is what it claims to be. A stand-in counts only if it appears in
  `docs/asset-inventory.md` as a placeholder.
- The props fit 1989 Everon and stay inside the chain's area. Nothing modern, no clutter
  spilling into the street.
- The shot rules below are met.

Any failure goes back to the builder with the item and shot named. Two failed reviews
on an area: ship what passes and list what's left. The maintainer reviews the final
gallery.

## Daily baseline

`tools\cli shots baseline [--areas all|a,b,c]` runs `shots area <name> --annotate` for every area in
`agent-box/areas.json` inside one session (a one-client playtest session, brought up and down once) and stores
each area's `contact.jpg`, `index.json`, `problems.txt` and a `stamp.json` (the main commit shot, the time) in
`tmp/shots-baseline/<area>/` of the main checkout, which every worktree can read. `shots area <name> --baseline`
prints that path and how many commits old it is (counted from the stamp to `origin/main`); it never starts a session.

- **Cost:** about 2.5 to 6 minutes per area (the wide-frame scoring dominates), so all 21 entries do not fit the 45-minute session hold cap (measured 2026-11-10: 4 areas in 33 min on the laptop). Run it on an idle pool (the cap only bites while others queue) or in slices that share the store: `shots baseline --areas bank,hospital,...`; each slice is one session and replaces only its own areas' folders.
- **Schedule: once a day**, from a worktree merged with `origin/main`, e.g. `tools\cli shots baseline`. Set `LF_SHOTS_BASELINE=<dir>` to store it elsewhere.
- **A builder takes no before session.** It reads the baseline, checks nothing in its area changed since the stamp, and spends exactly one session on the after shots.

## Shot rules

Use `toolscli shots area <name | x,z>`. Do not hand-place cameras for these shots. One
blocking call, about ten lines of output, no manual steps.

```
toolscli shots area hospital [--radius 18] [--annotate] [--pick] [--detail <Entity>] [--machine any]
toolscli shots crop wide 900,0,380,140
```

- **`--eye-only`** skips the wide (its scoring is most of an area's minutes) and keeps the four eye-level shots and the close-ups: about 75 s an area, so every area fits one session. Pair it with `tools\cli audit placement` (`docs/placement-audit.md`), which measures the whole town in 10 s without a session.

- **The areas** are in `agent-box/areas.json` (the 17 above; payphones has one entry per phone).
  A raw `x,z` also works; the ground height is looked up in-game.
- **Same scene every time.** Golden hour (local 17.9, the game clock reads 4.73 h less at this longitude), clear weather, no HUD, no hint box. The clock is set on the server by the harness `time <hour>` verb (an authority-side RPC, like pose); a client-side clock is overridden. The call prints the reply (`local=17.9 game=13.77 ...`). Nothing ships.
- **Fixed angles from the centre:** a wide taken from the area's `front` heading in `agent-box/areas.json` (the side players approach from and the main facade faces; 105 candidates within 65 degrees of the front, at eye level, 3 m and 6.5 m up, 0.45 to 1.9x the radius (or the `focus` framing size); the best six are ray-scored and the two best are shot as `wide` and `wide-2`), four eye-level
  shots at 1.7 m from N, E, S, W at the radius edge looking in, a top-down (only with `--free-camera`, see below), and a 2 to 3 m eye-level
  close-up of each interactable in the radius (the eight most important first: actions and size, signs and bystander NPCs last).
- **Camera guard.** Each camera is probed in-game: not inside a wall, no ceiling pressing on it, and a clear
  view line to the target. A blocked camera is backed off or turned to the next candidate; the label gets
  a `~` when that happened. A shot that is nearly one flat colour is flagged `suspect`.
- **Output** (default `tmp/shots/<area>/`):
  - `contact.jpg`: the sheet agents read. At most 1568 px on the long edge, 3x3 or 4x3, label under each
    tile, JPEG q85, about 3k tokens. `!` after a label means that entity has a problem.
  - the 1280-wide PNGs (`wide.png`, `top.png`, `N.png`..., `close-<Entity>.png`): for the website and for
    close inspection. Open one only when the sheet is not enough.
  - `index.json`: each shot's camera pose and entity, and every entity's measurements.
- **`--annotate`** writes each entity's name at its projected screen position on the contact sheet, so a
  problem can be tied to an entity without guessing. The projection assumes a fixed field of view.
- **Numbers before pictures.** For each entity in the radius the call measures, and prints under
  `problems:`: **floating / sunk** (the bounds bottom against the surface under the entity, found by a ray
  down from 0.5 m above the bottom so slabs and floors count, over 0.15 m; sunk also needs the pivot not above
  the floor, so a vehicle with a high pivot is not "sunk"); **clips** (a surface of another entity inside the
  entity's own bounds: rays cross the bounds on all three axes and keep hits more than 0.1 m inside, so an
  item inside a villa does not clip the villa's overall box, a payphone inside a wall does); **tilt** over 5
  degrees; and, for interactables, **reach** (from the bounds edge to the nearest standing spot on any side:
  floor within 0.7 m of its own floor and 1.4 m of air; over 1.5 m, or none within 3 m, means unreachable;
  walls behind it do not matter). Wall-mounted things (signs, anything above 1.5 m) skip the gap check.
  **faces away** compares an interactable or NPC's layer yaw with the direction to the area's approach point (`front` in `areas.json`, a radius out) and flags over 100 degrees; vehicles and pickups are exempt, and a yaw the layer sets on a parent is not seen.
  A clip against a map building is reported by that building's prefab name.
- **`shots crop <shot> <x,y,w,h>`** writes a native-resolution crop, at most 800 px, next to the shot. Use
  it to check one spot (a crate's foot on the ground) instead of loading the whole shot. `<shot>` is a
  path, or a name such as `wide` from the newest area folder.
- **Scores.** The wide is scored for what a player wants to see: `heroVis` (the area's interactables, plus any entity named in `focus` in `areas.json`, weighted by actions and size: in frame, big enough to read, and visible by 2 rays each), `backdrop` (rays that land on the area's structures behind them), minus `foreground` and `bigNear` (non-hero objects, and objects over 2.5 m, in the near 75% of the depth: canopies, walls, roofs), `treeNear` (canopies have no collision, so they come from the layers) and `crowd` (poles at the lens); plus a bonus for eye level and a 20 to 60 degree angle off the front (`bonus`). `index.json` has every scored candidate under `wideCandidates` with that breakdown and `blockedBy` (what hid a hero or filled the foreground), and the best one is printed on a `wide:` line. `focus` also moves the aim and the stand-off distance (cash-van: the van and the bank sign). Set `front` (and `focus` for a large radius) for a new area before relying on the wide.
- **`--pick`** copies the best-scoring wide and the best-scoring close-up (by importance: actions and size) to
  `website/public/docs-images/area-<n>-<slug>/` as `wide.png` and `detail.png`; `--detail <Entity>` picks
  another close-up. The builder may override either.
- **Known limits.** The player camera cannot pitch, so by default the wide is a level view from 12 m up and there is no top-down. `--free-camera` flies a free camera (real pitch, real top-down) but the client crashed about 4 s after the first shot, so it is opt-in until that is fixed. The field of view used by `--annotate` is an assumption (60 degrees vertical); the labels lined up with the entities in the hospital run.
- **Images** are quantised to a palette PNG when a full-colour one is over 500 KB (`agent-box/bmp.mjs`).
- **Harness against a remote session.** `harness.mjs <verb> ...` relays through `session.mjs run` when the session is on a pool machine (`--local` keeps it here); `session.mjs up --help` prints help.
- **Session.** It reuses a running session, or starts one (`session.mjs up --machine any --profile full --wait`, one client,
  playtest mission) and closes it afterwards; `--keep` leaves a session it started up. If the session is on
  a pool machine, the call runs there and copies the folder back.
- **Reviewer:** reads `contact.jpg` and the `problems:` list, and crops only when unsure.
- **1280 wide png.** Look at the contact sheet before keeping the shots.

## Props on counters, desks and tables

`validate-prop-on-surface` (`tools/validation/`, logic in `tools/actions/prop-on-surface.mjs`) reads every DebugWorld layer and every prefab that places children, takes each mesh's `.xob` header bounds, and fails when a prop resting on a counter, desk, table, workbench or market stand (bottom within 0.5 m of the top, centre within 0.3 m of the footprint) hangs more than 0.1 m past an edge. It tests the plan view only, so a workbench's raised back does not matter. It cannot see a prop whose centre is more than 0.3 m off the surface, vertical placement against a surface with a raised back (a bench top is lower than its bounds), parenting through scripts, or the side a prop faces; look at those in the close-ups. It needs the base-game pak for bounds and prints SKIPPED without it. `prop-on-surface-selftest.mjs` proves the math on fixed bounds.
