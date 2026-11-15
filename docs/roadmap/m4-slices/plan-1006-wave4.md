# Slices for #1006, wave 4 (2026-11-15).
# The ready queue had run down to one card with 18 running. Every card here starts on main as it is today, except
# RECRUIT, JOINNOTE and JNCFG, which wait on one small contract (JOINEV) or on #1405. A card that needs the shape of an
# unmerged PR says "cut from main, merge origin/<branch> in"; it never owns a file that PR touches.
# Maintainer, 2026-11-15: no town-pass or placement-cleanup cards (he fixes the existing placements by hand; the grid
# rule and the world-zones guard cover new ones), and no video, trailer, filming or storyboard cards. No card in this
# wave places a world prop; a proof that needs a spot uses the debug grid.
# Decisions followed: password-only alpha, 24 h wipe notice, no Discord account linking, the site shows the next session
# time only, police missions after opening, no town-grid redo, the live config in the repo, a 2 h in-game day,
# restarts at 06:00 and 18:00 with a toast only, the server name "Life Framework | Everon 1989 (Alpha)", only the
# maintainer is staff.
# Created 2026-11-15: JOINEV #1446, RECRUIT #1447, JOINNOTE #1448, JNCFG #1449, PRECRUIT #1450, PINCIDENT #1451, PERFLOG #1452, SAY #1453, VERIFY #1454, OFFSITE #1455, LOGARCH #1456, CRASHPACK #1457, APPS #1458, ATTEND #1459, PORTS #1460, LISTING #1461, DRIFT #1462 under #1006. #1006 then hit GitHub's 100 sub-issue cap, so SECRETS #1463, CHEAT #1464 and OWNERALPHA #1465 are under epic #230, STAFFADD #1466 and POLICEDOC #1467 under #231, ALPHAPAGE #1468 under #232 (`slice new --attach`; `slice list 1006` reads one level down).

# Slice JOINEV: A player spawn is an event on the bus
## Card
- type: system
- tier: T2
- model: sonnet
- machines: test
- budget: 30 min / 40 calls
- owns: `addons/LifeFramework/Scripts/Game/Feature/Session/**`, `addons/LifeFramework/Scripts/Game/Tests/LF_Test_SessionSpawnEvent.c`
- depends: none
- acceptance: test LF_Test_SessionSpawnEvent
- risk: none
- release-blocker: no

## Capability
`LF_OnPlayerSpawnedEvent` (player id, account uid, first spawn this session yes or no) is published on `LF_EventBus` every time a player spawns, so M4 features can greet or tell a player something without editing the game mode.

## Acceptance
`LF_Test_SessionSpawnEvent`: a subscriber sees one event per spawn, the first with `firstThisSession` true and a second spawn of the same uid false. Red before the publisher exists.

## Notes
The event goes in `Feature/Session/LF_SessionEvents.c` (`validate-registries`). Publish from a `modded class` of the game mode in `Feature/Session/LF_SessionHooks.c`, chaining `super.OnPlayerSpawned`; do not edit `Core/LF_GameModeRoleplay.c` (`:261` is the existing override, for reference). Server only; nothing replicates. The session set of seen uids is cleared by `LF_StaticReset` (append-only file with a merge driver).

# Slice RECRUIT: An approved recruit hears it at the next spawn
## Card
- type: capability
- tier: T2
- model: sonnet
- machines: test
- budget: 40 min / 50 calls
- owns: `addons/LifeFramework/Scripts/Game/Feature/Police/LF_PoliceRosterNotice.c`, `addons/LifeFramework/Scripts/Game/Tests/LF_Test_PoliceRosterNotice.c`
- depends: JOINEV
- acceptance: test LF_Test_PoliceRosterNotice
- risk: none
- release-blocker: no

## Capability
Beat 4 of the chapter: a player whose police application was approved while they were offline gets a toast at their first spawn of the session ("You are on the police roster: go on duty at the station locker"), and an officer whose trial ran out unpromoted is told the same way. Nobody else sees anything.

## Acceptance
`LF_Test_PoliceRosterNotice`: an approved application and a spawn event give one notice for that uid with the approved key; an expired trial gives the expired key; a second spawn the same session gives none; a civilian with no application gives none. Red first.

