// tools/host/remote-schedule.mjs - a scheduled off-site copy of the 0grind saves (#1725).
//
// 0grind's own backups skip world saves, so `host remote 0grind saves pull` is the only copy. This
// registers a Windows scheduled task on the operator's PC that runs the pull every N hours, hidden on
// the private LF-Agents desktop (tools/lib/quiet-launch), and prunes the `-remote` pulls beyond the
// newest N so the copy cannot fill the drive. The command is `saves pull --schedule` in
// tools/host/commands/remote.mjs.
//
// The task runs `node tools/host/remote-schedule.mjs --run --every H --keep N`, a short command
// (schtasks caps /TR at 261 characters); that entry starts the pull hidden. Every schtasks call goes
// through an injectable runner with a timeout; the selftest never reaches the real Task Scheduler.
import { existsSync, readdirSync, statSync } from "node:fs";
import { win32, join } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";
import { quietSpawnDetached } from "../lib/quiet-launch.mjs";
import { safeRemoveTree } from "../link-safety.mjs";
import { SCHTASKS_TIMEOUT_MS, currentUser, parseQuery, realRunner } from "./autostart.mjs";

export { currentUser, parseQuery, realRunner };
export const TASK_NAME = "LifeFramework 0grind Saves Pull";
export const DEFAULT_EVERY_HOURS = 6;
export const DEFAULT_KEEP = 20;
export const MAX_EVERY_HOURS = 23; // schtasks /SC HOURLY /MO takes 1..23

// A plain pull is <yyyy-mm-dd_hh-mm-ss>-remote[-<n>] (savesPull, freshDir). Anything else under backups/
// (a `-remote-keep` tag, a local start/stop/crash/wipe backup, a .partial folder) is never pruned.
const PLAIN_REMOTE = /^\d{4}-\d{2}-\d{2}_\d{2}-\d{2}-\d{2}-remote(-\d+)?$/;

export function isPlainRemotePull(name) {
  return PLAIN_REMOTE.test(name);
}

export function scriptPath(root) {
  return win32.join(root, "tools", "host", "remote-schedule.mjs");
}

export function taskCommand({ root, nodePath, everyHours = DEFAULT_EVERY_HOURS, keep = DEFAULT_KEEP }) {
  return `"${nodePath}" "${scriptPath(root)}" --run --every ${everyHours} --keep ${keep}`;
}

// The exact `schtasks /Create` arguments: every N hours for this user, no elevation, /F so a changed
// interval or a moved checkout replaces the old task.
export function createArgs({ root, nodePath, user, everyHours = DEFAULT_EVERY_HOURS, keep = DEFAULT_KEEP }) {
  return [
    "/Create", "/TN", TASK_NAME, "/TR", taskCommand({ root, nodePath, everyHours, keep }),
    "/SC", "HOURLY", "/MO", String(everyHours), "/RU", user, "/RL", "LIMITED", "/F",
  ];
}

export const queryArgs = () => ["/Query", "/TN", TASK_NAME, "/FO", "CSV", "/V", "/NH"];
export const deleteArgs = () => ["/Delete", "/TN", TASK_NAME, "/F"];

// A whole number in [min, max] from a flag value, or null.
export function wholeNumber(text, min, max) {
  if (!/^\d+$/.test(String(text ?? ""))) return null;
  const n = Number(text);
  return n >= min && n <= max ? n : null;
}

function failure(what, r) {
  if (r.error) {
    const timedOut = r.error.code === "ETIMEDOUT" || /ETIMEDOUT|timed out/i.test(r.error.message);
    return timedOut ? `${what}: schtasks timed out after ${SCHTASKS_TIMEOUT_MS / 1000}s` : `${what}: ${r.error.message}`;
  }
  const text = `${r.stderr}${r.stdout}`.trim().split(/\r?\n/).slice(-3).join(" ");
  return `${what}: schtasks exited ${r.status}${text ? ` (${text})` : ""}`;
}

// Is the task registered? Exit 1 with nothing on stdout means an unknown name (the message is localized).
export function queryTask(run) {
  const r = run(queryArgs(), { timeoutMs: SCHTASKS_TIMEOUT_MS });
  if (r.error) return { error: failure("query", r) };
  if (r.status === 0) return { info: parseQuery(r.stdout) };
  if (r.status === 1 && !String(r.stdout ?? "").trim()) return { info: { registered: false } };
  return { error: failure("query", r) };
}

