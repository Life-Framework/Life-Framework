// Proves the test-shard split and merge (tools/lib/test-shards.mjs) on fixtures. Does not boot the game.
// inputs: tools/lib/test-cache.mjs, tools/lib/test-shards.mjs, tools/validation/test-shards-selftest.mjs, tools/lib/test-world.mjs, addons/**
// red-proof 2026-11-18 (#2789, a real sharded run: "shard 1: ran 530 of 529 test(s) (a name matched twice or not at all)" from the runner's own suite/no-leftover-fixtures check): with the strict count check back, "a boot's suite check is not a name matched twice" fails; with the pack job removed from runPasses, "the pack leg is one more job" fails.
// red-proof 2026-11-15 (#1703 gate "shard 1: no SUMMARY (exit 0); its 568 test(s) have no verdict", a re-queued gate answered by the pass cache): with parseShardOutput not reading the CACHED line "a CACHED pass line is the slice's verdict (summary=false 0/0)".
import { rmSync } from "node:fs";
import { join } from "node:path";
import { fileURLToPath } from "node:url";
import * as s from "../lib/test-shards.mjs";
import * as tc from "../lib/test-cache.mjs";
import * as tw from "../lib/test-world.mjs";
import { scratchDir } from "../lib/scratch.mjs";

const failures = [];
const check = (ok, msg) => {
  if (!ok) failures.push(msg);
};

// --- parse the real registry
const here = fileURLToPath(new URL(".", import.meta.url));
const root = join(here, "..", "..");
const real = s.readRegistry(root);
check(real.length > 100, `the registry parses (${real.length} tests)`);
check(new Set(real.map((t) => t.cls)).size === real.length, "registry class names are unique");
check(!real.some((t) => t.tier === "PERF"), "a PERF test is not in the shard registry (`--tier all` does not run it, and a slice would run it at the lenient settings)");
{
  // #2789: a slice's legs each print an [LFTEST] SUMMARY; the slice count is their sum, never the last leg alone.
  const twoLegs = "test: leg 1/2 world GridWorld\n01:08:47.471 SCRIPT       : [LFTEST] SUMMARY tier=only passed=878 failed=1 total=879 skipped=0\ntest-e2e: SUMMARY tier=only passed=878 failed=1 total=879 (23 s)\ntest: leg 2/2 world pack\n01:09:09.768 SCRIPT       : [LFTEST] SUMMARY tier=only passed=19 failed=0 total=19 skipped=0\ntest-e2e: SUMMARY tier=only passed=19 failed=0 total=19 (7 s)\nSUMMARY test fail tier=all legs=2 passed=897 failed=1 total=898\n";
  const pl = s.parseShardOutput(twoLegs);
  check(pl.total === 898 && pl.passed === 897 && pl.failed === 1 && pl.seconds === 30, `parseShardOutput sums the core and pack legs (${pl.total}/898)`);
  const mm = s.mergeShards([{ index: 1, expected: 898, code: 1, parsed: pl, seconds: 30 }]);
  check(!mm.notes.some((n) => /matched twice/.test(n)), `a two-leg slice of 898 is not "ran 19 of 879" (${mm.notes.join("; ")})`);
}
{
  const plan = s.planShards(real, 4, {});
  check(plan.flat().length === real.length && new Set(plan.flat()).size === real.length, "the real registry splits into 4 slices covering every test once");
}

