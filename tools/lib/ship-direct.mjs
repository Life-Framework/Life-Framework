// `wt ship` without a pre-merge gate (maintainer decision 2026-11-15, docs/worktrees.md "Ship"): a fast local
// preflight (merge-tree conflict check, validate of changed inputs), push, open or reuse the PR, auto-merge on.
// GitHub's required `validate` check is the only pre-merge gate; the main guard checks main after the merge.
// When GitHub refuses auto-merge because the branch is behind, main is merged in and pushed again.
// Every side effect is a dependency, so tools/validation/ship-direct-selftest.mjs drives it with fakes.

import { conflictKind } from "../prs.mjs";

// The local preflight's conflict check: the paths `git merge-tree --write-tree --name-only HEAD <base>`
// prints after the tree id when it exits 1. Driver-merged files (docs/features.md, the string table, the
// registry ...) the merge of origin/<base> resolves; anything else needs the agent first, so `wt ship`
// stops in seconds instead of GitHub reporting the PR CONFLICTING.
export function conflictPaths(mergeTreeOut) {
  const out = [];
  for (const l of String(mergeTreeOut).split(/\r?\n/).slice(1)) {
    if (!l.trim()) break; // a blank line ends the file list; the messages follow
    out.push(l.trim().replace(/\\/g, "/"));
  }
  return [...new Set(out)];
}
export function humanConflicts(paths) {
  return paths.filter((p) => conflictKind(p) === null);
}

// Merge origin/<base> for ship the way `prs heal` does (prs.mjs healMerge, passed in as heal): a conflict only
// in the generated registry or language confs is regenerated and committed instead of failing the ship.
// 28 of 55 failed ships on 2026-11-17 were a registry conflict the agent then fixed by hand.
export function shipMergeBase(wtRoot, base, heal, log = () => {}) {
  const m = heal(wtRoot, { base: `origin/${base}` });
  if (m.status === "up-to-date" || m.status === "merged" || m.status === "healed") {
    if (m.status === "healed") log(`ship: merged origin/${base}; regenerated ${(m.resolved ?? []).join(", ")}`);
    return m;
  }
  const files = (m.conflicted ?? []).length ? `conflicts in: ${m.conflicted.join(", ")} (` : "(";
  throw new Error(`merge origin/${base} into this branch failed: ${files}${m.reason}); run git merge origin/${base}, fix, commit, re-run`);
}

// Files both the branch and origin/<base> changed since their merge-base. `git merge-tree` runs the custom merge
// drivers (docs/features/*.md and friends), so it calls a both-sides append clean; GitHub runs no custom driver and
// reports the PR CONFLICTING (#3735, #3736: five crime-police.md PRs the same night). Any such file that carries a
// merge driver sends ship to merge origin/<base> locally first, where the driver resolves it.
export function bothSidesTouched(ours, theirs) {
  const norm = (p) => String(p).replace(/\\/g, "/");
  const t = new Set(theirs.map(norm));
  return [...new Set(ours.map(norm))].filter((p) => t.has(p));
}

export const BEHIND = /base branch was modified|not up to date|is behind|out of date|update the branch/i;
export const isBehindRefusal = (text) => BEHIND.test(String(text ?? ""));
// A push git refuses because the remote branch has commits this one lacks: the pr-autoheal workflow (or `prs heal`)
// merged main into the PR branch after the last ship.
export const REMOTE_AHEAD = /non-fast-forward|fetch first|\[rejected\]|updates were rejected/i;
export const isRemoteAhead = (text) => REMOTE_AHEAD.test(String(text ?? ""));

// deps: touchedTests() (optional) -> lines naming the `test --only` runs the diff needs (advice, never fails); driverOverlap() (optional) -> driver-merged files both sides changed; mergeTree() -> {status, stdout, stderr}; mergeBase() -> throws on failure; validate() -> exit code;
// push() -> {status, stdout, stderr}; findPr() -> pr|null; createPr() -> pr; autoMerge(n) -> {status, stdout, stderr};
// mergeState(n) -> "BEHIND"|"DIRTY"|...; pullBranch() (optional) merges origin/<branch> into HEAD, throws on a conflict; regenSchema() (optional) rebuilds and commits docs/config/ when the branch needs it; log(line).
// draft: the PR is opened as a draft and auto-merge is never planned (town-draft and maintainer-review PRs).
// Push; when the remote branch moved ahead (an auto-heal merge of main), merge it in once and push again.
function pushOrPull(deps, log) {
  const first = deps.push();
  if (first.status === 0 || !deps.pullBranch || !isRemoteAhead(`${first.stdout ?? ""}\n${first.stderr ?? ""}`)) return first;
  log("ship: the PR branch on GitHub has commits this worktree lacks (an auto-heal merge of main); merging them in");
  try { deps.pullBranch(); } catch (e) { return { status: 1, stdout: "", stderr: `merging the remote branch: ${e.message}` }; }
  return deps.push();
}

