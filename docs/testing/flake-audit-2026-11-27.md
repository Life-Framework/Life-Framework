# Flake audit, 2026-11-27

Source: `tmp/logs/main-guard-flakes.log` (108 entries, 2026-11-16 to 2026-11-26): steps the main guard saw fail once and
pass on the recheck ("red twice" rule). This audit ranks them, finds causes where the evidence survives, and fixes what
is still live.

## Ranking

| Step / test | Hits | Last seen | State |
|---|---|---|---|
| `debugworld/entity-collision` (suite) | 40 | 11-21 | fixed before this audit (tests delete what they spawn; `BaseWorldClear`, `Armory`, `CargoBedPersist`, `Siren` clean up) |
| pre-move e2e | 24 | 11-25 | step retired with the M3 move; no evidence left |
| validate selftests (all) | 36 | 11-26 | **fixed here**, see below |
| `lockers/lost-and-found` | 6 | 11-22 | fixed 11-23 (`a2fb9d945`, stranger reclaims from the owner position) |
| `spawn/point-floor` | 5 | 11-22 | no recurrence in 5 days; cause unproven, see "Not fixed" |
| `persist/save-newer-version` | 4 | 11-17 | fixed 11-18 (`744f2af68`, newer save written in pieces to a file) |
| `quests/signals-progress` | 4 | 11-18 | fixed 11-23 (`cf7275099`, follows the 30-level ladder) |
| `placement/policy-cost` | 2 | 11-18 | fixed 11-18 (`1bc3db9b2`, fixed clear spot) |
| `farming/plant-world-reset` | 2 | 11-19 | no recurrence; evidence gone |
| `housing/furniture` | 1 | 11-19 | no recurrence; evidence gone |
| `reputation/contract-tracker` | 1 | 11-24 | **fixed here** |
| suite crash in the pack leg | 2 | 11-24 | engine access violation, same offset both times; **diagnosis added here** |

Evidence problem: of the ten in-game flakes only the 11-24 ones still had a raw log. The flake log named the failing
run's compact transcript, which lists the failed test but not its assertion lines, and the raw `test-*.log` rotates away
within hours. That is fixed here (see "Pipeline").

## Fixed here

### Node selftests (validate probes)

| Selftest | Cause | Fix |
|---|---|---|
| `wt-lock-contention-selftest` (7 hits) | A fixed 3 s window. On a loaded box a worker got a few hundred ms of CPU and took the lock 4 or 7 times; the check wants 10. | Work-based: each worker makes 1500 tries behind a start barrier (all four workers have imported `wt.mjs` before any churns), 60 s cap. 5 runs in a row green, 5 s each, plus 4 at once. |
| `merge-drivers`, `world-sync`, `main-ff-heal`, `main-guard-validate`, `pool-selftest` (timeouts, 14 hits) | Hundreds of git processes (world-sync alone 300+, 60 s of pure git) take 3 to 9 times longer when validate runs beside many agents; the 600 s per-check budget killed healthy checks. The first two got 1200 s on 11-22, the other three never did and were red on 11-22. | `SLOW_CHECK_S` now lists `main-ff-heal`, `main-guard-validate` and `pool` at 1200 s (`tools/cli.mjs`). |
| `validate-worktrees.ps1` (3 timeouts at 601 s, 1 missing-path error) | With 600+ registered worktrees and a cold status cache the scan ran one `git status` per merged worktree (one run took 213 s, three passed 600 s). The missing-path error is a worktree pruned mid-scan by another agent, and stays an error (selftest pins it). | A 240 s budget (`LF_VALIDATE_WORKTREES_GIT_BUDGET_S`) for the per-worktree `git status`; past it the rest are counted in one INFO line and read on a later run. The warning they would give is advice only. Selftest red then green. |
| `main-ff-heal`, `main-guard-validate`, `world-sync` (resource errors) | Windows could not start git under memory pressure: `error launching git: The paging file is too small`, `Insufficient system resources`, exit 0xC0000142. `git-retry` retried only silent exit-1 failures. | `git-retry.mjs` re-runs a launch failure too (longer backoff); `world-sync` goes through it. `git-retry-selftest` case 7: red (`ran 1 times and ended exit 1`) then green. |
| `main-write-guard-selftest`, `help-guard-selftest`, `town-map-selftest` | Wall-clock caps: hook max 20 s (a frozen box took 41 s), `--help` under 15 s, `town map --force` with a 60 s timeout. | The median is the claim for the hook; real hangs end at a 180 s spawn timeout; `--help` is judged by its output, not its time; `town map` gets a 300 s timeout. |

