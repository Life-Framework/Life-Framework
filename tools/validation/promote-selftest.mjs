// Proves the release-gate tooling (#399/#400/#401) in a scratch repo with a fake GitHub:
// inputs: tools/**, agent-box/**, .githooks/**
//   1. `promote` lists exactly the PRs merged since stable, writes the checklist with each PR's manual steps;
//   2. it refuses a non-fast-forward target, and a listing alone never moves stable;
//   3. `--confirm` fast-forwards stable, tags it, and verifies + closes only the issues of PRs in range;
//   4. a shipped PR (old "Closes #N" or new "Refs #N") leaves its issue OPEN with in-dev, or
//      needs-playtest when the PR lists manual steps; a PR that is not merged / a verified issue is left alone;
//   5. a failed issue becomes `regression` and reopens; --confirm refuses while its PR is in range;
//   6. `--revert` reverts one merge on main; `wt ship --closes` text becomes "Refs";
//   7. `--confirm` never verifies an issue labelled `regression` or one whose PR a later PR in the range reverted
//      (a re-land puts it back), and groups the label edits: issues with the same change share one edit call (#2980).
// Speed (2026-11-27): the scenes share nothing but code, so each is a section run in its own process in parallel
// (tools/lib/selftest-sections.mjs), and a scene's scratch history ("main with merge commits for PRs 11, 12, 13") is
// one `git fast-import` (tools/lib/selftest-git.mjs FastStream) instead of 6 to 7 git spawns per PR. Every assertion
// still runs, once, on the same commits.
// red-proof (2026-11-27, the sectioned file): the grouped-edit check flipped to 2 calls printed "label edits were not grouped", and 102's state flipped
//   printed "102 after confirm", each section exiting 1 and the run FAIL; both restored.
// red-proof: with the `regression` skip removed from verifyIssues it printed "confirm verified a regression-labelled issue";
//   with the reverted-issue skip removed it printed "confirm verified an issue whose PR was reverted";
//   with each issue edited alone it printed "label edits were not grouped";
//   (older:) with the ancestor check removed from promotionPlan it printed "moving stable backwards was
// accepted"; with planMerged's reopen forced false it printed "legacy Closes: issue 101 stayed closed".
import { gitSync } from "../lib/git-retry.mjs";
import { existsSync, readFileSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import * as rg from "../lib/release-gates.mjs";
import { describeRun, FastStream, fastImport, removeScratch, runGit, useHermeticGit } from "../lib/selftest-git.mjs";
import { sections } from "../lib/selftest-sections.mjs";
import { cmdPromote } from "../promote.mjs";
import { scratchDir } from "../lib/scratch.mjs";

let failed = 0;
const check = (ok, msg) => {
  if (!ok) {
    failed++;
    console.log(`FAIL  ${msg}`);
  }
};

useHermeticGit(); // identity, no machine git config, no auto-gc (tools/lib/selftest-git.mjs)
// A network-shaped git command (push, fetch, clone) is
// retried, because on the Windows CI runner a push into the scratch bare repo dropped now and then
// ("unexpected disconnect while reading sideband packet"; quorum 2026-11-16, promote 2026-11-16)
// while the selftests of the whole suite ran in parallel. Each of those commands is idempotent.
const sh = (cwd, ...args) => {
  const tries = ["push", "fetch", "clone"].includes(args[0]) ? 3 : 1;
  let r;
  for (let i = 0; i < tries; i++) {
    r = runGit(cwd, args);
    if (r.status === 0) break;
  }
  if (r.status !== 0) throw new Error(`git ${args.join(" ")}: ${describeRun(r)}`);
  return r.stdout.trim();
};

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
    editCalls: [],
    // One call edits many issues (gh issue edit n1 n2 ...).
    async editLabels(nums, add, remove) {
      this.editCalls.push({ nums: [...nums], add: [...add], remove: [...remove] });
      for (const n of nums) {
        await this.removeLabels(n, remove);
        if (add.length) await this.addLabels(n, add);
      }
    },
    async close(n) { issues[n].state = "closed"; },
    async reopen(n) { issues[n].state = "open"; },
  };
}

