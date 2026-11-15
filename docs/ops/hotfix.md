# Hotfixing the live server

A price is wrong, a wage is too high, the day length needs a nudge. This page is the
route from "the flagship needs a different value" to "it is live", and what to do
instead when the fix is not a value (#1324).

A command marked *(planned)* is in code font as its card names it and has not merged
yet. Run the commands that exist; for a planned one use the manual step beside it.

## Where the live files are

The flagship's `config.json` lives in this repo under `server/flagship/` (decision
2026-11-14), beside `server.json`. A config change is a pull request, so it is reviewed,
checked and recorded in git like any other change. Nobody edits the host's copy by hand. Issue #1321 commits the two files; until it merges only `staff.txt` is there.

**Secrets live only in the host's `secrets.json`**, in the host directory
(`%LOCALAPPDATA%\LifeFramework\host`, or `LF_HOST_DIR`): the join password, the admin
password and the webhook URLs. This repo is public, so none of them go in
`config.json` or `server.json`. `host config apply` merges them in when it writes the
host's files and never prints them.

## Which kind of fix is it?

| The fix is | Route |
|---|---|
| A value in `config.json`: a price, a wage, an autosave interval, the day length | This page: config hotfix |
| A value in `server.json`: the listing name, the mod list, the scenario | This page, same route (the file is in `server/flagship/` too) |
| A password or a webhook URL | Edit the host's `secrets.json`, then step 5 onward. No PR |
| Script, a prefab, a layout, a pack resource | Not a hotfix: [a script change](#when-the-fix-needs-a-script-change) |

If you are not sure, run `tools\cli config-check` on the change (step 2). A key the
server does not read is a warning: ignored, so it is not a config fix.

## The config hotfix

Time: about 10 minutes, and one restart. Do not start it during a session without
telling players first (step 6).

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
   then update the host's checkout of this repo to that commit.
4. **Apply to the host.**

   ```
   tools\cli host config apply
   ```

   *(planned)* (#1322). It writes the profile's `config.json` and `server.json` from
   `server/flagship/`, adds the host's secrets, prints what changed with every secret
   masked, and refuses while the host runs. A secret found in a repo file refuses the run.
   Until it merges, copy `server/flagship/config.json` over
   `<host dir>\profile\profile\LifeFramework\config.json` while the server is stopped, and
   leave `server.json` and the secrets alone.
5. **Check what is on the host.** Run `config-check` on the file the server will read:

   ```
   tools\cli config-check "%LOCALAPPDATA%\LifeFramework\host\profile\profile\LifeFramework\config.json"
   ```

   The two checks agree when the apply wrote what the repo holds. If the host copy
   reports an error the repo copy did not, stop: the file was changed by hand or a
   secret was merged into the wrong key.
6. **Warn, then restart.** The server only picks up `config.json` at boot.
   - Post the restart on Discord (`tools\cli host announce --at <time>` *(planned)*, #1274;
     until then post the time by hand).
   - Players get an in-game toast at each warning lead time once the owner config lists
     restart times (`restarts` in `config.json`; planned in #1264 and #1265). The flagship
     restarts at 06:00 and 18:00 server time, with warnings at 15, 5 and 1 minute and no
     siren; for an unscheduled restart say the time in Discord and restart at it.
   - Back up, stop, start, the commands of the session runbook
     (`docs/ops/session-runbook.md` *(planned)*, #1275):

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
   ```

   *(planned)* (#1271, #1272). It reads the newest log, lists the loaded mods and the effective
   config, and ends in PASS or FAIL. Until it merges use `tools\cli host status` for
   the newest log and `tools\cli host triage` on it: no new error group, and the first-boot
   success lines are there:

   ```
   tools\cli logs "save loaded:"
   tools\cli logs "Entered online game state"
   ```

   Then look at the value in the game: join, and find the price, wage or day length the
   PR said. A fix is not live because the file changed.

## When it goes wrong

| What you see | Do |
|---|---|
| `config-check` reports an error in step 2 | Fix the value in the PR. The server would refuse it and keep the shipped one |
| `host config apply` refuses: the host runs | `tools\cli host stop`, then apply again |
| `host config apply` refuses: a secret in a repo file | Remove it from `server/flagship/`, put it in the host's `secrets.json`, and open a PR that removes it. If it was pushed, treat it as leaked and change it |
| The apply said the password is empty | The host has no `secrets.json` or it has no `game.password`. Players can join without one: stop and fix it before the start |
| The server does not start or `server-doctor` fails | Roll back: `tools\cli host restore`, revert the PR, `host config apply`, start. Read [game updates and saves](game-updates.md) first when the load line is missing |
| The value is not what the PR says in game | The server did not read the file you checked. Look for the newest log with `tools\cli host status`, and `[LFDebug:Config]` lines about a refused value |
| Players report a loss after the restart | `tools\cli host restore` the backup of step 6 and tell them |

## When the fix needs a script change

A bug in a script, a prefab, a layout or a string is not a config value, and the host
cannot apply it from this repo. It goes the long way:

1. Fix it through the normal pipeline: a worktree, a test with its red proof, `wt gate`,
   `wt ship`. Core scripts are in the `LifeFramework` addon and the flagship's own content
   in the `LF_Everon1989` pack (`docs/architecture/core-vs-pack.md`).
2. After it has merged and been play-tested, make a pack release: publish the changed
   mods to the Workshop, as [the release process](../release-process.md) describes.
3. The server downloads the new mod versions on its next start. That is a restart with a
   game-build check, so run it as a session, from the session runbook
   (`docs/ops/session-runbook.md` *(planned)*, #1275), with a backup, and read
   [game updates and saves](game-updates.md) before the start.

Do not carry a script fix as a config key: an unknown key is ignored, and the server
loads the old script anyway.

## Related

- [Checking your owner config](../config/owner-config.md): every `config-check` message.
- [Game updates and saves](game-updates.md): the preflight and the restore.
- [Session playbook](../playtest/session-playbook.md): a coordinated playtest.
- `docs/community/wipe-policy.md`: a wipe is not a hotfix and has its own 24 hour notice.
