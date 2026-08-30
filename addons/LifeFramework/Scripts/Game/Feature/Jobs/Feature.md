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

1. **Security holes — DONE (verified 2026-08-30).** `RpcAsk_ClaimFruitCatcherReward`
   is server-clamped to `EL_FRUIT_CATCHER_MAX_SCORE` (pin test
   `security/fruitcatcher-reward-clamp`); `RpcAsk_DebugGrantLicense` is removed.
2. **Paychecks pay cash** (verified 2026-08-30): `ProcessPaycheck` now uses
   `EL_MoneyUtils.GiveCash` — the old `EL_BankAccountComponent` deposit branch
   is gone, and that entity bank is deleted. The only cash↔account boundary is
   the ATM (`Feature/ATM`). Same for `EL_JobManager.GiveReward`.
3. Make `AddExperience` actually add — it is a no-op today, so `OnLevelUp` can
   never fire — pending.
4. Fix the dead whitelist branch in `SetJob` (only POLICE is restricted and it
   is special-cased) — route job gating through `Whitelist` like every other
   restricted job — pending.
5. Localize the hard-coded Spanish strings — pending.
6. Persist per-job level/XP properly (convention-only today, no SaveData pair) —
   pending.

## Iteration path

- **V2** — job office / workplace sign-up (rung 3); punch-in at work.
- **V3** — job-specific work actions that pay on completion (delivery, taxi,
  medic callouts).
- **V4** — job hierarchy / promotions, business ownership.

## Current state

- `EL_PlayerJobComponent` — job state, paycheck clock (cash), license gates.
  ✅ mint closed, backdoor removed, paycheck pays cash; ⚠️ dead whitelist
  branch + no-op XP + Spanish strings + per-job XP not persisted.
- `EL_JobManager` — singleton reward funnel; `GetGatherReward`/
  `GetProcessReward` return 0 today (dead-but-safe); `GiveReward` pays cash.
- `EL_JobConfig` — job definitions.

## Dependencies

- `Level` (XP/level perks), `License` (job access), `Money` (cash paychecks),
  `Gathering`/`Processing` (job activities), `Whitelist` (restricted jobs).