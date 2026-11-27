// Proves wt.acquireLock treats a lock name another process is deleting as contention, not an error
// (tools/wt.mjs tryTakeLock). On Windows an exclusive create of a name that a releasing holder or a
// stale-lock thief is deleting fails with EPERM (or EACCES/EBUSY) for a few milliseconds; the code
// rethrew it, so a build, test or session start under load died with "EPERM: operation not permitted,
// open '...server-2001.lock'" instead of queuing, and flaked every check that takes a machine-wide lock
// (wb-lock-selftest under load). Four processes churn one lock (1500 tries each) on a scratch hub.
// inputs: tools/wt.mjs, tools/validation/wt-lock-contention-selftest.mjs
// red-proof: 2026-11-15, with tryTakeLock rethrowing every non-EEXIST error (HEAD's wt.mjs) this selftest failed
// 'acquireLock threw under contention (not "held"): {"EPERM":5}'; a longer 5 s probe of the same loop printed
// {"held":3864,"EPERM":5423} with 877 acquired, and {"held":2642} with 885 acquired after the fix. Restored.

import { spawn } from "node:child_process";
import { gitSync } from "../lib/git-retry.mjs";
import { readdirSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { pathToFileURL } from "node:url";
import { safeRemoveTree } from "../link-safety.mjs";
import { scratchDir } from "../lib/scratch.mjs";

const ATTEMPTS = 1500;
const WORKERS = 4;
const wtUrl = pathToFileURL(join(import.meta.dirname, "..", "wt.mjs")).href;

if (process.argv[2] === "worker") {
  const wt = await import(wtUrl);
  const root = process.argv[3];
  const errors = {};
  let got = 0;
  // Work-based, not a fixed 3 s window: on a loaded box (validate beside ~20 agents) a worker was scheduled for
  // a few hundred ms of its 3 s and took the lock only 4 or 7 times (main guard 2026-11-22/23, 4 flakes). Each
  // worker makes ATTEMPTS tries; the 60 s cap only ends a lock that is truly stuck.
  // A start barrier keeps the churn overlapping: a worker that imported wt.mjs late would otherwise run alone.
  writeFileSync(join(root, `ready-${process.pid}`), "");
  const barrierEnd = Date.now() + 30000;
  while (readdirSync(root).filter((n) => n.startsWith("ready-")).length < WORKERS && Date.now() < barrierEnd) await new Promise((r) => setTimeout(r, 20));
  const end = Date.now() + 60000;
  for (let i = 0; i < ATTEMPTS && Date.now() < end; i++) {
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
const root = scratchDir("lf-lockcontention-");
try {
  gitSync(["init", "-q", root], { windowsHide: true });
  const outs = await Promise.all(Array.from({ length: WORKERS },() => new Promise((res) => {
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
