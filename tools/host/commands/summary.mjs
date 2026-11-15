// `tools\cli host summary [<console.log>] [--perf <csv>] [--triage <file>] [--out <dir>] [--target N]
// [--json]` (#1501): reads a server console.log (default: the newest one of the host profile) and
// writes <host>/sessions/<stamp>.md: length, unique players and peak, restarts and crashes, the
// triage counts (known and new) and the perf verdict. The perf file is --perf or the newest
// <host>/perf/perf-*.csv. The logic is tools/host/summary.mjs.
import { existsSync, mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { basename, dirname, join, resolve } from "node:path";
import { hostPaths, newestConsoleLog, stamp } from "../host.mjs";
import { loadTriageRows } from "../../lib/log-triage.mjs";
import { crashFolders, DEFAULT_TARGET, newestPerfCsv, renderMarkdown, stampFromPath, summarize } from "../summary.mjs";

export const help =
  "summary [<console.log>] [--perf <csv>] [--triage <file>] [--out <dir>] [--target 40] [--json]   write <host>/sessions/<stamp>.md after a session: length, unique players and peak, restarts and crashes, triage known/new, the perf verdict";

const VALUE_FLAGS = ["--perf", "--triage", "--out", "--target"];

function flagValue(args, name) {
  const i = args.indexOf(name);
  return i >= 0 ? args[i + 1] : undefined;
}

export async function main(args, ctx) {
  const say = (m) => console.log(`[LFDebug:HostSummary] ${m}`);
  const skip = new Set(VALUE_FLAGS.map((f) => args.indexOf(f) + 1).filter((n) => n > 0));
  const positional = args.filter((a, i) => !a.startsWith("--") && !skip.has(i));
  const paths = hostPaths(ctx.hostDir);
  const logFile = positional[0] ? resolve(positional[0]) : newestConsoleLog(paths.profile);
  if (!logFile || !existsSync(logFile)) {
    say(`no console.log to read${positional[0] ? ` at ${positional[0]}` : " (the host has not run; pass a path)"}`);
    return 1;
  }
  const target = flagValue(args, "--target") === undefined ? DEFAULT_TARGET : Number(flagValue(args, "--target"));
  if (!Number.isFinite(target) || target < 1) {
    say("--target must be a number above 0");
    return 1;
  }
  const triageFile = resolve(flagValue(args, "--triage") ?? join(ctx.root, "docs", "debug-log-triage.md"));
  if (!existsSync(triageFile)) {
    say(`triage table not found at ${triageFile}`);
    return 1;
  }
  const perfFlag = flagValue(args, "--perf");
  const perfFile = perfFlag ? resolve(perfFlag) : newestPerfCsv(ctx.hostDir);
  if (perfFlag && !existsSync(perfFile)) {
    say(`perf file not found at ${perfFile}`);
    return 1;
  }

  const logText = readFileSync(logFile, "utf8");
  // Crash folders only mean something for a log under the host profile (its folder name is the start).
  const startStamp = stampFromPath(logFile);
  let crash = null;
  if (startStamp) {
    const probe = summarize({ logText, triageRows: [] });
    const endMs = Date.parse(startStamp.replace(/_(\d{2})-(\d{2})-(\d{2})$/, "T$1:$2:$3")) + probe.length.seconds * 1000;
    crash = crashFolders(paths.backups, startStamp, stamp(new Date(endMs)));
  }

  const now = new Date();
  const s = summarize({
    logText,
    logName: startStamp ? `${basename(dirname(logFile))}/${basename(logFile)}` : basename(logFile),
    triageRows: loadTriageRows(triageFile),
    perfCsv: perfFile && existsSync(perfFile) ? readFileSync(perfFile, "utf8") : null,
    crash,
    target,
    uploadMbit: Number(process.env.LF_HOST_UPLOAD_MBIT) > 0 ? Number(process.env.LF_HOST_UPLOAD_MBIT) : undefined,
  });
  if (args.includes("--json")) {
    console.log(JSON.stringify(s, null, 2));
    return 0;
  }
  const outDir = flagValue(args, "--out") ? resolve(flagValue(args, "--out")) : join(ctx.hostDir, "sessions");
  mkdirSync(outDir, { recursive: true });
  const name = stamp(now);
  const file = join(outDir, `${name}.md`);
  writeFileSync(file, renderMarkdown(s, name));
  say(`wrote ${file}`);
  console.log(`SUMMARY host-summary ok players=${s.attendance.unique} peak=${s.attendance.peak ? s.attendance.peak.count : 0} new=${s.triage.new} perf=${s.perf ? (s.perf.ok === null ? "none" : s.perf.ok ? "ok" : "over") : "none"} file=${file}`);
  return 0;
}
