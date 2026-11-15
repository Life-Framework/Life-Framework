// tools/validation/host-summary-selftest.mjs
// inputs: tools/host/summary.mjs, tools/host/commands/summary.mjs, tools/host/attendance.mjs, tools/host/perf.mjs, tools/lib/log-triage.mjs, tools/validation/fixtures/host-summary/**
//
// Proves `tools\cli host summary` (#1501): a fixture server log (3 players, peak 3, one scheduled
// restart, five triage lines in three groups), a perf CSV and a host folder with one crash backup
// inside the session and one outside give exactly the expected <host>/sessions/<stamp>.md; an
// over-budget perf file says OVER BUDGET; a log with no perf file, no players and no crash check
// still writes a page; a missing log exits 1 and writes nothing.
// red-proof: before tools/host/summary.mjs existed this printed
// "FAIL cannot load tools/host/summary.mjs: ERR_MODULE_NOT_FOUND" (2026-11-15).

import { cpSync, existsSync, mkdirSync, mkdtempSync, readdirSync, readFileSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";
import { safeRemoveTree } from "../link-safety.mjs";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");
const FIX = join(ROOT, "tools", "validation", "fixtures", "host-summary");
let failed = 0;
const fail = (msg) => {
  console.log(`FAIL  ${msg}`);
  failed++;
};
const check = (cond, msg) => {
  if (!cond) fail(msg);
};

let lib;
let cmd;
try {
  lib = await import(pathToFileURL(join(ROOT, "tools", "host", "summary.mjs")).href);
  cmd = await import(pathToFileURL(join(ROOT, "tools", "host", "commands", "summary.mjs")).href);
} catch (e) {
  fail(`cannot load tools/host/summary.mjs or commands/summary.mjs: ${e.code ?? e.message}`);
  console.log(`SUMMARY host-summary-selftest fail ${failed}`);
  process.exit(1);
}

// Runs the command with its console output captured; returns { code, out }.
async function run(args, ctx) {
  const lines = [];
  const real = console.log;
  console.log = (...a) => lines.push(a.join(" "));
  let code;
  try {
    code = await cmd.main(args, ctx);
  } finally {
    console.log = real;
  }
  return { code, out: lines.join("\n") };
}

const tmp = mkdtempSync(join(tmpdir(), "lf-host-summary-"));
try {
  // A host folder: the log under profile/logs/logs_<stamp>/, crash backups, one perf file.
  const host = join(tmp, "host");
  const logDir = join(host, "profile", "logs", "logs_2026-11-15_18-00-00");
  mkdirSync(logDir, { recursive: true });
  cpSync(join(FIX, "server-console.txt"), join(logDir, "console.log"));
  for (const d of ["2026-11-15_18-45-00-crash", "2026-11-15_20-00-00-crash", "2026-11-15_18-50-00-manual"]) mkdirSync(join(host, "backups", d), { recursive: true });
  mkdirSync(join(host, "perf"), { recursive: true });
  cpSync(join(FIX, "perf.csv"), join(host, "perf", "perf-2026-11-15T18-01-30-000Z.csv"));
  mkdirSync(join(tmp, "docs"), { recursive: true });
  cpSync(join(FIX, "triage.md"), join(tmp, "docs", "debug-log-triage.md"));
  const ctx = { root: tmp, hostDir: host };

  // The default log (newest under the host profile), the default perf file, the default triage table.
  const r = await run([], ctx);
  check(r.code === 0, `summary exited ${r.code}: ${r.out}`);
  const dir = join(host, "sessions");
  const files = existsSync(dir) ? readdirSync(dir) : [];
  check(files.length === 1 && /^\d{4}-\d{2}-\d{2}_\d{2}-\d{2}-\d{2}\.md$/.test(files[0] ?? ""), `want one <stamp>.md in <host>/sessions, got ${JSON.stringify(files)}`);
  if (files.length === 1) {
    const stampName = files[0].replace(/\.md$/, "");
    const want = readFileSync(join(FIX, "expected-summary.md"), "utf8").replace("<STAMP>", stampName);
    const got = readFileSync(join(dir, files[0]), "utf8");
    check(got.trim() === want.trim(), `summary page differs:\n--- got\n${got}\n--- want\n${want}`);
  }
  check(/^SUMMARY host-summary ok players=3 peak=3 new=1 perf=ok file=/m.test(r.out), `SUMMARY line wrong: ${r.out}`);

  // Pure pieces.
  const len = lib.logLength(readFileSync(join(FIX, "server-console.txt"), "utf8"));
  check(len.seconds === 3600 && len.start === "18:00:00" && len.end === "19:00:00", `logLength ${JSON.stringify(len)}`);
  check(lib.logLength("23:59:00.000 a\n00:01:00.000 b").seconds === 120, "logLength must roll past midnight");
  check(lib.logLength("no stamps here").start === null, "logLength of a log with no stamps must say so");
  check(lib.stampFromPath("C:\\h\\profile\\logs\\logs_2026-11-15_18-00-00\\console.log") === "2026-11-15_18-00-00", "stampFromPath on a Windows path");
  check(lib.stampFromPath("/tmp/console.log") === null, "stampFromPath with no logs_ folder is null");

  // Over budget: the verdict is named, and the SUMMARY line says over.
  const over = await run([join(logDir, "console.log"), "--perf", join(FIX, "perf-over.csv"), "--out", join(tmp, "out-over")], ctx);
  const overFile = join(tmp, "out-over", readdirSync(join(tmp, "out-over"))[0] ?? "none");
  check(over.code === 0 && /perf=over/.test(over.out), `over-budget run: ${over.out}`);
  check(existsSync(overFile) && /OVER BUDGET \(cpu\)/.test(readFileSync(overFile, "utf8")), "over-budget page must say OVER BUDGET (cpu)");

  // A log outside the host profile: no crash check, no perf file, nobody on, still a page.
  const bare = join(tmp, "bare.txt");
  writeFileSync(bare, "10:00:00.000  DEFAULT      : Engine start\n10:00:05.000  DEFAULT      : nothing else\n");
  const b = await run([bare, "--out", join(tmp, "out-bare")], { root: tmp, hostDir: join(tmp, "nohost") });
  const bareText = readFileSync(join(tmp, "out-bare", readdirSync(join(tmp, "out-bare"))[0] ?? "none"), "utf8");
  check(b.code === 0 && /Crashes: not checked/.test(bareText) && /no perf file/.test(bareText) && /nobody joined/.test(bareText) && /ends with: no close line/.test(bareText), `bare page wrong:\n${bareText}`);

  // A missing log writes nothing and exits 1.
  const none = await run([join(tmp, "missing.txt"), "--out", join(tmp, "out-none")], ctx);
  check(none.code === 1 && !existsSync(join(tmp, "out-none")), `missing log: code ${none.code}, out dir ${existsSync(join(tmp, "out-none"))}`);
} finally {
  safeRemoveTree(tmp);
}

if (failed) {
  console.log(`SUMMARY host-summary-selftest fail ${failed}`);
  process.exit(1);
}
console.log("SUMMARY host-summary-selftest ok");
