// Proves a refresh Workbench that starts but never gets anywhere fails fast with its reason instead of
// holding the build lock for 600 s (lf-cloud, 2026-11-15: Steam signed out after a reboot, every refresh
// froze after writing its rdb, each addon sat out 600 s, and every gate and session on the box queued
// behind it). Fake clock for the wait, a real log folder for the freeze dump. About 1 s.
// red-proof: waitUntilDone ignoring stalled() (the pre-2026-11-15 loop) -> "a freeze dump ends the wait
//   within ~65 s" fails (600000 ms, timedOut); findFreezeDump matching every file -> "an old dump from a
//   previous run is not this run's" fails.
// inputs: tools/lib/wb-until.mjs, tools/lib/wb-launch-health.mjs, tools/validation/wb-launch-health-selftest.mjs, tools/cli.mjs
import { mkdirSync, mkdtempSync, rmSync, utimesSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { waitUntilDone } from "../lib/wb-until.mjs";
import { findFreezeDump, selftest, stallReason } from "../lib/wb-launch-health.mjs";

const failures = selftest();
const check = (ok, msg) => {
  if (!ok) failures.push(msg);
};

const fake = () => {
  let t = 0;
  return { now: () => t, sleep: (ms) => { t += ms; }, at: () => t };
};

// Workbench alive, rdb never complete, the engine's freeze dump lands at 64 s: the wait ends there.
{
  const c = fake();
  const dumpAt = 64000;
  const r = waitUntilDone({
    done: () => false,
    alive: () => true,
    stalled: () => stallReason({ freezeDump: c.at() >= dumpAt ? "FREEZE.mdmp" : null, steam: false, host: "lf-cloud" }),
    timeoutMs: 600000,
    now: c.now,
    sleep: c.sleep,
  });
  check(!r.timedOut && !r.done && !r.exited && r.ms <= 66000 && /Steam is not signed in on lf-cloud/.test(r.stalled ?? ""), `a freeze dump ends the wait within ~65 s with the reason (${JSON.stringify(r)})`);
}
// The rdb is complete and Workbench then freezes: that is success, not a stall (the PC's normal case).
{
  const c = fake();
  const r = waitUntilDone({
    done: () => c.at() >= 3000,
    alive: () => true,
    stalled: () => (c.at() >= 64000 ? "froze" : ""),
    timeoutMs: 600000,
    now: c.now,
    sleep: c.sleep,
  });
  check(r.done && !r.stalled && r.ms < 10000, `a complete rdb wins over a later freeze (${JSON.stringify(r)})`);
}
// A stalled() that throws is "not stalled" (a folder being written), the wait carries on.
{
  const c = fake();
  const r = waitUntilDone({
    done: () => c.at() >= 5000,
    alive: () => true,
    stalled: () => { throw new Error("EBUSY"); },
    timeoutMs: 600000,
    now: c.now,
    sleep: c.sleep,
  });
  check(r.done && !r.stalled, `a throwing stalled() does not end the wait (${JSON.stringify(r)})`);
}
// A Workbench that dies at once ends the wait as an exit.
{
  const c = fake();
  const r = waitUntilDone({ done: () => false, alive: () => c.at() < 2000, stalled: () => "", timeoutMs: 600000, now: c.now, sleep: c.sleep });
  check(r.exited && !r.done && r.ms === 2000, `a dead Workbench ends the wait (${JSON.stringify(r)})`);
}

// Real folders: the profile layout Workbench writes (<profile>/logs/<run>/FREEZE_*.mdmp).
{
  const profile = mkdtempSync(join(tmpdir(), "lf-wbhealth-"));
  try {
    check(findFreezeDump(profile) === null, "no logs folder: no dump");
    const oldRun = join(profile, "logs", "logs_2026-11-15_03-06-07");
    const newRun = join(profile, "logs", "logs_2026-11-15_03-16-10");
    mkdirSync(oldRun, { recursive: true });
    mkdirSync(newRun, { recursive: true });
    writeFileSync(join(oldRun, "FREEZE_Workbench_old.mdmp"), "x");
    const past = new Date(Date.now() - 3600 * 1000);
    utimesSync(join(oldRun, "FREEZE_Workbench_old.mdmp"), past, past);
    writeFileSync(join(newRun, "console.log"), "");
    check(findFreezeDump(profile, Date.now() - 60000) === null, "an old dump from a previous run is not this run's");
    writeFileSync(join(newRun, "FREEZE_Workbench_new.mdmp"), "x");
    const hit = findFreezeDump(profile, Date.now() - 60000);
    check(!!hit && hit.endsWith("FREEZE_Workbench_new.mdmp"), `this run's dump is found (${hit})`);
  } finally {
    rmSync(profile, { recursive: true, force: true });
  }
}

if (failures.length) {
  for (const f of failures) console.log(`FAIL  ${f}`);
  process.exit(1);
}
console.log("wb-launch-health-selftest: OK");
