# M4 opening day: the partial run, 2026-11-15

A rehearsal of #1503 with what was on `main` (667368a6) when slice #1608 ran. Beat sheet:
[m4-opening-day.md](m4-opening-day.md). Command: `tools\cli opening --host <scratch dir>`. Every host folder was a
scratch folder (never `server/flagship`, never the maintainer's host), and every server and watcher the run left
behind was stopped by hand afterwards.

The run needed two passes because one `--host` cannot satisfy every beat (#1691): B1, B2, B6, B8 on a folder
outside the repo, B7 and B9 on a folder under `tmp/`.

## Result per beat

| Beat | Result | Line | Card |
|---|---|---|---|
| B1 Desk: go or no-go | FAIL | `config-apply` refused: the maintainer's uid is not in `staff.txt` (a human step); `host ready`: no save yet on a fresh host | #1693 |
| B2 Start and the password gate | FAIL | `host start --flagship --wait` timed out after 420 s; no live `server.json`, so server-name and password checks could not run | #1693, #1692 |
| B3 The rules board at the plaza | BLOCKED | by #1280 (3 of 5 checks passed, 2 blocked) | none |
| B4 The quiz and a late arrival | not run | game beat, needs `--game` (see below) | none |
| B5 A police application approved | not run | game beat, needs `--game` (see below) | none |
| B6 The 2-hour day | FAIL | no `perf-*.csv` (`host perf` has to run for the length of the beat); proof verb `day-rate` is not on main | #1693 |
| B7 The 18:00 warnings and the close | FAIL | `boot`: `host start exited 1: host: already running`, because B2's `host watch` outlived its beat and kept restarting the scratch host | #1692 |
| B8 The clean restart and the reload | FAIL | `b8-reload.mjs did not finish: timed out after 600s` on a host with no save | #1693 |
| B9 Wipe with notice | FAIL | the script dies with ENOENT on `sessions.json` (stack trace, no BEAT line) on a host with nothing to wipe | #1693 |
| B10 The site's next session time | BLOCKED | `website/dist/servers/index.html` is missing (site not built); the runner scored it FAIL because the script exits 2 | #1693 |
| B11 After: the log and the session record | BLOCKED | no host `console.log` to read: the host never ran to a save (0 of 6 checks passed, 6 blocked) | none |

Count by beat (11 beats): **PASS 0, FAIL 6 (B1, B2, B6, B7, B8, B9), BLOCKED 3 (B3, B10, B11), not run 2 (B4, B5)**.
The runner's own summaries read `pass=0 fail=7 blocked=2` for the first pass and `pass=0 fail=5 blocked=2` for the
second, because it counts B10 as a FAIL.

## The three desk-run problems, by root cause

None of these is a bug in a game system; each is the run's tooling or a fresh-host assumption, and they are cards
under #230, fixed nowhere in this slice.

1. **#1691** - one `--host` cannot satisfy B1 and B2 (outside the repo) and B7 and B9 (inside `tmp/`).
2. **#1692** - B2's `host watch --flagship` stays alive after the beat and restarts the server; B7 then fails on `already running`. Two scratch hosts and one watcher had to be stopped by hand.
3. **#1693** - B9 crashes on an empty host; B1, B2, B6 and B8 FAIL where a host with no save should be BLOCKED; the runner scores a BLOCKED B10 as FAIL.

## What #1503 still waits on

- **#1280** (B3, the rules board): two of its five checks are blocked.
- A host with a save (B2 `save loaded:`, B8, B9, B11): the real host folder has none (`host status`: `saves: none yet`), so the restart, reload and wipe beats have nothing to read until the maintainer's host has run once.
- The maintainer's uid in `server/flagship/staff.txt` (B1), a human step.
- The `day-rate` proof verb (B6), not on main yet.
- The password and ban beats are manual (#1520, #1571); this run did not touch them.
- `website/dist` built for B10.

## `opening --game`: not run (SPLIT)

The two-client session (`tools\cli session up --clients 2 --pack LF_Everon1989 --mission playtest --wait`) plus the
game proofs (B4, B5, B6 `day-rate`, B8 `restart-cycle`) did not fit the 45 minute budget after the desk beats timed out
(B2 and B8 alone cost 17 minutes). The proof verb for B8 (`restart-cycle`) is on main; `day-rate` is not. The
remaining step is one card: run `opening --game` on a two-client session and add its counts to this file.
