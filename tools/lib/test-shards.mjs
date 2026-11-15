// tools/lib/test-shards.mjs - split the LF_Test registry into balanced slices, run the slices as
// separate pool jobs at the same time, and merge their verdicts into one.
//
// `tools\cli test --tier all --shards N|auto` and `ci` (so `wt gate` and `wt ship`) use it. Each slice
// is a normal `tools\cli test --only-classes <slice>` child: its own pool ticket, its own lane, and a
// remote lane carries the uncommitted state exactly as a single run does. Nothing here touches the
// pool; the pure parts (parse, plan, merge, pick N) are proved by validation/test-shards-selftest.mjs.
//
// Facts the defaults rest on (527 tests, 2026-11-14): the suite itself runs ~4 s of an ~18 s run, the rest
// is the server boot; 220 WORLD tests take 3.3 s (one of them, perf/budget-128, 2.0 s), 264 LOGIC tests 0.7 s,
// 43 PERSISTENCE tests 0.1 s. So a shard saves only the test phase; running the normal and the core-only
// pass at the same time is the bigger part of a gate.

import { existsSync, mkdirSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { writeFileAtomic } from "./atomic-file.mjs";
import { dirname, join } from "node:path";
import { readTown, testNames, withoutTown } from "./test-world.mjs";

export const MAX_AUTO_SHARDS = 4;
// A slice pays its own server boot, run folder and sync (measured 2026-11-14: 14 s on this PC, 30+ s on a
// remote lane), so `auto` only adds a slice that carries at least this many seconds of tests.
export const MIN_SHARD_SECONDS = 15;
// Seconds per test when there is no history yet (measured, see above).
export const DEFAULT_WEIGHT = { LOGIC: 0.003, WORLD: 0.015, PERSISTENCE: 0.01, PERF: 8 };
const REGISTRY = ["addons", "LifeFramework", "Scripts", "Game", "Tests", "LF_TestRegistrations.generated.c"];

// ---------------------------------------------------------------- registry and history

// [{ cls, tier }] in registration order, from the generated registry file.
export function parseRegistry(text) {
  return [...String(text).matchAll(/new (LF_Test_\w+)\(\),\s*LF_TestTier\.(\w+)/g)].map((m) => ({ cls: m[1], tier: m[2] }));
}

// The tests a `--tier all` run executes: the PERF tier runs only under `--tier perf`, so it is not in a shard plan or a timing history.
// The town tests (tools/test/town-tests.json) run only under `test --town`, so no shard plan, gate or timing history holds them (#1895).
export function readRegistry(root) {
  const all = parseRegistry(readFileSync(join(root, ...REGISTRY), "utf8")).filter((t) => t.tier !== "PERF");
  return withoutTown(all, { town: readTown(root), names: testNames(root) });
}

// Seconds per test from one console.log: the gap between a test's START line and the next one (or the
// SUMMARY). `classes` are the tests that ran, in registration order; a log whose START count differs
// (skipped tests, a crash) yields null, never a misaligned guess.
export function parseTimes(logText, classes) {
  const stamps = [];
  for (const line of String(logText).split(/\r?\n/)) {
    const m = /^(\d+):(\d+):(\d+(?:\.\d+)?)\s.*\[LFTEST\] (START|SUMMARY)\b/.exec(line);
    if (!m) continue;
    const t = Number(m[1]) * 3600 + Number(m[2]) * 60 + Number(m[3]);
    stamps.push({ t, kind: m[4] });
  }
  const starts = stamps.filter((s) => s.kind === "START").length;
  const end = stamps.findIndex((s) => s.kind === "SUMMARY");
  if (starts === 0 || starts !== classes.length || end < 0) return null;
  const out = {};
  let i = 0;
  for (let k = 0; k < end; k++) {
    const next = stamps[k + 1];
    let d = next.t - stamps[k].t;
    if (d < 0) d += 86400; // a run across midnight
    out[classes[i++]] = Math.round(d * 1000) / 1000;
  }
  return out;
}

export function historyFile(poolDir) {
  return join(poolDir, "test-times.json");
}

export function loadHistory(file) {
  try {
    const j = JSON.parse(readFileSync(file, "utf8"));
    return j && typeof j === "object" && j.times && typeof j.times === "object" ? j.times : {};
  } catch {
    return {};
  }
}

// Blend new measurements into the stored ones (half old, half new), written whole through a rename.
export function saveHistory(file, fresh) {
  const times = loadHistory(file);
  for (const [cls, s] of Object.entries(fresh)) {
    times[cls] = times[cls] === undefined ? s : Math.round(((times[cls] + s) / 2) * 1000) / 1000;
  }
  try {
    mkdirSync(dirname(file), { recursive: true });
    writeFileAtomic(file, JSON.stringify({ times }));
  } catch {}
  return times;
}

// ---------------------------------------------------------------- the split

// Balanced slices of the registry: longest-processing-time first onto the lightest slice.
// Weight is the measured time when the history has it, else the tier default. Every PERSISTENCE
// test lands in one slice (they are save round trips that may share a profile and a save folder).
// Returns arrays of class names, each in registration order; never an empty slice, never more
// slices than there are atoms.
export function planShards(tests, n, times = {}) {
  const weightOf = (t) => (typeof times[t.cls] === "number" ? times[t.cls] : (DEFAULT_WEIGHT[t.tier] ?? DEFAULT_WEIGHT.WORLD));
  const atoms = [];
  const persistence = { members: [], weight: 0, at: -1 };
  tests.forEach((t, at) => {
    if (t.tier === "PERSISTENCE") {
      if (persistence.at < 0) persistence.at = at;
      persistence.members.push(t.cls);
      persistence.weight += weightOf(t);
    } else atoms.push({ members: [t.cls], weight: weightOf(t), at });
  });
  if (persistence.members.length) atoms.push(persistence);
  const count = Math.max(1, Math.min(Math.floor(n) || 1, atoms.length));
  const bins = Array.from({ length: count }, () => ({ weight: 0, members: [] }));
  const order = [...atoms].sort((a, b) => b.weight - a.weight || a.at - b.at);
  for (const atom of order) {
    let best = bins[0];
    for (const b of bins) if (b.weight < best.weight) best = b;
    best.weight += atom.weight;
    best.members.push(...atom.members);
  }
  const rank = new Map(tests.map((t, i) => [t.cls, i]));
  return bins.map((b) => b.members.sort((x, y) => rank.get(x) - rank.get(y)));
}

// Seconds of test time in the registry: history where there is some, tier defaults elsewhere.
export function estimateSeconds(tests, times = {}) {
  let sum = 0;
  for (const t of tests) sum += typeof times[t.cls] === "number" ? times[t.cls] : (DEFAULT_WEIGHT[t.tier] ?? DEFAULT_WEIGHT.WORLD);
  return sum;
}

// How many slices for one pass: `auto` is the free test lanes (this PC counts at most one, the slices
// of one worktree share its server profile), split between the passes that run together, 1..4, and no
// more than the suite can feed (estSeconds / MIN_SHARD_SECONDS: today ~4 s of tests, so 1). `freeLanes`
// is [{ local, free }].
export function pickShardCount(spec, freeLanes, passes = 1, estSeconds = Infinity) {
  const s = String(spec ?? "1").trim().toLowerCase();
  if (s !== "auto") {
    const n = Math.floor(Number(s));
    if (!(n >= 1)) throw new Error(`--shards must be a number or auto (got '${spec}')`);
    return Math.min(n, 16);
  }
  let free = 0;
  for (const l of freeLanes) free += l.local ? Math.min(1, l.free) : l.free;
  return Math.max(1, Math.min(MAX_AUTO_SHARDS, Math.floor(free / Math.max(1, passes)), Math.floor(estSeconds / MIN_SHARD_SECONDS)));
}

// ---------------------------------------------------------------- reading a shard's transcript

// What one shard run printed: the SUMMARY of the suite and every FAIL with its detail lines.
export function parseShardOutput(text) {
  const out = { passed: 0, failed: 0, total: 0, skipped: 0, summary: false, seconds: null, fails: [], where: "" };
  let cur = null;
  for (const raw of String(text).split(/\r?\n/)) {
    const line = raw.replace(/^\d+:\d+:\d+(?:\.\d+)?\s+\w+\s*:\s*/, "");
    const s = /\[LFTEST\] SUMMARY tier=\w+ passed=(\d+) failed=(\d+) total=(\d+)(?: skipped=(\d+))?/.exec(line);
    if (s) {
      out.passed = +s[1];
      out.failed = +s[2];
      out.total = +s[3];
      out.skipped = s[4] ? +s[4] : 0;
      out.summary = true;
      cur = null;
      continue;
    }
    // A slice the pass cache answered boots nothing and prints no [LFTEST] SUMMARY, only this line (a re-queued gate re-runs identical inputs).
    const c = /^test: CACHED pass\b.*\((\d+)\/(\d+) passed, tier=/.exec(line.trim());
    if (c) {
      out.passed = +c[1];
      out.total = +c[2];
      out.failed = out.total - out.passed;
      out.summary = true;
      continue;
    }
    const t = /test-e2e: SUMMARY .*\((\d+) s\)/.exec(line);
    if (t) out.seconds = +t[1];
    const w = /^pool: \w+(?: \w+)* runs on (.+?)(?:,| \()/.exec(line.trim());
    if (w) out.where = w[1].replace(/^this PC.*/, "this PC");
    const f = /\[LFTEST\] FAIL (.*)$/.exec(line);
    if (f) {
      cur = { name: f[1].trim(), lines: [] };
      out.fails.push(cur);
      continue;
    }
    const d = /\[LFTEST\]\s+- (.*)$/.exec(line);
    if (d && cur) cur.lines.push(d[1].trim());
    else if (!/\[LFTEST\]/.test(line)) cur = null;
  }
  return out;
}

// One verdict from the slices of a pass. `shards` is [{ index, expected, code, parsed, seconds }]:
// `expected` how many tests the slice was given, `code` its exit code, `parsed` parseShardOutput.
// Exit code: 0 all passed; 1 a test failed or a slice ran the wrong tests; 2 a slice gave no verdict;
// 3 a slice never ran (pool failure). The first non-zero severity in that order wins.
export function mergeShards(shards) {
  let passed = 0;
  let failed = 0;
  let total = 0;
  let skipped = 0;
  const fails = [];
  const notes = [];
  let worst = 0;
  const rank = { 0: 0, 3: 1, 2: 2, 1: 3 };
  const bump = (c) => {
    if ((rank[c] ?? 3) > (rank[worst] ?? 3)) worst = c;
  };
  let wall = 0;
  for (const s of shards) {
    const p = s.parsed;
    wall = Math.max(wall, s.seconds ?? p.seconds ?? 0);
    if (!p.summary) {
      notes.push(`shard ${s.index}: no SUMMARY (exit ${s.code}); its ${s.expected} test(s) have no verdict`);
      failed += s.expected;
      total += s.expected;
      bump(s.code === 3 ? 3 : s.code === 0 ? 2 : s.code);
      continue;
    }
    passed += p.passed;
    failed += p.failed;
    total += p.total;
    skipped += p.skipped;
    for (const f of p.fails) fails.push({ ...f, shard: s.index });
    if (p.total + p.skipped !== s.expected) {
      notes.push(`shard ${s.index}: ran ${p.total + p.skipped} of ${s.expected} test(s) (a name matched twice or not at all)`);
      bump(1);
    }
    if (s.code !== 0 || p.failed > 0) bump(s.code !== 0 ? s.code : 1);
  }
  // A non-zero shard exit with no failing test and no note is still a failure.
  if (worst === 0 && shards.some((s) => s.code !== 0)) worst = 1;
  return { code: worst, passed, failed, total, skipped, fails, notes, wall, shards: shards.length };
}

// The lines the merged run prints: the same SUMMARY line a single run does (what `suite-stamp`, the
// quiet summary and the gate read), then every FAIL, then the notes.
export function mergedLines(m, { tier = "all", label = "" } = {}) {
  const out = [];
  for (const f of m.fails) {
    out.push(`[LFTEST] FAIL ${f.name}${label ? ` (${label}, shard ${f.shard})` : ""}`);
    for (const l of f.lines) out.push(`[LFTEST]   - ${l}`);
  }
  for (const n of m.notes) out.push(`FAIL  ${label ? `${label}: ` : ""}${n}`);
  out.push(`test-e2e: SUMMARY tier=${tier} passed=${m.passed} failed=${m.failed} total=${m.total} (${Math.round(m.wall)} s, ${m.shards} shard(s))`);
  out.push(m.code === 0 ? "test-e2e: OK" : `test-e2e: FAILED (exit ${m.code})`);
  return out;
}

// ---------------------------------------------------------------- one local server per worktree

// The slices of a worktree share its server profile and ports, so at most one of them runs on this
// PC at a time (a remote lane has its own run folder). A lock file with the holder's pid.
export function lockFile(root) {
  return join(root, "tmp", "test-shards", "local.lock");
}

export function takeLocal(root, { pid = process.pid, alive = pidAlive } = {}) {
  const f = lockFile(root);
  mkdirSync(dirname(f), { recursive: true });
  for (let i = 0; i < 2; i++) {
    try {
      writeFileSync(f, String(pid), { flag: "wx" });
      return true;
    } catch {
      let holder = 0;
      try {
        holder = Number(readFileSync(f, "utf8").trim());
      } catch {}
      if (holder && alive(holder)) return false;
      rmSync(f, { force: true }); // its owner is gone
    }
  }
  return false;
}

export function dropLocal(root, pid = process.pid) {
  const f = lockFile(root);
  try {
    if (existsSync(f) && Number(readFileSync(f, "utf8").trim()) === pid) rmSync(f, { force: true });
  } catch {}
}

function pidAlive(pid) {
  try {
    process.kill(pid, 0);
    return true;
  } catch (e) {
    return e.code === "EPERM";
  }
}

// ---------------------------------------------------------------- running the slices

// Runs every pass's slices at the same time. `run(job)` starts one slice and resolves
// { code, text, seconds } (injected: cli.mjs spawns a child CLI; the selftest passes a fake).
// passes: [{ name, coreOnly, classes: [[...slice]] }]. Returns [{ pass, merged }].
export async function runPasses(passes, run) {
  const jobs = [];
  for (const pass of passes) {
    pass.classes.forEach((classes, i) => jobs.push({ pass, index: i + 1, count: pass.classes.length, classes }));
  }
  const done = await Promise.all(
    jobs.map(async (job) => {
      const t0 = Date.now();
      let r;
      try {
        r = await run(job);
      } catch (e) {
        r = { code: 3, text: `FAIL  shard ${job.index}/${job.count} could not run: ${e.message}` };
      }
      return { job, code: r.code, text: r.text ?? "", seconds: r.seconds ?? Math.round((Date.now() - t0) / 100) / 10 };
    }),
  );
  return passes.map((pass) => {
    const mine = done.filter((d) => d.job.pass === pass).sort((a, b) => a.job.index - b.job.index);
    const merged = mergeShards(
      mine.map((d) => ({ index: d.job.index, expected: d.job.classes.length, code: d.code, parsed: parseShardOutput(d.text), seconds: d.seconds })),
    );
    return { pass, merged, shards: mine };
  });
}