// Register the task. { ok, changed, message }: a task already running the same command is left alone.
export function scheduleTask({ run, root, nodePath, user, everyHours = DEFAULT_EVERY_HOURS, keep = DEFAULT_KEEP }) {
  const q = queryTask(run);
  if (q.error) return { ok: false, message: q.error };
  const want = taskCommand({ root, nodePath, everyHours, keep });
  if (q.info.registered && q.info.command === want) {
    return { ok: true, changed: false, message: `already scheduled: every ${everyHours} h, keeps the newest ${keep} -remote pulls` };
  }
  const r = run(createArgs({ root, nodePath, user, everyHours, keep }), { timeoutMs: SCHTASKS_TIMEOUT_MS });
  if (r.error || r.status !== 0) return { ok: false, message: failure("create", r) };
  const how = q.info.registered ? "updated" : "scheduled";
  return { ok: true, changed: true, message: `${how}: saves pull every ${everyHours} h as ${user}, hidden; keeps the newest ${keep} -remote pulls (tagged ones and local backups are never pruned)` };
}

export function unscheduleTask({ run }) {
  const q = queryTask(run);
  if (q.error) return { ok: false, message: q.error };
  if (!q.info.registered) return { ok: true, changed: false, message: "not scheduled, nothing to remove" };
  const r = run(deleteArgs(), { timeoutMs: SCHTASKS_TIMEOUT_MS });
  if (r.error || r.status !== 0) return { ok: false, message: failure("delete", r) };
  return { ok: true, changed: true, message: "unscheduled" };
}

// The interval and keep count a registered task's /TR carries (null for a task made some other way).
export function readSchedule(command) {
  const m = /--every (\d+) --keep (\d+)\s*$/.exec(String(command ?? ""));
  return m ? { everyHours: Number(m[1]), keep: Number(m[2]) } : null;
}

// What a prune would delete: the plain `-remote` pulls beyond the newest `keep`. Read-only.
export function planRemotePrune(backups, keep = DEFAULT_KEEP) {
  if (!Number.isInteger(keep) || keep < 1) throw new Error(`--keep must be a whole number >= 1 (got ${keep})`);
  if (!existsSync(backups)) return { keep: [], delete: [] };
  const plain = readdirSync(backups)
    .filter((n) => isPlainRemotePull(n) && statSync(join(backups, n)).isDirectory())
    .sort((a, b) => a.slice(0, 19).localeCompare(b.slice(0, 19)) || suffixOf(a) - suffixOf(b));
  const cut = Math.max(0, plain.length - keep);
  return { keep: plain.slice(cut), delete: plain.slice(0, cut) };
}

// <stamp>-remote is the first pull of that second, <stamp>-remote-2 the second one (freshDir).
function suffixOf(name) {
  const m = /-remote-(\d+)$/.exec(name);
  return m ? Number(m[1]) : 1;
}

// Deletes the planned folders through safeRemoveTree. Returns { deleted, kept }.
export function pruneRemotePulls(backups, keep = DEFAULT_KEEP, { dryRun = false } = {}) {
  const plan = planRemotePrune(backups, keep);
  if (!dryRun) for (const n of plan.delete) safeRemoveTree(join(backups, n));
  return { deleted: plan.delete, kept: plan.keep };
}

// What the scheduled task runs: the pull plus the prune, hidden on the LF-Agents desktop. Returns its pid.
export function runScheduled({ root, nodePath = process.execPath, keep = DEFAULT_KEEP, spawn = quietSpawnDetached }) {
  return spawn(nodePath, [win32.join(root, "tools", "cli.mjs"), "host", "remote", "0grind", "saves", "pull", "--prune", "--keep", String(keep)], { cwd: root });
}

// Entry for the scheduled task: `node tools/host/remote-schedule.mjs --run --every H --keep N`.
if (process.argv[1] && process.argv.includes("--run") && pathToFileURL(process.argv[1]).href === import.meta.url) {
  const root = win32.resolve(fileURLToPath(import.meta.url), "..", "..", "..");
  const ki = process.argv.indexOf("--keep");
  const keep = wholeNumber(ki >= 0 ? process.argv[ki + 1] : "", 1, 1000) ?? DEFAULT_KEEP;
  try {
    const pid = runScheduled({ root, keep });
    console.log(`[LFDebug:Host0grind] scheduled saves pull started hidden (pid ${pid}, keeps ${keep})`);
  } catch (e) {
    console.log(`[LFDebug:Host0grind] could not start the scheduled saves pull: ${e.message}`);
    process.exit(1);
  }
}
