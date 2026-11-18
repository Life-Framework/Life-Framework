# Everon island: server RAM and frame rate (D17, #2934)

Measured 2026-11-18 on PC1 for the 0grind cutover (#2212). The question: does the 16 GB 0grind plan hold
the island with 16 players?

**Verdict: PASS.** The 16-player estimate is about 2.9 GB of server private memory, under the 12 GB line
with about 9 GB to spare. Nothing in the plan needs to change for RAM. One real problem showed up instead:
every autosave freezes the island server for 2.8 to 6 s (see "Autosave hitch").

## Method

- **Session.** `node agent-box/session.mjs up --mission island --clients 2 --profile lowspec --levers
  res,fps,tex,geom,post,shadow,view,slog`: the hidden island header `LF_EveronIsland.conf` (MainWorld),
  a dedicated server at its default `-maxFPS 30` (what 0grind runs), `-logStats 2000`, two clients at
  640x360. Server and clients ran on the same machine (PC1). Game build of 2026-11-18, main at `7f0f35e9`.
- **Players.** Both clients passed the quiz and registration and spawned at the island spawn on the south
  road (4472, 10536), about 245 m south of the Saint-Philippe square, and stood there for 14 minutes.
- **Readings.** Every minute: server and client process memory (`Get-Process`: private bytes and
  working set) and the averaged server `-logStats` lines since the last reading (FPS, frame time, the
  engine's own `Mem`). The script is the one-off `tmp/ram-2934.mjs` of the slice worktree; its loop is
  `perf-stress`'s `measure` without the camera.
- **Counts.** `perfstress count` on client 1 around the square (4450, 10780) at 60 m and 150 m.
- **Host tools.** `tools\cli host remote 0grind status` reports the service as running (node eu-de-13,
  0/16) but no memory figure: the 0grind API has no memory reading, so the live figure comes from the
  0grind panel after the cutover. `tools\cli host perf` samples a local host's own server only.

## Readings

| When | Server private | Server working set | `-logStats` Mem | Server FPS (avg frame) | Players / AI |
|---|---:|---:|---:|---:|---|
| Boot (`session up` done, 2 clients joined, not spawned) | 2,544 MB | 393 MB | 962 MB | 27.5 (36 ms, loading) | 0 / 0 |
| 2 clients spawned | 2,561 MB | 509 MB | 964 MB | 27.8 (37 ms) | 2 / 2 |
| Minute 1 to 10, 2 clients | 2,561 to 2,586 MB | 446 to 930 MB | 966 to 985 MB | 29.2 to 30.0 (33.3 ms) | 2 / 2 |
| Minute 13 (end) | 2,584 MB | 927 MB | 1,008 MB | 30.0 (33.3 ms) | 2 / 2 |

- The server holds its 30 fps cap: the median frame is 33.3 ms in every window. The work inside a frame is
  well under the cap (an uncapped idle island server ran 2.0 ms a frame, `docs/architecture/performance.md`
  section 2).
- Private memory is flat after spawn: +40 MB from boot to the end, +22 MB per player on the engine's own
  `Mem` (962 to 1,008 MB). The working set swings between 450 and 930 MB because Windows trims it; private
  bytes is the figure that counts against the plan.
- `AI: 2` is the two player characters' controllers; no NPC was running.
- **Clients** (lowspec): 3.07 GB private each in the menus, 3.97 GB once spawned. `--profile full` measured
  5.3 GB on the island (performance.md section 2), so the 5.3 GB player warning in the weekend plan stands.

## Autosave hitch

The server's longest frames, all on the island with 2 players:

| Time | Longest frame | What ran |
|---|---:|---|
| first autosave | 5,985 ms | `[LFDebug:Persistence] save completed (AUTO)` |
| next autosaves (5 min apart in the session) | 2,871 ms, 2,810 ms | the same |
| player 1 joins, quiz, first spawn | 1,214 to 1,582 ms | spawn and account setup (once per join) |

Every autosave stalls the whole server for 3 to 6 s: on 0grind (`autosaveInterval` 600 in
`server/flagship/config.json`) every player rubber-bands for a few seconds every 10 minutes. It costs no
memory. Follow-up: #2945.

## The square, within 60 m

| Limit (per place, `docs/architecture/performance.md` section 3) | Island now | Vanilla (CoreOnEveron, 2026-11-17) | Within |
|---|---:|---:|---|
| Night lights, at most 24 | 12 | 12 | yes |
| Shadow casters, at most 12 | 12 | 12 | yes, at the limit (all vanilla) |
| Entities added over vanilla, at most 1,500 | 3,933, so +213 | 3,720 | yes |

At 150 m: 17,761 entities (vanilla 17,070), 55 lights, 55 casters, 6,731 replicated, 0 per-frame.
`lightsOn` read 0 because the count ran at daytime; the light entities are the same set.

## 16 players: the estimate

Linear in players from the measured slope, with a margin:

- Base: 2,544 MB private, island loaded, no player spawned.
- Per player: (2,584 âˆ’ 2,544) / 2 = 20 MB private (22 MB on the engine's `Mem`). Taken as **20 to 100 MB**:
  the upper bound covers inventories, cars and a busier save than two idle players.
- AI walkers: not measured. The relocated-arrival scene's markers live in CoreOnEveron's layer, not on
  MainWorld, so `eventscene start` ran no scene on the island (`no scene running`), and the vanilla
  `control` walker needs a running scene. A vanilla AI character costs a few MB; 20 walkers are taken as
  200 MB.

| | Low | High |
|---|---:|---:|
| Base | 2.5 GB | 2.5 GB |
| 16 players | 0.3 GB | 1.6 GB |
| AI walkers | 0.2 GB | 0.2 GB |
| **Total** | **3.0 GB** | **4.3 GB** |

**PASS:** even the high case is about a third of the 12 GB line on the 16 GB plan. The `docs/architecture/
everon-world.md` budget of 4 to 6 GB is conservative. RAM does not block #2212; the autosave hitch is the
player-facing cost to fix first.

## After the cutover

The lead reads the server's memory on the 0grind panel during the Friday live smoke and adds a row here:

| Date | Players | AI | Server memory (panel) | Note |
|---|---:|---:|---:|---|
| | | | | |
