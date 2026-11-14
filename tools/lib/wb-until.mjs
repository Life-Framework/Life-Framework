// tools/lib/wb-until.mjs - run a headless Workbench until its job is done, not until it exits.
//
// The rdb refresh (`-wbModule=ResourceManager -exitAfterInit`) writes resourceDatabase.rdb about 2 s
// after launch. On this PC the same Workbench then freezes after "Workbench Create Game" ("Application
// hangs (generated dump and waiting)", Steam API init failed) and never exits, so every refresh sat out
// its full 600 s timeout: two addons turned one `test --only` into 20+ minutes (supermarket-shop,
// 2026-11-14). waitUntilDone polls the caller's done() predicate and returns once it has held for
// settleMs; the caller then stops Workbench. validation/wb-until-selftest.mjs proves it.

import { spawnSync } from "node:child_process";

function sleepSync(ms) {
  Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
}

export function pidAlive(pid) {
  try {
    process.kill(pid, 0);
    return true;
  } catch {
    return false;
  }
}

// Stops a process and its children (Workbench through the quiet-launch helper).
export function killTreeSync(pid) {
  if (!pid) return;
  try {
    if (process.platform === "win32") spawnSync("taskkill", ["/PID", String(pid), "/T", "/F"], { windowsHide: true, stdio: "ignore", timeout: 15000 });
    else process.kill(pid, "SIGKILL");
  } catch {}
}

// Pure loop. done(): the job's result is in place (a throw counts as "not yet": a file being written).
// alive(): the process still runs. Returns { done, exited, timedOut, ms }.
export function waitUntilDone({ done, alive, timeoutMs, settleMs = 3000, pollMs = 1000, now = Date.now, sleep = sleepSync }) {
  const started = now();
  let doneAt = 0;
  for (;;) {
    let ok = false;
    try {
      ok = !!done();
    } catch {}
    if (ok) doneAt ||= now();
    else doneAt = 0;
    if (doneAt && now() - doneAt >= settleMs) return { done: true, exited: false, timedOut: false, ms: now() - started };
    if (!alive()) {
      let finalOk = false;
      try {
        finalOk = !!done();
      } catch {}
      return { done: finalOk, exited: true, timedOut: false, ms: now() - started };
    }
    if (now() - started >= timeoutMs) return { done: false, exited: false, timedOut: true, ms: now() - started };
    sleep(pollMs);
  }
}
