# Performance budget: 128 players (#335)

The target (decision 2026-11-08): a dedicated server holds 128 players. This page is the
budget, how it is measured, the numbers so far, and what a synthetic run cannot tell us.

## Tools

| What | Where |
|---|---|
| Counters and timers behind a flag | `LF_Perf` (`Scripts/Game/Core/LF_Perf.c`). Off until the server starts with `-lfPerf` (or a test calls `LF_Perf.SetEnabled(true)`). With the flag on, `[LFDebug:Perf] count/time/gauge ...` lines print every 60 s once a player connects |
| Budget thresholds | `Configs/Perf/LF_PerfBudget.conf` (class `LF_PerfBudget`). `validate-perf-budget.mjs` keeps conf and class in step and the player count at 128 or more |
| Benchmark and gate | test `perf/budget-128` (`LF_Test_PerfBudget`, PERSISTENCE tier): builds 128 synthetic characters through the real managers, bus and serializer, fails when a cost passes its limit |
| Report | `tools\cli perf [--local]` runs the test, prints the table, writes `tmp/perf/report.{md,json}`, exits 1 over budget. The test also runs in `--tier all`, so `wt gate` / `wt ship` enforce the budget |

Instrumented today: `Account.refreshAll` (the 10 s pass over every player), `Account.publish`,
`Account.ownerScan` (walks over the players to find an account's owner: at most one a frame, #716)
and `Replication.accountState` (bumps), `Ledger.record`, `Medic.bleedAdvance` and `Medic.downed`,
`Persistence.savePoint` (request to commit). Add a site with `LF_Perf.Begin()` / `End("Sys.name", t)`
or `LF_Perf.Count("Replication.x")`; keys are `<System>.<name>`. Timers are whole milliseconds,
so read `calls` and `maxMs` beside `totalMs`; the benchmark times batches and divides.

## Budget

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

## Measured

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

## What extrapolates to 128, what does not

Extrapolates (script cost with no engine or network in it): bus fan-out, ledger and incident
append, account and record export, store write and read of the LF payloads. They are linear in
players or rows, and the run is already at 128 characters and the retention caps, so these
numbers are the 128 numbers on this machine. A slower server CPU scales them; the 4 to 10x
headroom in the budget covers laptop-class hardware, not a cheap VPS.

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

## The multi-client run (needs people)

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
