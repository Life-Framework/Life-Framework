// Host history (#1409) behind `tools\cli host history [--json]`: one row per past session, rebuilt
// from what the host already wrote. Nothing is recorded by `host start` or `host stop`; this reads:
//   backups/<stamp>-<reason>   the backup folders (start, stop, crash, manual, preflight-..., ...)
//   backups/wipes.log          one row per wipe (#1274)
//   perf/perf-*.csv            the perf samples (#1406); the `players` column when it is filled
// and writes nothing.
//
// A session opens at a `start` backup and closes at the next `stop` or `crash` backup (a `start`
// while one is open means a scheduled restart closed it: it ends there). Every other backup belongs
// to the session it was taken in, or to the next session (the preflight before a start), or to the
// last one. A wipe belongs to the session that ended before it. Stamps are local time, like the
// folder names.
import { existsSync, readdirSync, readFileSync, statSync } from "node:fs";
import { join } from "node:path";
import { hostPaths } from "./host.mjs";

const STAMP = /^(\d{4})-(\d\d)-(\d\d)_(\d\d)-(\d\d)-(\d\d)-(.+)$/;
// The reasons freshDir may suffix with -2, -3 when two backups share a second.
const PLAIN = new Set(["start", "stop", "crash", "wipe", "manual"]);

const p2 = (n) => String(n).padStart(2, "0");
export function display(d) {
  return `${d.getFullYear()}-${p2(d.getMonth() + 1)}-${p2(d.getDate())} ${p2(d.getHours())}:${p2(d.getMinutes())}:${p2(d.getSeconds())}`;
}

// { name, at (Date), reason } for a backup folder name, or null when it is not one.
export function parseBackupName(name) {
  const m = STAMP.exec(name);
  if (!m) return null;
  const [y, mo, d, h, mi, s] = m.slice(1, 7).map(Number);
  let reason = m[7];
  const twin = /^(.*)-\d+$/.exec(reason);
  if (twin && PLAIN.has(twin[1])) reason = twin[1];
  return { name, at: new Date(y, mo - 1, d, h, mi, s), reason };
}

// Rows of backups/wipes.log: { at (Date), backup, aside }. A line that does not parse is skipped.
export function parseWipesLog(text) {
  const out = [];
  for (const line of text.split(/\r?\n/)) {
    const m = /^(\S+) wipe .*\bbackup=(\S+)(?:.*\baside=(\S+))?/.exec(line);
    if (!m) continue;
    const at = new Date(m[1]);
    if (Number.isNaN(at.getTime())) continue;
    out.push({ at, backup: m[2], aside: m[3] ?? null });
  }
  return out;
}

// [{ t (ms), players (number|null) }] from one perf CSV. A file without a players column or a
// line with a bad time gives no row; a blank players cell gives null.
export function parsePerfCsv(text) {
  const lines = text.split(/\r?\n/).filter(Boolean);
  if (!lines.length) return [];
  const head = lines[0].split(",").map((s) => s.trim());
  const ti = head.indexOf("time");
  const pi = head.indexOf("players");
  if (ti < 0 || pi < 0) return [];
  const out = [];
  for (const line of lines.slice(1)) {
    const cells = line.split(",");
    const t = Date.parse(cells[ti]);
    const raw = (cells[pi] ?? "").trim();
    const n = raw === "" ? NaN : Number(raw);
    if (Number.isNaN(t)) continue;
    out.push({ t, players: Number.isFinite(n) ? n : null });
  }
  return out;
}

