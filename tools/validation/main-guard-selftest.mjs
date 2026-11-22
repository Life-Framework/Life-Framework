// Proves the main guard's rules (tools/lib/main-guard.mjs, docs/worktrees.md "Main guard"): batching, the merges it
// may blame, the bisect, the flake rule, never reverting a revert or a pre-start merge, and the status line.
// inputs: tools/lib/main-guard.mjs, tools/main-guard.mjs
// red-proof: with revertRefusal's isRevert check removed this printed "a revert merge must never be reverted";
//   with bisect's `hi = mid` swapped for `lo = mid` the run never finished (killed by `timeout 10`).
// red-proof #2052: with setHits returning every failed test (red/green again) this printed "FAIL same set: blame m3, got
//   m1111111" and "FAIL different set: no blame".
// red-proof 2026-11-22 (newest first, revert caution): with newestFirst's `p.verdict === "ok"` return removed this printed "FAIL
//   newer head passes: fixed, old head never judged"; with isLargePr's `>` flipped to `<` "FAIL a large merge is not reverted for a suite
//   red"; with newerFixes returning null "FAIL newerFixes: a newer head that passes the set".
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

// #3586: the recheck and every bisect probe run with the shared rdb cache off and the checkout's rdb dropped.
check(g.RECHECK_ENV.LF_RDB_CACHE === "0", "RECHECK_ENV must switch the shared rdb cache off");
{
  const src = readFileSync(new URL("../main-guard.mjs", import.meta.url), "utf8");
  const calls = src.split(/\r?\n/).filter((l) => /await runCli\(/.test(l) && /(recheck|bisect-|probe-)/.test(l));
  check(calls.length === 4 && calls.every((l) => /mg\.RECHECK_ENV, true\)/.test(l)), `the recheck, the newest-head probe and both bisect probes must pass RECHECK_ENV and drop the rdb: ${calls.length} call(s)`);
}

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

// #2812: a pre-move e2e red whose plain e2e passes is a missing LF_E2E guard, not a broken save: fix card, no revert.
{
  const culprit = { sha: "n1111111", subject: "Merge pull request #2798 from Life-Framework/ws/slice-2798" };
  check(g.needsSkip("pre-move e2e", 0) === true, "needs-skip: pre-move red, plain e2e green");
  check(g.needsSkip("pre-move e2e", 1) === false, "needs-skip: plain e2e red too is a real failure");
  check(g.needsSkip("suite", 0) === false, "needs-skip: only the pre-move step");
  check(g.revertRefusal(culprit, {}, { step: "pre-move e2e" }) === null, "pre-move red alone: the blamed PR may still be reverted");
  check(/never reverted/.test(g.revertRefusal(culprit, {}, { step: "pre-move e2e", needsSkip: true }) ?? ""), "needs-skip: no revert, a fix card instead");
  check(g.fixCardInstead(culprit, "pre-move e2e", { needsSkip: true }) !== null && g.fixCardInstead(culprit, "suite", { needsSkip: true }) === null, "needs-skip changes only the pre-move step");
  const w = readFileSync(new URL("../main-guard.mjs", import.meta.url), "utf8");
  check(w.includes("mg.PLAIN_E2E") && w.includes("{ step: step.name, needsSkip }"), "worker: runs the plain e2e after a reproduced pre-move red and passes needsSkip to the revert rule");
}

