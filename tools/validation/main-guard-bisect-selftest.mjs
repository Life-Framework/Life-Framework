// Proves the two main-guard behaviours of the 2026-11-27 revert audit, class D (docs/testing/revert-audit-2026-11-27.md),
// by replaying the four real cases: #2369, #2881 and #2887 (the machine, not the merge) and #3017 (a test fix reverted
// after the data PR it fixed was reverted).
//   1. Several merges red and no compiler output anywhere: infra, nothing reverted (mg.infraRun).
//   2. A test-only merge is bisected together with the merge it follows (mg.groupFollowUps) and, when the data PR was
//      already reverted, is explained by that revert with no shared file (mg.explainedByEarlierRevert, `range`).
// inputs: tools/lib/main-guard.mjs, tools/main-guard.mjs, tools/lib/selftest-git.mjs
// red-proof 2026-11-27: on main before the change this printed "FAIL cannot load: infraRun missing" (the run stopped
//   there); with infraRun's `n < 2` guard removed it printed "FAIL one merge is not several: the build is judged as
//   before"; with groupFollowUps returning one unit per merge it printed "FAIL #3017: the unit [#3008+#3017] is probed at
//   the fix ... got blame m3008"; with the `range.has` clause removed it printed "FAIL #3017 follows the landed revert of
//   #3008 and is test-only: explained with no shared file".
import { readFileSync, writeFileSync, mkdirSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import * as g from "../lib/main-guard.mjs";
import { gitMust, removeScratch, runGit, useHermeticGit } from "../lib/selftest-git.mjs";
import { scratchDir } from "../lib/scratch.mjs";

useHermeticGit();
if (typeof g.infraRun !== "function" || typeof g.groupFollowUps !== "function" || typeof g.isTestOnly !== "function") {
  console.log("FAIL  cannot load: infraRun missing");
  process.exit(1);
}
const fails = [];
const check = (ok, msg) => { if (!ok) fails.push(msg); };
const repoRoot = dirname(dirname(dirname(fileURLToPath(import.meta.url))));

// ---- 1. The machine, not the merge.
// The 2026-11-17 transcript of #2881 and #2887 (tmp/logs/main-guard-7587bfd5-build-*.log), verbatim.
const LOCK = [
  "SUMMARY build fail exit=1 147.6s lines=3 failures=1 warnings=0 log=tmp\\logs\\build-20261117-232542.log",
  "FAIL  lock 'build' is held (C:\\LF\\agents\\Life-Framework\\tmp\\wt\\locks\\build.lock); retry with --wait to queue (a headless Workbench build is running in another worktree)",
].join("\n");
// A build that died before it compiled: no refusal line the classifier knows, no compiler words (the case no refusal list covers).
const DIED = "SUMMARY build fail exit=1 300.1s lines=2 failures=1 warnings=0 log=tmp\\logs\\build-x.log\nFAIL  build: Workbench exited 1 after 300 s";
const COMPILE_ERR = `${DIED}\nSCRIPT (E): LF_Foo.c(12): Undefined variable 'x'`;
const probes = (n, failed = true) => Array.from({ length: n }, (_, i) => ({ sha: `p${i}`, failed }));

// #2369: build red for six merges in a row; the bisect (all probes red) blamed the oldest, a layer-only PR.
const six = g.infraRun({ step: "build", n: 6, tested: probes(3), texts: [DIED, DIED, DIED, DIED, DIED] });
check(typeof six === "string" && /6 merges/.test(six), `#2369: six red builds with no compiler output are infra, got ${six}`);
check(typeof g.infraRun({ step: "build", n: 6, tested: probes(3), texts: [LOCK, LOCK, LOCK, LOCK, LOCK] }) === "string", "#2369 with the lock transcript: infra");
// #2881 + #2887: two consecutive merges, every run the held lock.
check(typeof g.infraRun({ step: "build", n: 2, tested: probes(1), texts: [LOCK, LOCK, LOCK] }) === "string", "#2881 and #2887: two merges, a held lock in every run: infra");
check(g.classifyStep("build", 1, LOCK).verdict === "infra", "#2881: a single held-lock run is infra (the classifier, #2759)");
// A real break stays a revert.
check(g.infraRun({ step: "build", n: 6, tested: probes(3), texts: [DIED, DIED, COMPILE_ERR, DIED, DIED] }) === null, "one run with a compile error: a real red, judged as before");
check(g.infraRun({ step: "build", n: 6, tested: [{ sha: "a", failed: true }, { sha: "b", failed: false }], texts: [DIED, DIED] }) === null, "a green probe found a boundary: judged as before");
check(g.infraRun({ step: "build", n: 1, tested: [], texts: [DIED, DIED] }) === null, "one merge is not several: the build is judged as before");
check(g.infraRun({ step: "suite", n: 6, tested: probes(3), texts: [DIED] }) === null, "only a build is judged this way");
check(g.infraRun({ step: "build", n: 6, tested: probes(3), texts: [] }) === null, "no transcripts: no verdict");

// ---- 2. The test-only follow-up.
// A merge's files, by PR: #3008 pack data, #3017 only the test that pinned it.
const TEST = "addons/LF_Everon1989/Scripts/Game/Tests/LF_Test_EveronCafeOwner.c";
const DATA = "addons/LF_Everon1989/Configs/Dialogue/CafeOwner.conf";
check(g.isTestOnly([TEST]) && g.isTestOnly([TEST, "docs/features/cafe.md"]), "a test file (plus an append-only docs row) is test-only");
check(!g.isTestOnly([TEST, DATA]) && !g.isTestOnly([DATA]) && !g.isTestOnly([]), "data, or data beside a test, is not test-only");
check(g.isTestOnly(["tools/validation/foo-selftest.mjs"]) && !g.isTestOnly(["tools/validation/validate-foo.mjs"]), "a selftest is a test, a validator is not");

const mk = (n, order) => ({ sha: `m${n}`, subject: `Merge pull request #${n} from Life-Framework/ws/pr${n}`, order });
const X = mk(3000, 0);
const A = mk(3008, 1); // data + a test that counts the old greetings: red alone
const B = mk(3017, 2); // the test fix: test-only
const C = mk(3020, 3); // a later merge that breaks the same test again
const testOnly = (c) => c.sha === "m3017";
const units = g.groupFollowUps([X, A, B, C], testOnly);
check(units.length === 3 && units[1].sha === "m3008" && units[1].followers.length === 1 && units[1].probeSha === "m3017", `#3017 joins #3008: ${JSON.stringify(units.map((u) => [u.sha, u.probeSha]))}`);
const revertMerge = { sha: "mR", subject: "Merge pull request #3011 from Life-Framework/ws/revert-pr3008-guard", order: 1 };
check(g.groupFollowUps([revertMerge, B], testOnly).length === 2, "a test-only merge after a revert merge is not attached to it");
check(g.groupFollowUps([B, C], testOnly).length === 2 && g.groupFollowUps([B], testOnly)[0].followers.length === 0, "a test-only merge with nothing before it stands alone");

// The cafe-owner test fails at #3008 alone (new data, old test), passes at #3017 (fixed), fails again at #3020.
const failsAt = new Set(["m3008", "m3020"]);
const SET = ["everon/cafe-owner"];
const probeOf = (calls) => async (c) => { const sha = c.probeSha ?? c.sha; calls.push(sha); return failsAt.has(sha) ? SET : []; };
const ungroupedCalls = [];
const ungrouped = await g.bisectBySet([X, A, B, C], SET, probeOf(ungroupedCalls), { head: "m3020" });
check(ungrouped.culprit?.sha === "m3008", "(the old behaviour, for the record) one by one the bisect blames the data PR that the test fix repaired");
const calls = [];
const grouped = await g.bisectBySet(units, SET, probeOf(calls), { head: "m3020" });
check(grouped.culprit?.sha === "m3020" && !grouped.stop, `#3017: the unit [#3008+#3017] is probed at the fix and is green, so the break is #3020; got blame ${grouped.culprit?.sha} ${grouped.stop ?? ""}`);
check(calls.includes("m3017") && !calls.includes("m3008"), `the unit is probed at the fix, never in between: ${calls}`);
// The head itself being a unit's last follower is not probed twice (the head check compares the probe sha).
const tail = g.groupFollowUps([X, A, B], testOnly);
const tailCalls = [];
await g.bisectBySet(tail, SET, async (c) => { tailCalls.push(c.probeSha ?? c.sha); return SET; }, { head: "m3017" });
check(tailCalls.filter((s) => s === "m3017").length === 0, `the head is the unit's fix (its probe sha) and is assumed red, not probed before the bisect: ${tailCalls}`);

// The history of 3008 -> its landed revert -> 3017, as real merges (the guard's own revert branch names the merge).
const fixture = scratchDir("lf-mg-bisect-");
const git = (...a) => gitMust(fixture, a);
git("init", "-q", "-b", "main");
const w = (rel, text) => { const f = join(fixture, rel); mkdirSync(dirname(f), { recursive: true }); writeFileSync(f, text); };
w("README.md", "base\n");
git("add", "-A");
git("commit", "-q", "-m", "base");
const pr = (n, files) => {
  git("checkout", "-q", "-b", `ws/pr${n}`);
  for (const [rel, text] of Object.entries(files)) w(rel, text);
  git("add", "-A");
  git("commit", "-q", "-m", `change for #${n}`);
  git("checkout", "-q", "main");
  git("merge", "-q", "--no-ff", "-m", `Merge pull request #${n} from Life-Framework/ws/pr${n}`, `ws/pr${n}`);
  return git("rev-parse", "HEAD");
};
const m3008 = pr(3008, { [DATA]: "greetings=4\n", "docs/features/cafe.md": "row\n" });
git("checkout", "-q", "-b", "ws/revert-pr3008-guard");
git("revert", "-m", "1", "--no-edit", m3008);
git("checkout", "-q", "main");
git("merge", "-q", "--no-ff", "-m", "Merge pull request #3011 from Life-Framework/ws/revert-pr3008-guard", "ws/revert-pr3008-guard");
const m3017 = pr(3017, { [TEST]: "// counts four greetings\n" }); // shares no file with #3008
const m3020 = pr(3020, { "tools/validation/other-selftest.mjs": "// other\n" }); // test-only too, but unrelated
const m3030 = pr(3030, { "addons/LifeFramework/Configs/Other.conf": "x=1\n" });
const landed = (n) => git("log", "main", "--first-parent", "--merges", "-F", `--grep=ws/revert-pr${n}-guard`, "--format=%H", "-1") !== "";
const filesOf = (sha) => git("diff", "--name-only", `${sha}^1`, sha).split(/\r?\n/).filter(Boolean);
const isAfter = (rev, sha) => rev !== sha && runGit(fixture, ["merge-base", "--is-ancestor", rev, sha]).status === 0;
const reverted = [{ pr: 3008, sha: m3008, revertPr: "https://github.com/x/y/pull/3011" }];
const cand = (n, sha) => ({ sha, subject: `Merge pull request #${n} from Life-Framework/ws/pr${n}`, beforeStart: false });
const range = new Set([m3008, m3017, m3020, m3030]);
const ex = (c, r = range) => g.explainedByEarlierRevert(c, { reverted }, { landed, isAfter, filesOf, range: r });
const e3017 = ex(cand(3017, m3017)) ?? { pr: 3008, revertPr: null, missing: true };
check(e3017.pr === 3008 && e3017.testOnly === true && !e3017.missing && e3017.files.length === 0, `#3017 follows the landed revert of #3008 and is test-only: explained with no shared file; got ${JSON.stringify(e3017)}`);
check(g.explainedByEarlierRevert(cand(3017, m3017), { reverted }, { landed, isAfter, filesOf }) === null, "without the range only the shared-file rule applies (the old behaviour)");
check(ex(cand(3017, m3017), new Set([m3017])) === null, "a revert from before this bisect's range explains nothing");
check(ex(cand(3030, m3030)) === null, "a PR that changes data (not test-only) with no shared file is not explained");
check(/test-only change that follows #3008/.test(g.earlierRefusal(3017, e3017)), "the refusal says why");
check(/re-land of #3008/.test(g.earlierRefusal(3017, e3017)), "the author is told to ship with the re-land");
check(/#3008/.test(g.revertRefusal(cand(3017, m3017), { reverted }, { step: "suite", earlier: e3017 }) ?? ""), "revertRefusal: the test-only follow-up is not reverted");
// (3020 is test-only and also follows the landed revert: it is explained too, since a test-only PR cannot be the data break.)
check(ex(cand(3020, m3020))?.testOnly === true, "any test-only merge after the landed revert inside the range is held back for a fix card, not reverted");

// ---- 3. The wiring in the worker.
const main = readFileSync(join(repoRoot, "tools", "main-guard.mjs"), "utf8");
const body = main.slice(main.indexOf("async function redAt"), main.indexOf("async function validateHead"));
check(/mg\.groupFollowUps\(merges/.test(body) && /c\.probeSha \?\? c\.sha/.test(body), "redAt bisects units and probes them at the follow-up");
check((body.match(/checkout\(dir, c\.probeSha \?\? c\.sha\)/g) ?? []).length === 2, "both bisect probes check out the unit's follow-up");
check(/mg\.infraRun\(/.test(body) && body.indexOf("mg.infraRun(") < body.indexOf("await revert("), "the non-suite bisect asks infraRun before anything is reverted");
check(/verdict: "infra"/.test(body), "an infra run is recorded as infra, not red");
check(/earlierRevertFor\(dir, b\.culprit, readState\(root\), range\)/.test(body), "the earlier-revert rule gets the bisect range");

removeScratch(fixture);
if (fails.length) {
  for (const m of fails) console.log(`FAIL  ${m}`);
  process.exit(1);
}
console.log("PASS  main-guard-bisect-selftest");
