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

1. **Before shots.** Take the wide and detail shots described below, as they are now.
2. **Fix placement.** For every entity in the area: ground snap and surface height,
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
6. **After shots,** then hand the area to the reviewer.
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

## Shot rules

- **Golden hour, clear weather, the same time for every shot:** set the in-game time and
  weather before shooting. Use the harness or a debug define, never a script change
  that ships.
- **How, on a session:** the harness `shot world` pins its own clock on the client only, and the server
  wins. For golden hour, set `"Initial Day Time" 13.2` on the `TimeAndWeatherManagerEntity` in
  `default.layer` before `session.mjs up`, and revert it before committing (the world is at
  latitude -4, longitude 71, so the game clock reads about 4.7 h behind local time). Shots hide the HUD
  themselves; wait 15 s after signing form R-89 so the quest toast is gone. `session.mjs up --machine any`
  may land on the test box: drive it with `session.mjs run <script>` (a script inside the repo when
  `up` ran) and fetch captures with `session.mjs pull`. The BMPs are over 500 KB as plain PNGs;
  quantise to 128 colours.
- **Two shots per area:**
  - one wide establishing shot that shows the whole area in context;
  - one detail shot of the main interaction or prop.
- **No HUD, debug text or markers,** unless the UI is the subject.
- **1280 wide, under 500 KB, png.** Look at each shot before keeping it.
