# Slices for #1006, wave 8 (2026-11-15).
# Only 2 M4 slices were ready. This wave takes the lead's follow-ups first: the nightSpeed gap in the day-cycle
# page (#1650, DAYDOC), the opening-gate sheet against `host gate` (#1625, #1640, GATEDOC), the appeals page once
# `host bans` lands (#1623, APPEALSPLANNED), validate-ops-commands scanning docs/community (VOPSCOMM), the flagship
# start that times out on lf-cloud with no console.log (#1647, FLAGSTART) and the C:\LF\b2-hosts scratch folders
# on lf-cloud (CLEANB2). Then the epic acceptance items with no card: #379 a fresh self-hosted run from the guide
# (SELFHOSTDRY), #556 the wipe dry run on lf-testbox (WIPEDRYRUN), #313 a moderator ruling from the guide alone
# (MODDRYRUN), #316 one session's feedback into linked drafts (FEEDBACKDRY), and the decision guards no check held
# yet: the 2 h day and the 06:00/18:00 restarts on the live host (DAYREADY), the exact server name on every page
# (SERVERNAME), the next session time only on the site (SITENOSTATUS). For the opening-day practice run (#1608, now
# waiting on #1600 alone): FLAGSTART is #1600's live failure, and SCRATCHROSTER lets a scratch host apply the
# flagship config while the committed roster's administrator uid is still pending (#1647 had to copy the repo).
# Card rule (lead, 2026-11-15): every card owns its wiring point and names it under "Wiring" in Notes.
# Maintainer decisions followed: the password beat is manual, and so is the ban leg (#1571); wipes need 24 h notice;
# the site shows the next session time only; restarts at 06:00 and 18:00 with a toast only; a 2 h in-game day; the
# server name "Life Framework | Everon 1989 (Alpha)"; the maintainer is the only staff; new world props in the debug
# grid (no card here places one). No town-pass, placement, video or M5 cards. No issue is re-milestoned.
# Left out on purpose: run-your-own links (validate-run-your-own already passes on all 7 surfaces), the #318 listing
# check (#1461 running), #314 (#1570 ready, #1569 blocked on #1288).
# #1006 is at GitHub's 100 sub-issue cap: Ops cards go under #230, Staff under #231, Showcase under #232
# (`slice new 1006 --plan <part> --attach <epic>`, one run per epic; the plan is split at the comment lines below).
# Created 2026-11-15 under #230: DAYDOC #1665, VOPSCOMM #1666, FLAGSTART #1667, CLEANB2 #1668, SELFHOSTDRY #1669,
# WIPEDRYRUN #1670, DAYREADY #1671, SCRATCHROSTER #1672, CHEATSYNC #1673; under #231: GATEDOC #1674, APPEALSPLANNED
# #1675 (waits on #1609), MODDRYRUN #1677, FEEDBACKDRY #1678; under #232: SERVERNAME #1679, SITENOSTATUS #1680.

# --- attach #230 (Ops) ---

# Slice DAYDOC: The day-cycle page says nightSpeed is not supported
## Card
- type: docs
- tier: T0
- model: haiku
- machines: none
- budget: 20 min / 25 calls
- owns: `docs/owner/day-cycle.md`, `docs/config/config.schema.json`
- depends: none
- acceptance: fixture website/scripts/check-links.mjs
- risk: none
- release-blocker: no

