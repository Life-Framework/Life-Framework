// tools/validation/host-restart-backup-selftest.mjs
// inputs: tools/host/**
//
// Proves the backup a scheduled restart takes (#1560, refs #306): when the host closes on the restart
// marker (#1267) and the watch starts it again, backups/<time>-restart/ holds the closing save
// BEFORE the start is called; a `host stop` exit leaves no restart backup; a crash keeps its crash
// backup and gets no restart one. A restart backup is never pruned: rotate.mjs deletes only the plain
// start and stop backups.
// red-proof: before watchLoop took an onRestart seam and watchdog.mjs exported recordRestart, this
// file failed "recordRestart is not exported" and "a scheduled restart took no restart backup" (2026-11-15).

import { existsSync, mkdirSync, mkdtempSync, readdirSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join, resolve, dirname } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";
import { safeRemoveTree } from "../link-safety.mjs";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");
const wd = await import(pathToFileURL(join(ROOT, "tools", "host", "watchdog.mjs")).href);
const { hostPaths, findSavePoints } = await import(pathToFileURL(join(ROOT, "tools", "host", "host.mjs")).href);
const { isPlainBackup, planPrune } = await import(pathToFileURL(join(ROOT, "tools", "host", "rotate.mjs")).href);

const failures = [];
const fail = (m) => failures.push(m);

const NONE = { stopFilePresent: false, stopSeen: false, logStop: false, restartMarker: false, logRestart: false };
const T0 = new Date(2026, 10, 15, 12, 0, 0);

const base = mkdtempSync(join(tmpdir(), "lf-host-restart-backup-"));

// A fixture host dir holding one save point and one console log.
function fixture(name) {
  const dir = join(base, name);
  const p = hostPaths(dir);
  const sp = join(p.profile, "profile", ".save", "u", "game", "M", "playthrough000", "savepoint000");
  mkdirSync(sp, { recursive: true });
  writeFileSync(join(sp, "meta-info.json"), "{}");
  const logs = join(p.profile, "logs", "logs_2026-11-15_12-00-00");
  mkdirSync(logs, { recursive: true });
  writeFileSync(join(logs, "console.log"), "12:00 DEFAULT : Entered online game state.\n");
  return { dir, paths: p };
}

const backupsOf = (paths, tag) => (existsSync(paths.backups) ? readdirSync(paths.backups).filter((n) => n.endsWith(`-${tag}`) || new RegExp(`-${tag}-\\d+$`).test(n)) : []);

