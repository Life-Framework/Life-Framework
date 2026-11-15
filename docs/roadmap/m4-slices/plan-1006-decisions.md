# Slices for #1006 from the maintainer's decisions of 2026-11-14 (docs/roadmap/m4-slice-plan.md, "Decisions (2026-11-14)").
# O17 was held on Q4; R1, C1, C2 and W1 are the slices the answers to Q1 to Q9 need and no card covered.
# Create with: tools\cli slice new 1006 --plan docs/roadmap/m4-slices/plan-1006-decisions.md
# Cross-plan dependencies the tool cannot express are added to the issue bodies after creation:
#   C2 needs K2 (#1259). Since the widening re-cut (plan-1006-widen.md) C1 needs CF (#1343, the `dayCycle` section)
#   instead of X1, and W1 and O17 are written from the plan with no dependency; O16R (#1350) reconciles O17.

# Slice R1: Staff roster file with a checker; the maintainer is the only administrator at opening
## Card
- type: tooling
- tier: T0
- model: sonnet
- machines: none
- budget: 35 min / 50 calls
- owns: `server/flagship/staff-roster.json`, `tools/commands/staff.mjs`, `tools/validation/staff-roster-selftest.mjs`
- depends: none
- acceptance: fixture tools/validation/staff-roster-selftest.mjs
- risk: none
- release-blocker: no

## Capability
`server/flagship/staff-roster.json` lists staff by level (`moderator`, `operator`, `administrator`) with a display name and an account identity id, and ships with the maintainer as the only administrator and nobody else. `tools\cli staff roster check` validates it; `tools\cli staff roster admins` prints the administrators' identity ids as the `game.admins` array for `server.json`.

## Acceptance
Selftest on fixture rosters: one administrator prints that id; an unknown level, a duplicate id or a malformed id fails `check`; a blank id is a warning on `check` and an error on `admins`. The committed file passes `check` (blank id warned, the maintainer fills it in). Red first.

