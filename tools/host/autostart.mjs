// tools/host/autostart.mjs - bring the PC host back after a reboot (#1504).
//
// `host watch` covers a crashed server, not a rebooted PC. This registers a Windows scheduled
// task that runs at logon for the host user and starts `host watch --flagship` hidden on the
// private LF-Agents desktop (tools/lib/quiet-launch). The command is tools/host/commands/autostart.mjs.
//
// The task runs `node tools/host/autostart.mjs --run`, a short command, because schtasks caps /TR
// at 261 characters. Every schtasks call goes through an injectable runner with a timeout.
import { spawnSync } from "node:child_process";
import { win32 } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";
import { quietSpawnDetached } from "../lib/quiet-launch.mjs";

export const TASK_NAME = "LifeFramework Host Autostart";
export const SCHTASKS_TIMEOUT_MS = 30000;
// Give the desktop, network and Steam half a minute after logon before the host starts.
const LOGON_DELAY = "0000:30";

// Column order of `schtasks /Query /FO CSV /V /NH`. Fixed by Windows and the same in every
// language, which the LIST labels ("Last Run Time:") are not.
const COL = { status: 3, lastRun: 5, lastResult: 6, command: 8, user: 14 };
const NEVER_RESULT = 267011; // 0x41303: the task has not yet run
const NEVER_YEAR = /(^|[^0-9])1999([^0-9]|$)/; // an unrun task reports 30/11/1999

export function scriptPath(root) {
  return win32.join(root, "tools", "host", "autostart.mjs");
}

export function taskCommand({ root, nodePath }) {
  return `"${nodePath}" "${scriptPath(root)}" --run`;
}

export function currentUser() {
  const name = process.env.USERNAME || "";
  const domain = process.env.USERDOMAIN || "";
  return domain && name ? `${domain}\\${name}` : name;
}

// The exact `schtasks /Create` arguments: at logon of this user, no elevation, /F so a stale
// task from a moved checkout is replaced (install only gets here when it must).
export function createArgs({ root, nodePath, user }) {
  return [
    "/Create", "/TN", TASK_NAME, "/TR", taskCommand({ root, nodePath }),
    "/SC", "ONLOGON", "/RU", user, "/RL", "LIMITED", "/DELAY", LOGON_DELAY, "/F",
  ];
}

export function queryArgs() {
  return ["/Query", "/TN", TASK_NAME, "/FO", "CSV", "/V", "/NH"];
}

export function deleteArgs() {
  return ["/Delete", "/TN", TASK_NAME, "/F"];
}

// Real runner: { status, stdout, stderr, error }. error is set on a spawn failure or a timeout.
export function realRunner(args, { timeoutMs = SCHTASKS_TIMEOUT_MS } = {}) {
  const r = spawnSync("schtasks.exe", args, { encoding: "utf8", timeout: timeoutMs, windowsHide: true });
  return { status: r.status, stdout: r.stdout ?? "", stderr: r.stderr ?? "", error: r.error };
}

// One CSV record into fields: double quotes, "" inside a field is a quote.
function csvFields(line) {
  const out = [];
  let cur = "";
  let q = false;
  for (let i = 0; i < line.length; i++) {
    const c = line[i];
    if (q) {
      if (c === '"' && line[i + 1] === '"') { cur += '"'; i++; }
      else if (c === '"') q = false;
      else cur += c;
    } else if (c === '"') q = true;
    else if (c === ",") { out.push(cur); cur = ""; }
    else cur += c;
  }
  out.push(cur);
  return out;
}

// The text of `/Query /FO CSV /V /NH` into what status prints. Empty text is "not registered".
export function parseQuery(text) {
  const line = String(text ?? "").split(/\r?\n/).find((l) => l.trim().startsWith('"'));
  if (!line) return { registered: false };
  const f = csvFields(line);
  const lastResult = Number.parseInt(f[COL.lastResult], 10);
  const lastRun = f[COL.lastRun] ?? "";
  const ran = !NEVER_YEAR.test(lastRun) && lastResult !== NEVER_RESULT && lastRun !== "N/A" && lastRun !== "";
  return {
    registered: true,
    status: f[COL.status] ?? "",
    lastRun,
    lastResult: Number.isNaN(lastResult) ? null : lastResult,
    ran,
    command: f[COL.command] ?? "",
    user: f[COL.user] ?? "",
  };
}

function failure(what, r) {
  if (r.error) {
    const timedOut = r.error.code === "ETIMEDOUT" || /ETIMEDOUT|timed out/i.test(r.error.message);
    return timedOut ? `${what}: schtasks timed out after ${SCHTASKS_TIMEOUT_MS / 1000}s` : `${what}: ${r.error.message}`;
  }
  const text = `${r.stderr}${r.stdout}`.trim().split(/\r?\n/).slice(-3).join(" ");
  return `${what}: schtasks exited ${r.status}${text ? ` (${text})` : ""}`;
}

// Is the task registered? schtasks exits 1 with nothing on stdout when the name is unknown (the
// message is localized, so only the exit code and the empty output are read).
export function queryTask(run) {
  const r = run(queryArgs(), { timeoutMs: SCHTASKS_TIMEOUT_MS });
  if (r.error) return { error: failure("query", r) };
  if (r.status === 0) return { info: parseQuery(r.stdout) };
  if (r.status === 1 && !String(r.stdout ?? "").trim()) return { info: { registered: false } };
  return { error: failure("query", r) };
}

// Register the task. { ok, changed, message }: a task already running the same command is left alone.
export function installTask({ run, root, nodePath, user }) {
  const q = queryTask(run);
  if (q.error) return { ok: false, message: q.error };
  const want = taskCommand({ root, nodePath });
  if (q.info.registered && q.info.command === want) {
    return { ok: true, changed: false, message: `already installed (runs at logon as ${q.info.user || user})` };
  }
  const r = run(createArgs({ root, nodePath, user }), { timeoutMs: SCHTASKS_TIMEOUT_MS });
  if (r.error || r.status !== 0) return { ok: false, message: failure("create", r) };
  const how = q.info.registered ? "updated (the checkout moved)" : "installed";
  return { ok: true, changed: true, message: `${how}: \`host watch --flagship\` starts at logon of ${user}, hidden` };
}

export function removeTask({ run }) {
  const q = queryTask(run);
  if (q.error) return { ok: false, message: q.error };
  if (!q.info.registered) return { ok: true, changed: false, message: "not installed, nothing to remove" };
  const r = run(deleteArgs(), { timeoutMs: SCHTASKS_TIMEOUT_MS });
  if (r.error || r.status !== 0) return { ok: false, message: failure("delete", r) };
  return { ok: true, changed: true, message: "removed" };
}

// What the logon task runs: `host watch --flagship` on the hidden LF-Agents desktop. Returns its pid.
export function runAtLogon({ root, nodePath = process.execPath, spawn = quietSpawnDetached }) {
  return spawn(nodePath, [win32.join(root, "tools", "cli.mjs"), "host", "watch", "--flagship"], { cwd: root });
}

// Entry for the scheduled task: `node tools/host/autostart.mjs --run`.
if (process.argv[1] && process.argv.includes("--run") && pathToFileURL(process.argv[1]).href === import.meta.url) {
  const root = win32.resolve(fileURLToPath(import.meta.url), "..", "..", "..");
  try {
    const pid = runAtLogon({ root });
    console.log(`[LFDebug:HostAutostart] host watch started hidden (pid ${pid})`);
  } catch (e) {
    console.log(`[LFDebug:HostAutostart] could not start host watch: ${e.message}`);
    process.exit(1);
  }
}
