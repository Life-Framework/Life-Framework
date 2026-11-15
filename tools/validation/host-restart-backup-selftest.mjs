// tools/validation/host-restart-backup-selftest.mjs
// inputs: tools/host/**
//
// Proves the backup a scheduled restart takes (#1560, refs #306): when the host closes on the restart
// marker (#1267) and the watch starts it again, backups/<time>-restart/ holds the closing save
// BEFORE the start is called; a `host stop` exit leaves no restart backup; a crash keeps its crash
// backup and gets no restart one. A restart backup rotates like a start or stop one (rotate.mjs,
// #1560); a crash, wipe, preflight, manual or replaced-by-restore backup is never pruned. `host watch`
// wires the seam: its main() takes the restart backup on a scheduled close.
// red-proof: before watchLoop took an onRestart seam and watchdog.mjs exported recordRestart, this
// file failed "recordRestart is not exported" and "a scheduled restart took no restart backup" (2026-11-15).
// Restart prune (2026-11-15, lead decision): recordRestart pruned nothing, so 25 scheduled restarts left 25 backups.
// Wire and rotation (2026-11-15), watch.mjs without onRestart and `restart` not plain: "host watch took 0 restart
// backups on a scheduled close", "a restart backup beyond N is not pruned", "the prune plan deletes 7, expected the 10 oldest".

