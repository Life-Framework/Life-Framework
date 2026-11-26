# Night-light and shelf-goods perf audit, 2026-11-27

The 11-25/11-26 night work added dynamic lights (fascia downlights #4967, six street lamps #4968, street-lamp and
interior fills #4970 to #4977, outpost lanterns #4980) and 263 shelf-goods entities (#4918 to #4951). The 11-26
round saw three 1080p clients at 1 to 6 fps late in a session on one laptop that also ran the server. This audit asks
whether our content regressed real performance, or whether that was the box.

**Verdict: no regression found, and the clean paired measurement could not be taken.** The static counts keep every
new light and shelf entity inside the budget in `docs/architecture/performance.md`. The one set of client numbers
taken before the box saturated shows a night cost no bigger than vanilla's. Every later run was pinned at the
100 ms frame floor with all lights switched off, which means the box, not the lights, set the frame time. A second
finding is unrelated to fps: `perf/budget-128` fails on save size (below).

## What was measured

Machine: lf-testbox (PC1), game build 26266485, `--profile full`, one client, `--levers uncapped,slog,suncapped`,
`--mission island` (the pack world). Baseline for the vanilla square is `docs/architecture/performance.md` part 2
(2026-11-17, same machine, vanilla Everon, 1280x720).

Client frame time, first run (one 10 s `perf` window per row, 6 s settle; the box was already shared):

| Spot | Noon | Night 22:00 | Night minus noon | Lights in 30 m (on at night) | Shadow casters in 30 m | Reading |
|---|---:|---:|---:|---|---:|---|
| Square (vanilla, 11-17 baseline) | 17.1 ms, 59 fps | 22.2 ms, 45 fps | +5.1 ms | 5 | 5 | the reference |
| Square (island, market board) | 20.4 ms, 49 fps | 23.6 ms, 42 fps | +3.2 ms | 43 (4 on) | 12 | night costs less than vanilla's |
| Supermarket | 15.0 ms, 67 fps | 16.1 ms, 62 fps | +1.0 ms | 9 (5 on, then 9) | 5 | no cost; a later window read 14.8 ms, 67 fps at night |
| Gun store | 19.0 ms, 53 fps | not valid (99 ms, hitch) | n/a | 29 (2 to 5 on) | 13 | noon only |
| Dock | not valid (98 ms) | not valid (97 ms) | n/a | 5 (up to 5 on) | 4 | box-bound |
| North outpost | not valid (92 ms, first teleport) | 25.0 ms, 40 fps (p95 62) | n/a | 8 (3 on) | 3 | streaming hitches; noon n/a |

"Not valid" rows are windows whose mean sits at 91 to 99 ms with p95 at the 100 ms clamp. The same value came back
at noon and at night and with every light switched off, so it is the shared box, and the row says nothing about
the content.

Server frame time (`-logStats`, `suncapped`), one client: 3.0 to 4.6 ms in every window, against 2.0 ms on 11-17.
That baseline was the idle playtest town with no pack and no other load; the island run is the full pack world on
a CPU at 70 to 95 %. A two-client server number was not taken (the one-client rows never became clean).

Before/after of any fix: none, because no fix was made. Nothing measurable pointed at a light or a shelf.

## Why the clean run failed

At the time of the runs the pool held three to four full sessions beside three Workbench `look` jobs on this one
machine: CPU 70 to 95 %, GPU 100 %, VRAM 5.3 to 8.0 of 8.0 GB. Three later attempts (a full-profile two-window run,
a paired lights-on/shadows-off/lights-off run, and a 640x360 `lowspec` rerun, plus one that waited up to 5 minutes
for a quiet box) all read 100 ms on every window, lights off included. That is the 11-26 symptom reproduced
without our content being at fault: the 1 to 6 fps was most likely the laptop running the server and three clients.

## Static counts (independent of box load)

- Every light prefab the night work added writes `CastShadow 0`: `LF_LightCeiling_Interior` (51 placed),
  `LF_LightInteriorFill` (23), `LF_CampFlame`, `LF_LampHeadNight`, `LF_LanternGlow`, `LF_WindowGlow`.
  The performance page measured unshadowed lights at no cost up to 128 in one room.
- The only new shadow casters are six vanilla `LampStreet_E_01_4m` placements from #4968 (vanilla lamps leave
  `CastShadow` unset) and one deliberately shadowed spot in each of three shop furniture prefabs
  (`GunStore_furniture_01`, `GeneralStore_furniture_01`, the drugstore), which is the budget's "one shadowed spot per shop".
- Shadow casters within 30 m: 12 at the square, 13 at the gun store. The per-place limit is 12 within 60 m, so the
  gun store block is at the limit and the square sits on it. If a later measurement on a quiet box shows a cost,
  the cheapest cut is `CastShadow 0` on the six placed street lamps (an override prefab over `LampStreet_E_01_4m`);
  it was not done on a hunch.
- Shelf goods are mesh-only entities (`LF_ShelfGoods_*.et`: one `MeshObject`, no collider, no `RplComponent`, no
  script): 132 entities in the gun store layer, 96 in the hardware layer, 49 in the general store layer, all below
  the 300-entity medium-building budget and the 600-in-view mark where the performance page found +0.7 ms.

## Side finding: `perf/budget-128` fails

`tools\cli test --no-build --tier perf` on this branch: 3 of 4 pass, `perf/budget-128` fails on
`perf save-bytes-per-player 1058 bytes/player over budget 1000`. The budget's own note puts 436 bytes per character
on 11-11, so the save sections added since carry about 600 bytes per player. This is a persistence size, not a frame
cost, and it is not touched here; it needs an owner who knows which payload grew (the log is
`tmp/logs/test-20261126-211258.log`).

## How to repeat it on a quiet box

```
node agent-box/session.mjs up --wait --clients 1 --profile full --levers uncapped,slog,suncapped --mission island
node agent-box/session.mjs perf-night-audit square supermarket gunstore dock outpost --hours 12,22
node agent-box/session.mjs down
```

The proof prints, per spot and hour, the median frame time with all lights on, with shadows off and with lights off.
A difference larger than about 3 ms between neighbouring windows is the lights; a row where all three read 100 ms
is a loaded box and is thrown away. `--clients 2` shows the server with both clients. Run it when `tools\cli pool
status` shows no other session and no Workbench job.
