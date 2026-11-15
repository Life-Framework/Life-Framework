# M4 opening day: the beat sheet for the integration run

The integration run of chapter #1006 (M4 Flagship Live). One agent runs this sheet end to end on `main`
once every slice of the chapter has merged ([roadmap pipeline, section 4](../process/roadmap-pipeline.md)).
It follows one opening day of the flagship, "Life Framework | Everon 1989 (Alpha)": the maintainer's
desk, the first join, the town, the staff tools, one in-game day, the 06:00 and 18:00 restart, a wipe
with notice and the site's next session. Issue: #1425.

Each beat names the slices that prove it (`Proves`), what to do, and what to see. A beat that does not
show what it says is not fixed inside the run: it becomes a new slice card under #1006
(`tools\cli slice new 1006 --plan <file>`), and the run goes on to the next beat. `tools\cli playtest
checklist` reads this sheet after the run, so keep one line per observation.

## Fixed decisions this sheet relies on

| Decision (maintainer) | What it means for a beat |
|---|---|
| The alpha is joined with the server password only | No account list. A join without the password is refused, checked by hand because the password check is backend-only (#1520, B2). |
| Wipes happen with 24 hours' notice | The wipe command refuses without a notice posted 24 hours earlier (B9). |
| Restarts at 06:00 and 18:00 server time, warnings at 15, 5 and 1 minute | The warning is a toast for every player (B7). |
| The in-game day is 2 real hours (12x) | One real minute moves the clock 12 minutes (B6). |
| Only the maintainer is staff | The one administrator account approves applications (B5). |
| Police missions come after opening | No beat plays one. |

## Setup

- Run from a worktree or the playtest checkout, never the main checkout (`host start` refuses there).
  The runbook is [session-runbook.md](../ops/session-runbook.md); the alpha page is
  `docs/community/alpha.md` (#1389, linked here once it is on `main`).
- Two game clients: client A is the administrator account, client B a fresh profile with no account
  on the server. Run the smoke list ([smoke-list.md](smoke-list.md)) first, as `playtest checklist` does.
- Use a scratch host folder (`LF_HOST_DIR`) for anything destructive. Never run the wipe step on the real saves.
- Do not edit the committed `server/flagship/config.json`. A test restart time goes in the profile's own
  copy of the config.
- A command name for a slice that has not merged is the name its card gives and is marked *(planned)* here
  with that card; `tools\cli host help` is the truth. A name that differs is a doc fix, not a failed beat.
  Not on `main` as of 2026-11-15: `host next` (#1416), `host history` (#1409), `host post` (#1414), the
  proof verbs `day-rate` (#1303) and `restart-cycle` (#1268).

Markers: **[two-player]** needs both clients at once, **[restart]** needs the server to stop and start,
**[late-join]** needs a client that joins a world already in progress. Times are minutes of work, not of
waiting. About 70 minutes in all.

## B1 Desk: go or no-go before opening (6 min)

Proves: #1408 (`host ready`), #1407 (disk warning), #1322 (`host config apply`), #1320 (staff roster),
#1261 (restore drill), #1260 (backup rotation).

Do: `tools\cli host config apply`, `tools\cli staff roster check`, `tools\cli host drill`, then
`tools\cli host ready`.

- [ ] `host config apply` writes the profile's `config.json` and `server.json` from `server/flagship/` and the host's
  `secrets.json`; the join password is in the live `server.json` and in no repo file. #1322
- [ ] The roster check passes and names the maintainer as the only administrator. #1320
- [ ] The drill boots the newest backup on a scratch profile and the log shows `save loaded:`. #1261
- [ ] `host ready` prints go. Fill the drive on a scratch host folder past the disk limit and it prints no-go with
  the disk as the reason. #1408, #1407
- [ ] `host prune --dry-run` keeps the newest 20 plain backups and lists no preflight, wipe or crash backup for removal. #1260

## B2 Start and the password gate (5 min)

Proves: #1321 and #1405 (the flagship config and its decisions), #1322, #1291 (the alpha page and its
password-only gate), #1262 (the watchdog).

Do: `tools\cli host start --flagship --wait`, then client A joins from the server browser. The join password
check is backend-only (#1520), so the wrong-password join is a manual check that no script proves.

- [ ] The command ends with `host: up (pid N)` and the join lines; the log has `save loaded:` and `Entered online game state.`. #1262
- [ ] The server is listed as exactly `Life Framework | Everon 1989 (Alpha)`. #1321, #1405
- [ ] **Manual:** a join with a wrong password is refused with the engine's password message. Not scripted (#1520). #1291
- [ ] A join with the right password reaches the world and the character spawns in town. #1291
- [ ] `host watch` is running and `host status` says so. #1262

## B3 The rules board at the plaza (4 min)

Proves: #1279 (the rules and run-your-own cards), #1280 (the boards placed in the town),
#1281 (rules export to the site and Discord).

Do: client A walks to the plaza board and reads it. Open `/rules` on the site and the Discord copy that
`tools\cli rules export` writes (`tmp/rules-discord.md`).

- [ ] The rules board stands at the plaza on the ground, not inside another prop, and reads: server name, the rules
  one line each, the alpha wipe notice. #1280, #1279 [look]
- [ ] The run-your-own board stands beside it and names the framework, the install guide and the config
  generator on life-framework.org. #1280, #1279 [look]
- [ ] The board, `/rules` and the Discord block carry the same rule text. #1281

## B4 [two-player] [late-join] The quiz and a late arrival (8 min)

Proves: #1421 (every quiz question tests a published rule), #1279 and #1281 (the published rules),
#1291 (the join), #1280 (the board the player reads).

Do: client A stays at the plaza. Client B joins with the password and a fresh profile, while A is mid-action
(holding an item, standing at the board). Run the `quiz-proof` proof verb, then the `quiz-rejoin-proof` proof verb (#1490).

- [ ] B gets the rules quiz before the registration form. Every question has a rule on the board it can be
  answered from. #1421
- [ ] A wrong answer shows the rule it tests and lets B try again. #1421
- [ ] A passed quiz leads to the registration form (R-89) and then the world; B spawns in town with a character and cash. #1421, #1291
- [ ] B sees A at the plaza in the state A was left in, not a blank or default one. This is the late-joiner check
  (S6 in the smoke list).
- [ ] B leaves and rejoins: the quiz does not run again for the same account. #1421

## B5 [two-player] A police application approved in the GM panel (10 min)

Proves: #1287 (staff API for applications), #1288 (the Game Master panel), #1289 (the application relay
to staff), #1283 and #1346 (the staff webhook), #1320 (who is staff).

Do: B files an application at the station duty desk. A opens the GM panel. Run the `police-application-proof` proof verb.

- [ ] The desk takes B's application and B gets a receipt toast. The server log has an `[LFDebug:Police]` line for it.
- [ ] A's GM panel lists B's application with the oldest open one first. #1288
- [ ] Approve: B is on the police roster and can clock in. B, who is not staff, cannot approve or list applications,
  and the server logs the refusal. #1288, #1287
- [ ] Reject a second application: B gets the cooldown. #1287
- [ ] With a test webhook URL in the host's `secrets.json`, the staff channel gets a post for the filed application.
  With no URL set, nothing breaks and the log says the post was skipped. #1289, #1283, #1346

## B6 The 2-hour day (10 min)

Proves: #1300 (day cycle config), #1301 (the server applies it), #1303 (proof verb `day-rate`),
#1406 (`host perf`).

Do: note the in-game time on A, wait five real minutes, note it again. Run the `day-rate` proof verb *(planned)* (#1303).
Start `tools\cli host perf` for the length of the beat.

- [ ] Five real minutes move the clock by one in-game hour (12x). The start hour is the config's. #1300, #1301
- [ ] The `day-rate` proof *(planned)* (#1303) passes on a dedicated server. #1303
- [ ] **[late-join]** B leaves and rejoins: the clock reads the same time as A's, within a few seconds. #1301
- [ ] `host perf` printed CPU, memory and upload samples for the beat. #1406

## B7 [restart] The 18:00 warnings and the close (9 min)

Proves: #1264 (restart schedule), #1348 (the ticker publishes the warnings), #1265 (the toast),
#1267 (save and close), #1344 (the events and tickers start).

Do: in the profile's copy of the config, set the next restart about 16 minutes ahead (do not edit the repo
file), restart the server once so it reads it, rejoin both clients, then `tools\cli host next` *(planned)* (#1416).

- [ ] `host next` *(planned)* (#1416) prints the next restart time and the next session. #1264, #1416
- [ ] A and B each get a toast at 15, 5 and 1 minute before the restart, "Server restart in N minutes". Nothing else
  is required of the player at those times. #1265, #1348
- [ ] At the restart time the server saves and closes by itself: the log shows the shutdown save and a restart marker
  for the watchdog. #1267
- [ ] Both clients end back at the menu, not frozen in the world.

## B8 [restart] The clean restart and the reload (7 min)

Proves: #1262 (watchdog), #1267, #1302 (time survives a restart), #1268 (proof verb `restart-cycle`),
#1415 (`host password`).

Do: watch the host after B7's close; rejoin A and B. Then `tools\cli host password` to rotate the join
password and restart once more.

- [ ] The watchdog starts the server again within a minute, with a backup kept; the log shows `save loaded:` and no
  `Unable to find` or `Wrong GUID/name`. #1262
- [ ] A and B rejoin as the same characters with the same cash, items and bank balance (S5 in the smoke list). #1267
- [ ] The in-game time continues from where it was at the close, not from the start hour. #1302
- [ ] `restart-cycle` *(planned)* (#1268) passes: warnings reach a client, the server closes, comes back, loads the save. #1268
- [ ] After the password rotates, the new password joins and the old one is refused. #1415

## B9 Wipe with notice (6 min, desk)

Proves: #1273 (`host announce`), #1413 (a wipe needs a notice 24 hours old), #1274 (the backed-up wipe step),
#1323 (the wipe policy page), #1414 (`host post`).

Do: on the scratch host folder only. Stop the host, then `tools\cli host wipe --confirm <host folder name>`
before any notice; then `tools\cli host announce --wipe --at <time at least 24 hours ahead>`.

- [ ] The wipe refuses while the host runs, and refuses with no notice, saying a notice 24 hours old is needed. #1413, #1274
- [ ] The announce draft says the wipe time, "at least 24 hours' notice" and links the
  [wipe policy](../community/wipe-policy.md); `host post` *(planned)* (#1414) sends it to the test Discord webhook. #1273, #1414, #1323
- [ ] The wipe still refuses until 24 hours after the notice. Once the notice is old enough on the scratch folder it
  writes a `wipe` backup, moves the saves aside (nothing is deleted) and adds a line to `backups/wipes.log`. #1413, #1274
  If the notice cannot be aged on a scratch folder, record "refusal proven, wipe not proven" in the report and
  open a slice card; do not wipe the real saves to prove it.

## B10 The site's next session time (4 min, desk)

Proves: #1273 (writes `sessions.json`), #1347 (its schema), #1298 (the servers page).

Do: after B9's announce, build the site (`website/`) and open the servers page.

- [ ] `website/content/sessions.json` holds the announced time and passes its schema. #1347, #1273
- [ ] The servers page shows that next session time and nothing else about the server: no player count and no live
  status. #1298

## B11 After: the log and the session record (4 min, desk)

Proves: #1269 (`host triage`), #1270 (issue drafts), #1271 and #1272 (`server-doctor`), #1349
(its config report), #1409 (`host history`).

Do: `tools\cli host triage`, `tools\cli server-doctor`, `tools\cli host history` *(planned)* (#1409).

- [ ] Triage groups the session's log into known and new lines against
  [debug-log-triage](../debug-log-triage.md). Each new group has a drafted issue body and the `gh` command, and the
  run files nothing by itself. #1269, #1270
- [ ] `server-doctor` lists the mods that loaded, says the server booted, and shows the effective config with the
  fields that fell back to a default. #1271, #1272, #1349
- [ ] `host history` *(planned)* (#1409) lists the session this run held, with its start, stop and restart. #1409

## Not in this run

- Nothing here films anything, uses the launch trailer or its shots, or needs the restart siren. The trailer
  slices (#1292 to #1295, #1351, #1352, #1422, #1423) have their own sign-off, and the maintainer has not
  signed off any filming. The flagship warns by toast only.
- Police missions come after the alpha opens (held cards: [plan-1006-held.md](../roadmap/m4-slices/plan-1006-held.md)).
- The public opening: `host gate` (#1410) checks the opening gate, and the maintainer removes the password.
  The alpha runs on the password alone.
- #1263 (the crash-loop guard) needs three forced crashes in ten minutes; it is proved by its own selftest and left
  out of this walk.
- Rented-box and Workshop page work (#1276, #1420), and the doc-only slices whose proof is that `tools\cli
  validate` passes: #1275, #1324, #1350, #1417 to #1419, #1424.

## After the run

Report one line per beat: pass, or the new card's number. Beats that name a slice that is not on `main` are
reported as blocked by that slice, not failed. Fill the run's row in the
[session playbook](session-playbook.md). A passed run goes to the maintainer as the list for the batched
playtest; nothing here closes an issue.
