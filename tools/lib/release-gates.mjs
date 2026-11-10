// Release gates (epic #399): issue states and the main -> stable promotion.
//
// Agents merge to `main`. An issue then sits at `in-dev` (or `needs-playtest`
// when the PR lists manual steps) and stays open. A human playtests `main`;
// `tools\cli promote --confirm` fast-forwards `stable` to the verified commit,
// marks the issues of the PRs in that range `verified` and closes them.
// docs/worktrees.md "Release gates" has the human side.
//
// Everything here takes a `gh` adapter (realGh below, a fake in
// tools/validation/promote-selftest.mjs) and a git root, so the logic is
// testable in a scratch repo with no GitHub.
import { mkdirSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ghBin, git, run } from "../wt.mjs";

export const STATE_LABELS = ["in-dev", "needs-playtest", "verified", "regression"];
export const LABEL_SPECS = {
  "in-dev": ["0e8a16", "Merged to main, not yet verified by a human"],
  "needs-playtest": ["fbca04", "Merged to main; the PR lists manual steps to verify"],
  verified: ["1d76db", "A human signed this off; the issue is closed"],
  regression: ["d73a4a", "Failed verification; the fix lands on main"],
};
const WORKING = "agent:working";

// ------------------------------------------------------------------ PR text

// Issue numbers a PR body points at: "Refs #1", "Closes #2, #3", "Fixes #4 and #5".
// "Part of #N" (an epic link) is not a ref.
export function parseRefs(body) {
  const out = [];
  const re = /\b(?:closes|closed|fixes|fixed|resolves|resolved|refs?)\s*:?\s+((?:#\d+)(?:\s*(?:,|and|&)\s*#\d+)*)/gi;
  for (const m of String(body ?? "").matchAll(re)) {
    for (const n of m[1].matchAll(/#(\d+)/g)) out.push(Number(n[1]));
  }
  return [...new Set(out)];
}

// Merging a PR never closes an issue (release gates, #401): "Closes/Fixes/Resolves #N" becomes
// "Refs #N", and --refs N adds one. The issue-state workflow moves it to in-dev / needs-playtest.
export function refsBody(body, n) {
  let out = body.replace(/\b(closes|closed|fixes|fixed|resolves|resolved)(\s*:?\s+#\d+)/gi, "Refs$2");
  if (n && !new RegExp("\\brefs?\\s*:?\\s+#" + n + "\\b", "i").test(out)) out = "Refs #" + n + "\n\n" + out;
  return out;
}

// "Reverts #N" in a revert PR body.
export function parseReverts(body) {
  const out = [];
  for (const m of String(body ?? "").matchAll(/^\s*reverts\s+#(\d+)/gim)) out.push(Number(m[1]));
  return [...new Set(out)];
}

// The manual-steps text of a PR body, or "" when it lists none. The PR
// template's "Manual testing" section counts only when it holds a real step.
export function manualSteps(body) {
  const text = String(body ?? "").replace(/\r\n/g, "\n").replace(/<!--[\s\S]*?-->/g, "");
  const lines = text.split("\n");
  const start = lines.findIndex((l) => /^#{1,6}\s*manual\b/i.test(l));
  if (start === -1) return "";
  let end = lines.length;
  for (let i = start + 1; i < lines.length; i++) {
    if (/^#{1,6}\s/.test(lines[i])) {
      end = i;
      break;
    }
  }
  const section = lines.slice(start + 1, end).join("\n").trim();
  const flat = section.replace(/[*_`]/g, "").trim();
  if (!flat) return "";
  if (/^(none|n\/a|na|no manual steps?|nothing)\.?$/i.test(flat)) return "";
  const lead = flat.replace(/^steps?:?/i, "").trim();
  // Template boilerplate: bare "Steps / 1. / Expected" with nothing written.
  const written = flat.split("\n").map((l) => l.replace(/^\s*(\d+\.|[-*])\s*/, "").trim()).filter((l) => l && !/^(steps?|expected):?$/i.test(l));
  if (written.length === 0 || !lead) return "";
  return section;
}

export function hasManualSteps(body) {
  return manualSteps(body) !== "";
}

// ------------------------------------------------------------ issue states

// Labels to add/remove and whether to reopen when a PR that refs an issue merges.
// A verified issue is left alone; needs-playtest is never downgraded to in-dev.
export function planMerged(issue, manual) {
  const labels = issue.labels ?? [];
  if (labels.includes("verified")) return { add: [], remove: [], reopen: false, skip: "already verified" };
  let target = manual ? "needs-playtest" : "in-dev";
  if (!manual && labels.includes("needs-playtest")) target = "needs-playtest";
  const add = labels.includes(target) ? [] : [target];
  const remove = [WORKING, ...STATE_LABELS.filter((l) => l !== target)].filter((l) => labels.includes(l));
  // An old CLI's "Closes #N" closed it on merge; a human's "not planned" close stays closed.
  const reopen = issue.state === "closed" && issue.stateReason !== "not_planned";
  return { add, remove, reopen, target };
}

// A merged PR (base main): move every issue it refs to in-dev / needs-playtest.
// A merged "Reverts #P" PR sends that PR's issues back to open + regression.
export async function onPrMerged(gh, prNumber, log = () => {}) {
  const pr = await gh.getPr(prNumber);
  if (!pr) throw new Error(`PR #${prNumber} not found`);
  if (!pr.mergedAt) {
    log(`PR #${prNumber} is not merged; nothing to do`);
    return [];
  }
  const manual = hasManualSteps(pr.body);
  const done = [];
  for (const n of parseRefs(pr.body)) {
    const issue = await gh.getIssue(n);
    if (!issue) {
      log(`#${n}: not found (PR #${prNumber} refs it)`);
      continue;
    }
    const plan = planMerged(issue, manual);
    if (plan.skip) {
      log(`#${n}: ${plan.skip}; left alone`);
      continue;
    }
    if (plan.remove.length) await gh.removeLabels(n, plan.remove);
    if (plan.add.length) await gh.addLabels(n, plan.add);
    if (plan.reopen) await gh.reopen(n, `PR #${prNumber} merged to main. Open until a human verifies it (\`tools\\cli promote --confirm\`).`);
    log(`#${n}: ${plan.target}${plan.reopen ? " (reopened)" : ""}`);
    done.push({ issue: n, state: plan.target, reopened: plan.reopen });
  }
  for (const reverted of parseReverts(pr.body)) {
    const old = await gh.getPr(reverted);
    for (const n of parseRefs(old?.body)) {
      const issue = await gh.getIssue(n);
      if (!issue || issue.labels.includes("verified")) continue;
      const remove = ["in-dev", "needs-playtest"].filter((l) => issue.labels.includes(l));
      if (remove.length) await gh.removeLabels(n, remove);
      if (!issue.labels.includes("regression")) await gh.addLabels(n, ["regression"]);
      log(`#${n}: regression (PR #${reverted} reverted by #${prNumber})`);
      done.push({ issue: n, state: "regression", reopened: false });
    }
  }
  return done;
}

export async function markVerified(gh, n, via, log = () => {}) {
  const issue = await gh.getIssue(n);
  if (!issue) {
    log(`#${n}: not found`);
    return false;
  }
  const remove = [WORKING, ...STATE_LABELS.filter((l) => l !== "verified")].filter((l) => issue.labels.includes(l));
  if (remove.length) await gh.removeLabels(n, remove);
  if (!issue.labels.includes("verified")) await gh.addLabels(n, ["verified"]);
  if (issue.state !== "closed") await gh.close(n, `Verified by a human; \`stable\` now contains it (${via}).`);
  log(`#${n}: verified, closed`);
  return true;
}

export async function markRegression(gh, n, log = () => {}) {
  const issue = await gh.getIssue(n);
  if (!issue) {
    log(`#${n}: not found`);
    return false;
  }
  const remove = ["verified", "in-dev", "needs-playtest"].filter((l) => issue.labels.includes(l));
  if (remove.length) await gh.removeLabels(n, remove);
  if (!issue.labels.includes("regression")) await gh.addLabels(n, ["regression"]);
  if (issue.state === "closed") await gh.reopen(n, "Failed verification. The fix lands on `main`, or the change is reverted there (`tools\\cli promote --revert <PR>`).");
  log(`#${n}: regression, open`);
  return true;
}

// ----------------------------------------------------------------- promote

export function checkRefName(name) {
  if (!/^[A-Za-z0-9._\/-]+$/.test(name) || name.startsWith("-")) throw new Error(`bad ref '${name}'`);
  return name;
}

function must(res, what) {
  if (res.status !== 0) throw new Error(`${what}: ${(res.stderr || res.stdout).trim()}`);
  return res.stdout.trim();
}

export function resolveSha(root, ref) {
  const r = git(root, ["rev-parse", "--verify", "--quiet", `${checkRefName(ref)}^{commit}`]);
  return r.status === 0 ? r.stdout.trim() : null;
}

export function isAncestor(root, a, b) {
  return git(root, ["merge-base", "--is-ancestor", a, b]).status === 0;
}

// PR number of a first-parent commit on main: a merge commit or a squash "(#N)".
export function prOfSubject(subject) {
  const m = /^Merge pull request #(\d+)\b/.exec(subject) || /\(#(\d+)\)\s*$/.exec(subject);
  return m ? Number(m[1]) : null;
}

// First-parent commits in (from, to], oldest first, each with its PR number or null.
export function commitsBetween(root, from, to) {
  const range = from ? `${from}..${to}` : to;
  const out = must(git(root, ["log", "--first-parent", "--reverse", "--format=%H%x09%s", range]), "git log");
  return out
    .split(/\r?\n/)
    .filter(Boolean)
    .map((l) => {
      const i = l.indexOf("\t");
      const subject = l.slice(i + 1);
      return { sha: l.slice(0, i), subject, pr: prOfSubject(subject) };
    });
}

export function fetchAll(root) {
  must(git(root, ["fetch", "origin", "--prune", "--tags", "--quiet"]), "git fetch");
}

// The state of a promotion: what is on stable, where it would move, which PRs go with it.
export function promotionPlan(root, { to } = {}) {
  const stable = resolveSha(root, "origin/stable");
  if (!stable) throw new Error("origin/stable does not exist; create it once with 'tools\\cli promote --init'");
  const target = resolveSha(root, to ?? "origin/main");
  if (!target) throw new Error(`cannot resolve ${to ?? "origin/main"}`);
  if (!isAncestor(root, stable, target)) {
    throw new Error(`not a fast-forward: origin/stable (${stable.slice(0, 8)}) is not an ancestor of ${target.slice(0, 8)}. stable only moves forward; pick a commit on main's history`);
  }
  const commits = commitsBetween(root, stable, target);
  const main = resolveSha(root, "origin/main");
  const after = main && main !== target && isAncestor(root, target, main) ? commitsBetween(root, target, main) : [];
  return { stable, target, commits, prs: commits.filter((c) => c.pr).map((c) => c.pr), direct: commits.filter((c) => !c.pr), afterPrs: after.filter((c) => c.pr).map((c) => c.pr) };
}

export async function buildChecklist(gh, plan) {
  const lines = [];
  lines.push(`# Promotion checklist: stable ${plan.stable.slice(0, 8)} -> ${plan.target.slice(0, 8)}`);
  lines.push("");
  lines.push(`${plan.prs.length} PR(s) merged to main since the last stable. Playtest main at ${plan.target.slice(0, 8)}, tick each item, then:`);
  lines.push("");
  lines.push("```");
  lines.push(`tools\\cli promote --to ${plan.target.slice(0, 12)} --confirm            # all pass`);
  lines.push(`tools\\cli promote --to ${plan.target.slice(0, 12)} --confirm --fail 123  # fail #123 (regression, stays open)`);
  lines.push("```");
  lines.push("");
  const issues = new Set();
  for (const n of plan.prs) {
    const pr = await gh.getPr(n);
    const refs = pr ? parseRefs(pr.body) : [];
    refs.forEach((r) => issues.add(r));
    const manual = pr ? manualSteps(pr.body) : "";
    lines.push(`## [ ] PR #${n}: ${pr ? pr.title : "(not found)"}`);
    lines.push(refs.length ? `Issues: ${refs.map((r) => "#" + r).join(", ")}` : "Issues: none referenced");
    lines.push("");
    lines.push(manual ? manual.split("\n").map((l) => "> " + l).join("\n") : "No manual steps listed: confirm the change is visible in play, or tick on the PR's evidence.");
    lines.push("");
  }
  if (plan.direct.length) {
    lines.push("## Direct commits (no PR)");
    for (const c of plan.direct) lines.push(`- ${c.sha.slice(0, 8)} ${c.subject}`);
    lines.push("");
  }
  return { text: lines.join("\n"), issues: [...issues] };
}

export function writeChecklist(root, plan, text) {
  const dir = join(root, "tmp", "promote");
  mkdirSync(dir, { recursive: true });
  const file = join(dir, `checklist-${plan.target.slice(0, 8)}.md`);
  writeFileSync(file, text);
  return file;
}

export function tagName(root, sha, now = new Date()) {
  const day = now.toISOString().slice(0, 10).replace(/-/g, "");
  return `stable-${day}-${sha.slice(0, 8)}`;
}

// Fast-forward origin/stable to the plan's target and tag it. Never forces: git rejects a non-fast-forward push.
export function moveStable(root, plan, now = new Date()) {
  if (!isAncestor(root, plan.stable, plan.target)) throw new Error("not a fast-forward; refusing");
  must(git(root, ["push", "origin", `${plan.target}:refs/heads/stable`]), "push stable");
  const tag = tagName(root, plan.target, now);
  must(git(root, ["tag", "-a", tag, plan.target, "-m", `Promoted to stable: ${plan.prs.length} PR(s)`]), "tag");
  must(git(root, ["push", "origin", `refs/tags/${tag}`]), "push tag");
  return tag;
}

// Create origin/stable once, at a commit on main.
export function initStable(root, sha) {
  if (resolveSha(root, "origin/stable")) throw new Error("origin/stable already exists");
  const target = resolveSha(root, sha ?? "origin/main");
  if (!target) throw new Error(`cannot resolve ${sha ?? "origin/main"}`);
  must(git(root, ["push", "origin", `${target}:refs/heads/stable`]), "push stable");
  return target;
}

// The commit on main that merged PR n.
export function findMergeCommit(root, n, main = "origin/main") {
  const hit = commitsBetween(root, null, main).find((c) => c.pr === n);
  return hit ? hit.sha : null;
}

// Revert one merged PR on the current branch of `dir`. Returns the new commit.
export function revertMerge(dir, sha) {
  const parents = must(git(dir, ["rev-list", "--parents", "-n", "1", sha]), "rev-list").split(/\s+/).length - 1;
  const args = ["revert", "--no-edit"];
  if (parents > 1) args.push("-m", "1");
  args.push(sha);
  const r = git(dir, args);
  if (r.status !== 0) {
    git(dir, ["revert", "--abort"]);
    throw new Error(`revert of ${sha.slice(0, 8)} conflicts with later changes on main; revert by hand: ${(r.stderr || r.stdout).trim().slice(0, 300)}`);
  }
  return must(git(dir, ["rev-parse", "HEAD"]), "rev-parse");
}

// ---------------------------------------------------------------- real gh

export function realGh(cwd) {
  const bin = ghBin();
  if (!bin) throw new Error("the GitHub CLI (gh) is required");
  const gh = (args) => run(bin, args, { cwd });
  const json = (args) => {
    const r = gh(args);
    if (r.status !== 0) return null;
    try {
      return JSON.parse(r.stdout);
    } catch {
      return null;
    }
  };
  const ok = (r, what) => {
    if (r.status !== 0) throw new Error(`${what}: ${(r.stderr || r.stdout).trim()}`);
  };
  return {
    async getPr(n) {
      const p = json(["pr", "view", String(n), "--json", "number,title,body,mergedAt,baseRefName,url"]);
      return p;
    },
    async getIssue(n) {
      const i = json(["issue", "view", String(n), "--json", "number,state,stateReason,labels"]);
      if (!i) return null;
      return { number: i.number, state: String(i.state).toLowerCase(), stateReason: String(i.stateReason ?? "").toLowerCase(), labels: i.labels.map((l) => l.name) };
    },
    async addLabels(n, labels) {
      await this.ensureLabels(labels);
      ok(gh(["issue", "edit", String(n), "--add-label", labels.join(",")]), `label #${n}`);
    },
    async removeLabels(n, labels) {
      ok(gh(["issue", "edit", String(n), "--remove-label", labels.join(",")]), `unlabel #${n}`);
    },
    async close(n, comment) {
      ok(gh(["issue", "close", String(n), "--comment", comment]), `close #${n}`);
    },
    async reopen(n, comment) {
      ok(gh(["issue", "reopen", String(n), "--comment", comment]), `reopen #${n}`);
    },
    async ensureLabels(labels) {
      for (const l of labels) {
        const spec = LABEL_SPECS[l];
        if (spec) gh(["label", "create", l, "--color", spec[0], "--description", spec[1], "--force"]);
      }
    },
  };
}