// Pure: sessions from the three record sets. `backups` are folder names; `wipes` as parseWipesLog;
// `perf` as parsePerfCsv rows. Wipe folders (`wipe`, `wiped-saves`) are not session backups.
export function deriveHistory({ backups = [], wipes = [], perf = [] }) {
  const events = [];
  for (const name of backups) {
    const b = parseBackupName(name);
    if (b && b.reason !== "wipe" && b.reason !== "wiped-saves") events.push({ ...b, kind: "backup" });
  }
  for (const w of wipes) events.push({ kind: "wipe", at: w.at, name: w.backup });
  events.sort((a, b) => a.at - b.at || (a.name < b.name ? -1 : a.name > b.name ? 1 : 0));

  const lastStart = events.filter((e) => e.kind === "backup" && e.reason === "start").map((e) => e.at.getTime());
  const sessions = [];
  let open = null;
  let pending = []; // backups taken between sessions, for the next one
  const fresh = (start) => ({ start, end: null, ended: "open", crashes: 0, backups: [], wipes: [] });
  const close = (end, how, crashed) => {
    open.end = end;
    open.ended = how;
    if (crashed) open.crashes++;
    sessions.push(open);
    open = null;
  };

  for (const e of events) {
    if (e.kind === "wipe") {
      if (!sessions.length) sessions.push({ ...fresh(null), ended: "none" });
      sessions[sessions.length - 1].wipes.push({ at: e.at, backup: e.name });
    } else if (e.reason === "start") {
      if (open) close(e.at, "restart", false);
      open = fresh(e.at);
      open.backups.push(...pending, e.name);
      pending = [];
    } else if (e.reason === "stop" || e.reason === "crash") {
      if (!open) {
        open = fresh(null);
        open.backups.push(...pending);
        pending = [];
      }
      open.backups.push(e.name);
      close(e.at, e.reason, e.reason === "crash");
    } else if (open) {
      open.backups.push(e.name);
    } else if (sessions.length && !lastStart.some((t) => t > e.at.getTime())) {
      sessions[sessions.length - 1].backups.push(e.name);
    } else {
      pending.push(e.name);
    }
  }
  if (open) sessions.push(open);

  return sessions.map((s) => {
    const inside = perf.filter((r) => r.players !== null && s.start && r.t >= s.start.getTime() && (s.end ? r.t <= s.end.getTime() : true));
    return {
      start: s.start ? display(s.start) : null,
      end: s.end ? display(s.end) : null,
      minutes: s.start && s.end ? Math.round((s.end - s.start) / 60000) : null,
      ended: s.ended,
      crashes: s.crashes,
      backups: s.backups,
      wipes: s.wipes.map((w) => ({ at: display(w.at), backup: w.backup })),
      peakPlayers: inside.length ? Math.max(...inside.map((r) => r.players)) : null,
    };
  });
}

// Reads the host dir and returns { sessions }. Writes nothing; a missing folder is just empty.
export function hostHistory(dir) {
  const { backups } = hostPaths(dir);
  const names = existsSync(backups)
    ? readdirSync(backups).filter((n) => statSync(join(backups, n)).isDirectory())
    : [];
  const logFile = join(backups, "wipes.log");
  const wipes = existsSync(logFile) ? parseWipesLog(readFileSync(logFile, "utf8")) : [];
  const perfDir = join(dir, "perf");
  const perf = [];
  if (existsSync(perfDir)) {
    for (const f of readdirSync(perfDir).filter((n) => /^perf-.*\.csv$/.test(n)).sort()) {
      perf.push(...parsePerfCsv(readFileSync(join(perfDir, f), "utf8")));
    }
  }
  return { sessions: deriveHistory({ backups: names, wipes, perf }) };
}

// The plain-text table for `host history`.
export function historyText(sessions) {
  if (!sessions.length) return "[LFDebug:HostHistory] no sessions: no start/stop/crash backups under this host dir";
  const lines = ["start                end                  min   ended    crashes  peak  backups  wipe"];
  for (const s of sessions) {
    lines.push([
      (s.start ?? "?").padEnd(20),
      (s.end ?? "-").padEnd(20),
      String(s.minutes ?? "-").padStart(4),
      s.ended.padEnd(8),
      String(s.crashes).padStart(7),
      String(s.peakPlayers ?? "-").padStart(5),
      String(s.backups.length).padStart(8),
      s.wipes.length ? s.wipes.map((w) => w.at).join(", ") : "-",
    ].join("  "));
  }
  return lines.join("\n");
}