// Newest first (2026-11-22): main went red on two suite checks and the guard re-reproduced and bisected one old head after
// another for over an hour while newer heads that already held the fix waited. A newer compiled-ok head runs the failing
// checks first; when they pass the old head is neither rechecked, bisected nor reverted.
{
  const S = ["LF_Test_A", "LF_Test_B"];
  const mkRun = (calls, value = "done") => async (cur, set, guard) => { calls.push({ cur, set }); return value; };
  const newerOf = (map) => (sha) => map[sha] ?? null;
  const probeOf = (table, seen = []) => async (sha, set) => { seen.push(sha); return table[sha] ?? { verdict: "ok", failing: [] }; };

  // Nothing newer: the judgement runs at the red head, nothing is probed.
  {
    const calls = []; const seen = [];
    const r = await g.newestFirst({ head: "h1", set: S, newerThan: newerOf({}), probe: probeOf({}, seen), run: mkRun(calls) });
    check(r.kind === "done" && r.head === "h1" && calls.length === 1 && calls[0].cur === "h1" && seen.length === 0, `no newer head: judged at h1, got ${JSON.stringify(r)}`);
  }
  // A newer head passes the failing tests: fixed. No run at the old head (no recheck, no bisect, no revert).
  {
    const calls = []; const seen = [];
    const r = await g.newestFirst({ head: "h1", set: S, newerThan: newerOf({ h1: "h5" }), probe: probeOf({ h5: { verdict: "ok", failing: [] } }, seen), run: mkRun(calls) });
    check(r.kind === "fixed" && r.by === "h5" && r.from === "h1" && calls.length === 0 && seen.join() === "h5", `newer head passes: fixed, old head never judged, got ${JSON.stringify(r)} calls=${calls.length}`);
  }
  // The newer head still fails the set: it is the head to judge (recheck and bisect restart from it, with its own set).
  {
    const calls = [];
    const r = await g.newestFirst({ head: "h1", set: S, newerThan: newerOf({ h1: "h5" }), probe: probeOf({ h5: { verdict: "red", failing: ["LF_Test_A"] } }), run: mkRun(calls, "flake") });
    check(r.kind === "done" && r.head === "h5" && r.value === "flake" && calls.length === 1 && calls[0].cur === "h5" && calls[0].set.join() === "LF_Test_A", `newer head still red: judged at h5 with its set, got ${JSON.stringify(r)}`);
  }
  // Heads keep arriving: the newest one decides, each probed once.
  {
    const calls = []; const seen = [];
    const r = await g.newestFirst({ head: "h1", set: S, newerThan: newerOf({ h1: "h3", h3: "h7" }), probe: probeOf({ h3: { verdict: "red", failing: S }, h7: { verdict: "ok", failing: [] } }, seen), run: mkRun(calls) });
    check(r.kind === "fixed" && r.by === "h7" && r.from === "h3" && seen.join() === "h3,h7" && calls.length === 0, `h1 -> h3 red -> h7 green: fixed by h7, got ${JSON.stringify(r)}`);
  }
  // A newer head with no verdict (machine, lock): nothing is judged, nothing blamed.
  {
    const calls = [];
    const r = await g.newestFirst({ head: "h1", set: S, newerThan: newerOf({ h1: "h5" }), probe: probeOf({ h5: { verdict: "env", failing: [], why: "no free Workbench slot" } }), run: mkRun(calls) });
    check(r.kind === "env" && r.sha === "h5" && calls.length === 0, `newer head with no verdict: env, got ${JSON.stringify(r)}`);
  }
  // A newer head appears in the middle of the bisect: the bisect is abandoned and the newer head is tried first.
  {
    const calls = []; let arrived = false; const seen = [];
    const run = async (cur, set, guard) => {
      calls.push(cur);
      if (cur === "h1") { arrived = true; await guard(); throw new Error("the guard must have thrown Superseded"); }
      return "done";
    };
    const newerThan = (sha) => (sha === "h1" && arrived ? "h5" : null);
    const r = await g.newestFirst({ head: "h1", set: S, newerThan, probe: probeOf({ h5: { verdict: "red", failing: S } }, seen), run });
    check(r.kind === "done" && r.head === "h5" && calls.join() === "h1,h5" && seen.join() === "h5", `a head arriving mid-bisect restarts at the newest: ${JSON.stringify(r)} calls=${calls}`);
    const calls2 = []; let arrived2 = false;
    const r2 = await g.newestFirst({ head: "h1", set: S, newerThan: (sha) => (sha === "h1" && arrived2 ? "h5" : null), probe: probeOf({ h5: { verdict: "ok", failing: [] } }), run: async (cur, set, guard) => { calls2.push(cur); arrived2 = true; await guard(); return "done"; } });
    check(r2.kind === "fixed" && r2.by === "h5" && calls2.join() === "h1", `mid-bisect arrival that passes: fixed, bisect dropped, got ${JSON.stringify(r2)}`);
  }
  // A head that never stops moving cannot starve the judgement: after MAX_HOPS the current head is judged.
  {
    let n = 0; const calls = [];
    const r = await g.newestFirst({ head: "h0", set: S, newerThan: (sha) => `h${Number(sha.slice(1)) + 1}`, probe: async () => ({ verdict: "red", failing: S }), run: mkRun(calls) });
    check(r.kind === "done" && calls.length === 1 && r.head === `h${g.MAX_HOPS + 1}`, `endless newer heads: judged after MAX_HOPS, got ${JSON.stringify(r)}`);
  }
  // Before a revert: a newer head that passes the set means the fix is on main.
  {
    const probe = (v) => async () => v;
    check(await g.newerFixes({ head: "h1", set: S, newerThan: newerOf({ h1: "h5" }), probe: probe({ verdict: "ok", failing: [] }) }) === "h5", "newerFixes: a newer head that passes the set");
    check(await g.newerFixes({ head: "h1", set: S, newerThan: newerOf({ h1: "h5" }), probe: probe({ verdict: "red", failing: S }) }) === null, "newerFixes: still red there, the revert stands");
    check(await g.newerFixes({ head: "h1", set: S, newerThan: newerOf({ h1: "h5" }), probe: probe({ verdict: "env", failing: [] }) }) === null, "newerFixes: no verdict there, the revert is not blocked");
    check(await g.newerFixes({ head: "h1", set: S, newerThan: newerOf({}), probe: probe({ verdict: "ok", failing: [] }) }) === null, "newerFixes: no newer head");
  }
}

