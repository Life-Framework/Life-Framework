# Session cheatsheet

One page to print for a session of "Life Framework | Everon 1989 (Alpha)". Each line is one
command and what you should see. The long form, with the reasons and the failure table, is the
[session runbook](session-runbook.md).

A command marked *(planned)* is not on main yet: it is in code font, never linked, and named as
its card names it. Until it lands, do the step by hand as the runbook says. `tools\cli host help`
lists what exists today. Run `tools\cli` from a worktree or the playtest checkout, never the main
checkout.

## Before

| Command | Success looks like |
|---|---|
| `tools\cli host config apply` | Writes the profile's `config.json` and `server.json` from `server/flagship/` and the host's `secrets.json`; secrets are not printed; with no `secrets.json` it warns that the join password is empty. Run it first on a new host and after any config change, with the host stopped. Until it has run there is no `server.json`, so `host players` times out and `host ready` says NO-GO. |
| `tools\cli host drift` *(planned)* | Exit 0: the live `config.json` and `server.json` match `server/flagship/`. A changed field is named; exit 1. |
| `tools\cli host ready` *(planned)* | One line per check, then `GO`. `NO-GO` means do not open. |
| `tools\cli host preflight` | After a game update only: it backs up, boots the new build and passes. A failure restores the backup; read [game updates](game-updates.md). |
| `tools\cli host backup` | `host: backed up saves -> <folder>`. |
| `tools\cli host verify` *(planned)* | Every backup `ok`, none `empty` or `changed`; exit 0. |
| `tools\cli host drill` | The newest backup boots on a scratch profile and the log shows `save loaded:`. Weekly and after a preflight. |
| `tools\cli host applications` *(planned)* | The open police applications, oldest first, then the officers on trial. Read only; approve in game. |
| `tools\cli host announce --at <time>` *(planned)* | The post is printed and the next session is written to `website/content/sessions.json`. Post it in Discord ([session playbook](../playtest/session-playbook.md)). |

## Start

| Command | Success looks like |
|---|---|
| `tools\cli host start --flagship --wait` | `host: up (pid N)` and the join lines. |
| `tools\cli host status` | The log path, then `save loaded:` in the log with the build you expect. `save load damaged` means stop. |
| `tools\cli host watch --flagship` | The server restarts after a crash or a scheduled close; it ends after `host stop`. |
| Join with the server password and walk to the plaza | You spawn in. If you cannot, the session is not open. |

## During

| Command | Success looks like |
|---|---|
| `tools\cli host players` | Server name, map, and players over max within 3 seconds. A timeout on a host that never ran `host config apply` (Before) is that missing `server.json`, not a dead server. |
| `tools\cli host say "Hotfix restart in 10 minutes"` *(planned)* | Every player gets one toast within 10 seconds; the server logs `host say:`. |
| `tools\cli host perf` *(planned)* | A verdict line against the 40-player budget and a CSV in `<host>/perf/`. |
| `tools\cli host status` | Still `up`. Saves are never touched while it runs. |

## Restart at 06:00 or 18:00

The restart is on a schedule and players get a toast only; there is nothing to announce. For a
restart by hand, say it in Discord first, then:

| Command | Success looks like |
|---|---|
| `tools\cli host stop` | `host: backed up saves -> <folder>`. `FAIL ... force-killing` means no shutdown save: see the runbook table. |
| `tools\cli host start --flagship --wait` | `host: up (pid N)` and `save loaded:`. (Not needed under `host watch`.) |

## Stop

| Command | Success looks like |
|---|---|
| `tools\cli host stop` | `host: backed up saves -> <folder>` within 180 seconds. Then say in Discord that the session is over and when the next one is. |

## After

| Command | Success looks like |
|---|---|
| `tools\cli host logs archive` *(planned)* | The finished session's logs moved to `<host>/logs/<session-stamp>/`; the live folder untouched. |
| `tools\cli host triage` | Error groups counted, each `known` or `new` against [debug-log-triage](../debug-log-triage.md). A `new` group becomes an issue. |
| `tools\cli host attendance` *(planned)* | Unique players, the peak and when it was, minutes per player. |
| `tools\cli host offsite <dir>` *(planned)* | Every backup copied to the second drive and checked by size and hash. |
| `tools\cli host prune --dry-run` | Lists what it would delete. Then `tools\cli host prune` keeps the newest 20 and never deletes a tagged backup. |

## A wipe, with 24 hours' notice

A wipe is announced in Discord at least 24 hours ahead ([wipe policy](../community/wipe-policy.md)).
Never wipe before the announcement is out.

| Command | Success looks like |
|---|---|
| `tools\cli host announce --at <time> --wipe` *(planned)* | Refuses a time less than 24 hours away; otherwise prints the post and records the notice. |
| `tools\cli host stop` | The host is down; `wipe` refuses while it runs. |
| `tools\cli host wipe --confirm <host dir name>` | A `wipe` backup, the saves moved aside (nothing deleted), a line in `backups/wipes.log`, and how to restore. |
| `tools\cli host start --flagship --wait` | A fresh world comes up. |
| `tools\cli host restore` | Mistake: puts the newest backup back and keeps what it replaced. |
