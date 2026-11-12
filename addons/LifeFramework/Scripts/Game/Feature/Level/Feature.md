# Level — Design Requirements

> Context file for progression. Written against
> `docs/design.md`.

## Intent

Progression is **earned in the world** — you level up by doing (gathering,
processing, jobs), not by clicking. A job level is the skill level: one XP
track per job, and config gates jobs, licences and actions on job plus level
(decision D12, 2026-11-12, #749). Levels are background state that other
systems read for gates; there is nothing to spend.

## Interaction pattern

Rung 4 background: `LF_JobLevels` holds total XP per character and job on the
server and derives each level from the job's thresholds in the `jobs` config
(`LF_JobsConfig.m_aTracks`). XP comes from doing the work: a delivery run, a
fruit picked, a gather, a quest turned in (`AddXp` / `Award`, server only).

Target: rung 3 for reading it — a trainer or the job board desk tells the
player where they stand. No screen.

## Current state

- `LF_JobLevels` (`Feature/Level/LF_JobLevels.c`): XP per (character record
  key, job), level from thresholds, per-action cooldown, per-job daily cap,
  the `joblevels` payload (save version 28) and the migration of a pre-#749
  save (single level onto CIVILIAN, delivery career onto DELIVERY).
- Readers: `LF_CivilianJob` (DELIVERY: fuel level 2, taxi level 3),
  `LF_LicenseGate` (the licence family's job, or a `licenceGates` row),
  `LF_ChainGate` (`m_sLevelJob`), `LF_QuestTrackerComponent` (`m_sJob`),
  `LF_Lockpick` (CIVILIAN).
- `LF_PlayerLevelComponent` (the single level, skill points, passive bonuses)
  is removed; its defects (unguarded setters, save-per-gain, Spanish literal)
  went with it.

## Iteration path

- **V2** — the trainer / job board desk reads a job level aloud (rung 3), and
  gated actions toast the job and level they need.
- **V3** — XP sources for MINER, LUMBERJACK, MEDIC and POLICE; per-level
  effects (Overthrow's skill effects) as config.

## Dependencies

- `Jobs` (XP source, config), `License` (level gates), `Gathering`/`Quests`
  (XP sources), `Chains` and `Crime` (level gates), `Persistence`.
