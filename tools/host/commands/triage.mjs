// tools\cli host triage [<console.log>] [--triage <file>] [--json] [--draft [--out <dir>]]
// Groups the [LFDebug:*] warnings and errors, SCRIPT (E) and VME lines of a server log by normalised
// text, counts them, and marks each group known (a docs/debug-log-triage.md row matches) or new.
// The matcher is tools/lib/log-triage.mjs; the server-doctor reuses it. `--draft` also writes one
// issue draft per new group (tools/host/triage-draft.mjs) and prints the `gh issue create` commands;
// it never files an issue itself.

import { existsSync, readFileSync } from "node:fs";
import { basename, join, relative, resolve } from "node:path";
import { hostPaths, newestConsoleLog } from "../host.mjs";
import { loadTriageRows, triageLog } from "../../lib/log-triage.mjs";
import { draftGroups, ghCreateCommand, writeDrafts } from "../triage-draft.mjs";

export const help =
  "triage [<console.log>] [--triage <file>] [--json] [--draft [--out <dir>]]   group a server log's errors and warnings: known (docs/debug-log-triage.md) or new; --draft writes tmp/triage/<date>/<n>.md per new group and prints the gh issue create commands (never runs them)";

const VALUE_FLAGS = ["--triage", "--out"];

function flagValue(args, name) {
  const i = args.indexOf(name);
  return i >= 0 ? args[i + 1] : undefined;
}

function today() {
  const d = new Date();
  const p = (n) => String(n).padStart(2, "0");
  return `${d.getFullYear()}-${p(d.getMonth() + 1)}-${p(d.getDate())}`;
}

export async function main(args, ctx) {
  const triageFile = resolve(flagValue(args, "--triage") ?? join(ctx.root, "docs", "debug-log-triage.md"));
  const skip = new Set(VALUE_FLAGS.map((f) => args.indexOf(f) + 1).filter((n) => n > 0));
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
  const logText = readFileSync(logFile, "utf8");
  const { total, groups } = triageLog(logText, loadTriageRows(triageFile));
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
  let draftNote = "";
  if (args.includes("--draft")) {
    const outFlag = flagValue(args, "--out");
    const dir = outFlag ? resolve(outFlag) : join(ctx.root, "tmp", "triage", today());
    const drafts = draftGroups(logText, groups, basename(logFile));
    const files = writeDrafts(drafts, dir);
    if (drafts.length) {
      console.log(`drafts in ${dir}; nothing is filed. Review each draft, then run from ${ctx.root}:`);
      drafts.forEach((d, i) => {
        const rel = relative(ctx.root, join(dir, files[i])).replace(/\\/g, "/");
        console.log(ghCreateCommand(d, rel.startsWith("..") ? join(dir, files[i]).replace(/\\/g, "/") : rel));
      });
    } else {
      console.log("host triage: no new groups, no drafts written");
    }
    draftNote = ` drafts=${drafts.length}`;
  }
  console.log(`SUMMARY host-triage ok groups=${groups.length} known=${known} new=${groups.length - known}${draftNote} lines=${total} log=${logFile}`);
  return 0;
}