// Revert caution (2026-11-22: #4128 reverted a big feature PR whose break was stale test expectations): a merge that
// changes more than LARGE_PR_FILES files gets a fix card, never a revert, unless the compile breaks (or a stage-0
// data-safety check, which keeps its revert).
{
  const big = { sha: "b1111111", subject: "Merge pull request #4096 from Life-Framework/ws/everon-map", files: g.LARGE_PR_FILES + 1 };
  const edge = { ...big, files: g.LARGE_PR_FILES };
  const small = { ...big, files: 3 };
  check(g.LARGE_PR_FILES === 20 && g.isLargePr(big) && !g.isLargePr(edge) && !g.isLargePr(small) && !g.isLargePr({ sha: "x" }), "isLargePr: over 20 files, an unknown count is not large");
  check(/large merge \(21 files/.test(g.revertRefusal(big, {}, { step: "suite" }) ?? ""), `a large merge is not reverted for a suite red: ${g.revertRefusal(big, {}, { step: "suite" })}`);
  check(g.revertRefusal(big, {}, { step: "pre-move e2e" }) !== null && g.revertRefusal(big, {}, { step: "build" }) !== null, "a large merge is not reverted for a build or e2e red either");
  check(g.revertRefusal(edge, {}, { step: "suite" }) === null && g.revertRefusal(small, {}, { step: "suite" }) === null, "20 files and fewer revert as before");
  check(g.revertRefusal(big, {}, { step: "compile" }) === null && g.fixCardInstead(big, "compile") === null, "a compile break still reverts a large merge");
  check(g.revertRefusal(big, {}, { step: "validate" }) === null, "stage-0 data-safety reverts (step validate) keep their revert");
  check(/large merge/.test(g.fixCard(big, "suite", "LF_Test_A", "log").body), "the fix card says why it was not reverted");
  check(g.countFiles(["a.c", "docs/features/economy.md", "addons/x/Language/everonlife_localization.st", "tools/validation/record-shapes.json", "b.c"]) === 2, "countFiles leaves the append-only shared files out");
}

// Wiring (tools/main-guard.mjs).
{
  const w = readFileSync(new URL("../main-guard.mjs", import.meta.url), "utf8");
  check(/mg\.newestFirst\(\{/.test(w) && w.indexOf("mg.newestFirst(") < w.indexOf("const again = await runCli(") , "worker: newest-first runs before the recheck at the old head");
  check((w.match(/await guard\(\);/g) ?? []).length >= 3, "worker: the bisect probes and the recheck look for a newer head");
  check(/mg\.newerFixes\(\{[^}]*probe: probeOnce/.test(w) && w.indexOf("mg.newerFixes(") < w.indexOf("await revert(root, dir, b.culprit"), "worker: a newer passing head is checked just before a revert");
  check(w.includes("b.culprit.files = mg.countFiles(mergeFiles(dir, b.culprit.sha))") && w.indexOf("b.culprit.files =") < w.indexOf("mg.revertRefusal(b.culprit"), "worker: the culprit's size is known before the revert rule");
  check(/pinned: true/.test(w) && /if \(pinned\) return null;/.test(w), "worker: a requested check of an exact commit is never moved to a newer head");
  check(/verdict: "superseded"/.test(w) && /lastRunAt: null/.test(w), "worker: a superseded head records it and lets the newest head run at once");
}

if (fails.length) {
  for (const f of fails) console.log(`FAIL  ${f}`);
  process.exit(1);
}
console.log("PASS  main-guard: batching, candidates, bisect, flakes, failing tests, revert refusals, status line");
