# Slices for #1006, wave 3 (2026-11-15).
# Every card of plan-1006.md, plan-1006-decisions.md and plan-1006-widen.md is an issue; this wave fills the gaps the
# M4 outline and the maintainer's decisions leave: the flagship config carrying the decisions, host tools for a
# session and the opening gate, owner and player pages, the trailer sign-off (no filming), and the chapter's town
# placement. `depends: #N` names an issue already under #1006 (`slice new` checks the plan against them).
# World props: a slice places them in a DebugWorld grid cell (`tools\cli grid place`, ws/world-zones) and owns no town
# layer; only TOWN, the chapter-close card, places M4's systems in the polished town.
# Created 2026-11-15: FD #1405, PERF #1406, DISK #1407, READY #1408, HIST #1409, GATE #1410, WIPEN #1413, POST #1414, PASSROT #1415, NEXT #1416, LOGFIX #1417, DAYDOC #1418, FAQ #1419, WSCOPY #1420, QUIZMAP #1421, SIGNOFF #1422, SCRIPT #1423, RYO #1424, BEATS #1425, TOWN #1427.

# Slice FD: The flagship config carries the maintainer's decisions, and a check keeps them
## Card
- type: data
- tier: T1
- model: sonnet
- machines: none
- budget: 35 min / 45 calls
- owns: `server/flagship/config.json`, `server/flagship/server.json`, `tools/validation/flagship-decisions-selftest.mjs`, `tools/validation/fixtures/flagship-decisions/**`
- depends: #1321
- acceptance: fixture tools/validation/flagship-decisions-selftest.mjs
- risk: none
- release-blocker: no

## Capability
`server/flagship/config.json` sets restarts at 06:00 and 18:00 with warnings at 15, 5 and 1 minutes (toast only, no siren), an accelerated 120-minute day, and `server.json` names the server exactly "Life Framework | Everon 1989 (Alpha)"; a selftest fails when any of them drifts or a secret field (join password, admin password, webhook URL) appears in either file.

## Acceptance
`flagship-decisions-selftest.mjs`: the committed files pass; fixtures with a wrong name, a third restart time, a 60-minute day and a `passwordAdmin` value each fail with one line naming the field. Red first (run it before editing the config).

