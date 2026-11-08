# M4 Flagship Live (outline)

M4 opens the 1989 Everon flagship to the public as a recurring server with its
own community. The maintainer set the shape on 2026-11-08. This file is an
outline. Each workstream is broken into sub-issues on GitHub, listed under
its section; they get full scope cards after M3 ships. Read it with
[direction](../direction.md), [decisions](../decisions.md), and the
[M3 outline](m3-outline.md).

**Launch gate.** M3 has shipped. The flagship runs the released `LifeFramework`
core and the `LF_Everon1989` pack, configured through its own `config.json`,
which is the same build and the same path an owner gets. Nothing flagship-only
is patched in.

**Done when.** No bar: M4 is the live phase, and it keeps iterating
(maintainer, 2026-11-08). Its first goal is the step to 24/7.

**How it runs.**

| Stage | Host | Cadence | Moves on when |
| --- | --- | --- | --- |
| Sessions | The maintainer's PC (`lf-testbox` as a fallback) | Ad hoc, announced on Discord and the site | Several sessions in a row reach the player target **and** donations cover a rented box for three months (`website/content/costs.json`) |
| 24/7 | A rented dedicated box | Always on, with scheduled restarts | — |

**Sizing.** The target is about 40 players. Plan the host, the police ratio,
and staff cover for that: roughly 6 to 8 police and 2 to 3 moderators online.

---

## 1. Hosting and ops

- Session runbook: start, announce, restart, shut down, from `tools\cli`
  commands, written so a second person can run a session.
- Save backups before every session and every restart, with a restore that
  has been tested (`wt e2e`-style proof against a copy of the live save).
- Crash auto-restart and a restart schedule. Players get an in-world warning
  before a planned restart.
- Log watching: `[ELDebug:*]` errors and `VME:` lines from the live server are
  summarised after each session and triaged into issues
  (`docs/debug-log-triage.md`).
- Hotfix path: a config-only fix goes out through `config.json` without a new
  Workshop build. A script fix goes through the normal PR and a pack release.
- Move to the rented box: a checklist (box spec for 40 players, ports, profile
  copy, save copy), done once the 24/7 trigger is met.

**Sub-issues** (epic #230, in order):

1. #305 Session runbook and session commands (flagship)
2. #306 Save backup before every session and restart, with a tested restore
3. #307 Crash auto-restart
4. #308 Scheduled restarts with in-world warnings
5. #309 Post-session log triage into issues
6. #310 Config-only hotfix path (flagship)
7. #311 Move to a rented box checklist (flagship)


## 2. Staff and community

- Server rules, written and linked from the town board and Discord.
- A moderator team with named people, and Game Master use from the M2
  moderation tools (incident log, logged admin actions).
- The police path: the application at the station desk (#128, M2) feeds the
  whitelist. Staff review applications on Discord.
- An appeals channel and a ban record.
- Session announcements and a feedback channel. Feedback is triaged into
  GitHub issues with the existing template.

**Sub-issues** (epic #231, in order):

1. #312 Server rules on the town board and Discord (flagship)
2. #313 Moderator team and moderator guide (flagship)
3. #314 Police applications reviewed by staff (flagship)
4. #315 Appeals channel and ban record (flagship)
5. #316 Session announcements and feedback triage (flagship)


## 3. Showcase and growth

- A launch trailer and session clips from the filming harness.
- A Workshop page for the flagship and a server-browser listing with a clear
  name and description.
- The site's servers page lists the flagship's status and next session.
- A "run your own" funnel: every flagship surface links back to the framework,
  its install guide, and the generator (M3).

**Sub-issues** (epic #232, in order):

1. #317 Launch trailer from the filming harness (flagship)
2. #318 Flagship Workshop page and server-browser listing (flagship)
3. #319 Servers page shows the flagship's status and next session (flagship)
4. #320 Run-your-own links from every flagship surface (flagship)


## Scopes Mapping A sends to M4

- Day and night cycle: accelerated by default, real-time option (#129). It is
  a server setting the flagship needs live.
- [Police missions](scopes/later/police-missions.md): these need a live police
  roster to be worth building.

## Not in M4

- Live economy tuning from telemetry. The maintainer did not pick it as a
  workstream; config-only tuning through `config.json` is still possible.
- Managed hosting for other communities, and an external database (M3
  non-goals).
- Cosmetic donations. The decision allows them on the flagship only, within
  Bohemia's policy; they are not planned in M4.
- Scheduled and organised events (parked).
- Flagship story content, bus stops, trash cans, housing (M5).

## Risks

- A PC host for 40 players: check CPU and upload on a full session before
  announcing a large one.
- Staff burnout is what kills life servers (benchmark). Recruit moderators
  before the first public session, not after.
- A save lost on the live server is a trust event. No session starts without a
  fresh backup.
