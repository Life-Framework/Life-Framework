// Proves the main guard's rules (tools/lib/main-guard.mjs, docs/worktrees.md "Main guard"): batching, the merges it
// may blame, the bisect, the flake rule, never reverting a revert or a pre-start merge, and the status line.
// inputs: tools/lib/main-guard.mjs
// red-proof: with revertRefusal's isRevert check removed this printed "a revert merge must never be reverted";
//   with bisect's `hi = mid` swapped for `lo = mid` the run never finished (killed by `timeout 10`).
import * as g from "../lib/main-guard.mjs";

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

// Failing tests from real output lines.
const ft = g.failingTests("12:28:33.241 SCRIPT       : [LFTEST] FAIL LF_Test_ShopBuy (2 assertions)\nFAIL  LF_Test_Other: boom\n[LFTEST] START LF_Test_X");
check(ft.join() === "LF_Test_ShopBuy,LF_Test_Other", `failingTests: ${ft}`);

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

if (fails.length) {
  for (const f of fails) console.log(`FAIL  ${f}`);
  process.exit(1);
}
console.log("PASS  main-guard: batching, candidates, bisect, flakes, failing tests, revert refusals, status line");