// --- the split
const tests = [];
for (let i = 0; i < 40; i++) tests.push({ cls: `LF_Test_L${i}`, tier: "LOGIC" });
for (let i = 0; i < 20; i++) tests.push({ cls: `LF_Test_W${i}`, tier: "WORLD" });
for (let i = 0; i < 6; i++) tests.push({ cls: `LF_Test_P${i}`, tier: "PERSISTENCE" });
tests.splice(10, 0, { cls: "LF_Test_Heavy", tier: "WORLD" });
const times = { LF_Test_Heavy: 2.0 };
for (const n of [1, 2, 3, 4]) {
  const plan = s.planShards(tests, n, times);
  const flat = plan.flat();
  check(plan.length === n, `${n} shards give ${plan.length} slices`);
  check(flat.length === tests.length && new Set(flat).size === tests.length, `${n} shards cover every test exactly once`);
  const per = plan.filter((p) => p.some((c) => c.startsWith("LF_Test_P")));
  check(per.length === 1 && tests.filter((t) => t.tier === "PERSISTENCE").every((t) => per[0].includes(t.cls)), `${n} shards keep PERSISTENCE tests together`);
  const rank = new Map(tests.map((t, i) => [t.cls, i]));
  check(plan.every((p) => p.every((c, i) => i === 0 || rank.get(p[i - 1]) < rank.get(c))), `${n} shards keep registration order inside a slice`);
}
{
  const plan = s.planShards(tests, 2, times);
  const heavy = plan.find((p) => p.includes("LF_Test_Heavy"));
  check(heavy.length === 1 || heavy.length < tests.length / 2, `the 2.0 s test does not share a slice with half the suite (slice has ${heavy.length})`);
  const w = (p) => p.reduce((a, c) => a + (c === "LF_Test_Heavy" ? 2 : s.DEFAULT_WEIGHT[tests.find((t) => t.cls === c).tier]), 0);
  const [a, b] = plan.map(w);
  check(heavy.length === 1 && Math.abs(a - 2) < 1e-9 && b < 2, `the heavy test is alone against the rest (${a.toFixed(3)} vs ${b.toFixed(3)})`);
  const even = s.planShards(tests.filter((t) => t.cls !== "LF_Test_Heavy"), 2, {}).map(w);
  check(Math.abs(even[0] - even[1]) < 0.02, `without the heavy test 2 shards balance by weight (${even[0].toFixed(3)} vs ${even[1].toFixed(3)})`);
  const flat = s.planShards(tests.filter((t) => t.cls !== "LF_Test_Heavy"), 3, {});
  const ws = flat.map((p) => p.reduce((a2, c) => a2 + s.DEFAULT_WEIGHT[tests.find((t) => t.cls === c).tier], 0));
  check(Math.max(...ws) - Math.min(...ws) <= s.DEFAULT_WEIGHT.PERSISTENCE * 6, "no history: slices stay balanced by tier weight");
}
check(s.planShards(tests.slice(0, 2), 4).length === 2, "more shards than atoms gives one slice per atom");
check(s.planShards([], 3).length === 1 && s.planShards([], 3)[0].length === 0, "an empty registry gives one empty slice");

// --- auto
const lanes = (...l) => l.map(([local, free]) => ({ local, free }));
check(s.pickShardCount("auto", lanes([true, 1], [false, 8]), 1) === 4, "auto: capped at 4");
check(s.pickShardCount("auto", lanes([true, 1], [false, 8]), 2) === 4, "auto: two passes with 9 free lanes -> 4 each");
check(s.pickShardCount("auto", lanes([true, 2], [false, 3]), 2) === 2, "auto: this PC counts one lane (1 + 3 over 2 passes)");
check(s.pickShardCount("auto", lanes([true, 0], [false, 0]), 1) === 1, "auto: no free lane gives 1");
check(s.pickShardCount("auto", [], 2) === 1, "auto: no machine gives 1");
check(s.pickShardCount("3", [], 1) === 3, "an explicit count is kept");
check(s.pickShardCount("auto", lanes([false, 8]), 1, 4) === 1, "auto: a 4 s suite is not worth a slice (a boot costs more)");
check(s.pickShardCount("auto", lanes([false, 8]), 1, 50) === 3, "auto: 50 s of tests feeds 3 slices");
check(s.pickShardCount("auto", lanes([false, 8]), 1, 1000) === 4, "auto: a long suite still stops at 4");
check(s.pickShardCount("4", [], 1, 4) === 4, "an explicit count ignores the estimate");
check(Math.abs(s.estimateSeconds([{ cls: "A", tier: "LOGIC" }, { cls: "B", tier: "WORLD" }], { A: 1 }) - 1.015) < 1e-9, "estimateSeconds uses history, else the tier default");
let threw = false;
try {
  s.pickShardCount("zero", [], 1);
} catch {
  threw = true;
}
check(threw, "a bad --shards value throws");

