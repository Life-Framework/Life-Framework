// Server cost benchmark report (#335). `tools\cli perf` runs LF_Test_PerfBudget (128 synthetic
// characters through the real managers, bus and serializer), reads its `[LFDebug:Perf] BENCH`
// lines from the newest test log, prints a table against the budget in
// addons/LifeFramework/Configs/Perf/LF_PerfBudget.conf, and writes tmp/perf/report.{md,json}.
// The exit code is the test's: a cost over its budget fails the run.
import { existsSync, mkdirSync, readFileSync, readdirSync, statSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { spawnSync } from "node:child_process";

const BENCH = /\[LFDebug:Perf\] BENCH (\S+) value=(-?\d+) budget=(none|-?\d+) unit=(\S+)/;
const COUNT = /\[LFDebug:Perf\] count (\S+) (-?\d+)/;
const TIME = /\[LFDebug:Perf\] time (\S+) calls=(\d+) totalMs=(\d+) maxMs=(\d+)/;
const GAUGE = /\[LFDebug:Perf\] gauge (\S+) (-?\d+)/;

export function parseBench(text) {
  const rows = new Map();
  for (const line of text.split("\n")) {
    const m = BENCH.exec(line);
    if (m) rows.set(m[1], { name: m[1], value: Number(m[2]), budget: m[3] === "none" ? null : Number(m[3]), unit: m[4] });
  }
  return [...rows.values()];
}

export function parseCounters(text) {
  const out = { count: {}, time: {}, gauge: {} };
  for (const line of text.split("\n")) {
    let m;
    if ((m = COUNT.exec(line))) out.count[m[1]] = Number(m[2]);
    else if ((m = TIME.exec(line))) out.time[m[1]] = { calls: Number(m[2]), totalMs: Number(m[3]), maxMs: Number(m[4]) };
    else if ((m = GAUGE.exec(line))) out.gauge[m[1]] = Number(m[2]);
  }
  return out;
}

export function overBudget(rows) {
  return rows.filter((r) => r.budget !== null && r.value > r.budget);
}

export function renderMarkdown(rows, counters, meta = {}) {
  const lines = [];
  lines.push(`Benchmark: ${meta.players ?? "?"} synthetic characters, ${meta.when ?? new Date().toISOString()}`);
  lines.push("");
  lines.push("| Measure | Value | Budget | Headroom | Status |");
  lines.push("|---|---:|---:|---:|---|");
  for (const r of rows) {
    const budget = r.budget === null ? "-" : `${r.budget} ${r.unit}`;
    const headroom = r.budget === null || r.value === 0 ? "-" : `${(r.budget / r.value).toFixed(1)}x`;
    const status = r.budget === null ? "info" : r.value <= r.budget ? "ok" : "OVER";
    lines.push(`| ${r.name} | ${r.value} ${r.unit} | ${budget} | ${headroom} | ${status} |`);
  }
  const timed = Object.entries(counters.time);
  if (timed.length) {
    lines.push("", "| Timer | Calls | Total ms | Max ms |", "|---|---:|---:|---:|");
    for (const [k, v] of timed) lines.push(`| ${k} | ${v.calls} | ${v.totalMs} | ${v.maxMs} |`);
  }
  return lines.join("\n") + "\n";
}

function newestLog(root) {
  const dir = join(root, "server", "profile", "test", "logs");
  if (!existsSync(dir)) return null;
  let best = null;
  for (const e of readdirSync(dir, { withFileTypes: true })) {
    if (!e.isDirectory()) continue;
    const f = join(dir, e.name, "lfdebug-lines.txt");
    if (!existsSync(f)) continue;
    const m = statSync(f).mtimeMs;
    if (!best || m > best.m) best = { f, m };
  }
  return best ? best.f : null;
}

export function runPerf(root, flags = []) {
  const logIdx = flags.indexOf("--log");
  let code = 0;
  let logPath = logIdx >= 0 ? flags[logIdx + 1] : null;
  if (!logPath) {
    const res = spawnSync(process.execPath, [join(root, "tools", "cli.mjs"), "test", "--no-build", "--only", "LF_Test_PerfBudget", ...flags], { cwd: root, stdio: "inherit" });
    code = res.status ?? 1;
    logPath = newestLog(root);
  }
  if (!logPath || !existsSync(logPath)) {
    console.log("perf: no test log found to read (a remote run keeps its log on the box)");
    return code || 1;
  }
  const text = readFileSync(logPath, "utf8");
  const rows = parseBench(text);
  if (!rows.length) {
    console.log(`perf: no BENCH lines in ${logPath}; the test did not reach its measures`);
    return code || 1;
  }
  const counters = parseCounters(text);
  const md = renderMarkdown(rows, counters, { players: 128 });
  const outDir = join(root, "tmp", "perf");
  mkdirSync(outDir, { recursive: true });
  writeFileSync(join(outDir, "report.md"), md);
  writeFileSync(join(outDir, "report.json"), JSON.stringify({ rows, counters }, null, 1) + "\n");
  console.log(md);
  const over = overBudget(rows);
  for (const r of over) console.log(`perf: OVER BUDGET ${r.name} ${r.value} ${r.unit} > ${r.budget}`);
  console.log(`perf: report in ${join(outDir, "report.md")}`);
  return over.length ? 1 : code;
}

export function selftestPerf() {
  const failures = [];
  const sample = [
    "02:42:29.118 SCRIPT       : [LFDebug:Perf] BENCH crime-event-us value=20 budget=100 unit=us/event",
    "02:42:29.321 SCRIPT       : [LFDebug:Perf] BENCH ledger-rows value=5120 budget=none unit=rows",
    "02:42:29.322 SCRIPT       : [LFDebug:Perf] BENCH save-write-ms value=900 budget=300 unit=ms",
    "[LFDebug:Perf] count Account.publish 384",
    "[LFDebug:Perf] time Ledger.record calls=7120 totalMs=89 maxMs=1",
    "[LFDebug:Perf] gauge Ledger.rows 20000",
  ].join("\n");
  const rows = parseBench(sample);
  if (rows.length !== 3) failures.push(`parseBench found ${rows.length} rows, expected 3`);
  if (!rows[1] || rows[1].budget !== null) failures.push("a budget of none must parse as null");
  const over = overBudget(rows);
  if (over.length !== 1 || over[0].name !== "save-write-ms") failures.push("overBudget must flag exactly save-write-ms");
  const c = parseCounters(sample);
  if (c.count["Account.publish"] !== 384 || !c.time["Ledger.record"] || c.time["Ledger.record"].calls !== 7120 || c.gauge["Ledger.rows"] !== 20000) failures.push("parseCounters lost a value");
  if (!renderMarkdown(rows, c).includes("| save-write-ms | 900 ms | 300 ms | 0.3x | OVER |")) failures.push("renderMarkdown row for an over-budget measure is wrong");
  return failures;
}
