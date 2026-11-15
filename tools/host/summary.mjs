// Post-session report (`tools\cli host summary`, #1501): one markdown page per session that says how
// long it ran, who came, whether it restarted or crashed, which log lines are new, and whether the PC
// held up. It is the runbook's "after" step and the evidence for the roadmap's "sessions at the player
// target". It calls the other host tools' exported functions (attendance.mjs, perf.mjs,
// tools/lib/log-triage.mjs); it parses no one else's CLI text. Pure text in, text out: the command
// (commands/summary.mjs) does the file reads and the one write, and the selftest
// (tools/validation/host-summary-selftest.mjs) reads no host.
import { existsSync, readdirSync } from "node:fs";
import { join } from "node:path";
import { parseAttendance } from "./attendance.mjs";
import { CPU_BUDGET_PCT, DEFAULT_UPLOAD_MBIT, verdict, verdictLine } from "./perf.mjs";
import { triageLog } from "../lib/log-triage.mjs";

export const DEFAULT_TARGET = 40;

// A stamp starts a log line, as in attendance.mjs (never preceded by a digit or colon).
const STAMP = /(?<![\d:])(\d{2}):(\d{2}):(\d{2})\.(\d{3})(?!\d)/g;
const READY = /Entered online game state\./;
// The lines the mod prints just before it closes (the same two tools/host/watchdog.mjs reads).
const LOG_STOP = /\[LFDebug:Host\] stop requested/;
const LOG_RESTART = /\[LFDebug:Ops\] restart due: wrote/;
// A host log folder is logs_<stamp>; a crash backup is backups/<stamp>-crash (host.mjs stamp()).
const STAMP_NAME = /(\d{4}-\d{2}-\d{2}_\d{2}-\d{2}-\d{2})/;

const p2 = (n) => String(n).padStart(2, "0");
const clock = (sec) => `${p2(Math.floor((sec % 86400) / 3600))}:${p2(Math.floor((sec % 3600) / 60))}:${p2(Math.floor(sec % 60))}`;

// Seconds between the first and last time stamp; a clock that runs backwards is a new day.
export function logLength(text) {
  let first = null;
  let lastSec = -1;
  let day = 0;
  let now = 0;
  for (const m of text.matchAll(STAMP)) {
    const sec = Number(m[1]) * 3600 + Number(m[2]) * 60 + Number(m[3]) + Number(m[4]) / 1000;
    if (lastSec >= 0 && sec < lastSec - 1) day++;
    lastSec = sec;
    now = day * 86400 + sec;
    if (first === null) first = now;
  }
  if (first === null) return { seconds: 0, start: null, end: null };
  return { seconds: Math.round(now - first), start: clock(first), end: clock(now) };
}

// The `<stamp>` inside a path such as .../logs_2026-11-15_18-00-00/console.log, or null.
export function stampFromPath(path) {
  const parts = String(path).replace(/\\/g, "/").split("/");
  for (let i = parts.length - 1; i >= 0; i--) {
    if (!parts[i].startsWith("logs_")) continue;
    const m = STAMP_NAME.exec(parts[i]);
    if (m) return m[1];
  }
  return null;
}

// Crash backup folders (`<stamp>-crash`) whose stamp falls in [fromStamp, toStamp]. Stamps sort as text.
export function crashFolders(backupsDir, fromStamp, toStamp) {
  if (!existsSync(backupsDir)) return [];
  return readdirSync(backupsDir)
    .filter((d) => d.endsWith("-crash"))
    .filter((d) => {
      const m = STAMP_NAME.exec(d);
      return m && m[1] >= fromStamp && m[1] <= toStamp;
    })
    .sort();
}

// `<host>/perf/perf-<stamp>.csv` rows (tools/host/perf.mjs toCsv) back into verdict() rows. The span of
// a row is its distance from the row before; the first row takes the second's (the sampler's step).
export function parsePerfCsv(text) {
  const lines = text.split(/\r?\n/).filter((l) => l.trim() && !l.startsWith("time,"));
  const raw = lines.map((l) => l.split(",")).map((c) => ({ t: Date.parse(c[0]), cpuPct: Number(c[1]), wsMb: Number(c[2]), upMbit: Number(c[3]) }));
  const good = raw.filter((r) => [r.t, r.cpuPct, r.wsMb, r.upMbit].every(Number.isFinite));
  return good.map((r, i) => {
    const prev = good[i - 1];
    const next = good[i + 1];
    const step = prev ? (r.t - prev.t) / 1000 : next ? (next.t - r.t) / 1000 : 0;
    return { ...r, spanSec: step > 0 ? step : 0 };
  });
}

