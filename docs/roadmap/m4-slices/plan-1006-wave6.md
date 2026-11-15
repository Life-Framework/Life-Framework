# Slices for #1006, wave 6 (2026-11-15).
# Fewer than 5 M4 slices were ready. This wave takes the lead's follow-up cards from merged wave-4 and wave-5 PRs
# (docs sync, `host drill` result file, the quiz-pass clear verb, site registration of the new pages), the proof cards
# the epics still need (police approval through the GM panel, the late-join clock, the backup before a scheduled
# restart) and the epic acceptance items that had no card: #315 "a staged ban appears in the record with its
# incident" (BANPROOF), #556 "backup written, saves cleared, boots clean, backup restores" (WIPEDRY), #307 "kill the
# server; it returns and clients can rejoin" (CRASHBACK), #306 "back up, change state, restore, boot: the old state is
# back" (RESTORERT), #314 "an application filed in game appears in the staff channel" (APPRELAY), #129 "real-time mode
# matches the host clock" (REALTIME), and the decisions' live-host check (READYCFG; #1405 holds the repo files).
# Maintainer decisions followed: password-only alpha, 24 h wipe notice, no Discord account linking, the site shows the
# next session time only, police missions after opening, the live config in server/flagship, a 2 h in-game day,
# restarts at 06:00 and 18:00 with a toast only, the server name "Life Framework | Everon 1989 (Alpha)", only the
# maintainer is staff, new world props in the debug grid (no card here places one). No town-pass, placement-cleanup,
# video or M5 cards. No issue is re-milestoned.
# #1006 is at GitHub's 100 sub-issue cap: Ops cards go under #230, Staff under #231, Showcase under #232
# (`slice new 1006 --plan <this file> --attach <epic>`, one run per epic; the plan is split by the comment lines below).
# Created 2026-11-15 under #230: SYNCA #1556, SYNCB #1557, DRILLRESULT #1558, READYCFG #1559, RESTARTBACKUP #1560, RESTORERT #1561, WIPEDRY #1562, CRASHBACK #1563, LATECLOCK #1564, REALTIME #1565, RESTARTLATE #1566; under #231: QUIZCLEAR #1567, QUIZRED #1568, GMAPPROVE #1569, APPRELAY #1570, BANPROOF #1571; under #232: SITEREG #1572. FLAGCFG was dropped: #1405 covers it.

# --- attach #230 (Ops) ---

# Slice SYNCA: Community and flagship pages name the merged host commands
## Card
- type: docs
- tier: T0
- model: haiku
- machines: none
- budget: 20 min / 25 calls
- owns: `docs/community/feedback-triage.md`, `server/flagship/README.md`
- depends: none
- acceptance: fixture website/scripts/check-links.mjs
- risk: none
- release-blocker: no

