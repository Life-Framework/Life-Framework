// tools/validation/host-watchdog-selftest.mjs
// inputs: tools/host/**
//
// Proves the host watchdog (`tools\cli host watch`, #1262): the decision function maps an exit of
// the host process to restart (crash), stop (clean `host stop`) or restart (scheduled close, the
// marker #1267 writes); a crash keeps the console log in backups/<time>-crash/, backs the saves
// up with the crash tag, and the rotation never deletes it; and the watch loop, driven with fake
// process and clock seams, restarts a crash, ends on a stop, and restarts a scheduled close.
// red-proof: before tools/host/watchdog.mjs existed this file died on the import:
// "Cannot find module '.../tools/host/watchdog.mjs'" (2026-11-15).

import { existsSync, mkdirSync, mkdtempSync, readdirSync, readFileSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join, resolve, dirname } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";
import { spawnSync } from "node:child_process";
import { safeRemoveTree } from "../link-safety.mjs";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");
const wd = await import(pathToFileURL(join(ROOT, "tools", "host", "watchdog.mjs")).href);
const { hostPaths, findSavePoints } = await import(pathToFileURL(join(ROOT, "tools", "host", "host.mjs")).href);
const { planPrune } = await import(pathToFileURL(join(ROOT, "tools", "host", "rotate.mjs")).href);

const failures = [];
const fail = (m) => failures.push(m);

const NONE = { stopFilePresent: false, stopSeen: false, logStop: false, restartMarker: false, logRestart: false };

// 1. The decision function.
const cases = [
  ["a bare exit is a crash", { ...NONE }, "restart", true],
  ["a stop file at exit is a clean stop", { ...NONE, stopFilePresent: true }, "stop", false],
  ["a stop file seen while the host ran (the mod already deleted it) is a clean stop", { ...NONE, stopSeen: true }, "stop", false],
  ["the stop log line is a clean stop", { ...NONE, logStop: true }, "stop", false],
  ["the scheduled-close marker is a restart but not a crash", { ...NONE, restartMarker: true }, "restart", false],
  ["the scheduled-close log line is a restart but not a crash", { ...NONE, logRestart: true }, "restart", false],
  ["a stop beats a scheduled close", { ...NONE, restartMarker: true, stopSeen: true }, "stop", false],
];
for (const [name, state, action, crash] of cases) {
  const d = wd.decideExit(state);
  if (d.action !== action || d.crash !== crash) fail(`${name}: got ${JSON.stringify(d)}`);
}

const base = mkdtempSync(join(tmpdir(), "lf-host-watchdog-"));
const dir = join(base, "host");
const paths = hostPaths(dir);
const T0 = new Date(2026, 10, 15, 12, 0, 0);

function seed() {
  const sp = join(paths.profile, "profile", ".save", "u", "game", "M", "playthrough000", "savepoint000");
  mkdirSync(sp, { recursive: true });
  writeFileSync(join(sp, "meta-info.json"), "{}");
  const logs = join(paths.profile, "logs", "logs_2026-11-15_12-00-00");
  mkdirSync(logs, { recursive: true });
  writeFileSync(join(logs, "console.log"), "12:00 DEFAULT : Entered online game state.\n12:05 SCRIPT (E): boom\n");
}

