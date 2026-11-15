// Proves the validate runner's per-check budget (tools/lib/check-budget.mjs): a check over its budget
// is reported by name, and a freeze of the runner's own process is not blamed on the checks that were
// running (2026-11-14: three ~20 s selftests all reported 908.8 s while `ci` sat in a synchronous
// 15-minute lock wait beside them). Fake clock, then real timers with a real 4 s freeze and a real
// hanging child that is killed. About 6 s.
// red-proof: own() without `- (frozen - j.frozenAtStart)` -> "a freeze times out no check" and "a freeze is
//   not the checks' time" fail (913.8 s, the 2026-11-14 numbers).
// inputs: tools/lib/check-budget.mjs, tools/lib/wb-until.mjs, tools/validation/check-budget-selftest.mjs, tools/cli.mjs
import { spawn } from "node:child_process";
import { checkBudgetMs, createWatchdog, DEFAULT_CHECK_BUDGET_S } from "../lib/check-budget.mjs";
import { killTreeSync, pidAlive } from "../lib/wb-until.mjs";

const failures = [];
const check = (ok, msg) => {
  if (!ok) failures.push(msg);
};

// --- budget from the environment
check(checkBudgetMs({}) === DEFAULT_CHECK_BUDGET_S * 1000, "default budget");
check(checkBudgetMs({ LF_VALIDATE_CHECK_BUDGET_S: "42" }) === 42000, "LF_VALIDATE_CHECK_BUDGET_S sets it");
check(checkBudgetMs({ LF_VALIDATE_CHECK_BUDGET_S: "junk" }) === DEFAULT_CHECK_BUDGET_S * 1000, "a bad value falls back to the default");
check(checkBudgetMs({}, 1200) === 1200000, "a slow check keeps its own longer budget");

// --- fake clock: the 2026-11-14 freeze. Three checks start, the process freezes 900 s, they end.
{
  let t = 0;
  const timeouts = [];
  const dog = createWatchdog({ now: () => t, timers: false, onTimeout: (k) => timeouts.push(k) });
  for (const k of ["promote-selftest.mjs", "quorum-selftest.mjs", "wt-fixes-selftest.mjs"]) dog.start(k, 300000);
  for (let i = 0; i < 5; i++) {
    t += 1000;
    dog.tick();
  }
  t += 900000; // a synchronous lock wait: no tick for 900 s
  t += 8800;
  const r = ["promote-selftest.mjs", "quorum-selftest.mjs", "wt-fixes-selftest.mjs"].map((k) => dog.end(k));
  check(timeouts.length === 0, `a freeze times out no check (got ${timeouts.join(", ")})`);
  check(r.every((x) => x && x.ownMs < 20000), `a freeze is not the checks' time (${r.map((x) => x && x.ownMs).join(", ")} ms)`);
  check(r.every((x) => x && x.frozenMs >= 900000), "each check reports the freeze it sat through");
  check(dog.frozenMs() >= 900000 && dog.longestFreezeMs() >= 900000, "the runner reports the freeze once");
}

// --- fake clock: a check that really hangs is named once its own time passes the budget
{
  let t = 0;
  const timeouts = [];
  const dog = createWatchdog({ now: () => t, timers: false, onTimeout: (k, ms) => timeouts.push([k, ms]) });
  dog.start("hangs-selftest.mjs", 300000);
  dog.start("quick-selftest.mjs", 300000);
  t += 1000;
  dog.tick();
  check(dog.end("quick-selftest.mjs")?.ownMs === 1000, "a quick check ends with its own time");
  for (let i = 0; i < 300; i++) {
    t += 1000;
    dog.tick();
  }
  check(timeouts.length === 1 && timeouts[0][0] === "hangs-selftest.mjs" && timeouts[0][1] > 300000, `the hanging check is named once (${JSON.stringify(timeouts)})`);
  check(dog.end("hangs-selftest.mjs") === null, "a timed-out check ends as timed out");
}

// --- fake clock: a check that finishes over budget between ticks is not timed out by its own end
{
  let t = 0;
  const timeouts = [];
  const dog = createWatchdog({ now: () => t, timers: false, onTimeout: (k) => timeouts.push(k) });
  dog.start("slow.mjs", 2000);
  t += 2500;
  const r = dog.end("slow.mjs");
  check(timeouts.length === 0 && r && r.ownMs === 2500, "end() never times out the check it ends");
}

// --- real timers: a 4 s synchronous freeze beside a running check
{
  const dog = createWatchdog({ graceMs: 1500, onTimeout: () => {} });
  dog.start("beside-a-freeze", 60000);
  await new Promise((r) => setTimeout(r, 1200)); // one real tick first
  Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, 4000);
  const r = dog.end("beside-a-freeze");
  dog.stop();
  check(r && r.frozenMs >= 2500 && r.ownMs < 3000, `real freeze: own ${r?.ownMs} ms, frozen ${r?.frozenMs} ms`);
}

// --- real timers: a hanging child is killed by name within its budget
{
  const child = spawn(process.execPath, ["-e", "setInterval(() => {}, 1000)"], { stdio: "ignore", windowsHide: true });
  const named = [];
  const dog = createWatchdog({
    tickMs: 200,
    onTimeout: (k) => {
      named.push(k);
      killTreeSync(child.pid);
    },
  });
  const t0 = Date.now();
  dog.start("hang.mjs", 1000);
  await new Promise((r) => child.on("exit", r));
  dog.stop();
  const ms = Date.now() - t0;
  check(named.length === 1 && named[0] === "hang.mjs", `the hanging child is named (${named.join(", ")})`);
  check(ms < 30000 && !pidAlive(child.pid), `and killed within seconds (${ms} ms)`);
}

if (failures.length) {
  for (const f of failures) console.log(`FAIL  ${f}`);
  process.exit(1);
}
console.log("check-budget-selftest: OK");
