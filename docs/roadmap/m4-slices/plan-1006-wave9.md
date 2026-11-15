# Slices for #1006, wave 9 (2026-11-15).
# 3 M4 slices were ready and 11 running. This wave takes the lead's collected follow-ups first:
# host feedback labels a question and a playtest report (#1717, FBLABELS, then FBTAG and FBFINDINGS from the
# dry-run record docs/community/feedback-dryrun-2026-11.md); the six moderator-guide gaps from the desk drill
# (#1707, docs/community/moderator-drill-run-2026-11.md "Cards to file under #231": gaps 1-3 in a new rulings page,
# MODRULINGS, startable now; gaps 4-6 and the link in the guide, MODGUIDE, after #1557, which owns the guide); the
# 0grind off-site backup as a scheduled `host remote 0grind saves pull` (OGPULLSCHED) and a 0grind section in the
# session runbook (OGRUNBOOK, after #1557, which owns the runbook); the opening-gate sheet's `host open` markers
# (GATEOPENDOC, after #1512).
# Already in flight, so no new card: host-trial.md:101 `host listing` marker (#1673, PR #1713); the day-cycle page's
# nightSpeed (#1665, PR #1696); the appeals page's `host bans` markers (#1675, ready). Opening-gate line 4 already
# matches `host gate`'s staff line (administrator uid, trial moderators information only; #1674 merged).
# Epic acceptance with no card: #230 "a fresh tested backup" on the rented box (OGREADY: a remote go / no-go that
# reads the newest pull's age and config drift); #232/#318 the server-browser listing of the box the alpha runs on
# (LISTING0G). #232's trailer cards are held (video); nothing else in #232 is uncovered.
# Card rule (lead, 2026-11-15): every card owns its wiring point and names it under "Wiring" in Notes.
# Maintainer decisions followed: the alpha runs on the rented 0grind box (docs/ops/rented-box.md, 2026-11-15);
# only the maintainer is staff; wipes need 24 h notice. No town-pass, placement, video or M5 cards. No issue is
# re-milestoned.
# #1006 is at GitHub's 100 sub-issue cap: Ops cards go under #230, Staff under #231, Showcase under #232
# (`slice new 1006 --plan <part> --attach <epic>`, one run per epic; the plan is split at the comment lines below).
# Created 2026-11-15 under #230: OGPULLSCHED #1725, OGREADY #1726, OGRUNBOOK #1727; under #231: FBLABELS #1728,
# FBTAG #1729, FBFINDINGS #1730, MODRULINGS #1731, MODGUIDE #1732, GATEOPENDOC #1733; under #232: LISTING0G #1734.

# --- attach #230 (Ops) ---

# Slice OGPULLSCHED: A scheduled saves pull keeps an off-site copy of the 0grind saves
## Card
- type: tooling
- tier: T0
- model: sonnet
- machines: none
- budget: 40 min / 50 calls
- owns: `tools/host/remote-schedule.mjs`, `tools/host/commands/remote.mjs`, `tools/validation/host-remote-schedule-selftest.mjs`
- depends: none
- acceptance: fixture tools/validation/host-remote-schedule-selftest.mjs
- risk: none
- release-blocker: no

## Capability
The flagship's saves live on the rented 0grind box, and 0grind's own backups skip world saves. `tools\cli host
remote 0grind saves pull --schedule [--every <hours>]` registers a Windows scheduled task on the operator's PC that
runs the pull every N hours (default 6) hidden, `--unschedule` removes it, and `--schedule-status` says whether it
is registered and when it last ran. After each scheduled pull the `-remote` pulls beyond the newest N (default 20,
tagged ones kept) are pruned, so the off-site copy cannot fill the drive.

## Acceptance
Selftest with an injected schtasks runner (as host-autostart-selftest does): `--schedule` builds the exact
`/Create` arguments with the interval and a `/TR` under 261 characters; `--unschedule` and `--schedule-status`
parse the fixture CSV; `--dry-run` prints the schtask without running it; the prune keeps the newest N `-remote`
pulls and every tagged one and never touches a local backup. Red first.

## Notes
Wiring: `tools/host/commands/remote.mjs` (the `saves pull` branch and the help line). Copy the runner shape from
`tools/host/autostart.mjs` (it is not edited here); the pull itself is `savesPull` in
`tools/host/remote-0grind.mjs` (not edited). Never run the real schtasks in a test.

# Slice OGREADY: host remote 0grind ready says go or no-go for the rented box
## Card
- type: tooling
- tier: T0
- model: sonnet
- machines: none
- budget: 40 min / 50 calls
- owns: `tools/host/remote-ready.mjs`, `tools/host/commands/remote.mjs`, `tools/validation/host-remote-ready-selftest.mjs`
- depends: OGPULLSCHED
- acceptance: fixture tools/validation/host-remote-ready-selftest.mjs
- risk: none
- release-blocker: no

