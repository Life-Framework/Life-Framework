// Proves the main guard's validate stage (#2889, tools/lib/main-guard.mjs validateGate): `validate --all` on every new main
// head finds the merge that introduced a failure, comments and cards a soft one, reverts only a hard data-safety check, and
// never reverts the follow-up of a PR the guard already reverted (2026-11-18: #3008 reverted, then #3017, the test fix that
// matched it, reverted as well; closed by hand as #3031). Runs on a fixture repo: real merges, real first-parent history.
// inputs: tools/lib/main-guard.mjs, tools/main-guard.mjs, tools/cli.mjs, tools/lib/selftest-git.mjs
// red-proof 2026-11-18: with HARD_CHECKS emptied this printed "FAIL hard check: the merge is reverted"; with the earlier-revert
//   rule's `if (ex) action = "explained"` removed it printed "FAIL follow-up of a reverted PR: explained, never reverted";
//   with the dedupe branch removed it printed "FAIL still red with the flagged set: known, no second bisect".
import { readFileSync, writeFileSync, mkdirSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import * as g from "../lib/main-guard.mjs";
import { gitMust, removeScratch, runGit, useHermeticGit } from "../lib/selftest-git.mjs";
import { scratchDir } from "../lib/scratch.mjs";

useHermeticGit();
const fails = [];
const check = (ok, msg) => { if (!ok) fails.push(msg); };
const repoRoot = dirname(dirname(dirname(fileURLToPath(import.meta.url))));

// ---- Transcript parsing: the compact validate output (FAIL headers with their explanation under them).
const transcript = [
  "FAIL  validate/validate-persistence-version.ps1 (1.2 s)",
  "FAIL addons/LifeFramework/Scripts/Game/Persistence/LF_PersistenceRegistry.c:41 RegisterPersistencePayloads has 31 entries; frozen at 30",
  "FAIL  validate/validate-docs.mjs (0.4 s)",
  "docs/features/shops.md:9 row is not in the index",
  "WARN static-init-budget: 200 is at least 200",
  "slowest: world-sync-selftest.mjs 103.3 s",
  "validate: 382 check(s) run, 2 failed (194.2 s)",
].join("\n");
check(g.failingChecks(transcript).join() === "validate-persistence-version.ps1,validate-docs.mjs", `failingChecks: ${g.failingChecks(transcript)}`);
check(g.failingChecks("PASS  validate/x.mjs (1 s)\nvalidate: 3 check(s) run, 0 failed").length === 0, "a green transcript names no check");
const outLines = g.checkOutput(transcript, ["validate-persistence-version.ps1"]);
check(outLines.length === 2 && /Registry\.c:41/.test(outLines[1]), `checkOutput keeps the failing check's block only: ${JSON.stringify(outLines)}`);
check(g.isHardCheck("validate-persistence-version.ps1") && g.isHardCheck("validate-repo.ps1") && !g.isHardCheck("validate-docs.mjs"), "hard checks are the data-safety ones");
for (const h of g.HARD_CHECKS) check(readFileSync(join(repoRoot, "tools", "validation", h), "utf8").length > 0, `HARD_CHECKS names a missing check: ${h}`);

// ---- Batching: one validate per ~3 min, never twice for a head.
const T = Date.parse("2026-11-18T12:00:00Z");
check(g.shouldValidate({}, "h1", T).run, "a new head validates");
check(!g.shouldValidate({ lastValidated: "h1" }, "h1", T).run, "a validated head is not validated again");
check(!g.shouldValidate({ lastValidateRed: "h1" }, "h1", T).run, "a flagged head is not validated again");
check(!g.shouldValidate({ lastValidateAt: new Date(T - 60000).toISOString() }, "h2", T).run, "a head 1 min after the last run waits");
check(g.shouldValidate({ lastValidateAt: new Date(T - 4 * 60000).toISOString() }, "h2", T).run, "a head 4 min after the last run validates");

// ---- The fixture repo: main plus PR merges. Check rules read the files of a commit, like validate reads a checkout.
const dir = scratchDir("lf-mg-validate-");
const git = (...a) => gitMust(dir, a);
const write = (rel, text) => { const f = join(dir, rel); mkdirSync(dirname(f), { recursive: true }); writeFileSync(f, text); };
git("init", "-q", "-b", "main");
write("README.md", "base\n");
git("add", "-A");
git("commit", "-q", "-m", "base");
function pr(n, files) {
  git("checkout", "-q", "-b", `ws/pr${n}`);
  for (const [rel, text] of Object.entries(files)) write(rel, text);
  git("add", "-A");
  git("commit", "-q", "-m", `change for #${n}`);
  git("checkout", "-q", "main");
  git("merge", "-q", "--no-ff", "-m", `Merge pull request #${n} from Life-Framework/ws/pr${n}`, `ws/pr${n}`);
  return git("rev-parse", "HEAD");
}
const RULES = {
  "validate-repo.ps1": (files) => files.includes("dup.meta"),
  "validate-docs.mjs": (files) => files.includes("docs/bad.md"),
  "validate-other.mjs": (files) => files.includes("other.bad"),
};
let probes = [];
const runAt = (sha, only) => {
  probes.push(`${sha.slice(0, 7)}${only ? "!" : ""}`);
  const files = git("ls-tree", "-r", "--name-only", sha).split(/\r?\n/);
  const failed = Object.entries(RULES).filter(([name, f]) => (!only || only.includes(name)) && f(files)).map(([name]) => name);
  return { verdict: failed.length ? "red" : "ok", failed };
};
const mergesFrom = (from, head) => g.parseMerges(git("log", "--first-parent", "--merges", "--reverse", "--format=%H%x09%s", `${from}..${head}`));
const filesOf = (sha) => git("diff", "--name-only", `${sha}^1`, sha).split(/\r?\n/).filter(Boolean);
const base = git("rev-parse", "HEAD");

const m10 = pr(10, { "a.txt": "a1\n", "docs/features/x.md": "row\n" });
const m11 = pr(11, { "b.txt": "b\n" });
const m12 = pr(12, { "docs/bad.md": "oops\n" }); // a soft check starts failing here
const m13 = pr(13, { "c.txt": "c\n" });
const m14 = pr(14, { "d.txt": "d\n" });

// 1. A soft check: found at the merge that introduced it, commented and carded, not reverted.
probes = [];
let r = await g.validateGate({ head: m14, merges: mergesFrom(base, m14), state: {}, run: async (s, o) => runAt(s, o) });
check(r.verdict === "red" && r.culprit?.sha === m12, `soft: blame #12, got ${r.verdict} ${r.culprit?.subject}`);
check(r.action === "flag" && r.set.join() === "validate-docs.mjs", `soft check: flag, not revert (got ${r.action} ${r.set})`);
check(probes.length <= 6, `soft: bisect over 4 merges probed ${probes.length} times (${probes.join(" ")})`);
const body = g.validateComment(r.culprit, r.set, ["FAIL  validate/validate-docs.mjs (0.4 s)", "  docs/bad.md:1 oops"], "C:\\log");
check(/#12/.test(body) && /validate-docs\.mjs/.test(body) && /docs\/bad\.md:1/.test(body) && /Not reverted/.test(body), "the PR comment names the PR, the check and its output");
const card = g.validateFixCard(r.culprit, r.set, [], "C:\\log");
check(/after #12/.test(card.title) && /validate-docs\.mjs/.test(card.title), `fix card title: ${card.title}`);

// 2. A hard data-safety check reverts the merge that introduced it.
const m15 = pr(15, { "dup.meta": "guid\n" });
probes = [];
r = await g.validateGate({ head: m15, merges: mergesFrom(m11, m15), state: {}, run: async (s, o) => runAt(s, o) });
check(r.verdict === "red" && r.culprit?.sha === m12, "mixed: the earliest merge that fails the recorded set is blamed");
// The recorded set at the head is both checks; #12 explains only the docs one, #15 the hard one: no single merge.
check(r.stop && /mixed causes/.test(r.stop) && r.action === "flag", `two causes at the head: mixed causes, no revert (got ${r.action}, ${r.stop})`);
git("revert", "-m", "1", "--no-edit", m12);
const afterRevert = git("rev-parse", "HEAD");
r = await g.validateGate({ head: afterRevert, merges: mergesFrom(m13, afterRevert), state: {}, run: async (s, o) => runAt(s, o) });
check(r.verdict === "red" && r.culprit?.sha === m15 && r.set.join() === "validate-repo.ps1", `hard: blame #15 (got ${r.culprit?.subject})`);
check(r.action === "revert", "hard check: the merge is reverted");
check(r.hits.join() === "validate-repo.ps1", "the hits name the hard check");
// A hard check on a merge from before the guard started, or one already reverted, is never reverted.
const stale = await g.validateGate({ head: afterRevert, merges: g.parseMerges(git("log", "--first-parent", "--merges", "--reverse", "--format=%H%x09%s", `${m13}..${afterRevert}`), () => true), state: {}, run: async (s, o) => runAt(s, o) });
check(stale.verdict === "red" && stale.culprit === null && stale.action === "flag", `a merge from before the guard started is not blamed (got ${stale.action}, ${stale.culprit?.sha})`);
const again = await g.validateGate({ head: afterRevert, merges: mergesFrom(m13, afterRevert), state: { reverted: [{ pr: 15, sha: m15 }] }, run: async (s, o) => runAt(s, o) });
check(again.action === "flag" && /already reverted/.test(again.refusal ?? ""), `a PR is reverted at most once (got ${again.action}, ${again.refusal})`);

// 3. The follow-up of a reverted PR: #3017 against #3008.
const fixture2 = scratchDir("lf-mg-validate2-");
const git2 = (...a) => gitMust(fixture2, a);
git2("init", "-q", "-b", "main");
const w2 = (rel, text) => { const f = join(fixture2, rel); mkdirSync(dirname(f), { recursive: true }); writeFileSync(f, text); };
w2("README.md", "base\n");
git2("add", "-A");
git2("commit", "-q", "-m", "base");
const pr2 = (n, files) => {
  git2("checkout", "-q", "-b", `ws/pr${n}`);
  for (const [rel, text] of Object.entries(files)) w2(rel, text);
  git2("add", "-A");
  git2("commit", "-q", "-m", `change for #${n}`);
  git2("checkout", "-q", "main");
  git2("merge", "-q", "--no-ff", "-m", `Merge pull request #${n} from Life-Framework/ws/pr${n}`, `ws/pr${n}`);
  return git2("rev-parse", "HEAD");
};
const b2 = git2("rev-parse", "HEAD");
const m3008 = pr2(3008, { "cafe.conf": "owner=a\n", "docs/features/cafe.md": "row\n" });
// The guard reverts #3008 on its revert branch; the merge on main names ws/revert-pr3008-guard.
git2("checkout", "-q", "-b", "ws/revert-pr3008-guard");
git2("revert", "-m", "1", "--no-edit", m3008);
git2("checkout", "-q", "main");
git2("merge", "-q", "--no-ff", "-m", "Merge pull request #3011 from Life-Framework/ws/revert-pr3008-guard", "ws/revert-pr3008-guard");
const m3017 = pr2(3017, { "cafe.conf": "owner=b\n", "docs/features/cafe.md": "row2\n" }); // the test fix that matched #3008
const m3020 = pr2(3020, { "unrelated.txt": "u\n", "docs/features/cafe.md": "row3\n" });
const landed = (n) => git2("log", "main", "--first-parent", "--merges", "-F", `--grep=ws/revert-pr${n}-guard`, "--format=%H", "-1") !== "";
const asFiles2 = (sha) => git2("diff", "--name-only", `${sha}^1`, sha).split(/\r?\n/).filter(Boolean);
const isAfter2 = (rev, sha) => rev !== sha && runGit(fixture2, ["merge-base", "--is-ancestor", rev, sha]).status === 0;
const reverted = [{ pr: 3008, sha: m3008, revertPr: "https://github.com/x/y/pull/3011" }];
const cand3017 = { sha: m3017, subject: "Merge pull request #3017 from Life-Framework/ws/pr3017", beforeStart: false };
const cand3020 = { sha: m3020, subject: "Merge pull request #3020 from Life-Framework/ws/pr3020", beforeStart: false };
const ex = (c) => g.explainedByEarlierRevert(c, { reverted }, { landed, isAfter: isAfter2, filesOf: asFiles2 });
check(ex(cand3017)?.pr === 3008 && ex(cand3017).files.join() === "cafe.conf", `#3017 follows the reverted #3008 and shares cafe.conf: ${JSON.stringify(ex(cand3017))}`);
check(ex(cand3020) === null, "a later PR that shares only an append-only docs file is not explained by the revert");
check(g.explainedByEarlierRevert(cand3017, { reverted }, { landed: () => false, isAfter: isAfter2, filesOf: asFiles2 }) === null, "a revert that is not on main explains nothing");
check(g.explainedByEarlierRevert({ sha: m3008, subject: "Merge pull request #3008 from x/ws/y" }, { reverted }, { landed, isAfter: isAfter2, filesOf: asFiles2 }) === null, "the reverted PR itself is not its own follow-up");
check(/#3008/.test(g.revertRefusal(cand3017, { reverted }, { step: "suite", earlier: ex(cand3017) }) ?? ""), "revertRefusal: the follow-up of a reverted PR is not reverted");
check(g.revertRefusal(cand3020, { reverted }, { step: "suite", earlier: ex(cand3020) }) === null, "revertRefusal: an unrelated PR may still be reverted");
// The gate on the same history: a hard check the follow-up trips is explained, never reverted.
const run2 = async (sha, only) => {
  const failed = !only || only.includes("validate-repo.ps1") ? (String(runGit(fixture2, ["cat-file", "-p", `${sha}:cafe.conf`]).stdout).includes("owner=b") ? ["validate-repo.ps1"] : []) : [];
  return { verdict: failed.length ? "red" : "ok", failed };
};
const g2 = await g.validateGate({ head: m3020, merges: g.parseMerges(git2("log", "--first-parent", "--merges", "--reverse", "--format=%H%x09%s", `${b2}..${m3020}`), (s) => s === m3008), state: { reverted }, run: run2, earlier: ex });
check(g2.verdict === "red" && g2.culprit?.sha === m3017, `follow-up: blame #3017 (got ${g2.verdict} ${g2.culprit?.subject} ${g2.stop})`);
check(g2.action === "explained" && g2.earlier?.pr === 3008, `follow-up of a reverted PR: explained, never reverted (got ${g2.action})`);
check(/re-land of #3008/.test(g.validateComment(cand3017, g2.set, [], "L", { earlier: g2.earlier })), "the comment tells the author to ship with the re-land");

// 4. Flake, env, unnamed, and a head already flagged.
let calls = 0;
let f = await g.validateGate({ head: m14, merges: [], state: {}, run: async (s, o) => { calls++; return o ? { verdict: "ok", failed: [] } : { verdict: "red", failed: ["validate-docs.mjs"] }; } });
check(f.verdict === "flake" && calls === 2, `red once, green on the rerun: flake (got ${f.verdict})`);
f = await g.validateGate({ head: m14, merges: [], state: {}, run: async () => ({ verdict: "infra", failed: [], why: "a lock was held" }) });
check(f.verdict === "env", "an infra refusal is no verdict");
f = await g.validateGate({ head: m14, merges: [], state: {}, run: async () => ({ verdict: "red", failed: [] }) });
check(f.verdict === "unnamed", "a failure that names no check is no verdict");
calls = 0;
f = await g.validateGate({ head: m14, merges: mergesFrom(base, m14), state: { validateRed: { checks: ["validate-docs.mjs"], pr: 12 } }, run: async (s, o) => { calls++; return runAt(s, o); } });
check(f.verdict === "known" && calls === 1, `still red with the flagged set: known, no second bisect (got ${f.verdict}, ${calls} runs)`);
f = await g.validateGate({ head: base, merges: [], state: {}, run: async () => ({ verdict: "ok", failed: [] }) });
check(f.verdict === "green", "green is green");
f = await g.validateGate({ head: m14, merges: [], state: {}, run: async (s, o) => ({ verdict: "red", failed: ["validate-docs.mjs"] }) });
check(f.verdict === "red" && f.culprit === null && f.action === "flag" && /no merge/.test(f.stop ?? ""), `no merge to blame: flagged without a culprit (got ${f.action}, ${f.stop})`);

// 5. The status line shows a red validate.
check(/validate RED \(validate-docs\.mjs after #12\)/.test(g.statusLine({ validateRed: { checks: ["validate-docs.mjs"], pr: 12 } }, { alive: 1 })), "statusLine names the red validate");

// 6. The wiring: the worker validates before it compiles, probes rerun only the failing checks, and the validate command honours it.
const main = readFileSync(join(repoRoot, "tools", "main-guard.mjs"), "utf8");
check(main.indexOf("validateHead(root, cdir, head)") > 0 && main.indexOf("validateHead(root, cdir, head)") < main.indexOf("await compileHead(root, cdir, head)"), "the worker validates a new head before it compiles it");
check(/mg\.VALIDATE_ONLY_ENV/.test(main) && /mg\.validateGate\(/.test(main) && /earlierRevertFor\(dir, b\.culprit/.test(main), "main-guard.mjs runs the gate, the probe env and the earlier-revert rule");
check(/mg\.staleRefusal\(drift\)/.test(main.slice(main.indexOf("async function validateHead"))), "the validate stage honours the stale-code rule");
check(/mg\.earlierRefusal\(/.test(main), "stage 2 refuses to revert the follow-up of a reverted PR");
const cli = readFileSync(join(repoRoot, "tools", "cli.mjs"), "utf8");
check(/LF_VALIDATE_ONLY/.test(cli), "validate reads LF_VALIDATE_ONLY");
check(g.VALIDATE_ONLY_ENV === "LF_VALIDATE_ONLY", "the env name matches the command's");

removeScratch(dir);
removeScratch(fixture2);
if (fails.length) {
  for (const m of fails) console.log(`FAIL  ${m}`);
  process.exit(1);
}
console.log("main-guard-validate-selftest: OK");
