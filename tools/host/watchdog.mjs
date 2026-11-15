// Host watchdog (#1262): the logic of `tools\cli host watch` (tools/host/commands/watch.mjs).
//
// The watch supervises the host process `host start` launched. An exit is one of:
//   stop     `host stop` asked for it (stop file, or the mod's "stop requested" log line): the
//            watch ends.
//   restart  a scheduled close (Ops O8, #1267: the mod writes the restart marker, then closes
//            like `host stop`): the watch backs the saves up as backups/<time>-restart/ (#1560),
//            then starts the host again. Not a crash, no crash record.
//   crash    anything else: the console log is kept in backups/<time>-crash/, the saves are
//            backed up there, the plain backups are rotated, and the host starts again (host start
//            continues the newest save with -loadSessionSave).
//
// The mod deletes the stop file within 2 s of seeing it, so the watch polls fast and also reads the
// log line the mod prints at the stop and at the restart. A stop beats a scheduled close.
import { cpSync, existsSync, mkdirSync, openSync, readSync, closeSync, statSync } from "node:fs";
import { dirname, join } from "node:path";
import { backupSaves, freshDir, hostPaths, newestConsoleLog, pidAlive, readHostState, stamp, STOP_FILE } from "./host.mjs";
import { DEFAULT_KEEP, pruneBackups } from "./rotate.mjs";
import { crashAlertText, decideCrashLoop } from "./crashloop.mjs";
import { safeRemoveTree } from "../link-safety.mjs";

// LF_RestartClose.MARKER_FILE = "$profile:lf-host-restart" (Ops O8, #1267), under -profile <dir>\profile.
export const RESTART_MARKER = ["profile", "lf-host-restart"];
// The folder inside a crash backup that holds the crashed run's log folder.
export const CRASH_LOG_DIR = "crash-log";
export const POLL_MS = 250;
export const DEFAULT_MAX_CRASHES = 10;

// The log lines the mod prints just before it closes (LF_HostStop.Poll, LF_RestartClose.OnRestartDue).
const LOG_STOP = /\[LFDebug:Host\] stop requested/;
const LOG_RESTART = /\[LFDebug:Ops\] restart due: wrote/;

// What an exit of the host process means. Pure.
// state: { stopFilePresent, stopSeen, logStop, restartMarker, logRestart }.
export function decideExit(state) {
  const stop = !!(state.stopFilePresent || state.stopSeen || state.logStop);
  if (stop) return { action: "stop", crash: false, why: "host stop" };
  if (state.restartMarker || state.logRestart) return { action: "restart", crash: false, why: "scheduled restart" };
  return { action: "restart", crash: true, why: "exit without a stop or a restart marker" };
}

export function watchFiles(dir) {
  const { profile } = hostPaths(dir);
  return { stopFile: join(profile, ...STOP_FILE), marker: join(profile, ...RESTART_MARKER) };
}

function tailText(file, bytes = 1 << 20) {
  try {
    const size = statSync(file).size;
    const fd = openSync(file, "r");
    try {
      const len = Math.min(size, bytes);
      const buf = Buffer.alloc(len);
      readSync(fd, buf, 0, len, size - len);
      return buf.toString("utf8");
    } finally {
      closeSync(fd);
    }
  } catch {
    return "";
  }
}

// Reads the signals that tell why the host exited. `final` also reads the console log (once, after
// the exit); the polling reads only the two flag files, which is cheap at 250 ms.
export function readSignals(dir, { final = false } = {}) {
  const { stopFile, marker } = watchFiles(dir);
  const s = { stopFilePresent: existsSync(stopFile), stopSeen: false, logStop: false, restartMarker: existsSync(marker), logRestart: false };
  if (final) {
    const log = newestConsoleLog(hostPaths(dir).profile);
    const text = log ? tailText(log) : "";
    s.logStop = LOG_STOP.test(text);
    s.logRestart = LOG_RESTART.test(text);
  }
  return s;
}

// A flag file the watch consumed. Deleted through safeRemoveTree (AGENTS.md: delete only that way).
export function clearFlags(dir) {
  const { stopFile, marker } = watchFiles(dir);
  for (const f of [stopFile, marker]) if (existsSync(f)) safeRemoveTree(f);
}

// A crash: keep the crashed run's console log in backups/<time>-crash/, back the saves up there
// (the `crash` tag: rotate.mjs never deletes it) and rotate the plain backups.
// Returns { backup, logDir, pruned }: backup is null when no save existed yet; logDir is the
// crash folder either way.
export function recordCrash(dir, now = new Date(), { keep = DEFAULT_KEEP } = {}) {
  const { profile, backups } = hostPaths(dir);
  const log = newestConsoleLog(profile);
  let backup = null;
  try {
    backup = backupSaves(dir, "crash", now);
  } catch (e) {
    console.log(`[LFDebug:HostWatch] crash backup of the saves failed: ${e.message}`);
  }
  let logDir = backup;
  if (!logDir) {
    logDir = freshDir(join(backups, `${stamp(now)}-crash`));
    mkdirSync(logDir, { recursive: true });
  }
  if (log) {
    try {
      cpSync(dirname(log), join(logDir, CRASH_LOG_DIR), { recursive: true, verbatimSymlinks: true });
    } catch (e) {
      console.log(`[LFDebug:HostWatch] crash log copy failed: ${e.message}`);
    }
  }
  let pruned = [];
  try {
    pruned = pruneBackups(backups, keep).deleted;
  } catch (e) {
    console.log(`[LFDebug:HostWatch] backup rotation failed: ${e.message}`);
  }
  return { backup, logDir, pruned };
}

