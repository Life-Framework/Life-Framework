// `tools\cli host perf` (#1406). The logic is tools/host/perf.mjs.
//
// Samples the running server's CPU, working set and upload and prints one verdict against the
// 16-player budget. The rows go to <host>/perf/perf-<stamp>.csv.
import { mkdirSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { hostPid } from "../watchdog.mjs";
import { collect, CPU_BUDGET_PCT, deriveRows, parseArgs, toCsv, verdictLine, windowsSampler } from "../perf.mjs";

export const help = "perf [--every 30] [--for 120] [--upload-mbit 20]   sample the running server CPU, memory and upload (Windows; upload is the machine's adapter total) into <host>/perf/perf-<stamp>.csv and print one verdict against the 16-player budget (CPU under 80% of one core, upload under --upload-mbit or env LF_HOST_UPLOAD_MBIT)";

function sleepMs(ms) {
  Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
}

export async function main(args, ctx) {
  const say = (m) => console.log(`[LFDebug:HostPerf] ${m}`);
  const opts = parseArgs(args);
  if (opts.error) {
    say(opts.error);
    return 1;
  }
  if (process.platform !== "win32") {
    say("host perf reads Windows counters; it only runs on Windows");
    return 1;
  }
  const pid = hostPid(ctx.hostDir);
  if (!pid) {
    say("no host is running (start it with `host start`, then run this)");
    return 1;
  }
  say(`sampling pid ${pid} every ${opts.everySec} s for ${opts.forSec} s`);
  const samples = collect({
    sample: windowsSampler(pid, say),
    sleep: sleepMs,
    now: Date.now,
    everySec: opts.everySec,
    forSec: opts.forSec,
  });
  const rows = deriveRows(samples);
  if (rows.length) {
    const dir = join(ctx.hostDir, "perf");
    mkdirSync(dir, { recursive: true });
    const file = join(dir, `perf-${new Date(samples[0].t).toISOString().replace(/[:.]/g, "-")}.csv`);
    writeFileSync(file, toCsv(rows));
    say(`wrote ${file}`);
  }
  const budget = { cpuPct: CPU_BUDGET_PCT, uploadMbit: opts.uploadMbit };
  console.log(verdictLine(rows, budget));
  return rows.length ? 0 : 1;
}
