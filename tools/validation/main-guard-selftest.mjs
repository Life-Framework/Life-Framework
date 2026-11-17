// Proves the main guard's rules (tools/lib/main-guard.mjs, docs/worktrees.md "Main guard"): batching, the merges it
// may blame, the bisect, the flake rule, never reverting a revert or a pre-start merge, and the status line.
// inputs: tools/lib/main-guard.mjs, tools/main-guard.mjs
// red-proof: with revertRefusal's isRevert check removed this printed "a revert merge must never be reverted";
//   with bisect's `hi = mid` swapped for `lo = mid` the run never finished (killed by `timeout 10`).
// red-proof #2052: with setHits returning every failed test (red/green again) this printed "FAIL same set: blame m3, got
//   m1111111" and "FAIL different set: no blame".
import { readFileSync } from "node:fs";
import * as g from "../lib/main-guard.mjs";
import * as pin from "../lib/guard-pin.mjs";

const fails = [];
const check = (ok, msg) => { if (!ok) fails.push(msg); };
const T = Date.parse("2026-11-16T12:00:00Z");

// Batching: new head runs; a second head within 10 min waits; unchanged head never runs.
check(g.shouldRun({ lastChecked: "a" }, "b", T).run, "a new head must run");
check(!g.shouldRun({ lastChecked: "b" }, "b", T).run, "an unchanged head must not run");
check(!g.shouldRun({ lastChecked: "a", lastRunAt: new Date(T - 3 * 60000).toISOString() }, "b", T).run, "a head 3 min after the last run waits (batch)");
check(g.shouldRun({ lastChecked: "a", lastRunAt: new Date(T - 11 * 60000).toISOString() }, "b", T).run, "a head 11 min after the last run runs");

check(g.prOfMerge("Merge pull request #1590 from Life-Framework/ws/staff-relay-install") === 1590, "prOfMerge");
check(g.prOfMerge("Merge remote-tracking branch 'origin/main' into ws/x") === null, "a main merge is not a PR");

// Candidates: merges before the guard started are never blamed.
const merges = [
  { sha: "m0000000", subject: "Merge pull request #10 from x/ws/a", beforeStart: true, order: 0 },
  { sha: "m1111111", subject: "Merge pull request #11 from x/ws/b", order: 1 },
  { sha: "m2222222", subject: "Merge pull request #12 from x/ws/c", order: 2 },
  { sha: "m3333333", subject: "Merge pull request #13 from x/ws/d", order: 3 },
  { sha: "m4444444", subject: "Merge pull request #14 from x/ws/e", order: 4 },
];
const cands = g.candidates(merges);
check(cands.length === 4 && cands[0].sha === "m1111111", `candidates: ${cands.map((c) => c.sha)}`);

// Bisect: #13 broke it, so m3 and m4 fail; the culprit is m3 with log2 tests.
const broken = new Set(["m3333333", "m4444444"]);
const b = await g.bisect(cands, async (c) => broken.has(c.sha));
check(b.culprit?.sha === "m3333333", `bisect: expected m3, got ${b.culprit?.sha}`);
check(b.tested.length <= 2, `bisect tested ${b.tested.length} candidates, expected at most 2`);
const one = await g.bisect([cands[3]], async () => true);
check(one.culprit?.sha === "m4444444" && one.tested.length === 0, "bisect of one candidate needs no rerun");
check((await g.bisect([], async () => true)).culprit === null, "bisect of nothing blames nothing");

// Flakes: only a failure that reproduces counts.
check(g.judgeRecheck(true, false) === "flake", "a failure that passes on recheck is a flake");
check(g.judgeRecheck(true, true) === "red", "a failure that reproduces is red");
check(g.judgeRecheck(false, false) === "green", "green is green");

// Environmental exits: a logoff/shutdown kill (0x40010004 = 1073807364), Ctrl+C, a budget kill: never a recheck or a revert.
check(g.isEnvExit(1073807364) && g.isEnvExit(3221225786) && g.isEnvExit(124) && g.isEnvExit(null), "NTSTATUS, budget-kill and missing exit codes are environmental");
check(!g.isEnvExit(1) && !g.isEnvExit(2) && !g.isEnvExit(0), "ordinary failing exit codes are the commit's");