// --- timing history
const log = [
  "16:28:21.000 SCRIPT       : [LFTEST] SUITE START tier=all",
  "16:28:21.000 SCRIPT       : [LFTEST] START a",
  "16:28:21.250 SCRIPT       : [LFTEST] PASS a (2 assertions)",
  "16:28:21.250 SCRIPT       : [LFTEST] START b",
  "16:28:23.250 SCRIPT       : [LFTEST] PASS b (1 assertions)",
  "16:28:23.250 SCRIPT       : [LFTEST] START c",
  "16:28:23.300 SCRIPT       : [LFTEST] SUMMARY tier=only passed=3 failed=0 total=3 skipped=0",
].join("\n");
const t = s.parseTimes(log, ["LF_Test_A", "LF_Test_B", "LF_Test_C"]);
check(t && t.LF_Test_A === 0.25 && t.LF_Test_B === 2 && t.LF_Test_C === 0.05, `parseTimes reads gaps (${JSON.stringify(t)})`);
check(s.parseTimes(log, ["LF_Test_A", "LF_Test_B"]) === null, "parseTimes refuses a log whose START count differs");
const dir = scratchDir("lf-shards-");
try {
  const file = s.historyFile(dir);
  s.saveHistory(file, { LF_Test_A: 1 });
  s.saveHistory(file, { LF_Test_A: 3, LF_Test_B: 2 });
  const h = s.loadHistory(file);
  check(h.LF_Test_A === 2 && h.LF_Test_B === 2, `history blends old and new (${JSON.stringify(h)})`);
  check(Object.keys(s.loadHistory(join(dir, "missing.json"))).length === 0, "a missing history is empty");
  check(s.takeLocal(dir, { pid: 111, alive: () => true }) === true, "first slice takes the local lock");
  check(s.takeLocal(dir, { pid: 222, alive: () => true }) === false, "second slice does not while the first lives");
  check(s.takeLocal(dir, { pid: 333, alive: (p) => p !== 111 }) === true, "a dead holder's lock is taken over");
  s.dropLocal(dir, 111);
  check(s.takeLocal(dir, { pid: 444, alive: () => true }) === false, "dropLocal only drops its own lock");
  s.dropLocal(dir, 333);
  check(s.takeLocal(dir, { pid: 555, alive: () => true }) === true, "dropLocal frees the lock");
} finally {
  rmSync(dir, { recursive: true, force: true });
}

// --- shard transcripts and the merge
const out = (tier, passed, failed, total, extra = []) =>
  [
    "pool: test only runs on lf-cloud C:/x, slot server-test-1; waited 0.1 s",
    ...extra,
    `12:00:00.000 SCRIPT       : [LFTEST] SUMMARY tier=${tier} passed=${passed} failed=${failed} total=${total} skipped=0`,
    "test-e2e: SUMMARY tier=only passed=1 failed=0 total=1 (14 s)",
  ].join("\n");
const okOut = out("only", 3, 0, 3);
const failOut = out("only", 1, 1, 2, ["12:00:00.000 SCRIPT       : [LFTEST] FAIL money/pay", "12:00:00.000 SCRIPT       : [LFTEST]   - expected 5 got 4"]);
const p = s.parseShardOutput(failOut);
check(p.summary && p.passed === 1 && p.failed === 1 && p.total === 2 && p.seconds === 14, "parseShardOutput reads the SUMMARY");
check(p.fails.length === 1 && p.fails[0].name === "money/pay" && p.fails[0].lines[0] === "expected 5 got 4", "parseShardOutput reads a FAIL and its detail");
check(p.where.startsWith("lf-cloud"), `parseShardOutput reads the machine (${p.where})`);