## Capability
An owner reading the day-cycle page learns that `nightSpeed` does nothing: the vanilla time manager has one day
duration and no night rate (#1650), so a value other than 1 logs a Warn and the night runs at the day's rate.

## Acceptance
`docs/owner/day-cycle.md` no longer says `nightSpeed` multiplies the night (lines 42, 72 and 91 today); the field
table marks it "not supported (#1650)"; the schema's `nightSpeed` description says the same and keeps the field so
old configs still load. `node website/scripts/check-links.mjs` and `tools\cli validate` pass.

## Notes
Wiring: the page is published by `website/scripts/build-content.mjs` from `docs/`, and the schema description is what
the site's config reference (`website/src/pages/ConfigReference.tsx`) renders; editing both files is the wiring.
Quote the Warn line text from `LF_DayCycleApply.c` exactly. Leave `docs/config/config.example.json` at 1.

# Slice VOPSCOMM: validate-ops-commands scans docs/community and knows a numbered planned marker
## Card
- type: tooling
- tier: T0
- model: sonnet
- machines: none
- budget: 35 min / 45 calls
- owns: `tools/validation/validate-ops-commands.mjs`, `tools/validation/fixtures/ops-commands/**`
- depends: none
- acceptance: fixture tools/validation/fixtures/ops-commands
- risk: none
- release-blocker: no

## Capability
The check that ops pages name real commands also reads `docs/community/*.md`, accepts `*(planned, #N)*` as a
planned marker (today `docs/ops/host-trial.md:101` warns although it is marked), and warns when a planned marker
names a command that now exists, so stale markers show up.

## Acceptance
New fixtures: a community page with an unknown unmarked command warns; `*(planned, #N)*` on a missing command is
clean; `*(planned)*` on an existing command warns "stale planned marker". Each seen red before the change. The
`// inputs:` header lists `docs/community/*.md`. Stays warn-only.

## Notes
Wiring: the validator itself is the wiring (`tools\cli validate` runs every script in `tools/validation/`); the scan
list is `PAGES` and the `docs/ops` readdir near line 114. Do not edit the pages it warns about: list the warnings in
the PR body for the lead.

# Slice FLAGSTART: host start --flagship fails fast and says why when no console.log appears
## Card
- type: tooling
- tier: T0
- model: opus
- machines: test
- budget: 45 min / 60 calls
- owns: `tools/host/host.mjs`, `tools/validation/host-start-nolog-selftest.mjs`, `tools/validation/fixtures/host-start-nolog/**`
- depends: none
- acceptance: fixture tools/validation/host-start-nolog-selftest.mjs
- risk: none
- release-blocker: no

## Capability
On lf-cloud, `host start --flagship --wait` with an applied flagship `server.json` timed out at 420 s with no
console.log (#1647). Find why on lf-cloud and make the start stop within 60 s of no log, naming the cause: the
process exited (with its code), the profile log folder was never created, or the args the server got.

## Acceptance
The selftest drives the wait with a stand-in server that writes no log and one that exits early: each ends inside
the short budget with a line naming the cause, red first. The PR states the root cause found on lf-cloud with the
log or process evidence, and whether it is `-config` with the join password needing the backend (#1520; `host.mjs`
line 38 already drops `-config`), the applied `server.json`, or the machine.

## Notes
Wiring: `tools/host/host.mjs` is the start path `tools\cli host start` calls; the wait and the launch args live
there. Reproduce with a fresh scratch host under `C:\LF\scratch\flagstart-<stamp>` and remove it with
`node tools/safe-rm.mjs` when done. A fix outside host.mjs (the applied config, #1520) is a new card, not this one.
Opus: the cause is unknown and the lane is lf-cloud.

# Slice CLEANB2: lf-cloud's b2-hosts scratch folders are removed and scratch hosts have one home
## Card
- type: tooling
- tier: T0
- model: haiku
- machines: none
- budget: 20 min / 25 calls
- owns: `docs/ops/scratch-hosts.md`
- depends: none
- acceptance: fixture website/scripts/check-links.mjs
- risk: none
- release-blocker: no

## Capability
`C:\LF\b2-hosts\*` on lf-cloud (left by #1647's live run) is gone, removed with `node tools/safe-rm.mjs` over ssh,
never `rm -rf`. A short page says where a scratch host goes (`C:\LF\scratch\<slug>-<stamp>`), that the agent that
made it removes it with safe-rm, and how to list leftovers on a pool machine.

## Acceptance
`ssh lf-cloud` lists no `C:\LF\b2-hosts`; the PR quotes the listing before and after. The page passes check-links
and validate-ops-commands.

## Notes
Wiring: a new page in `docs/ops/` is picked up by glob (validate-ops-commands' `docs/ops` readdir and the site's
docs build); the runbook link waits for #1350 and #1557, which own the runbook. Check first that no process on lf-cloud holds a file in the folders (#1600's
agent may still be live; if `agent:working` is on #1600, ask in a comment on it before deleting).

# Slice SELFHOSTDRY: A fresh self-hosted server from the install guide alone (#379)
## Card
- type: docs
- tier: T0
- model: sonnet
- machines: test
- budget: 45 min / 50 calls
- owns: `docs/ops/selfhost-dryrun-2026-11.md`, `docs/owner/install.md`
- depends: none
- acceptance: fixture website/scripts/check-links.mjs
- risk: none
- release-blocker: no

## Capability
#379's acceptance: someone who has not run the framework follows `docs/owner/install.md` on a fresh folder on
lf-testbox and reaches a joinable server. The record gives the time taken and each step where they stumbled.

## Acceptance
The record holds start and end times, the commands as typed, the success log lines seen (or the first failure), and
one row per stumble with the guide line it came from. check-links passes. Every stumble that needs more than a
wording fix becomes a card under #230.

## Notes
Wiring: a "Tried on a fresh host" line in `docs/owner/install.md` linking the record; small wording fixes to
install.md are in scope. Work from the guide only: no repo tools beyond what it names. Remove the folder with
`node tools/safe-rm.mjs` at the end.

# Slice WIPEDRYRUN: The wipe dry run on lf-testbox, with the 24 h notice (#556)
## Card
- type: docs
- tier: T0
- model: sonnet
- machines: test
- budget: 40 min / 45 calls
- owns: `docs/ops/wipe-dryrun-2026-11.md`, `docs/community/wipe-policy.md`
- depends: none
- acceptance: fixture website/scripts/check-links.mjs
- risk: none
- release-blocker: no

## Capability
#556's acceptance on a real machine: on a scratch host on lf-testbox, a wipe without a 24 h notice is refused, a
notice aged past 24 h (as the B9 beat script ages it) lets `host wipe` run, the backup is written, the saves are
cleared, the server boots clean, and `host restore` brings the old save back.

## Acceptance
The record has one line per step with the command, its SUMMARY line and the log line that shows it (`save loaded:`
before, a clean boot after the wipe, `save loaded:` after the restore). check-links passes.

## Notes
Wiring: a "Dry run" line in `docs/community/wipe-policy.md` linking the record. The proof verb `wipe-restore` (#1562)
and `tools/playtest/opening/b9-*.mjs` (#1492) are the reference; do not edit them. Scratch host only, removed with
safe-rm at the end.

# Slice DAYREADY: host ready checks the 2 h day and the 06:00 and 18:00 restarts
## Card
- type: tooling
- tier: T0
- model: sonnet
- machines: none
- budget: 35 min / 45 calls
- owns: `tools/host/ready.mjs`, `tools/validation/host-ready-config-selftest.mjs`
- depends: none
- acceptance: fixture tools/validation/host-ready-config-selftest.mjs
- risk: none
- release-blocker: no

## Capability
Before a session, `host ready` also reads the live `config.json` and fails when the day cycle is not `accelerated`
with `dayLengthMinutes: 120`, or the restart times are not exactly 06:00 and 18:00 with the toast only.

## Acceptance
New selftest cases, each red first: a 60-minute day fails "day cycle", a third restart time fails "restarts", a
siren warning fails "restarts", the flagship's committed `server/flagship/config.json` passes.

## Notes
Wiring: `configCheck` in `tools/host/ready.mjs` (the check list `host ready` runs, #1559) gets the new checks; keep
one `{ name, run }` per check. Read the values from the constants `tools/validation/flagship-decisions-selftest.mjs`
uses if it exports them; otherwise name them once at the top of ready.mjs.

# Slice SCRATCHROSTER: host config apply takes a roster file for a scratch host
## Card
- type: tooling
- tier: T0
- model: sonnet
- machines: none
- budget: 35 min / 45 calls
- owns: `tools/host/config-apply.mjs`, `tools/validation/host-config-apply-selftest.mjs`
- depends: none
- acceptance: fixture tools/validation/host-config-apply-selftest.mjs
- risk: none
- release-blocker: no

## Capability
`tools\cli host config apply --roster <file>` uses that roster instead of `server/flagship/staff.txt`, so a
rehearsal on a scratch host applies the flagship config while the committed administrator uid is still `#pending`.
It refuses `--roster` on the real host folder, as the beat scripts refuse it.

## Acceptance
Selftest cases, red first: `--roster` with a filled uid applies; the committed roster still refuses with `#pending`;
`--roster` on the real host folder refuses.

## Notes
Wiring: the roster path is read at `tools/host/config-apply.mjs:123`; the flag parse is in the same file (check
`tools/host/commands/config.mjs` only passes args through; if it parses flags itself, stop and report). #1647 needed a
copied repo for this; the opening run #1608 and #1503 need it on scratch hosts.

# Slice CHEATSYNC: The cheatsheet and the host-trial sheet drop planned markers of merged commands
## Card
- type: docs
- tier: T0
- model: haiku
- machines: none
- budget: 20 min / 25 calls
- owns: `docs/ops/session-cheatsheet.md`, `docs/ops/host-trial.md`
- depends: none
- acceptance: fixture website/scripts/check-links.mjs
- risk: none
- release-blocker: no

## Capability
The maintainer's one-page cheatsheet (12 `*(planned)*` markers today) and the 0grind trial sheet mark planned only
what has not merged: each marker whose command now exists under `tools/host/commands/` or `tools/commands/` goes,
and each that stays carries its card number, `*(planned, #N)*`.

## Acceptance
`node tools/validation/validate-ops-commands.mjs` prints no warning for the two pages; check-links passes.

## Notes
Wiring: both pages are in validate-ops-commands' `docs/ops` scan and the site's docs build. `host listing` is #1461,
`host gate` #1410, `host bans` #1609 (open today).

# --- attach #231 (Staff) ---

# Slice GATEDOC: The opening-gate sheet matches host gate
## Card
- type: docs
- tier: T0
- model: haiku
- machines: none
- budget: 20 min / 25 calls
- owns: `docs/community/opening-gate.md`
- depends: none
- acceptance: fixture website/scripts/check-links.mjs
- risk: none
- release-blocker: no

## Capability
The sheet's four lines are the four `host gate` checks (#1640): the fourth reads "Administrator in the roster" and
passes when `server/flagship/staff.txt` names an administrator with a uid (not `#pending`); trial moderators are
listed under it as information only, not a condition. The regression line is named "No open regression". The
password refusal is a manual line (#1520 is held).

## Acceptance
The line names match the `host gate` PASS/FAIL names in PR #1640; `staff-roster.json` is gone (the roster is
`staff.txt`); `host history` loses its planned marker (merged, #1409); `host gate` is `*(planned, #1410)*` until it
merges. check-links and validate-ops-commands pass.

## Notes
Wiring: the sheet is the comment the maintainer pastes on #555 and is linked from `docs/community/alpha.md`; keep the
fenced block shape. Read `tools/host/gate.mjs` from PR #1640's branch for the exact names.

# Slice APPEALSPLANNED: The appeals page drops its planned markers once host bans lands
## Card
- type: docs
- tier: T0
- model: haiku
- machines: none
- budget: 15 min / 20 calls
- owns: `docs/community/appeals.md`
- depends: #1609
- acceptance: fixture website/scripts/check-links.mjs
- risk: none
- release-blocker: no

## Capability
`docs/community/appeals.md` names `tools\cli host bans --player <uid>` and `--open` as merged commands (lines 105 and
120 today say *(planned)*), with the flags exactly as `host bans` takes them. The ban leg of an appeal stays manual
(#1571).

## Acceptance
No `planned` marker for `host bans` is left; the flags match `tools/host/commands/bans.mjs`; check-links and
validate-ops-commands pass.

## Notes
Wiring: the page is linked from the moderator guide and the Discord appeals channel text; this card edits only the
page.

# Slice MODDRYRUN: A moderator rules on the staged incident from the guide alone (#313)
## Card
- type: docs
- tier: T0
- model: haiku
- machines: none
- budget: 25 min / 30 calls
- owns: `docs/community/moderator-drill.md`, `docs/community/moderator-drill-run-2026-11.md`
- depends: none
- acceptance: fixture website/scripts/check-links.mjs
- risk: none
- release-blocker: no

## Capability
#313's acceptance as a desk run: a reader with only `docs/community/moderator-guide.md` takes the staged incident
in `moderator-drill.md` and writes the ruling, the rule cited and the log entry the guide asks for; the drill's
answer key marks it right or wrong.

## Acceptance
The run record holds the ruling, the guide sections used, the key's verdict and every place the guide was silent.
check-links passes. A guide gap becomes a card under #231 (the guide is #1557's to edit).

## Notes
Wiring: a "Runs" line in `moderator-drill.md` linking the record. The reader reads nothing but the guide and the
drill's incident text; the key is read after the ruling is written. The in-game half (a real moderator) stays
manual for the maintainer.

# Slice FEEDBACKDRY: One session's feedback becomes linked issue drafts (#316)
## Card
- type: docs
- tier: T0
- model: sonnet
- machines: none
- budget: 35 min / 40 calls
- owns: `docs/community/feedback-dryrun-2026-11.md`, `docs/community/discord.md`
- depends: none
- acceptance: fixture website/scripts/check-links.mjs
- risk: none
- release-blocker: no

## Capability
#316's acceptance rehearsed: a scratch host with one recorded session and five feedback posts (bug, idea, question,
playtest, a duplicate) goes through `tools\cli host feedback`; the drafts link back to their session (#1611) and the
duplicate is folded.

## Acceptance
The record holds the input posts, the drafts as printed, and for each draft the session link and the label. No
issue is filed on GitHub. check-links passes.

## Notes
Wiring: a link to the record from the `#feedback` row's section in `docs/community/discord.md`
(`feedback-triage.md` is #1556's). Build the scratch host from `tools/validation/fixtures/` of the host feedback
selftest if one fits; remove it with safe-rm.

# --- attach #232 (Showcase) ---

# Slice SERVERNAME: validate fails when a page misnames the flagship server
## Card
- type: tooling
- tier: T0
- model: sonnet
- machines: none
- budget: 35 min / 45 calls
- owns: `tools/validation/validate-flagship-name.mjs`, `tools/validation/fixtures/flagship-name/**`
- depends: none
- acceptance: fixture tools/validation/fixtures/flagship-name
- risk: none
- release-blocker: no

## Capability
Every page that names the flagship's browser listing (docs, `website/src`, `website/content`, `server/flagship/`)
writes it exactly as `server/flagship/server.json` has it, "Life Framework | Everon 1989 (Alpha)". A near miss (a
colon for the bar, no "(Alpha)", other spacing) fails with its path and line.

## Acceptance
Fixtures `near-miss.md` fails and `exact.md` passes, red first; the repo passes today, or the PR lists each page it
flags as a warning and stays warn-only.

## Notes
Wiring: the validator is the wiring (`tools\cli validate` runs every script in `tools/validation/`); declare
`// inputs:` and a `// red-proof:` line like `validate-run-your-own.mjs`. The Workshop project name "Life Framework:
Everon 1989" is the pack's title, not the listing: match only strings ending "(Alpha)" or following "server
browser".

# Slice SITENOSTATUS: validate keeps the site to the next session time
## Card
- type: tooling
- tier: T0
- model: sonnet
- machines: none
- budget: 30 min / 40 calls
- owns: `tools/validation/validate-site-next-session.mjs`, `tools/validation/fixtures/site-next-session/**`
- depends: none
- acceptance: fixture tools/validation/fixtures/site-next-session
- risk: none
- release-blocker: no

## Capability
The decision (2026-11-14) as a check: `website/src/pages/Servers.tsx`, `Alpha.tsx` and `Join.tsx` show the next
session from `website/content/sessions.json` and nothing live: no fetch to a status endpoint, no A2S query, no player
count field.

## Acceptance
Fixtures: a page that fetches a status URL fails, a page with a `players` count from a live source fails, the real
pages pass. Red first.

## Notes
Wiring: the validator is the wiring (validate runs the folder); `// inputs:` names the three pages and
`website/content/sessions.json`. `website/src/lib/serverbundle.ts` is the generator's server bundle (its `a2s` port is
config, not a query): do not flag it.
