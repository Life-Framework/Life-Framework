// Proves a wait on a held lock no longer freezes the process it runs in (harness-hangs, 2026-11-14):
// `ci` ran the build's 15-minute wait on the `build` lock (wt.acquireLock, Atomics.wait) beside
// `validate --full` in one process, and every finished check sat unreaped until it ended (908.8 s each).
// wt.acquireLockAsync must keep timers running while it waits, take the lock once it frees, and still
// fail fast with waitMs 0. Real lock file in a throwaway git repo; about 2 s.
// red-proof: buildAddon/this test calling wt.acquireLock instead of wt.acquireLockAsync -> "the event
//   loop ticks while waiting" fails (0 ticks) and the lock is never freed (its release timer cannot run).
// inputs: tools/wt.mjs, tools/validation/lock-async-selftest.mjs
import { execFileSync } from "node:child_process";
import { existsSync, mkdtempSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import * as wt from "../wt.mjs";

const failures = [];
const check = (ok, msg) => {
  if (!ok) failures.push(msg);
};

const dir = mkdtempSync(join(tmpdir(), "lf-lockasync-"));
try {
  execFileSync("git", ["init", "-q", dir], { windowsHide: true });
  const lockFile = join(wt.ensureHub(dir), "locks", "build.lock");
  // Held by a live process (this one), as another worktree's build would hold it.
  writeFileSync(lockFile, JSON.stringify({ pid: process.pid, at: new Date().toISOString(), name: "build" }));

  let fastFail = null;
  try {
    await wt.acquireLockAsync(dir, "build", { waitMs: 0 });
  } catch (e) {
    fastFail = e.message;
  }
  check(/is held/.test(fastFail ?? ""), `waitMs 0 fails fast on a held lock (got ${fastFail})`);

  let ticks = 0;
  const iv = setInterval(() => ticks++, 100);
  const release = setTimeout(() => rmSync(lockFile, { force: true }), 1200);
  const t0 = Date.now();
  let got = null;
  try {
    got = await wt.acquireLockAsync(dir, "build", { waitMs: 10000, pollMs: 100 });
  } catch (e) {
    check(false, `takes the lock once it frees (threw ${e.message})`);
  }
  clearInterval(iv);
  clearTimeout(release);
  const ms = Date.now() - t0;
  check(ticks >= 5, `the event loop ticks while waiting (${ticks} ticks of 100 ms in ${ms} ms)`);
  check(got && existsSync(lockFile), "takes the lock once it frees");
  check(ms < 5000, `takes it within seconds of the release (${ms} ms)`);
  wt.releaseLock(dir, "build");
} finally {
  rmSync(dir, { recursive: true, force: true });
}

if (failures.length) {
  for (const f of failures) console.log(`FAIL  ${f}`);
  process.exit(1);
}
console.log("lock-async-selftest: OK");
