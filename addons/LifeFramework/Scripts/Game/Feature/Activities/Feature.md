# Activities: what to do next

## Intent

After the arrival errands a player should not wonder what to do. The personal file (F2) gets a third
tab, **Next**, listing ways to earn a living that the player can do now, each with a one-line pitch,
rough pay an hour and how far away it is. **Start** begins a short guided starter through the quest
system: every step puts a waypoint on the screen and a mark on the player's own map, and shows a
one-sentence note. Maintainer ask, 2026-11-22.

## Interaction pattern

Rung 1 screen (the F2 sheet, UI kit parts only). Rows: name; "about $470 an hour · 350 m away" (or
"· started", or "Done"). Picking a row prints its pitch, its step count or the running step, and one
button: **Start** (available), **Guide me** (running: the waypoint goes back to its step), nothing
(done). Locked activities (level, licence, faction) are not listed.

## Data

- `Configs/Activities/LF_Activities.conf` (`LF_ActivityConfig`) ships no rows; a pack overlays
  activities by id (`m_sOp`; a patch gives the activity whole). The Everon pack's
  `LF_Activities_Everon1989.conf` holds mining, parcel, lumber, fishing, apples, police, medic.
- An activity: name and pitch keys, pay an hour (from `tools\cli economy`), a place for the distance,
  a job (level gate and XP), an optional licence or faction gate, and 2-7 steps.
- A step: title and note keys, a check (`JOB`, `ITEM`, `VEHICLE`, `ARRIVE`, `DUTY`, `XP`, or `NONE` with
  its own quest objectives), a target (`garage`, `job` + job id for the nearest workplace of that job,
  `giver`, or an `LF_Places` kind + id or nearest). No coordinates in core.

## Server path

- Each step is an `LF_QuestDefinition` (`LF_ActivityTable.BuildQuest`), registered at world start:
  quest id `activity_<activity>_<step>`, auto turn-in, steps 2+ auto-accept after the step before. A
  checked step waits on the SIGNAL `activity.step:<questId>`, reported by `LF_ActivityGuide` when the
  check holds (read every 2 s for followed players, and at once when the step starts).
- `RpcAsk_LF_Activities` (no argument) and `RpcAsk_LF_ActivityGo(id)` on the player's own controller,
  QUEST rate-limit bucket, living body only; the server re-checks everything (`LF_ActivityRules`).
- Persistence: none of its own. Turn-ins are saved by the quest system, so a restart mid-activity
  resumes at the next step (`SettleChain`); step one, if it was running, is pressed again.
- The waypoint id is `activity` and the map share key `activity:<uid>`, apart from the arrival guide's.
  A running activity step takes the waypoint after a turn-in or spawn (`RefreshGuide`).

## Tests

`activities/next-activities` (LOGIC: filtering, states, start once, chain order, checks, wire),
`activities/next-activities-world` (WORLD: a character runs a 3-step activity through the quest
system), pack `everon/activities` (the Everon table loads, its prefabs and targets resolve). Harness:
`playermenu open next`, `activities state|pick <n>|go`; proof `next-activities-proof`.

## Adding an activity later (the postal run)

A pack adds an activity with one overlay row (`m_sOp "add"`, a new `m_sId`), or replaces one (`m_sOp "patch"` on
`parcel` with new steps, or `"remove"` then `"add"`). The postal truck job slots in that way when it ships: its row
names its own place kind, pay, and steps (for example ARRIVE at its depot, then a `job.run_complete:<job>` SIGNAL
objective). No core change is needed.