## Capability
The staff pages say the commands that exist: `docs/community/feedback-triage.md` points at `tools\cli host feedback` (#1540, merged) for drafting issues from a session's feedback export, and `host triage` loses its "(planned)"; `server/flagship/README.md` drops "*(planned)*" from `tools\cli host config apply` (#1525, merged).

## Acceptance
`grep -n "planned" docs/community/feedback-triage.md server/flagship/README.md` finds no merged command; the flags match `tools\cli host help`. `node website/scripts/check-links.mjs` passes.

## Notes
Lead follow-ups from #1540 and #1525. The ops pages (runbook, hotfix, rented box) are SYNCB, after #1350.

# Slice SYNCB: Ops pages and the moderator guide catch up with drift, boxcheck and the ban record
## Card
- type: docs
- tier: T0
- model: haiku
- machines: none
- budget: 25 min / 30 calls
- owns: `docs/ops/session-runbook.md`, `docs/ops/hotfix.md`, `docs/ops/rented-box.md`, `docs/community/moderator-guide.md`
- depends: #1350, #1462, #1496, #1285
- acceptance: fixture website/scripts/check-links.mjs
- risk: none
- release-blocker: no

## Capability
Runbook step 4 runs `tools\cli host drift` (#1462) beside `host config apply`. `hotfix.md` and `rented-box.md` drop "*(planned)*" from `host config apply` (#1525); `hotfix.md` step 6 says plainly that the flagship restarts at 06:00 and 18:00 server time with a toast only, no "planned in #1264 and #1265". `rented-box.md` step 9 names `tools\cli host boxcheck` (#1496) for the 40-player spec. The moderator guide's ban section uses the commands #1285 shipped (`#banrecord <uid>`, `#ban remove`; read the merged code for the exact form) and loses the "Coming (#1285)" paragraph.

## Acceptance
Every command the four pages name is in `tools\cli host help` or `tools\cli help`, or is a vanilla/LF chat command in the merged code; `node website/scripts/check-links.mjs` passes; RBCHECK (#1502) passes if merged.

## Notes
Waits on #1350 (it owns the three ops pages first). Lead follow-ups from #1486, #1525, #1543 and the ban-record PR #1373.

# Slice DRILLRESULT: host drill writes a result file that host ready reads
## Card
- type: tooling
- tier: T0
- model: sonnet
- machines: none
- budget: 35 min / 45 calls
- owns: `tools/host/drill.mjs`, `tools/host/commands/drill.mjs`, `tools/host/ready.mjs`, `tools/validation/host-drill-result-selftest.mjs`
- depends: none
- acceptance: fixture tools/validation/host-drill-result-selftest.mjs
- risk: none
- release-blocker: no

## Capability
`host drill` writes `<host dir>/drill-last.json` (`{ ok, backup, machine, accounts, reason, at }`) after every run, local or on a pool machine, and `host ready` reads that file first, so a remote drill no longer reads as "no drill on record" (gap named in #1544). The log-file reader stays as the fallback.

## Acceptance
Selftest: a fixture host dir with a passing result 2 days old gives GO for the drill check; a failing one gives NO-GO naming its reason; one 8 days old is stale; no file and no log is "no drill on record". Red first (ready ignores the file today).

## Notes
`summaryLine` and `drillVerdict` in `tools/host/drill.mjs` already hold the fields; `host-ready-selftest.mjs` must still pass.

# Slice READYCFG: host ready checks the live server name and that a password is set
## Card
- type: tooling
- tier: T0
- model: sonnet
- machines: none
- budget: 30 min / 40 calls
- owns: `tools/host/ready.mjs`, `tools/validation/host-ready-config-selftest.mjs`
- depends: DRILLRESULT
- acceptance: fixture tools/validation/host-ready-config-selftest.mjs
- risk: none
- release-blocker: no

## Capability
Runbook step 4's hand check becomes a `host ready` line: the live profile's `server.json` has a non-empty game password (the alpha gate is a password only) and the name is exactly `Life Framework | Everon 1989 (Alpha)` when started with `--flagship`. The password is never printed.

## Acceptance
Selftest: an empty password is NO-GO, a wrong name is NO-GO naming the expected name, both right is green, and the output never contains the password. Red first.

## Notes
Same file as DRILLRESULT, so it waits on it.

# Slice RESTARTBACKUP: A scheduled restart backs the saves up before the host starts again
## Card
- type: tooling
- tier: T0
- model: sonnet
- machines: none
- budget: 35 min / 45 calls
- owns: `tools/host/watchdog.mjs`, `tools/validation/host-restart-backup-selftest.mjs`
- depends: none
- acceptance: fixture tools/validation/host-restart-backup-selftest.mjs
- risk: none
- release-blocker: no

## Capability
#306 asks for a backup before every restart. After the scheduled close (#1267, merged in #1394) the watchdog sees the restart marker and starts the host again; this card proves a plain backup of the closing save is written, tagged `restart`, before that start, and adds it if it is not.

## Acceptance
Selftest on a fixture host dir: a restart-marker exit leaves `backups/<time>-restart/` holding the save files before the injected start is called; a stop exit leaves none; a crash exit keeps its crash backup. Red first.

## Notes
`host start` may already back up; if so, the selftest proves the order and the card only adds the tag. Rotation (#1260) must keep counting these as plain backups.

# Slice RESTORERT: Proof verb restore-roundtrip: a restored backup brings the old state back
## Card
- type: tooling
- tier: T0
- model: sonnet
- machines: session
- budget: 45 min / 55 calls
- owns: `agent-box/proofs/restore-roundtrip-proof.mjs`
- depends: none
- acceptance: proof restore-roundtrip
- risk: none
- release-blocker: no

## Capability
#306's restart acceptance, agent-runnable: a client registers and notes its bank balance, the host backs up, the client earns money, the server stops, `host restore` puts the backup back, the server boots, and the rejoined client reads the old balance.

## Acceptance
`tools\cli session restore-roundtrip-proof` passes; with the restore step skipped it fails at "old balance back". Red first.

## Notes
Reuse `rejoin-proof.mjs` and the `wt e2e` two-boot pattern; restore a copy, never a live save. Logs are the proof.

# Slice WIPEDRY: Proof verb wipe-restore: wipe, boot clean, restore
## Card
- type: tooling
- tier: T0
- model: sonnet
- machines: session
- budget: 45 min / 55 calls
- owns: `agent-box/proofs/wipe-restore-proof.mjs`
- depends: none
- acceptance: proof wipe-restore
- risk: none
- release-blocker: no

## Capability
#556's dry run without a person: a session with one registered account stops; `host wipe` (with a notice stamped 24 h earlier in the fixture notice file) backs up and clears; the next boot has no accounts; `host restore` of the wipe backup brings the account back on the boot after.

## Acceptance
`tools\cli session wipe-restore-proof` passes; with the wipe's backup deleted before the restore it fails at "account back". Red first.

## Notes
Uses `tools/host/wipe.mjs` and the notice rule of #1413 as merged; if #1413 has not merged, run the wipe with its confirmation only and say so in the PR.

# Slice CRASHBACK: Proof verb crash-rejoin: a killed server comes back and a client rejoins
## Card
- type: tooling
- tier: T0
- model: sonnet
- machines: session
- budget: 45 min / 55 calls
- owns: `agent-box/proofs/crash-rejoin-proof.mjs`
- depends: none
- acceptance: proof crash-rejoin
- risk: none
- release-blocker: no

## Capability
#307's manual line: with `host watch` running, the proof kills the server process; the watchdog writes a crash backup, starts the host, the save loads, and the client rejoins with its character.

## Acceptance
`tools\cli session crash-rejoin-proof` passes; with the watch not started it fails at "server back". Red first.

## Notes
The watchdog is #1262 (merged, needs-playtest). Kill by pid from the host state, never by image name (other servers run on pool machines).

# Slice LATECLOCK: Proof verb clock-late: a late joiner sees the server's time of day
## Card
- type: tooling
- tier: T0
- model: sonnet
- machines: session-2p
- budget: 40 min / 50 calls
- owns: `agent-box/proofs/clock-late-proof.mjs`
- depends: #1301
- acceptance: proof clock-late
- risk: none
- release-blocker: no

## Capability
With the flagship's 120-minute day, client 2 joins 3 minutes after client 1; both read the same in-game time within one in-game minute, and client 2's clock moves at the same 12x rate over the next minute.

## Acceptance
`tools\cli session clock-late-proof` passes; with client 2's reading taken from its local default instead of the replicated time it fails at "same time". Red first.

## Notes
Uses the time manager #1301 drives; #1303's `day-rate` proof is the single-client sibling.

# Slice REALTIME: Proof verb day-realtime: real-time mode follows the host clock
## Card
- type: tooling
- tier: T0
- model: sonnet
- machines: session
- budget: 35 min / 45 calls
- owns: `agent-box/proofs/day-realtime-proof.mjs`
- depends: #1301
- acceptance: proof day-realtime
- risk: none
- release-blocker: no

## Capability
#129's second harness line: a session booted with the day cycle in real-time mode shows the in-game time equal to the host's clock (within a minute) and advancing at 1x.

## Acceptance
`tools\cli session day-realtime-proof` passes; booted in accelerated mode it fails at "matches the host clock". Red first.

## Notes
The flagship stays on the 2 h day; this proves the owner option only.

# Slice RESTARTLATE: Proof verb restart-late: a client that joins after the first warning still gets the rest
## Card
- type: tooling
- tier: T0
- model: sonnet
- machines: session-2p
- budget: 40 min / 50 calls
- owns: `agent-box/proofs/restart-late-proof.mjs`
- depends: #1265
- acceptance: proof restart-late
- risk: none
- release-blocker: no

## Capability
The restart toast reaches a late joiner: a restart is set a few minutes ahead (the B7SETUP helper or config), client 2 joins after the 15-minute warning has fired and still gets the 5- and 1-minute toasts; neither client gets a siren (the flagship is toast only).

## Acceptance
`tools\cli session restart-late-proof` passes; with client 2 joining after the last warning it fails at "5-minute toast". Red first.

## Notes
#1268's restart-cycle proof is the one-client sibling; reuse its setup, do not edit it.

# --- attach #231 (Staff) ---

# Slice QUIZCLEAR: Harness verb quizclear forgets a player's stored quiz pass
## Card
- type: tooling
- tier: T2
- model: sonnet
- machines: test
- budget: 40 min / 50 calls
- owns: `addons/LifeFramework/Scripts/Game/Feature/AgentHarness/LF_QuizHarness.c`, `addons/LifeFramework/Scripts/Game/Tests/LF_Test_QuizHarness.c`
- depends: none
- acceptance: test LF_Test_QuizHarness
- risk: none
- release-blocker: no

## Capability
The harness can clear the rules-quiz pass of a player (the `flag:` entry `LF_Progression.NoteFlag` keeps, #915) in memory and in the progression store, so a proof can make a rejoin see the quiz again. Gap named in #1542.

## Acceptance
LF_Test_QuizHarness: note the flag, clear it through `LF_QuizHarness.Clear(uid)`, and the quiz check says the player has not passed. Red first.

## Notes
Dispatch is three lines in `LF_AgentHarness.c` beside the other per-feature harness verbs (`spikepreview` shape); that file is shared, add only. Harness-only: it must not be reachable outside the agent harness.

# Slice QUIZRED: quiz-rejoin goes red live
## Card
- type: tooling
- tier: T0
- model: sonnet
- machines: session-2p
- budget: 30 min / 40 calls
- owns: `agent-box/proofs/quiz-rejoin-proof.mjs`
- depends: QUIZCLEAR, #1490
- acceptance: proof quiz-rejoin
- risk: none
- release-blocker: no

## Capability
`quiz-rejoin-proof --clear-pass` sends `quizclear` before the rejoin, and the live run then fails at "no second quiz", so the proof's red is a live run, not only its selftest.

## Acceptance
On `--pack LF_Everon1989 --mission playtest`, the plain run passes and `--clear-pass` fails at "no second quiz". Record both lines in the PR.

# Slice GMAPPROVE: Proof verb police-approve-gm: approval in the GM panel lets the recruit clock in
## Card
- type: tooling
- tier: T0
- model: sonnet
- machines: session-2p
- budget: 45 min / 55 calls
- owns: `agent-box/proofs/police-approve-gm-proof.mjs`
- depends: #1288
- acceptance: proof police-approve-gm
- risk: none
- release-blocker: no

## Capability
#314's second half: client 2 files a police application at the desk; client 1, an administrator, approves it through the Game Master panel's application list; client 2 clocks in at the station.

## Acceptance
`tools\cli session police-approve-gm-proof` passes; with the approve step skipped it fails at "clocked in". Red first.

## Notes
Build on `police-application-proof.mjs`. The admin client and `staffapprove` verbs are in PR #1539; if it has not merged, cut from main and merge `origin/ws/harness-staff-approve` in, owning none of its files.

# Slice APPRELAY: Proof verb application-relay: a filed application reaches the staff webhook
## Card
- type: tooling
- tier: T0
- model: sonnet
- machines: session
- budget: 40 min / 50 calls
- owns: `agent-box/proofs/application-relay-proof.mjs`
- depends: #1289
- acceptance: proof application-relay
- risk: none
- release-blocker: no

## Capability
#314's first half: a local HTTP sink stands in for the staff channel; the session's config points the staff webhook at it; a client files an application; the sink receives one POST naming the application and no account uid beyond what #1289 sends.

## Acceptance
`tools\cli session application-relay-proof` passes; with the webhook URL unset it fails at "one POST". Red first.

## Notes
No Discord call ever; the sink binds to the session machine only.

# Slice BANPROOF: Proof verb ban-record: a staged ban appears in the record with its incident
## Card
- type: tooling
- tier: T0
- model: sonnet
- machines: session-2p
- budget: 45 min / 55 calls
- owns: `agent-box/proofs/ban-record-proof.mjs`
- depends: #1285
- acceptance: proof ban-record
- risk: none
- release-blocker: no

## Capability
#315's acceptance: client 2 causes an incident, administrator client 1 bans client 2 with a reason, and `#banrecord <uid>` returns the row with the reason, the staff uid and the incident number; client 2 cannot rejoin.

## Acceptance
`tools\cli session ban-record-proof` passes; with the incident not raised it fails at "incident in the row". Red first.

## Notes
Command forms come from #1285 as merged. Logs are the proof.

# --- attach #232 (Showcase) ---

# Slice SITEREG: The site publishes today's player and owner pages
## Card
- type: tooling
- tier: T0
- model: sonnet
- machines: none
- budget: 30 min / 40 calls
- owns: `website/scripts/build-content.mjs`
- depends: none
- acceptance: fixture website/scripts/check-links.mjs
- risk: none
- release-blocker: no

## Capability
The pages written this week are on the site, so `check-links` sees them: register in `build-content.mjs` the player- and owner-facing ones among `docs/community/alpha.md`, `docs/community/appeals.md`, `docs/community/moderator-drill.md`, `docs/community/police-path.md`, `docs/owner/password-alpha.md`, `docs/owner/day-cycle.md` and `docs/owner/log-lines.md`, with group, order and title like the rows already there. `docs/player-clips.md` is already registered (`clip-guide`). Skip internal ops and staff-only pages (the moderator drill is staff-facing: decide and say why in the PR).

## Acceptance
`npm run build` in `website/`, then `node website/scripts/check-links.mjs` on the built `dist` passes, and each registered page has a route. Red first: run check-links before and show the pages are absent.

## Notes
`moderator-drill.md` (PR #1552) and `police-path.md` may not be on main yet: register what exists, list the rest in the PR for a follow-up line. Lead request, 2026-11-15.
