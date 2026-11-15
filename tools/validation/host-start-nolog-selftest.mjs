// tools/validation/host-start-nolog-selftest.mjs
// inputs: tools/host/host.mjs, tools/validation/fixtures/host-start-nolog/silent-server.mjs, tools/validation/fixtures/host-start-nolog/early-exit.mjs, tools/validation/fixtures/host-start-nolog/ready-server.mjs
//
// Proves the `host start` wait (#1667) stops inside a short budget and names the cause, with
// stand-in servers (real child processes) in place of ArmaReforgerServer:
//   - one that writes no log and no logs folder: "nolog", says the folder was never created, prints
//     the args the server got, and kills it;
//   - the same with an empty logs folder: "nolog", says no new console.log;
//   - one that exits with code 3 before any log: "exited", names exit code 3 (Windows: the exit
//     watch sidecar);
//   - one that writes the ready line: "ready".
// red-proof: before waitForHostLog existed every case failed with "FAIL waitForHostLog missing"
// (2026-11-15).

import { mkdirSync, mkdtempSync, rmSync } from "node:fs";
import { spawn } from "node:child_process";
import { tmpdir } from "node:os";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");
const FIX = join(ROOT, "tools", "validation", "fixtures", "host-start-nolog");
const tmp = mkdtempSync(join(tmpdir(), "lf-host-nolog-"));
let failed = 0;
function fail(msg) {
  console.log(`FAIL  ${msg}`);
  failed++;
}

const host = await import(pathToFileURL(join(ROOT, "tools", "host", "host.mjs")).href);
const NOLOG_MS = 3000;
const BUDGET_MS = 15000; // well under the 40 s deadline: the cause, not the timeout, must end the wait

function run(name, script, { mkLogs = false, noLogMs = NOLOG_MS } = {}) {
  const profile = join(tmp, name, "profile");
  mkdirSync(profile, { recursive: true });
  if (mkLogs) mkdirSync(join(profile, "logs"), { recursive: true });
  const args = ["-server", "M.conf", "-profile", profile];
  const child = spawn(process.execPath, [join(FIX, script), profile], { detached: true, stdio: "ignore", windowsHide: true });
  child.unref();
  const exitWatch = host.startExitWatch(child.pid, join(tmp, name, "host-exit.txt"));
  const t0 = Date.now();
  const w = host.waitForHostLog({ pid: child.pid, profile, before: new Set(), deadlineMs: t0 + 40000, noLogMs, pollMs: 200, args, exitWatch });
  const ms = Date.now() - t0;
  if (host.pidAlive(child.pid) && w.status !== "ready") fail(`${name}: the stand-in is still running after the wait`);
  if (w.status === "ready") process.kill(child.pid);
  return { w, ms, text: (w.lines ?? []).join("\n") };
}

try {
  if (typeof host.waitForHostLog !== "function" || typeof host.startExitWatch !== "function") {
    fail("waitForHostLog missing (tools/host/host.mjs exports no waitForHostLog/startExitWatch)");
  } else {
    const a = run("silent", "silent-server.mjs");
    if (a.w.status !== "nolog") fail(`silent: status ${a.w.status}, want nolog`);
    if (a.ms > BUDGET_MS) fail(`silent: the wait took ${a.ms} ms, want under ${BUDGET_MS}`);
    if (!/log folder .* was never created/.test(a.text)) fail(`silent: no 'log folder ... was never created' line:\n${a.text}`);
    if (!a.text.includes("the server got: -server M.conf -profile")) fail(`silent: the args the server got are not printed:\n${a.text}`);

    const b = run("emptylogs", "silent-server.mjs", { mkLogs: true });
    if (b.w.status !== "nolog") fail(`emptylogs: status ${b.w.status}, want nolog`);
    if (!/no new console\.log under/.test(b.text)) fail(`emptylogs: no 'no new console.log under' line:\n${b.text}`);

    // The stand-in exits at 6 s (time for the exit watch to attach under a loaded validate); the
    // no-log stop is pushed past that so the exit, not the missing log, ends the wait.
    const c = run("exits", "early-exit.mjs", { noLogMs: 30000 });
    if (c.w.status !== "exited") fail(`exits: status ${c.w.status}, want exited`);
    if (c.ms > BUDGET_MS) fail(`exits: the wait took ${c.ms} ms, want under ${BUDGET_MS}`);
    if (!c.text.startsWith("FAIL  the server exited during startup")) fail(`exits: first line is not 'FAIL  the server exited during startup':\n${c.text}`);
    if (process.platform === "win32" && !c.text.includes("exit code 3")) fail(`exits: the exit code is not named:\n${c.text}`);

    const d = run("ready", "ready-server.mjs");
    if (d.w.status !== "ready") fail(`ready: status ${d.w.status}, want ready (${d.text})`);
    if (!d.w.log || !d.w.log.endsWith("console.log")) fail(`ready: log ${d.w.log}`);
  }
} finally {
  rmSync(tmp, { recursive: true, force: true });
}

console.log(failed ? `SUMMARY host-start-nolog-selftest fail ${failed}` : "SUMMARY host-start-nolog-selftest ok");
process.exit(failed ? 1 : 0);
