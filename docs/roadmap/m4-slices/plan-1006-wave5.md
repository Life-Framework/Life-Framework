# Slices for #1006, wave 5 (2026-11-15).
# `slice list 1006 --ready` was at ready 8, running 26, with about 30 PRs in the ship queue. Every card here starts on
# main as it is today except STAMPHUD, OPENSTEP and OPENRUN, which wait on cards in this plan or on running slices.
# A card that needs the shape of an open PR says "cut from main, merge origin/<branch> in" and owns none of its files.
# Gaps this wave closes (epic acceptance with no card before): #804 clip stamp and guide (STAMP, STAMPHUD, CLIPDOC);
# #310 "a price fix goes live in one warned restart" (HOTFIX); #311 "a dry run of the checklist on lf-testbox"
# (BOXCHECK); #378 the 0grind trial worksheet (TRIAL); #379 the rented-host guide (HOSTGUIDE); #313 "a moderator rules on
# a staged incident using only the guide" and trial moderators on probation (MODDRILL, TRIALMODS); #316 "one session's
# feedback produces linked issues" (FEEDBACK); #555 the opening step (OPENSTEP); the outline's "check CPU and upload on a
# full session" risk (CAPACITY); live log watching (ERRORS, SUMMARY); the opening-day dry run as agent-runnable beat
# scripts (B1DESK, B2PASS, B4QUIZ, B9WIPE, B10SITE, B11AFTER, ODRUN, OPENRUN).
# Contract for the beat scripts: each is `tools/playtest/opening/<beat>.mjs`, runs standalone, prints one line per
# check `BEAT <Bn> <check-id> PASS|FAIL|BLOCKED <reason>` (BLOCKED when the command or slice it needs is not on main),
# and exits 1 only on a FAIL. ODRUN collects them by glob; no central list.
# Maintainer decisions followed: password-only alpha, 24 h wipe notice, no Discord account linking, the site shows the
# next session time only, police missions after opening, the live config in server/flagship, a 2 h in-game day,
# restarts at 06:00 and 18:00 with a toast only, the server name "Life Framework | Everon 1989 (Alpha)", only the
# maintainer is staff, new world props in the debug grid (no card here places one). No town-pass, placement-cleanup,
# video, trailer or filming cards; nothing from M5.
# Created 2026-11-15 under #230: HOTFIX #1487, B1DESK #1488, B2PASS #1489, B4QUIZ #1490, B9WIPE #1492, B10SITE #1493, B11AFTER #1494, ODRUN #1495, BOXCHECK #1496, TRIAL #1497, HOSTGUIDE #1498, CAPACITY #1499, ERRORS #1500, SUMMARY #1501, RBCHECK #1502, OPENRUN #1503, AUTOSTART #1504, B7SETUP #1505; under #231: STAMP #1506, STAMPHUD #1507, CLIPDOC #1508, MODDRILL #1509, TRIALMODS #1510, FEEDBACK #1511, OPENSTEP #1512, RULESSYNC #1513; under #232: PASTSESSION #1514, JOINALPHA #1515.
# #1006 is at GitHub's 100 sub-issue cap: Ops cards go under #230, Staff under #231, Showcase under #232.

# Slice HOTFIX: Proof verb config-hotfix: a price fix goes live in one restart
## Card
- type: tooling
- tier: T0
- model: sonnet
- machines: session
- budget: 45 min / 60 calls
- owns: `agent-box/proofs/config-hotfix-proof.mjs`
- depends: none
- acceptance: proof config-hotfix
- risk: none
- release-blocker: no

## Capability
The #310 acceptance driven by an agent: a session boots with a profile `config.json`, the proof reads one shop price, changes it in the profile copy only, restarts the server, and the store sells at the new price; no Workshop build, no script change.

## Acceptance
`node agent-box/harness.mjs proof config-hotfix` passes on main; with the restart step skipped it fails at "new price". Red first.

## Notes
Build on `config-boot-proof.mjs` and `store-proof.mjs`; reuse their helpers, edit neither. Never edit `server/flagship/config.json`. One session, `session down` straight after.

