// Host perf sampler (#1406) behind `tools\cli host perf`: is this PC able to carry a full session?
//
// The server process is sampled every N seconds for at most M seconds. A raw sample is
// { t (epoch ms), cpuSec (cumulative CPU seconds of the server), wsMb (working set), sentBytes
// (cumulative bytes sent) }. Rows are the change between two samples: CPU as a percent of ONE core,
// working set, upload in Mbit/s. The verdict compares the mean CPU and upload with the 40-player
// budget (CPU under 80% of one core, upload under the configured Mbit/s) and names the peak too.
// The sampler, the clock and the sleep are injected, so the selftest
// (tools/validation/host-perf-selftest.mjs) reads no process.
//
// Windows only: one bounded PowerShell call reads the process and the adapters. Windows keeps no
// per-process network counter, so `sentBytes` is the machine's adapter total; run it with the
// server as the only busy network user. A later column may add `players` from A2S (#1297); it is
// left empty here.
import { spawnSync } from "node:child_process";

export const DEFAULT_EVERY_SEC = 30;
export const DEFAULT_FOR_SEC = 120;
export const CPU_BUDGET_PCT = 80;
export const DEFAULT_UPLOAD_MBIT = 20;
export const CSV_HEADER = "time,cpu_pct,ws_mb,up_mbit,players";

// { everySec, forSec, uploadMbit } from the words after `host perf`, or { error }. Pure.
export function parseArgs(args, env = process.env) {
  const out = { everySec: DEFAULT_EVERY_SEC, forSec: DEFAULT_FOR_SEC, uploadMbit: DEFAULT_UPLOAD_MBIT };
  const envMbit = env.LF_HOST_UPLOAD_MBIT;
  if (envMbit !== undefined && envMbit !== "") out.uploadMbit = Number(envMbit);
  const flags = { "--every": "everySec", "--for": "forSec", "--upload-mbit": "uploadMbit" };
  for (let i = 0; i < args.length; i++) {
    const key = flags[args[i]];
    if (!key) continue;
    out[key] = Number(args[++i]);
  }
  for (const [flag, key] of Object.entries(flags)) {
    if (!Number.isFinite(out[key]) || out[key] <= 0) return { error: `${flag} must be a number above 0` };
  }
  return out;
}

// Rows from consecutive raw samples. A step with no time is skipped; a counter that went backwards
// (the server restarted) gives 0 for that counter, never a negative or NaN. Pure.
export function deriveRows(samples) {
  const rows = [];
  for (let i = 1; i < samples.length; i++) {
    const a = samples[i - 1];
    const b = samples[i];
    const dt = (b.t - a.t) / 1000;
    if (!(dt > 0)) continue;
    const cpu = Math.max(0, b.cpuSec - a.cpuSec);
    const sent = Math.max(0, b.sentBytes - a.sentBytes);
    rows.push({ t: b.t, cpuPct: (cpu / dt) * 100, wsMb: b.wsMb, upMbit: (sent * 8) / dt / 1e6, spanSec: dt });
  }
  return rows;
}

export function toCsv(rows) {
  const lines = rows.map((r) => `${new Date(r.t).toISOString()},${r.cpuPct.toFixed(1)},${Math.round(r.wsMb)},${r.upMbit.toFixed(2)},`);
  return `${CSV_HEADER}\n${lines.join("\n")}${lines.length ? "\n" : ""}`;
}

const mean = (xs) => xs.reduce((s, x) => s + x, 0) / xs.length;

// { ok, over: ["cpu"|"upload"], cpuAvg, cpuPeak, upAvg, upPeak, wsPeak, samples, seconds }. Pure.
export function verdict(rows, budget) {
  const cpus = rows.map((r) => r.cpuPct);
  const ups = rows.map((r) => r.upMbit);
  const v = {
    cpuAvg: mean(cpus), cpuPeak: Math.max(...cpus),
    upAvg: mean(ups), upPeak: Math.max(...ups),
    wsPeak: Math.max(...rows.map((r) => r.wsMb)),
    samples: rows.length,
    seconds: Math.round(rows.reduce((s, r) => s + r.spanSec, 0)),
    over: [],
  };
  if (v.cpuAvg >= budget.cpuPct) v.over.push("cpu");
  if (v.upAvg >= budget.uploadMbit) v.over.push("upload");
  v.ok = v.over.length === 0;
  return v;
}

export function verdictLine(rows, budget) {
  if (!rows.length) return "[LFDebug:HostPerf] verdict none: no samples were taken (is the server running?)";
  const v = verdict(rows, budget);
  const head = v.ok ? "ok" : `OVER BUDGET (${v.over.join(", ")})`;
  return `[LFDebug:HostPerf] verdict ${head}: cpu avg ${v.cpuAvg.toFixed(1)}% (peak ${v.cpuPeak.toFixed(1)}%) of ${budget.cpuPct}% budget, `
    + `upload avg ${v.upAvg.toFixed(2)} Mbit/s (peak ${v.upPeak.toFixed(2)}) of ${budget.uploadMbit} Mbit/s budget, `
    + `working set peak ${Math.round(v.wsPeak)} MB, ${v.samples} samples over ${v.seconds} s`;
}

// Raw samples: one at the start, then one every `everySec` while the next still fits in `forSec`.
// A sampler that returns null (the server is gone) ends the run. The loop is bounded by forSec.
export function collect({ sample, sleep, now, everySec, forSec }) {
  const out = [];
  const start = now();
  for (;;) {
    const s = sample();
    if (!s) break;
    out.push(s);
    if (now() - start + everySec * 1000 > forSec * 1000) break;
    sleep(everySec * 1000);
  }
  return out;
}

function psScript(pid) {
  return `$ErrorActionPreference='Stop';$p=Get-Process -Id ${Number(pid)};`
    + "$n=(Get-NetAdapterStatistics | Measure-Object -Property SentBytes -Sum).Sum;"
    + "[Console]::Out.Write(('{0}|{1}|{2}' -f $p.CPU.ToString([cultureinfo]::InvariantCulture),$p.WorkingSet64,$n))";
}

// The real sampler for `pid` (Windows): one bounded PowerShell call. Returns null when the process
// is gone or the counters cannot be read; `onError(msg)` says why.
export function windowsSampler(pid, onError = () => {}) {
  return () => {
    const r = spawnSync("powershell.exe", ["-NoProfile", "-NonInteractive", "-Command", psScript(pid)], {
      encoding: "utf8", timeout: 15000, windowsHide: true,
    });
    const [cpu, ws, sent] = (r.stdout ?? "").trim().split("|").map(Number);
    if (r.status !== 0 || ![cpu, ws, sent].every(Number.isFinite)) {
      onError(`could not read pid ${pid}: ${(r.stderr || r.stdout || `exit ${r.status}`).toString().trim().split(/\r?\n/)[0]}`);
      return null;
    }
    return { t: Date.now(), cpuSec: cpu, wsMb: ws / 1048576, sentBytes: sent };
  };
}
