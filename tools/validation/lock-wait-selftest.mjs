// Proves a held build lock can no longer hang a gate (gate-shard-hang, 2026-11-14): the rdb refresh
// waits through tools/lib/lock-wait.mjs (prints the holder, stops at one budget shared by every addon),
// and shard children never wait on the lock a second time. Uses a fake clock, then a real lock file
// in a throwaway git repo. Does not boot the game.
// red-proof: waitForLock calling acquire(left) instead of acquire(Math.min(sliceMs, left)) -> "progress is
//   printed every slice" fails (one silent wait for the whole budget, the 25-minute gate of 2026-11-14).
// inputs: tools/lib/lock-wait.mjs, tools/validation/lock-wait-selftest.mjs, tools/cli.mjs, tools/wt.mjs
import { execFileSync } from "node:child_process";
import { mkdtempSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { fileURLToPath } from "node:url";
import { Budget, describeHolder, waitForLock } from "../lib/lock-wait.mjs";
import * as wt from "../wt.mjs";

const failures = [];
const check = (ok, msg) => {
  if (!ok) failures.push(msg);
};
const held = () => {
  throw new Error("lock 'build' is held (x); retry with --wait to queue");
};

// --- fake clock: a lock that never frees
{
  let t = 0;
  const now = () => t;
  const budget = new Budget(90000, now);
  const logs = [];
  const calls = [];
  let err = null;
  try {
    waitForLock({
      what: "the build lock",
      budget,
      sliceMs: 30000,
      holder: () => "pid 7 (rdb refresh in ws-x)",
      log: (l) => logs.push(l),
      acquire: (ms) => {
        calls.push(ms);
        t += ms;
        held();
      },
    });
  } catch (e) {
    err = e;
  }
  check(err && /is held/.test(err.message) && /gave up after the 90 s/.test(err.message), `a lock held past the budget throws a budgeted 'is held' error (got ${err?.message})`);
  check(t === 90000, `the wait stops at the budget (waited ${t} ms)`);
  check(/^waiting on the build lock: held by pid 7 \(rdb refresh in ws-x\); budget 90 s/.test(logs[0] || ""), `the first line names the lock, the holder and the budget (got ${logs[0]})`);
  check(logs.filter((l) => /^still waiting on the build lock \(\d+ s of 90 s\): held by pid 7/.test(l)).length === 2, `progress is printed every slice (${logs.length} line(s))`);
  check(calls[0] === 0 && calls.slice(1).every((ms) => ms > 0 && ms <= 30000), `one fast try, then slices of at most 30 s (${calls.join(",")})`);

  // The next addon in the same process: no second wait.
  const before = t;
  const calls2 = [];
  let err2 = null;
  try {
    waitForLock({ what: "the build lock [LF_Everon1989]", budget, log: () => {}, acquire: (ms) => {
      calls2.push(ms);
      t += ms;
      held();
    } });
  } catch (e) {
    err2 = e;
  }
  check(err2 && /already spent/.test(err2.message) && /is held/.test(err2.message), `a spent budget fails the next addon at once (got ${err2?.message})`);
  check(t === before && calls2.length === 1 && calls2[0] === 0, `a spent budget fails the next addon at once (waited ${t - before} ms, ${calls2.length} try)`);
}

// --- the lock frees during the wait
{
  let t = 0;
  const budget = new Budget(600000, () => t);
  let n = 0;
  let got = null;
  try {
    got = waitForLock({ what: "the build lock", budget, log: () => {}, acquire: (ms) => {
      t += ms;
      if (++n < 3) held();
      return { name: "build" };
    } });
  } catch {}
  check(got?.name === "build" && t === 60000, `a lock that frees is taken (after ${t} ms)`);
  check(budget.remaining() === 540000, "a wait that got the lock leaves the rest of the budget");
}

// --- other errors are not swallowed
{
  let err = null;
  try {
    waitForLock({ what: "x", budget: new Budget(1000), log: () => {}, acquire: () => {
      throw new Error("EACCES");
    } });
  } catch (e) {
    err = e;
  }
  check(err?.message === "EACCES", "an error other than 'is held' propagates");
}

// --- a real lock file held by a live process, in a throwaway repo
{
  const dir = mkdtempSync(join(tmpdir(), "lf-lockwait-"));
  try {
    execFileSync("git", ["init", "-q", dir], { windowsHide: true });
    const lockFile = join(wt.ensureHub(dir), "locks", "build.lock");
    writeFileSync(lockFile, JSON.stringify({ pid: process.pid, at: new Date().toISOString(), name: "build", what: "build", root: "ws-other" }));
    const logs = [];
    const t0 = Date.now();
    let err = null;
    try {
      waitForLock({ what: "the build lock", budget: new Budget(1200), sliceMs: 400, holder: () => describeHolder(lockFile), log: (l) => logs.push(l), acquire: (ms) => wt.acquireLock(dir, "build", { waitMs: ms, pollMs: 100 }) });
    } catch (e) {
      err = e;
    }
    const ms = Date.now() - t0;
    check(err && /is held/.test(err.message), `a real held lock gives up with 'is held' (got ${err?.message})`);
    check(ms < 4000, `a real held lock gives up near its 1.2 s budget (took ${ms} ms)`);
    check(new RegExp(`held by pid ${process.pid} \\(build in ws-other\\), held \\d+ s`).test(logs[0] || ""), `the holder is read from the lock file (got ${logs[0]})`);
  } finally {
    rmSync(dir, { recursive: true, force: true });
  }
}

// --- the wiring in cli.mjs: the refresh waits through waitForLock, shard children never refresh again
{
  const cli = readFileSync(join(fileURLToPath(new URL(".", import.meta.url)), "..", "cli.mjs"), "utf8");
  const ensure = cli.slice(cli.indexOf("function ensureAddonResourceDatabase"), cli.indexOf("// One headless Workbench run of a ResourceManager plugin"));
  check(/waitForLock\(/.test(ensure) && !/acquireLock\(root, "build", \{ waitMs: \d/.test(ensure), "the rdb refresh takes the build lock through waitForLock, not a silent fixed wait");
  check(/LF_RDB_CHECKED === "1"/.test(ensure), "the rdb refresh honours LF_RDB_CHECKED");
  const child = cli.slice(cli.indexOf("function runShardChild"), cli.indexOf("async function runShardedTests"));
  check(/LF_RDB_CHECKED: "1"/.test(child), "shard children run with LF_RDB_CHECKED=1");
  check(/LF_SHARD_TIMEOUT_S/.test(child) && /still running after/.test(child), "a shard child has a heartbeat and a timeout");
}

if (failures.length) {
  for (const f of failures) console.log(`FAIL  lock-wait-selftest: ${f}`);
  process.exit(1);
}
console.log("lock-wait-selftest: OK (budgeted, loud build-lock wait; one budget for every addon; shard children skip the refresh; shard heartbeat and timeout)");
