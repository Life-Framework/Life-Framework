# M4 launch trailer: shot list (#1292)

Eight shots, about 35 s plus a title and an end card (40 s). The storyboard is
[`tools/film/scenes/m4-trailer/m4-trailer.json`](../../tools/film/scenes/m4-trailer/m4-trailer.json)
(`lf-storyboard/1`, the format in [`docs/film-pipeline.md`](../film-pipeline.md)).
This card plans and sets up only. Trailer and launch media come last and need
the maintainer's sign-off before anything is published; nothing is posted from
here. The finished cut must pass [`trailer/quality-gate.md`](trailer/quality-gate.md).

Dry run, no session needed (parses the board with the film tool's validator and
prints each shot's harness camera command or why it is skipped):

```
node tools/film/scenes/m4-trailer/check.mjs
```

## The eight shots

| Shot | Group | Seconds | Picture | Camera | State |
|---|---|---|---|---|---|
| G01 | village | 5 | Drone rises over the spawn square at golden hour | `drone` (110 3 252) to (118 14 250) | ready |
| G02 | heist | 4 | Low orbit round the cash van while a player drills the box | `orbit` (196 0 252.4), r 4, azimuth 90 | pending: driller actor setup |
| G03 | chase | 4 | Follow camera behind a car on the main street | `follow` (128 0 250), 10 m | ready |
| G04 | ambulance | 4 | Hospital front: ambulance in its bay, medic over a downed player | `orbit` (262 0 252), r 6, azimuth 180 | pending: ambulance prefab and the revive pair |
| G05 | quiz | 4 | First person at the clerk's desk answering a rules question (#915) | `firstPerson` client 1, HUD on | pending: fresh account and `quiz-proof` |
| G06 | moderator | 4 | A moderator with the game-master panel open | `firstPerson` client 2, HUD on | blocked: cards P1 and S9 |
| G07 | recruits | 4 | Police recruits in uniform in front of the station | `static` (236 1.7 240) to (244 1.4 246) | pending: three outfit puppets |
| G08 | crowd | 6 | Slow dusk orbit over the square with a real crowd | `orbit` (125 0 250), r 16, azimuth 270 | blocked: needs 12 or more players, after the playtest |

A `pending` camera is listed and skipped by `film-make`, never guessed. Cards
#1293 (G01 to G04) and #1294 (G05 to G08) delete the `pending` line when its
cause is gone.

## Reuse of M2 clips

No M2 PR clip is committed (takes are git-ignored under `agent-box/reports/film/`),
so "reuse" means the same camera keys and scene as the M2 storyboards, recorded
per shot in `reuse.holdsUp`:

- G01 reuses `video/trailer-571.json` S05 as is (same drone keys).
- G03 reuses the `follow` call of `video/camera-test.json`, which was proven in game.
- G02, G04, G07 replace M2 shots (S01 and S02, S17 and S18, S04) that were never shot.
- G05 keeps the clerk's-desk layout of S06; the quiz screen is new.
- G06 and G08 have nothing to reuse.

## What each shot needs before it is shot

- **Session:** this PC, three clients, `--profile full`, `--sound`, `--priority low`
  (the line in the storyboard's `session`). Client 1 the hero, client 2 the moderator or patient,
  client 3 holds the camera.
- **Look:** 17.9 local, Clear, day frozen, HUD and hints off, except G05 and G06 where the UI is the picture.
- **Fallbacks:** every shot names a `fallback` (a still or a simpler orbit) if its feature is late.
- **Copy:** the title card and end card lines are placeholders for the maintainer to approve.

## Not done here

No footage is shot, no music or voice-over is chosen, nothing is uploaded. The maintainer
signs off the storyboard before #1293 and #1294 shoot, and the cut before anything is published (#1295).
