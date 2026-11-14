// Output budget: the heavy commands print one compact summary by default (tools/lib/quiet-run.mjs).
// This runs the quick commands (help, wt help, pool status, validate with every check but one
// skipped) and feeds large transcripts through the summariser, and fails when the default
// output goes over its line budget or a failure, a skip or a timeout is hidden.
//
// red-proof: with MAX_FAILURE_LINES set to 1000 in quiet-run.mjs, "a 300-failure transcript stays
// inside its budget" failed; with LF_VERBOSE=1 forced into the child environment, "validate stays
// inside 14 lines" and "help stays inside 12 lines" failed. Both restored.
import { readdirSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { spawnSync } from "node:child_process";
import { fileURLToPath } from "node:url";
import { PASSTHROUGH, summarize } from "../lib/quiet-run.mjs";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");
const failures = [];
const check = (ok, msg) => {
  if (!ok) failures.push(msg);
};

// ---- the summariser, on transcripts of known shape
const log = "tmp/logs/x.log";
const sum = (label, code, lines) => summarize({ label, code, text: lines.join("\n"), logPath: log, seconds: 1 });

const bigPass = sum("validate", 0, Array.from({ length: 300 }, (_, i) => `PASS  validate/c${i}.mjs (0.5 s)\nc${i}: OK`));
check(bigPass.length <= 14, `a 600-line passing transcript stays inside 14 lines (got ${bigPass.length})`);
check(/^SUMMARY validate ok exit=0 /.test(bigPass[0]) && bigPass[0].includes(log), "the summary line says ok and names the log");

const manyFail = sum("test", 1, Array.from({ length: 300 }, (_, i) => `FAIL  LF_Test_X${i}: Scripts/Game/Foo.c:${i}: expected 1 got 2`));
check(manyFail.length <= 45, `a 300-failure transcript stays inside its budget (got ${manyFail.length})`);
check(manyFail[0].startsWith("SUMMARY test fail exit=1 "), "a failing run says fail");
check(manyFail.some((l) => l.includes("Scripts/Game/Foo.c:0: expected 1 got 2")), "the first failure keeps file:line and the exact error");
check(manyFail.some((l) => /more failure line/.test(l)), "failures beyond the cap are counted, not dropped silently");

const timeout = sum("e2e", 1, ["boot", "TIMEOUT after 300s", "..."]);
check(timeout.some((l) => l.includes("TIMEOUT after 300s")), "a timeout is shown");

const skipped = sum("validate", 0, [...Array.from({ length: 20 }, (_, i) => `PASS  validate/c${i}.mjs (0.1 s)`), "SKIP  validate/validate-scripts.ps1 (LF_VALIDATE_SKIP)", "validate: 20 check(s), 0 failed (3 s)"]);
check(skipped.some((l) => l.includes("validate-scripts.ps1")), "a skipped check is named in the summary");
check(skipped.some((l) => l.startsWith("validate: 20 check(s)")), "validate's own verdict line is kept");

const warned = sum("session-up", 0, [...Array.from({ length: 20 }, (_, i) => `session: queued ${i}`), "pool: the session of x is past its 20 min cap with 3 waiting; warned, it closes in 5 min", "session: ready in 25.7 s; 1 client(s) joined"]);
check(warned.some((l) => l.includes("past its 20 min cap")), "a pool warning that changes a decision is shown");
check(warned.some((l) => l.includes("session: ready")), "the ready line is kept");

const fixture = sum("wt-ship", 0, [...Array.from({ length: 12 }, (_, i) => `PASS  validate/c${i}.mjs (0.1 s)`), "PASS  validate/pool-selftest.mjs (0.5 s)", "pool: the session of x is past its 20 min cap with 1 waiting; warned, it closes in 5 min", "WARN  pool: the scheduler reclaimed this run's slot", "PASS  validate/validate-worktrees.ps1 (1 s)", "WARN  worktree 'a' is clean"]);
check(fixture.some((l) => l.includes("warnings=1 ")) && !fixture.some((l) => l.includes("scheduler reclaimed")), "warnings printed by a passing selftest (pool fixtures) are not this run's warnings, a check's own WARN is");

const short = sum("look", 0, ["look: 17/17 shot(s) in 30.8 s", "  bank: tmp\\look\\bank\\contact.jpg"]);
check(short.length === 3 && short[2].includes("contact.jpg"), `a run of at most ${PASSTHROUGH} lines prints as it is`);

const silentFail = sum("build", 2, Array.from({ length: 40 }, (_, i) => `line ${i}`));
check(silentFail.length > 12 && silentFail.some((l) => l === "line 39"), "a failure with no recognisable error shows the end of the transcript");

const stepFail = sum("keys-proof", 1, [...Array.from({ length: 20 }, (_, i) => `client 1: s:abc step ${i} -> ok x`), "client 2: s:abc use LF_X Door -> error no_target", "keys proof: FAIL"]);
check(stepFail.some((l) => l.includes("-> error no_target")) && stepFail.some((l) => l.includes("keys proof: FAIL")), "a failing proof step and its verdict are shown");

// ---- the real quick commands
function run(args, env = {}) {
  const r = spawnSync(process.execPath, [join(ROOT, "tools", "cli.mjs"), ...args], {
    cwd: ROOT,
    encoding: "utf8",
    windowsHide: true,
    env: { ...process.env, LF_QUIET_CHILD: "", LF_VERBOSE: "", LF_OUTPUT: "", ...env },
  });
  const lines = `${r.stdout}${r.stderr}`.split(/\r?\n/).filter((l, i, a) => l !== "" || i < a.length - 1);
  return { lines, code: r.status };
}

const budgets = [
  ["help", ["help"], 12],
  ["wt help", ["wt", "help"], 6],
  ["pool status", ["pool", "status"], 40],
];
for (const [name, args, max] of budgets) {
  const r = run(args);
  check(r.lines.length <= max, `${name} stays inside ${max} lines (got ${r.lines.length})`);
}
const full = run(["help", "--verbose"]);
check(full.lines.length > 60, `help --verbose still prints the full usage (got ${full.lines.length} lines)`);

// validate with every check but one skipped: the wrapper's output, not the checks', is under test.
const keep = "validate-no-bom.mjs";
const skipAll = readdirSync(join(ROOT, "tools", "validation")).filter((f) => /\.(mjs|ps1)$/.test(f) && f !== keep);
const v = run(["validate"], { LF_VALIDATE_SKIP: skipAll.join(",") });
check(v.lines.length <= 14 + 1, `validate stays inside 14 lines (got ${v.lines.length})`);
check(v.lines[0]?.startsWith("SUMMARY validate "), `validate starts with a SUMMARY line (got '${v.lines[0]}')`);
check(v.lines.some((l) => /^skipped \d+ check/.test(l)), "validate names the checks it skipped");
const vv = run(["validate", "--verbose"], { LF_VALIDATE_SKIP: skipAll.join(",") });
check(vv.lines.length > v.lines.length && vv.lines.some((l) => l.startsWith("PASS")), "validate --verbose restores the per-check lines");

if (failures.length) {
  console.log("output-budget-selftest: FAIL");
  for (const f of failures) console.log(`  ${f}`);
  process.exit(1);
}
console.log("output-budget-selftest: OK (summaries, caps, skips and timeouts kept; help, wt help, pool status, validate inside their budgets)");