import { spawn } from "node:child_process";
import { existsSync, mkdirSync, mkdtempSync, readdirSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join, resolve, dirname } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";
import { safeRemoveTree } from "../link-safety.mjs";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");
const wd = await import(pathToFileURL(join(ROOT, "tools", "host", "watchdog.mjs")).href);
const { hostPaths, findSavePoints } = await import(pathToFileURL(join(ROOT, "tools", "host", "host.mjs")).href);
const { isPlainBackup, planPrune, pruneBackups } = await import(pathToFileURL(join(ROOT, "tools", "host", "rotate.mjs")).href);

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

  // 7. A restart backup rotates like a start or stop one (#1560); a crash backup never does.
  const rot = fixture("rotate");
  const mk = (n) => mkdirSync(join(rot.paths.backups, n), { recursive: true });
  for (let i = 0; i < 6; i++) mk(`2026-11-14_10-0${i}-00-stop`);
  for (let i = 0; i < 3; i++) mk(`2026-11-14_11-0${i}-00-start`);
  mk("2026-11-14_09-00-00-restart");
  mk("2026-11-14_09-30-00-restart-2");
  mk("2026-11-14_09-45-00-crash");
  mk("2026-11-14_09-50-00-wipe");
  mk("2026-11-14_09-55-00-manual");
  mk("2026-11-14_09-58-00-replaced-by-restore");
  mk("2026-11-14_09-59-00-preflight-1.8.0.13-to-1.9.0.37");
  mk("2026-11-15_06-00-00-restart");
  const plan = planPrune(rot.paths.backups, 2);
  // 12 plain (2 old restart, 6 stop, 3 start, 1 new restart): the 10 oldest go, the newest two stay.
  if (plan.delete.length !== 10) fail(`the prune plan deletes ${plan.delete.length}, expected the 10 oldest plain ones`);
  for (const n of ["2026-11-14_09-00-00-restart", "2026-11-14_09-30-00-restart-2"]) if (!plan.delete.includes(n)) fail(`a restart backup beyond N is not pruned: ${n}`);
  if (!plan.keep.includes("2026-11-15_06-00-00-restart")) fail("the newest restart backup is not kept");
  for (const t of ["crash", "wipe", "manual", "replaced-by-restore", "preflight-1.8.0.13-to-1.9.0.37"]) {
    if (plan.delete.some((n) => n.endsWith(`-${t}`))) fail(`a prune would delete a ${t} backup`);
  }
  const done = pruneBackups(rot.paths.backups, 2);
  if (done.deleted.length !== 10) fail(`pruneBackups deleted ${done.deleted.length}, expected 10`);
  if (!existsSync(join(rot.paths.backups, "2026-11-14_09-45-00-crash"))) fail("pruneBackups deleted the crash backup");
  if (existsSync(join(rot.paths.backups, "2026-11-14_09-00-00-restart"))) fail("pruneBackups kept a restart backup beyond N");
  if (!isPlainBackup("2026-11-14_09-00-00-restart") || !isPlainBackup("2026-11-14_09-30-00-restart-2")) fail("a restart backup is not plain, so rotation never counts it");
  if (isPlainBackup("2026-11-14_09-45-00-crash") || isPlainBackup("2026-11-14_09-50-00-wipe")) fail("a crash or wipe backup counts as plain");

  // 8. `host watch` wires the seam: main() with a host that exits on the restart marker takes the
  //    restart backup before it calls `host start` again (a stub cli that fails ends the watch there).
  const watchMod = await import(pathToFileURL(join(ROOT, "tools", "host", "commands", "watch.mjs")).href);
  const w = fixture("watch");
  const stubRoot = join(base, "stub-root");
  mkdirSync(join(stubRoot, "tools"), { recursive: true });
  writeFileSync(join(stubRoot, "tools", "cli.mjs"), "process.exit(1);");
  const child = spawn(process.execPath, ["-e", "setTimeout(() => {}, 1500)"], { stdio: "ignore", windowsHide: true });
  writeFileSync(w.paths.state, JSON.stringify({ pid: child.pid }));
  writeFileSync(join(w.paths.profile, ...wd.RESTART_MARKER), "scheduled restart");
  const lines = [];
  const realLog = console.log;
  console.log = (m) => lines.push(String(m));
  let code;
  try {
    code = await watchMod.main([], { hostDir: w.dir, root: stubRoot });
  } finally {
    console.log = realLog;
  }
  const watched = backupsOf(w.paths, "restart");
  if (watched.length !== 1) fail(`host watch took ${watched.length} restart backups on a scheduled close, expected 1 (watch.mjs passes no onRestart?) ${lines.join(" | ")}`);
  else if (!findSavePoints(join(w.paths.backups, watched[0])).length) fail("the restart backup host watch took holds no save point");
  if (code !== 1) fail(`host watch returned ${code} after the stubbed start failed, expected 1`);
  if (backupsOf(w.paths, "crash").length) fail("host watch recorded a scheduled close as a crash");

  // 9. Scheduled restarts prune too (lead decision): 25 scheduled restarts leave the newest 20 restart
  //    backups; the crash, wipe and manual backups are all still there; a failing prune never stops the
  //    restart backup (it is still taken and returned).
  const many = fixture("many");
  const others = ["2026-11-14_09-45-00-crash", "2026-11-14_09-50-00-wipe", "2026-11-14_09-55-00-manual"];
  for (const n of others) mkdirSync(join(many.paths.backups, n), { recursive: true });
  let took = 0;
  for (let i = 0; i < 25; i++) if (wd.recordRestart(many.dir, new Date(T0.getTime() + i * 60000)).backup) took++;
  if (took !== 25) fail(`25 scheduled restarts took ${took} restart backups, expected 25`);
  const left = readdirSync(many.paths.backups);
  const restartsLeft = left.filter((n) => isPlainBackup(n));
  if (restartsLeft.length !== 20) fail(`after 25 scheduled restarts ${restartsLeft.length} restart or plain backups remain, expected 20`);
  if (restartsLeft.some((n) => !/-restart$/.test(n))) fail(`a non-restart plain backup was left among the restarts: ${restartsLeft}`);
  if (left.some((n) => /_12-0[0-4]-00-restart$/.test(n))) fail("the 5 oldest scheduled restart backups were not pruned");
  if (!left.some((n) => /_12-24-00-restart$/.test(n))) fail("the newest scheduled restart backup was pruned");
  for (const n of others) if (!existsSync(join(many.paths.backups, n))) fail(`a scheduled restart pruned the ${n} backup`);
  const broken = wd.recordRestart(fixture("badkeep").dir, T0, { keep: -1 });
  if (!broken.backup) fail("a failing prune stopped the restart backup from being taken");
} finally {
  safeRemoveTree(base);
}

if (failures.length) {
  for (const f of failures) console.log(`FAIL  ${f}`);
  process.exit(1);
}
console.log("SUMMARY host-restart-backup-selftest ok");