function duration(sec) {
  const m = Math.round(sec / 60);
  return m >= 60 ? `${Math.floor(m / 60)} h ${m % 60} min` : `${m} min`;
}

// The whole report as an object. Inputs: logText; triageRows (parsed triage table); perfCsv (text or
// null); crash (array of crash folder names or null when the host folder was not checked); target.
export function summarize({ logText, logName = "console.log", triageRows = [], perfCsv = null, crash = null, target = DEFAULT_TARGET, uploadMbit = DEFAULT_UPLOAD_MBIT }) {
  const length = logLength(logText);
  const att = parseAttendance(logText);
  const lines = logText.split(/\r?\n/);
  const restarts = lines.filter((l) => LOG_RESTART.test(l)).length;
  const stops = lines.filter((l) => LOG_STOP.test(l)).length;
  const boots = lines.filter((l) => READY.test(l)).length;
  const tail = [...lines].reverse().find((l) => LOG_STOP.test(l) || LOG_RESTART.test(l));
  const ended = !tail ? "no close line" : LOG_STOP.test(tail) ? "host stop" : "scheduled restart";
  const { total, groups } = triageLog(logText, triageRows);
  const knownGroups = groups.filter((g) => g.known);
  const newGroups = groups.filter((g) => !g.known);
  let perf = null;
  if (perfCsv !== null) {
    const rows = parsePerfCsv(perfCsv);
    const budget = { cpuPct: CPU_BUDGET_PCT, uploadMbit };
    perf = rows.length ? { ok: verdict(rows, budget).ok, line: verdictLine(rows, budget) } : { ok: null, line: "the perf file has no samples" };
  }
  return {
    log: logName,
    length,
    attendance: att,
    target,
    targetMet: (att.peak?.count ?? 0) >= target,
    boots,
    restarts,
    stops,
    ended,
    crash,
    triage: { lines: total, groups: groups.length, known: knownGroups.length, new: newGroups.length, newGroups: newGroups.map((g) => ({ key: g.key, count: g.count })) },
    perf,
  };
}

// The markdown page. `generated` is the stamp the file is named by.
export function renderMarkdown(s, generated) {
  const a = s.attendance;
  const out = [`# Session report ${generated}`, ""];
  out.push(`- Log: ${s.log}`);
  out.push(s.length.start ? `- Length: ${duration(s.length.seconds)} (${s.length.start} to ${s.length.end})` : "- Length: unknown (the log has no time stamps)");
  out.push(`- Players: ${a.unique} unique, peak ${a.peak ? `${a.peak.count} at ${a.peak.at}` : "0"}, target ${s.target}: ${s.targetMet ? "reached" : "not reached"}`);
  out.push(`- Joins: ${a.joins} (${a.rejoins} rejoins), leaves ${a.leaves}, still on at the end ${a.stillConnected}`);
  out.push(`- Restarts: ${s.restarts} scheduled, ${s.stops} host stop${s.stops === 1 ? "" : "s"}, boots in this log ${s.boots}; the log ends with: ${s.ended}`);
  if (s.crash === null) out.push("- Crashes: not checked (pass the host's own console.log, under logs/logs_<stamp>/)");
  else out.push(`- Crashes: ${s.crash.length}${s.crash.length ? ` (${s.crash.join(", ")})` : ""}`);
  out.push(`- Triage: ${s.triage.groups} groups from ${s.triage.lines} lines, ${s.triage.known} known, ${s.triage.new} new`);
  for (const g of s.triage.newGroups.slice(0, 10)) out.push(`  - NEW x${g.count} ${g.key.length > 160 ? `${g.key.slice(0, 160)}...` : g.key}`);
  out.push(`- Perf: ${s.perf ? s.perf.line.replace(/^\[LFDebug:HostPerf\] /, "") : "no perf file (run `tools\\cli host perf` while the server is busy)"}`);
  out.push("", "## Players", "");
  for (const p of [...a.players].sort((x, y) => y.minutes - x.minutes)) out.push(`- ${p.names.join("/")}: ${p.minutes} min, ${p.sessions} session${p.sessions === 1 ? "" : "s"}`);
  if (!a.players.length) out.push("- nobody joined");
  out.push("");
  return out.join("\n");
}

export function newestPerfCsv(hostDir) {
  const dir = join(hostDir, "perf");
  if (!existsSync(dir)) return null;
  const files = readdirSync(dir).filter((f) => /^perf-.*\.csv$/.test(f)).sort();
  return files.length ? join(dir, files[files.length - 1]) : null;
}
