// Proves a failing test is demoted to a warning on a maintainer world edit (tools/lib/world-test-judge.mjs)
// while a compile error or a run that judged nothing still blocks, and that the warnings reach the PR body.
// inputs: tools/lib/world-test-judge.mjs, tools/commands/world.mjs, tools/validation/world-test-judge-selftest.mjs
// red-proof: with judgeWorldTests returning `blocks: "tests failed"` for every non-zero status this printed
//   "FAIL a failing test is a warning, not a block" and exited 1 (2026-11-15); restored, it passes.
import { judgeWorldTests, testWarningLines, TEST_WARNINGS_HEADING } from "../lib/world-test-judge.mjs";
import { prBody } from "../commands/world.mjs";
import { diffFiles } from "../lib/world-diff.mjs";

const fails = [];
const check = (ok, msg) => { if (!ok) fails.push(msg); };

// The transcript of the 2026-11-15 world ship: the board moved, the arrival chain failed, nothing names the test.
const compact = [
  "11:35:28.007 SCRIPT       : [LFTEST] SUMMARY tier=only passed=166 failed=1 total=167 skipped=0",
  "test-e2e: SUMMARY tier=only passed=166 failed=1 total=167 (8 s)",
  "  11:35:28.003 SCRIPT       : [LFTEST]   - reading the town board beside the clerk counts",
  "  11:35:28.003 SCRIPT       : [LFTEST]   - the welcome cash lands in the pockets (expected 250, got 0)",
  "test-e2e: FAILED (1 failing test(s))",
  "remote: key lines from the lf-cloud console.log:",
  "  11:35:28.003 SCRIPT       : [LFTEST]   - reading the town board beside the clerk counts",
].join("\n");
const sources = [
  { file: "LF_Test_ArrivalChain.c", text: 'ctx.True(tracker.RequestSignRead("town_map"), "reading the town board beside the clerk counts");\nctx.Equal(1, 1, "the welcome cash lands in the pockets");' },
  { file: "LF_Test_Other.c", text: 'ctx.True(true, "something else");' },
];
const v = judgeWorldTests({ status: 1, text: compact, fallbackTests: ["LF_Test_A", "LF_Test_B"], tests: sources });
check(v.blocks === null, "a failing test is a warning, not a block");
check(v.failed === 1 && v.warnings.length === 2, `each failing assertion is listed once, got ${JSON.stringify(v.warnings)}`);
check(v.warnings.every((w) => w.test === "LF_Test_ArrivalChain"), `the test is named from its source, got ${JSON.stringify(v.warnings)}`);
check(v.warnings.some((w) => /expected 250, got 0/.test(w.assertion)), "the assertion keeps its expected/got text");

const headed = judgeWorldTests({ status: 1, text: "[LFTEST] FAIL quests/arrival-chain\n[LFTEST]   - a step\ntest-e2e: SUMMARY tier=only passed=0 failed=1 total=1 (3 s)" });
check(headed.warnings.length === 1 && headed.warnings[0].test === "quests/arrival-chain", "a FAIL header names the test");

check(judgeWorldTests({ status: 0, text: "" }).warnings.length === 0, "a passing run has no warnings");
check(judgeWorldTests({ status: 1, text: "test-e2e: FAILED (scripts do not compile):\n  SCRIPT    (E): Can't compile \"Game\" script module!" }).blocks !== null, "a compile error blocks");
check(judgeWorldTests({ status: 1, text: "[LFTEST] SUMMARY tier=only passed=0 failed=1 total=1\nCan't compile \"Game\" script module!" }).blocks !== null, "a compile error blocks even beside a SUMMARY");
check(judgeWorldTests({ status: 1, text: "TIMEOUT after 281s (boot budget 240s, the server never reached ready)" }).blocks !== null, "a run with no SUMMARY judged nothing and blocks");
check(judgeWorldTests({ status: 1, text: "test-e2e: SUMMARY tier=only passed=3 failed=0 total=3 (3 s)" }).blocks !== null, "a non-zero exit with no failing test blocks");

const body = prBody(diffFiles([]), { slug: "town-maintainer-edits", testWarnings: testWarningLines(v.warnings) });
check(body.includes(TEST_WARNINGS_HEADING) && body.includes("LF_Test_ArrivalChain") && body.includes("reading the town board beside the clerk counts"), "the warnings reach the PR body under the heading");
check(!prBody(diffFiles([]), { slug: "town-maintainer-edits" }).includes(TEST_WARNINGS_HEADING), "no warnings, no heading");

if (fails.length) {
  for (const f of fails) console.log(`FAIL ${f}`);
  process.exit(1);
}
console.log("world-test-judge-selftest: OK");
