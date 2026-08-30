# Level — Design Requirements

> Context file for progression. Written against
> `docs/design-philosophy.md`.

## Intent

Progression is **earned in the world** — you level up by doing (gathering,
processing, jobs), not by clicking. Skill points are spent at a trainer, not in
a screen you can open anywhere. The level itself is background state that other
systems read for perks and gates.

## Interaction pattern

Rung 4 background today: `EL_PlayerLevelComponent` holds level/XP/skill-point
state, computes `GetExperienceForNextLevel = level * 100`, applies passive
bonuses (gathering speed/amount, sale bonus), and cascades level-ups.

Target: rung 3 for the SP spend — a trainer at a gym/agency/office where you
convert skill points into perks. XP gain stays automatic from doing the work.

## V1 (shippable)

1. **Fix the XP loop**: `Jobs.AddExperience` is a no-op, so level-ups never
   happen. Level is dead weight until XP flows.
2. Server-guard the setters (`SetLevel`/`SetExperience`/`SetSkillPoints`/
   `SetTotalSkillPointsEarned` have no guard, no validation, no recompute — a
   corrupted or hostile write leaves inconsistent state).
3. Stop saving on every XP gain (DB-write hazard) — batch the persistence.
4. Localize `GetBonusesText` (Spanish literal).

## Iteration path

- **V2** — trainer interaction (rung 3) for skill-point spend.
- **V3** — perk tree per discipline, prestige/soft-cap.

## Current state

- `EL_PlayerLevelComponent` (`Feature/Level/`) — level/XP/SP + passive bonuses;
  ⚠️ unguarded setters, save-per-gain, Spanish literal.

## Dependencies

- `Jobs` (XP source), `License` (level gates), `Gathering`/`Processing`
  (bonuses applied), `Money` (trainer fees).