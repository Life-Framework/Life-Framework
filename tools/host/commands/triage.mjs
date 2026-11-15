// tools\cli host triage [<console.log>] [--triage <file>] [--json]
// Groups the [LFDebug:*] warnings and errors, SCRIPT (E) and VME lines of a server log by normalised
// text, counts them, and marks each group known (a docs/debug-log-triage.md row matches) or new.
// The matcher is tools/lib/log-triage.mjs; the server-doctor reuses it.

import { existsSync, readFileSync } from "node:fs";
import { join, relative, resolve } from "node:path";
import { hostPaths, newestConsoleLog } from "../host.mjs";
import { loadTriageRows, triageLog } from "../../lib/log-triage.mjs";

export const help = "triage [<console.log>] [--triage <file>] [--json]   group a server log's errors and warnings: known (docs/debug-log-triage.md) or new";

function flagValue(args, name) {
  const i = args.indexOf(name);
  return i >= 0 ? args[i + 1] : undefined;
}

export async function main(args, ctx) {
  const triageFile = resolve(flagValue(args, "--triage") ?? join(ctx.root, "docs", "debug-log-triage.md"));
  const skip = new Set([args.indexOf("--triage") + 1].filter((n) => n > 0));
  const positional = args.filter((a, i) => !a.startsWith("--") && !skip.has(i));
  const logFile = positional[0] ? resolve(positional[0]) : newestConsoleLog(hostPaths(ctx.hostDir).profile);
  if (!logFile || !existsSync(logFile)) {
    console.log(`host triage: no console.log to read${positional[0] ? ` at ${positional[0]}` : " (the host has not run; pass a path)"}`);
    return 1;
  }
  if (!existsSync(triageFile)) {
    console.log(`host triage: triage table not found at ${triageFile}`);
    return 1;
  }
  const { total, groups } = triageLog(readFileSync(logFile, "utf8"), loadTriageRows(triageFile));
  const known = groups.filter((g) => g.known).length;
  if (args.includes("--json")) {
    console.log(JSON.stringify({ log: logFile, total, groups }, null, 2));
  } else {
    for (const g of groups) {
      const where = g.known ? `[${g.verdict}] ` : "";
      const key = g.key.length > 200 ? `${g.key.slice(0, 200)}...` : g.key;
      const ref = g.known ? `  (${relative(ctx.root, triageFile).replace(/\\/g, "/")}:${g.row})` : `  (log line ${g.firstLine})`;
      console.log(`${g.known ? "KNOWN" : "NEW  "}  x${g.count}  ${where}${key}${ref}`);
    }
  }
  console.log(`SUMMARY host-triage ok groups=${groups.length} known=${known} new=${groups.length - known} lines=${total} log=${logFile}`);
  return 0;
}
