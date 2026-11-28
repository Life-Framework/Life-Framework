# Session cheatsheet

One page to print for a session of "Life Framework | Everon 1989 (Alpha)". Each line is one
command and what you should see. The long form, with the reasons and the failure table, is the
[session runbook](session-runbook.md).

A command marked *(planned)* is not on main yet: it is in code font, never linked, and named as
its card names it. Until it lands, do the step by hand as the runbook says. `tools\cli host help`
lists what exists today. Run `tools\cli` from a worktree or the playtest checkout, never the main
checkout.

## The live flagship is on 0grind

The tables below are for a host on a PC. The real server is the rented 0grind one, driven from a worktree:

| Command | Success looks like |
|---|---|
| `tools\cli host remote 0grind status` | `status="running" ... players=N`, an A2S line for `144.76.90.77:3023`, and no `WARNING` about System settings. |
| `tools\cli host remote 0grind config push` | A masked diff, nothing written. `--yes` writes it; a `password ... -> (empty)` line means this PC has no `secrets.json`: stop. |
| `tools\cli host remote 0grind restart --yes` | The box restarts (say it in Discord first; 06:00 and 18:00 UTC restarts need nothing). |
| `tools\cli host remote 0grind saves pull` | The saves copied to `host-0grind\backups\<stamp>-remote`, sizes checked. Before anything risky, and after each session. |
| `tools\cli host remote 0grind logs pull` | The newest `console.log` path. `tools\cli server-doctor <path>` ends in PASS; its first lines say the game version. |
| `tools\cli game-update --check` | This PC's client and server builds (`1.9.0.42` on 2026-11-28). It never touches 0grind. |

A release is the one page [publish to the Workshop and 0grind](../playtest/workshop-publish.md).

## Before

| Command | Success looks like |
|---|---|
| `tools\cli host config apply` | Host stopped. Writes the profile's `config.json` and `server.json` from `server/flagship/` and the host's `secrets.json`; never prints a secret; with no `secrets.json` it warns that the join password is empty. A new host needs it once, and after any config change. `--dry-run` shows what it would write. Until it has run there is no `server.json`, so `host players` times out and `host ready` says NO-GO. |
| `tools\cli host ready` | After `host config apply`, and again at the end: one line per check (build gate, staff roster, backup age, last restore drill, disk), then `GO`. `NO-GO` names the failed checks and means do not open; exit 1. |
| `tools\cli host drift` | Exit 0: the live `config.json` and `server.json` match `server/flagship/`. A changed field is named; exit 1. |
| `tools\cli host preflight` | After a game update only: it backs up, boots the new build and passes. A failure restores the backup; read [game updates](game-updates.md). |
| `tools\cli host backup` | `host: backed up saves -> <folder>`. |
| `tools\cli host verify` | Every backup `ok`, none `empty` or `changed`; exit 0. |
| `tools\cli host drill` | The newest backup boots on a scratch profile and the log shows `save loaded:`. Weekly and after a preflight. It queues for a pool lane by itself (no `--wait`) and may run on a pool machine. |
| `tools\cli host applications` | The open police applications, oldest first, then the officers on trial. Read only; approve in game. |
| `tools\cli host announce --at <time> --build <build>` | The post is printed and the next session is written to `website/content/sessions.json` (a tracked file: `git checkout -- website/content/sessions.json` after a dry run, commit it for a real one). `--build` is the installed game build, e.g. `1.9.0.42`; needed until the host has a save. Post it in Discord ([session playbook](../playtest/session-playbook.md)). |

## Start

| Command | Success looks like |
|---|---|
| `tools\cli host start --flagship --wait` | `host: up (pid N)` and the join lines. The first start in a new worktree spends about 45 s refreshing the resource database first, and the server runs on this PC. |
| `tools\cli host status` | The log path, then `save loaded:` in the log with the build you expect. On a fresh host (no save yet) it is `save present: false` and `fresh session: no save point exists for this mission` instead. `save load damaged` means stop. |
| `tools\cli host watch --flagship` | The server restarts after a crash or a scheduled close; it ends after `host stop`. |
| Join with the server password and walk to the plaza | You spawn in. If you cannot, the session is not open. |

## During

| Command | Success looks like |
|---|---|
| `tools\cli host players` | Server name, map, and players over max within 3 seconds. A timeout on a host that never ran `host config apply` (Before) is that missing `server.json`, not a dead server. |
| `tools\cli host say "Hotfix restart in 10 minutes"` | Every player gets one toast within 10 seconds; the server logs `host say:`. |
| `tools\cli host perf` | A verdict line against the 16-player budget and a CSV in `<host>/perf/`. |
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
| `tools\cli host logs archive` | The finished session's logs moved to `<host>/logs/<session-stamp>/`; the live folder untouched. |
| `tools\cli host triage` | Error groups counted, each `known` or `new` against [debug-log-triage](../debug-log-triage.md). A `new` group becomes an issue. |
| `tools\cli host attendance` | Unique players, the peak and when it was, minutes per player. |
| `tools\cli host offsite <dir>` | Every backup copied to the second drive and checked by size and hash. |
| `tools\cli host prune --dry-run` | Lists what it would delete. Then `tools\cli host prune` keeps the newest 20 and never deletes a tagged backup. |

## A wipe, with 24 hours' notice

A wipe is announced in Discord at least 24 hours ahead ([wipe policy](../community/wipe-policy.md)).
Never wipe before the announcement is out.

| Command | Success looks like |
|---|---|
| `tools\cli host announce --at <time> --build <build> --wipe` | Refuses a time less than 24 hours away; otherwise prints the post and records the notice. |
| `tools\cli host stop` | The host is down; `wipe` refuses while it runs. |
| `tools\cli host wipe --confirm <host dir name>` | A `wipe` backup, the saves moved aside (nothing deleted), a line in `backups/wipes.log`, and how to restore. |
| `tools\cli host start --flagship --wait` | A fresh world comes up. |
| `tools\cli host restore` | Mistake: puts the newest backup back and keeps what it replaced. |