## Capability
Before a session on the rented box, `tools\cli host remote 0grind ready [--max-pull-hours N]` prints one PASS/FAIL
line per check and `SUMMARY host-remote-ready go|no-go`, exit 1 on no-go: the API says the service exists, A2S
answers with the exact server name "Life Framework | Everon 1989 (Alpha)", a join password is set (from the masked
config show, never printed), the newest `-remote` saves pull is younger than N hours (default 12), `config push`
would change nothing (no drift from `server/flagship`), and the scheduled pull is registered. Read-only: it never
pushes, starts or stops.

## Acceptance
Selftest with an injected client, A2S reply and backups folder: each check fails alone on its fixture and the
summary is no-go; all pass gives go; the password value never appears in the output. Red first.

## Notes
Wiring: `tools/host/commands/remote.mjs` (a `ready` branch and the help line). Reuse `createClient`, `diffSettings`
and `FLAGSHIP_0GRIND` from `tools/host/remote-0grind.mjs` and `tools/host/a2s.mjs` without editing them. The local
`host ready` (`tools/host/ready.mjs`) is the model for the output shape.

# Slice OGRUNBOOK: The session runbook has a 0grind section
## Card
- type: docs
- tier: T0
- model: haiku
- machines: none
- budget: 25 min / 30 calls
- owns: `docs/ops/session-runbook.md`
- depends: #1557, OGPULLSCHED
- acceptance: fixture tools/validation/validate-ops-commands.mjs
- risk: none
- release-blocker: no

## Capability
A second operator can run an alpha session on the rented box from the runbook: a "On the 0grind box" section with
before (`host remote 0grind status`, `config push` as a diff then `--yes`, `system fix`, a fresh `saves pull`),
start and restart (`host remote 0grind start|restart --yes`), during (`rcon #players`), stop, and after
(`logs pull`, then `host triage`, `host summary` and `host attendance` on the pulled log), plus the scheduled
pull as the off-site backup (`saves pull --schedule`) and how to drill a pull (`host drill <pull>`).

## Acceptance
`node tools/validation/validate-ops-commands.mjs` passes (every command the section names is real; OGREADY's
`ready` is marked `*(planned, #N)*` until it merges); `node website/scripts/check-links.mjs` passes.

## Notes
Wiring: the runbook is published by the site's docs build and linked from the cheatsheet; editing it is the wiring.
Waits on #1557, which owns the runbook first. Flags exactly as `tools\cli host remote 0grind` prints its usage.

# --- attach #231 (Staff) ---

# Slice FBLABELS: host feedback drafts a question and a playtest report with their own labels
## Card
- type: tooling
- tier: T0
- model: sonnet
- machines: none
- budget: 35 min / 45 calls
- owns: `tools/host/feedback.mjs`, `tools/host/commands/feedback.mjs`, `tools/validation/host-feedback-selftest.mjs`
- depends: none
- acceptance: fixture tools/validation/host-feedback-selftest.mjs
- risk: none
- release-blocker: no

## Capability
`tools\cli host feedback` knows four kinds, not two (#1717): a question ("how do I", a question mark and no failure
words) is drafted with the `question` label and the note "answer in the channel" instead of a `gh issue create`
line; a playtest report (a post that lists what was tried, or starts "playtest") gets the `playtest` label and a
`[Playtest]:` title; bugs and suggestions keep `bug` and `enhancement`.

## Acceptance
The selftest feeds the five posts of `docs/community/feedback-dryrun-2026-11.md` (bug, duplicate, idea, question,
playtest report): the question and the report are no longer `[Bug]`, each draft has the label above, the duplicate
still folds. Red first.

## Notes
Wiring: `classify()` and the draft's `label` in `tools/host/feedback.mjs` (lines ~79 and ~248 today) and the help
text in `tools/host/commands/feedback.mjs`. Both labels exist on the repo (`question`, `playtest`).

# Slice FBTAG: The forum tag picks the feedback label before the words do
## Card
- type: tooling
- tier: T0
- model: sonnet
- machines: none
- budget: 30 min / 40 calls
- owns: `tools/host/feedback.mjs`, `tools/validation/host-feedback-selftest.mjs`
- depends: FBLABELS
- acceptance: fixture tools/validation/host-feedback-selftest.mjs
- risk: none
- release-blocker: no

## Capability
When the Discord export carries the forum post's tag (Bug, Idea, Question, Playtest), `host feedback` takes the
kind from the tag and guesses from the words only when there is none, so a tagged question is never a bug.

## Acceptance
Selftest: a post tagged Question whose words read like a bug drafts as a question; an untagged post still
classifies by its words; an unknown tag falls back to the words and the draft says so. Red first.

## Notes
Wiring: the export reader in `tools/host/feedback.mjs` (where posts are parsed) feeds the tag to `classify()`.
Read the forum tag field from a real DiscordChatExporter forum export's shape; put a two-post fixture in the selftest.

