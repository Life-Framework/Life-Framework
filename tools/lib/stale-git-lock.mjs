// Self-heal for a git lock file a killed git left behind (2026-11-27: the main guard's stage 2 failed "after 3
// tries" on `.git/worktrees/<tree>/index.lock: File exists`, 11 min old, no git process holding it; the guard sat
// at "green none yet" until the lead deleted the file by hand).
//
// How a lock gets orphaned: git creates index.lock / HEAD.lock with O_EXCL, holds it for the length of the
// command and renames or deletes it at the end. A git ended in the middle (taskkill /T on a worker or a step
// that timed out, a crash, a reboot) never reaches that line. The guard's run tree lost its worker at 13:11 during
// the pre-move e2e; the lock was 9 to 11 minutes old at 13:14.
//
// `healStaleLock(file)` removes ONE named lock and only when it is provably abandoned:
//   - the basename is on the list (index.lock, HEAD.lock), never a ref or packed-refs lock;
//   - its mtime is at least `minAgeMs` old (default 5 min; a real index write takes seconds);
//   - no git process that could have created it is alive: a git started at or before the lock's mtime might hold
//     it, so it blocks the removal (a git started after the lock appeared is the victim, not the holder). The
//     long-lived fsmonitor daemon is ignored. An unreadable process list blocks the removal (fail safe).
// `runGitHealing(run, dirOf)` wraps a git call: when it fails with "Unable to create '<lock>': File exists" and the
// lock is stale, the lock is removed, one line is logged and the call is run once more.
//
// No imports beyond node: wt.mjs and main-guard.mjs both load this.
import { spawnSync } from "node:child_process";
import { existsSync, statSync, unlinkSync } from "node:fs";
import { basename } from "node:path";

export const STALE_LOCK_NAMES = ["index.lock", "HEAD.lock"];
export const DEFAULT_MIN_AGE_MS = 5 * 60 * 1000;
const LOCK_ERROR = /Unable to create '([^']+\.lock)': File exists/;

export const staleLockAgeMs = () => {
  const n = Number(process.env.LF_GIT_STALE_LOCK_MIN);
  return Number.isFinite(n) && n > 0 ? n * 60 * 1000 : DEFAULT_MIN_AGE_MS;
};

// [{ pid, startedMs | null, cmd }] for every running git; null when the list could not be read.
export function listGitProcesses() {
  try {
    if (process.platform === "win32") {
      const ps = "Get-CimInstance Win32_Process -Filter \"Name like 'git%'\" | ForEach-Object { '{0}|{1}|{2}' -f $_.ProcessId, ([DateTimeOffset]$_.CreationDate).ToUnixTimeMilliseconds(), $_.CommandLine }";
      const r = spawnSync("powershell.exe", ["-NoProfile", "-NonInteractive", "-Command", ps], { encoding: "utf8", windowsHide: true, timeout: 20000 });
      if (r.status !== 0 || r.error) return null;
      return r.stdout.split(/\r?\n/).filter(Boolean).map((l) => {
        const [pid, ms, ...cmd] = l.split("|");
        const started = Number(ms);
        return { pid: Number(pid), startedMs: Number.isFinite(started) && started > 0 ? started : null, cmd: cmd.join("|") };
      });
    }
    const r = spawnSync("ps", ["-A", "-o", "pid=,etimes=,args="], { encoding: "utf8", timeout: 20000 });
    if (r.status !== 0 || r.error) return null;
    const now = Date.now();
    const out = [];
    for (const l of r.stdout.split("\n")) {
      const m = l.trim().match(/^(\d+)\s+(\d+)\s+(.*)$/);
      if (m && /(^|[\\/])git(-[\w-]+)?(\.exe)?(\s|$)/.test(m[3])) out.push({ pid: Number(m[1]), startedMs: now - Number(m[2]) * 1000, cmd: m[3] });
    }
    return out;
  } catch {
    return null;
  }
}

// Why the lock must stay, or "" when it is abandoned. `procs` is injectable for the selftest.
// Selftest hook: replaces the process lister for wt.git / main-guard calls (null restores the real one).
let lister = listGitProcesses;
export const setProcessLister = (fn) => { lister = fn ?? listGitProcesses; };

export function lockHolderReason(file, { minAgeMs = staleLockAgeMs(), procs = () => lister(), now = Date.now() } = {}) {
  if (!STALE_LOCK_NAMES.includes(basename(file))) return `${basename(file)} is not a lock this heals`;
  let mtime;
  try {
    mtime = statSync(file).mtimeMs;
  } catch {
    return "gone";
  }
  const age = now - mtime;
  if (age < minAgeMs) return `only ${Math.round(age / 1000)} s old`;
  const list = typeof procs === "function" ? procs() : procs;
  if (!list) return "the git process list could not be read";
  const holders = list.filter((p) => !/fsmonitor--daemon/.test(p.cmd ?? "") && (p.startedMs == null || p.startedMs <= mtime + 2000));
  if (holders.length) return `git pid ${holders.map((p) => p.pid).join(",")} was already running when it was written`;
  return "";
}

// Removes the lock when abandoned. Returns { removed, ageMin, reason }.
export function healStaleLock(file, opts = {}) {
  const reason = lockHolderReason(file, opts);
  if (reason) return { removed: false, reason };
  const now = opts.now ?? Date.now();
  let ageMin = 0;
  try {
    ageMin = Math.round((now - statSync(file).mtimeMs) / 60000);
    unlinkSync(file);
  } catch (e) {
    return { removed: false, reason: `could not delete: ${e.message}` };
  }
  return { removed: true, ageMin, reason: "" };
}

// The lock file a failed git call complained about, or "".
export function lockFileFromError(res) {
  const m = LOCK_ERROR.exec(String(res?.stderr ?? "") + "\n" + String(res?.stdout ?? ""));
  return m ? m[1].replace(/\//g, process.platform === "win32" ? "\\" : "/") : "";
}

// run() is a git call returning a spawnSync-style result. A "lock exists" failure on a stale lock removes it
// (one log(line) per removal; the file is gone afterwards, so it is never logged twice) and runs the call again
// once; any other result is returned as it is.
export function runGitHealing(run, { log = () => {}, ...opts } = {}) {
  const res = run();
  if (res.status === 0) return res;
  const file = lockFileFromError(res);
  if (!file || !existsSync(file)) return res;
  const h = healStaleLock(file, opts);
  if (!h.removed) return res;
  log(`git lock self-heal: removed stale ${file} (${h.ageMin} min old, no git process could hold it); retrying`);
  return run();
}
