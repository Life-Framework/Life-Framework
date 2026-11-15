// Proves the test-shard split and merge (tools/lib/test-shards.mjs) on fixtures. Does not boot the game.
// inputs: tools/lib/test-shards.mjs, tools/validation/test-shards-selftest.mjs, addons/LifeFramework/Scripts/Game/Tests/LF_TestRegistrations.generated.c
import { mkdtempSync, rmSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { fileURLToPath } from "node:url";
import * as s from "../lib/test-shards.mjs";

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
check(s.parseRegistry("manager.Register(new LF_Test_P(), LF_TestTier.PERF);\nmanager.Register(new LF_Test_Q(), LF_TestTier.LOGIC);").length === 2, "parseRegistry reads the PERF tier");
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
const dir = mkdtempSync(join(tmpdir(), "lf-shards-"));
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

if (failures.length) {
  console.log("test-shards-selftest: FAIL");
  for (const f of failures) console.log(`  ${f}`);
  process.exit(1);
}
console.log("test-shards-selftest: OK (balanced split, persistence together, auto count, history, merge and exit codes, passes concurrent)");
