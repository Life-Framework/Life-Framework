# Slices for #1006, wave 7 (2026-11-15).
# Fewer than 5 M4 slices were ready. This wave cuts the epic acceptance items that still had no card and the
# integration cards toward the opening-day run (#1503): beat scripts for the desk halves of B2, B3, B7 and B8, an
# `opening --game` mode that runs the game beats' proof verbs, a partial opening run on today's main, and the beat
# sheet brought in line with the decisions. Epic items covered: #305 (RUNBOOKDRY), #311 (RENTDRY), #309 and #380
# together (DOCTORSUMMARY), #315 (BANLIST, APPEALDOC), #316 (FEEDBACKLINK), #555 (GATESHEET), #312 and #320 Discord
# rules surface (RULESDISCORD), #319 link previews (SITEEMBED), #556 wipe notice on the site (WIPESITE).
# Card rule from the lead (2026-11-15): four slices shipped "not wired" because their owns-list left out the one
# file that calls the new code. Every card below owns its wiring point and names it under "Wiring" in Notes: the
# command file (tools/host/commands/<sub>.mjs, tools/commands/<name>.mjs), the beat-script glob in
# tools/playtest/opening/, the validate glob in tools/validation/, or the caller it edits.
# Maintainer decisions followed: password-only alpha with the join password check backend-only (the password beat
# stays manual, #1520); wipes need 24 h notice; no Discord linking; the site shows the next session time only;
# police missions after opening; the live config in server/flagship; a 2 h in-game day; restarts at 06:00 and 18:00
# with a toast only; the server name "Life Framework | Everon 1989 (Alpha)"; the maintainer is the only staff; new
# world props in the debug grid (no card here places one). No town-pass, placement, video or M5 cards. No issue is
# re-milestoned.
# #1006 is at GitHub's 100 sub-issue cap: Ops cards go under #230, Staff under #231, Showcase under #232
# (`slice new 1006 --plan <part> --attach <epic>`, one run per epic; the plan is split at the comment lines below).
# Created 2026-11-15 under #230: SHEETSYNC #1598, OPENGAME #1599, BEAT2 #1600, BEAT3 #1601, BEAT6 #1602, BEAT7 #1603, BEAT8 #1604, DOCTORSUMMARY #1605, RUNBOOKDRY #1606, RENTDRY #1607, OPENPART #1608; under #231: BANLIST #1609, APPEALDOC #1610, FEEDBACKLINK #1611, GATESHEET #1612; under #232: RULESDISCORD #1613, SITEEMBED #1615, WIPESITE #1616.
# Waiting cards re-checked: ready now #1568, #1571; still blocked #1557 (#1350, #1462, #1496 running), #1564 and #1565 (#1301 <- #1300), #1569 (#1288), #1570 (#1289).

# --- attach #230 (Ops) ---

# Slice SHEETSYNC: The opening-day beat sheet follows the decisions and today's main
## Card
- type: docs
- tier: T0
- model: haiku
- machines: none
- budget: 20 min / 25 calls
- owns: `docs/playtest/m4-opening-day.md`
- depends: none
- acceptance: fixture website/scripts/check-links.mjs
- risk: none
- release-blocker: no