# Slice B1DESK: Opening-day beat script B1, the desk go or no-go
## Card
- type: tooling
- tier: T0
- model: sonnet
- machines: none
- budget: 35 min / 45 calls
- owns: `tools/playtest/opening/b1-desk.mjs`, `tools/validation/opening-b1-selftest.mjs`, `tools/validation/fixtures/opening-b1/**`
- depends: none
- acceptance: fixture tools/validation/opening-b1-selftest.mjs
- risk: none
- release-blocker: no

## Capability
`node tools/playtest/opening/b1-desk.mjs --host <scratch dir>` runs beat B1 of `docs/playtest/m4-opening-day.md` on a scratch host: config apply, roster check, prune dry run, ready; one `BEAT B1` line per checkbox, BLOCKED for a command not on main.

## Acceptance
Selftest: a fixture host gives PASS for the roster (maintainer the only administrator) and the prune dry run (no tagged backup listed), and BLOCKED for each command `tools\cli host help` does not list. Red first.

## Notes
Line contract in the plan header. Call commands through `tools/cli.mjs` as child processes with a timeout; never the real host folder (`LF_HOST_DIR`). Read only towards `server/flagship/`.

# Slice B2PASS: Proof verb password-gate
## Card
- type: tooling
- tier: T0
- model: sonnet
- machines: session
- budget: 45 min / 60 calls
- owns: `agent-box/proofs/password-gate-proof.mjs`
- depends: none
- acceptance: proof password-gate
- risk: none
- release-blocker: no