// Failing tests from real output lines.
const ft = g.failingTests("12:28:33.241 SCRIPT       : [LFTEST] FAIL LF_Test_ShopBuy (2 assertions)\nFAIL  LF_Test_Other: boom\n[LFTEST] START LF_Test_X");
check(ft.join() === "LF_Test_ShopBuy,LF_Test_Other", `failingTests: ${ft}`);
// The shape of the red at b622741c (2026-11-17): the compact suite output kept the assertion line and dropped the
// FAIL header, so the guard read no test name and its bisect could not act. The test sources name the owner.
const orphan = [
  "test-e2e: SUMMARY tier=only passed=670 failed=1 total=671 (30 s)",
  "  16:16:47.954 SCRIPT       : [LFTEST]   - no sign near: the report names the location (expected '#AR-MapLocation_Arleville', got 'free')",
  "test-e2e: FAILED (1 failing test(s))",
  "SUMMARY test fail tier=all legs=2 passed=684 failed=1 total=685 worlds=GridWorld+LF_Everon1989_Test",
].join("\n");
const sources = [
  { file: "LF_Test_DispatchNamesPlace.c", text: 'ctx.EqualStr(locationName, DescribePlace(beside), "no sign near: the report names the location");' },
  { file: "LF_Test_Other.c", text: 'ctx.True(x, "something else");' },
];
check(g.failingTests(orphan).length === 0, "with no test sources an orphan assertion names nothing");
check(g.failingTests(orphan, sources).join() === "LF_Test_DispatchNamesPlace", `an assertion line with no FAIL header is named by its owner: ${g.failingTests(orphan, sources)}`);
check(g.failingTests("[LFTEST] FAIL LF_Test_Other\n[LFTEST]   - no sign near: the report names the location", sources).join() === "LF_Test_Other", "an assertion under a FAIL header stays with that header's test");

// Revert refusals.
const st = { reverted: [{ pr: 12 }] };
check(g.revertRefusal(cands[2], st) === null, "a normal PR merge may be reverted");
check(/revert/.test(g.revertRefusal({ sha: "r0000000", subject: "Merge pull request #20 from x/ws/revert-pr13" }, st) ?? ""), "a revert merge must never be reverted");
check(/revert/.test(g.revertRefusal({ sha: "r0000001", subject: "Revert #13 (main guard: LF_Test_X)" }, st) ?? ""), "a revert commit must never be reverted");
check(/before the guard/.test(g.revertRefusal(merges[0], st) ?? ""), "a merge from before the guard started must never be reverted");
check(/already/.test(g.revertRefusal(cands[1], st) ?? ""), "a PR is reverted at most once");
check(/not a PR/.test(g.revertRefusal({ sha: "d0000000", subject: "Merge branch 'x'" }, st) ?? ""), "a non-PR merge is not reverted");
check(g.revertTitle(13, "LF_Test_ShopBuy") === "Revert #13 (main guard: LF_Test_ShopBuy)", "revert title");
check(g.issuesOf("Refs #1512\nCloses #7, fixes #7").join() === "1512,7", "issuesOf");

// Status line.
check(/^MAIN GUARD watching \(pid 42\)/.test(g.statusLine({ lastGreen: "abcdef0123" }, { alive: 42, now: T })), "status watching");
check(/not running/.test(g.statusLine({}, { alive: null })), "status not running");
check(/off/.test(g.statusLine({}, { alive: 1, off: true })), "status off");

