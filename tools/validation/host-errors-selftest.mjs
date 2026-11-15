// tools/validation/host-errors-selftest.mjs
// inputs: tools/host/**, tools/lib/log-triage.mjs
//
// Proves `tools\cli host errors` (#1500): a fixture console.log grown in three writes (the second
// ends mid-line, so a half-written line is never printed early) prints its two error lines once
// each and nothing else (no info, no warning), marks one known against a triage table and one
// new, and ends once the `--for` span has passed on a fake clock. A truncated log (new run
// overwrote the file) is read from its start again.
// red-proof: before tools/host/errors.mjs existed this file died on the import:
// "Cannot find module '.../tools/host/errors.mjs'" (2026-11-15).

import { appendFileSync, mkdtempSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join, resolve, dirname } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");
const errors = await import(pathToFileURL(join(ROOT, "tools", "host", "errors.mjs")).href);
const { loadTriageRows } = await import(pathToFileURL(join(ROOT, "tools", "lib", "log-triage.mjs")).href);

const failures = [];
const fail = (m) => failures.push(m);

const dir = mkdtempSync(join(tmpdir(), "lf-host-errors-"));
try {
  const triage = join(dir, "triage.md");
  writeFileSync(
    triage,
    "| Message (pattern) | Verdict | What it is |\n|---|---|---|\n| `[LFDebug:Persistence] save point FAILED` | FIXED | engine owns the save |\n",
  );
  const rows = loadTriageRows(triage);
  const log = join(dir, "console.log");

  const knownLine = "12:00:03.100  SCRIPT (E): [LFDebug:Persistence] save point FAILED (SHUTDOWN)";
  const newLine = "12:00:05.200  SCRIPT (E): Virtual Machine Exception: null pointer in LF_Fresh.Run";
  const writes = [
    // write 1: noise, an info, a warning and the known error
    "12:00:01.000  DEFAULT: engine booting\n12:00:02.000  SCRIPT (W): [LFDebug:Jobs] job request slow\n12:00:02.500  SCRIPT : [LFDebug:Jobs] info line\n" + knownLine + "\n",
    // write 2: ends mid-line; the new error is not complete yet
    "12:00:04.000  DEFAULT: tick\n" + newLine.slice(0, 40),
    // write 3: finishes the new error and ends with another info line
    newLine.slice(40) + "\n12:00:06.000  DEFAULT: done\n",
  ];
  writeFileSync(log, "");

  let now = 0;
  let step = 0;
  const printed = [];
  const slept = [];
  const result = errors.follow({
    file: log,
    rows,
    forSec: 30,
    pollSec: 5,
    out: (e) => printed.push(e),
    sleep: (ms) => {
      slept.push(ms);
      now += ms;
      if (step < writes.length) appendFileSync(log, writes[step++]);
    },
    now: () => now,
  });

  if (printed.length !== 2) fail(`expected 2 printed errors, got ${printed.length}: ${JSON.stringify(printed.map((p) => p.body))}`);
  const [a, b] = printed;
  if (a && !(a.known && a.verdict === "FIXED" && a.body.includes("save point FAILED"))) fail(`first error should be the known save-point line: ${JSON.stringify(a)}`);
  if (b && !(!b.known && b.kind === "VME" && b.body.includes("LF_Fresh.Run"))) fail(`second error should be the new VME rejoined across two writes: ${JSON.stringify(b)}`);
  if (printed.some((p) => /info line|job request slow|engine booting/.test(p.body))) fail("a non-error line was printed");
  if (result.errors !== 2 || result.known !== 1 || result.fresh !== 1) fail(`totals wrong: ${JSON.stringify(result)}`);
  if (result.reason !== "span") fail(`should end on the --for span, ended on ${result.reason}`);
  if (now < 30000 || now > 35000) fail(`should stop right after 30 s on the fake clock, stopped at ${now} ms`);
  if (slept.some((ms) => ms !== 5000)) fail(`every sleep should be one 5 s poll: ${JSON.stringify(slept)}`);

  // formatting: one line, marked KNOWN or NEW
  const lines = printed.map((p) => errors.formatError(p));
  if (lines[0] && !/^KNOWN\b.*save point FAILED/.test(lines[0])) fail(`known line format: ${lines[0]}`);
  if (lines[1] && !/^NEW\b.*LF_Fresh\.Run/.test(lines[1])) fail(`new line format: ${lines[1]}`);

  // a log that restarts smaller than the read offset is read again from its start
  writeFileSync(log, "13:00:00.000  SCRIPT (E): [LFDebug:Jobs] second run error one\n");
  const again = [];
  let t2 = 0;
  let writes2 = 0;
  const r2 = errors.follow({
    file: log, rows, forSec: 10, pollSec: 5,
    out: (e) => again.push(e),
    sleep: (ms) => {
      t2 += ms;
      if (writes2++ === 0) writeFileSync(log, "13:00:07.000  SCRIPT (E): [LFDebug:Jobs] fresh after rotate\n");
    },
    now: () => t2,
  });
  if (again.length !== 2 || r2.errors !== 2) fail(`a truncated log should be re-read: got ${again.length} lines`);

  // a missing log is waited for, not a crash, and still ends at --for
  let t3 = 0;
  const r3 = errors.follow({ file: join(dir, "nope.log"), rows, forSec: 10, pollSec: 5, out: () => {}, sleep: (ms) => { t3 += ms; }, now: () => t3 });
  if (r3.errors !== 0 || r3.reason !== "span") fail(`a missing log should end quietly at --for: ${JSON.stringify(r3)}`);

  // argument parsing
  if (errors.parseArgs([]).forMin !== 60) fail("default --for should be 60 min");
  if (errors.parseArgs(["--for", "5"]).forMin !== 5) fail("--for 5 should be 5 min");
  if (!errors.parseArgs(["--for", "0"]).error) fail("--for 0 should be refused");
  if (!errors.parseArgs(["--for", "x"]).error) fail("--for x should be refused");
} finally {
  rmSync(dir, { recursive: true, force: true });
}

if (failures.length) {
  console.log(`host-errors-selftest FAILED:\n${failures.map((f) => `  ${f}`).join("\n")}`);
  process.exit(1);
}
console.log("host-errors-selftest ok");
