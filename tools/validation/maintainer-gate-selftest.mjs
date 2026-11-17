// Proves the maintainer gate fixes (docs/worktrees.md "Maintainer world edits"):
//   (a) on a town-maintainer-* branch the ci placement audit is warn-only, whatever the tier, and its findings
//       reach the PR body (tools/lib/gate-warnings.mjs);
//   (b) the tier is judged on the maintainer's own change, not on what a merge of main brought in, even when the
//       local origin/main ref is stale (gate-tier.mjs ownBase / collectDiff);
//   (d) a failing test is a warning at every tier (demoteTests, ci test steps);
//   (c) a world ship body stays under GitHub's 65 KB limit with a "+N more" line, and `gh pr create` gets the body
//       through --body-file, never on the command line (ENAMETOOLONG).
// inputs: tools/lib/gate-tier.mjs, tools/lib/gate-warnings.mjs, tools/lib/world-test-judge.mjs, tools/cli.mjs, tools/commands/world.mjs, tools/wt.mjs, tools/validation/maintainer-gate-selftest.mjs
// red-proof (d, 2026-11-15): with gate-warnings.mjs, world-test-judge.mjs and cli.mjs from main, (d) printed FAIL for every case (no demoteTests, tier=all-core unparsed, bare test steps).
// red-proof: before the fix this printed FAIL for every case below (ownBase, gate-warnings.mjs and the body cap did not
//   exist; createPrGh passed --body) and exited 1 (2026-11-15); with the fix it passes.
import { spawnSync } from "node:child_process";
import { existsSync, mkdirSync, mkdtempSync, readFileSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import * as gt from "../lib/gate-tier.mjs";
import * as gw from "../lib/gate-warnings.mjs";
import * as world from "../commands/world.mjs";
import * as wt from "../wt.mjs";
import { diffFiles } from "../lib/world-diff.mjs";

const fails = [];
const check = (ok, msg) => { if (!ok) fails.push(msg); };

// ---- (b) own change only: a temp repo whose local origin/main is stale.
const dir = mkdtempSync(join(tmpdir(), "lf-maint-gate-"));
const g = (...a) => {
  const r = spawnSync("git", ["-C", dir, "-c", "user.email=t@t", "-c", "user.name=t", ...a], { encoding: "utf8", windowsHide: true });
  if (r.status !== 0) throw new Error(`git ${a.join(" ")}: ${r.stderr}`);
  return r.stdout.trim();
};
const put = (p, text) => { mkdirSync(join(dir, p, ".."), { recursive: true }); writeFileSync(join(dir, p), text); };
const LAYER = "addons/LifeFramework/Worlds/DebugWorld/DebugWorld_Layers/Signs.layer";
g("init", "-q", "-b", "main");
put(LAYER, "a 1\n");
put("docs/a.md", "one\n");
g("add", "-A"); g("commit", "-q", "-m", "c1");
const c1 = g("rev-parse", "HEAD");
g("update-ref", "refs/remotes/origin/main", c1);
g("checkout", "-q", "-b", "ws/town-maintainer-edits");
put(LAYER, "a 2\n");
g("add", "-A"); g("commit", "-q", "-m", "maintainer move");
g("checkout", "-q", "main");
put("docs/a.md", "two\n");
put("addons/LifeFramework/Scripts/Game/Core/LF_X.c", "class LF_X {}\n");
g("add", "-A"); g("commit", "-q", "-m", "main moves on");
const c2 = g("rev-parse", "HEAD");
g("checkout", "-q", "ws/town-maintainer-edits");
g("merge", "-q", "--no-ff", "-m", "Merge remote-tracking branch 'origin/main' into ws/town-maintainer-edits", c2);
// origin/main stays at c1: stale, as when the gate runs before the next fetch.
const gitFn = (args) => { const r = spawnSync("git", ["-C", dir, ...args], { encoding: "utf8", windowsHide: true }); return { status: r.status, stdout: r.stdout ?? "" }; };
const read = (p) => { try { return readFileSync(join(dir, p), "utf8"); } catch { return null; } };
const files = gt.collectDiff({ git: gitFn, read, base: "main" });
const paths = (files ?? []).map((f) => f.path).sort();
check(paths.join() === LAYER, `a branch that merged main must be judged on its own layer edit only, got ${JSON.stringify(paths)}`);
check(typeof gt.ownBase === "function" && gt.ownBase(gitFn, "main") === c2, "ownBase is the newest main commit merged into the branch");
// A fresh ref gives the same answer.
g("update-ref", "refs/remotes/origin/main", c2);
check((gt.collectDiff({ git: gitFn, read, base: "main" }) ?? []).map((f) => f.path).join() === LAYER, "a fresh origin/main still gives only the own layer edit");
// No main merge at all: the plain merge base.
g("update-ref", "refs/remotes/origin/main", c1);
g("checkout", "-q", "-b", "ws/plain", c1);
put(LAYER, "a 3\n");
g("add", "-A"); g("commit", "-q", "-m", "plain edit");
check(typeof gt.ownBase === "function" && gt.ownBase(gitFn, "main") === c1, "no main merge: ownBase is the merge base");

// The tier reason names why a hand edit was refused, not ".layer is not a data file".
const why = gt.classify([{ path: LAYER, status: "M", before: "a\n", after: "b\n" }], { branch: "ws/town-maintainer-edits", worldEdit: () => "PROP CHANGED Foo: prefab" });
check(why.tier === 2 && why.reasons.some((r) => /PROP CHANGED Foo: prefab/.test(r)), `a refused hand edit must say why, got ${JSON.stringify(why.reasons)}`);

// ---- (a) placement audit warn-only on a maintainer branch.
const auditText = [
  "placement audit: 15 of 1069 entities flagged",
  "  1.  15.5  Market_Tarp_Juice [Village_Dressing.layer, market] @217.51,2.54,233.12: overlaps Market_Post_Juice_B1 by 0.19 m",
  "  2.   7.8  Market_Post_Veg_B1 [Village_Dressing.layer, market] @195.61,1.46,232.22: overlaps Market_Tarp_Veg by 0.19 m",
  "report: C:\\x\\report.txt",
].join("\n");
const failing = async () => { process.stdout.write(`${auditText}\n`); return 1; };
check(typeof gw.placementFindingLines === "function" && gw.placementFindingLines(auditText).length === 2, "placementFindingLines picks the ranked rows");
if (typeof gw.demotePlacement === "function") {
  const recorded = [];
  const out = process.stdout.write.bind(process.stdout);
  process.stdout.write = () => true;
  let code;
  let plain;
  try {
    code = await gw.demotePlacement("ws/town-maintainer-edits", failing, (e) => recorded.push(e))(["--strict"]);
    plain = await gw.demotePlacement("ws/some-slice", failing, (e) => recorded.push({ other: e }))(["--strict"]);
  } finally { process.stdout.write = out; }
  check(code === 0, `a maintainer branch must not fail on the placement audit, got exit ${code}`);
  check(recorded.length === 1 && recorded[0].check === "placement audit" && recorded[0].lines.length === 2, `the findings are recorded for the PR body, got ${JSON.stringify(recorded)}`);
  check(plain === 1, "any other branch still fails on the placement audit");
} else check(false, "gate-warnings.mjs has no demotePlacement");

// ---- (d) a failing test is a warning on a maintainer branch, at every tier; a compile error or an empty run still blocks.
{
  const shardFail = async () => {
    process.stdout.write("[LFTEST] FAIL disconnect/jail (normal, shard 4)\n[LFTEST]   - the incident log has the logout with the state and the sentence\ntest-e2e: SUMMARY tier=all passed=580 failed=1 total=581 (48 s, 4 shard(s))\ntest-e2e: FAILED (exit 1)\n");
    return 1;
  };
  // a two-pass run: the normal pass is green, the core-only pass has the failure (the first SUMMARY says failed=0)
  const corePassFail = async () => {
    process.stdout.write("test-e2e: SUMMARY tier=all passed=581 failed=0 total=581 (48 s)\n[LFTEST] FAIL disconnect/jail (core, shard 4)\n[LFTEST]   - the incident log has the logout with the state and the sentence\ntest-e2e: SUMMARY tier=all-core passed=580 failed=1 total=581 (47 s)\n");
    return 1;
  };
  const compileErr = async () => { process.stdout.write("Can't compile \"Game\" script module!\ntest-e2e: SUMMARY tier=all passed=0 failed=1 total=1\n"); return 1; };
  const noSummary = async () => { process.stdout.write("TIMEOUT after 281s (the server never reached ready)\n"); return 1; };
  const green = async () => 0;
  if (typeof gw.demoteTests === "function") {
    const recorded = [];
    const out = process.stdout.write.bind(process.stdout);
    process.stdout.write = () => true;
    let a, b, cErr, none, ok, plain;
    try {
      const rec = (e) => recorded.push(e);
      a = await gw.demoteTests("ws/town-maintainer-edits", shardFail, rec)();
      b = await gw.demoteTests("ws/town-maintainer-edits", corePassFail, rec)();
      cErr = await gw.demoteTests("ws/town-maintainer-edits", compileErr, rec)();
      none = await gw.demoteTests("ws/town-maintainer-edits", noSummary, rec)();
      ok = await gw.demoteTests("ws/town-maintainer-edits", green, rec)();
      plain = await gw.demoteTests("ws/some-slice", shardFail, rec)();
    } finally { process.stdout.write = out; }
    check(a === 0, `a failing test must not fail a maintainer branch, got exit ${a}`);
    check(b === 0, `a failure in the second (core-only) pass must warn too, got exit ${b}`);
    check(recorded.length === 2 && recorded.every((e) => e.check === "tests" && /disconnect\/jail|the incident log/.test(e.lines.join(" "))), `the failing test is recorded for the PR body, got ${JSON.stringify(recorded)}`);
    check(cErr === 1, "a compile error still blocks a maintainer branch");
    check(none === 1, "a run with no SUMMARY (timeout, crash) still blocks a maintainer branch");
    check(ok === 0, "a green run stays green");
    check(plain === 1, "any other branch still fails on a failing test");
  } else check(false, "gate-warnings.mjs has no demoteTests");
  // the step runs in every ci path: full, sharded, slim and T1 (the cli wraps each test step in maint.test)
  const src = readFileSync(new URL("../cli.mjs", import.meta.url), "utf8");
  const bare = [...src.matchAll(/\["test(?! \(T1w)[^"]*",\s*(?:async )?\(\) => (?!maint\.test)/g)].map((m) => m[0]);
  check(bare.length === 0, `every ci test step must go through maint.test, found ${JSON.stringify(bare)}`);
  check((src.match(/maint\.test\(/g) ?? []).length >= 5, "the full, sharded, core-only, slim and T1 test steps use maint.test");
}

// The PR body section replaces itself (a re-gate does not stack it) and stays inside the size cap.
if (typeof gw.applyGateWarnings === "function") {
  const md1 = gw.gateWarningsMarkdown([{ check: "placement audit", lines: ["row one"] }]);
  const b1 = gw.applyGateWarnings("## Changes\n\n- x\n", md1);
  check(/## Gate warnings/.test(b1) && /row one/.test(b1) && /## Changes/.test(b1), "the section is appended to the PR body");
  const b2 = gw.applyGateWarnings(b1, gw.gateWarningsMarkdown([{ check: "placement audit", lines: ["row two"] }]));
  check(!/row one/.test(b2) && /row two/.test(b2) && b2.match(/## Gate warnings/g).length === 1, "a re-gate replaces the section");
  check(gw.applyGateWarnings(b2, "") === "## Changes\n\n- x\n" || !/## Gate warnings/.test(gw.applyGateWarnings(b2, "")), "no warnings removes the section");
  const huge = gw.gateWarningsMarkdown([{ check: "placement audit", lines: Array.from({ length: 5000 }, (_, i) => `row ${i} ${"x".repeat(60)}`) }]);
  check(huge.length < 30000 && /\+\d+ more/.test(huge), `the section is capped with a +N more line, got ${huge.length} chars`);
} else check(false, "gate-warnings.mjs has no applyGateWarnings");

// ---- (c) the world ship body stays under 65 KB; the change list is what gets cut.
{
  const lines = [];
  for (let i = 0; i < 2000; i++) lines.push(`GenericEntity Prop_${i} : "{419130031F0009C9}Prefabs/Structures/Walls/Brick/BrickWall_01/BrickWall_01_4m.et" {\n coords ${i} 0 ${i}\n}`);
  const before = "";
  const after = `${lines.join("\n")}\n`;
  const d = diffFiles([{ path: "addons/X/Y_Layers/Big.layer", before, after }]);
  const body = world.prBody(d, { slug: "town-maintainer-edits" });
  check(body.length < 62000, `a huge change list must stay under the cap, got ${body.length} chars`);
  check(/\+\d+ more.*world diff/.test(body), "the cut change list ends with a '+N more, see world diff' line");
  check(/## Manual testing/.test(body) && /Generated with \[Claude Code\]/.test(body), "the sections after the change list survive the cut");
  const small = world.prBody(diffFiles([]), { slug: "town-maintainer-edits" });
  check(!/more, see/.test(small), "a small body is not cut");
}

// ---- (c) gh pr create gets --body-file, never --body.
{
  const calls = [];
  const fake = (cmd, args) => {
    calls.push(args);
    const i = args.indexOf("--body-file");
    if (i >= 0) calls.at(-1).fileText = existsSync(args[i + 1]) ? readFileSync(args[i + 1], "utf8") : null;
    return { status: 0, stdout: "https://github.com/o/r/pull/7\n", stderr: "" };
  };
  const root = mkdtempSync(join(tmpdir(), "lf-ghbody-"));
  const body = "x".repeat(40000);
  let pr = null;
  try { pr = wt.createPrGh(root, { head: "ws/a", title: "t", body, base: "main" }, { run: fake, gh: "gh" }); } catch (e) { check(false, `createPrGh threw: ${e.message}`); }
  const a = calls[0] ?? [];
  check(!a.includes("--body"), "gh pr create must not take the body on the command line");
  check(a.includes("--body-file") && a.fileText === body, "gh pr create reads the body from --body-file, with the full text");
  check(pr?.number === 7, `createPrGh still returns the PR, got ${JSON.stringify(pr)}`);
  const bf = a[a.indexOf("--body-file") + 1];
  check(!bf || !existsSync(bf), "the body file is removed after the call");
}

if (fails.length) {
  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(`maintainer-gate-selftest: ${fails.length} case(s) failed`);
  process.exit(1);
}
console.log("maintainer-gate-selftest: OK (own-change tier with a stale origin/main, warn-only placement audit, PR body section, body cap, --body-file)");
