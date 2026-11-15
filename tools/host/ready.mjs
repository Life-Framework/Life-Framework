// `host ready` (#1408): one go / no-go before a session. Calls the existing pieces and reimplements none:
// the build gate (tools/host/host.mjs buildGate), the staff roster check (tools/commands/staff.mjs), the
// newest backup's age, the last restore drill's result (tools/host/drill.mjs drillVerdict), the disk
// report (tools/host/disk.mjs) and the live server.json's name and password (#1559). Every check is { name, run(ctx) -> { ok, detail } } so a test injects its own;
// a check that throws is red with the message. Read only: nothing is started, copied or deleted.

import { existsSync, readdirSync, readFileSync, statSync } from "node:fs";
import { join } from "node:path";
import { buildGate, exeBuild, findSavePoints, hostPaths, newestSaveBuild } from "./host.mjs";
import { diskReport, fmtBytes, FLOOR_BYTES } from "./disk.mjs";
import { DRILL_RESULT_FILE, drillVerdict, readDrillResult } from "./drill.mjs";
import { localSetting } from "../lib/local-config.mjs";
import { SERVER_NAME } from "./announce.mjs";
import { ownerConfigPath, serverJsonPath } from "./config-apply.mjs";

export const MAX_BACKUP_HOURS = 12; // the flagship restarts every 12 h; a backup older than a restart gap is stale
export const MAX_DRILL_DAYS = 7;

const hoursLabel = (h) => `${h < 10 ? h.toFixed(1) : Math.round(h)} h`;

// "2026-11-15_10-00-00-reason" -> Date (local time), or null
function stampDate(name) {
  const m = /^(\d{4})-(\d{2})-(\d{2})_(\d{2})-(\d{2})-(\d{2})/.exec(name);
  return m ? new Date(+m[1], +m[2] - 1, +m[3], +m[4], +m[5], +m[6]) : null;
}

// The build gate: the newest save must have been written by the installed game build, else `host preflight`
// comes first. An unknown build is not a block (buildGate says "changed: false"), only a note.
export async function buildCheck({ hostDir, root }) {
  const { profile } = hostPaths(hostDir);
  if (!findSavePoints(profile).length) return { ok: true, detail: "no save yet; a fresh world needs no preflight" };
  const configured = localSetting(root, "ENFUSION_SERVER_PATH") || "C:/Program Files (x86)/Steam/steamapps/common/Arma Reforger Server";
  const exe = /\.exe$/i.test(configured) ? configured : join(configured, "ArmaReforgerServer.exe");
  const gate = buildGate({ saveBuild: newestSaveBuild(profile), gameBuild: exeBuild(exe) });
  if (gate.changed) return { ok: false, detail: `${gate.reason}; run \`tools\\cli host preflight\` first` };
  return { ok: true, detail: gate.reason };
}

export async function rosterCheck({ root, rosterFile }) {
  const { parseRoster, checkRoster } = await import("../commands/staff.mjs");
  const file = rosterFile ?? join(root, "server", "flagship", "staff.txt");
  if (!existsSync(file)) return { ok: false, detail: `roster not found: ${file}` };
  const entries = parseRoster(readFileSync(file, "utf8"));
  const { errors, warnings } = checkRoster(entries);
  if (errors.length) return { ok: false, detail: `${errors.length} roster error(s): ${errors[0]}` };
  const w = warnings.length ? `, ${warnings.length} warning(s)` : "";
  return { ok: true, detail: `${entries.length} entr${entries.length === 1 ? "y" : "ies"}${w}` };
}

// The newest backup folder is named by its stamp; the stamp is its age.
export async function backupCheck({ hostDir, now = new Date(), maxBackupHours = MAX_BACKUP_HOURS }) {
  const { backups } = hostPaths(hostDir);
  const names = existsSync(backups) ? readdirSync(backups).filter((n) => statSync(join(backups, n)).isDirectory()).sort() : [];
  if (!names.length) return { ok: false, detail: "no backup; run `tools\\cli host backup`" };
  const newest = names[names.length - 1];
  const when = stampDate(newest) ?? statSync(join(backups, newest)).mtime;
  const hours = (now.getTime() - when.getTime()) / 3600e3;
  if (!findSavePoints(join(backups, newest)).length) return { ok: false, detail: `newest backup ${newest} holds no save point` };
  if (hours > maxBackupHours) return { ok: false, detail: `newest backup is ${hoursLabel(hours)} old (limit ${maxBackupHours} h): ${newest}; run \`tools\\cli host backup\`` };
  return { ok: true, detail: `${newest}, ${hoursLabel(Math.max(hours, 0))} old` };
}