Not touched, one hit each: `asset-materials-selftest` (Blender run, 3.5 s; outside the repo's control), `git-retry`,
`host-restart-backup`, `heal-hotspots`, `merge-drivers` early flakes from 11-19 (covered by the retry and budget work,
no logs left).

### `reputation/contract-tracker` (11-24, one flake)

Surviving log: every client-side assertion false (`the received line is held`, then each time word `expected 1, got 0`).
`RunClient` handed `Receive` and `ClientTimeWord` the tick `0`. When a workspace exists, `Receive` builds the tag and its
first `Tick()` reads `System.GetTickCount()`; a line received at tick 0 is then as old as the process, expires and is
cleared before the next assertion. A dedicated test server normally has no workspace, so the test passes until the
layout parts load, which is the load-dependent part. The game is right (a real client passes its real tick). Fix:
the test counts from the real tick (`t0`), in `LF_Test_ContractTracker` and the same pattern in
`LF_Test_ContractTrackerResend`. 5 runs in a row green, `--no-cache`. A workspace cannot be forced on the dedicated
test server, so there is no red run for this one.

### `investigation/locker-fill` fails alone (#5166, order dependence)

`test --only LF_Test_EvidenceLockerFill` failed `- the cool-down is spent` on unmodified main; the full suite passed.
Cause: `LF_GameModeRoleplay` installs the real convoy subscriber (`LF_EvidenceConvoy.Install()`). Run alone, it hears
every `LF_OnEvidenceConvoyDue` the test raises; `RearmNow` re-checks the still-full locker, the subscriber's
`Start` is refused in the minimal LOGIC world (no route), it calls `ScheduleRearm`, and `IsRearmPending()` is true when
the test asserts it is false. In the full suite an earlier test (`LF_Test_EvidenceConvoy` reset, or a world-end reset)
had already dropped that subscriber, so the test only passed by what another test left behind. The game is right. Fix:
the test calls `LF_EvidenceConvoy.ResetStatics()` (its own feature's reset, never `ResetAll`) before subscribing its
listener, so only the listener hears the order. Red then green: unfixed `--only` failed (`locker-fill`), fixed `--only`
x3 green with `--no-cache`. The other Evidence tests (`Convoy`, `LockerPersist`, `BagHeld`, `Casing`, `BreachEvidence`,
`PlacesEvidenceLocker`) each pass alone.

### `booking-desk-proof` 1f0 frame gap (RC dry run 5, 11-27, one flake)

Flake-log line (a proof, not a main-guard step, so written here by hand):

```
2026-11-27T15:11:28Z 12cb371c booking-desk-proof: 1f0 longest gap 3616 ms failed once, passed on rerun at 533 ms (D:\LF\worktrees\Life-Framework-ws-rc-dryrun-5\tmp\logs\booking-desk-proof-20261127-161128.log)
```

Surviving log: the transcript above; the client console of that session is gone. Step 1f0 reset `framegap` before
the aim sweep, so the window held the sweep, a `capture ui` screenshot (`booking_desk_1_answer_in_view`), the
confirm and the settle. The box had 0.5 GB of VRAM free.

The pay path itself does no heavy client step: `PayNow` and `LF_ChargePayment.Settle` run on the server; the client
gets the cash stack's removal (one `FindItemsByPrefab` walk for the HUD recount since #4892), the uncuff and the
closed offer. No save, no UI rebuild, no `GetDepositItemCountByResource`.

A screenshot does stall a client past 1 s on a loaded box: in a `drive-gate-ahead` session the same day, 4 of 12
logged `client frame gap` lines sat on a `Screenshot successfully saved` line; in this fix's own session 1 of 6 did
(1092 ms, `booking_desk_2_offer_time_only`). None was in a pay window.

Fix (`agent-box/proofs/booking-desk-proof.mjs`): `framegap` is reset just before the confirm key, so 1f0 measures the
pay alone; the sweep and shot gap is still printed beside it. The 1 s threshold stays (#4824 was 15-30 s). Three runs
on one session, two clients, with three other agents' sessions on the box and 0.1 GB of VRAM free (no quiet box was
available):

| Run | Sweep and shot | Pay (1f0) | Result |
|---|---|---|---|
| 1 | 480 ms | 467 ms | PASS, 24 steps |
| 2 | 616 ms | 580 ms | PASS, 24 steps |
| 3 | 656 ms | 677 ms | PASS, 24 steps |

## Pipeline

- `tools/main-guard.mjs` copies the raw transcripts a failing run names (`log=...`) to `tmp/logs/flakes/` (newest 80 kept)
  and the flake-log line now ends with the failing lines. Validate flakes now point at the failing run, not the passing
  probe (the old line named the probe, so none of the validate entries above had a failure to read). Selftest: red then
  green (`main-guard-selftest`).
- `tools/test/test-e2e.ps1`: a crash now prints the last test started before the engine died and the two before it.

## Not fixed (no evidence, no recurrence)

- **Pack-leg engine crash** (`Access violation ... at 0x0`, same module offset `6db3` on 11-24 01:14 and 08:12). Pure engine
  fault with no script line; the profile folder was overwritten. Seen twice, not since. The next one names the test that
  was running.
- **`spawn/point-floor`** (5 hits, 11-21 to 11-22, the days after it landed). The world sweep judges every house, bed, jail
  and spawn point, so a prop an earlier test leaves near one fails it; it failed together with `entity-collision` and
  `lost-and-found` in the same runs, and the leftover work since then (BaseWorldClear, Armory, CargoBedPersist, Siren,
  `suite/no-leftover-fixtures`) removes that class. Its failure text already names the row. No recurrence since 11-22.
- **`farming/plant-world-reset`, `housing/furniture`**: two and one hits on 11-19, nothing since, no logs.

## Runs

| What | Result |
|---|---|
| `wt-lock-contention-selftest` x5, plus 4 parallel | green |
| `validate-worktrees-selftest`, `git-retry-selftest`, `main-guard-selftest` | red without the fix, green with it |
| `help-guard`, `main-write-guard`, `town-map`, `world-sync`, `main-ff-heal` selftests | green (5 in parallel) |
| `LF_Test_ContractTracker,LF_Test_ContractTrackerResend` `--no-cache` x5 | 5 of 5 green |
| the 9 flaky in-game tests together, `--no-cache` | green (10 passed) |
| `tools\cli validate` | ok, 0 failures |
| `tools\cli test --no-build --tier all --no-cache` | green, 1563 passed, 0 failed (both legs) |