const mk = (index, expected, code, text, seconds) => ({ index, expected, code, parsed: s.parseShardOutput(text), seconds });
let m = s.mergeShards([mk(1, 3, 0, okOut, 14), mk(2, 3, 0, okOut, 20)]);
check(m.code === 0 && m.passed === 6 && m.total === 6 && m.wall === 20, "all green merges to exit 0 with summed counts and the slowest wall");
m = s.mergeShards([mk(1, 3, 0, okOut, 14), mk(2, 2, 1, failOut, 9)]);
check(m.code === 1 && m.failed === 1 && m.fails.length === 1 && m.fails[0].shard === 2, "a failing test gives exit 1 and is listed with its shard");
const lines = s.mergedLines(m, { tier: "all" });
check(lines.some((l) => l.startsWith("[LFTEST] FAIL money/pay")) && lines.some((l) => l.includes("expected 5 got 4")), "mergedLines lists every FAIL with its detail");
check(lines.some((l) => /^test-e2e: SUMMARY tier=all passed=4 failed=1 total=5 /.test(l)), "mergedLines prints the single-run SUMMARY shape");
m = s.mergeShards([mk(1, 3, 0, okOut, 14), mk(2, 3, 2, "boot failed\n", 5)]);
check(m.code === 2 && m.failed === 3 && m.notes.length === 1, "a slice with no SUMMARY counts its tests as failed, exit 2");
m = s.mergeShards([mk(1, 3, 0, okOut, 14), mk(2, 3, 3, "pool: no slot\n", 1)]);
check(m.code === 3, "a slice that never ran gives exit 3");
m = s.mergeShards([mk(1, 4, 0, okOut, 14)]);
check(m.code === 1 && m.notes.some((n) => n.includes("ran 3 of 4")), "a slice that ran the wrong number of tests is refused");
m = s.mergeShards([mk(1, 3, 1, okOut, 14)]);
check(m.code === 1, "a non-zero exit with a green SUMMARY still fails");
m = s.mergeShards([mk(1, 3, 2, failOut, 9), mk(2, 3, 1, okOut, 9)]);
check(m.code === 1, "a failed test outranks a missing verdict");

// --- the passes run at the same time
{
  let running = 0;
  let peak = 0;
  const passes = [
    { name: "normal", classes: [["a", "b", "c"], ["d", "e", "f"]] },
    { name: "core", classes: [["a", "b", "c"], ["d", "e", "f"]] },
  ];
  const results = await s.runPasses(passes, async (job) => {
    running++;
    peak = Math.max(peak, running);
    await new Promise((r) => setTimeout(r, 15));
    running--;
    if (job.pass.name === "core" && job.index === 2) throw new Error("ssh gone");
    return { code: 0, text: okOut, seconds: 1 };
  });
  check(peak === 4, `every slice of both passes runs at once (peak ${peak})`);
  check(results.length === 2 && results[0].merged.code === 0 && results[0].merged.total === 6, "the normal pass merges green");
  check(results[1].merged.code === 3 && results[1].merged.notes.length === 1, "a slice that cannot start fails its pass with exit 3");
}

// A slice answered from the pass cache prints no [LFTEST] SUMMARY, only the CACHED line: that is a verdict (a re-queued gate re-runs identical inputs).
{
  const cache = tc.reuseLine({ at: "2026-11-15T13:32:51.327Z", machine: "lf-cloud", worktree: "x", passed: 568, total: 568, tier: "all", key: "f6e1d38b8136aa" });
  const cp = s.parseShardOutput(cache);
  check(cp.summary && cp.passed === 568 && cp.failed === 0 && cp.total === 568, `a CACHED pass line is the slice's verdict (summary=${cp.summary} ${cp.passed}/${cp.total})`);
  const m = s.mergeShards([{ index: 1, expected: 568, code: 0, parsed: cp, seconds: 1 }]);
  check(m.code === 0 && m.passed === 568 && !m.notes.length, `a cached slice merges as a pass (code ${m.code}, ${m.notes.join("; ")})`);
  check(!s.parseShardOutput(tc.reuseLine({ at: "t", passed: 1, tier: "all", key: "k" })).summary, "a cached line with no counts is no verdict");
  const cm = s.mergeShards([{ index: 1, expected: 567, code: 0, parsed: cp, seconds: 1 }]);
  check(cm.code === 0 && cm.suiteChecks === 1, `a cached slice carries its past run's suite check (${cm.notes.join("; ")})`);
}

