// Proves wt.acquireLock treats a lock name another process is deleting as contention, not an error
// (tools/wt.mjs tryTakeLock). On Windows an exclusive create of a name that a releasing holder or a
// stale-lock thief is deleting fails with EPERM (or EACCES/EBUSY) for a few milliseconds; the code
// rethrew it, so a build, test or session start under load died with "EPERM: operation not permitted,
// open '...server-2001.lock'" instead of queuing, and flaked every check that takes a machine-wide lock
// (wb-lock-selftest under load). Four processes churn one lock for 3 s on a scratch hub.
// inputs: tools/wt.mjs, tools/validation/wt-lock-contention-selftest.mjs
// red-proof: 2026-11-15, with tryTakeLock rethrowing every non-EEXIST error (HEAD's wt.mjs) this selftest failed
// 'acquireLock threw under contention (not "held"): {"EPERM":5}'; a longer 5 s probe of the same loop printed
// {"held":3864,"EPERM":5423} with 877 acquired, and {"held":2642} with 885 acquired after the fix. Restored.

import { spawn, spawnSync } from "node:child_process";
import { mkdtempSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { pathToFileURL } from "node:url";
import { safeRemoveTree } from "../link-safety.mjs";

const wtUrl = pathToFileURL(join(import.meta.dirname, "..", "wt.mjs")).href;

if (process.argv[2] === "worker") {
  const wt = await import(wtUrl);
  const root = process.argv[3];
  const errors = {};
  let got = 0;
  const end = Date.now() + 3000;
  while (Date.now() < end) {
    try {
      wt.acquireLock(root, "probe", { waitMs: 0 });
      got++;
      wt.releaseLock(root, "probe");
    } catch (e) {
      const k = /is held/.test(e.message) ? "held" : e.code || e.message.slice(0, 60);
      errors[k] = (errors[k] || 0) + 1;
    }
  }
  console.log(JSON.stringify({ got, errors }));
  process.exit(0);
}

const failures = [];
const root = mkdtempSync(join(tmpdir(), "lf-lockcontention-"));
try {
  spawnSync("git", ["init", "-q", root], { windowsHide: true });
  const outs = await Promise.all(Array.from({ length: 4 }, () => new Promise((res) => {
    const c = spawn(process.execPath, [import.meta.filename, "worker", root], { stdio: ["ignore", "pipe", "inherit"], windowsHide: true });
    let o = "";
    c.stdout.on("data", (d) => (o += d));
    c.on("close", () => res(o.trim()));
  })));
  let got = 0;
  const other = {};
  for (const o of outs) {
    let j;
    try {
      j = JSON.parse(o);
    } catch {
      failures.push(`a worker printed no result: ${o.slice(0, 200)}`);
      continue;
    }
    got += j.got;
    for (const [k, v] of Object.entries(j.errors)) if (k !== "held") other[k] = (other[k] || 0) + v;
  }
  if (got < 10) failures.push(`the workers took the lock only ${got} times`);
  if (Object.keys(other).length) failures.push(`acquireLock threw under contention (not "held"): ${JSON.stringify(other)}`);
} finally {
  safeRemoveTree(root);
}
if (failures.length) {
  for (const f of failures) console.log(`FAIL  ${f}`);
  process.exit(1);
}
console.log("wt-lock-contention-selftest: OK (4 processes churning one lock: no EPERM thrown, only 'held')");
