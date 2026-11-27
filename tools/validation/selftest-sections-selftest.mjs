// tools/validation/selftest-sections-selftest.mjs
// inputs: tools/lib/selftest-sections.mjs, tools/lib/validate-order.mjs, tools/lib/validate-order-seed.json
//
// Proves the two pieces that make `tools\cli validate` finish sooner without dropping an assertion:
//   * tools/lib/selftest-sections.mjs runs the sections of a selftest in parallel child processes, replays their output in
//     registration order, adds up their failures, and turns a crash or a missing section into a failure;
//   * tools/lib/validate-order.mjs starts the slowest checks first, from this worktree's last times, else the committed seed.
// red-proof: 2026-11-27, with run() ignoring a child's exit code "a failing section fails the whole run" and "a crashing
// section fails the run" failed; with the replay sorted by finish time "output is replayed in registration order" failed;
// with orderJobs reading the seed before the state "this worktree's own time beats the seed" failed. All restored.
import { spawnSync } from "node:child_process";
import { writeFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";
import { scratchDir } from "../lib/scratch.mjs";
import { sectionJobs } from "../lib/selftest-sections.mjs";
import { loadSeed, orderJobs, seedFrom } from "../lib/validate-order.mjs";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");
const lib = pathToFileURL(join(ROOT, "tools", "lib", "selftest-sections.mjs")).href;
let failed = 0;
const check = (ok, msg) => {
  if (!ok) {
    failed++;
    console.log(`FAIL  ${msg}`);
  }
};

const dir = scratchDir("lf-sections-");
const fixture = (name, body) => {
  const file = join(dir, `${name}.mjs`);
  writeFileSync(file, `import { sections } from ${JSON.stringify(lib)};\nconst S = sections(import.meta.url);\nlet failed = 0;\nconst fail = (m) => { failed++; console.log("FAIL  " + m); };\n${body}\nprocess.exit(await S.run({ failed: () => failed, summary: (n) => "SUMMARY fx " + (n ? "fail " + n : "ok") }));\n`);
  return file;
};
// Runs a fixture as a parent: LF_SELFTEST_SECTION (set when this very check runs inside a section) is removed so
// the fixture's own run() starts its children.
const runClean = (file, env = {}) => {
  const e = { ...process.env, ...env };
  if (!("LF_SELFTEST_SECTION" in env)) delete e.LF_SELFTEST_SECTION;
  const r = spawnSync(process.execPath, [file], { encoding: "utf8", windowsHide: true, env: e });
  return { code: r.status, out: String(r.stdout ?? ""), err: String(r.stderr ?? "") };
};

try {
  // ---- all sections pass: output replayed in registration order whatever order they finish in
  const ok = fixture("ok", `
S.add("slow first", () => { const t = Date.now(); while (Date.now() - t < 400); console.log("line from slow first"); });
S.add("fast second", () => { console.log("line from fast second"); });
S.add("fast third", () => { console.log("line from fast third"); });
`);
  for (const jobs of ["1", "3"]) {
    const r = runClean(ok, { LF_SELFTEST_JOBS: jobs });
    check(r.code === 0, `all sections pass: exit ${r.code} with ${jobs} job(s): ${r.out}${r.err}`);
    const at = ["line from slow first", "line from fast second", "line from fast third", "SUMMARY fx ok"].map((s) => r.out.indexOf(s));
    check(at.every((i) => i >= 0) && at.every((i, k) => k === 0 || i > at[k - 1]), `output is replayed in registration order with ${jobs} job(s): ${JSON.stringify(r.out)}`);
  }

  // ---- a failing section fails the whole run, and its own lines are shown
  const bad = fixture("bad", `
S.add("good", () => { console.log("good ran"); });
S.add("bad", () => { fail("the assertion in bad"); fail("a second one"); });
`);
  const b = runClean(bad);
  check(b.code === 1, `a failing section fails the whole run: exit ${b.code}`);
  check(b.out.includes("FAIL  the assertion in bad") && b.out.includes("FAIL  a second one"), `the failing section's lines are replayed: ${b.out}`);
  check(b.out.includes("SUMMARY fx fail 3") || /SUMMARY fx fail \d/.test(b.out), `the parent's summary counts the failures: ${b.out}`);
  check(b.out.includes("good ran"), "a passing section's output is still shown beside a failing one");

  // ---- a crash is a failure with its message on stderr
  const crash = fixture("crash", `
S.add("fine", () => {});
S.add("boom", () => { throw new Error("section blew up"); });
`);
  const c = runClean(crash);
  check(c.code === 1, `a crashing section fails the run: exit ${c.code}`);
  check(c.err.includes("section blew up"), `the crash message reaches stderr: ${c.err}`);

  // ---- a section async function is awaited
  const asy = fixture("async", `
S.add("later", async () => { await new Promise((r) => setTimeout(r, 100)); fail("failed after an await"); });
`);
  const a = runClean(asy);
  check(a.code === 1 && a.out.includes("failed after an await"), `an async section is awaited and its failure counts: exit ${a.code} ${a.out}`);

  // ---- a duplicate name is refused, an unknown section selected from outside fails
  const dup = fixture("dup", `S.add("x", () => {}); S.add("x", () => {});`);
  check(runClean(dup).code !== 0, "a duplicate section name is refused");
  const sel = runClean(ok, { LF_SELFTEST_SECTION: "no such section" });
  check(sel.code === 1 && sel.out.includes("no section named"), `an unknown section is a failure, not a silent pass: exit ${sel.code} ${sel.out}`);

  // ---- the job count: explicit, bounded by the CPU count, never under 2
  check(sectionJobs({ LF_SELFTEST_JOBS: "7" }, 4) === 7, "LF_SELFTEST_JOBS wins");
  check(sectionJobs({}, 16) === 4 && sectionJobs({}, 4) === 2 && sectionJobs({}, 1) === 2, "default jobs: half the CPUs, between 2 and 4");
  check(sectionJobs({ LF_SELFTEST_JOBS: "zero" }, 8) === 4, "a non-numeric LF_SELFTEST_JOBS is ignored");

  // ---- validate-order: this worktree's time beats the seed, the seed beats nothing, slowest first, stable for ties
  const jobs = ["a.mjs", "b.mjs", "c.mjs", "d.mjs", "e.mjs"].map((s) => ({ s }));
  const state = { checks: { "a.mjs": { secs: 50 }, "b.mjs": { secs: 1 } } };
  const seed = { "b.mjs": 100, "c.mjs": 20, "a.mjs": 5 };
  check(orderJobs(jobs, state, seed).map((j) => j.s).join() === "a.mjs,c.mjs,b.mjs,d.mjs,e.mjs", `this worktree's own time beats the seed; unknown checks keep their order: ${orderJobs(jobs, state, seed).map((j) => j.s)}`);
  check(orderJobs(jobs, null, seed).map((j) => j.s).join() === "b.mjs,c.mjs,a.mjs,d.mjs,e.mjs", "with no state the seed orders the checks");
  check(orderJobs(jobs, null, {}).map((j) => j.s).join() === "a.mjs,b.mjs,c.mjs,d.mjs,e.mjs", "with neither, file order stays");
  check(orderJobs(jobs, state, seed) !== jobs, "orderJobs does not reorder the caller's array in place");
  const text = seedFrom({ checks: { "slow.mjs": { secs: 12.34 }, "fast.mjs": { secs: 0.4 }, "mid.mjs": { secs: 3 } } });
  const parsed = JSON.parse(text);
  check(Object.keys(parsed.secs).join() === "slow.mjs,mid.mjs" && parsed.secs["slow.mjs"] === 12.3, `seedFrom keeps only the slow checks, slowest first: ${text}`);
  check(Object.keys(loadSeed(join(dir, "missing.json"))).length === 0, "a missing seed file is an empty seed");
  const committed = loadSeed();
  check(Object.keys(committed).length >= 20 && Object.values(committed).every((s) => typeof s === "number" && s >= 2), "the committed seed names the slow checks with their seconds");
} finally {
  /* scratchDir removes the folder when the process ends */
}

console.log(failed ? `selftest-sections-selftest: ${failed} failed` : "selftest-sections-selftest: ok");
process.exit(failed ? 1 : 0);
