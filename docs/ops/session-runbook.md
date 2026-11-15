# Session runbook

How to run one session of the flagship server, "Life Framework | Everon 1989 (Alpha)", from the
announcement to the log read afterwards. It is written so a second person can follow it with the
maintainer away. Paste the commands as they are. Issue: #1275 (M4, ops).

A command marked *(planned)* is on the M4 plan and has not merged yet. Its name is written as its
card names it, in code font and unlinked. `tools\cli host help` lists what exists today; the
last section says what to do until a planned command lands.

Rules for the whole page:

- Run `tools\cli` from a worktree or the playtest checkout, never the main checkout:
  `host start` refuses to run there. `tools\cli playtest` keeps a pinned checkout beside the
  repo.
- Saves, backups and `host.json` live in the host folder, `%LOCALAPPDATA%\LifeFramework\host`
  (set `LF_HOST_DIR` to move it). Backups are in its `backups` folder.
- Only the maintainer is staff with in-game powers at opening. Everyone else follows the
  [moderator guide](../community/moderator-guide.md) and asks the maintainer for in-game action.
- Never delete anything by hand. The commands below move or copy, and keep what they replace.

## Before

1. **Pick the time and announce it.** The player-facing wording and the Discord steps are in the
   [session playbook](../playtest/session-playbook.md).
   `tools\cli host announce --at <iso time> --build <build> [--wipe]` prints the post and writes
   the next session to `website/content/sessions.json`; with `--send` (maintainer only) it posts
   through the Discord tooling. Without `--send` it is a draft: nothing is posted. The website
   shows the next session time only, so the time in the post is the one that counts.

   - **`--build` is needed on a fresh host.** The command reads the build from the newest host
     save, and a new host has none, so it stops with `no build: pass --build <version>`. The
     number is the installed game build, the file version of `ArmaReforgerServer.exe` (the Steam
     folder, or `ENFUSION_SERVER_PATH`), for example `1.9.0.37`. After the first save,
     `host status` prints `save and game are both <build>` and `--build` can be left out.
   - **It edits a tracked file, even as a draft.** The command rewrites
     `website/content/sessions.json` in your worktree, so the checkout is dirty afterwards. If
     that was a dry run, put the file back with `git checkout -- website/content/sessions.json`.
     If it was the real announcement, commit the file on purpose in a PR; do not leave it
     modified.

   A wipe is announced **at least 24 hours ahead**, with `--wipe`; the command refuses a time
   less than 24 hours away. Policy: [wipe policy](../community/wipe-policy.md).

2. **Check what changed.** After any Arma Reforger update, run the preflight before opening the
   server. It backs the save up, boots it on the new build, and restores it if it does not load:

   ```
   tools\cli host status
   tools\cli host preflight
   ```

   `host status` prints both builds and says when a preflight is due. Read
   [game updates](game-updates.md) if the preflight fails; do not open the server after a failure.

3. **Back up.** `host start` and `host stop` each back up by themselves. Take one more by hand
   before anything risky (a new build of the mod, a wipe):

   ```
   tools\cli host backup
   ```

4. **Check the staff roster and the config.** *(planned)* `tools\cli staff roster check`
   validates `server/flagship/staff-roster.json`. *(planned)* `tools\cli host config apply` writes
   the profile's `config.json` and `server.json` from `server/flagship/` and the host-only
   `secrets.json` (the join and admin passwords and the webhook URLs stay out of the repo). Until
   they merge, check by hand that the password in the live `server.json` is set and the name is
   exactly `Life Framework | Everon 1989 (Alpha)`.

5. **Prove the newest backup restores.** *(planned)* `tools\cli host drill` boots the newest
   backup on a scratch profile and passes only when the log shows `save loaded:`. Run it once a
   week and after a preflight.

## Start

1. Start the flagship and wait for the world:

   ```
   tools\cli host start --flagship --wait
   ```

   It backs the saves up first, continues the latest save, and prints `host: up (pid N)` and the
   join lines. Without `--flagship` it boots the playtest header, which is not the real server.

   Two things to expect that the command does not say:
   - The first `host start` in a new worktree refreshes the resource database on a pool machine
     (lf-cloud) before it launches: about 45 seconds of output before `host: up`. Wait for it; it
     is not a hang.
   - The server itself runs on **this PC**, not on a test machine, with the game port from the
     join lines (2001 by default). Nothing else may hold that port; see the table below.

2. Confirm it is up and read the first lines of the log:

   ```
   tools\cli host status
   Select-String -Path "<log path that host status prints>" -Pattern "LFDebug:Persistence"
   ```

   (The second line is PowerShell.) What you want depends on whether the host has a save:

   - **Continuing a save** (every start after a `host stop`): `save loaded:` with the build you
     expect.
   - **A fresh host** (no save yet, or right after a wipe): `save loaded:` does not appear. You
     want `save present: false` and `fresh session: no save point exists for this mission`; the
     first `save loaded:` comes on the next start, after a stop wrote the shutdown save.

   `save load damaged` or `saving disabled for this session` means stop and see "When it goes
   wrong".

