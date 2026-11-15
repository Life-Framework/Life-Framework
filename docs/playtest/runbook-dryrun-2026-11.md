# Runbook dry run by a second operator, 2026-11-15 (#1606, proof for #305)

An agent that did not write [the session runbook](../ops/session-runbook.md) ran a scratch session
from that page and [the cheatsheet](../ops/session-cheatsheet.md) alone: announce (dry), start,
restart and stop. Build `41364e222`, game build 1.9.0.37, worktree `ws/slice-1606`.

Setup: `LF_HOST_DIR=<worktree>\tmp\dryrun-host`, so the real host, the live saves and
`server/flagship` were never touched. Every command ran from the worktree, not the main checkout.
`host announce` ran without `--send`, and the line it wrote to `website/content/sessions.json` was
reverted. The runbook and cheatsheet were not edited: #1350 and #1557 own them.

## Result

Start, restart and stop work as written. The stumbles are in the planned/shipped labels, in what a
*fresh* host looks like, and in three commands (`verify`, `drill`, `players`) that failed on a
scratch host. Seven stumbles, each filed under [#230](https://github.com/Life-Framework/Life-Framework/issues/230):
[#1632](https://github.com/Life-Framework/Life-Framework/issues/1632),
[#1633](https://github.com/Life-Framework/Life-Framework/issues/1633),
[#1634](https://github.com/Life-Framework/Life-Framework/issues/1634),
[#1635](https://github.com/Life-Framework/Life-Framework/issues/1635),
[#1636](https://github.com/Life-Framework/Life-Framework/issues/1636),
[#1637](https://github.com/Life-Framework/Life-Framework/issues/1637),
[#1638](https://github.com/Life-Framework/Life-Framework/issues/1638).

## Runbook steps

Each row is a runbook step, the command as run, the line that proved it, and PASS or the stumble.
All commands were `tools\cli host ...` with `LF_HOST_DIR` set.

### Before

| Step | Command | Output line | Verdict |
|---|---|---|---|
| 1 Announce (dry) | `host announce --at 2026-11-21T19:00:00Z` | `no build: pass --build <version> (no host save to read it from)`, exit 1 | **Stumble** [#1632](https://github.com/Life-Framework/Life-Framework/issues/1632): the page marks it *(planned)* but it exists; it needs `--build`, and the page never says where the number comes from |
| 1 Announce (dry), second try | `host announce --at 2026-11-21T19:00:00Z --build 1.6.0` | `next session recorded in ...\website\content\sessions.json (draft: no announced time)` then `draft only; add --send to post it` | **Stumble** [#1632](https://github.com/Life-Framework/Life-Framework/issues/1632): it rewrites `sessions.json` in the checkout even without `--send`, which the page does not warn about. The build `1.6.0` was a guess; `host status` prints the real one (1.9.0.37) only after the first save |
| 1 Announce `--wipe` | `host announce --at ... --build 1.9.0.37 --wipe` | the post gains `**WIPE:** this session starts with a wipe ...` | PASS (file reverted) |
| 2 Check what changed | `host status` | `host: stopped`, `saves: none yet` | PASS |
| 2 Preflight | `host preflight` | `preflight: no save yet; nothing to prove. 'tools\cli host start' starts a fresh world` | PASS |
| 3 Back up | `host backup` | `host: no save to back up` | PASS (nothing to back up on a new host; the page does not say that this is expected) |
| 4 Roster and config | `host ready`, `host drift` (the page still calls the related commands *(planned)*) | `host ready: NO-GO drill, config`; `drift: 2 difference(s)` (both files missing) | **Stumble** [#1633](https://github.com/Life-Framework/Life-Framework/issues/1633) and [#1635](https://github.com/Life-Framework/Life-Framework/issues/1635): `ready` and `drift` exist but the page says do it by hand; `config apply` is absent from the cheatsheet, so a scratch host cannot reach GO |
| 5 Prove the newest backup restores | `host drill`, run twice | `SUMMARY host-drill fail ... lock 'server-2001' is held ...; retry with --wait`, both times | **Stumble** [#1637](https://github.com/Life-Framework/Life-Framework/issues/1637): `drill` takes no `--wait`; stopped after two tries as the page says |
| (cheatsheet) Verify backups | `host verify` | `nosave 2026-11-15_12-22-14-stop  no save point (meta-info.json ...)`, `3 backup(s), 3 bad`, exit 1 | **Stumble** [#1636](https://github.com/Life-Framework/Life-Framework/issues/1636): every backup holds `savepoint000` and `savepoint001` with `meta-info.json` and the next start loaded that save |

### Start

| Step | Command | Output line | Verdict |
|---|---|---|---|
| 1 Start | `host start --flagship --wait` | `host: up (pid 74432)` and the join lines; `Entered online game state.` | PASS. First start in a new worktree printed about 45 s of resource-database refresh on lf-cloud before launch, and the server ran on this PC (port 2001), not on a test machine: [#1634](https://github.com/Life-Framework/Life-Framework/issues/1634) |
| 2 Confirm and read the log | `host status`, then `Select-String ... -Pattern "LFDebug:Persistence"` | `host: running (pid 74432)`; log lines `save scan complete, save present: false`, `fresh session: no save point exists for this mission` | **Stumble** [#1634](https://github.com/Life-Framework/Life-Framework/issues/1634): the page says you want `save loaded:`, which a fresh start never prints |
| 3 Keep it alive | `host watch --flagship` | not run: a supervisor would hold the host and the card asks for a restart by hand | SKIPPED |
| 4 Join once | not run: a scratch host on this PC has no player client in this run | | SKIPPED |

### During

| Step | Command | Output line | Verdict |
|---|---|---|---|
| Is it healthy | `host status` | `host: running (pid 74432)` | PASS |
| Players | `host players` | `A2S query to 127.0.0.1:17777 timed out after 3000ms`, exit 1 | **Stumble** [#1635](https://github.com/Life-Framework/Life-Framework/issues/1635): no `server.json` exists until `host config apply` runs, and the cheatsheet does not list it |

### Restart

| Step | Command | Output line | Verdict |
|---|---|---|---|
| Stop | `host stop` | `stop requested (pid 74432)`, `save completed (SHUTDOWN)`, `host: backed up saves -> ...\backups\2026-11-15_12-21-57-stop` | PASS (stopped in 2 s) |
| Start again | `host start --flagship --wait` | `host: continuing the latest save (1 save point(s))`, `save loaded: version 34, game build 1.9.0.37, running 1.9.0.37`, `host: up (pid 21972)` | PASS |

### Stop

| Step | Command | Output line | Verdict |
|---|---|---|---|
| Stop | `host stop` | `save completed (SHUTDOWN)`, `host: backed up saves -> ...\backups\2026-11-15_12-22-14-stop`, then `host: stopped` | PASS |

### After

| Step | Command | Output line | Verdict |
|---|---|---|---|
| 1 Read the log | `host triage` | `SUMMARY host-triage ok groups=6 known=0 new=6 lines=285`, all `[LFDebug:DebugWorld] collision repair enabled traceability ...` | **Stumble** [#1638](https://github.com/Life-Framework/Life-Framework/issues/1638): the page says a new group becomes an issue; an operator would file six on a clean start |
| 2 Rotate the backups | `host prune --dry-run`, then `host prune` | `would delete 0, kept 3 plain`, `deleted 0, kept 3 plain` | PASS |
| 3 Wipe | `host wipe --confirm dryrun-host` | `refused: no wipe notice: the next session in ...sessions.json is not scheduled; post it first: ...` | PASS (the refusal is right: no notice 24 h earlier; the wipe itself was not run) |
| 4 Session log row | not done: a scratch run is not a session | | SKIPPED |

### When it goes wrong

| Table row | Seen? | Verdict |
|---|---|---|
| `a test or server run holds port 2001` | Yes, on `host drill` (lf-cloud) | The row says to retry with `--wait`; `drill` has no such option ([#1637](https://github.com/Life-Framework/Life-Framework/issues/1637)) |
| Every other row | No | not exercised |

## Not covered

`host watch`, a join by a game client, `host restore`, a forced kill on stop, and a game-update
preflight were not run. A real preflight needs a new game build; the restore path needs a wrong
save to undo. The next run should add them.
