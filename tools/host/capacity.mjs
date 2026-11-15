// Host capacity estimate (#1499) behind `tools\cli host capacity`: how many players does this box carry?
//
// Inputs are two things the host already writes: the perf CSV of `host perf` (#1406; header
// `time,cpu_pct,ws_mb,up_mbit,players`, CPU as a percent of ONE core, upload in Mbit/s) and the game's own
// `[LFDebug:Perf] players=N fps=F frameMs=M entities=E` lines from the server log (#1452's format, one a minute).
// The player count comes from, in order: the `--players` override, a filled `players` CSV column, the
// log's perf lines that had at least one player. The model is linear: CPU and upload per player are the
// mean of the run divided by that player count, and the estimate is the lower of what CPU (80% of one
// core, `CPU_BUDGET_PCT`) and upload (`--upload-mbit`) allow. It ignores the idle baseline, so it
// errs low for a run at few players and is best read from a run near a full session. Pure; the command
// (tools/host/commands/capacity.mjs) reads the files. Selftest: tools/validation/host-capacity-selftest.mjs.
import { CPU_BUDGET_PCT, DEFAULT_UPLOAD_MBIT } from "./perf.mjs";

export const TARGET_PLAYERS = 16;
export { CPU_BUDGET_PCT, DEFAULT_UPLOAD_MBIT };

const mean = (xs) => xs.reduce((s, x) => s + x, 0) / xs.length;

// [{ time, cpuPct, wsMb, upMbit, players|null }] from a `host perf` CSV. Rows that do not parse are dropped.
export function parseCsv(text) {
  const lines = text.split(/\r?\n/).filter((l) => l.trim() !== "");
  if (!lines.length) return [];
  const head = lines[0].split(",").map((h) => h.trim());
  const col = (name) => head.indexOf(name);
  const [iT, iC, iW, iU, iP] = ["time", "cpu_pct", "ws_mb", "up_mbit", "players"].map(col);
  if (iC < 0 || iU < 0) return [];
  const rows = [];
  for (const line of lines.slice(1)) {
    const f = line.split(",");
    const cpuPct = Number(f[iC]);
    const upMbit = Number(f[iU]);
    if (f[iC] === "" || f[iU] === "" || !Number.isFinite(cpuPct) || !Number.isFinite(upMbit)) continue;
    const p = iP >= 0 && (f[iP] ?? "").trim() !== "" ? Number(f[iP]) : null;
    rows.push({ time: f[iT] ?? "", cpuPct, wsMb: iW >= 0 ? Number(f[iW]) : 0, upMbit, players: Number.isFinite(p) ? p : null });
  }
  return rows;
}

const PERF_LINE = /\[LFDebug:Perf\]\s+players=(\d+)\s+fps=([\d.]+)\s+frameMs=([\d.]+)\s+entities=(\d+)/;

// [{ players, fps, frameMs, entities }] for each `[LFDebug:Perf] players=... entities=...` line.
export function parsePerfLines(text) {
  const out = [];
  for (const line of text.split(/\r?\n/)) {
    const m = PERF_LINE.exec(line);
    if (m) out.push({ players: Number(m[1]), fps: Number(m[2]), frameMs: Number(m[3]), entities: Number(m[4]) });
  }
  return out;
}

// The estimate, or { error } that starts with "no data". opts.players overrides the player count. Pure.
export function estimate(rows, samples, budget, opts = {}) {
  const cpuBudget = budget.cpuPct ?? CPU_BUDGET_PCT;
  const upBudget = budget.uploadMbit ?? DEFAULT_UPLOAD_MBIT;
  if (!rows.length) return { error: "no data: the perf CSV has no rows (run `host perf` while players are connected)" };
  const loaded = samples.filter((s) => s.players > 0);
  const colPlayers = rows.map((r) => r.players).filter((p) => p !== null && p > 0);
  let players;
  if (opts.players !== undefined) players = opts.players;
  else if (colPlayers.length) players = mean(colPlayers);
  else if (loaded.length) players = mean(loaded.map((s) => s.players));
  else {
    const idle = samples.length ? ` (${samples.length} idle line(s))` : "";
    return { error: `no data: no [LFDebug:Perf] players=N line with players above 0 in the log${idle}; the server logs one a minute with -lfPerf, or pass --players N` };
  }
  if (!(players > 0)) return { error: "no data: the player count is 0" };
  const cpuAvg = mean(rows.map((r) => r.cpuPct));
  const upAvg = mean(rows.map((r) => r.upMbit));
  const cpuPerPlayer = cpuAvg / players;
  const upPerPlayer = upAvg / players;
  const cpuEstimate = cpuPerPlayer > 0 ? Math.floor(cpuBudget / cpuPerPlayer) : Infinity;
  const upEstimate = upPerPlayer > 0 ? Math.floor(upBudget / upPerPlayer) : Infinity;
  return {
    players, cpuAvg, upAvg, cpuPerPlayer, upPerPlayer, cpuEstimate, upEstimate,
    estimate: Math.min(cpuEstimate, upEstimate),
    limit: upEstimate < cpuEstimate ? "upload" : "cpu",
    rows: rows.length, perfLines: samples.length,
    fpsAvg: loaded.length ? mean(loaded.map((s) => s.fps)) : 0,
    frameMsMax: loaded.length ? Math.max(...loaded.map((s) => s.frameMs)) : 0,
    entitiesMax: samples.length ? Math.max(...samples.map((s) => s.entities)) : 0,
  };
}

const fmtN = (n) => (Number.isFinite(n) ? String(n) : "unbounded");

// The greppable verdict line for an estimate.
export function summaryLine(e, budget) {
  const cpuBudget = budget.cpuPct ?? CPU_BUDGET_PCT;
  const upBudget = budget.uploadMbit ?? DEFAULT_UPLOAD_MBIT;
  const target = budget.targetPlayers ?? TARGET_PLAYERS;
  const fits = e.estimate >= target ? `the ${target}-player budget fits` : `the ${target}-player budget is NOT met`;
  const cpuWhy = `${cpuBudget}% of one core at ${e.cpuPerPlayer.toFixed(2)}% per player`;
  const upWhy = `${upBudget} Mbit/s at ${e.upPerPlayer.toFixed(3)} Mbit/s per player`;
  const other = e.limit === "cpu"
    ? `upload would carry ${fmtN(e.upEstimate)} at ${e.upPerPlayer.toFixed(3)} Mbit/s per player of ${upBudget} Mbit/s`
    : `cpu would carry ${fmtN(e.cpuEstimate)} at ${e.cpuPerPlayer.toFixed(2)}% per player of ${cpuBudget}%`;
  const why = e.limit === "cpu" ? cpuWhy : upWhy;
  return `[LFDebug:HostCapacity] estimate ${fmtN(e.estimate)} players (${e.limit} is the limit: ${why}; ${other}); ${fits}`;
}

// The measured lines above the verdict.
export function measureLines(e) {
  const out = [
    `[LFDebug:HostCapacity] ${Math.round(e.players * 10) / 10} players: cpu ${e.cpuAvg.toFixed(1)}% of one core (${e.cpuPerPlayer.toFixed(2)}% per player), `
      + `upload ${e.upAvg.toFixed(2)} Mbit/s (${e.upPerPlayer.toFixed(3)} Mbit/s per player), from ${e.rows} CSV rows`,
  ];
  if (e.perfLines) out.push(`[LFDebug:HostCapacity] game view: fps avg ${e.fpsAvg.toFixed(1)}, frame max ${e.frameMsMax.toFixed(1)} ms, entities max ${e.entitiesMax}, from ${e.perfLines} perf lines`);
  return out;
}
