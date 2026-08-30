# Jobs — Design Requirements

> Context file for employment. Written against
> `docs/design-philosophy.md` (in-world first).

## Intent

Jobs are a **place you go to**. You apply at the workplace, you punch in when
you arrive, you do the work in the world (gather, process, deliver), and the
paycheck lands. The job is anchored to a location and an activity, not a
dropdown.

## Interaction pattern

Rung 4 today: `EL_PlayerJobComponent` holds per-player job state, paycheck
clock, per-job XP, license gates; `RpcAsk_SetJob` re-validates licenses
server-side.

Target: rung 3 → 1. A job office / workplace where you apply (rung 3), then the
work itself is physical — the job is *done* by doing the physical activity, and
that activity is what pays (gathering/processing already feed this funnel).

## V1 (shippable)

1. **Close the security holes**: `RpcAsk_ClaimFruitCatcherReward` mints
   unbounded fruit + XP (only `score <= 0` checked), and
   `RpcAsk_DebugGrantLicense` is a live backdoor granting any license. Remove
   both.
2. Make `AddExperience` actually add — it is a no-op today, so `OnLevelUp` can
   never fire.
3. Fix the dead whitelist branch in `SetJob` (only POLICE is restricted and it
   is special-cased) — route job gating through `Whitelist` like every other
   restricted job.
4. Localize the hard-coded Spanish strings.
5. Persist per-job level/XP properly (convention-only today, no SaveData pair).

## Iteration path

- **V2** — job office / workplace sign-up (rung 3); punch-in at work.
- **V3** — job-specific work actions that pay on completion (delivery, taxi,
  medic callouts).
- **V4** — job hierarchy / promotions, business ownership.

## Current state

- `EL_PlayerJobComponent` — job state, paycheck clock, license gates.
  ❌ mint + backdoor + dead whitelist + no-op XP + Spanish strings.
- `EL_JobManager` — singleton reward funnel; `GetGatherReward`/
  `GetProcessReward` return 0 today (dead-but-safe). ⚠️ unreachable
  `GiveReward`, null `EL_ATMManager.GetInstance()`.
- `EL_JobConfig` — job definitions.

## Dependencies

- `Level` (XP/level perks), `License` (job access), `Money`/`Banking`
  (paychecks), `Gathering`/`Processing` (job activities), `Whitelist`
  (restricted jobs).