// The scene's repos: a bare origin with main = "base" plus one merge commit "Merge pull request #N" per PR in `merges`
// (each merged side is one commit that adds `<file>`), cloned into `work`. `stableAt` puts origin's stable branch on
// the merge commit of that PR. Returns { dir, origin, work, baseSha, merge: { [pr]: sha } }.
function sceneRepos(dir, merges, { stableAt } = {}) {
  const origin = join(dir, "origin.git");
  const work = join(dir, "work");
  sh(dir, "init", "--bare", "-b", "main", origin);
  writeFileSync(join(origin, "config"), `${readFileSync(join(origin, "config"), "utf8")}[receive]\n\tdenyNonFastForwards = true\n`);
  const fs = new FastStream();
  let mark = 1;
  let tip = mark;
  fs.commit("refs/heads/main", { mark, message: "base", files: { "base.txt": "base\n" } });
  const markOf = {};
  for (const [n, file] of merges) {
    const side = ++mark;
    fs.commit(`refs/tmp/pr${n}`, { mark: side, from: tip, message: `work for ${n}`, files: { [file]: `pr${n}\n` } });
    const m = ++mark;
    fs.commit("refs/heads/main", { mark: m, from: tip, merge: [side], message: `Merge pull request #${n} from x/ws/pr${n}`, files: { [file]: `pr${n}\n` } });
    markOf[n] = m;
    tip = m;
  }
  if (stableAt !== undefined) fs.reset("refs/heads/stable", stableAt === "base" ? 1 : markOf[stableAt]);
  const sha = fastImport(origin, fs);
  sh(dir, "clone", origin, work);
  const merge = {};
  for (const n of Object.keys(markOf)) merge[n] = sha[markOf[n]];
  return { dir, origin, work, baseSha: sha[1], merge };
}

// The two PR-state tables every scene starts from (PRs 11..13, issues 101..103).
function baseTables() {
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
  return { prs, issues, gh: fakeGh(prs, issues) };
}

// Runs one scene: its own scratch folder, removed at the end whatever happens.
const scene = (name, body) => {
  S.add(name, async () => {
    const dir = scratchDir("lf-promote-");
    try {
      await body(dir);
    } catch (err) {
      failed++;
      console.log(`FAIL  selftest crashed in '${name}': ${err.stack || err.message}`);
    } finally {
      removeScratch(dir);
    }
  });
};

const log = () => {};
const green = () => "green";
const S = sections(import.meta.url);

// ---- 4c. a ref to another PR is skipped with a note; real issues beside it are still labelled; text helpers (no git)
S.add("PR refs and text helpers", async () => {
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
});

// ---- 4. a merged PR moves its issues to in-dev / needs-playtest and keeps them open
//      (the same merges the chain scene sees, then the scenes that read the issues' labels)
async function mergePrs(gh, prs, issues, asserting) {
  const ok = asserting ? check : () => {};
  await rg.onPrMerged(gh, 11, log);
  ok(issues[101].state === "open", "legacy Closes: issue 101 stayed closed");
  ok(issues[101].labels.includes("in-dev") && !issues[101].labels.includes("agent:working"), `PR without manual steps: 101 labels ${issues[101].labels}`);
  await rg.onPrMerged(gh, 12, log);
  ok(issues[102].state === "open" && issues[102].labels.includes("needs-playtest") && !issues[102].labels.includes("in-dev"), `PR with manual steps: 102 labels ${issues[102].labels}`);
  prs[99] = { number: 99, title: "unmerged", body: "Refs #103", mergedAt: null };
  await rg.onPrMerged(gh, 99, log);
  ok(issues[103].labels.length === 1, "an unmerged PR changed an issue");
  await rg.onPrMerged(gh, 13, log);
  ok(issues[101].labels.includes("in-dev") && issues[103].labels.includes("in-dev"), "PR 13 did not label 101/103");
  // re-run is idempotent; needs-playtest is never downgraded
  await rg.onPrMerged(gh, 12, log); await rg.onPrMerged(gh, 11, log);
  ok(issues[102].labels.filter((l) => l === "needs-playtest").length === 1, "re-run duplicated a label");
}

// ---- 1/2. stable does not exist yet in a fresh repo, then --init creates it at main
scene("stable init", (dir) => {
  const { work, baseSha } = sceneRepos(dir, []);
  check(rg.resolveSha(work, "origin/stable") === null, "stable existed in a fresh scratch repo");
  const initSha = rg.initStable(work, "origin/main");
  check(initSha === baseSha, "init: stable not at main");
  let threw = false;
  try { rg.initStable(work); } catch { threw = true; }
  check(threw, "init twice: second init was accepted");
});