## Capability
The beat sheet the integration run (#1503) walks says what main does: B2's wrong-password join is a manual check
(the join password check is backend-only, #1520), B7 drops the siren fallback line (#1266 is held: toast only), and
every command and slice number it names is on main or marked with the card that adds it.

## Acceptance
`node website/scripts/check-links.mjs` and `tools\cli validate` (validate-ops-commands) pass; the B2 and B7 lines
read as above.

## Notes
Wiring: the sheet is read by `tools/commands/opening.mjs` (`--sheet` default); keep the `## Bn` headings and the
`- [ ]` lines in the shape it parses. Do not touch the "Fixed decisions" table except to add the #1520 note.

# Slice OPENGAME: opening --game runs the game beats' proof verbs on a two-client session
## Card
- type: tooling
- tier: T0
- model: sonnet
- machines: none
- budget: 45 min / 55 calls
- owns: `tools/commands/opening.mjs`, `tools/validation/opening-game-selftest.mjs`, `tools/validation/fixtures/opening-game/**`
- depends: none
- acceptance: fixture tools/validation/opening-game-selftest.mjs
- risk: none
- release-blocker: no

## Capability
`tools\cli opening --game` runs the proof verb each game beat names (B4 quiz and quiz-rejoin, B5
police-application, B6 day-rate, B8 restart-cycle) through `node agent-box/harness.mjs proof <verb>` on the session
that is up, and turns each result into `BEAT <Bn>` PASS, FAIL or BLOCKED lines in the same report. A verb that is
not on main is BLOCKED by its card, never a FAIL.

## Acceptance
Selftest with a fixture proof runner: a passing verb gives PASS, a failing one FAIL with its reason, a missing verb
BLOCKED by its card number; the report keeps the desk beats' lines. Red first.

## Notes
Wiring: the flag lives in `tools/commands/opening.mjs`, the file `tools\cli opening` already loads; no new command
file. Read the verb per beat from the sheet's "Run the `<verb>` proof verb" lines, not from a list in code. It never
runs `session up` itself; it prints the line to run when no session is up.

# Slice BEAT2: Opening-day beat script B2, the flagship start
## Card
- type: tooling
- tier: T0
- model: sonnet
- machines: test
- budget: 40 min / 50 calls
- owns: `tools/playtest/opening/b2-start.mjs`, `tools/validation/opening-b2-selftest.mjs`, `tools/validation/fixtures/opening-b2/**`
- depends: none
- acceptance: fixture tools/validation/opening-b2-selftest.mjs
- risk: none
- release-blocker: no

## Capability
`node tools/playtest/opening/b2-start.mjs --host <scratch dir>` runs the desk half of B2: `host start --flagship`
on the scratch host, the `host: up` and `save loaded:` lines, the server name exactly "Life Framework | Everon 1989
(Alpha)", a join password set, and `host watch` running. The two join checks print BLOCKED by #1520 (manual).

## Acceptance
Selftest on fixture logs and a fixture server.json: a good start gives PASS for each check, a wrong name and an
empty password FAIL, the join lines BLOCKED by #1520. Red first. One live run on the test machine in the PR.

## Notes
Wiring: `tools\cli opening` finds `tools/playtest/opening/*.mjs` by glob, so the script file is the wiring point.
Reuse `tools/host/ready.mjs` exports for the name and password checks; do not edit it.

# Slice BEAT3: Opening-day beat script B3, the rules match everywhere
## Card
- type: tooling
- tier: T0
- model: sonnet
- machines: none
- budget: 30 min / 40 calls
- owns: `tools/playtest/opening/b3-rules.mjs`, `tools/validation/opening-b3-selftest.mjs`, `tools/validation/fixtures/opening-b3/**`
- depends: none
- acceptance: fixture tools/validation/opening-b3-selftest.mjs
- risk: none
- release-blocker: no

## Capability
`node tools/playtest/opening/b3-rules.mjs` runs the desk half of B3: `rules export` and the site's
`website/content/rules.json` and the Discord block carry the same rule text. The two board lines print BLOCKED by
#1280 (the town board is a town-pass card, out of this wave).

## Acceptance
Selftest on fixtures: matching texts PASS, one changed rule FAILs naming it, a missing Discord block file is BLOCKED
by its card. Red first.

## Notes
Wiring: the beat-script glob in `tools/playtest/opening/`. Read the Discord block from
`docs/community/discord-rules.md` when it exists (RULESDISCORD, #232), else from `tmp/rules-discord.md` after export.

# Slice BEAT6: Opening-day beat script B6, the desk half of the 2-hour day
## Card
- type: tooling
- tier: T0
- model: sonnet
- machines: none
- budget: 30 min / 40 calls
- owns: `tools/playtest/opening/b6-day.mjs`, `tools/validation/opening-b6-selftest.mjs`, `tools/validation/fixtures/opening-b6/**`
- depends: none
- acceptance: fixture tools/validation/opening-b6-selftest.mjs
- risk: none
- release-blocker: no

## Capability
`node tools/playtest/opening/b6-day.mjs --host <scratch dir>` checks what B6 can check from the desk: the flagship
config's `dayCycle` is accelerated with a 120-minute day, and `host perf` has CPU, memory and upload samples for the
session. The clock-rate and late-join lines print BLOCKED by #1301 and #1303 until those merge, then name the
`day-rate` verb for `opening --game`.

## Acceptance
Selftest: the committed config PASSes, a 60-minute fixture FAILs, an empty perf CSV FAILs. Red first.

## Notes
Wiring: the beat-script glob in `tools/playtest/opening/`.

# Slice BEAT7: Opening-day beat script B7, the warnings and the close
## Card
- type: tooling
- tier: T0
- model: sonnet
- machines: test
- budget: 45 min / 55 calls
- owns: `tools/playtest/opening/b7-restart.mjs`, `tools/validation/opening-b7-selftest.mjs`, `tools/validation/fixtures/opening-b7/**`
- depends: none
- acceptance: fixture tools/validation/opening-b7-selftest.mjs
- risk: none
- release-blocker: no

## Capability
`node tools/playtest/opening/b7-restart.mjs --host <scratch dir>` sets a test restart a few minutes ahead with
`b7-restart-setup.mjs`, starts the scratch host, and judges the log: each configured warning published (toast only,
no siren), the save before the close, the close, and the watchdog bringing the server back.

## Acceptance
Selftest on fixture logs: a full cycle gives PASS per check; a log missing a warning or the save FAILs naming it.
Red first. One live run on the test machine in the PR.

## Notes
Wiring: the beat-script glob in `tools/playtest/opening/`. Use `b7-restart-setup.mjs` as it is (a helper, not a
beat). The test restart goes in the scratch profile's config copy, never `server/flagship/config.json`.

# Slice BEAT8: Opening-day beat script B8, the clean restart and the reload
## Card
- type: tooling
- tier: T0
- model: sonnet
- machines: test
- budget: 40 min / 50 calls
- owns: `tools/playtest/opening/b8-reload.mjs`, `tools/validation/opening-b8-selftest.mjs`, `tools/validation/fixtures/opening-b8/**`
- depends: none
- acceptance: fixture tools/validation/opening-b8-selftest.mjs
- risk: none
- release-blocker: no

## Capability
`node tools/playtest/opening/b8-reload.mjs --host <scratch dir>` stops and starts the scratch host and judges the
second boot: `save loaded:`, no `Unable to find` or `Wrong GUID/name`, and `host password` rotated a new password
into the scratch config. The time-continuation check prints BLOCKED by #1302, the player rejoin check names the
`restart-cycle` verb (#1268) for `opening --game`.

## Acceptance
Selftest on fixture logs: a clean reload PASSes, a log with `Unable to find` FAILs with the line. Red first. One
live run on the test machine in the PR.

## Notes
Wiring: the beat-script glob in `tools/playtest/opening/`.

# Slice DOCTORSUMMARY: host summary carries server-doctor's verdict on the session log
## Card
- type: tooling
- tier: T0
- model: sonnet
- machines: none
- budget: 35 min / 45 calls
- owns: `tools/host/summary.mjs`, `tools/validation/host-summary-doctor-selftest.mjs`, `tools/validation/fixtures/host-summary-doctor/**`
- depends: none
- acceptance: fixture tools/validation/host-summary-doctor-selftest.mjs
- risk: none
- release-blocker: no

## Capability
The post-session report from `host summary` opens with server-doctor's verdict on the same log: booted or not,
mods loaded, the effective config's server name, and the known lines it marked. One report after a session covers
#309 (triage groups) and #380 (the doctor) together.

## Acceptance
Selftest: the fixture good-boot log puts "booted" and the mod list in the report; the missing-mod fixture puts the
doctor's fix line in it. Red first.

## Notes
Wiring: `tools/host/commands/summary.mjs` already calls `summarize()` in `tools/host/summary.mjs`; the doctor call
goes inside `summarize()`. Import server-doctor's pure function from `tools/commands/server-doctor.mjs` (export it
there only if it is not exported; that one-line export is in owns' spirit, say so in the PR).

# Slice RUNBOOKDRY: A second operator runs a scratch session from the runbook alone
## Card
- type: docs
- tier: T0
- model: sonnet
- machines: test
- budget: 45 min / 50 calls
- owns: `docs/playtest/runbook-dryrun-2026-11.md`
- depends: none
- acceptance: fixture website/scripts/check-links.mjs
- risk: none
- release-blocker: no

## Capability
#305's proof, done by an agent that did not write the page: start, announce (dry), restart and shut down a scratch
host on the test machine using only `docs/ops/session-runbook.md` and the cheatsheet, recording each command, its
output line and every step that was unclear or wrong.

## Acceptance
The record lists every runbook step with PASS or the stumble; each stumble is filed as a docs issue under #230 (the
runbook itself is owned by #1350 and #1557, so this card does not edit it). check-links passes.

## Notes
Wiring: link the record from a comment on #305 and from the PR body; no other page is edited. Scratch host only
(`LF_HOST_DIR`); never the live saves. `host announce` without `--send`.

# Slice RENTDRY: The rented-box checklist dry run on lf-testbox
## Card
- type: docs
- tier: T0
- model: sonnet
- machines: test
- budget: 45 min / 50 calls
- owns: `docs/ops/rented-box-dryrun-2026-11.md`
- depends: none
- acceptance: fixture website/scripts/check-links.mjs
- risk: none
- release-blocker: no

## Capability
#311's proof: walk `docs/ops/rented-box.md` on `lf-testbox` as if it were the rented box (ports, profile copy, save
copy, autostart, a boot from the copied save), recording each step's command and result.

## Acceptance
The record has one line per checklist step with PASS or the gap; gaps are filed as issues under #230; check-links
passes. `host boxcheck` (#1496) is BLOCKED in the record if it is not on main.

## Notes
Wiring: link the record from a comment on #311 and the PR body; `docs/ops/rented-box.md` is owned by #1557.

# Slice OPENPART: The opening-day partial run on today's main
## Card
- type: tooling
- tier: T0
- model: sonnet
- machines: session-2p
- budget: 45 min / 60 calls
- owns: `docs/playtest/m4-opening-partial-2026-11.md`
- depends: SHEETSYNC, OPENGAME, BEAT2, BEAT3, BEAT6, BEAT7, BEAT8, #1494
- acceptance: fixture website/scripts/check-links.mjs
- risk: none
- release-blocker: no

## Capability
A rehearsal of #1503 with what is merged: `tools\cli opening --host <scratch>` for the desk beats, then
`opening --game` on a two-client session, and the report kept in the repo. Every FAIL becomes a card under its
epic (`slice new 1006 --attach <epic>`); BLOCKED lines list what #1503 still waits on.

## Acceptance
The record holds the report with one line per beat and the card number for each FAIL; check-links passes.

## Notes
Wiring: the record is linked from a comment on #1503. Fix nothing here. `--profile full` is not needed (no shots).

# --- attach #231 (Staff) ---

# Slice BANLIST: host bans lists the ban record with its incidents
## Card
- type: tooling
- tier: T0
- model: sonnet
- machines: none
- budget: 35 min / 45 calls
- owns: `tools/host/bans.mjs`, `tools/host/commands/bans.mjs`, `tools/validation/host-bans-selftest.mjs`, `tools/validation/fixtures/host-bans/**`
- depends: none
- acceptance: fixture tools/validation/host-bans-selftest.mjs
- risk: none
- release-blocker: no

## Capability
`tools\cli host bans [--player <uid>] [--open]` reads the host's `$profile:LifeFramework/ban_record.txt` (written by
`LF_BanRecord.c`, #1285) and prints one row per ban and unban with reason, staff uid and incident ids; `--open`
shows only bans with no later unban. An appeal is answered from this list.

## Acceptance
Selftest on a fixture record: a ban then unban shows as closed, `--open` hides it, a malformed row is reported with
its line number and skipped. Red first.

## Notes
Wiring: `tools/host/commands/bans.mjs` is what `host bans` loads (`tools/host/commands/README.md`); `host help`
lists it. Read the row format from `LF_BanRecord.c`; do not change it.

# Slice APPEALDOC: The appeals page answers an appeal from host bans
## Card
- type: docs
- tier: T0
- model: haiku
- machines: none
- budget: 20 min / 25 calls
- owns: `docs/community/appeals.md`
- depends: BANLIST
- acceptance: fixture website/scripts/check-links.mjs
- risk: none
- release-blocker: no

## Capability
The appeals page says how the maintainer (the only staff) answers an appeal: find the ban with `host bans`, read its
incident, unban with `#unban` and the reason, which writes the unban row.

## Acceptance
validate-ops-commands passes with `host bans` named; check-links passes.

## Notes
Wiring: the page is already linked from the moderator guide and the alpha page; no link change needed.

# Slice FEEDBACKLINK: Feedback drafts link back to their session
## Card
- type: tooling
- tier: T0
- model: sonnet
- machines: none
- budget: 30 min / 40 calls
- owns: `tools/host/feedback.mjs`, `tools/validation/host-feedback-link-selftest.mjs`
- depends: none
- acceptance: fixture tools/validation/host-feedback-link-selftest.mjs
- risk: none
- release-blocker: no

## Capability
#316's "one session's feedback produces linked issues": each issue draft from `host feedback` names its session
date and the session's `host summary` report, and the drafts of one session list each other, so the filed issues
link back to the session and to one another.

## Acceptance
Selftest on the existing feedback fixture plus a fixture summary: every draft carries the session line and the
sibling list. Red first.

## Notes
Wiring: `tools/host/commands/feedback.mjs` already calls `tools/host/feedback.mjs`; no command change. Still no
author names or Discord ids in a draft.

# Slice GATESHEET: The opening-gate checklist names the proof for each line
## Card
- type: docs
- tier: T0
- model: haiku
- machines: none
- budget: 25 min / 30 calls
- owns: `docs/community/opening-gate.md`, `docs/community/alpha.md`
- depends: none
- acceptance: fixture website/scripts/check-links.mjs
- risk: none
- release-blocker: no

## Capability
The #555 gate as a checklist the maintainer fills in on the issue: several sessions at the player target, no save
loss, no blocking regression, trial moderators in place (after opening, since the maintainer is the only staff at
opening), and each line names its evidence (`host history`, `host summary`, `host drill`, the regression label
query). Lines whose command is not on main name the card (#1409, #1410, #1512).

## Acceptance
validate-ops-commands and check-links pass; the alpha page links the checklist.

## Notes
Wiring: the one link line in `docs/community/alpha.md`. The password removal stays `host open` (#1512); this page
does not describe a different path.

# --- attach #232 (Showcase) ---

# Slice RULESDISCORD: The Discord rules block is committed and links run-your-own
## Card
- type: tooling
- tier: T0
- model: sonnet
- machines: none
- budget: 30 min / 40 calls
- owns: `tools/commands/rules.mjs`, `docs/community/discord-rules.md`, `tools/validation/validate-run-your-own.mjs`
- depends: none
- acceptance: fixture tools/validation/validate-run-your-own.mjs
- risk: none
- release-blocker: no

## Capability
`tools\cli rules export` also writes `docs/community/discord-rules.md`, the block the maintainer pins on Discord,
ending with the run-your-own line. validate-run-your-own's last PENDING surface (`discord-rules`) becomes linked,
closing #320's "every flagship surface" and #312's Discord half.

## Acceptance
`node tools/validation/validate-run-your-own.mjs` reports 7 linked, 0 pending; deleting the line from the block
fails it. Red first (it is PENDING today).

## Notes
Wiring: the export path is in `tools/commands/rules.mjs` (the command file). The rules-drift check (#1513) must
still pass on the committed block.

# Slice SITEEMBED: A shared servers link previews the next session time
## Card
- type: capability
- tier: T0
- model: sonnet
- machines: none
- budget: 35 min / 45 calls
- owns: `website/src/entry-server.tsx`, `website/src/pages/Servers.tsx`, `website/scripts/check-links.mjs`
- depends: none
- acceptance: fixture website/scripts/check-links.mjs
- risk: none
- release-blocker: no

## Capability
When the servers page link is posted on Discord, its preview (og:description) says the next session time from
`website/content/sessions.json`, or "No session announced" when there is none. Next session time only: no player
count, no status (decision 2026-11-14).

## Acceptance
A check in check-links reads the built servers page and expects the next session's time in its og:description; red
first with the current static description.

## Notes
Wiring: the meta comes from the page's meta object that `entry-server.tsx` renders; set it in `Servers.tsx`. Use the
same "has the session passed" rule as #1514.

# Slice WIPESITE: The alpha page shows a wipe notice when one is announced
## Card
- type: capability
- tier: T0
- model: sonnet
- machines: none
- budget: 30 min / 40 calls
- owns: `website/src/pages/Alpha.tsx`
- depends: none
- acceptance: fixture website/scripts/check-links.mjs
- risk: none
- release-blocker: no

## Capability
When the next row of `website/content/sessions.json` has `wipe: true` (written by `host announce --wipe --send`,
#1413), the alpha page shows "The world is wiped before the session on <date>" with the 24-hour notice, so the
notice is on the site as well as Discord. No row, no notice.

## Acceptance
`npm --prefix website run build` and check-links pass; the built alpha page carries the notice with a fixture
sessions row and not without it (shown in the PR with both builds). Red first.

## Notes
Wiring: `Alpha.tsx` is the page the router already serves; it reads sessions.json the way `Servers.tsx` does. Do not
edit `Servers.tsx` (SITEEMBED owns it).