// The last result is <host dir>/drill-last.json, which `host drill` writes after every run, including one on a
// pool machine (#1558). With no readable file, `host drill`'s local console log tmp/logs/host-drill-<stamp>.log is
// the fallback: the newest one, judged by the drill's own verdict. No file and no log counts as no drill.
export async function drillCheck({ root, hostDir, now = new Date(), maxDrillDays = MAX_DRILL_DAYS }) {
  const last = readDrillResult(hostDir);
  if (last) {
    if (!last.ok) return { ok: false, detail: `the last restore drill failed: ${last.reason || "no reason recorded"} (${DRILL_RESULT_FILE}${last.machine ? `, ${last.machine}` : ""})` };
    const at = Date.parse(last.at ?? "");
    if (!Number.isFinite(at)) return { ok: false, detail: `${DRILL_RESULT_FILE} has no valid time; run \`tools\\cli host drill\`` };
    const days = (now.getTime() - at) / 86400e3;
    if (days > maxDrillDays) return { ok: false, detail: `the last passing restore drill is ${Math.round(days)} days old (limit ${maxDrillDays}); run \`tools\\cli host drill\`` };
    return { ok: true, detail: `passed, ${last.accounts} account(s), ${hoursLabel(Math.max(days * 24, 0))} ago${last.machine ? ` on ${last.machine}` : ""}` };
  }
  const dir = join(root, "tmp", "logs");
  const logs = existsSync(dir) ? readdirSync(dir).filter((n) => /^host-drill-.*\.log$/.test(n)) : [];
  if (!logs.length) return { ok: false, detail: "no restore drill on record; run `tools\\cli host drill`" };
  const newest = logs.map((n) => ({ n, t: statSync(join(dir, n)).mtimeMs })).sort((a, b) => a.t - b.t || (a.n < b.n ? -1 : 1)).pop();
  const v = drillVerdict(readFileSync(join(dir, newest.n), "utf8"));
  const days = (now.getTime() - newest.t) / 86400e3;
  if (!v.ok) return { ok: false, detail: `the last restore drill failed: ${v.reason} (${newest.n})` };
  if (days > maxDrillDays) return { ok: false, detail: `the last passing restore drill is ${Math.round(days)} days old (limit ${maxDrillDays}); run \`tools\\cli host drill\`` };
  return { ok: true, detail: `passed, ${v.accounts} account(s), ${hoursLabel(Math.max(days * 24, 0))} ago` };
}

export async function diskCheck({ hostDir, freeBytes }) {
  const r = diskReport(hostDir, { freeBytes });
  if (r.low) return { ok: false, detail: `${fmtBytes(r.freeBytes)} free, under ${fmtBytes(FLOOR_BYTES)}; free space or run \`tools\\cli host prune\`` };
  const fits = Number.isFinite(r.sessionsFit) ? `${r.sessionsFit} sessions fit` : "unbounded sessions fit";
  return { ok: true, detail: `${fmtBytes(r.freeBytes)} free, ${fits}` };
}

// The live profile's server.json (what `host config apply` wrote, which the game reads): the alpha gate is a
// game password and nothing else, and the listing name is the flagship's exact name (runbook step 4). The
// password is only tested for being non-empty; no detail ever carries it.
export async function configCheck({ hostDir }) {
  const file = serverJsonPath(hostDir);
  if (!existsSync(file)) return { ok: false, detail: `no live server.json at ${file}; run \`tools\\cli host config apply\`` };
  let game;
  try {
    const json = JSON.parse(readFileSync(file, "utf8"));
    game = json && typeof json === "object" && !Array.isArray(json) ? json.game : undefined;
  } catch {
    return { ok: false, detail: `the live server.json is not valid JSON (${file}); run \`tools\\cli host config apply\`` };
  }
  const problems = [];
  if (typeof game?.name !== "string" || game.name !== SERVER_NAME) problems.push(`server name is not exactly "${SERVER_NAME}"`);
  if (typeof game?.password !== "string" || !game.password.trim()) problems.push("game password is empty (the alpha gate is a password; run `tools\\cli host rotate`)");
  if (problems.length) return { ok: false, detail: problems.join("; ") };
  return { ok: true, detail: `name "${SERVER_NAME}", password set` };
}