export async function shipDirect(deps, { base = "main", slug = "?", preflight = true, retries = 2, draft = false } = {}) {
  const log = deps.log ?? (() => {});
  const fail = (why) => { log(`FAIL  ${why}`); return { code: 1, why }; };
  // Rebuild docs/config/ when the branch touches a config class or conf (#3717); optional dependency, never fatal.
  const regen = () => { try { deps.regenSchema?.(); } catch (e) { log(`ship: config schema regeneration threw (${e.message}); validate will report it`); } };
  // merge-tree called the merge clean, but a driver-merged file changed on both sides: GitHub would call it CONFLICTING.
  const mergeForDrivers = () => {
    let files = [];
    try { files = deps.driverOverlap?.() ?? []; } catch (e) { log(`ship: driver-overlap check threw (${e.message}); GitHub reports any conflict`); }
    if (!files.length) return null;
    log(`ship: ${files.join(", ")} changed on both sides and GitHub cannot run the merge driver; merging origin/${base} here`);
    try { deps.mergeBase(); } catch (e) { return fail(`merge origin/${base}: ${e.message}. Nothing was pushed.`); }
    return null;
  };
  // Advice only (tools/lib/touched-tests.mjs): the tests this diff can break, which GitHub's validate does not run.
  try { for (const l of deps.touchedTests?.() ?? []) log(l); } catch (e) { log(`ship: test advice threw (${e.message})`); }
  if (preflight) {
    const mt = deps.mergeTree();
    if (mt.status === 1) {
      const paths = conflictPaths(mt.stdout);
      const human = humanConflicts(paths);
      if (human.length) return fail(`preflight: origin/${base} conflicts with this branch in ${human.join(", ")}; merge it: git merge origin/${base}, fix, commit, re-run 'tools\\cli wt ship ${slug}'. Nothing was pushed.`);
      log(`ship: origin/${base} conflicts only in driver-merged files (${paths.join(", ")}); merging it here`);
      try { deps.mergeBase(); } catch (e) { return fail(`merge origin/${base}: ${e.message}. Nothing was pushed.`); }
    } else if (mt.status !== 0) log(`ship: git merge-tree failed (${String(mt.stderr || mt.stdout).trim().slice(0, 200)}); GitHub reports any conflict`);
    if (mt.status !== 1) { const f = mergeForDrivers(); if (f) return f; }
    regen();
    const v = await deps.validate();
    if (v !== 0) return fail(`preflight: validate failed (exit ${v}); fix it, commit, re-run 'tools\\cli wt ship ${slug}'. Nothing was pushed.`);
  }
  if (!preflight) { const f = mergeForDrivers(); if (f) return f; regen(); }
  const push = pushOrPull(deps, log);
  if (push.status !== 0) return fail(`push:${String(push.stderr || push.stdout).trim()}`);
  let pr;
  let reused = false;
  try {
    pr = deps.findPr();
    reused = !!pr;
    if (!pr) pr = deps.createPr({ draft });
  } catch (e) {
    return fail(`PR creation: ${e.message}`);
  }
  log(`PR: ${pr.url}`);
  if (draft) {
    // A PR opened by an earlier ship (or by hand) may already be ready with auto-merge queued; --draft must undo
    // both, or the review gate (money, persistence, replication PRs) merges itself (#2940, PR #2938).
    if (reused) {
      const off = deps.disableAutoMerge?.(pr.number);
      if (off && off.status !== 0 && !/not enabled|isn't enabled|no auto/i.test(`${off.stdout ?? ""}\n${off.stderr ?? ""}`)) {
        return { ...fail(`draft: could not turn auto-merge off on PR ${pr.url}: ${`${off.stdout ?? ""}\n${off.stderr ?? ""}`.trim()}; run: gh pr merge ${pr.number} --disable-auto`), pr };
      }
      const dr = deps.convertToDraft?.(pr.number);
      if (dr && dr.status !== 0 && !/already/i.test(`${dr.stdout ?? ""}\n${dr.stderr ?? ""}`)) log(`ship: could not convert PR ${pr.number} to draft (${`${dr.stdout ?? ""}\n${dr.stderr ?? ""}`.trim()}); auto-merge is off`);
    }
    log(`draft: auto-merge off (REVIEW-NEEDED)`);
    return { code: 0, pr, draft: true };
  }
  for (let attempt = 0; ; attempt++) {
    const am = deps.autoMerge(pr.number);
    const text = `${am.stdout ?? ""}\n${am.stderr ?? ""}`;
    const behind = am.status !== 0 ? isBehindRefusal(text) : deps.mergeState?.(pr.number) === "BEHIND";
    if (am.status !== 0 && !behind) return { ...fail(`auto-merge: ${text.trim()} (PR is at ${pr.url})`), pr };
    if (!behind) break;
    if (attempt >= retries) return { ...fail(`auto-merge: the branch is still behind origin/${base} after ${retries} merge(s) (PR ${pr.url})`), pr };
    log(`ship: GitHub says the branch is behind origin/${base}; merging it in and pushing again`);
    try { deps.mergeBase(); } catch (e) { return { ...fail(`merge origin/${base}: ${e.message} (PR ${pr.url})`), pr }; }
    regen();
    const p2 = pushOrPull(deps, log);
    if (p2.status !== 0) return { ...fail(`push after merging origin/${base}: ${String(p2.stderr || p2.stdout).trim()}`), pr };
  }
  log(`ship: auto-merge on; GitHub merges once its validate check passes. The main guard checks main after the merge.`);
  return { code: 0, pr };
}
