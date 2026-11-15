# Wipe dry run, November 2026 (#1670)

The record of #556's wipe path run on a scratch host on `lf-testbox` (the maintainer's PC; the pool maps that name to
it) on 2026-11-15, for chapter #1006. The policy is [wipe policy](../community/wipe-policy.md). Scratch host
`C:\LF\scratch\wipe-1670-20261115-2337` (the folder name is what `host wipe --confirm` must match), `LF_HOST_DIR` and
`LF_ANNOUNCE_SESSIONS` pointed at it, so the real host and `website/content/sessions.json` were never touched. Mission
`LF_PlaytestTown`, core plus the 1989 pack from this worktree, game build 1.9.0.37. The folder was removed with
`tools/safe-rm.mjs` at the end.

Two things stand in for a person. The Discord post is not sent (agents never post): the notice was posted by
`simulatePost`, and aged 25 h by `ageNotice`, both from `tools/playtest/opening/b9-wipe.mjs`, which this run only
imported. The host holds a save but no player account, because no client was on the box (0.9 GB RAM free); the
account-level proof is the `wipe-restore` verb (#1562). The save that comes back here is the first boot's save point.

`host` commands print no `SUMMARY` line; the line shown is the `[LFDebug:HostWipe]` or `host:` line each one prints.

## Steps

| # | Step | Command | Result and the line that shows it |
|---|---|---|---|
| 1 | First boot | `host start --force --wait --timeout 300`, then `host stop` | PASS, 23:40:13 `fresh session: no save point exists for this mission`; stop wrote `save completed (SHUTDOWN)` and `backed up saves -> ...-stop`. |
| 2 | Boot with the save | `host start --force --wait`, then `host stop` | PASS, 23:44:08 `save loaded: version 34, game build 1.9.0.37, running 1.9.0.37` and `session continued from a save point (-loadSessionSave)`. This is the "before". |
| 3 | Draft the notice | `host announce --wipe --at <now + 30 h>` | PASS, `next session recorded in ...\sessions.json (draft: no announced time)`. |
| 4 | Wipe, notice only drafted | `host wipe --confirm wipe-1670-20261115-2337` | REFUSED as it should (exit 1): `the wipe notice is drafted, not posted (no announced time in ...)`. |
| 5 | Wipe, notice posted now | the same command, after `simulatePost` | REFUSED (exit 1): `the wipe notice was posted 2026-11-15T13:44:29Z; a wipe needs 24 hours of notice, so it is allowed from 2026-11-16T13:44:29Z (in 24.0 h)`. |
| 6 | Wipe, wrong name | `host wipe --confirm nope` | REFUSED (exit 1): `confirm does not match: type the host folder name exactly`. |
| 7 | Wipe, notice aged 25 h | the same command, after `ageNotice(file, 25)` (`{"aged":true,"fields":["announced"]}`) | PASS (exit 0): `wiped: backup ...\backups\2026-11-15_23-44-32-wipe`, `old saves moved to ...-wiped-saves`, `to restore: tools\cli host restore 2026-11-15_23-44-32-wipe`. `backups/wipes.log` has one row: `wipe host=wipe-1670-20261115-2337 backup=2026-11-15_23-44-32-wipe aside=2026-11-15_23-44-32-wiped-saves`. `host status` then says `saves: none yet`. |
| 8 | Clean boot | `host start --force --wait`, `host stop` | PASS, 23:44:47 `save scan complete, save present: false`, `Entered online game state.`, `fresh session: no save point exists for this mission`. No `save loaded:` line, no `SCRIPT (E)`, no VME. |
| 9 | Restore | `host restore 2026-11-15_23-44-32-wipe` | PASS (exit 0): `restored ...-wipe -> ...\profile\profile\.save`; the fresh save is kept in `...-replaced-by-restore`. |
| 10 | Boot after the restore | `host start --force --wait`, `host stop` | PASS, 23:45:13 `save loaded: version 34, game build 1.9.0.37, running 1.9.0.37` and `session continued from a save point (-loadSessionSave)`. This is the "after". |

From the first start (23:37:47) to the last stop (23:45:22) took about 8 minutes. The first two boots took 2.4 and 3.7 minutes
from command to ready; the two after them took about 15 and 23 seconds. Nothing in the output says why the first two
were slow.

## Not run

- The host running at the moment of the wipe (`refuses while the host runs`): `tools/playtest/opening/b9-wipe.mjs`
  covers it, and the host here was stopped for every wipe.
- The Discord post itself (`host announce --wipe --send`): maintainer only.
- A wipe on a real account (cash, house, character): the `wipe-restore` proof (#1562).
- A wipe on the live flagship host or the rented box.

## Stumbles

None needing a card. One wording fix: the policy page called `host announce --wipe` and `host wipe --confirm`
"planned"; both are on `main` and behaved as written, so the page now says so.
