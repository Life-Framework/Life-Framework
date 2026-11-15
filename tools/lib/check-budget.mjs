// tools/lib/check-budget.mjs - a per-check time budget for the validate runner that does not blame a
// check for time its parent process was frozen.
//
// `validate --full` inside `ci` runs beside the Workbench gate in one node process. When something in
// that process blocks the event loop (a synchronous lock wait, a spawnSync), finished checks cannot be
// reaped, and their measured times include the freeze: on 2026-11-14 three selftests that take ~20 s
// each all reported 908.8 s. The watchdog ticks once a second; a tick that arrives late by more than
// `graceMs` is a freeze of this process, counted once and subtracted from every check that was running.
// A check whose own time passes its budget is reported through onTimeout (the runner kills it and fails
// loudly, naming it). validation/check-budget-selftest.mjs proves both.

// 600 s: on a PC busy with ~20 agents a healthy check has taken 203 s (release-cli-selftest, 2026-11-14).
export const DEFAULT_CHECK_BUDGET_S = 600;

// Budget in ms for one check: LF_VALIDATE_CHECK_BUDGET_S (seconds) or the default; `slowS` for the
// checks the caller knows are slow (the Workbench compile).
export function checkBudgetMs(env = process.env, slowS = null) {
  const v = Number(env.LF_VALIDATE_CHECK_BUDGET_S);
  const base = Number.isFinite(v) && v > 0 ? v : DEFAULT_CHECK_BUDGET_S;
  return Math.round((slowS !== null ? Math.max(base, slowS) : base) * 1000);
}

export function createWatchdog({ onTimeout, now = Date.now, tickMs = 1000, graceMs = 3000, timers = true } = {}) {
  const jobs = new Map(); // key -> { started, frozenAtStart, budgetMs }
  let last = now();
  let frozen = 0; // ms this process spent frozen since the watchdog started
  let longest = 0;
  const tick = () => {
    const t = now();
    const lag = t - last - tickMs;
    if (lag > graceMs) {
      frozen += lag;
      longest = Math.max(longest, lag);
    }
    last = t;
    for (const [key, j] of [...jobs]) {
      if (own(j, t) > j.budgetMs) {
        jobs.delete(key);
        onTimeout?.(key, own(j, t));
      }
    }
  };
  const own = (j, t = now()) => t - j.started - (frozen - j.frozenAtStart);
  const iv = timers ? setInterval(tick, tickMs) : null;
  iv?.unref?.();
  return {
    tick,
    // start and end tick first: after a freeze, the close events may run before the next timer does.
    start(key, budgetMs) {
      tick();
      jobs.set(key, { started: now(), frozenAtStart: frozen, budgetMs });
    },
    // Ends a job; returns { ownMs, frozenMs }: its time without this process's freezes, and the freeze
    // it sat through. null when it already timed out.
    end(key) {
      const j = jobs.get(key);
      jobs.delete(key); // before the tick, so a check that did finish is never timed out here
      tick();
      if (!j) return null;
      return { ownMs: Math.max(0, own(j)), frozenMs: frozen - j.frozenAtStart };
    },
    frozenMs: () => frozen,
    longestFreezeMs: () => longest,
    stop() {
      if (iv) clearInterval(iv);
    },
  };
}
