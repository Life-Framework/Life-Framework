// tools/validation/host-applications-selftest.mjs
// inputs: tools/host/applications.mjs, tools/host/commands/applications.mjs, tools/validation/fixtures/host-applications/**
//
// Proves `tools\cli host applications`: a fixture police_applications.txt (open, six- and
// seven-field old rows, approved on trial, promoted, rejected, expired, and three malformed
// rows) gives the expected queue table (oldest open first, then officers on trial with the hours
// left), `--all` adds the closed rows, each malformed row is skipped with a warning naming its
// line, and the CLI subcommand prints the same table for `--file`/`--now`.
// red-proof: before tools/host/applications.mjs existed this printed
// "FAIL cannot load tools/host/applications.mjs: ERR_MODULE_NOT_FOUND" (2026-11-15).

import { readFileSync } from "node:fs";
import { spawnSync } from "node:child_process";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");
const FIX = join(ROOT, "tools", "validation", "fixtures", "host-applications");
const NOW = 1700000000;
let failed = 0;

function fail(msg) {
  console.log(`FAIL  ${msg}`);
  failed++;
}

function check(cond, msg) {
  if (!cond) fail(msg);
}

function sameText(got, want, msg) {
  if (got !== want) fail(`${msg}\n--- got ---\n${got}\n--- want ---\n${want}`);
}

let apps;
try {
  apps = await import(pathToFileURL(join(ROOT, "tools", "host", "applications.mjs")).href);
} catch (e) {
  fail(`cannot load tools/host/applications.mjs: ${e.code ?? e.message}`);
  console.log(`SUMMARY host-applications-selftest fail ${failed}`);
  process.exit(1);
}

const text = readFileSync(join(FIX, "police_applications.txt"), "utf8");
const want = readFileSync(join(FIX, "expected.txt"), "utf8");
const wantAll = readFileSync(join(FIX, "expected-all.txt"), "utf8");

const { rows, warnings } = apps.parseApplications(text);
check(rows.length === 8, `parsed ${rows.length} rows, expected 8`);
check(warnings.length === 3, `${warnings.length} warnings, expected 3 (one per malformed row)`);
for (const line of [9, 10, 11]) {
  check(warnings.some((w) => w.includes(`line ${line}:`)), `no warning names line ${line}`);
}
const six = rows.find((r) => r.uid === "u2");
check(six && six.rejectedAt === 0 && six.trialEndsAt === 0, "six-field row should default rejectedAt and trialEndsAt to 0");
const seven = rows.find((r) => r.uid === "u8");
check(seven && seven.status === "OPEN" && seven.availability === "mornings", "seven-field row should parse");

sameText(apps.formatQueue(rows, NOW, { all: false }), want, "default table differs from expected.txt");
sameText(apps.formatQueue(rows, NOW, { all: true }), wantAll, "--all table differs from expected-all.txt");
sameText(apps.formatQueue([], NOW, { all: false }), "No open applications.\n\nNo officers on trial.\n", "empty queue text");

const trialEnded = apps.parseApplications("APPROVED|u1|Old Trial|x|any|1|0|1699990000").rows;
check(apps.formatQueue(trialEnded, NOW, {}).includes("0h"), "a trial past its end shows 0h left");

// The CLI subcommand, end to end.
const cli = (extra) =>
  spawnSync(process.execPath, [join(ROOT, "tools", "cli.mjs"), "host", "applications", "--file", join(FIX, "police_applications.txt"), "--now", String(NOW), ...extra], {
    encoding: "utf8",
    timeout: 30000,
    windowsHide: true,
  });
const r1 = cli([]);
check(r1.status === 0, `host applications exited ${r1.status}: ${r1.stdout}${r1.stderr}`);
check((r1.stdout || "").includes(want.trimEnd()), "host applications output lacks the expected table");
check((r1.stdout || "").includes("warning:"), "host applications output lacks the malformed-row warnings");
const r2 = cli(["--all"]);
check((r2.stdout || "").includes(wantAll.trimEnd()), "host applications --all output lacks the expected table");
const r3 = spawnSync(process.execPath, [join(ROOT, "tools", "cli.mjs"), "host", "applications", "--file", join(FIX, "nope.txt")], { encoding: "utf8", timeout: 30000, windowsHide: true });
check(r3.status === 0 && (r3.stdout || "").includes("no applications file"), "a missing file should say so and exit 0");

if (failed) {
  console.log(`SUMMARY host-applications-selftest fail ${failed}`);
  process.exit(1);
}
console.log("SUMMARY host-applications-selftest ok");