## Capability
Beat B2: a session whose `server.json` has a test join password refuses a client with a wrong password (the engine's message in the log) and lets one in with the right one.

## Acceptance
`node agent-box/harness.mjs proof password-gate` passes on main; with the password removed from the test `server.json` it fails at "refused". Red first.

## Notes
The test password goes in the session's own profile copy, never in a committed file (SECRETS #1463 checks). If the harness cannot pass a join password to a client, ship what passes and SPLIT with a harness card; do not edit `agent-box/harness.mjs` in this slice.

# Slice B4QUIZ: Proof verb quiz-rejoin
## Card
- type: tooling
- tier: T0
- model: sonnet
- machines: session-2p
- budget: 45 min / 60 calls
- owns: `agent-box/proofs/quiz-rejoin-proof.mjs`
- depends: none
- acceptance: proof quiz-rejoin
- risk: none
- release-blocker: no

## Capability
Beat B4 of chapter #1006 ("a new arrival passes the rules quiz"): client 2 joins late on a fresh profile while client 1 holds an item at the plaza, passes the quiz, registers and spawns; it sees client 1 holding the item; it leaves and rejoins and gets no second quiz.

## Acceptance
`node agent-box/harness.mjs proof quiz-rejoin` passes on main; with the quiz-passed flag cleared before the rejoin it fails at "no second quiz". Red first.

## Notes
Build on `quiz-proof.mjs` and `rejoin-proof.mjs`; reuse, do not edit. Logs are the proof; no pictures.

# Slice B9WIPE: Opening-day beat script B9, wipe with notice
## Card
- type: tooling
- tier: T0
- model: sonnet
- machines: none
- budget: 35 min / 45 calls
- owns: `tools/playtest/opening/b9-wipe.mjs`, `tools/validation/opening-b9-selftest.mjs`, `tools/validation/fixtures/opening-b9/**`
- depends: none
- acceptance: fixture tools/validation/opening-b9-selftest.mjs
- risk: none
- release-blocker: no

## Capability
Beat B9 on a scratch host copy: the wipe refuses while the host runs and with no 24 h notice; `host announce --wipe` drafts a notice with the wipe-policy link; with the notice aged in the scratch copy the wipe writes a `wipe` backup, moves the saves aside and logs to `wipes.log`.

## Acceptance
Selftest on a fixture host: every check is PASS, or BLOCKED naming #1413 while the notice gate is not on main. Red first.

## Notes
Never the real host folder: the script copies the fixture into `tmp/` and refuses any `--host` outside `tmp/`. Ageing the notice means rewriting its timestamp in the scratch copy only.

# Slice B10SITE: Opening-day beat script B10, the site's next session
## Card
- type: tooling
- tier: T0
- model: sonnet
- machines: none
- budget: 30 min / 40 calls
- owns: `tools/playtest/opening/b10-site.mjs`, `tools/validation/opening-b10-selftest.mjs`
- depends: none
- acceptance: fixture tools/validation/opening-b10-selftest.mjs
- risk: none
- release-blocker: no

## Capability
Beat B10: `sessions.json` passes its schema, the built site's servers page shows that time, and the page holds no player count and no live status words.

## Acceptance
Selftest: a fixture `sessions.json` and a fixture built page pass; a page with "players online" or a status badge fails; a schema-invalid file fails. Red first.

## Notes
Read the prerendered `website/dist/servers/index.html` when present, else BLOCKED "site not built"; never run the site build inside the selftest.

# Slice B11AFTER: Opening-day beat script B11, the log and the session record
## Card
- type: tooling
- tier: T0
- model: sonnet
- machines: none
- budget: 30 min / 40 calls
- owns: `tools/playtest/opening/b11-after.mjs`, `tools/validation/opening-b11-selftest.mjs`, `tools/validation/fixtures/opening-b11/**`
- depends: none
- acceptance: fixture tools/validation/opening-b11-selftest.mjs
- risk: none
- release-blocker: no

## Capability
Beat B11 on a fixture session log: triage groups known and new lines and drafts without filing, server-doctor reports mods and boot, host history lists the session; BLOCKED for each command not on main.

## Acceptance
Selftest: the fixture log gives PASS for triage and its drafts (no `gh` run), BLOCKED for `host history` while #1409 is open. Red first.

## Notes
Copy a real server console log tail into the fixture (`tools\cli logs`), secrets stripped.

# Slice ODRUN: playtest opening runs the beat scripts and writes the report
## Card
- type: tooling
- tier: T0
- model: sonnet
- machines: none
- budget: 35 min / 45 calls
- owns: `tools/commands/opening.mjs`, `tools/validation/opening-run-selftest.mjs`, `tools/validation/fixtures/opening-run/**`
- depends: none
- acceptance: fixture tools/validation/opening-run-selftest.mjs
- risk: none
- release-blocker: no

## Capability
`tools\cli opening [--beats B1,B9] [--host <scratch>]` runs every `tools/playtest/opening/*.mjs` (glob, no list), collects the `BEAT` lines and writes `tmp/playtest/opening-<date>.md` with one line per beat (pass, fail, or blocked by #N) in the shape the beat sheet's "After the run" asks for; game beats with a proof verb are listed with the `harness.mjs proof <verb>` line to run.

## Acceptance
Selftest: two fixture beat scripts (one pass, one blocked) give the expected report and exit 0; a fixture FAIL gives exit 1. Red first.

## Notes
Line contract in the plan header. Check `tools\cli help` that `opening` is free.

# Slice BOXCHECK: host boxcheck measures a machine against the 40-player box spec
## Card
- type: tooling
- tier: T0
- model: sonnet
- machines: none
- budget: 40 min / 50 calls
- owns: `tools/host/boxcheck.mjs`, `tools/host/commands/boxcheck.mjs`, `tools/validation/host-boxcheck-selftest.mjs`
- depends: none
- acceptance: fixture tools/validation/host-boxcheck-selftest.mjs
- risk: none
- release-blocker: no

## Capability
#311's dry run: `tools\cli host boxcheck [--ssh <pool machine>]` reads cores, clock, RAM, free disk and the game install, compares them with the spec in `docs/ops/rented-box.md`, and prints one line per item and a verdict; on `lf-testbox` it is the checklist's dry run.

## Acceptance
Selftest: injected machine facts give pass, a short-RAM fail and a missing-install fail with one line each. Red first.

## Notes
Every ssh call bounded (`timeout`); read only. Take the spec numbers from `rented-box.md` (#1276) and quote them; if the page has none, put them in the command's help and say so in the PR.

# Slice TRIAL: The 0grind trial worksheet for the maintainer
## Card
- type: docs
- tier: T0
- model: haiku
- machines: none
- budget: 30 min / 30 calls
- owns: `docs/ops/host-trial.md`
- depends: none
- acceptance: fixture website/scripts/check-links.mjs
- risk: none
- release-blocker: no

## Capability
#378 is a human trial; this is its worksheet: each requirement (mod install by Workshop ID, file access to the profile, startup parameters, persistence across a restart, ports, 40-player CPU) with how to check it in the 7-day trial, the log line or file that proves it, and a pass/fail box.

## Acceptance
Links resolve; every check names the repo command or log line it uses (`host ports`, `host listing`, `server-doctor`, `save loaded:`).

## Notes
Promise nothing about 0grind the public page does not say (m4-outline "Host and server DX").

# Slice HOSTGUIDE: Owner guide for a rented game-server host
## Card
- type: docs
- tier: T0
- model: haiku
- machines: none
- budget: 30 min / 30 calls
- owns: `docs/owner/rented-host.md`
- depends: none
- acceptance: fixture website/scripts/check-links.mjs
- risk: none
- release-blocker: no

## Capability
#379's per-host template: install the framework and a pack on a rented host panel by Workshop ID, where `config.json` and `server.json` go, startup parameters, the success lines, backups; 0grind named as the candidate with each step marked "unverified until #378".

## Acceptance
Links resolve; `validate-core-place-names` passes (no Everon names); it links `install.md` and `log-lines.md` instead of repeating them.

# Slice CAPACITY: host capacity estimates how many players the box carries
## Card
- type: tooling
- tier: T0
- model: sonnet
- machines: none
- budget: 40 min / 50 calls
- owns: `tools/host/capacity.mjs`, `tools/host/commands/capacity.mjs`, `tools/validation/host-capacity-selftest.mjs`, `tools/validation/fixtures/host-capacity/**`
- depends: none
- acceptance: fixture tools/validation/host-capacity-selftest.mjs
- risk: none
- release-blocker: no

## Capability
The outline's risk "check CPU and upload on a full session before announcing a large one": `tools\cli host capacity` reads `<host>/perf/*.csv` (#1406) and the `[LFDebug:Perf] players=N fps=F frameMs=M entities=E` lines (PERFLOG #1452's format) and prints CPU and upload per player and the estimated player count before the 40-player budget is hit.

## Acceptance
Selftest: a fixture CSV and log at 10 players give the expected estimate; no perf lines gives a clear "no data" exit 2. Red first.

## Notes
Read the CSV header from `tools/host/perf.mjs`; do not edit it. The perf line format is fixed by the #1452 card; parse it, do not depend on its branch.

# Slice ERRORS: host errors follows the live log for errors only
## Card
- type: tooling
- tier: T0
- model: sonnet
- machines: none
- budget: 30 min / 40 calls
- owns: `tools/host/errors.mjs`, `tools/host/commands/errors.mjs`, `tools/validation/host-errors-selftest.mjs`
- depends: none
- acceptance: fixture tools/validation/host-errors-selftest.mjs
- risk: none
- release-blocker: no

## Capability
During a session, `tools\cli host errors [--for <min>]` follows the newest console log and prints only `[LFDebug:*]` errors, `VME:` and `SCRIPT (E)` lines, each marked known or new against `docs/debug-log-triage.md`, so the operator sees a problem before players report it.

## Acceptance
Selftest: a fixture log grown in three writes prints the two error lines once each, marks one known, and ends after `--for`. Red first.

## Notes
Reuse the matcher from `tools/host/commands/triage.mjs` by import; do not edit it. Bounded by `--for` (default 60 min).

# Slice SUMMARY: host summary writes one post-session report
## Card
- type: tooling
- tier: T0
- model: sonnet
- machines: none
- budget: 40 min / 50 calls
- owns: `tools/host/summary.mjs`, `tools/host/commands/summary.mjs`, `tools/validation/host-summary-selftest.mjs`, `tools/validation/fixtures/host-summary/**`
- depends: none
- acceptance: fixture tools/validation/host-summary-selftest.mjs
- risk: none
- release-blocker: no

## Capability
`tools\cli host summary [<console.log>]` writes `<host>/sessions/<stamp>.md`: length, unique players and peak (attendance), restarts and crashes, the triage counts known and new, and the perf verdict; the runbook's "after" step and the evidence for #555's "sessions at the player target".

## Acceptance
Selftest: a fixture log and perf CSV give the expected summary file. Red first.

## Notes
Cut from main, merge `origin/ws/slice-1459` (attendance, PR #1477) in, owning none of its files; if it has merged, use main. Calls the others' exported functions, never their CLI text.

# Slice STAMP: The clip stamp text and its config switch
## Card
- type: system
- tier: T2
- model: sonnet
- machines: test
- budget: 40 min / 50 calls
- owns: `addons/LifeFramework/Scripts/Game/HUD/LF_ClipStamp.c`, `addons/LifeFramework/Scripts/Game/Core/Config/LF_ClipStampConfig.c`, `addons/LifeFramework/Scripts/Game/Tests/LF_Test_ClipStamp.c`
- depends: none
- acceptance: test LF_Test_ClipStamp
- risk: none
- release-blocker: no

## Capability
#804's formatter: `LF_ClipStamp.Format(serverShort, uid, worldTime)` gives "<server short> | <last 6 of the uid> | 1989-MM-DD HH:MM" from the world clock the incident log uses; a server config section `clipStamp` (`enabled`, `serverShort`) is off by default.

## Acceptance
`LF_Test_ClipStamp`: a fixed uid and world time give the exact text; a uid shorter than 6 characters is used whole; disabled gives empty. Red first.

## Notes
Check which clock `LF_IncidentLog` keys rows by and use that one. Config class beside the M4 sections (#1343); regenerate the schema (merge driver). No widget in this slice: STAMPHUD draws it.

# Slice STAMPHUD: The clip stamp in the corner of the screen
## Card
- type: visual
- tier: T2
- model: sonnet
- machines: full-profile
- budget: 45 min / 60 calls
- owns: `addons/LifeFramework/Scripts/Game/HUD/LF_ClipStampHud.c`, `addons/LifeFramework/UI/Layouts/HUD/LF_ClipStamp.layout*`, `addons/LifeFramework/Prefabs/Characters/Core/Character_Base.et`
- depends: STAMP
- acceptance: test LF_Test_ClipStamp
- risk: none
- release-blocker: no

## Capability
With `clipStamp.enabled`, every player sees the stamp bottom right at 60 percent opacity, ticked once a second; off, nothing is drawn.

## Acceptance
`LF_Test_ClipStamp` still passes; one `--profile full` session picture of the stamp in the PR (a HUD widget, so a session shot is allowed). Red first: the picture before shows no stamp.

## Notes
Pattern: `HUD/LF_LawlessHud.c` and its layout; the HUD list is on `Character_Base.et` (add one entry by GUID, change nothing else). UI kit parts only (`validate-ui-kit`).

# Slice CLIPDOC: How to record a clip for a report
## Card
- type: docs
- tier: T0
- model: haiku
- machines: none
- budget: 30 min / 30 calls
- owns: `docs/community/clip-guide.md`
- depends: none
- acceptance: fixture website/scripts/check-links.mjs
- risk: none
- release-blocker: no

## Capability
#804's guide: record the whole screen with the stamp visible, keep the link up a week, report with the `/report` incident number; short PC, Xbox and PlayStation steps; the server's incident log covers most cases, so a clip is a backup.

## Acceptance
Links resolve; the stamp is described as "planned" until STAMPHUD is on main; no Discord account linking is mentioned.

# Slice MODDRILL: The staged incident a trial moderator rules on
## Card
- type: docs
- tier: T0
- model: haiku
- machines: none
- budget: 30 min / 30 calls
- owns: `docs/community/moderator-drill.md`
- depends: none
- acceptance: fixture website/scripts/check-links.mjs
- risk: none
- release-blocker: no

## Capability
#313's acceptance as a script the maintainer can run with a trial moderator: two players stage an incident (damage, `/report`), the trial moderator rules using only `moderator-guide.md`, and a scoring sheet says what a correct ruling logs.

## Acceptance
Links resolve; every step names the real command or GM action from `moderator-guide.md`; no ban step (the ban record #1285 is still open).

## Notes
`proof incident-handled` (#1451) drives the same incident for agents; link it as planned until merged.

# Slice TRIALMODS: Trial moderators: recruitment and probation
## Card
- type: docs
- tier: T0
- model: haiku
- machines: none
- budget: 30 min / 30 calls
- owns: `docs/community/trial-moderators.md`
- depends: none
- acceptance: fixture website/scripts/check-links.mjs
- risk: none
- release-blocker: no

## Capability
The 2026-11-09 decision written out: trial moderators come from the playtesters, on probation; how one is asked, the drill they pass, how long probation lasts, how they are added to `server/flagship/staff.txt` later (`tools\cli staff add`, #1466), and that at opening only the maintainer is staff.

## Acceptance
Links resolve; the page changes no roster entry and says the roster holds only the maintainer today.

# Slice FEEDBACK: host feedback drafts issues from a session's feedback
## Card
- type: tooling
- tier: T0
- model: sonnet
- machines: none
- budget: 35 min / 45 calls
- owns: `tools/host/feedback.mjs`, `tools/host/commands/feedback.mjs`, `tools/validation/host-feedback-selftest.mjs`, `tools/validation/fixtures/host-feedback/**`
- depends: none
- acceptance: fixture tools/validation/host-feedback-selftest.mjs
- risk: none
- release-blocker: no

## Capability
#316's "one session's feedback produces linked issues": `tools\cli host feedback <export.txt>` reads a copied Discord feedback channel, groups messages per report, drops thanks and duplicates, writes one issue body per report in the repo's bug or feature template under `tmp/feedback/<date>/`, and prints the `gh issue create` commands. It files nothing.

## Acceptance
Selftest: a fixture export with five messages (two the same bug, one thanks) gives three drafts. Red first.

## Notes
Templates: `.github/ISSUE_TEMPLATE/`. Same never-run rule as `host triage --draft`. Strip Discord user ids from the drafts.

# Slice OPENSTEP: host open removes the alpha password once the gate passes
## Card
- type: tooling
- tier: T0
- model: sonnet
- machines: none
- budget: 35 min / 45 calls
- owns: `tools/host/open.mjs`, `tools/host/commands/open.mjs`, `tools/validation/host-open-selftest.mjs`
- depends: #1410, #1322
- acceptance: fixture tools/validation/host-open-selftest.mjs
- risk: none
- release-blocker: no

## Capability
#555's opening step: `tools\cli host open --confirm <host dir name>` refuses unless `host gate` passes, then clears the join password in the host's `secrets.json`, reapplies the config, and drafts the opening announcement; `--dry-run` prints the steps.

## Acceptance
Selftest: a failing gate refuses; a passing gate clears the password in a fixture secrets file and nothing else. Red first.

## Notes
Maintainer-only step; agents run only `--dry-run` on a scratch host.

# Slice PASTSESSION: The servers page hides a session that has passed
## Card
- type: visual
- tier: T0
- model: sonnet
- machines: none
- budget: 30 min / 40 calls
- owns: `website/src/pages/Servers.tsx`
- depends: none
- acceptance: fixture website/scripts/check-routes.mjs
- risk: none
- release-blocker: no

## Capability
When the announced session in `sessions.json` is in the past, the servers page says "No session announced yet: watch Discord" instead of an old time, and shows the time in the visitor's own zone with the server zone beside it. Still the next session time only.

## Acceptance
The site builds and the route check passes; a picture of the page with a past and a future fixture session in the PR.

## Notes
ALPHAPAGE (#1468) reuses this page's session reader: keep its export name and shape.

# Slice RULESSYNC: validate fails when the published rules drift
## Card
- type: tooling
- tier: T0
- model: sonnet
- machines: none
- budget: 30 min / 40 calls
- owns: `tools/validation/validate-rules-sync.mjs`
- depends: none
- acceptance: fixture tools/validation/validate-rules-sync.mjs
- risk: none
- release-blocker: no

## Capability
Beat B3's "board, /rules and the Discord block carry the same text": a validate check runs `rules export` into a temp folder and fails when `website/content/rules.json` differs, so the board strings and the site never drift.

## Acceptance
`validate-rules-sync.mjs --selftest`: a changed rule string with a stale `rules.json` fails naming the rule; the real tree passes. Red first.

## Notes
Import the exporter from `tools/commands/rules.mjs`; do not edit it. Declare inputs (the pack string table, `rules.json`) so `validate` can skip it.

# Slice RBCHECK: validate checks the ops pages name real commands
## Card
- type: tooling
- tier: T0
- model: sonnet
- machines: none
- budget: 35 min / 45 calls
- owns: `tools/validation/validate-ops-commands.mjs`, `tools/validation/fixtures/ops-commands/**`
- depends: none
- acceptance: fixture tools/validation/validate-ops-commands.mjs
- risk: none
- release-blocker: no

## Capability
"A second person can run a session from the runbook" stays true: every `tools\cli <cmd> [<sub>]` in `docs/ops/*.md` and `docs/playtest/m4-opening-day.md` exists (`tools/commands/*.mjs`, `tools/host/commands/*.mjs`, the host core verbs) or is in code font marked *(planned)*.

## Acceptance
`--selftest`: a fixture page naming a real command, a planned one and a missing one gives pass, pass, fail. On the real tree it warns (not fails) while wave-4 commands are still open; say so in the PR. Red first.

## Notes
#1350 fixes the pages by hand; this keeps them right afterwards.

# Slice OPENRUN: The opening-day integration run on main
## Card
- type: tooling
- tier: T0
- model: sonnet
- machines: session-2p
- budget: 45 min / 60 calls
- owns: `docs/playtest/m4-opening-day-run.md`
- depends: ODRUN, B1DESK, B2PASS, B4QUIZ, B9WIPE, B10SITE, B11AFTER, HOTFIX, #1268, #1303, #1450, #1451
- acceptance: proof restart-cycle
- risk: none
- release-blocker: no

## Capability
Chapter #1006's integration run (roadmap pipeline section 4): `tools\cli opening` for the desk beats, then each game beat's proof verb on one two-client session, one line per beat in `m4-opening-day-run.md`: pass, or the number of a new card filed with `slice new 1006 --attach <epic>`. Nothing is fixed inside the run.

## Acceptance
Every beat has a line; every FAIL has a card; `proof restart-cycle` was run in it.

## Notes
The beat sheet is `docs/playtest/m4-opening-day.md`. Pictures only where a beat says [look]. This is the run whose report goes to the maintainer as the batched playtest list.

# Slice AUTOSTART: host autostart brings the PC host back after a reboot
## Card
- type: tooling
- tier: T0
- model: sonnet
- machines: none
- budget: 35 min / 45 calls
- owns: `tools/host/autostart.mjs`, `tools/host/commands/autostart.mjs`, `tools/validation/host-autostart-selftest.mjs`
- depends: none
- acceptance: fixture tools/validation/host-autostart-selftest.mjs
- risk: none
- release-blocker: no

## Capability
The watchdog covers a crashed server, not a rebooted PC. `tools\cli host autostart install|remove|status` registers a Windows scheduled task that runs `host watch --flagship` at logon for the host user, hidden (`tools/lib/quiet-launch`); `status` says whether it is registered and last ran.

## Acceptance
Selftest: with an injected `schtasks` runner, install builds the exact task arguments, a second install is a no-op, remove deletes it, status parses a fixture query. Red first.

## Notes
The selftest never registers a real task. Refuses in the main checkout like `host start`. Every `schtasks` call has a timeout.

# Slice JOINALPHA: The join page says how to get into the alpha
## Card
- type: visual
- tier: T0
- model: haiku
- machines: none
- budget: 30 min / 30 calls
- owns: `website/src/pages/Join.tsx`
- depends: none
- acceptance: fixture website/scripts/check-routes.mjs
- risk: none
- release-blocker: no

## Capability
The join page's flagship block says the server is "Life Framework | Everon 1989 (Alpha)", joined with a password asked for in the Discord channel, with progress wiped on 24 hours' notice; it links the wipe policy and keeps the run-your-own block (#1299).

## Acceptance
The site builds and the route check passes; one picture of the block in the PR. No live status, no player count, no account linking.

## Notes
Link `/alpha` only once ALPHAPAGE (#1468) is on main; until then link `docs/community/alpha.md` through the docs route.

# Slice B7SETUP: Opening-day helper: set a test restart N minutes ahead
## Card
- type: tooling
- tier: T0
- model: sonnet
- machines: none
- budget: 30 min / 40 calls
- owns: `tools/playtest/opening/b7-restart-setup.mjs`, `tools/validation/opening-b7-selftest.mjs`
- depends: none
- acceptance: fixture tools/validation/opening-b7-selftest.mjs
- risk: none
- release-blocker: no

## Capability
Beat B7's setup step as a command: `node tools/playtest/opening/b7-restart-setup.mjs --profile <dir> --in 16` writes the next restart time into the profile's own `config.json` restart section (never `server/flagship/`), keeps the 15, 5 and 1 minute warnings, and prints `BEAT B7 setup PASS <time>`; `--undo` restores the file.

## Acceptance
Selftest: a fixture profile config gets the expected restart entry and `--undo` gives back the original bytes; a path under `server/flagship/` is refused. Red first.

## Notes
The restart section's shape is the M4 config section from #1343 (`Feature/Ops/LF_RestartConfig.c`, `LF_Test_M4ConfigSections`); read it, change no script.
