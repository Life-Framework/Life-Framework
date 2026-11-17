# Starter pack: map card (the Everon map epic)

Cards M0 to M19 are in `docs/architecture/everon-map.md` section 8; each owns its files outright. New code is in
`Feature/Map/` (core). POI data comes from `LF_Places`; the pack holds the 1989 icons and overlays, never the core.

## Start
```
tools\cli slice brief <N>
tools\cli wt new slice-<N> --wait ; gh issue edit <N> --add-label agent:working
```
Read your card in everon-map.md and the `Feature.md` of the area it touches. Not the whole spike.

## Order of work: emulator, LOGIC tests, one in-game shot
1. **Emulator first.** Render the map layout offline: `tools\cli map render <fixture> [--zoom z]` (card M0; until it
   merges `tools\cli map` says "unknown command", so check `tools\cli help` and do not write your own renderer). Iterate
   on cluster, label and hatch rules here; it needs no Workbench and no session.
2. **LOGIC tests** hold the rules (clusters by zoom, visibility, area outlines, waypoint, shares, known-state):
   ```
   tools\cli test --no-build --only LF_Test_MapPois            # the loop
   tools\cli test --no-build --red LF_Test_MapPois,LF_Test_MapAreas
   LF_RDB_REFRESH=0 ...                                         # script-only runs ($env:LF_RDB_REFRESH = "0")
   ```
3. **One in-game `map-shots` at the end**, not per tweak (full profile, 5 GB, queued):
   ```
   node agent-box/session.mjs up --mission everon --profile full
   node agent-box/session.mjs map-shots
   node agent-box/session.mjs down
   ```
   Prepare every edit before `up`; `down` straight after. Keep the PNGs with `tools\cli evidence <N> after <png>`.
4. Ship as in `tools\cli brief slice` (`wt ship`, `slice record`, final line).

## Known traps
- The harness `input` verb refuses `GadgetMap` on both sides. `hold PaperMap_01_folded` opens the map; pan with
  `SCR_MapEntity.ZoomPanSmooth` (card M0 notes).
- `GameMode_Roleplay.et` does not inherit `GameMode_Base.et`: the map needs `SCR_MapConfigComponent` and
  `SCR_MapMarkerManagerComponent` on it (card M1). Check inherited component GUIDs against the base; never add a second
  component of the same class (it shadows).
- Verify a map API with `tools\cli api class <Name>` before use; never guess it.
- Core text is neutral: place names and flavour are pack strings. Core script never holds a world coordinate.
- A test fixture names fake places; a real Everon name in core fails `validate-core-place-names`.
- A new string is appended, then `tools\cli regen-localization`; a new test needs `tools\cli regen-tests`.

## Environment
No Workbench on this PC and never pin a machine. Run in the foreground (timeout up to 600000 ms) or
`t3_background_wait`; never end the turn with a run pending. Never `--force`, `rm -rf` or `Remove-Item -Recurse` a
worktree (`wt prune`).