try {
  seed();

  // 2. The crash record: console log copied, saves backed up with the crash tag.
  const rec = wd.recordCrash(dir, T0);
  if (!rec.backup || !/-crash$/.test(rec.backup)) fail(`crash backup is not tagged crash: ${rec.backup}`);
  else {
    if (!findSavePoints(rec.backup).length) fail("the crash backup holds no save point");
    const copy = join(rec.backup, wd.CRASH_LOG_DIR, "console.log");
    if (!existsSync(copy) || !readFileSync(copy, "utf8").includes("boom")) fail("the crash log was not copied into the crash backup");
  }
  // A crash with no save: still keeps the log in backups/<time>-crash/.
  const logsOnly = join(base, "host2");
  const lp = hostPaths(logsOnly);
  mkdirSync(join(lp.profile, "logs", "logs_2026-11-15_13-00-00"), { recursive: true });
  writeFileSync(join(lp.profile, "logs", "logs_2026-11-15_13-00-00", "console.log"), "early crash\n");
  const early = wd.recordCrash(logsOnly, T0);
  if (!early.logDir || !/-crash$/.test(early.logDir) || !existsSync(join(early.logDir, wd.CRASH_LOG_DIR, "console.log"))) fail(`a crash with no save kept no log: ${JSON.stringify(early)}`);

  // 3. A crash backup is never pruned, however many plain ones surround it.
  for (let i = 0; i < 25; i++) mkdirSync(join(paths.backups, `2026-11-14_10-${String(i).padStart(2, "0")}-00-stop`), { recursive: true });
  const plan = planPrune(paths.backups, 5);
  if (plan.delete.some((n) => /crash/.test(n))) fail(`a prune would delete a crash backup: ${plan.delete.filter((n) => /crash/.test(n))}`);
  if (plan.delete.length !== 20) fail(`the prune plan deletes ${plan.delete.length}, expected the 20 oldest plain ones`);

  // 4. The loop with fake seams. Each scenario is a list of host runs; run i ends with the
  //    given signals. `launches` counts the restarts, `slept` stands in for the clock.
  function drive(runs, opts = {}) {
    const events = [];
    let run = -1;
    let alive = false;
    let tick = 0;
    const seams = {
      launch() {
        run++;
        alive = true;
        tick = 0;
        events.push(`launch ${run}`);
        return { ok: true, pid: 1000 + run };
      },
      isAlive: () => alive,
      signals() {
        tick++;
        const r = runs[run];
        if (tick >= r.exitAfter) alive = false;
        return { ...NONE, ...(tick >= r.exitAfter ? r.signals : {}) };
      },
      onCrash: (n) => events.push(`crash ${n}`),
      afterExit: (d) => events.push(`exit ${d.action}${d.crash ? "+crash" : ""}`),
      sleep: () => {},
      log: () => {},
    };
    const r = wd.watchLoop({ ...seams, maxRestarts: opts.maxRestarts ?? 10 });
    return { events, result: r, launches: run + 1 };
  }

  const crash = drive([{ exitAfter: 3, signals: {} }, { exitAfter: 2, signals: { stopSeen: true } }]);
  if (crash.launches !== 2) fail(`a crash was not restarted exactly once (launches ${crash.launches}: ${crash.events})`);
  if (!crash.events.includes("crash 1")) fail("a crash did not call the crash recorder");
  if (crash.result.reason !== "stop") fail(`the watch did not end on the stop: ${JSON.stringify(crash.result)}`);

  const clean = drive([{ exitAfter: 2, signals: { stopFilePresent: true } }]);
  if (clean.launches !== 1 || clean.result.reason !== "stop") fail(`a clean stop did not end the watch: ${JSON.stringify(clean.result)} launches ${clean.launches}`);
  if (clean.events.some((e) => e.startsWith("crash"))) fail("a clean stop was recorded as a crash");

  const sched = drive([{ exitAfter: 2, signals: { restartMarker: true } }, { exitAfter: 2, signals: { stopFilePresent: true } }]);
  if (sched.launches !== 2) fail(`a scheduled close was not restarted (launches ${sched.launches}: ${sched.events})`);
  if (sched.events.some((e) => e.startsWith("crash"))) fail("a scheduled close was recorded as a crash");

  // A launch that fails ends the watch rather than looping; so does a restart cap.
  const bad = wd.watchLoop({ launch: () => ({ ok: false, reason: "no exe" }), isAlive: () => false, signals: () => NONE, onCrash() {}, afterExit() {}, sleep() {}, log() {}, maxRestarts: 3 });
  if (bad.reason !== "launch-failed") fail(`a failed launch gave ${JSON.stringify(bad)}`);
  const loop = drive(Array.from({ length: 6 }, () => ({ exitAfter: 1, signals: {} })), { maxRestarts: 3 });
  if (loop.result.reason !== "max-crashes" || loop.launches !== 4) fail(`the restart cap did not stop the watch: ${JSON.stringify(loop.result)} launches ${loop.launches}`);

  // The consumed flag files go away through safeRemoveTree, and a missing one is fine.
  const flags = wd.watchFiles(dir);
  mkdirSync(dirname(flags.marker), { recursive: true });
  writeFileSync(flags.marker, "scheduled restart");
  writeFileSync(flags.stopFile, "x");
  wd.clearFlags(dir);
  wd.clearFlags(dir);
  if (existsSync(flags.marker) || existsSync(flags.stopFile)) fail("clearFlags left a flag file");
  if (wd.readSignals(dir).restartMarker) fail("readSignals still sees a cleared marker");
  writeFileSync(flags.marker, "scheduled restart");
  if (!wd.readSignals(dir).restartMarker) fail("readSignals does not see the restart marker");
  const logSig = wd.readSignals(dir, { final: true });
  if (logSig.logStop || logSig.logRestart) fail("the crash log has no stop or restart line but readSignals found one");

  // 5. The CLI subcommand loads and `host help` lists it.
  const help = spawnSync(process.execPath, [join(ROOT, "tools", "cli.mjs"), "host", "help"], { cwd: ROOT, encoding: "utf8", timeout: 60000, windowsHide: true });
  if (!/watch\b/.test(`${help.stdout}${help.stderr}`)) fail("host help does not list the watch subcommand");

  // 6. The stop and marker paths the watchdog reads are the ones the mod and `host stop` use.
  if (join(...wd.RESTART_MARKER) !== join("profile", "lf-host-restart")) fail(`restart marker is ${wd.RESTART_MARKER}, the mod writes $profile:lf-host-restart (#1267)`);
  if (!readdirSync(paths.backups).some((n) => /-crash$/.test(n))) fail("no crash folder left in backups");
} finally {
  safeRemoveTree(base);
}

if (failures.length) {
  for (const f of failures) console.log(`FAIL  ${f}`);
  process.exit(1);
}
console.log("host-watchdog-selftest ok");