## Notes
Field names are the CF contract (#1343): `restarts.times`, `restarts.warningMinutes`, `dayCycle.mode`, `dayCycle.dayLengthMinutes`. Restart times are the host's local time. The siren (#1266) is on hold by decision: restarts warn with the toast only, so nothing here mentions a siren.

# Slice PERF: host perf samples the server's CPU, memory and upload during a session
## Card
- type: tooling
- tier: T0
- model: sonnet
- machines: none
- budget: 40 min / 50 calls
- owns: `tools/host/perf.mjs`, `tools/host/commands/perf.mjs`, `tools/validation/host-perf-selftest.mjs`, `tools/validation/fixtures/host-perf/**`
- depends: none
- acceptance: fixture tools/validation/host-perf-selftest.mjs
- risk: none
- release-blocker: no

## Capability
`tools\cli host perf [--every 30] [--for 120]` samples the running server process (CPU %, working set, network bytes sent per second) into `<host>/perf/perf-<stamp>.csv` and prints one verdict line against a 40-player budget (CPU under 80% of one core's share, upload under the configured Mbit/s), so the maintainer knows the PC can carry a full session.

## Acceptance
`host-perf-selftest.mjs`: a fixture sample series gives the expected CSV and verdict (ok, then over budget on upload); the sampler is injected, so no process is read. Red first.

## Notes
Windows first (`Get-Counter` / `typeperf` through a bounded child with `timeout`), the sampler behind a seam. Nothing waits forever: `--for` caps the run. A later row in the CSV may add players from A2S (#1297); leave a `players` column empty.

# Slice DISK: host disk warns before backups fill the drive
## Card
- type: tooling
- tier: T0
- model: sonnet
- machines: none
- budget: 30 min / 40 calls
- owns: `tools/host/disk.mjs`, `tools/host/commands/disk.mjs`, `tools/validation/host-disk-selftest.mjs`
- depends: none
- acceptance: fixture tools/validation/host-disk-selftest.mjs
- risk: none
- release-blocker: no

## Capability
`tools\cli host disk` prints the save size, the backups' total size and growth over the last five backups, the free space on the drive, and how many sessions fit before it drops under 5 GB; it exits 1 under 5 GB free.

## Acceptance
`host-disk-selftest.mjs`: a fixture host dir with sized backups and an injected free-space value gives the expected numbers, and 4 GB free exits 1. Red first.

## Notes
Backups and rotation: `tools/host/host.mjs` (`backupSaves`), `tools/host/rotate.mjs` (#1260). Read only; never deletes.

# Slice READY: host ready says go or no-go before a session
## Card
- type: tooling
- tier: T0
- model: sonnet
- machines: none
- budget: 40 min / 50 calls
- owns: `tools/host/ready.mjs`, `tools/host/commands/ready.mjs`, `tools/validation/host-ready-selftest.mjs`
- depends: DISK
- acceptance: fixture tools/validation/host-ready-selftest.mjs
- risk: none
- release-blocker: no

## Capability
`tools\cli host ready` runs the pre-session checks in one go (the build gate from `host preflight`, `staff roster check`, the newest backup's age, the last restore drill's result, `host disk`) and prints one line each plus GO or NO-GO, so no session starts without a fresh, restorable backup.

## Acceptance
`host-ready-selftest.mjs`: each check is injected; all green gives GO, a failed drill or a stale backup gives NO-GO naming it, exit 1. Red first.

## Notes
Calls the existing pieces, never reimplements them: `buildGate` in `tools/host/host.mjs`, `tools/host/drill.mjs` (#1261), `tools/commands/staff.mjs` roster check (#1320). Runbook wording is O16R's (#1350); this card adds no ops page.

# Slice HIST: host history lists past sessions from the host's own records
## Card
- type: tooling
- tier: T0
- model: sonnet
- machines: none
- budget: 35 min / 45 calls
- owns: `tools/host/history.mjs`, `tools/host/commands/history.mjs`, `tools/validation/host-history-selftest.mjs`, `tools/validation/fixtures/host-history/**`
- depends: PERF
- acceptance: fixture tools/validation/host-history-selftest.mjs
- risk: none
- release-blocker: no

## Capability
`tools\cli host history [--json]` rebuilds one row per past session (start, end, length, crashes, the backups taken, the wipe if any, peak players when a perf CSV recorded them) from the backup stamps, the wipe log and the perf files, writing nothing.

## Acceptance
`host-history-selftest.mjs`: a fixture host dir with three sessions, one crash and one wipe gives the expected rows. Red first.

## Notes
Backup reasons are in the backup folder names (`host.mjs` `backupSaves(dir, reason)`); the wipe log is `backups/wipes.log` (#1274). No hook in `host start`/`stop`: history is derived.

# Slice GATE: host gate checks the opening gate for the public launch
## Card
- type: tooling
- tier: T0
- model: sonnet
- machines: none
- budget: 40 min / 50 calls
- owns: `tools/host/gate.mjs`, `tools/host/commands/gate.mjs`, `tools/validation/host-gate-selftest.mjs`
- depends: HIST
- acceptance: fixture tools/validation/host-gate-selftest.mjs
- risk: none
- release-blocker: no

## Capability
`tools\cli host gate` prints the #555 opening gate: sessions in a row at the player target (from `host history`), no save loss (drills passed, no restore after a session), no open `regression` issue, and the roster's administrator uid filled in; each line passes or fails, and the gate opens only when all do.

## Acceptance
`host-gate-selftest.mjs`: injected history, drill results, issue list and roster give open, then closed on one open regression. Red first.

## Notes
Decisions: the alpha gate is the server password only; at opening only the maintainer is staff, so the staff line checks the roster's administrator row, not a moderator count. Opening itself (removing the password) stays a human step.

# Slice WIPEN: host wipe refuses without a wipe notice posted 24 hours earlier
## Card
- type: tooling
- tier: T0
- model: sonnet
- machines: none
- budget: 30 min / 40 calls
- owns: `tools/host/wipe.mjs`, `tools/host/commands/wipe.mjs`, `tools/validation/host-wipe-selftest.mjs`
- depends: #1273
- acceptance: fixture tools/validation/host-wipe-selftest.mjs
- risk: none
- release-blocker: no

## Capability
`tools\cli host wipe --confirm <dir>` refuses unless `host announce --wipe` recorded a wipe notice at least 24 hours before now, and prints when the wipe becomes allowed; the backup, the move aside and the log stay as they are.

## Acceptance
`host-wipe-selftest.mjs` gains: a notice 23 hours old is refused with the allowed time, 25 hours old wipes, no notice is refused. Red first.

## Notes
Decision: wipes any time during the alpha, with 24 hours' notice on Discord. Read the notice where #1273 records it (`website/content/sessions.json` per `sessions.schema.json`); do not write it.

# Slice POST: host post sends a drafted announcement to the Discord webhook
## Card
- type: tooling
- tier: T0
- model: sonnet
- machines: none
- budget: 35 min / 45 calls
- owns: `tools/host/discord-post.mjs`, `tools/host/commands/post.mjs`, `tools/validation/host-post-selftest.mjs`
- depends: #1273, #1322
- acceptance: fixture tools/validation/host-post-selftest.mjs
- risk: none
- release-blocker: no

## Capability
`tools\cli host post session|wipe` prints the post `host announce` drafted and, only with `--send`, POSTs it to the announcements webhook URL from the host's `secrets.json`; without the URL it says so and exits 1.

## Acceptance
`host-post-selftest.mjs`: the HTTP call goes through a capture seam; dry run prints and sends nothing, `--send` sends one body under 2000 characters, a missing URL exits 1. Red first.

## Notes
The webhook URL is host-only (C2, #1322), never in the repo. No Discord account linking (not in M4). A bounded `fetch` with a timeout.

# Slice PASSROT: host password rotates the alpha join password
## Card
- type: tooling
- tier: T0
- model: sonnet
- machines: none
- budget: 30 min / 40 calls
- owns: `tools/host/password.mjs`, `tools/host/commands/password.mjs`, `tools/validation/host-password-selftest.mjs`
- depends: #1322
- acceptance: fixture tools/validation/host-password-selftest.mjs
- risk: none
- release-blocker: no

## Capability
`tools\cli host password rotate` writes a new readable join password into the host's `secrets.json`, re-applies the profile's `server.json` through `host config apply`, and prints the invite line for the testers; `host password show` prints the current one.

## Acceptance
`host-password-selftest.mjs`: a fixture host dir gets a new password in `secrets.json` and in the applied `server.json`, never in a repo file. Red first.

## Notes
Decision: the alpha gate is a server password only. The invite wording is S12's page (#1291); link it, do not rewrite it.

# Slice NEXT: host next prints the next restart and the next session
## Card
- type: tooling
- tier: T0
- model: sonnet
- machines: none
- budget: 30 min / 40 calls
- owns: `tools/host/next.mjs`, `tools/host/commands/next.mjs`, `tools/validation/host-next-selftest.mjs`
- depends: FD
- acceptance: fixture tools/validation/host-next-selftest.mjs
- risk: none
- release-blocker: no

## Capability
`tools\cli host next` prints the next scheduled restart from the profile's config (06:00 or 18:00 on the flagship) with its warning times, and the next announced session from `sessions.json`, both in local time.

## Acceptance
`host-next-selftest.mjs`: at 05:50 the next restart is 06:00 with a warning at 05:55 already due; at 19:00 it is 06:00 tomorrow; no `restarts` gives "no scheduled restarts". Red first.

## Notes
Mirror the game's schedule rule (O5, #1264) in JS; the field names are CF's (#1343).

# Slice LOGFIX: Owners get the exact success lines and a log-line-to-fix table
## Card
- type: docs
- tier: T0
- model: sonnet
- machines: none
- budget: 40 min / 50 calls
- owns: `docs/owner/log-lines.md`, `tools/validation/owner-log-lines-selftest.mjs`
- depends: none
- acceptance: fixture tools/validation/owner-log-lines-selftest.mjs
- risk: none
- release-blocker: no

## Capability
`docs/owner/log-lines.md` lists the lines that mean the server booted with the framework and the pack, and the fifteen commonest error lines from `docs/debug-log-triage.md` with what each means and the fix, linked from `docs/owner/install.md`.

## Acceptance
`owner-log-lines-selftest.mjs`: every success line on the page passes `judgeBootLog` (`tools/release/boot-check.mjs`), and every error row's pattern appears in the triage doc. Red first.

## Notes
#379 and #380: the outline promises a log-line-to-fix table that beats the Overthrow wiki. One link line in `install.md` is allowed (append only).

# Slice DAYDOC: The day-cycle owner page
## Card
- type: docs
- tier: T0
- model: haiku
- machines: none
- budget: 30 min / 35 calls
- owns: `docs/owner/day-cycle.md`
- depends: none
- acceptance: proof validate (docs links) and the checklist in Acceptance
- risk: none
- release-blocker: no

## Capability
`docs/owner/day-cycle.md` tells an owner how to choose vanilla, accelerated or real-time time, with the `dayCycle` fields, the flagship's 2-hour day as the worked example, and what happens to the clock over a restart.

## Acceptance
Checklist: the three modes each with a copy-paste `config.json` block; field names exactly as `LF_DayCycleConfig.c` declares them; the restart behaviour marked *(planned)* until #1302 merges; `tools\cli validate` clean.

## Notes
Written from the plan (X1 #1300, X2 #1301, X3 #1302). The core default stays vanilla; only the flagship config sets 120 minutes.

# Slice FAQ: The player FAQ covers the alpha
## Card
- type: docs
- tier: T0
- model: haiku
- machines: none
- budget: 30 min / 35 calls
- owns: `docs/player-faq.md`
- depends: none
- acceptance: proof validate (docs links) and the checklist in Acceptance
- risk: none
- release-blocker: no

## Capability
The player FAQ answers the alpha questions: how to get the password, that the world can be wiped with 24 hours' notice on Discord until the public launch, restarts at 06:00 and 18:00 with an in-game warning, a day of 2 real hours, and where the next session time is posted.

## Acceptance
Checklist: each answer matches the decisions table in `docs/roadmap/m4-slice-plan.md`; links to `docs/community/wipe-policy.md` and the servers page; no live player count promised; `tools\cli validate` clean.

## Notes
Decisions of 2026-11-14 and 2026-11-15 (restart times, toast only).

# Slice WSCOPY: The Workshop page and listing copy follow the decisions
## Card
- type: docs
- tier: T0
- model: haiku
- machines: none
- budget: 30 min / 35 calls
- owns: `docs/marketing/workshop-flagship.md`
- depends: none
- acceptance: proof validate (docs links) and the checklist in Acceptance
- risk: none
- release-blocker: no

## Capability
The flagship Workshop copy names the server exactly "Life Framework | Everon 1989 (Alpha)", says the alpha is invite-only by password with wipes on 24 hours' notice, gives the restart times and the 2-hour day, and ends with the run-your-own link.

## Acceptance
Checklist: the name string is exact; no live status or player count promised; the run-your-own link resolves; `tools\cli validate` clean.

## Notes
G5 (#1296) wrote the page before the 2026-11-15 restart decision; edit, do not rewrite.

# Slice QUIZMAP: Every quiz question tests a published rule
## Card
- type: data
- tier: T1
- model: sonnet
- machines: none
- budget: 40 min / 50 calls
- owns: `tools/rules/quiz-map.json`, `tools/validation/quiz-rules-selftest.mjs`, `addons/LF_Everon1989/Configs/Rules/LF_Quiz_Everon.conf`, `website/content/rules.json`
- depends: none
- acceptance: fixture tools/validation/quiz-rules-selftest.mjs
- risk: none
- release-blocker: no

## Capability
A map from each quiz question id to the published rule it tests, and a check that refuses a question with no published rule; today `safezone` has none, so the published rules gain the safe-zone rule (pack string, then `tools\cli rules export`) or the question goes.

## Acceptance
`quiz-rules-selftest.mjs`: the real quiz and rules pass; a fixture quiz with an unmapped id and a map entry naming a rule that is not published each fail with one line. Red first (it fails on `safezone` today).

## Notes
Decision: the quiz is a written subset of the published rules, never generated from them; the map links them, it does not derive one from the other. Rules source: the pack string table (`tools/commands/rules.mjs`, #1281). Prefer publishing the rule if the town has a safe zone; say which you chose in the PR.

# Slice SIGNOFF: The trailer sign-off packet for the maintainer
## Card
- type: docs
- tier: T0
- model: sonnet
- machines: none
- budget: 35 min / 45 calls
- owns: `docs/marketing/trailer/signoff.md`
- depends: #1352
- acceptance: proof validate (docs links) and the checklist in Acceptance
- risk: none
- release-blocker: no

## Capability
One page the maintainer reads to sign the trailer off before any filming: the script, the storyboard from `video/m4-trailer.json` shot by shot with duration and caption, the music bed, the end card, what each shot needs in the world, and a sign-off box; the filming cards (#1293, #1294, #1351, #1295) stay on hold until it is ticked.

## Acceptance
Checklist: every storyboard shot appears once with its caption and length; the total runtime is stated; each shot names the feature it shows; the sign-off box and the hold rule are on the page; `tools\cli validate` clean.

## Notes
Decision 2026-11-15: storyboard and script first, no filming until the maintainer signs off. This card films nothing and runs no session.

# Slice SCRIPT: The trailer script says only what main does
## Card
- type: docs
- tier: T0
- model: sonnet
- machines: none
- budget: 35 min / 45 calls
- owns: `docs/marketing/trailer/script.md`, `tools/validation/trailer-script-selftest.mjs`
- depends: none
- acceptance: fixture tools/validation/trailer-script-selftest.mjs
- risk: none
- release-blocker: no

## Capability
Each line of the trailer script names the `docs/features.md` row it shows, in the 1989 tone, and a selftest fails on a line that names no row or a row that does not exist, so the trailer never promises what the flagship lacks.

## Acceptance
`trailer-script-selftest.mjs`: the real script passes; a fixture line with no feature reference and one naming a missing row each fail. Red first.

## Notes
The storyboard and captions are G4E's (#1352, `video/m4-trailer.json`); this card edits the script only. No filming.

# Slice RYO: Every flagship surface links run-your-own
## Card
- type: tooling
- tier: T0
- model: sonnet
- machines: none
- budget: 35 min / 45 calls
- owns: `tools/validation/validate-run-your-own.mjs`, `tools/validation/fixtures/run-your-own/**`
- depends: none
- acceptance: fixture tools/validation/fixtures/run-your-own/missing-link.md
- risk: none
- release-blocker: no

## Capability
A validator lists the flagship surfaces (Workshop copy, the site's join, servers and rules pages, the pack's run-your-own sign string, the Discord rules block) and fails when one lacks the run-your-own link to the install guide, closing #320's "every flagship surface" as a rule that stays true.

## Acceptance
The fixture surface with no link fails with its path; the committed surfaces pass (a surface that does not exist yet is listed as pending, not failed). Red first.

## Notes
Rule 10: a new validator is its own file in `tools/validation/`; declare its inputs so `validate` skips it when unchanged. Fix no surface here: report the missing ones as follow-up cards.

# Slice BEATS: The opening-day beat sheet for the integration run
## Card
- type: docs
- tier: T0
- model: haiku
- machines: none
- budget: 30 min / 35 calls
- owns: `docs/playtest/m4-opening-day.md`
- depends: none
- acceptance: proof validate (docs links) and the checklist in Acceptance
- risk: none
- release-blocker: no

## Capability
The chapter's beat sheet: from `host ready` and a password join, through the rules board, the quiz, a police application approved in the GM panel, a 2-hour day, the 06:00/18:00 restart toast and a clean restart, to a wipe with notice and the site's next session time; each beat names the slice issue it proves and what to observe.

## Acceptance
Checklist: every beat cites an issue under #1006; two-player, restart and late-join beats are marked; no beat needs filming or the siren; `tools\cli validate` clean.

## Notes
The integration agent and `tools\cli playtest checklist` read it once the chapter's slices merge (`docs/process/roadmap-pipeline.md` section 4).

# Slice TOWN: Town placement: M4's systems placed in the polished town
## Card
- type: visual
- tier: T1
- model: sonnet
- machines: full-profile
- budget: 45 min / 60 calls
- owns: `addons/LifeFramework/Worlds/DebugWorld/DebugWorld_Layers/Town_M4.layer`
- depends: #1280
- acceptance: proof look town (before and after) and audit placement --strict
- risk: none
- release-blocker: no

## Capability
The chapter-close town placement: the rules board and the run-your-own board, the props M4's slices left in their debug-grid cells, stand in DebugWorld's polished town where a player meets them on arrival, to the polish bar.

## Acceptance
`tools\cli look` of the town square before and after, the `polish` checklist against the pictures, `tools\cli audit placement --strict` clean; both pictures in the PR.

## Notes
Maintainer decision 2026-11-15: DebugWorld has a fenced debug grid (slices place props there with `tools\cli grid place`, ws/world-zones) and a polished town that only one chapter-close slice per chapter touches. This is M4's. No siren (toast only). Place with `tools\cli place`, never typed coordinates; read `polish`. If the town's layer name differs once ws/world-zones lands, use its town layer and say so in the PR.