// The failing-test-set bisect (#2052): a commit is red only when it fails one of the recorded tests.
{
  // Today's set is {A, B}. #11 broke an unrelated test X (fixed later, still failing when probed); #13 broke A and B.
  const S = ["LF_Test_A", "LF_Test_B"];
  const probeOf = (table) => async (c) => table[c.sha] ?? [];
  const HEAD = cands[3].sha; // m4444444, the red head

  // Same set: the earliest merge that fails it is blamed, not the earliest red one.
  const same = await g.bisectBySet(cands, S, probeOf({ m1111111: ["LF_Test_X"], m2222222: ["LF_Test_X"], m3333333: S, m4444444: S }), { head: HEAD });
  check(same.culprit?.sha === "m3333333" && same.stop === null, `same set: blame m3, got ${same.culprit?.sha} (${same.stop})`);
  check(g.revertRefusal(same.culprit, {}) === null, "same set: the blamed PR may be reverted");
  // The old red/green bisect, with X counted as red, blamed #11 (the 2026-11-16 bug).
  const old = await g.bisect(cands, async (c) => ["m1111111", "m2222222", "m3333333", "m4444444"].includes(c.sha));
  check(old.culprit?.sha === "m1111111", "red/green alone blames the earliest red merge (what the set match fixes)");

  // A different set: nothing fails the recorded tests at the newest merge, so nothing is blamed or reverted.
  const diff = await g.bisectBySet(cands, S, probeOf({ m1111111: ["LF_Test_X"], m2222222: ["LF_Test_X"], m3333333: ["LF_Test_X"], m4444444: ["LF_Test_Y"] }), { head: HEAD });
  check(diff.culprit === null && /mixed causes/.test(diff.stop ?? ""), `different set: no blame, got ${diff.culprit?.sha} (${diff.stop})`);
  // A failure with no parseable test line (a missing test, an exit with no FAIL line) is no hit either.
  const none = await g.bisectBySet(cands, S, probeOf({}), { head: HEAD });
  check(none.culprit === null && !!none.stop, "no parsed failures at the newest merge: no blame");
  // The head is not the last merge: the last merge is probed first, and a green one stops the bisect there.
  const later = await g.bisectBySet(cands, S, probeOf({ m4444444: [] }), { head: "headnotmerge" });
  check(later.culprit === null && !!later.stop, "head after the last merge: last merge probed, green for the set, no blame");

  // Mixed causes: #12 breaks A; #14 breaks B too. No single merge explains {A, B}: stop, never revert.
  const mixed = await g.bisectBySet(cands, S, probeOf({ m2222222: ["LF_Test_A"], m3333333: ["LF_Test_A"], m4444444: S }), { head: HEAD });
  check(mixed.culprit?.sha === "m2222222" && /^mixed causes/.test(mixed.stop ?? "") && mixed.missing.join() === "LF_Test_B", `mixed causes: stop, got ${mixed.culprit?.sha} (${mixed.stop})`);
  check(/LF_Test_B/.test(mixed.stop ?? "") && !/LF_Test_A\b.*LF_Test_A\b.*fail from/.test(""), "mixed causes line names the unexplained test");
  // Two candidates: the earlier one fails only part of the set, so it is not the whole explanation: stop.
  const m2 = await g.bisectBySet([cands[2], cands[3]], S, probeOf({ m3333333: ["LF_Test_A"], m4444444: S }), { head: HEAD });
  check(!!m2.stop && m2.culprit?.sha === "m3333333", "a culprit that fails only part of the set: stop, no revert");
  // A single candidate is probed once; failing the whole set it is blamed.
  const solo = await g.bisectBySet([cands[3]], S, probeOf({ m4444444: S }), { head: HEAD });
  check(solo.culprit?.sha === "m4444444" && solo.stop === null, "one candidate failing the set: blamed");
  check((await g.bisectBySet([], S, probeOf({}))).stop !== null, "no candidates: nothing to blame");
  check(g.setHits(S, ["LF_Test_B", "LF_Test_Z", "LF_Test_B"]).join() === "LF_Test_B", "setHits keeps recorded tests only");
  // FAIL lines recorded beside the names.
  const fl = g.failLines("ok\n12:28:33.241 SCRIPT : [LFTEST] FAIL LF_Test_ShopBuy (2 assertions)\nFAIL  LF_Test_Other: boom\n[LFTEST] START LF_Test_X");
  check(fl.length === 2 && /LF_Test_ShopBuy/.test(fl[0]), `failLines: ${fl}`);
}

// Wiring: the worker bisects the suite by the recorded set, stops on a mixed-causes verdict without reverting, and
// posts the reason as the commit status description.
{
  const w = readFileSync(new URL("../main-guard.mjs", import.meta.url), "utf8");
  check(w.includes("bisectBySet(list, failing,"), "worker: the suite bisect matches the recorded failing tests");
  check(w.includes("const refusal = b.stop ?? mg.revertRefusal("), "worker: a mixed-causes stop is a refusal (no revert)");
  check(w.includes('note: b.stop ? refusal : ""') && w.includes("postStatus(dir, sha, status, extra?.step, extra?.note)"), "worker: the stop reason goes on the GitHub status");
  const d = pin.guardToGh("red", "suite", "mixed causes: LF_Test_B fail from another merge")[0];
  check(d.state === "failure" && d.context === pin.SUITE_CTX && /^red: suite failed; mixed causes/.test(d.description) && d.description.length <= 140, `status description: ${d.description}`);
  check(pin.guardToGh("red", "suite")[0].description === "red: suite failed", "no note: the old description");
}

if (fails.length) {
  for (const f of fails) console.log(`FAIL  ${f}`);
  process.exit(1);
}
console.log("PASS  main-guard: batching, candidates, bisect, flakes, failing tests, revert refusals, status line");