// Drives watchLoop over a list of host runs against a real fixture dir. launch() snapshots the
// restart backups that exist at that moment, which is how the order is proved.
function drive(name, runs) {
  const { dir, paths } = fixture(name);
  const seen = [];
  const events = [];
  let run = -1;
  let alive = false;
  let tick = 0;
  const result = wd.watchLoop({
    launch() {
      run++;
      alive = true;
      tick = 0;
      seen.push(backupsOf(paths, "restart").length);
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
    onCrash: (n) => {
      wd.recordCrash(dir, new Date(T0.getTime() + n * 1000));
      events.push(`crash ${n}`);
    },
    onRestart: () => {
      const r = wd.recordRestart(dir, T0);
      events.push(`restart-backup ${r.backup ? "saved" : "none"}`);
      return r;
    },
    afterExit: (d) => events.push(`exit ${d.action}${d.crash ? "+crash" : ""}`),
    sleep: () => {},
    log: () => {},
    maxRestarts: 5,
  });
  return { dir, paths, seen, events, result, launches: run + 1 };
}

try {
  // 1. A restart-marker exit: the restart backup holds the save before the second launch.
  if (typeof wd.recordRestart !== "function") fail("recordRestart is not exported by tools/host/watchdog.mjs");
  else {
    const sched = drive("sched", [{ exitAfter: 2, signals: { restartMarker: true } }, { exitAfter: 2, signals: { stopFilePresent: true } }]);
    const made = backupsOf(sched.paths, "restart");
    if (made.length !== 1) fail(`a scheduled restart took ${made.length} restart backups, expected 1 (${sched.events})`);
    else if (!findSavePoints(join(sched.paths.backups, made[0])).length) fail("the restart backup holds no save point");
    if (sched.seen[1] !== 1) fail(`the restart backup did not exist when the host started again (seen ${sched.seen})`);
    if (sched.seen[0] !== 0) fail(`a restart backup existed before the first start (seen ${sched.seen})`);
    if (sched.events.some((e) => e.startsWith("crash"))) fail("a scheduled restart was recorded as a crash");
    const order = sched.events.join(" > ");
    if (!/exit restart > restart-backup saved > launch 1/.test(order)) fail(`the backup did not sit between the exit and the next start: ${order}`);

    // 2. The log line stands in for the marker the same way.
    const viaLog = drive("via-log", [{ exitAfter: 2, signals: { logRestart: true } }, { exitAfter: 2, signals: { logStop: true } }]);
    if (backupsOf(viaLog.paths, "restart").length !== 1) fail("a restart seen only in the log took no restart backup");

    // 3. A stop exit: no restart backup, the watch ends.
    const stop = drive("stop", [{ exitAfter: 2, signals: { stopFilePresent: true } }]);
    if (backupsOf(stop.paths, "restart").length) fail("a host stop left a restart backup");
    if (stop.result.reason !== "stop" || stop.launches !== 1) fail(`a host stop did not end the watch: ${JSON.stringify(stop.result)}`);
    // A stop beats a scheduled close.
    const both = drive("both", [{ exitAfter: 2, signals: { restartMarker: true, stopSeen: true } }]);
    if (backupsOf(both.paths, "restart").length) fail("a stop that raced a scheduled close left a restart backup");

    // 4. A crash exit keeps its crash backup and takes no restart backup.
    const crash = drive("crash", [{ exitAfter: 2, signals: {} }, { exitAfter: 2, signals: { stopSeen: true } }]);
    if (backupsOf(crash.paths, "restart").length) fail("a crash left a restart backup");
    if (backupsOf(crash.paths, "crash").length !== 1) fail(`a crash kept ${backupsOf(crash.paths, "crash").length} crash backups, expected 1`);
    if (!crash.events.includes("crash 1")) fail("a crash did not call the crash recorder");

    // 5. recordRestart on its own: tag, stamp, no save yet, twin stamps, and a failure never throws.
    const lone = fixture("lone");
    const r1 = wd.recordRestart(lone.dir, T0);
    if (!r1.backup || !/\d{4}-\d{2}-\d{2}_\d{2}-\d{2}-\d{2}-restart$/.test(r1.backup)) fail(`recordRestart wrote ${r1.backup}, expected backups/<stamp>-restart`);
    const r2 = wd.recordRestart(lone.dir, T0);
    if (!r2.backup || r2.backup === r1.backup || !/-restart-2$/.test(r2.backup)) fail(`two restarts in the same second collided: ${r1.backup} / ${r2.backup}`);
    const empty = join(base, "empty");
    const none = wd.recordRestart(empty, T0);
    if (none.backup !== null) fail(`recordRestart with no save returned ${none.backup}`);

    // 6. The loop survives a failing backup seam: the host still starts again.
    const thrown = [];
    let up = false;
    let starts = 0;
    const sturdy = wd.watchLoop({
      launch: () => { starts++; up = true; return { ok: true, pid: 1 }; },
      isAlive: () => up,
      signals: () => { up = false; return { ...NONE, ...(starts === 1 ? { restartMarker: true } : { stopFilePresent: true }) }; },
      onCrash() {},
      onRestart() { throw new Error("disk full"); },
      afterExit() {},
      sleep() {},
      log: (m) => thrown.push(m),
      maxRestarts: 3,
    });
    if (sturdy.launches !== 2 || sturdy.reason !== "stop") fail(`a failing restart backup stopped the restart: ${JSON.stringify(sturdy)}`);
    if (!thrown.some((m) => /disk full/.test(m))) fail("a failing restart backup was not logged");
  }

  // 7. Rotation never deletes a restart backup, however many plain ones it prunes (#1260), and
  //    the plain test still names only the start and stop tags.
  const rot = fixture("rotate");
  for (let i = 0; i < 6; i++) mkdirSync(join(rot.paths.backups, `2026-11-14_10-0${i}-00-stop`), { recursive: true });
  for (let i = 0; i < 3; i++) mkdirSync(join(rot.paths.backups, `2026-11-14_11-0${i}-00-start`), { recursive: true });
  mkdirSync(join(rot.paths.backups, "2026-11-14_09-00-00-restart"), { recursive: true });
  mkdirSync(join(rot.paths.backups, "2026-11-14_09-30-00-restart-2"), { recursive: true });
  mkdirSync(join(rot.paths.backups, "2026-11-14_09-45-00-crash"), { recursive: true });
  const plan = planPrune(rot.paths.backups, 2);
  if (plan.delete.some((n) => /restart|crash/.test(n))) fail(`a prune would delete a restart or crash backup: ${plan.delete.filter((n) => /restart|crash/.test(n))}`);
  if (plan.delete.length !== 7) fail(`the prune plan deletes ${plan.delete.length}, expected the 7 oldest start/stop ones`);
  if (isPlainBackup("2026-11-14_09-00-00-restart") || isPlainBackup("2026-11-14_09-30-00-restart-2")) fail("a restart backup counts as a plain backup, so rotation could delete it");
  if (!isPlainBackup("2026-11-14_10-00-00-stop") || !isPlainBackup("2026-11-14_10-00-00-start-2")) fail("a start or stop backup is no longer plain");
} finally {
  safeRemoveTree(base);
}

if (failures.length) {
  for (const f of failures) console.log(`FAIL  ${f}`);
  process.exit(1);
}
console.log("host-restart-backup-selftest ok");
