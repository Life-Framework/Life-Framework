# Hotfixing the live server

A price is wrong, a wage is too high, the day length needs a nudge. This page is the
route from "the flagship needs a different value" to "it is live", and what to do
instead when the fix is not a value (#1324, #310).

The flagship runs on a rented 0grind server (service `298627`), so the live route is
[the 0grind one](#the-config-hotfix-on-0grind). A host on your own PC uses
[the local one](#the-config-hotfix-on-a-local-host). Steps 1 to 3 are the same for both.

## Where the live files are

The flagship's `config.json` lives in this repo under `server/flagship/`, beside
`server.json` (decision 2026-11-14). A config change is a pull request, so it is
reviewed, checked and recorded in git like any other change. Nobody edits the host's copy
by hand, and never through the 0grind panel's Config page.

**Secrets live only in the host's `secrets.json`**, in the host directory
(`%LOCALAPPDATA%\LifeFramework\host`, or `LF_HOST_DIR`; for 0grind
`%LOCALAPPDATA%\LifeFramework\host-0grind`): the join password, the admin password and the
webhook URLs. This repo is public, so none of them go in `config.json` or `server.json`.
`host config apply` and `host remote 0grind config push` merge them in when they write the
host's files and never print them.

## Which kind of fix is it?

| The fix is | Route |
|---|---|
| A value in `config.json`: a price, a wage, an autosave interval, the day length | This page: config hotfix |
| A value in `server.json`: the listing name, the mod list, the scenario | This page, same route (the file is in `server/flagship/` too) |
| A password or a webhook URL | Edit the host's `secrets.json`, then the push or apply and restart steps. No PR |
| Script, a prefab, a layout, a pack resource | Not a hotfix: [a script change](#when-the-fix-needs-a-script-change) |

If you are not sure, run `tools\cli config-check` on the change (step 2). A key the
server does not read is a warning: ignored, so it is not a config fix.

## Steps 1 to 3: the PR (both hosts)

Time: about 10 minutes of work, and one restart.

1. **Edit in a PR.** In a worktree (`tools\cli wt new hotfix-<what>`) change
   `server/flagship/config.json`. Change only the value you mean to. The host's
   `secrets.json` is never touched here and a password never appears in the diff.
2. **Check it offline.**

   ```
   tools\cli config-check server\flagship\config.json
   ```

   Exit code 0 is good. An error means the server would refuse the value and keep the
   shipped one: fix it before the PR goes up. Read
   [Checking your owner config](../config/owner-config.md) for the problem lines, and
   the `Migrations:` block if you also changed `schemaVersion`.
3. **Ship it.** `tools\cli wt ship hotfix-<what> --refs <issue>`, with a `## Manual
   testing` section that names the value and where a player sees it. Merge to `main`,
   then update the checkout the host commands run from to that commit.

## The config hotfix on 0grind

The server reads `config.json` at boot, so the fix is live after one restart. The cheapest
warned restart is the next scheduled one: the flagship closes itself at 06:00 and 18:00
server time and the mod warns every player 15, 5 and 1 minute ahead (the toast needs no
action from you). `tools\cli host next` prints the time and the warnings.

4. **Push the file.** The first command is a dry run: it prints the diff against the box and
   writes nothing. Read it, then add `--yes`.

   ```
   tools\cli host remote 0grind config push
   tools\cli host remote 0grind config push --yes
   ```

   It keeps 0grind's ports, RCON block and slot count, and a secret found in a repo file
   refuses the run. A file it wrote still needs a restart to take effect.
5. **Back up.** Pull the saves off the box before the restart:

   ```
   tools\cli host remote 0grind saves pull
   ```

6. **Warn, then restart.**
   - Wait for the scheduled restart, or, if the fix cannot wait, post the time on Discord
     (`tools\cli host announce --at <time>`, then `tools\cli host post session --send`) and
     restart at it with `tools\cli host remote 0grind restart --yes`. There is no in-game
     message for an unscheduled restart on 0grind: `host say` queues a file in a local
     profile, and RCON is read-only (monitor) on the flagship. Say it in Discord and keep
     the time.
   - A scheduled close is followed by the panel Workflow that starts the server again at
     06:02 and 18:02 ([the 0grind section](../owner/host-the-flagship.md#workflows-the-0600-and-1800-restarts)).
7. **Verify.** Pull the log and read it:

   ```
   tools\cli host remote 0grind logs pull
   tools\cli server-doctor %LOCALAPPDATA%\LifeFramework\host-0grind\profile
   tools\cli host triage
   ```

   `server-doctor` ends in PASS and lists the effective config, so the changed value shows
   there. `host triage` shows no new error group. Then join and look at the value the PR
   said (the price on the shelf, the wage on the slip). A fix is not live because the file
   changed.

## The config hotfix on a local host

For a host on your own PC (a scratch host, or the maintainer's box before the move).

4. **Apply to the host.** It refuses while the host runs, so stop first (step 6).

   ```
   tools\cli host config apply
   ```

   It writes the profile's `config.json` and `server.json` from `server/flagship/`, adds
   the host's secrets, and prints what changed with every secret masked. A secret found in
   a repo file refuses the run.
5. **Check what is on the host.** Run `config-check` on the file the server will read:

   ```
   tools\cli config-check "%LOCALAPPDATA%\LifeFramework\host\profile\profile\LifeFramework\config.json"
   ```

   The two checks agree when the apply wrote what the repo holds. If the host copy
   reports an error the repo copy did not, stop: the file was changed by hand or a
   secret was merged into the wrong key.
6. **Warn, then restart.** The server only picks up `config.json` at boot. Post the
   restart on Discord (`tools\cli host announce --at <time>`; `--build` is needed only
   while the host has no save, see the [session runbook](session-runbook.md)). The mod
   warns players in game at 15, 5 and 1 minute before each time in `restarts`, so a scheduled
   restart needs nothing more; for an unscheduled one say the time in Discord and restart at
   it. Then, with the commands of the session runbook:

   ```
   tools\cli host backup
   tools\cli host stop
   tools\cli host config apply
   tools\cli host start --flagship
   ```

   `host stop` writes the shutdown save and `host start` backs up again first, so
   the backup line is the one you keep when something goes wrong. Order matters: stop,
   apply, then start, because the apply refuses while the host runs.
7. **Verify.**

   ```
   tools\cli server-doctor <host dir>\profile
   tools\cli host triage
   ```

   `server-doctor` reads the newest log, lists the loaded mods and the effective config,
   and ends in PASS or FAIL. `host triage` shows no new error group. Then look at the value
   in the game: join, and find the price, wage or day length the PR said.

## When it goes wrong

| What you see | Do |
|---|---|
| `config-check` reports an error in step 2 | Fix the value in the PR. The server would refuse it and keep the shipped one |
| `config push` or `host config apply` refuses: a secret in a repo file | Remove it from `server/flagship/`, put it in the host's `secrets.json`, and open a PR that removes it. If it was pushed, treat it as leaked and change it |
| `host config apply` refuses: the host runs | `tools\cli host stop`, then apply again |
| The apply or push said the password is empty | The host has no `secrets.json` or it has no `game.password`. Players can join without one: fix it before the restart |
| The server does not start or `server-doctor` says FAIL | Roll back: revert the PR, push (0grind) or apply (local) the old file, restart. If the save is the problem, `tools\cli host restore` the backup of step 5 or 6 on a local host, and read [game updates and saves](game-updates.md) first when the load line is missing |
| The value is not what the PR says in game | The server did not read the file you checked. Look at `[LFDebug:Config]` lines about a refused value in the pulled log |
| Players report a loss after the restart | Restore the backup of step 5 or 6 and tell them |

## When the fix needs a script change

A bug in a script, a prefab, a layout or a string is not a config value, and the host
cannot apply it from this repo. It goes the long way:

1. Fix it through the normal pipeline: a worktree, a test with its red proof, `wt gate`,
   `wt ship`. Core scripts are in the `LifeFramework` addon and the flagship's own content
   in the `LF_Everon1989` pack (`docs/architecture/core-vs-pack.md`).
2. After it has merged and been play-tested, make a pack release: publish the changed
   mods to the Workshop, as [the release process](../release-process.md) describes.
3. The server downloads the new mod versions on its next start. That is a restart with a
   game-build check, so run it as a session, from the [session runbook](session-runbook.md),
   with a backup, and read [game updates and saves](game-updates.md) before the start.

Do not carry a script fix as a config key: an unknown key is ignored, and the server
loads the old script anyway.

## Related

- [Checking your owner config](../config/owner-config.md): every `config-check` message.
- [Game updates and saves](game-updates.md): the preflight and the restore.
- [Session playbook](../playtest/session-playbook.md): a coordinated playtest.
- `docs/community/wipe-policy.md`: a wipe is not a hotfix and has its own 24 hour notice.