## Notes
Read `LF_PoliceApplications` (approve at `:201`, `ExpireTrials` at `:269`) through its accessors; do not change its file format (S9 #1288 and S6 #1285 are running beside it). Toast: `LF_NotificationManagerComponent.NotifyPlayer`. Strings: two core keys, neutral text (Police reads from `LF_Terms`).

# Slice JOINNOTE: The alpha notice at a player's first spawn
## Card
- type: capability
- tier: T2
- model: sonnet
- machines: test
- budget: 40 min / 50 calls
- owns: `addons/LifeFramework/Scripts/Game/Feature/Rules/LF_JoinNotice*.c`, `addons/LifeFramework/Scripts/Game/Tests/LF_Test_JoinNotice.c`
- depends: JOINEV
- acceptance: test LF_Test_JoinNotice
- risk: none
- release-blocker: no

## Capability
A server config section `joinNotice` (`enabled`, `key`) shows one toast at each player's first spawn of the session; the pack ships the key `LF_Everon1989_AlphaNotice` ("Alpha: progress can be wiped, with 24 hours' notice on Discord"). Off by default in core.

## Acceptance
`LF_Test_JoinNotice`: enabled with a key gives one toast per uid per session; disabled gives none; an empty key logs `LF_Debug.Warn` once and shows nothing. Red first.

## Notes
The config class is a new file beside the CF sections (#1343, merged); regenerate the schema (`docs/config/config.schema.json` has a merge driver). The pack string goes in `LF_Everon1989/Language/` (merge driver). Do not touch `server/flagship/config.json`: JNCFG turns it on.

# Slice JNCFG: The flagship turns the alpha notice on
## Card
- type: data
- tier: T1
- model: haiku
- machines: none
- budget: 30 min / 30 calls
- owns: `server/flagship/config.json`
- depends: JOINNOTE, #1405
- acceptance: fixture tools/validation/flagship-decisions-selftest.mjs
- risk: none
- release-blocker: no

## Capability
`server/flagship/config.json` sets `joinNotice.enabled` true with the pack's alpha key, and the decisions selftest from FD (#1405) still passes.

## Acceptance
The FD selftest passes with the new section; `tools\cli config check server/flagship/config.json` reports no unknown field. Red first: run the config check before JOINNOTE's class is in your branch.

## Notes
One section added, nothing else in the file changes.

# Slice PRECRUIT: Proof verb recruit-uniform
## Card
- type: tooling
- tier: T0
- model: sonnet
- machines: session-2p
- budget: 45 min / 60 calls
- owns: `agent-box/proofs/recruit-uniform-proof.mjs`
- depends: none
- acceptance: proof recruit-uniform
- risk: none
- release-blocker: no

## Capability
Beat 4 driven end to end: client 2 applies at the station desk, client 1 (the administrator) approves through the staff API, client 2 goes on duty at the locker and wears the police uniform. The verb prints one PASS or FAIL line per step from the server log and the harness.

## Acceptance
`node agent-box/harness.mjs proof recruit-uniform` on a two-client playtest session passes on main; with the approval step skipped it fails at "on duty". Red first.

## Notes
Build on `agent-box/proofs/police-application-proof.mjs` (apply and approve) and `duty-order-proof.mjs`; reuse their helpers, do not edit them. Approve through `LF_PoliceApplicationStaff` (#1287, merged), not the GM panel (#1288, running). Pictures: none needed, logs are the proof. One session, `session down` straight after.

# Slice PINCIDENT: Proof verb incident-handled
## Card
- type: tooling
- tier: T0
- model: sonnet
- machines: session-2p
- budget: 45 min / 60 calls
- owns: `agent-box/proofs/incident-handled-proof.mjs`
- depends: none
- acceptance: proof incident-handled
- risk: none
- release-blocker: no

## Capability
Beat 3 driven end to end: client 2 damages client 1 and files `/report`, the administrator lists the incident in the GM tools and takes one admin action (a warn or a kick), and the incident log holds the damage, the report flag and the admin action in order.

## Acceptance
`node agent-box/harness.mjs proof incident-handled` passes on main; without the report it fails at "flag". Red first.

## Notes
Moderation code: `Feature/Moderation/LF_IncidentLog.c`, `LF_IncidentReportCommand.c`, `LF_IncidentGm.c`, `LF_AdminAction.c`. The ban record (#1285) is running: do not ban in this proof.

# Slice PERFLOG: The server logs a perf line every minute
## Card
- type: system
- tier: T2
- model: sonnet
- machines: test
- budget: 40 min / 50 calls
- owns: `addons/LifeFramework/Scripts/Game/Feature/Ops/LF_ServerPerfLog.c`, `addons/LifeFramework/Scripts/Game/Tests/LF_Test_ServerPerfLog.c`
- depends: none
- acceptance: test LF_Test_ServerPerfLog
- risk: none
- release-blocker: no

## Capability
On a server, one `[LFDebug:Perf] players=N fps=F frameMs=M entities=E` line every 60 seconds, so a 40-player session can be judged from the log afterwards (`host perf` #1406 samples the process; this line is the game's own view).

## Acceptance
`LF_Test_ServerPerfLog`: the formatter turns a fixed sample into the exact line; the interval gate fires once per 60 s of fed time, not per frame. Red first.

## Notes
Self-start from a `modded class` of the game mode in the same file (chain `super`); never per-frame logging. Add the line to `docs/debug-log-triage.md` (merge driver). The restart files in `Feature/Ops/` belong to running slices (#1264, #1267): touch none of them.

# Slice SAY: host say puts an operator message on every screen
## Card
- type: capability
- tier: T2
- model: sonnet
- machines: test
- budget: 45 min / 60 calls
- owns: `addons/LifeFramework/Scripts/Game/Feature/Ops/LF_HostSay.c`, `addons/LifeFramework/Scripts/Game/Tests/LF_Test_HostSay.c`, `tools/host/say.mjs`, `tools/host/commands/say.mjs`, `tools/validation/host-say-selftest.mjs`
- depends: none
- acceptance: test LF_Test_HostSay
- risk: none
- release-blocker: no

## Capability
`tools\cli host say "Hotfix restart in 10 minutes"` writes `$profile:LifeFramework/host_say.txt`; the server reads it within 10 seconds, toasts every player once, logs `[LFDebug:Ops] host say: ...` and deletes the file. The operator can speak to the town without joining.

## Acceptance
`LF_Test_HostSay`: a file with one line gives one NotifyAll and the file is gone; an empty file gives nothing and a warn; text over 200 characters is cut. `host-say-selftest.mjs`: the command writes the file in a fixture profile and refuses an empty message. Red first.

## Notes
Same pattern as `LF_HostStop` (`Core/LF_MissionHeader.c:83`), but a new file; do not edit the header. Poll with a 10 s call-later, server only. Toast: `LF_NotificationManagerComponent.NotifyAll`.

# Slice VERIFY: host verify checks every backup can be read
## Card
- type: tooling
- tier: T0
- model: sonnet
- machines: none
- budget: 35 min / 45 calls
- owns: `tools/host/verify.mjs`, `tools/host/commands/verify.mjs`, `tools/validation/host-verify-selftest.mjs`, `tools/validation/fixtures/host-verify/**`
- depends: none
- acceptance: fixture tools/validation/host-verify-selftest.mjs
- risk: none
- release-blocker: no

## Capability
`tools\cli host verify [--newest N]` opens each backup, checks it has the save files a restore needs, that none is empty, and that its build stamp is readable, and records a checksum beside it; a later run flags a backup whose checksum changed. Exit 1 on any bad backup.

## Acceptance
Selftest: a fixture host with one good, one empty and one tampered backup gives ok, empty and changed, and exit 1. Red first.

## Notes
Backups: `tools/host/host.mjs` (`backupSaves`), rotation `tools/host/rotate.mjs`, the build stamp from #980. Read only, never deletes. `host drill` (#1261) is the full boot test; this is the cheap check.

# Slice OFFSITE: host backup copies to a second drive
## Card
- type: tooling
- tier: T0
- model: sonnet
- machines: none
- budget: 35 min / 45 calls
- owns: `tools/host/offsite.mjs`, `tools/host/commands/offsite.mjs`, `tools/validation/host-offsite-selftest.mjs`
- depends: none
- acceptance: fixture tools/validation/host-offsite-selftest.mjs
- risk: none
- release-blocker: no

## Capability
`tools\cli host offsite <dir>` copies every backup not already there to a second folder (another drive or a NAS share), verifies the copy by size and hash, and keeps the same rotation as the host; `--dry-run` lists what it would copy. A lost save is a trust event; one disk failure must not cost it.

## Acceptance
Selftest: two fixture folders; the first run copies three, the second copies none, a corrupted copy is replaced. Red first.

## Notes
Never deletes from the source. Reuse `rotate.mjs` rules for the target (tagged backups are kept).

# Slice LOGARCH: host logs archives each session's logs
## Card
- type: tooling
- tier: T0
- model: sonnet
- machines: none
- budget: 35 min / 45 calls
- owns: `tools/host/logs.mjs`, `tools/host/commands/logs.mjs`, `tools/validation/host-logs-selftest.mjs`, `tools/validation/fixtures/host-logs/**`
- depends: none
- acceptance: fixture tools/validation/host-logs-selftest.mjs
- risk: none
- release-blocker: no

## Capability
`tools\cli host logs archive` moves each finished session's engine log folder from the profile into `<host>/logs/<session-stamp>/`, keeps the newest 30 sessions, and `host logs list` prints them with size and the error count from `host triage` (#1269).

## Acceptance
Selftest: a fixture profile with three log folders, one still being written (newest, open), archives two and leaves the live one. Red first.

## Notes
Do not name the command `host log`; check `tools\cli host help` for clashes. Never touches the live session's folder.

# Slice CRASHPACK: host crashpack bundles a crash for an issue
## Card
- type: tooling
- tier: T0
- model: sonnet
- machines: none
- budget: 35 min / 45 calls
- owns: `tools/host/crashpack.mjs`, `tools/host/commands/crashpack.mjs`, `tools/validation/host-crashpack-selftest.mjs`, `tools/validation/fixtures/host-crashpack/**`
- depends: none
- acceptance: fixture tools/validation/host-crashpack-selftest.mjs
- risk: none
- release-blocker: no

## Capability
`tools\cli host crashpack [--session <stamp>]` writes one zip with the crash dump, the console log tail, the build stamp, the mod list and the profile's `config.json` and `server.json` with every password and webhook URL replaced by `<redacted>`, plus an issue body draft; the operator attaches it to a GitHub issue.

## Acceptance
Selftest: a fixture crash folder gives a zip whose `server.json` has `<redacted>` for `passwordAdmin`, `password` and any `https://discord.com/api/webhooks/` value, and no other change. Red first.

## Notes
The watchdog keeps crash logs (#1262, `tools/host/watchdog.mjs`). Redaction is the point: the selftest must fail on any leaked secret.

# Slice APPS: host applications lists the police queue
## Card
- type: tooling
- tier: T0
- model: sonnet
- machines: none
- budget: 30 min / 40 calls
- owns: `tools/host/applications.mjs`, `tools/host/commands/applications.mjs`, `tools/validation/host-applications-selftest.mjs`, `tools/validation/fixtures/host-applications/**`
- depends: none
- acceptance: fixture tools/validation/host-applications-selftest.mjs
- risk: none
- release-blocker: no

## Capability
`tools\cli host applications [--all]` reads `$profile:LifeFramework/police_applications.txt` and prints the open applications oldest first (name, age, availability, why), then the officers on trial with the hours left, so the maintainer can review the queue before a session.

## Acceptance
Selftest: a fixture file with open, approved-on-trial, rejected and old 7-field rows gives the expected table; a malformed row is skipped with a warning. Red first.

## Notes
Format: `STATUS|uid|name|why|availability|unix|rejectedAt|trialEndsAt` (`Feature/Police/LF_PoliceApplications.c:3`); old rows have fewer fields. Read only: approving stays in the game (S9 #1288).

# Slice ATTEND: host attendance reads joins and leaves from the log
## Card
- type: tooling
- tier: T0
- model: sonnet
- machines: none
- budget: 40 min / 50 calls
- owns: `tools/host/attendance.mjs`, `tools/host/commands/attendance.mjs`, `tools/validation/host-attendance-selftest.mjs`, `tools/validation/fixtures/host-attendance/**`
- depends: none
- acceptance: fixture tools/validation/host-attendance-selftest.mjs
- risk: none
- release-blocker: no

## Capability
`tools\cli host attendance [<log>]` reads a server console log and prints the session's unique players, the peak concurrent count and when it was, and the minutes each player stayed; `--json` for `host history` (#1409) and `host gate` (#1410), which judge "sessions at the player target".

## Acceptance
Selftest: a fixture log with 12 joins, 3 rejoins and 2 disconnects mid-line gives unique 9, peak 7 at the right time. Red first.

## Notes
Use the engine's real join and leave lines; copy them from a newest server log (`tools\cli logs`) into the fixture. Do not depend on unmerged HIST code; it reads this command's JSON later.

# Slice PORTS: host ports checks the game is reachable
## Card
- type: tooling
- tier: T0
- model: sonnet
- machines: none
- budget: 35 min / 45 calls
- owns: `tools/host/ports.mjs`, `tools/host/commands/ports.mjs`, `tools/validation/host-ports-selftest.mjs`
- depends: none
- acceptance: fixture tools/validation/host-ports-selftest.mjs
- risk: none
- release-blocker: no

## Capability
`tools\cli host ports` reads the profile's `server.json`, checks each port (game, A2S, RCON if set) is free before a start or held by the server after it, and prints the router forward rules (protocol, port) a PC host needs; `--external <host:port>` probes A2S from outside with a bounded timeout.

## Acceptance
Selftest: an injected port table gives free, held-by-server and held-by-other verdicts; the forward rules match a fixture `server.json`. Red first.

## Notes
Nothing waits forever: every probe has a timeout. The A2S client is #1297 (running, `ws/slice-1297`); if you need it, cut from main and merge `origin/ws/slice-1297` in, and own none of its files.

# Slice LISTING: host listing checks what the server browser shows
## Card
- type: tooling
- tier: T0
- model: sonnet
- machines: none
- budget: 35 min / 45 calls
- owns: `tools/host/listing.mjs`, `tools/host/commands/listing.mjs`, `tools/validation/host-listing-selftest.mjs`, `tools/validation/fixtures/host-listing/**`
- depends: none
- acceptance: fixture tools/validation/host-listing-selftest.mjs
- risk: none
- release-blocker: no

## Capability
Beat 1: `tools\cli host listing` queries the running server over A2S and compares name, map, max players, password flag and version with `server/flagship/server.json`; it fails when the name is not exactly "Life Framework | Everon 1989 (Alpha)" or the password flag is off during the alpha.

## Acceptance
Selftest: an injected A2S reply that matches passes; a wrong name, a missing password flag and 64 max players each fail with one line. Red first.

## Notes
Cut from main, merge `origin/ws/slice-1297` (A2S client, #1297) and `origin/ws/slice-1321` (`server/flagship/server.json`, #1321) in; own none of their files. If either has merged by then, use main.

# Slice DRIFT: host drift compares the live config with the repo
## Card
- type: tooling
- tier: T0
- model: sonnet
- machines: none
- budget: 30 min / 40 calls
- owns: `tools/host/drift.mjs`, `tools/host/commands/drift.mjs`, `tools/validation/host-drift-selftest.mjs`, `tools/validation/fixtures/host-drift/**`
- depends: none
- acceptance: fixture tools/validation/host-drift-selftest.mjs
- risk: none
- release-blocker: no

## Capability
The live config is in the repo, so a hand edit on the host is drift. `tools\cli host drift` diffs the profile's `config.json` and `server.json` against `server/flagship/`, ignoring the host-only secret fields, and prints each changed field; exit 1 on drift. The runbook runs it before every session.

## Acceptance
Selftest: equal files pass; a changed `dayCycle.dayLengthMinutes` fails naming the field; a different `passwordAdmin` is ignored. Red first.

## Notes
Cut from main, merge `origin/ws/slice-1321` in for the flagship files (#1321); own none of them. `host config apply` (#1322) writes the files; this only reads.

# Slice SECRETS: validate fails on a committed secret
## Card
- type: tooling
- tier: T0
- model: sonnet
- machines: none
- budget: 30 min / 40 calls
- owns: `tools/validation/validate-no-secrets.mjs`, `tools/validation/fixtures/no-secrets/**`
- depends: none
- acceptance: fixture tools/validation/validate-no-secrets.mjs
- risk: none
- release-blocker: no

## Capability
The live config is public in this repo, so a repo-wide check fails on any tracked file holding a Discord webhook URL, a non-empty `password`, `passwordAdmin` or RCON password in a `server.json`, or a `secrets.json`. It runs in every validate and in CI.

## Acceptance
`validate-no-secrets.mjs --selftest`: fixtures with a webhook URL, a filled `passwordAdmin` and a committed `secrets.json` fail; placeholders (`""`, `<set on host>`) pass; the real tree passes. Red first.

## Notes
FD (#1405) checks the two flagship files; this covers every tracked file. Declare its inputs the way the other validate scripts do so `validate` can skip it when nothing changed.

# Slice STAFFADD: staff add and remove edit the roster safely
## Card
- type: tooling
- tier: T0
- model: sonnet
- machines: none
- budget: 30 min / 40 calls
- owns: `tools/commands/staff.mjs`, `tools/validation/staff-roster-selftest.mjs`, `tools/validation/fixtures/staff-roster/**`
- depends: none
- acceptance: fixture tools/validation/staff-roster-selftest.mjs
- risk: none
- release-blocker: no

## Capability
`tools\cli staff add <uid> <level> --name "<display>"`, `staff remove <uid>` and `staff promote <name-of-pending> <uid>` edit `server/flagship/staff.txt` in place, keep comments and order, and run the roster check after; the maintainer adds the first trial moderators without hand-editing the file.

## Acceptance
Selftest: on a fixture roster, add, a duplicate add (refused), promoting a `#pending` line to a uid, and remove each give the expected file; the check still passes. Red first.

## Notes
`tools/commands/staff.mjs` (#1320, merged) has `parseRoster` and `check`; extend it. If the selftest file already exists from #1320, extend it rather than adding a second. Only the maintainer is staff at opening: change no roster entry in the repo.

# Slice CHEAT: One-page session cheatsheet for the maintainer
## Card
- type: docs
- tier: T0
- model: haiku
- machines: none
- budget: 30 min / 30 calls
- owns: `docs/ops/session-cheatsheet.md`
- depends: none
- acceptance: fixture website/scripts/check-links.mjs
- risk: none
- release-blocker: no

## Capability
One printable page: before a session (drift, ready, verify, applications), start, during (say, players, perf), restart at 06:00 or 18:00, stop, after (logs archive, triage, attendance, offsite), and the wipe with 24 h notice. Each line is one command and what success looks like.

## Acceptance
The page's links resolve (`node website/scripts/check-links.mjs`); every command that is not on main yet is in code font and marked *(planned)*, never linked.

## Notes
Write from the cards: `docs/ops/session-runbook.md` is the long form; link it. Names of wave-4 commands as their cards give them.

# Slice POLICEDOC: How to join the police in the alpha
## Card
- type: docs
- tier: T0
- model: haiku
- machines: none
- budget: 30 min / 30 calls
- owns: `docs/community/police-path.md`
- depends: none
- acceptance: fixture website/scripts/check-links.mjs
- risk: none
- release-blocker: no

## Capability
A player page: apply at the station desk, the playtime gate and the rejection cooldown, the maintainer reviews applications, the trial and how it ends, going on duty at the locker. No Discord account linking; police missions come after opening, so the page promises none.

## Acceptance
Links resolve; the gate numbers match `LF_PoliceConfig` defaults and the pack config, quoted with their config field names.

## Notes
Code: `Feature/Police/LF_PoliceApplications.c`, `LF_PoliceApplicationRules.c`, `LF_PoliceConfig.c`. Link it from nowhere else in this slice (other pages are owned elsewhere).

# Slice OWNERALPHA: Owner page: run a password alpha
## Card
- type: docs
- tier: T0
- model: haiku
- machines: none
- budget: 30 min / 30 calls
- owns: `docs/owner/password-alpha.md`
- depends: none
- acceptance: fixture website/scripts/check-links.mjs
- risk: none
- release-blocker: no

## Capability
Core-neutral owner guide for any server: set a join password in `server.json` (host-only, never committed), share it, rotate it, wipe with notice, and open the server by removing it. Uses no flagship names.

## Acceptance
Links resolve (`node website/scripts/check-links.mjs`) and `validate-core-place-names` passes: the page names no Everon place.

## Notes
The flagship's own alpha page is `docs/community/alpha.md` (#1291, running); link it only once it is on main.

# Slice ALPHAPAGE: The site has an alpha page
## Card
- type: visual
- tier: T0
- model: sonnet
- machines: none
- budget: 40 min / 50 calls
- owns: `website/src/pages/Alpha.tsx`, `website/src/App.tsx`
- depends: none
- acceptance: fixture website/scripts/check-routes.mjs
- risk: none
- release-blocker: no

## Capability
`/alpha` on life-framework.org: what the alpha is, how to ask for the password (the Discord channel), the 24 h wipe notice, and the next session time from `sessions.json` (the same source as the servers page). No live status, no player count.

## Acceptance
The website build passes and its route check lists `/alpha`; the page renders the next session from a fixture `sessions.json`. If the site has no route check, add the smallest one inside owns. One picture of the page in the PR.

## Notes
Text from `docs/community/alpha.md`: cut from main and merge `origin/ws/slice-1291` in (#1291), owning none of its files. Reuse the servers page's session reader (`website/src/pages/Servers.tsx`), do not copy it.
