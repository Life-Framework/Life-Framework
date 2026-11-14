// Proves the rdb refresh stops Workbench once its rdb is complete (tools/lib/wb-until.mjs) instead of
// waiting for an exit that never comes (supermarket-shop, 2026-11-14: the rdb was written ~2 s in,
// Workbench froze, and each refresh ran its full 600 s). Fake clock, then a real process that writes a
// file and never exits. About 4 s.
// red-proof: waitUntilDone ignoring done() (looping until alive() is false or the timeout) -> "returns
//   once done held for settleMs" fails (600 s, the old behaviour).
// inputs: tools/lib/wb-until.mjs, tools/validation/wb-until-selftest.mjs, tools/cli.mjs
import { spawn } from "node:child_process";
import { existsSync, mkdtempSync, rmSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { killTreeSync, pidAlive, waitUntilDone } from "../lib/wb-until.mjs";

const failures = [];
const check = (ok, msg) => {
  if (!ok) failures.push(msg);
};

const fake = () => {
  let t = 0;
  return { now: () => t, sleep: (ms) => { t += ms; }, at: () => t };
};

// The frozen Workbench: rdb complete at 2 s, the process never exits.
{
  const c = fake();
  const r = waitUntilDone({ done: () => c.at() >= 2000, alive: () => true, timeoutMs: 600000, settleMs: 3000, now: c.now, sleep: c.sleep });
  check(r.done && !r.exited && !r.timedOut && r.ms <= 6000, `returns once done held for settleMs (${JSON.stringify(r)})`);
}
// A done() that flips back (Workbench re-saving the file) restarts the settle time.
{
  const c = fake();
  const r = waitUntilDone({ done: () => c.at() >= 2000 && c.at() !== 4000, alive: () => true, timeoutMs: 600000, settleMs: 3000, now: c.now, sleep: c.sleep });
  check(r.done && r.ms >= 7000, `a flip back restarts the settle (${r.ms} ms)`);
}
// A throwing done() (a half-written file) counts as not yet.
{
  const c = fake();
  const r = waitUntilDone({ done: () => { if (c.at() < 3000) throw new Error("EBUSY"); return true; }, alive: () => true, timeoutMs: 600000, now: c.now, sleep: c.sleep });
  check(r.done && r.ms >= 6000, `a throw is "not yet" (${r.ms} ms)`);
}
// Workbench exits on its own: report it and whether the job got done.
{
  const c = fake();
  const r = waitUntilDone({ done: () => false, alive: () => c.at() < 5000, timeoutMs: 600000, now: c.now, sleep: c.sleep });
  check(r.exited && !r.done && !r.timedOut && r.ms === 5000, `an exit ends the wait (${JSON.stringify(r)})`);
}
// Never done, never exits: the timeout still holds.
{
  const c = fake();
  const r = waitUntilDone({ done: () => false, alive: () => true, timeoutMs: 600000, now: c.now, sleep: c.sleep });
  check(r.timedOut && !r.done && r.ms === 600000, `the timeout still holds (${JSON.stringify(r)})`);
}

// Real process: writes its "rdb" and then hangs, as the frozen Workbench did.
{
  const dir = mkdtempSync(join(tmpdir(), "lf-wbuntil-"));
  const rdb = join(dir, "resourceDatabase.rdb");
  try {
    const child = spawn(process.execPath, ["-e", `setTimeout(() => require("fs").writeFileSync(${JSON.stringify(rdb)}, "x"), 500); setInterval(() => {}, 1000)`], { stdio: "ignore", windowsHide: true });
    const t0 = Date.now();
    const r = waitUntilDone({ done: () => existsSync(rdb), alive: () => pidAlive(child.pid), timeoutMs: 20000, settleMs: 1000, pollMs: 200 });
    killTreeSync(child.pid);
    const ms = Date.now() - t0;
    check(r.done && !r.exited && ms < 6000, `real hang: returns within seconds of the file (${ms} ms, ${JSON.stringify(r)})`);
    await new Promise((res) => setTimeout(res, 300));
    check(!pidAlive(child.pid), "real hang: the process is stopped");
  } finally {
    rmSync(dir, { recursive: true, force: true });
  }
}

if (failures.length) {
  for (const f of failures) console.log(`FAIL  ${f}`);
  process.exit(1);
}
console.log("wb-until-selftest: OK");
