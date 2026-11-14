# Warm-runner spike (2026-11-14)

Research prototype, not shipped. Question: can one long-lived Workbench, server or
client pick up a worktree's new code without restarting? Measured on lf-cloud.

## Files

- `LF_WarmProbePlugin.c`: a World Editor plugin that serves commands from
  `$profile:lf-warm/go.txt`: `open <world>`, `reload <menu|path>` (runs
  `ScriptEditor.ExecuteAction`), `call <fn>`, `play <ms>`, `quit`.
- `LF_WarmProbe.c.tmpl`: a game-script probe whose token changes every round.
  It modifies `LF_TestRunnerComponent` so the suite can run in play mode.
- `LF_WarmClientProbe.c.tmpl`: the H3 client probe. It prints its token and, on a flag
  file, calls `Game.RequestReload()`.
- `driver.mjs <reload|h2|h2b|h2c>`: runs on a pool machine in its own run folder.
  It takes a Workbench lock and launches Workbench in the desktop session. Every wait
  is bounded. It writes `tmp/warm-spike/results.jsonl`.

## Results

**Reloading scripts in-process works.** `ExecuteAction({"Build", "Reload Scripts"})`
(Script Editor, Ctrl+Shift+F7) recompiles the Game module in a running Workbench. The
next play mode then runs the new code. Five rounds in a row worked (T1..T5), with no
crash. A reload from a clean state to a clean state takes 13-15 s: script validation
3.9 s, Game compile 4.4 s, then the game is recreated. A fresh Workbench takes 14-41 s
to start, plus 4-26 s to open DebugWorld.

What breaks:

- The Diag Workbench (the only Workbench build) raises modal `Assertion failed:
  Resources are leaking!` dialogs (`GameApp.cpp:1284`). This happened on the first
  reload after opening a world (4 dialogs) and on the reload after a failed compile
  (about 108 dialogs, which pushed that reload to 71 s). Workbench waits until each
  one is answered; pressing Ignore through UI Automation lets it go on.
- After every reload the log says `Leaked 'LF_WarmProbePlugin' script instance`. The
  plugin that is running survives as a leaked instance. `GetGame()` from the plugin
  returns null in edit mode after a reload.
- A compile error is not fatal: the log says `Can't compile "Game" script module!` and
  play mode keeps running the **previous** build. Any runner must fail on that line.
- `int x = ;` is not a compile error in Enforce (only a warning that x is unused).
  An undefined function call is one.

**The suite in Workbench play mode does not match the dedicated server.** In two of
two runs it hung at `whitelist/logic` (the log stops), once in the full suite after
513 tests and once in the fast tier after 257. Tests that pass on the dedicated
server failed here: `account/characters-per-account`, `film/recorder` (fast tier),
plus `debugworld/entity-collision`, `e2e/pass1-write-save`, `perf/budget-128` and
`save/manager-wiring`. Play mode also has no `LifeFrameworkSystems.conf`, so there is
no `SCR_PersistenceSystem`, and it has no `-scrDefine` tier flags. A 5-test `--only`
list passed 5/5. The red proof was not finished: the reload timed out on an assert
dialog that the helper missed.

**Server boot** (lf-cloud, `--only`, timed 2026-11-14, 57 s door to door):
- pool wait 3.9 s; holder + prep 24.4 s (a new lane folder); sync 1.0 s;
- ssh, checkout, node and `test-e2e.ps1` setup, about 10 s;
- engine: addons and rdb 1.3 s, script compile 6.5 s, world 3.1 s, the fixed
  `RUN_DELAY_MS` 3.8 s, the test itself 0.002 s;
- teardown and log fetch, about 3 s.

Across all logs, the median engine time is 17.5 s on lf-cloud, 9.9 s on lf-testbox
and 7.9 s here. Door to door it is 48 s, 54 s and 13 s. Holder + prep on lf-cloud has
a median of 7.7 s and a p90 of 19.9 s. 13 of 27 lf-cloud runs created a new lane
folder.

**Clients**: the server checks each client's script checksum when it joins
(`-rpl-validation-scr-disable` turns that off, Diag only), so a client on the old
scripts cannot join a server on new ones. Clients can only connect through `-client`
at launch or through a backend Room, and these sessions run with `-noBackend`.
Across 15 clients, launch to connected took 14-111 s, and most of it is engine init
plus script compile (12-80 s). The one live attempt failed: lf-cloud was running 4
other sessions and the client hit an L2 connection timeout.