3. Keep it alive. *(planned)* `tools\cli host watch` supervises the server: a crash (an exit
   without `host stop`) copies the console log to `backups/<time>-crash/`, backs the saves up and
   restarts it with the latest save. Three crashes in ten minutes stop the watch and alert staff.
   Until it merges, nobody restarts a crashed server except by `host start` below.

4. Join once yourself with the server password and walk to the plaza. If you cannot, the session
   is not open.

## During

- **Who is staff.** Read the roster, not memory. *(planned)* `tools\cli staff roster check`
  validates `server/flagship/staff-roster.json`; the maintainer is the only administrator. A
  moderator works from tickets and `/report` flags as the [moderator guide](../community/moderator-guide.md)
  says and does not act in game.
- **Is it healthy.** Every so often:

  ```
  tools\cli host status
  ```

  *(planned)* `tools\cli host players` asks the server who is on, with a 3 second timeout.
- **Warnings.** *(planned)* a restart schedule in the config sends every player a toast "Server
  restart in N minutes" at each warning; nothing in this runbook needs doing at those times. A
  restart you start by hand is announced in Discord first (see "Restart").
- **Do not touch the saves while it runs.** `host wipe` and `host restore` refuse while the host
  runs; stop it first.

## Restart

Say so in Discord first, with a few minutes' notice, then:

```
tools\cli host stop
tools\cli host start --flagship --wait
```

`host stop` asks the game to close so the engine writes its SHUTDOWN save, waits up to 180 s,
then backs the saves up. If it prints `FAIL ... force-killing`, there is no shutdown save: the
backup is the last good state, see the table below.

To restart after a mod update or a game update, run the preflight between the two commands
(see "Before", step 2).

## Stop

```
tools\cli host stop
```

It prints `host: backed up saves -> <folder>`. Check it, then say in Discord that the session is
over and when the next one is. If `tools\cli host watch` is running *(planned)*, an exit after
`host stop` ends the watch by itself.

## After

1. **Read the log.** *(planned)* `tools\cli host triage` groups the newest `console.log` by
   normalised text, counts each group and marks it known or new against
   [debug-log-triage](../debug-log-triage.md). Until it merges:

   ```
   tools\cli host status
   Select-String -Path "<log path that host status prints>" -Pattern "SCRIPT \(E\)","VME:","LFDebug:[A-Za-z]+\] .*(Error|Warn)"
   ```

   Anything new and not in the triage page becomes an issue with the lines pasted in.

2. **Rotate the backups.** It keeps the newest 20 plain (start, stop) backups and never touches a
   preflight, wipe, crash or replaced-by-restore backup:

   ```
   tools\cli host prune --dry-run
   tools\cli host prune
   ```

3. **A wipe, only when it was announced 24 hours ago.** It refuses while the host runs, writes a
   `wipe` backup, moves the saves aside (nothing is deleted) and logs the step to
   `backups/wipes.log`:

   ```
   tools\cli host wipe --confirm host
   ```

   `host` is the name of the host folder (the last part of the path `host status` prints as
   `host dir`); the command prints how to restore. Do not wipe before
   the announcement is out.

4. Add a row to the session log in the [session playbook](../playtest/session-playbook.md).

## When it goes wrong

Stop after two tries of a step and ask the maintainer. Do not improvise with the save folder.

| What you see | What to do |
|---|---|
| `REFUSE 'host start' runs in the main checkout` | Run it from a worktree or the playtest checkout, not the main checkout. |
| `FAIL  game update: ...` on `host start` | Run `tools\cli host preflight`. `--force` is for the maintainer only. |
| Preflight fails | It has already restored the backup; do not open the server. Read [game updates](game-updates.md), tell the maintainer. |
| `host: already running` | `tools\cli host status`; if it is the server you want, leave it; otherwise `tools\cli host stop` first. |
| `a test or server run holds port 2001` | Another run owns the port. Wait for it to end, or retry with `--wait`. `host drill` has no `--wait`: it queues for a pool lane itself and boots on that lane's port, never 2001. A drill that still says `lock 'server-2001' is held` is running a tree from before that hand-off: merge `origin/main` and run it again. |
| `FAIL  the server exited during startup` | The command prints the last log lines. Read them with [debug-log-triage](../debug-log-triage.md); try once more; on a second failure stop and report. |
| No `Entered online game state.` within the timeout | The server is still running. `tools\cli host status` for the log path; if it never gets there, `tools\cli host stop` and start again once. |
| `save load damaged` or `continued save never reached the LF store` in the log | Saving is off for the session on purpose. `tools\cli host stop`, then `tools\cli host restore`, and find the cause before opening again. |
| `FAIL  the server did not close within 180s` on stop | It was killed with no shutdown save. `tools\cli host restore` puts the newest backup back and keeps what it replaced. |
| The wrong save is live, or a wipe was a mistake | `tools\cli host stop`, then `tools\cli host restore` (the newest backup) or `tools\cli host restore <backup folder name>`. |
| The host folder is full | `tools\cli host prune`. |
| The server crashed and nothing restarted it | `tools\cli host start --flagship --wait`. The log from before the crash is in the newest `logs` folder under the host profile; keep it and triage it. |
| A planned command is not there | Do the step by hand as written beside it, and write down what you did in the session log. |

When the planned commands merge, slice O16R replaces the *(planned)* names here with the real
ones.
