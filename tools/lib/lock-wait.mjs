// tools/lib/lock-wait.mjs - a lock wait that says what it waits on and stops at a budget.
//
// The rdb refresh used to call acquireLock(build, 15 min) once per addon, in the gate and again in
// every shard child, with nothing printed while it slept: a held build lock turned a gate into
// 25+ silent minutes (gate-shard-hang, 2026-11-14). waitForLock prints the holder when it starts
// waiting and every `sliceMs` after, and a Budget is spent once across every addon of one process:
// after one wait ran out the next caller fails at once instead of waiting again.
// validation/lock-wait-selftest.mjs proves it.

import { readFileSync } from "node:fs";

export class Budget {
  constructor(ms, now = Date.now) {
    this.now = now;
    this.total = ms;
    this.deadline = now() + ms;
  }
  remaining() {
    return Math.max(0, this.deadline - this.now());
  }
}

// Who holds a lock file, from its JSON ({ pid, at, what?, root? }).
export function describeHolder(lockFile, now = Date.now) {
  try {
    const info = JSON.parse(readFileSync(lockFile, "utf8"));
    const age = info.at ? Math.round((now() - Date.parse(info.at)) / 1000) : null;
    return `pid ${info.pid}${info.what ? ` (${info.what}${info.root ? ` in ${info.root}` : ""})` : ""}${age !== null ? `, held ${age} s` : ""}`;
  } catch {
    return "an unknown holder";
  }
}

const isHeld = (e) => /is held/.test(String(e?.message));

// acquire(waitMs) takes the lock or throws "... is held ..." after waitMs (wt.acquireLock). Returns
// what acquire returned; throws "<what> is held ... gave up after N s" (still matching /is held/, the
// cue for the pool fallback) once the budget is spent.
export function waitForLock({ acquire, holder = () => "an unknown holder", budget, what, sliceMs = 30000, log = console.log }) {
  try {
    return acquire(0);
  } catch (e) {
    if (!isHeld(e)) throw e;
  }
  const total = Math.round(budget.total / 1000);
  if (budget.remaining() <= 0) {
    throw new Error(`${what} is held by ${holder()}; the ${total} s wait budget is already spent, not waiting again`);
  }
  log(`waiting on ${what}: held by ${holder()}; budget ${Math.round(budget.remaining() / 1000)} s of ${total} s`);
  for (;;) {
    const left = budget.remaining();
    if (left <= 0) {
      throw new Error(`${what} is held by ${holder()}; gave up after the ${total} s wait budget`);
    }
    try {
      return acquire(Math.min(sliceMs, left));
    } catch (e) {
      if (!isHeld(e)) throw e;
    }
    if (budget.remaining() > 0) log(`still waiting on ${what} (${Math.round((budget.total - budget.remaining()) / 1000)} s of ${total} s): held by ${holder()}`);
  }
}
