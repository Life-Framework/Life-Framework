// Run by .github/workflows/pr-autoheal.yml on every push to main (and every 20 minutes).
//
// GitHub runs no custom merge driver, so two PRs that both append rows to a driver-merged file
// (docs/features/*.md, the string table, the language confs, debug-log-triage.md, the island baseline ...)
// go CONFLICTING the moment one merges, although the repo's drivers merge them cleanly. `merge=union` does not
// help: GitHub ignores merge attributes too (tested 2026-11-27, PRs #5030-#5033).
//
// For each open same-repo, non-draft PR into BASE:
//   1. `git merge-tree` without drivers (GitHub's view). Clean: nothing to do.
//   2. Every conflicted path must carry a `merge=lf-*` attribute; any other conflict is left for a human.
//   3. Merge origin/BASE into the PR head with tools/merge/drivers.gitconfig included. A driver that fails
//      leaves the PR alone.
//   4. Push the merge commit (never forced: a push that lost a race with the agent is skipped).
//   5. A push made with GITHUB_TOKEN starts no workflow, so the PR's required `validate` check would never
//      report on the new head. workflow_dispatch is the documented exception: dispatch validate on the branch.
//
// Env: GH_TOKEN, BASE (default main), HEAD_PREFIX (only heads starting with it; the scratch experiment),
// DRY_RUN=1 (report, push nothing). A PR labeled `no-autoheal` is skipped.
import { spawnSync } from "node:child_process";
import { copyFileSync, mkdtempSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";

const run = (cmd, args, opts = {}) => {
  const r = spawnSync(cmd, args, { encoding: "utf8", maxBuffer: 64 * 1024 * 1024, ...opts });
  return { status: r.status ?? 1, stdout: r.stdout ?? "", stderr: r.stderr ?? "" };
};
const git = (args) => run("git", args);

// `git merge-tree --write-tree --name-only` output: tree oid, then one conflicted path per line, then a blank line.
export function conflictedPaths(stdout) {
  const lines = String(stdout).split(/\r?\n/);
  const out = [];
  for (const l of lines.slice(1)) {
    if (!l.trim()) break;
    out.push(l.trim());
  }
  return [...new Set(out)];
}

// `git check-attr merge -- <paths>` lines "path: merge: <value>". Paths with no lf-* driver need a human.
export function withoutDriver(paths, checkAttrStdout) {
  const driven = new Set();
  for (const l of String(checkAttrStdout).split(/\r?\n/)) {
    const m = /^(.*): merge: (lf-\S+)$/.exec(l.trim());
    if (m) driven.add(m[1]);
  }
  return paths.filter((p) => !driven.has(p));
}

// Which PRs to look at.
export function candidates(prs, { headPrefix = "" } = {}) {
  return prs.filter((p) => !p.isDraft && !p.isCrossRepository
    && !(p.labels ?? []).some((l) => l.name === "no-autoheal")
    && (!headPrefix || p.headRefName.startsWith(headPrefix)));
}

function healOne(pr, { base, drivers, dryRun }) {
  const head = pr.headRefName;
  const f = git(["fetch", "-q", "origin", `+refs/heads/${head}:refs/remotes/origin/${head}`]);
  if (f.status !== 0) return { pr, result: "skip", why: `fetch failed: ${f.stderr.trim()}` };
  const mt = git(["merge-tree", "--write-tree", "--name-only", `origin/${base}`, `origin/${head}`]);
  if (mt.status === 0) return { pr, result: "clean" };
  if (mt.status !== 1) return { pr, result: "skip", why: `merge-tree failed: ${mt.stderr.trim().slice(0, 200)}` };
  const paths = conflictedPaths(mt.stdout);
  // The PR head's .gitattributes decides which driver git runs, so read the attributes there.
  const attr = git(["check-attr", "--source", `origin/${head}`, "merge", "--", ...paths]);
  const human = withoutDriver(paths, attr.stdout);
  if (human.length) return { pr, result: "human", why: human.join(", ") };
  if (dryRun) return { pr, result: "would-heal", why: paths.join(", ") };
  git(["checkout", "-q", "-B", "autoheal", `origin/${head}`]);
  const m = git(["-c", `include.path=${drivers}`, "merge", "--no-edit", "-m",
    `Merge origin/${base} into ${head} (pr-autoheal: driver-merged ${paths.join(", ")})`, `origin/${base}`]);
  if (m.status !== 0) {
    git(["merge", "--abort"]);
    return { pr, result: "skip", why: `a driver could not merge: ${(m.stdout + m.stderr).trim().split("\n").slice(-3).join(" | ")}` };
  }
  const p = git(["push", "origin", `HEAD:refs/heads/${head}`]);
  if (p.status !== 0) return { pr, result: "skip", why: `push refused (the agent pushed, or a workflow file changed): ${p.stderr.trim().split("\n").slice(-2).join(" | ")}` };
  const d = run("gh", ["workflow", "run", "validate.yml", "--ref", head, "-f", "all=false"]);
  const sha = git(["rev-parse", "HEAD"]).stdout.trim();
  return { pr, result: "healed", why: paths.join(", "), sha, dispatched: d.status === 0 ? "validate dispatched" : `validate dispatch failed: ${d.stderr.trim()}` };
}

function main() {
  const base = process.env.BASE || "main";
  const headPrefix = process.env.HEAD_PREFIX || "";
  const dryRun = process.env.DRY_RUN === "1";
  // A copy: the checkout moves to each PR head, which may carry an older drivers file or none.
  const drivers = join(mkdtempSync(join(tmpdir(), "lf-autoheal-")), "drivers.gitconfig");
  copyFileSync("tools/merge/drivers.gitconfig", drivers);
  const home = git(["rev-parse", "HEAD"]).stdout.trim();
  git(["config", "user.name", "github-actions[bot]"]);
  git(["config", "user.email", "41898282+github-actions[bot]@users.noreply.github.com"]);
  git(["fetch", "-q", "origin", `+refs/heads/${base}:refs/remotes/origin/${base}`]);
  const list = run("gh", ["pr", "list", "--base", base, "--state", "open", "--limit", "200",
    "--json", "number,headRefName,isDraft,isCrossRepository,labels"]);
  if (list.status !== 0) { console.log(`pr-autoheal: gh pr list failed: ${list.stderr.trim()}`); return 1; }
  const prs = candidates(JSON.parse(list.stdout), { headPrefix });
  let healed = 0;
  for (const pr of prs) {
    const r = healOne(pr, { base, drivers, dryRun });
    git(["checkout", "-q", "--force", "--detach", home]); // back to the workflow's own commit for the next PR
    if (r.result === "clean") continue;
    if (r.result === "healed") healed++;
    console.log(`#${pr.number} ${pr.headRefName}: ${r.result}${r.why ? ` (${r.why})` : ""}${r.sha ? ` head ${r.sha}` : ""}${r.dispatched ? `; ${r.dispatched}` : ""}`);
  }
  console.log(`SUMMARY pr-autoheal ok base=${base} looked=${prs.length} healed=${healed}${dryRun ? " (dry run)" : ""}`);
  return 0;
}

const invoked = process.argv[1] && process.argv[1].replace(/\\/g, "/").endsWith("tools/pr-autoheal.mjs");
if (invoked) process.exit(main());