// ---- 1/2. listing, the checklist, and the plan up to #12 (stable at the base, main at #13)
scene("listing and checklist", async (dir) => {
  const { work, origin, merge } = sceneRepos(dir, [[11, "a.txt"], [12, "b.txt"], [13, "c.txt"]], { stableAt: "base" });
  const { prs, issues, gh } = baseTables();
  await mergePrs(gh, prs, issues, false);
  const stableBefore = sh(origin, "rev-parse", "stable");
  const res = await mute(() => cmdPromote(work, [], { gh, guardStatus: green }));
  check(res.code === 0, `list failed: ${res.out}`);
  check(/#11 /.test(res.out) && /#12 /.test(res.out) && /#13 /.test(res.out), `list missed a PR: ${res.out}`);
  check(sh(origin, "rev-parse", "stable") === stableBefore, "listing moved stable");
  const ck = join(work, "tmp", "promote", `checklist-${merge[13].slice(0, 8)}.md`);
  check(existsSync(ck), "no checklist written");
  const ckText = existsSync(ck) ? readFileSync(ck, "utf8") : "";
  check(/PR #12: Payphone/.test(ckText) && /dial 911/.test(ckText) && /PR #11/.test(ckText), `checklist content: ${ckText}`);

  const plan12 = rg.promotionPlan(work, { to: merge[12] });
  check(plan12.prs.join() === "11,12" && plan12.afterPrs.join() === "13", `plan to #12: ${plan12.prs} after ${plan12.afterPrs}`);
});

// ---- the chain: 4, then 5, 3, 2 (refusals), 3b, 5b; each step starts from the state the one before left
scene("merge, confirm, refuse, promote again, fail", async (dir) => {
  const { work, origin, baseSha, merge } = sceneRepos(dir, [[11, "a.txt"], [12, "b.txt"], [13, "c.txt"]], { stableAt: "base" });
  const { m11, m12, m13 } = { m11: merge[11], m12: merge[12], m13: merge[13] };
  const { prs, issues, gh } = baseTables();

  // ---- 4. a merged PR moves its issues to in-dev / needs-playtest and keeps them open
  await mergePrs(gh, prs, issues, true);
  const stableBefore = sh(origin, "rev-parse", "stable");

  // ---- 2. non-fast-forward: a side commit off the base is not a descendant of stable once stable moved
  const side = fastImport(work, new FastStream().commit("refs/heads/side", { mark: 1, from: baseSha, message: "side", files: { "side.txt": "s\n" } }))[1];

  // ---- 5. a failed issue whose PR is in range blocks --confirm
  let res = await mute(() => cmdPromote(work, ["--to", m12.slice(0, 12), "--confirm", "--fail", "102"], { gh, guardStatus: green }));
  check(res.code === 1 && /stable not moved/.test(res.out), `confirm with an in-range failure: ${res.out}`);
  check(sh(origin, "rev-parse", "stable") === stableBefore, "stable moved despite an in-range failure");
  check(issues[102].labels.includes("regression"), "failed issue lacks regression");
  await rg.markRegression(gh, 102, log); // idempotent
  issues[102].labels = issues[102].labels.filter((l) => l !== "regression"); // human re-tests it as fixed
  issues[102].labels.push("needs-playtest");

  // ---- 3. confirm up to #12: stable moves, tag, 102 verified+closed; 101 also refs PR 13 (after range) so it stays
  res = await mute(() => cmdPromote(work, ["--to", m12.slice(0, 12), "--confirm"], { gh, guardStatus: green }));
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
  res = await mute(() => cmdPromote(work, ["--to", side, "--confirm"], { gh, guardStatus: green }));
  check(res.code === 1 && /not a fast-forward/.test(res.out), `non-ff target was accepted: ${res.out}`);
  res = await mute(() => cmdPromote(work, ["--to", m11, "--confirm"], { gh, guardStatus: green }));
  check(res.code === 1 && /not a fast-forward/.test(res.out), `moving stable backwards was accepted: ${res.out}`);
  check(sh(origin, "rev-parse", "stable") === m12, "stable moved on a refusal");
  const direct = gitSync(["push", "origin", `${side}:refs/heads/stable`], { cwd: work, encoding: "utf8", windowsHide: true });
  check(direct.status === 0 ? false : true, "git accepted a raw non-ff push (bare repo should refuse with denyNonFastForwards)") ;

  // ---- 3b. second promotion to main: PR 13's issues verified, 101 now verified too
  res = await mute(() => cmdPromote(work, ["--confirm"], { gh, guardStatus: green }));
  check(res.code === 0 && sh(origin, "rev-parse", "stable") === m13, `second confirm: ${res.out}`);
  check(issues[101].state === "closed" && issues[103].state === "closed" && issues[101].labels.includes("verified"), "second promotion did not verify 101/103");
  res = await mute(() => cmdPromote(work, ["--confirm"], { gh, guardStatus: green }));
  check(res.code === 0 && /nothing to promote/.test(res.out), `promote with nothing new: ${res.out}`);

  // ---- 5b. fail after the fact
  res = await mute(() => cmdPromote(work, ["--fail", "103"], { gh, guardStatus: green }));
  check(issues[103].state === "open" && issues[103].labels.includes("regression") && !issues[103].labels.includes("verified"), `--fail: ${issues[103].state} ${issues[103].labels}`);
});

// ---- 7. --confirm skips a regression-labelled issue and an issue whose PR was reverted; batches label edits (#2980)
//      (stable is where the chain leaves it, at #13; PRs 14 to 20 are the range)
scene("regression and reverted issues", async (dir) => {
  const { work, origin, merge } = sceneRepos(dir, [[11, "a.txt"], [12, "b.txt"], [13, "c.txt"], [14, "d.txt"], [16, "e.txt"], [17, "f.txt"], [18, "g.txt"], [19, "h.txt"], [20, "i.txt"]], { stableAt: 13 });
  const m14 = merge[14];
  const m20 = merge[20];
  const prs = {};
  const issues = {};
  const gh = fakeGh(prs, issues);
  const merged = (n, title, body) => { prs[n] = { number: n, title, body, mergedAt: "t" }; };
  merged(14, "Two issues", "Refs #104, #105, #108");
  merged(16, "Gets reverted", "Refs #106");
  merged(17, "Revert 16", "Reverts #16");
  merged(18, "Reverted, then re-landed", "Refs #107");
  merged(19, "Revert 18", "Reverts #18");
  merged(20, "Re-land 18", "Refs #107");
  issues[104] = { state: "open", labels: ["scope", "in-dev", "regression"] }; // failed verification, fix not landed
  issues[105] = { state: "open", labels: ["scope", "in-dev"] };
  issues[106] = { state: "open", labels: ["scope", "in-dev"] };             // its PR was reverted in the range
  issues[107] = { state: "open", labels: ["scope", "in-dev"] };             // reverted, then re-landed by PR 20
  issues[108] = { state: "closed", labels: ["scope", "verified"] };         // already verified: no write at all
  gh.editCalls.length = 0;
  const res = await mute(() => cmdPromote(work, ["--to", m20.slice(0, 12), "--confirm"], { gh, guardStatus: green }));
  check(res.code === 0, `confirm over regression/revert issues failed: ${res.out}`);
  check(sh(origin, "rev-parse", "stable") === m20, "stable not at #20 after the regression confirm");
  check(issues[104].state === "open" && issues[104].labels.includes("regression") && !issues[104].labels.includes("verified"), `confirm verified a regression-labelled issue: ${issues[104].state} ${issues[104].labels}`);
  check(/#104: labelled regression/.test(res.out), `no skip line for #104: ${res.out}`);
  check(issues[106].state === "open" && !issues[106].labels.includes("verified"), `confirm verified an issue whose PR was reverted: ${issues[106].state} ${issues[106].labels}`);
  check(/#106: its PR was reverted/.test(res.out), `no skip line for #106: ${res.out}`);
  check(issues[105].state === "closed" && issues[105].labels.includes("verified") && !issues[105].labels.includes("in-dev"), `105 after confirm: ${issues[105].state} ${issues[105].labels}`);
  check(issues[107].state === "closed" && issues[107].labels.includes("verified"), `a re-landed issue was not verified: ${issues[107].state} ${issues[107].labels}`);
  check(issues[108].state === "closed" && issues[108].labels.join() === "scope,verified", "an already verified issue was rewritten");
  check(gh.editCalls.length === 1 && gh.editCalls[0].nums.join() === "105,107", `label edits were not grouped: ${JSON.stringify(gh.editCalls)}`);
  // markRegression's own labels stay out of the group; a --fail on an in-range issue is still refused (case 5)
  check(m14 !== m20, "scratch PRs did not advance main");
});

// ---- 6. revert one merge on main
scene("revert one merge", (dir) => {
  const { work, merge } = sceneRepos(dir, [[11, "a.txt"], [12, "b.txt"], [13, "c.txt"]]);
  check(rg.findMergeCommit(work, 12) === merge[12], "findMergeCommit(12)");
  check(rg.findMergeCommit(work, 77) === null, "findMergeCommit of an unknown PR");
  sh(work, "checkout", "-q", "-b", "revert12", "origin/main");
  rg.revertMerge(work, merge[12]);
  check(!existsSync(join(work, "b.txt")) && existsSync(join(work, "a.txt")) && existsSync(join(work, "c.txt")), "revert removed the wrong files");
});

process.exitCode = await S.run({
  failed: () => failed,
  summary: (n) => (n ? `promote-selftest: FAIL (${n})` : "promote-selftest: OK"),
});
