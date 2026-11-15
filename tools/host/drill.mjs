// Restore drill (#1261, #306): the newest backup boots on a throwaway profile and its accounts load.
//
// `tools\cli host drill [<backup>]` (tools/host/commands/drill.mjs) copies a backup into a scratch host
// dir, boots it there with -loadSessionSave (`host start` with LF_HOST_DIR pointed at the scratch dir,
// so the live profile, its state file and its backups are never touched), stops it for the shutdown
// save, and judges the console log: `save loaded:` plus at least one applied account record.
// The judge reuses preflightVerdict (tools/host/host.mjs); the account line is the one
// tools/dedicated/session.mjs expects after a restart.
import { existsSync, mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { spawnSync } from "node:child_process";
import { join } from "node:path";
import { findSavePoints, hostPaths, newestConsoleLog, pickBackup, preflightVerdict, restoreBackup } from "./host.mjs";

const ACCOUNTS = /\[LFDebug:Persistence\] applied ([1-9]\d*) account record\(s\)/;

// Judges a drill boot's console.log: everything preflightVerdict demands (loaded, nothing damaged, no
// script error, shutdown save) and at least one account record applied, so a save that loaded
// empty is not a restore.
export function drillVerdict(text) {
  const base = preflightVerdict(text);
  if (!base.ok) return { ok: false, reason: base.reason, accounts: 0 };
  const m = ACCOUNTS.exec(String(text));
  if (!m) return { ok: false, reason: `${base.reason}; but no '[LFDebug:Persistence] applied N account record(s)' with N >= 1: the save loaded without its accounts`, accounts: 0 };
  return { ok: true, reason: base.reason, accounts: Number(m[1]) };
}

// One compact result line, the repo's SUMMARY form (AGENTS.md "Compact output").
export function summaryLine({ ok, backup, machine, accounts, log, reason }) {
  const parts = [`SUMMARY host-drill ${ok ? "ok" : "fail"}`, `backup=${backup ?? "none"}`, `machine=${machine ?? "local"}`];
  if (ok) parts.push(`accounts=${accounts ?? 0}`);
  else parts.push(`reason="${String(reason ?? "unknown").replace(/"/g, "'")}"`);
  if (log) parts.push(`log=${log}`);
  return parts.join(" ");
}

// The last result, kept beside the backups: <host dir>/drill-last.json = { ok, backup, machine, accounts, reason, at }.
// A drill that ran on a pool machine leaves its log there, so `host ready` (tools/host/ready.mjs) reads this file
// first and the log only as a fallback.
export const DRILL_RESULT_FILE = "drill-last.json";

export function drillResultPath(hostDir) {
  return join(hostDir, DRILL_RESULT_FILE);
}

// Writes the result file; returns its path. `at` defaults to now.
export function writeDrillResult(hostDir, { ok, backup, machine, accounts, reason, at }) {
  mkdirSync(hostDir, { recursive: true });
  const file = drillResultPath(hostDir);
  const doc = {
    ok: !!ok,
    backup: backup ?? null,
    machine: machine ?? "local",
    accounts: Number(accounts) || 0,
    reason: String(reason ?? ""),
    at: at ?? new Date().toISOString(),
  };
  writeFileSync(file, `${JSON.stringify(doc, null, 2)}\n`);
  return file;
}

// The result file, or null when it is missing, unreadable or holds no pass/fail.
export function readDrillResult(hostDir) {
  if (!hostDir) return null;
  const file = drillResultPath(hostDir);
  if (!existsSync(file)) return null;
  try {
    const j = JSON.parse(readFileSync(file, "utf8"));
    if (!j || typeof j !== "object" || typeof j.ok !== "boolean") return null;
    return { ok: j.ok, backup: j.backup ?? null, machine: j.machine ?? null, accounts: Number(j.accounts) || 0, reason: String(j.reason ?? ""), at: j.at ?? null };
  } catch {
    return null;
  }
}

// The inverse of summaryLine: the last `SUMMARY host-drill` line in a pool machine's output, or null.
export function parseSummaryLine(text) {
  const lines = String(text ?? "").split(/\r?\n/).filter((l) => /\bSUMMARY host-drill (ok|fail)\b/.test(l));
  const line = lines[lines.length - 1];
  if (!line) return null;
  const ok = /\bSUMMARY host-drill ok\b/.test(line);
  const field = (k) => new RegExp(`\\b${k}=(\\S+)`).exec(line)?.[1] ?? null;
  const reason = /\breason="(.*?)"(?: log=|$)/.exec(line)?.[1] ?? "";
  const acc = field("accounts");
  return { ok, backup: field("backup"), machine: field("machine"), accounts: acc ? Number(acc) : 0, reason, log: field("log") };
}

// The named backup of the host dir, else its newest; null when there is none.
export function pickDrillBackup(hostDir, name) {
  return pickBackup(hostDir, name);
}

// Copies the backup into a fresh scratch host dir (<scratchDir>/profile/profile/.save) and returns scratchDir.
// Throws when the backup holds no save point. Nothing of the live host dir is read or written.
export function prepareScratch(backup, scratchDir) {
  if (!backup || !existsSync(backup)) throw new Error(`backup not found: ${backup}`);
  if (!findSavePoints(backup).length) throw new Error(`backup holds no save point: ${backup}`);
  mkdirSync(hostPaths(scratchDir).profile, { recursive: true });
  restoreBackup(scratchDir, backup);
  return scratchDir;
}

// Runs `host <args>` in a child on the scratch host dir. The child is bounded by timeoutMs.
function hostCli(root, scratchDir, args, timeoutMs) {
  const r = spawnSync(process.execPath, [join(root, "tools", "cli.mjs"), "host", ...args], {
    cwd: root, encoding: "utf8", timeout: timeoutMs, windowsHide: true,
    env: { ...process.env, LF_HOST_DIR: scratchDir, LF_QUIET_CHILD: "1" },
  });
  return { code: r.status ?? 1, out: `${r.stdout ?? ""}${r.stderr ?? ""}` };
}

// Boots the scratch profile on this machine, stops it, judges the log. Returns { ok, reason, accounts, log }.
export function runDrillHere({ root, scratchDir, timeoutSec = 300, log = console.log }) {
  const started = hostCli(root, scratchDir, ["start", "--force", "--timeout", String(timeoutSec)], (timeoutSec + 120) * 1000);
  const tail = started.out.trim().split(/\r?\n/).slice(-6).join("\n  ");
  const consoleLog = newestConsoleLog(hostPaths(scratchDir).profile);
  if (started.code !== 0) {
    // A server that came up but missed the ready line still needs closing; for a dead one stop is a no-op.
    hostCli(root, scratchDir, ["stop"], 240000);
    return { ok: false, reason: `the server did not start: ${tail}`, accounts: 0, log: consoleLog };
  }
  const stopped = hostCli(root, scratchDir, ["stop"], 240000);
  const text = consoleLog && existsSync(consoleLog) ? readFileSync(consoleLog, "utf8") : "";
  if (stopped.code !== 0) log("drill: the server did not stop cleanly; judging the log anyway");
  return { ...drillVerdict(text), log: consoleLog };
}

export const DRILL_ARCHIVE_ENV = "LF_DRILL_ARCHIVE";

function ps(script) {
  const r = spawnSync("powershell", ["-NoProfile", "-NonInteractive", "-Command", script], { encoding: "utf8", timeout: 300000, windowsHide: true });
  if (r.status !== 0) throw new Error((r.stderr || r.stdout || "powershell failed").trim());
}

const q = (s) => `'${String(s).replace(/'/g, "''")}'`;

// A zip of the scratch dir for a pool machine, and its extraction there.
export function zipDir(dir, zip) {
  ps(`Compress-Archive -Path ${q(join(dir, "*"))} -DestinationPath ${q(zip)} -Force`);
  return zip;
}

export function unzipTo(zip, dir) {
  mkdirSync(dir, { recursive: true });
  ps(`Expand-Archive -LiteralPath ${q(zip)} -DestinationPath ${q(dir)} -Force`);
  return dir;
}
