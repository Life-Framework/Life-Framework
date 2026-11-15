// `tools\cli host attendance [<console.log>] [--json]`: reads a server console.log (default: the
// newest one of the host profile) and prints the session's unique players, the peak concurrent count
// and when it was, and the minutes each player stayed. `--json` prints one JSON document for
// `host history` and `host gate`. The parser is tools/host/attendance.mjs.
import { existsSync, readFileSync } from "node:fs";
import { resolve } from "node:path";
import { hostPaths, newestConsoleLog } from "../host.mjs";
import { parseAttendance } from "../attendance.mjs";

export const help =
  "attendance [<console.log>] [--json]   unique players, peak concurrent and when, minutes each stayed, read from the server log's joins and leaves (default: the newest host log)";

export async function main(args, ctx) {
  const positional = args.filter((a) => !a.startsWith("--"));
  const logFile = positional[0] ? resolve(positional[0]) : newestConsoleLog(hostPaths(ctx.hostDir).profile);
  if (!logFile || !existsSync(logFile)) {
    console.log(`[LFHost:attendance] no console.log to read${positional[0] ? ` at ${positional[0]}` : " (the host has not run; pass a path)"}`);
    return 1;
  }
  const r = parseAttendance(readFileSync(logFile, "utf8"));
  if (args.includes("--json")) {
    console.log(JSON.stringify({ log: logFile, ...r }, null, 2));
    return 0;
  }
  console.log(`[LFHost:attendance] ${logFile}`);
  console.log(`unique players:  ${r.unique}`);
  console.log(`peak concurrent: ${r.peak ? `${r.peak.count} at ${r.peak.at}` : "0"}`);
  console.log(`joins ${r.joins} (${r.rejoins} rejoins), leaves ${r.leaves}, still on at log end ${r.stillConnected}${r.unmatchedLeaves ? `, leaves with no matching join ${r.unmatchedLeaves}` : ""}`);
  const width = Math.max(4, ...r.players.map((p) => p.names.join("/").length));
  for (const p of [...r.players].sort((a, b) => b.minutes - a.minutes)) {
    console.log(`  ${p.names.join("/").padEnd(width)}  ${String(p.minutes).padStart(7)} min  ${p.sessions} session${p.sessions === 1 ? "" : "s"}`);
  }
  console.log(`SUMMARY host-attendance ok unique=${r.unique} peak=${r.peak ? r.peak.count : 0} log=${logFile}`);
  return 0;
}