// The live owner config (<host>/profile/profile/LifeFramework/config.json, what `host config apply` wrote): the
// maintainer's decisions of #1405, the same values tools/validation/flagship-decisions-selftest.mjs pins in the
// committed file. Two checks, so the owner sees which one is red: the day cycle, and the restart schedule.
export const DAY_CYCLE_MODE = "accelerated";
export const DAY_LENGTH_MINUTES = 120;
export const RESTART_TIMES = ["06:00", "18:00"];
const RESTART_KEYS = ["times", "warningMinutes"]; // restarts warn with the toast only: no siren or other key

function readOwnerConfig(hostDir) {
  const file = ownerConfigPath(hostDir);
  if (!existsSync(file)) return { bad: `no live owner config at ${file}; run \`tools\\cli host config apply\`` };
  try {
    const json = JSON.parse(readFileSync(file, "utf8"));
    if (!json || typeof json !== "object" || Array.isArray(json)) return { bad: `the live owner config is not a JSON object (${file}); run \`tools\\cli host config apply\`` };
    return { json };
  } catch {
    return { bad: `the live owner config is not valid JSON (${file}); run \`tools\\cli host config apply\`` };
  }
}

export async function dayCycleCheck({ hostDir }) {
  const { json, bad } = readOwnerConfig(hostDir);
  if (bad) return { ok: false, detail: bad };
  const day = json.dayCycle;
  const problems = [];
  if (day?.mode !== DAY_CYCLE_MODE) problems.push(`dayCycle.mode must be "${DAY_CYCLE_MODE}"`);
  if (day?.dayLengthMinutes !== DAY_LENGTH_MINUTES) problems.push(`dayCycle.dayLengthMinutes must be ${DAY_LENGTH_MINUTES} (a 2 hour day)`);
  if (problems.length) return { ok: false, detail: `${problems.join("; ")}; run \`tools\\cli host config apply\`` };
  return { ok: true, detail: `${DAY_CYCLE_MODE}, ${DAY_LENGTH_MINUTES}-minute day` };
}

export async function restartsCheck({ hostDir }) {
  const { json, bad } = readOwnerConfig(hostDir);
  if (bad) return { ok: false, detail: bad };
  const r = json.restarts;
  const problems = [];
  const times = r?.times;
  if (!Array.isArray(times) || times.length !== RESTART_TIMES.length || RESTART_TIMES.some((t, i) => times[i] !== t)) problems.push(`restarts.times must be exactly ${RESTART_TIMES.join(" and ")}`);
  const extra = r && typeof r === "object" ? Object.keys(r).filter((k) => !RESTART_KEYS.includes(k)) : [];
  if (extra.length) problems.push(`restarts.${extra.join(", restarts.")}: restarts warn with the toast only (no siren)`);
  if (problems.length) return { ok: false, detail: `${problems.join("; ")}; run \`tools\\cli host config apply\`` };
  return { ok: true, detail: `${RESTART_TIMES.join(" and ")}, toast only` };
}

export function defaultChecks() {
  return [
    { name: "build", run: buildCheck },
    { name: "roster", run: rosterCheck },
    { name: "backup", run: backupCheck },
    { name: "drill", run: drillCheck },
    { name: "disk", run: diskCheck },
    { name: "config", run: configCheck },
    { name: "day cycle", run: dayCycleCheck },
    { name: "restarts", run: restartsCheck },
  ];
}

// Runs every check (never short-circuits: the owner sees all reds at once).
// -> { go, results: [{ name, ok, detail }], failed: [name] }
export async function runChecks(checks, ctx) {
  const results = [];
  for (const c of checks) {
    let r;
    try {
      r = await c.run(ctx);
    } catch (e) {
      r = { ok: false, detail: `check threw: ${e.message}` };
    }
    results.push({ name: c.name, ok: !!r?.ok, detail: String(r?.detail ?? "") });
  }
  const failed = results.filter((r) => !r.ok).map((r) => r.name);
  return { go: failed.length === 0, results, failed };
}