// --- #2789: the pack leg rides the shards; a mixed core/pack suite is counted once
{
  const pack = tw.readPackTests(root);
  const packSet = new Set(pack.map((x) => x.cls));
  check(pack.length > 5, `the pack tests parse from their [LF_TestInfo(pack: true)] (${pack.length})`);
  check(!real.some((x) => packSet.has(x.cls)), "no pack test is in the core registry a shard plan splits");
  const pl = tw.packLeg({ tier: "all", pack });
  check(pl && pl.classes.length === pack.filter((x) => x.tier !== "PERF").length && pl.classes.every((c) => packSet.has(c)), "the pack leg of a tier-all run is exactly the pack tests");
  check(tw.packLeg({ tier: "all", pack, coreOnly: true }) === null, "a core-only pass has no pack leg");
  for (const n of [1, 2, 3, 4]) {
    const slices = [...s.planShards(real, n, {}), pl.classes];
    const flat = slices.flat();
    check(flat.length === real.length + pl.classes.length && new Set(flat).size === flat.length, `${n} core slice(s) + the pack leg run every test exactly once (${flat.length})`);
  }
  // A fake run over a mixed fixture: each boot prints one more test (the suite check), the pack job runs on the pack world.
  const core = [];
  for (let i = 0; i < 10; i++) core.push({ cls: `LF_Test_C${i}`, tier: i % 2 ? "WORLD" : "LOGIC" });
  const fixturePack = [{ cls: "LF_Test_EveronA", tier: "WORLD" }, { cls: "LF_Test_EveronB", tier: "PERSISTENCE" }];
  const world = "{PACK}Missions/pack.conf";
  const plan = s.planShards(core, 2, {});
  const pass = { name: "normal", classes: plan, pack: { world, classes: fixturePack.map((x) => x.cls) } };
  const seen = [];
  const [res] = await s.runPasses([pass], async (job) => {
    seen.push(job);
    const n = job.classes.length + 1; // + suite/no-leftover-fixtures
    return { code: 0, text: out("only", n, 0, n), seconds: 1 };
  });
  check(seen.length === 3 && seen.filter((j) => j.pack).length === 1, `the pack leg is one more job beside the 2 slices (${seen.length} jobs)`);
  const pj = seen.find((j) => j.pack);
  check(pj && pj.world === world && pj.classes.join() === "LF_Test_EveronA,LF_Test_EveronB" && pj.count === 3 && pj.index === 3, "the pack job carries the pack world and exactly the pack classes");
  const ran = seen.flatMap((j) => j.classes);
  check(ran.length === 12 && new Set(ran).size === 12, "slices and pack job together run each of the 12 tests once");
  check(res.merged.code === 0 && res.merged.notes.length === 0, `a boot's suite check is not a name matched twice (${res.merged.notes.join("; ")})`);
  check(res.merged.suiteChecks === 3 && res.merged.total === 15, `the merge counts the 3 suite checks (total ${res.merged.total})`);
  check(s.mergedLines(res.merged).some((l) => /suite check\(s\) counted/.test(l)), "the merged SUMMARY line says the suite checks are in the total");
  // A pack job that ran nothing, and a slice that ran a test twice, still fail the pass.
  const bad = (job) => (job.pack ? { code: 0, text: out("only", 1, 0, 1), seconds: 1 } : { code: 0, text: out("only", job.classes.length + 3, 0, job.classes.length + 3), seconds: 1 });
  const [r2] = await s.runPasses([pass], async (job) => bad(job));
  check(r2.merged.code === 1 && r2.merged.notes.some((n) => /pack leg\): ran 1 of 2/.test(n)), `a pack job that ran 1 of 2 tests is named (${r2.merged.notes.join("; ")})`);
  check(r2.merged.notes.some((n) => /^shard 1: ran \d+ of \d+/.test(n)), "a slice that ran 3 more tests than it was given is a duplicate, not a suite check");
  // Slack is one per boot: two boots (a pinned test leg) may add two, not three.
  const twoBoots = [out("only", 4, 0, 4), out("only", 3, 0, 3)].join("\n");
  check(s.mergeShards([mk(1, 5, 0, twoBoots, 5)]).code === 0, "a slice of two boots may add two suite checks");
  check(s.mergeShards([mk(1, 4, 0, twoBoots, 5)]).code === 1, "a slice of two boots may not add three");
}

if (failures.length) {
  console.log("test-shards-selftest: FAIL");
  for (const f of failures) console.log(`  ${f}`);
  process.exit(1);
}
console.log("test-shards-selftest: OK (balanced split, persistence together, auto count, history, merge and exit codes, passes concurrent)");