// A scheduled restart (#1560, refs #306): back the closing save up as backups/<time>-restart/ before
// the host starts again. The `restart` tag is a plain one like start and stop: rotate.mjs prunes the
// oldest beyond N (crash, wipe and the other tags never). Returns { backup }, null when no save exists
// yet. Never throws: a failed backup is logged and the restart goes on.
export function recordRestart(dir, now = new Date(), { keep = DEFAULT_KEEP } = {}) {
  const { backups } = hostPaths(dir);
  let backup = null;
  try {
    backup = backupSaves(dir, "restart", now);
  } catch (e) {
    console.log(`[LFDebug:HostWatch] restart backup of the saves failed: ${e.message}`);
  }
  // Rotate after the backup, as recordCrash does, so a host that restarts twice a day stays at `keep` plain backups.
  try {
    pruneBackups(backups, keep);
  } catch (e) {
    console.log(`[LFDebug:HostWatch] backup rotation after the restart failed: ${e.message}`);
  }
  return { backup };
}

// The supervision loop. Every effect is a seam, so the selftest drives it with fakes:
//   launch()          -> { ok, pid?, reason? }  start the host, return once it is up
//   isAlive()         -> bool                   the host process still runs
//   signals(final)    -> state for decideExit   (flag files; with final, the log lines too)
//   onCrash(n)        record the nth crash (log, backups); may return { logDir } for the alert
//   onRestart(d)      optional: a scheduled close (not a crash, not a stop) is about to start the host
//                     again; back the saves up first (recordRestart). A throw is logged, never blocks the start
//   afterExit(d)      the exit was judged d (clear the flag files, print)
//   sleep(ms), log(msg)
//   now()             epoch ms, for the crash-loop window (default Date.now)
//   crashLoop         { limit, windowMs } turns the crash-loop guard on (crashloop.mjs, #1263); null/absent is off
// Ends with { reason: "stop" | "launch-failed" | "max-crashes" | "crash-loop", crashes, launches }.
// "crash-loop" also carries `alert`, the staff message naming the crash logs; the caller sends it.
export function watchLoop({ launch, isAlive, signals, onCrash, onRestart = null, afterExit, sleep, log, maxRestarts = DEFAULT_MAX_CRASHES, pollMs = POLL_MS, restartDelayMs = 0, now = Date.now, crashLoop = null }) {
  let crashes = 0;
  let launches = 0;
  const crashTimes = [];
  const crashLogs = [];
  const merge = (a, b) => ({
    stopFilePresent: a.stopFilePresent || b.stopFilePresent,
    stopSeen: a.stopSeen || b.stopSeen || a.stopFilePresent || b.stopFilePresent,
    logStop: a.logStop || b.logStop,
    restartMarker: a.restartMarker || b.restartMarker,
    logRestart: a.logRestart || b.logRestart,
  });
  for (;;) {
    const l = launch();
    launches++;
    if (!l.ok) {
      log(`launch failed: ${l.reason ?? "unknown"}; the watch ends`);
      return { reason: "launch-failed", crashes, launches };
    }
    let acc = { stopFilePresent: false, stopSeen: false, logStop: false, restartMarker: false, logRestart: false };
    while (isAlive()) {
      sleep(pollMs);
      acc = merge(acc, signals(false));
    }
    acc = merge(acc, signals(true));
    const d = decideExit(acc);
    afterExit(d);
    if (d.action === "stop") {
      log("the host was stopped (host stop); the watch ends");
      return { reason: "stop", crashes, launches };
    }
    if (d.crash) {
      crashes++;
      const at = now();
      const rec = onCrash(crashes);
      crashTimes.push(at);
      crashLogs.push(rec?.logDir ?? null);
      if (crashLoop && decideCrashLoop(crashTimes, at, crashLoop) === "hold") {
        const inWindow = crashTimes.map((t) => at - t < crashLoop.windowMs);
        const logDirs = crashLogs.filter((d, i) => d && inWindow[i]);
        const recent = inWindow.filter(Boolean).length;
        log(`crash loop: ${recent} crashes in ${crashLoop.windowMs / 60000} minute(s); the watch ends and the host stays down`);
        return { reason: "crash-loop", crashes, launches, alert: crashAlertText({ crashes: recent, windowMs: crashLoop.windowMs, logDirs }) };
      }
      if (crashes > maxRestarts) {
        log(`${crashes} crashes: more than --max-restarts ${maxRestarts}; the watch ends and the host stays down`);
        return { reason: "max-crashes", crashes, launches };
      }
    }
    if (!d.crash && onRestart) {
      try {
        onRestart(d);
      } catch (e) {
        log(`restart backup failed: ${e.message}; starting the host anyway`);
      }
    }
    log(`restarting the host (${d.why})`);
    if (restartDelayMs) sleep(restartDelayMs);
  }
}

// The host's pid from host.json, or 0.
export function hostPid(dir) {
  const s = readHostState(dir);
  return s?.pid && pidAlive(s.pid) ? s.pid : 0;
}
