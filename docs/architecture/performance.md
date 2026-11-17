# Performance

Two budgets live here.

- **Buildings and places** (#2201): what a fitted building may put in the world (lights, shadow
  casters, entities, physics, replication, scripts), with sources, our measurements and how to check.
- **Server scripts at 128 players** (#335): what the LF systems may cost the server per frame.

## Buildings and places (#2201)

The town pipeline began with a provisional budget: 6 dynamic lights with at most 2 shadow casters,
about 150 decorative entities per large building, and no physics or per-frame scripts on decoration.
The maintainer asked for sourced numbers. This section replaces that budget with three parts:
1. what Bohemia's documentation, the engine API and the vanilla game itself say;
2. what we measured on our hardware;
3. the budget derived from both, with a short check for building agents.

"Read" means taken from the cited source. "Measured" means from the runs in part 2. "Inferred" is our
reading, not a documented fact.

### 1. Sourced best practice

Sources used below:

| Key | Source |
|---|---|
| W-SP | BI wiki, [Startup Parameters](https://community.bistudio.com/wiki/Arma_Reforger:Startup_Parameters) |
| W-SC | BI wiki, [Server Config](https://community.bistudio.com/wiki/Arma_Reforger:Server_Config) |
| W-MP | BI wiki, [Model Performance](https://community.bistudio.com/wiki/Arma_Reforger:Model_Performance) |
| W-SPf | BI wiki, [Scripting: Performance](https://community.bistudio.com/wiki/Arma_Reforger:Scripting:_Performance) |
| W-DM | BI wiki, [Diag Menu](https://community.bistudio.com/wiki/Arma_Reforger:Diag_Menu) |
| W-FPS | BI wiki, [FPS Diagnostic Plugin](https://community.bistudio.com/wiki/Arma_Reforger:FPS_Diagnostic_Plugin) |
| W-PR | BI wiki, [Script Profiling](https://community.bistudio.com/wiki/Arma_Reforger:Script_Profiling) |
| E-RO | Enfusion engine docs, "Replication overview" and "Replicating entities, components and hierarchies" (the Workbench docs in the enfusion-mcp wiki dump, `tools\cli call wiki_read`) |
| API | Game script API from the installed paks, build 26266485 (`tools\cli api class LightEntity`; `scripts/Core/generated/Visual/LightFlags.c`, `.../World/EQueryEntitiesFlags.c`, `.../Entities/EntityFlags.c`) |
| V | Vanilla prefabs read with `game_read` (paths below), and the runtime counts in part 2 |

The wiki has no page on lights, light cost or entity budgets; the lighting rows rest on the API, on
vanilla data and on our measurements.

#### Lights

- **How vanilla lights a building (read, V).** Vanilla Everon building prefabs carry no lights:
  `House_Village_E_1I01.et`, `House_Town_E_2I01_Base.et`, `PubVillage_E_1L01.et`,
  `PubVillage_E_1L01_furniture_01.et` and `ShopModern_E_01_Base.et` each have 0 `LightEntity`. The pub's
  furniture uses the unlit meshes (`LightCeiling_01.et`, `LightWall_01.et`). A lit fixture is a separate
  `_on` prefab:
  - `Prefabs/Props/Furniture/LightCeiling_01_on.et` is a mesh with two `LightEntity` children (a 10 m
    ambient fill at LV 0.25 and an 8 m, 150° spot at LV 2) plus the emissive glass
    `LightCeiling_01_Glass_on.emat`.
  - Street lights are spots: `LampStreet_E_01_light.et` (radius 20, 160°, LV 8, with an `RplComponent`).
  - Cheap fills switch shadows off explicitly: `LampIndustrial_01_light_point_exterior.et` sets
    `CastShadow 0`.
- **The shadow default (measured, inferred).** Vanilla lamps leave `CastShadow` unset. In game every
  one of the 55 lights round Saint-Philippe square reports `IsCastShadow()` true (part 2). We infer that
  a light left unset casts shadows, so the budget makes every light state `CastShadow` explicitly.
- **Shadow maps are a fixed pool (inferred, W-DM).** The diag menu's "Show light shadowmap" takes an
  index from 0 to 36, which suggests a fixed pool of light shadow maps. Point lights shadow all round
  (a cube, six faces) and spots one cone; we infer that a point light costs several times a spot. Our
  runs could not resolve that difference (part 2). The budget allows no point-light shadows in buildings.
- **Range and culling (read, API).** `LightEntity.SetIntensityEVClip`: "Lights is clipped when its
  intensity is low"; a negative value culls later. Our fitted shops use `ClipEV -10` (cull late), the
  same as vanilla `_on` lamps. Radius is `SetRadius` / `Radius`. Light prefabs expose no LOD or
  draw-distance field (none of the vanilla light prefabs read sets one).
- **Static and dynamic (read, API).** `LightFlags` has only `CASTSHADOW` and `DIFFUSE_ONLY`.
  `CreateLight`'s comment still says "static light", and `SetLightFlags` warns that a static light
  cannot become dynamic. There is no flag to choose; `IsDynamic()` reports it.
- **Emissive as the cheap alternative (read, V).** Vanilla pairs every lit fixture with an emissive
  material (`LightCeiling_01_Glass_on.emat`, `LampFluorescent_01_cover_lit.emat`, `Glass_on_white.emat`);
  `StreetLampComponent` has an "Emissive Color". An emissive surface glows but lights nothing around
  it. Use emissive for any fixture that only has to look on, and a real light only where the room needs
  the light.

#### Entities, LODs and view distance

- **LODs (read, W-MP).** Create LODs (LOD0, LOD1 and the last by hand). "If you import a heavily
  detailed 3D model as LOD0 without retouching it/creating smaller LODs, the game will keep this model in
  memory for as long as it will be in game's world." Vanilla reference: a structure such as
  `Barracks_01.xob` is 13,455 vertices at LOD0 and 194 at its last LOD.
- **Instancing (read, W-MP).** "Repetitive complex elements must be split to save on memory, allowing
  then to assemble them in Prefabs" (the section is marked WIP). Inferred: reusing one prefab many
  times shares the mesh in memory, so many copies of a few props are cheaper than many unique props.
- **View distance (read, W-SC).** `serverMaxViewDistance` defaults to 1600 m (range 500 to 10000);
  `networkViewDistance` defaults to 1500 m and is the "maximum network streaming range of replicated
  entities".
- **Density (measured, V).** Saint-Philippe square holds 3,720 entities within 60 m and 17,070 within
  150 m, about 93 % of them static (part 2). That is the density the engine already handles there.
- **Proxy children (read, API).** `EntityFlags.PROXY`: "Slave child entity, that is linked into scene
  tree just by its parent. Usually some small props/parts of more complex entity". Building furniture
  hangs under the building, the vanilla way.

#### Static, dynamic and physics

- **Vanilla decoration is static (measured, V).** Saint-Philippe within 150 m has 16,745 physics bodies
  and **0 dynamic** ones. Our gun store (10 m) has 49 dynamic bodies, the playtest town (200 m) 893.
- **Server cost of static props is small (measured).** 4,800 extra static boxes on the server cost
  +0.38 ms a frame and about +3 MB (part 2).
- **Inferred.** A dynamic body can wake, simulate and replicate its position. Decoration has no reason
  to, so decoration is static (or has no collider), and only items a player picks up are dynamic.

#### Streaming and occlusion in buildings

- **Replication streaming (read, W-SP, E-RO).** A whole node hierarchy streams in or out together.
  `-streamingBudget` is charged "the replicated hierarchy size of each entity that needs to be streamed
  in". `-staggeringBudget` and `-streamsDelta` limit how much the server opens a tick. Too high
  "significant performance drops on spawning/teleporting", too low "pop-in". Inferred: a building whose
  decoration carries an `RplComponent` makes every client's stream-in of that building bigger.
- **Occlusion (read, V; inferred).** Vanilla Eden's world file contains the class `WorldOccluderEntity`
  and the property `m_fMaxDrawDistance` (its binary string table). So vanilla places occluder volumes
  and per-entity draw distances. How Eden uses them is not documented. We do not place occluders yet.
  Our measurements had the camera looking into the scene, the worst case.

#### Replicated and local entities

- **Read, E-RO.** `RplComponent` inserts only "entities or components whose replication layout is not
  empty (contains at least one replicated property, RPC, or replication callback)". Everything else
  "will be ignored by replication". The server is the only authority and decides what each client
  knows.
- **Read, W-SP.** `-logStats` prints `RplItemsS` (dynamic streams on the server) and `RplItemsC0`
  (streams open to a client). Watch them on a place with many interactables.
- **Measured.** Saint-Philippe within 60 m has 1,520 entities with an `RplComponent` (41 %: doors,
  destructibles, lamps); our gun store has 121 of 348 (its stock and its doors).
- **Rule.** Decoration never carries an `RplComponent` and is never spawned at runtime by the server. A
  thing that changes state (a door, a till, stock) is replicated; a prop that never changes is in the
  layer on both sides, the vanilla way.

#### Scripted components and frame callbacks

- **Read, W-SPf.** The page lists "OnEachFrame calculations - are they needed?" as a specific issue.
  The fixes: run it less often ("every X seconds, result being delayed and cached"), spread big loops
  over frames, stop early, and centralise per-instance work in one manager.
- **Read, W-PR.** Diag Menu > Statistics > Script Profiler (or `-scriptExtProfiler`) profiles script
  time; `-checkInstance` with `DumpInstances()` dumps allocations.
- **Measured.** Saint-Philippe within 150 m has **0** entities with `EntityEvent.FRAME`; the playtest
  town (200 m) has 3. A building adds none: a timed behaviour uses one `CallLater` in a manager, as
  `LF_ServerPerfLog` does.

#### AI

- **Read, W-SP and W-SC.** `-aiLimit` / `aiLimit` caps AI ("No systems will be able to spawn any AIs
  when this ceiling is reached"), `-aiPartialSim` (default 4) splits simulated AI into batches a frame,
  and `disableAI` turns the AI world off. AI LOD exists: the page's example has "100 in LOD0, 50 in LOD
  10".
- **Not measured.** Our runs had 0 AI (`AI: 0` in `-logStats`). Shopkeepers and clerks are
  `LF_NpcBaseEntity`. A place's AI budget belongs to the NPC work, not this page; a building adds no AI
  of its own.

### 2. Our measurements (2026-11-17)

#### Method

- **Machine.** lf-testbox, the pool laptop: Ryzen 9 6900HS with CPU boost off, RTX 3070 Ti, 15.2 GB of
  RAM, game build 26257316. Client and server ran on the same machine, one client at 1280x720,
  `--profile full`.
- **Levers.** `--levers uncapped,slog,suncapped` (`agent-box/session-profile.mjs`):
  - `uncapped`: the client's `-maxFPS 500` with Vsync off. Without the Vsync change the laptop client
    sat at exactly 60 fps (16.67 ms) whatever the scene.
  - `slog`: the server's `-logStats 2000` (W-SP).
  - `suncapped`: the server's `-maxFPS 500`, so its frame time is work, not the 30 fps wait.
- **Driver.** `node agent-box/session.mjs perf-stress <everon|playtest> ...`
  (`agent-box/proofs/perf-stress.mjs`) and the harness verb `perfstress`
  (`Feature/AgentHarness/LF_AgentHarness_PerfStress.c`):
  - `count` walks a sphere's entity hierarchies;
  - `spawn L S K` builds a local stress room: L ceiling lights over 10 x 10 m, 3 m up, the first S
    casting shadows, and K cardboard boxes on the floor;
  - `server K` puts K boxes on the server only.
- **One row.** Night (22:00), HUD off, a fixed camera looking into the scene, 5 s to settle, then the
  existing `perf 10` window (`World.GetTimeSlice` every frame, mean and p95). The same window records
  client and server process memory (`Get-Process`) and the averaged server `-logStats` lines.
- **Repeat.** Boot a session with those levers, then run:
  - `perf-stress everon`;
  - `perf-stress playtest --shop Village_GunStore shop lights shadows points props server`;
  - the same with `--paired` (an empty-scene reference before each row).

#### Counts (`perfstress count`, client world)

| Place | Radius | Entities | Static | Lights | Lights on | Shadow casters | Physics (dynamic) | Replicated | Per-frame |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| Saint-Philippe square (CoreOnEveron) | 30 m | 1,271 | 1,163 | 5 | 5 | 5 | 1,242 (0) | 530 | 0 |
| Saint-Philippe square | 60 m | 3,720 | 3,435 | 12 | 12 | 12 | 3,634 (0) | 1,520 | 0 |
| Saint-Philippe square | 150 m | 17,070 | 15,880 | 55 | 55 | 55 | 16,745 (0) | 6,544 | 0 |
| Playtest gun store (`Village_GunStore`) | 10 m | 348 | 250 | 4 | 4 | 2 | 309 (49) | 121 | 0 |
| Playtest town | 200 m | 5,573 | 3,687 | 322 | 24 | 133 | 4,589 (893) | 2,732 | 3 |

What the counts show:
- The playtest town has 6 times vanilla's light count with a third of its entities. Most of its lights
  are off (24 of 322 on), and 133 are flagged to cast shadows.
- The runtime walk undercounts lights inside prefab hierarchies. The gun store's prefabs declare 14
  lights (`perf building GunStore`) and the walk found 4. Read light counts from `perf building`, and
  entity, physics and replication counts from `count`.

#### Frame time and memory

The pool laptop's noise is the first finding:
- An empty reference row drifts from 6.5 ms to 13 ms over a 5-minute run, in step with its thermal
  throttling (CPU boost off, a 500 fps server sharing the CPU).
- Paired rows (reference, then configuration) differ by up to about ±3 ms with nothing changed.
- So only differences larger than about 3 ms inside one run mean anything here.

| Scene (night unless noted) | Client mean | p95 | fps | Client private | Server frame (avg) | Server memory (`-logStats` / private) |
|---|---:|---:|---:|---:|---:|---:|
| Saint-Philippe square, vanilla Everon | 22.2 ms | 29.5 | 45 | 5,332 MB | 2.00 ms | 961 MB / 2,501 MB |
| Saint-Philippe square, noon | 17.1 ms | 22.0 | 59 | 5,338 MB | 2.00 ms | 961 MB |
| Playtest gun store, inside | 12.5 ms | 15.3 | 80 | 2,898 MB | 2.12 ms | 56 MB |
| Playtest gun store, from the street | 10.8 ms | 12.7 | 93 | 2,898 MB | 2.00 ms | 56 MB |

Stress room. Each delta is against the row before it, or against the paired reference:

| Load in one 10 x 10 m room, in view | Client delta | Server delta | Reading |
|---|---:|---:|---|
| 2 to 128 spot lights, no shadows (paired) | -2.1 to +2.9 ms, no trend | 0 | not measurable up to 128 |
| 8 spots, 1 / 2 / 4 / 8 casting shadows (paired) | +0.4 / +2.0 / +0.7 / -0.8 ms | 0 | within noise |
| 16 spots, all 16 casting shadows (paired) | +0.6 ms | 0 | within noise |
| 8 points, 1 / 2 / 4 / 8 casting shadows (paired) | +3.5 / +0.9 / -2.0 / +0.2 ms | 0 | within noise. An unpaired run read +7 ms at 4 |
| 150 / 300 / 600 boxes | +0.5 / +0.5 / +0.7 ms | 0 | not measurable |
| 1,200 / 2,400 boxes | +4.1 / +10.3 ms against the empty room (unpaired; the run's later empty rows drifted about 4 ms) | 0 | the first clear rise |
| 600 / 2,400 / 4,800 boxes on the server only | n/a | +0.02 / +0.08 / +0.38 ms, +3 MB at 4,800 | small |
| Lights, shadows, props: client memory | about +0 MB | | not measurable |

What the numbers say:
- **The server does not care about decoration.** An idle server runs a frame in 2.0 ms with the
  playtest town loaded, and 4,800 extra static props add 0.4 ms. Buildings cost the client, and they
  cost every client.
- **The vanilla island is the heavy part.** Saint-Philippe at night is 22 ms on this laptop against
  11 to 13 ms in the playtest town. The client holds 5.3 GB against 2.9 GB, and the server 2.5 GB
  against well under 1 GB. A fitted building adds to a scene that already uses most of a mid-range
  machine's frame.
- **Lights did not show up at counts far above any budget.** Neither unshadowed spots (128 in one room)
  nor shadow casters (16 in one room) gave a clear cost here. Two cautions:
  - The laptop's ±3 ms noise hides anything smaller.
  - Not done: a screenshot confirming the spawned lights were drawn in view. The pool ran short of RAM,
    so the `shot` phase of `perf-stress` was not run. Run it before anyone leans on the light rows.
  The light budget below therefore rests on vanilla practice (part 1) more than on these rows.
- **Entities in view are the first clear cost:** +4 ms at 1,200 extra boxes and +10 ms at 2,400 in one
  view, part of it drift.

Not measured: a slower GPU, many players in one place, AI, streaming-in hitches (the server's
`-logStats` showed 180 to 720 ms single-frame spikes while props were spawned, which is spawn cost),
and a second client's view.

### 3. The budget

Per building, by size:
- **Small:** a house, a kiosk, one or two rooms.
- **Medium:** a shop or a pub, up to about 150 m² of floor.
- **Large:** a bank, the town hall, the police station.

The counts are from `tools\cli perf building <slug> --size <size>` (`BUDGET` in
`tools/perf/building-count.mjs`; keep the two in step).

| Per building | Small | Medium | Large | Why |
|---|---:|---:|---:|---|
| Decorative entities (the building and its children, an upper bound) | 150 | 300 | 500 | At these counts nothing was measurable (600 boxes in one view: +0.7 ms), and the first clear rise was at 1,200 in one view. Several of these buildings in one view stay under that. The playtest gun store sits at 293 (medium) |
| Lights (`LightEntity`, every kind) | 3 | 6 | 10 | Vanilla buildings have 0, and Saint-Philippe has 12 lights in 60 m in all. Unshadowed lights measured no cost, so this is a look and maintenance limit: light a room with one or two lights and make the fixtures emissive |
| Shadow casters (`CastShadow 1`, or unset) | 0 | 1 | 2 | Vanilla density is 12 casters within 60 m, all street and exterior lamps. The diag menu suggests a fixed pool of light shadow maps, and our noise cannot rule out +3 to 7 ms for a few casters |
| Point lights casting shadows | 0 | 0 | 0 | Six faces each (inferred); use a spot for the one shadowed light |
| `CastShadow` left unset | 0 | 0 | 0 | Unset behaves as on (vanilla lights all report shadows); write `CastShadow 0` on every fill light |
| Dynamic physics bodies on decoration | 0 | 0 | 0 | Vanilla Saint-Philippe has 0 within 150 m. Only items a player picks up (stock, loot) are dynamic |
| Replicated decoration (`RplComponent` on a prop that never changes) | 0 | 0 | 0 | A replicated hierarchy is charged against the streaming budget (W-SP). State lives on the few things that change |
| Per-frame scripts (`EntityEvent.FRAME`, `EOnFrame`) | 0 | 0 | 0 | Vanilla has 0 on the square. Use one manager with a `CallLater` (W-SPf) |

Per place (a slot set within 60 m, the square plus its streets):

| Per place | Limit | Why |
|---|---:|---|
| Lights switched on at night | 24 | 2 times vanilla's 12 in 60 m, for lit shop interiors vanilla does not have |
| Shadow casters | 12 | Vanilla's own density round the square (12 in 60 m) |
| Entities added over the vanilla map | 1,500 | About +40 % on the square's 3,720. A view across the square sees several buildings, and the clear rise came at 1,200 extra in one view |
| Interactables with replication | as many as the gameplay needs | Each is a stream; watch `RplItemsC0` in `-logStats` |

**What this means for today's shops.** `perf building` on the five fitted playtest shops (medium):

| Shop | Entities | Lights | Shadow casters (all unset) | Point lights casting |
|---|---:|---:|---:|---:|
| GunStore | 293 | 14 | 14 | 9 |
| GeneralStore | 845 (289 since #2327) | 11 (6) | 11 (1) | 4 (0) |
| ClothingStore | 258 | 11 | 11 | 11 |
| Drugstore | 468 | 12 | 12 | 2 |
| Supermarket | 133 | 16 | 16 | 4 |

All five are over on lights and shadow casters, and the general store and drugstore on entities. The
fix is mostly one property per light:
- write `CastShadow 0` on every fill light;
- keep one shadowed spot per shop over its counter;
- turn decorative fixtures into emissive meshes;
- thin the general store's and drugstore's dressing.

Fitting work after this page meets the budget. Moving the five shops to it is a follow-up issue, not
part of this change.

### How to check (building agents)

1. **Static, every fitted building.** `tools\cli perf building <Slug> --size small|medium|large` counts
   entities, lights, shadow casters (unset counts as a caster), point-light shadows and unset
   `CastShadow` from the prefab tree, vanilla prefabs included. It exits 1 and prints an `OVER` line for
   each limit passed; `--json` gives the counts for `town check` (#2206).
2. **In a session, for what files cannot show.** Run `perfstress count <LayerEntityName> <radius>` (or
   `perfstress count <x> <y> <z> <radius>`) through the harness. It gives:
   - dynamic physics bodies;
   - replicated entities;
   - per-frame entities;
   - the place totals for the per-place limits.
3. **Only when a place feels slow.** Use `perf-stress` with the measuring levers and compare the place
   against the vanilla square row in part 2, inside one run.

## Server scripts: 128 players (#335)

The target (decision 2026-11-08): a dedicated server holds 128 players. This page is the
budget, how it is measured, the numbers so far, and what a synthetic run cannot tell us.

### Tools

| What | Where |
|---|---|
| Counters and timers behind a flag | `LF_Perf` (`Scripts/Game/Core/LF_Perf.c`). Off until the server starts with `-lfPerf` (or a test calls `LF_Perf.SetEnabled(true)`). With the flag on, `[LFDebug:Perf] count/time/gauge ...` lines print every 60 s once a player connects |
| Budget thresholds | `Configs/Perf/LF_PerfBudget.conf` (class `LF_PerfBudget`). `validate-perf-budget.mjs` keeps conf and class in step and the player count at 128 or more |
| Benchmark and gate | test `perf/budget-128` (`LF_Test_PerfBudget`, PERF tier): builds 128 synthetic characters through the real managers, bus and serializer, fails when a cost passes its limit |
| Report | `tools\cli perf [--local]` runs the test, prints the table, writes `tmp/perf/report.{md,json}`, exits 1 over budget. It runs the PERF tier (below) |
| PERF tier | `// tier: PERF` tests (LF_TestTier.PERF) are timing gates. `--tier fast` and `--tier all` skip them (`[LFTEST] SKIP ... tier=perf`), so `wt gate` / `wt ship` do not run the budget and cannot flake on it. `tools\cli test --no-build --tier perf` runs them alone: the pool's RAM gate as for any run, one server, never sharded, never cached (`test-cache.mjs` skips the tier), and `test-e2e.ps1` first waits up to `LF_PERF_IDLE_WAIT_S` (60) s for the CPU to sit at or under `LF_PERF_IDLE_PCT` (20) % for three samples, warning and measuring anyway if it never does. Under `LF_TestManager.PerfRun()` the test takes 15 reference runs and 15 repeats, 6 windows per time measure, reads the cheapest cost and the cheapest reference op over all windows, and holds every limit to 70 % of the conf value. `--only LF_Test_PerfBudget` still runs it at the lenient shared-suite settings. Not done: the pool does not yet reserve a whole machine for a perf run, so a busy neighbour lane is covered only by the idle check |

Instrumented today: `Account.refreshAll` (the 10 s pass over every player), `Account.publish`,
`Account.ownerScan` (walks over the players to find an account's owner: at most one a frame, #716)
and `Replication.accountState` (bumps), `Ledger.record`, `Medic.bleedAdvance` and `Medic.downed`,
`Persistence.savePoint` (request to commit). Add a site with `LF_Perf.Begin()` / `End("Sys.name", t)`
or `LF_Perf.Count("Replication.x")`; keys are `<System>.<name>`. Timers are whole milliseconds,
so read `calls` and `maxMs` beside `totalMs`; the benchmark times batches and divides.

### Budget

Per-player targets (proposed; the issue left the numbers open, so these are the working budget
until a real run says otherwise):

| Cost | Target at 128 players |
|---|---|
| Server script time for all LF systems | under 4 ms per 33 ms frame averaged over a minute (about 12 percent of one frame at 30 fps) |
| Replicated props per player | a handful of ints; a prop is bumped only when its value changes, never per frame |
| Replication bumps | under 0.5 per player per second in steady play (64/s server-wide) |
| Money or crime event through the bus | under 200 microseconds, so 128 players doing 2 a minute cost under 1 ms per second |
| Save write and read of the LF payloads | under 300 ms and 400 ms (in the test's JSON form) |
| Save size per character, LF payloads only | under 1000 bytes with the shared logs empty; whole save under 6 MB with the logs at their caps |

The enforced limits are the thresholds in `LF_PerfBudget.conf`; the frame, bump and prop rows
are targets for the multi-client run below, not yet gated.

### Measured

Local box, test server, 128 synthetic characters (1 account, 1 character, bank balance, 3 crimes,
40 money events each), 2026-11-11. Run `tools\cli perf` for the current table.

| Measure | Value | Budget |
|---|---:|---:|
| Crime event through the bus (record + incident log) | 15-20 us | 100 us |
| Money event, ledger below cap (ledger + incident log) | 32-39 us | 200 us |
| Money event, ledger at its 20000-row cap | 36-45 us | 150 us |
| One ledger query per character, 128 queries over 20000 rows | 25 ms (0.2 ms each) | 150 ms |
| Export 128 accounts | under 1 ms | 20 ms |
| Store write (`SerializeStore`, JSON) | 29-31 ms | 300 ms |
| Store read (`DeserializeStore`, JSON) | 46-50 ms | 400 ms |
| Save bytes per character, logs empty | 436 | 1000 |
| Whole save, ledger 20000 rows + 2000 incidents | 3.9 MB | 6 MB |

Findings from the run:

- **The ledger is the save.** At its cap it is about 3.8 of the 3.9 MB (about 190 bytes a row as
  JSON); the 128 characters themselves are 56 KB. Retention (`Configs/Money/LF_Ledger.conf`, #330)
  sets the save size, not the player count. Binary saves are smaller, by how much is unmeasured.
- A ledger row costs the same at the cap as below it, so the front-of-array drop in `Prune` is
  not a problem at 20000 rows.
- `LF_Ledger.Last` and `RebuildBalance` scan the whole log. 0.2 ms a query is fine; it grows
  linearly, so a retention of 200000 rows would cost 2 ms a query. Index by character before
  raising the cap.
- `LF_AccountReplication.Publish` walks every player per account change (it matches the uid to a
  connected player), then walks them again to count police. That is O(players) per change; at 128
  it is cheap, but a mass change (everyone's wanted level at once) is O(players squared). No fix
  is filed: it needs live players to measure.

### What extrapolates to 128, what does not

Extrapolates (script cost with no engine or network in it): bus fan-out, ledger and incident
append, account and record export, store write and read of the LF payloads. They are linear in
players or rows, and the run is already at 128 characters and the retention caps, so these
numbers are the 128 numbers on this machine. The benchmark gate expresses its limits in reference ops (one turn of a small script
loop timed in the same run), so a slower or busy machine moves the limit with it; measured idle ratios
on this PC, lf-testbox and lf-cloud agree within about 1.5x (the at-cap money event, which moves
memory, within 3x), and each limit sits at 2.2 to 2.7x the typical ratio.

Cannot be known without a real load test:

- **Network.** Replication bandwidth, RPC rate, `Replication.BumpMe` fan-out to 127 observers,
  JIP state size. Synthetic characters here have no connection.
- **Real characters.** Inventories, vehicles, world entities, animation and physics. The engine's
  own persistence of those entities is not in the LF payloads this test writes, and it is likely
  most of a real save.
- **Per-frame ticks over live players.** `LF_Perf` times them, but a tick that walks players needs
  players. The counters exist so the multi-client run can read them.
- **Binary save size and the async save hitch** on a dedicated server (the test writes JSON in one call).
- **Client cost** (UI fan-out, toasts, the wanted board).

### The multi-client run (needs people)

The issue's second acceptance item, one measured multi-client run, needs humans or headless
clients. Run it on a dedicated server started with `-lfPerf`:

1. Start the server with the counters on: `LF_PERF=1 node agent-box/session.mjs up --wait --machine local --mission playtest`
   (`LF_PERF=1` adds `-lfPerf` to the server), or add `-lfPerf` to a real dedicated server's command line.
2. Join with as many clients as the pool allows (a real 128 needs a community test night).
   Everyone plays normally for 15 minutes: shops, ATM, a few crimes, an arrest, a revive.
3. Read the server `console.log` with `tools\cli logs "LFDebug:Perf"`. Each minute prints every
   counter. Check `time Account.refreshAll` (maxMs), `count Replication.accountState` per minute
   divided by players, `time Persistence.savePoint` (maxMs, the save hitch), and the server frame
   time from the engine's own diag overlay.
4. Paste the numbers under "Measured" with the player count and the hardware.

Until then the budget gate covers what a single process can show, and the table above is the
single-process answer.