## Notes
Decision 2026-11-14: only the maintainer is an administrator at opening; the staff tools ship with this roster to fill in later (#313). Only administrators have in-game powers (the #313 spec). The repo is public: names the person agrees to publish and identity ids only, no contact details. One command per file in `tools/commands/` (AGENTS.md rule 10). Filling in the maintainer's id is a human step.

# Slice C1: The flagship's live config.json and server.json in server/flagship, secrets left out
## Card
- type: data
- tier: T1
- model: sonnet
- machines: none
- budget: 35 min / 50 calls
- owns: `server/flagship/config.json`, `server/flagship/server.json`, `server/flagship/README.md`, `tools/validation/flagship-config-selftest.mjs`
- depends: CF
- acceptance: fixture tools/validation/flagship-config-selftest.mjs
- risk: none
- release-blocker: no

## Capability
The flagship's two live files are committed in `server/flagship/`, so every change is a PR. `config.json` carries the owner overrides, including `dayCycle` accelerated with a 120-minute day (12x); `server.json` carries the listing name `Life Framework | Everon 1989 (Alpha)`, the `LF_Everon1989.conf` scenario and the mod list. Both are free of secrets, and the README says which values are host-only.

## Acceptance
Selftest: `config.json` passes the config schema check (`docs/config/config.schema.json`); `server.json` has the exact name above and the same scenario and mod list as `server/example/server.json`; `game.password`, `game.passwordAdmin` and `game.admins` are empty, and no webhook URL or password-like value appears in either file. Red first: add a secret and see it fail.

## Notes
Decisions 2026-11-14: the live `config.json` lives in this repo under `server/`; the alpha gate is a server password only (host-only, set by C2, never committed); the in-game day is 2 real hours (12x); the name is as above. Start from `server/example/` and `docs/config/owner-config.md`. The host's profile copy is written by C2 only. Webhook URLs (`staffWebhookUrl`, S4) are host-only too. The `dayCycle` field exists once X1 has merged; do not start before. Restart times (open question 1 of the plan) stay unset: no `restarts` list until the maintainer answers.

Re-cut 2026-11-14 (widen, docs/roadmap/m4-slice-plan.md "Widening the graph"): the `dayCycle` field exists once CF has merged, so this slice waits for CF instead of X1.

# Slice C2: host config apply writes the profile's config.json and server.json from the repo plus the host's secrets
## Card
- type: tooling
- tier: T0
- model: sonnet
- machines: none
- budget: 40 min / 60 calls
- owns: `tools/host/config-apply.mjs`, `tools/host/commands/config.mjs`, `tools/validation/host-config-apply-selftest.mjs`
- depends: C1, R1
- acceptance: fixture tools/validation/host-config-apply-selftest.mjs
- risk: none
- release-blocker: no

## Capability
`tools\cli host config apply [<host dir>]` takes `server/flagship/config.json` and `server/flagship/server.json`, adds the values from a host-only `secrets.json` in the host directory (`game.password`, `game.passwordAdmin`, `staffWebhookUrl`), takes `game.admins` from `staff roster admins` (R1), and writes the profile's two files. It refuses while the host runs and prints what changed with every secret masked.

## Acceptance
Selftest on a fixture host dir: the written `server.json` has the secrets and the roster's admins and the written `config.json` has the webhook URL; the printed summary contains no secret; a secret key found in the repo file refuses the run; a missing `secrets.json` leaves the password empty and says so. Red first.

## Notes
Decision 2026-11-14: the alpha gate is the server password only, so rotating it is editing the host's `secrets.json` and running this command, then a warned restart. Never log a secret and never write one under the repo. One host subcommand per file in `tools/host/commands/` (K2).

# Slice W1: Wipe policy page for the alpha
## Card
- type: docs
- tier: T0
- model: sonnet
- machines: none
- budget: 30 min / 45 calls
- owns: `docs/community/wipe-policy.md`
- depends: none
- acceptance: proof validate (docs links) and the checklist in Acceptance
- risk: none
- release-blocker: no

## Capability
The wipe policy a player can read: during the alpha the server may be wiped at any time with 24 hours' notice on Discord; a wipe clears all progress; progress is kept from the public launch. For staff: how a wipe is announced and done.

## Acceptance
The page states the three rules above in one short paragraph each, names the commands (`host announce --wipe`, `host wipe --confirm`, the backup `host wipe` writes) exactly as `tools\cli host help` lists them, and says the 24 hour notice is enforced by the announce command. `validate` passes. Manual (#556): the maintainer reads it as a player and finds the answer to "will my progress be wiped" in one place.

## Notes
Decision 2026-11-14. This is the policy text #556 asked for. The commands land in O14 and O15; if either has not merged, name the command as planned in code font and do not link it. The rules page and the Discord copy link here.

Re-cut 2026-11-14 (widen, docs/roadmap/m4-slice-plan.md "Widening the graph"): no longer waits for O14 and O15; the card already names the planned commands in code font.

# Slice O17: Config-only hotfix path in the runbook
## Card
- type: docs
- tier: T0
- model: sonnet
- machines: none
- budget: 30 min / 45 calls
- owns: `docs/ops/hotfix.md`
- depends: none
- acceptance: proof validate (docs links) and the checklist in Acceptance
- risk: none
- release-blocker: no

## Capability
A page for a live config fix: edit `server/flagship/config.json` in a PR, merge, `host config apply` on the host, `config-check`, backup, warned restart, verify with `server-doctor`; and when a fix needs a script change instead (PR and pack release).

## Acceptance
It says the flagship's `config.json` lives in this repo under `server/flagship/` (decision 2026-11-14) and that secrets live only in the host's `secrets.json`; a command that has not merged yet is in code font as its card names it and marked *(planned)*; `validate` passes. Manual (#310): a price fix goes live in one warned restart.

## Notes
Link `docs/ops/session-runbook.md` (O16) rather than repeating it.

Re-cut 2026-11-14 (widen, docs/roadmap/m4-slice-plan.md "Widening the graph"): written from the plan now; it no longer waits for C2, O16 and O9. O16R reconciles the commands. Link `docs/ops/session-runbook.md` in code font if O16 has not merged.

