// `wt ship` without a pre-merge gate (maintainer decision 2026-11-15, docs/worktrees.md "Ship"): a fast local
// preflight (merge-tree conflict check, validate of changed inputs), push, open or reuse the PR, auto-merge on.
// GitHub's required `validate` check is the only pre-merge gate; the main guard checks main after the merge.
// When GitHub refuses auto-merge because the branch is behind, main is merged in and pushed again.
// Every side effect is a dependency, so tools/validation/ship-direct-selftest.mjs drives it with fakes.

import { conflictPaths, humanConflicts } from "./ship-queue.mjs";

export const BEHIND = /base branch was modified|not up to date|is behind|out of date|update the branch/i;
export const isBehindRefusal = (text) => BEHIND.test(String(text ?? ""));

// deps: mergeTree() -> {status, stdout, stderr}; mergeBase() -> throws on failure; validate() -> exit code;
// push() -> {status, stdout, stderr}; findPr() -> pr|null; createPr() -> pr; autoMerge(n) -> {status, stdout, stderr};
// mergeState(n) -> "BEHIND"|"DIRTY"|...; log(line).
export async function shipDirect(deps, { base = "main", slug = "?", preflight = true, retries = 2 } = {}) {
  const log = deps.log ?? (() => {});
  const fail = (why) => { log(`FAIL  ${why}`); return { code: 1, why }; };
  if (preflight) {
    const mt = deps.mergeTree();
    if (mt.status === 1) {
      const paths = conflictPaths(mt.stdout);
      const human = humanConflicts(paths);
      if (human.length) return fail(`preflight: origin/${base} conflicts with this branch in ${human.join(", ")}; merge it: git merge origin/${base}, fix, commit, re-run 'tools\\cli wt ship ${slug}'. Nothing was pushed.`);
      log(`ship: origin/${base} conflicts only in driver-merged files (${paths.join(", ")}); merging it here`);
      try { deps.mergeBase(); } catch (e) { return fail(`merge origin/${base}: ${e.message}. Nothing was pushed.`); }
    } else if (mt.status !== 0) log(`ship: git merge-tree failed (${String(mt.stderr || mt.stdout).trim().slice(0, 200)}); GitHub reports any conflict`);
    const v = await deps.validate();
    if (v !== 0) return fail(`preflight: validate failed (exit ${v}); fix it, commit, re-run 'tools\\cli wt ship ${slug}'. Nothing was pushed.`);
  }
  const push = deps.push();
  if (push.status !== 0) return fail(`push: ${String(push.stderr || push.stdout).trim()}`);
  let pr;
  try {
    pr = deps.findPr() ?? deps.createPr();
  } catch (e) {
    return fail(`PR creation: ${e.message}`);
  }
  log(`PR: ${pr.url}`);
  for (let attempt = 0; ; attempt++) {
    const am = deps.autoMerge(pr.number);
    const text = `${am.stdout ?? ""}\n${am.stderr ?? ""}`;
    const behind = am.status !== 0 ? isBehindRefusal(text) : deps.mergeState?.(pr.number) === "BEHIND";
    if (am.status !== 0 && !behind) return { ...fail(`auto-merge: ${text.trim()} (PR is at ${pr.url})`), pr };
    if (!behind) break;
    if (attempt >= retries) return { ...fail(`auto-merge: the branch is still behind origin/${base} after ${retries} merge(s) (PR ${pr.url})`), pr };
    log(`ship: GitHub says the branch is behind origin/${base}; merging it in and pushing again`);
    try { deps.mergeBase(); } catch (e) { return { ...fail(`merge origin/${base}: ${e.message} (PR ${pr.url})`), pr }; }
    const p2 = deps.push();
    if (p2.status !== 0) return { ...fail(`push after merging origin/${base}: ${String(p2.stderr || p2.stdout).trim()}`), pr };
  }
  log(`ship: auto-merge on; GitHub merges once its validate check passes. The main guard checks main after the merge.`);
  return { code: 0, pr };
}
