// `tools\cli e2e --help` started a real e2e run (2026-11-14). Every heavy command now treats --help
// or -h as "print usage, exit 0" through one guard (tools/lib/help-guard.mjs) that tools/cli.mjs asks
// before it dispatches. This checks the guard as a pure function for every heavy command and wt
// subcommand, then runs the real CLI with --help on each and checks usage came out and nothing ran.
// red-proof: with "e2e" removed from HEAVY_HELP in help-guard.mjs, "e2e --help is a help request"
//   failed (the pure checks stop the run before any real command line is tried, so the red run
//   starts nothing); with wantsHelp ignoring "-h", "e2e -h is a help request" failed. Both restored.
// inputs: tools/lib/help-guard.mjs, tools/lib/quiet-run.mjs, tools/cli.mjs, tools/validation/help-guard-selftest.mjs
import { spawnSync } from "node:child_process";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { HEAVY_HELP, WT_HELP, helpRequest, usageLines } from "../lib/help-guard.mjs";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");
const failures = [];
const check = (ok, msg) => {
  if (!ok) failures.push(msg);
};

// Every command the task names: e2e, test, session up, ci, build, serve, look, wt gate, wt ship.
const LINES = [
  ["e2e", ["--help"]],
  ["e2e", ["-h"]],
  ["test", ["--no-build", "-h"]],
  ["session", ["up", "--help"]],
  ["ci", ["--help"]],
  ["build", ["--help"]],
  ["serve", ["--help"]],
  ["look", ["--help"]],
  ["dev", ["-h"]],
  ["wt", ["gate", "slug", "--help"]],
  ["wt", ["ship", "--help"]],
  ["wt", ["new", "-h"]],
];
for (const [cmd, rest] of LINES) {
  const label = `${cmd} ${rest.join(" ")}`;
  check(helpRequest(cmd, rest) !== null, `${label} is a help request`);
}
check(HEAVY_HELP.includes("e2e") && HEAVY_HELP.includes("ci") && HEAVY_HELP.includes("serve"), "the heavy list names e2e, ci and serve");
check(WT_HELP.includes("gate") && WT_HELP.includes("ship"), "the wt list names gate and ship");
check(helpRequest("e2e", ["--local"]) === null && helpRequest("test", []) === null, "a command line without --help is not a help request");
check(helpRequest("status", ["--help"]) === null, "a light command keeps its own --help");
check(helpRequest("wt", ["--help"]) === null && helpRequest("wt", ["list"]) === null, "`wt --help` stays wt's own help");

const sample = ["usage: x", "", "  test [--tier t]  runs", "                 more", "  testbox  other", "  tools\\cli wt gate <slug>  gate", "                    more gate", "  tools\\cli wt ship <slug>  ship", "", "flags:"].join("\n");
check(usageLines(sample, "test").length === 2 && usageLines(sample, "test").every((l) => !l.startsWith("testbox")), "usageLines cuts one command's entry and its continuation lines");
check(usageLines(sample, "wt", "gate").length === 2 && usageLines(sample, "wt", "gate")[1].includes("more gate"), "usageLines cuts one wt subcommand");

if (failures.length) {
  console.log("help-guard-selftest: FAIL");
  for (const f of failures) console.log(`  ${f}`);
  process.exit(1);
}

// The real CLI: usage text, exit 0, and no run. A run would print a SUMMARY line, a lock message or
// a main-checkout refusal, and would not finish in a second or two.
for (const [cmd, rest] of LINES) {
  const label = `${cmd} ${rest.join(" ")}`;
  const t = Date.now();
  const r = spawnSync(process.execPath, [join(ROOT, "tools", "cli.mjs"), cmd, ...rest], {
    cwd: ROOT,
    encoding: "utf8",
    windowsHide: true,
    timeout: 20000,
    env: { ...process.env, LF_QUIET_CHILD: "", LF_VERBOSE: "", LF_OUTPUT: "" },
  });
  const out = `${r.stdout}${r.stderr}`;
  check(r.status === 0, `${label} exits 0 (got ${r.status}${r.error ? `, ${r.error.code}` : ""})`);
  check(/^usage: /m.test(out), `${label} prints usage`);
  check(!/^SUMMARY |lock '|FAIL|refus|unknown command/m.test(out), `${label} printed a run's output, not usage: ${out.split(/\r?\n/)[0]}`);
  check(Date.now() - t < 15000, `${label} answered quickly`);
}

if (failures.length) {
  console.log("help-guard-selftest: FAIL");
  for (const f of failures) console.log(`  ${f}`);
  process.exit(1);
}
console.log(`help-guard-selftest: OK (${LINES.length} command lines print usage and exit 0 without running)`);