# Slice FBFINDINGS: A playtest report becomes one draft per finding
## Card
- type: tooling
- tier: T0
- model: sonnet
- machines: none
- budget: 30 min / 40 calls
- owns: `tools/host/feedback.mjs`, `tools/validation/host-feedback-selftest.mjs`
- depends: FBTAG
- acceptance: fixture tools/validation/host-feedback-selftest.mjs
- risk: none
- release-blocker: no

## Capability
A playtest report that lists several findings (bullets or numbered lines) is drafted as one candidate issue per
finding, each with the report's session line and a "from playtest report by <n>" note, instead of one draft a
person has to split.

## Acceptance
Selftest: the dry run's playtest report (three findings) gives three drafts with the session link; a one-line
report gives one; no Discord id or name reaches a draft. Red first.

## Notes
Wiring: the draft builder in `tools/host/feedback.mjs` (`drafts` / the grouping before it). The record of the dry
run (`docs/community/feedback-dryrun-2026-11.md`) has the input post.

# Slice MODRULINGS: A rulings page gives moderators the rule, the warning and the damage call
## Card
- type: docs
- tier: T0
- model: haiku
- machines: none
- budget: 30 min / 35 calls
- owns: `docs/community/moderator-rulings.md`, `docs/community/moderator-drill.md`
- depends: none
- acceptance: fixture website/scripts/check-links.mjs
- risk: none
- release-blocker: no

## Capability
Three of the six desk-drill gaps (#1707): a moderator ruling on an incident finds (1) each published rule (#312)
quoted with the ruling it usually gets, (2) a warnings section: where a written warning is recorded and how it is
delivered, and the ladder warning, kick, ban, and (3) when player-on-player damage breaks a rule, with the incident
log rows that settle it.

## Acceptance
`docs/community/moderator-rulings.md` exists with those three sections; every rule quoted matches the published
rules text (`validate-rules-sync` passes); the drill page links it from the step where the drill run lost points;
`node website/scripts/check-links.mjs` and `tools\cli validate` pass.

## Notes
Wiring: `docs/community/moderator-drill.md` links the page; the guide's link is MODGUIDE's (the guide is #1557's).
Source: `docs/community/moderator-drill-run-2026-11.md` gaps 1 to 3, the rules page, the incident kinds in the guide.

# Slice MODGUIDE: The moderator guide closes the drill's last three gaps
## Card
- type: docs
- tier: T0
- model: haiku
- machines: none
- budget: 25 min / 30 calls
- owns: `docs/community/moderator-guide.md`
- depends: #1557, MODRULINGS
- acceptance: fixture website/scripts/check-links.mjs
- risk: none
- release-blocker: no

## Capability
The rest of #1707: step 2 of "From report to ruling" asks the administrator for the clock time the panel was read
(gap 4); the guide says where a moderator gets an account uid for a connected player (gap 5: `tools\cli host
players` or the admin's `#players`, read the merged code for which shows it); step 6 lists the one-week evidence
note (gap 6); "The short version" links the rulings page.

## Acceptance
The four edits are in the guide; every command it names is real (`validate-ops-commands` passes); check-links passes.

## Notes
Wiring: the guide is linked from the trial-moderators page and the drill; editing it is the wiring. Waits on #1557,
which edits the guide's ban section first.

# Slice GATEOPENDOC: The opening-gate sheet names host open
## Card
- type: docs
- tier: T0
- model: haiku
- machines: none
- budget: 15 min / 20 calls
- owns: `docs/community/opening-gate.md`
- depends: #1512
- acceptance: fixture tools/validation/validate-ops-commands.mjs
- risk: none
- release-blocker: no

## Capability
The maintainer's opening-gate sheet says `tools\cli host open` as merged (lines 65 and 80 today say *(planned,
#1512)*), with its flags exactly as the command takes them (`--confirm`, `--dry-run`), and the "until it merges"
sentence goes.

## Acceptance
No `planned` marker is left on the page; validate-ops-commands and check-links pass.

## Notes
Wiring: the sheet is copied into #555 and linked from the alpha page; this card edits only the sheet.

# --- attach #232 (Showcase) ---

# Slice LISTING0G: host listing checks the rented box by name
## Card
- type: tooling
- tier: T0
- model: sonnet
- machines: none
- budget: 25 min / 35 calls
- owns: `tools/host/commands/listing.mjs`, `tools/validation/host-listing-selftest.mjs`
- depends: none
- acceptance: fixture tools/validation/host-listing-selftest.mjs
- risk: none
- release-blocker: no

## Capability
#318's server-browser listing is checked where the alpha runs: `tools\cli host listing --remote 0grind` queries the
box's A2S port from `FLAGSHIP_0GRIND` instead of a typed address, and compares the name, map, slots and password
flag with `server/flagship/server.json` as it does for a local host.

## Acceptance
Selftest: `--remote 0grind` resolves to the box's address and A2S port; an unknown remote name refuses with the
list of known ones; a typed address still wins. Red first.

## Notes
Wiring: `tools/host/commands/listing.mjs` (argument parsing and help). Import `FLAGSHIP_0GRIND` from
`tools/host/remote-0grind.mjs`; do not edit it.
