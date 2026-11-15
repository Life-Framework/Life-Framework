// `tools\cli host capacity` (#1499). The logic is tools/host/capacity.mjs.
//
// Reads <host>/perf/*.csv (`host perf`, #1406) and the server log's `[LFDebug:Perf] players=...` lines
// (#1452) and prints CPU and upload per player and the estimated player count before the 40-player
// budget is hit. Exit 0 = the budget fits, 1 = the estimate is under it, 2 = no data.
import { existsSync, readdirSync, readFileSync, statSync } from "node:fs";
import { join, resolve } from "node:path";
import { hostPaths, newestConsoleLog } from "../host.mjs";
import { CPU_BUDGET_PCT, DEFAULT_UPLOAD_MBIT, estimate, measureLines, parseCsv, parsePerfLines, summaryLine, TARGET_PLAYERS } from "../capacity.mjs";

export const help = "capacity [--csv <file|dir>] [--log <console.log>] [--players N] [--upload-mbit 20]   CPU and upload per player from <host>/perf/*.csv and the log's [LFDebug:Perf] players= lines, and the estimated player count before the 40-player budget (cpu under 80% of one core, upload under --upload-mbit or env LF_HOST_UPLOAD_MBIT); exit 0 fits, 1 under 40, 2 no data";

const VALUE_FLAGS = ["--csv", "--log", "--players", "--upload-mbit"];

// The perf CSV files at `path`: the file itself, or every *.csv in the folder, oldest first.
function csvFiles(path) {
  if (!existsSync(path)) return [];
  if (statSync(path).isFile()) return [path];
  return readdirSync(path).filter((f) => f.endsWith(".csv")).sort().map((f) => join(path, f));
}

export async function main(args, ctx) {
  const say = (m) => console.log(`[LFDebug:HostCapacity] ${m}`);
  const opts = {};
  for (const flag of VALUE_FLAGS) {
    const i = args.indexOf(flag);
    if (i >= 0) opts[flag] = args[i + 1];
  }
  const env = process.env.LF_HOST_UPLOAD_MBIT;
  const uploadMbit = Number(opts["--upload-mbit"] ?? (env ? env : DEFAULT_UPLOAD_MBIT));
  if (!(uploadMbit > 0)) {
    say("--upload-mbit must be a number above 0");
    return 1;
  }
  const players = opts["--players"] === undefined ? undefined : Number(opts["--players"]);
  if (players !== undefined && !(players > 0)) {
    say("--players must be a number above 0");
    return 1;
  }

  const csvPath = resolve(opts["--csv"] ?? join(ctx.hostDir, "perf"));
  const files = csvFiles(csvPath);
  const rows = files.flatMap((f) => parseCsv(readFileSync(f, "utf8")));
  const logFile = opts["--log"] ? resolve(opts["--log"]) : newestConsoleLog(hostPaths(ctx.hostDir).profile);
  const samples = logFile && existsSync(logFile) ? parsePerfLines(readFileSync(logFile, "utf8")) : [];

  const budget = { cpuPct: CPU_BUDGET_PCT, uploadMbit, targetPlayers: TARGET_PLAYERS };
  const e = estimate(rows, samples, budget, { players });
  if (e.error) {
    say(files.length ? e.error : `no data: no perf CSV at ${csvPath} (run \`host perf\` while players are connected)`);
    return 2;
  }
  for (const l of measureLines(e)) console.log(l);
  console.log(summaryLine(e, budget));
  return e.estimate >= TARGET_PLAYERS ? 0 : 1;
}
