# Range - Design Requirements

> Context file for the practice range (#1929 targets, #3103 lane, #3184 timed round). What each piece does: `docs/features/toys.md`.

## Intent

The range keeps score. A player starts a round on a lane, the targets stand up, and the toast says how many went down
and how fast, next to their best. Nothing pays and nothing is saved: a prize or a board line is a follow-up card.

## Interaction pattern

Rung 1. One action, "Start a round", on any target of the lane (`LF_RangeStartAction`).

## Pieces

- `LF_RangeLaneComponent`: the lane id string on a target. A lane is every target with the same id; the targets
  register by `EntityID`. No coordinates, no names.
- `LF_RangeRound` (pure): starter, target count, distinct targets down, the 60 s limit. Counts only the starter's hits.
- `LF_RangeBests` (pure): best clear time per player record key, for the session.
- `LF_RangeRun`: the server side. `StartAs` raises the lane and holds its targets down when hit
  (`SetAutoResetTarget(false)`), `OnHit` counts, a 250 ms callqueue tick ends a round at the limit, `Finish` hands the
  targets back to their own reset and toasts the result. One round per lane; a second start is refused.
- A modded `SCR_FiringRangeTarget.OnDamage` tells `LF_RangeRun.OnHit` who hit (the instigator's record key).

## Rules

- A timed-out round never sets a best; only a full clear does, and only a strictly faster one replaces the old best.
- The server decides every step; a client only asks for the start.
- Logging is `[LFDebug:Range]`.
