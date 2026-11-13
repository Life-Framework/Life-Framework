// Proves the release-gate tooling (#399/#400/#401) in a scratch repo with a fake GitHub:
//   1. `promote` lists exactly the PRs merged since stable, writes the checklist with each PR's manual steps;
//   2. it refuses a non-fast-forward target, and a listing alone never moves stable;
//   3. `--confirm` fast-forwards stable, tags it, and verifies + closes only the issues of PRs in range;
//   4. a shipped PR (old "Closes #N" or new "Refs #N") leaves its issue OPEN with in-dev, or
//      needs-playtest when the PR lists manual steps; a PR that is not merged / a verified issue is left alone;
//   5. a failed issue becomes `regression` and reopens; --confirm refuses while its PR is in range;
//   6. `--revert` reverts one merge on main; `wt ship --closes` text becomes "Refs".
// red-proof: with the ancestor check removed from promotionPlan it printed "moving stable backwards was
// accepted"; with planMerged's reopen forced false it printed "legacy Closes: issue 101 stayed closed".
import { spawnSync } from "node:child_process";
import { existsSync, mkdirSync, mkdtempSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import * as rg from "../lib/release-gates.mjs";
import { cmdPromote } from "../promote.mjs";

const failures = [];
const check = (ok, msg) => {
  if (!ok) failures.push(msg);
};

Object.assign(process.env, {
  GIT_AUTHOR_NAME: "t", GIT_AUTHOR_EMAIL: "t@t", GIT_COMMITTER_NAME: "t", GIT_COMMITTER_EMAIL: "t@t",
});
const sh = (cwd, ...args) => {
  const r = spawnSync("git", args, { cwd, encoding: "utf8", windowsHide: true });
  if (r.status !== 0) throw new Error(`git ${args.join(" ")}: ${r.stderr}`);
  return r.stdout.trim();
};

const dir = mkdtempSync(join(tmpdir(), "lf-promote-"));
const origin = join(dir, "origin.git");
const work = join(dir, "work");
const mute = async (fn) => {
  const real = console.log;
  const lines = [];
  console.log = (...a) => lines.push(a.join(" "));
  try {
    return { code: await fn(), out: lines.join("\n") };
  } finally {
    console.log = real;
  }
};

// A fake GitHub: PRs and issues in memory, every write recorded.
function fakeGh(prs, issues) {
  return {
    prs, issues,
    async getPr(n) { return prs[n] ?? null; },
    async getIssue(n) { const i = issues[n]; return i ? { number: n, state: i.state, stateReason: i.stateReason ?? "", labels: [...i.labels] } : null; },
    // A PR number is an issue to gh issue view, but the labels call on it is refused (the #896 failure).
    async isPullRequest(n) { return Boolean(issues[n]?.pr); },
    async addLabels(n, l) {
      if (issues[n].pr) throw new Error(`label #${n}: GraphQL: Resource not accessible by integration (addLabelsToLabelable)`);
      for (const x of l) if (!issues[n].labels.includes(x)) issues[n].labels.push(x);
    },
    async removeLabels(n, l) { issues[n].labels = issues[n].labels.filter((x) => !l.includes(x)); },
    async close(n) { issues[n].state = "closed"; },
    async reopen(n) { issues[n].state = "open"; },
  };
}

try {
  // ---- scratch repo: main with merge commits "Merge pull request #N"
  sh(dir, "init", "--bare", "-b", "main", origin);
  sh(origin, "config", "receive.denyNonFastForwards", "true");
  sh(dir, "clone", origin, work);
  writeFileSync(join(work, "base.txt"), "base\n");
  sh(work, "add", "."); sh(work, "commit", "-m", "base"); sh(work, "push", "origin", "HEAD:main");
  const baseSha = sh(work, "rev-parse", "HEAD");
  const mergePr = (n, file) => {
    sh(work, "checkout", "-q", "-b", `ws/pr${n}`, "main");
    writeFileSync(join(work, file), `pr${n}\n`);
    sh(work, "add", "."); sh(work, "commit", "-m", `work for ${n}`);
    sh(work, "checkout", "-q", "main");
    sh(work, "merge", "--no-ff", "-m", `Merge pull request #${n} from x/ws/pr${n}`, `ws/pr${n}`);
    sh(work, "push", "origin", "main");
    return sh(work, "rev-parse", "HEAD");
  };

  // stable does not exist yet: a clear message, then --init creates it at main.
  check(rg.resolveSha(work, "origin/stable") === null, "stable existed in a fresh scratch repo");
  sh(work, "fetch", "origin");
  const initSha = rg.initStable(work, "origin/main");
  check(initSha === baseSha, "init: stable not at main");
  let threw = false;
  try { rg.initStable(work); } catch { threw = true; }
  check(threw, "init twice: second init was accepted");

  const m11 = mergePr(11, "a.txt");
  const m12 = mergePr(12, "b.txt");
  const m13 = mergePr(13, "c.txt");
  sh(work, "fetch", "origin");

  const prs = {
    11: { number: 11, title: "Store receipts", body: "Closes #101\n\n## Summary\nx\n", mergedAt: "t" },
    12: { number: 12, title: "Payphone", body: "Refs #102\n\n## Manual testing\n\n**Steps**\n\n1. Host a server and dial 911\n\n**Expected**\n\nA dial tone\n", mergedAt: "t" },
    13: { number: 13, title: "Later work", body: "Refs #103, #101\n", mergedAt: "t" },
  };
  const issues = {
    101: { state: "closed", labels: ["scope", "agent:working"] },   // closed by an old CLI's "Closes #101"
    102: { state: "open", labels: ["scope"] },
    103: { state: "open", labels: ["scope"] },
  };
  const gh = fakeGh(prs, issues);

  // ---- 4. a merged PR moves its issues to in-dev / needs-playtest and keeps them open
  const log = () => {};
  await rg.onPrMerged(gh, 11, log);
  check(issues[101].state === "open", "legacy Closes: issue 101 stayed closed");
  check(issues[101].labels.includes("in-dev") && !issues[101].labels.includes("agent:working"), `PR without manual steps: 101 labels ${issues[101].labels}`);
  await rg.onPrMerged(gh, 12, log);
  check(issues[102].state === "open" && issues[102].labels.includes("needs-playtest") && !issues[102].labels.includes("in-dev"), `PR with manual steps: 102 labels ${issues[102].labels}`);
  prs[99] = { number: 99, title: "unmerged", body: "Refs #103", mergedAt: null };
  await rg.onPrMerged(gh, 99, log);
  check(issues[103].labels.length === 1, "an unmerged PR changed an issue");
  await rg.onPrMerged(gh, 13, log);
  check(issues[101].labels.includes("in-dev") && issues[103].labels.includes("in-dev"), "PR 13 did not label 101/103");
  // re-run is idempotent; needs-playtest is never downgraded
  await rg.onPrMerged(gh, 12, log); await rg.onPrMerged(gh, 11, log);
  check(issues[102].labels.filter((l) => l === "needs-playtest").length === 1, "re-run duplicated a label");

  // ---- 4c. a ref to another PR is skipped with a note; real issues beside it are still labelled
  {
    const prsB = { 20: { number: 20, title: "Refs a PR", body: "Refs #21, #22\n", mergedAt: "t" } };
    const issuesB = { 21: { state: "open", labels: ["scope"], pr: true }, 22: { state: "open", labels: ["scope"] } };
    const ghB = fakeGh(prsB, issuesB);
    const notes = [];
    let refused = null;
    try { await rg.onPrMerged(ghB, 20, (s) => notes.push(s)); } catch (err) { refused = err; }
    check(!refused, `a PR ref failed the whole run: ${refused?.message}`);
    check(issuesB[21].labels.join() === "scope" && issuesB[21].state === "open", `a PR ref was labelled: ${issuesB[21].labels}`);
    check(issuesB[22].labels.includes("in-dev"), `a real issue beside a PR ref was not labelled: ${issuesB[22].labels}`);
    check(notes.some((s) => /#21/.test(s) && /pull request/i.test(s) && /skipp?ed/i.test(s)), `no one-line skip note for PR #21: ${notes}`);
  }

  // text helpers
  check(rg.parseRefs("Closes #5, #6 and #7. Fixes #8\nPart of #9\nRefs #10").join() === "5,6,7,8,10", `parseRefs: ${rg.parseRefs("Closes #5, #6 and #7. Fixes #8\nPart of #9\nRefs #10")}`);
  check(!rg.hasManualSteps("## Manual testing\n\n<!-- Delete if N/A -->\n\n**Steps**\n\n1.\n\n**Expected**\n"), "template boilerplate counted as manual steps");
  check(!rg.hasManualSteps("## Manual steps\n\nNone.\n"), "'None.' counted as manual steps");
  check(rg.hasManualSteps("## Manual steps\n\nHost, join, restart.\n"), "prose manual steps not seen");
  check(!rg.hasManualSteps("## Summary\nx\n"), "no manual section counted as manual");
  const shipped = rg.refsBody("Closes #5\n\nFixes #6 and more", 5);
  check(!/closes|fixes/i.test(shipped) && /Refs #5/.test(shipped) && /Refs #6/.test(shipped), `refsBody: ${shipped}`);
  check(rg.refsBody("body", 7).startsWith("Refs #7\n\n"), "refsBody did not prepend");

  // ---- 1/2. listing
  const stableBefore = sh(origin, "rev-parse", "stable");
  let res = await mute(() => cmdPromote(work, [], { gh }));
  check(res.code === 0, `list failed: ${res.out}`);
  check(/#11 /.test(res.out) && /#12 /.test(res.out) && /#13 /.test(res.out), `list missed a PR: ${res.out}`);
  check(sh(origin, "rev-parse", "stable") === stableBefore, "listing moved stable");
  const ck = join(work, "tmp", "promote", `checklist-${m13.slice(0, 8)}.md`);
  check(existsSync(ck), "no checklist written");
  const ckText = existsSync(ck) ? readFileSync(ck, "utf8") : "";
  check(/PR #12: Payphone/.test(ckText) && /dial 911/.test(ckText) && /PR #11/.test(ckText), `checklist content: ${ckText}`);

  // ---- 2. non-fast-forward: a side commit off the base is not a descendant of stable once stable moved
  const plan12 = rg.promotionPlan(work, { to: m12 });
  check(plan12.prs.join() === "11,12" && plan12.afterPrs.join() === "13", `plan to #12: ${plan12.prs} after ${plan12.afterPrs}`);
  sh(work, "checkout", "-q", "-b", "side", baseSha);
  writeFileSync(join(work, "side.txt"), "s\n");
  sh(work, "add", "."); sh(work, "commit", "-m", "side");
  const side = sh(work, "rev-parse", "HEAD");
  sh(work, "checkout", "-q", "main");

  // ---- 5. a failed issue whose PR is in range blocks --confirm
  res = await mute(() => cmdPromote(work, ["--to", m12.slice(0, 12), "--confirm", "--fail", "102"], { gh }));
  check(res.code === 1 && /stable not moved/.test(res.out), `confirm with an in-range failure: ${res.out}`);
  check(sh(origin, "rev-parse", "stable") === stableBefore, "stable moved despite an in-range failure");
  check(issues[102].labels.includes("regression"), "failed issue lacks regression");
  await rg.markRegression(gh, 102, log); // idempotent
  issues[102].labels = issues[102].labels.filter((l) => l !== "regression"); // human re-tests it as fixed
  issues[102].labels.push("needs-playtest");

  // ---- 3. confirm up to #12: stable moves, tag, 102 verified+closed; 101 also refs PR 13 (after range) so it stays
  res = await mute(() => cmdPromote(work, ["--to", m12.slice(0, 12), "--confirm"], { gh }));
  check(res.code === 0, `confirm failed: ${res.out}`);
  check(sh(origin, "rev-parse", "stable") === m12, "stable not at #12 after confirm");
  check(/^stable-\d{8}-/.test(sh(origin, "tag", "--list", "stable-*")), "no stable tag pushed");
  check(issues[102].state === "closed" && issues[102].labels.includes("verified") && !issues[102].labels.includes("needs-playtest"), `102 after confirm: ${issues[102].state} ${issues[102].labels}`);
  check(issues[101].state === "open" && issues[101].labels.includes("in-dev"), `101 (also in PR 13) was touched: ${issues[101].state} ${issues[101].labels}`);
  check(issues[103].state === "open", "103 closed before its PR was verified");
  // a verified issue is left alone by a later merge
  await rg.onPrMerged(gh, 12, log);
  check(issues[102].state === "closed", "a verified issue was reopened by a re-run");

  // ---- 2. refusals: side commit, and moving back
  res = await mute(() => cmdPromote(work, ["--to", side, "--confirm"], { gh }));
  check(res.code === 1 && /not a fast-forward/.test(res.out), `non-ff target was accepted: ${res.out}`);
  res = await mute(() => cmdPromote(work, ["--to", m11, "--confirm"], { gh }));
  check(res.code === 1 && /not a fast-forward/.test(res.out), `moving stable backwards was accepted: ${res.out}`);
  check(sh(origin, "rev-parse", "stable") === m12, "stable moved on a refusal");
  const direct = spawnSync("git", ["push", "origin", `${side}:refs/heads/stable`], { cwd: work, encoding: "utf8", windowsHide: true });
  check(direct.status === 0 ? false : true, "git accepted a raw non-ff push (bare repo should refuse with denyNonFastForwards)") ;

  // ---- 3b. second promotion to main: PR 13's issues verified, 101 now verified too
  res = await mute(() => cmdPromote(work, ["--confirm"], { gh }));
  check(res.code === 0 && sh(origin, "rev-parse", "stable") === m13, `second confirm: ${res.out}`);
  check(issues[101].state === "closed" && issues[103].state === "closed" && issues[101].labels.includes("verified"), "second promotion did not verify 101/103");
  res = await mute(() => cmdPromote(work, ["--confirm"], { gh }));
  check(res.code === 0 && /nothing to promote/.test(res.out), `promote with nothing new: ${res.out}`);

  // ---- 5b. fail after the fact
  res = await mute(() => cmdPromote(work, ["--fail", "103"], { gh }));
  check(issues[103].state === "open" && issues[103].labels.includes("regression") && !issues[103].labels.includes("verified"), `--fail: ${issues[103].state} ${issues[103].labels}`);

  // ---- 6. revert one merge on main
  sh(work, "fetch", "origin");
  check(rg.findMergeCommit(work, 12) === m12, "findMergeCommit(12)");
  check(rg.findMergeCommit(work, 77) === null, "findMergeCommit of an unknown PR");
  sh(work, "checkout", "-q", "-b", "revert12", "origin/main");
  rg.revertMerge(work, m12);
  check(!existsSync(join(work, "b.txt")) && existsSync(join(work, "a.txt")) && existsSync(join(work, "c.txt")), "revert removed the wrong files");
} catch (err) {
  failures.push(`selftest crashed: ${err.stack || err.message}`);
} finally {
  try { rmSync(dir, { recursive: true, force: true }); } catch { /* temp scratch only */ }
}

if (failures.length) {
  console.log("promote-selftest: FAIL");
  for (const f of failures) console.log("  " + f);
  process.exit(1);
}
console.log("promote-selftest: OK");
