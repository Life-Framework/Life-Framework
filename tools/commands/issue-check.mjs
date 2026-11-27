// tools\cli issue-check <N> [<N>...] [--skip-list <file>] [--json]: before dispatching an agent to an
// open issue, ask GitHub and git whether merged work already covers it (tools/lib/issue-preflight.mjs).
// One verdict per issue: LIKELY DONE, CHECK or CLEAR. Exit 0 always (it is advice); --json prints the
// facts and verdicts. The skip list is the lead's stale-issue file (lines with ALREADY-FIXED); it
// defaults to LF_STALE_LIST when that is set.
import { existsSync, readFileSync } from "node:fs";
import { ghBin, run } from "../wt.mjs";
import { keySymbols, keywords, namedIssues, render, skipListNumbers, slicedIssues, verdict } from "../lib/issue-preflight.mjs";

export const help = "issue-check <N...> [--skip-list f] [--json]   before dispatch: is the issue already done under another number? (LIKELY DONE: a merged PR or commit that is for it, i.e. title ends (#N) or a Refs/Closes/Fixes #N line, a PR titled like the issue, every slice card a \"Sliced: #N\" comment names done, or a done label; a plain mention or a commit naming its classes is only CHECK)";

function gh(root, args) {
  const bin = ghBin();
  if (!bin) throw new Error("the GitHub CLI (gh) is required");
  const r = run(bin, args, { cwd: root, maxBuffer: 64 * 1024 * 1024, timeout: 30000 });
  if (r.status !== 0) throw new Error(`gh ${args.slice(0, 3).join(" ")}: ${(r.stderr || r.stdout || "").trim().split("\n")[0]}`);
  return JSON.parse(r.stdout || "[]");
}

export function gather(root, n, { skipList = new Set() } = {}) {
  const issue = gh(root, ["issue", "view", String(n), "--json", "number,title,body,labels,state,comments,createdAt"]);
  const prs = gh(root, ["pr", "list", "--state", "merged", "--search", `${n} in:title,body`, "--limit", "30", "--json", "number,title,body,mergedAt"]);
  // A PR that never cites the number but is titled like the issue (2026-11-27 sweep).
  const phrase = issue.title.replace(/[^A-Za-z0-9' -]+/g, " ").replace(/\s+/g, " ").trim();
  if (phrase.length >= 24) {
    try {
      for (const p of gh(root, ["pr", "list", "--state", "merged", "--search", `"${phrase}" in:title,body`, "--limit", "10", "--json", "number,title,body,mergedAt"]))
        if (!prs.some((q) => q.number === p.number)) prs.push(p);
    } catch {
      /* search hiccup: the number search still stands */
    }
  }
  const g = run("git", ["log", "origin/main", "-E", `--grep=#${n}([^0-9]|$)`, "--no-merges", "--format=%H%x09%s", "-n", "10"], { cwd: root, timeout: 30000 });
  const commits = (g.status === 0 ? g.stdout : "")
    .split("\n")
    .filter(Boolean)
    .map((l) => ({ sha: l.split("\t")[0], subject: l.split("\t").slice(1).join("\t") }));
  const words = keywords(issue.title);
  let similar = [];
  if (words.length >= 2) {
    try {
      similar = gh(root, ["pr", "list", "--state", "merged", "--search", `${words.slice(0, 3).join(" ")} in:title`, "--limit", "5", "--json", "number,title,mergedAt"]);
    } catch {
      /* search hiccup: no similar list */
    }
  }
  const named = [];
  for (const m of namedIssues(issue.body, n).slice(0, 8)) {
    try {
      named.push(gh(root, ["issue", "view", String(m), "--json", "number,title,state,labels"]));
    } catch {
      /* a PR number or a deleted issue */
    }
  }
  // The cards the lead cut it into ("Sliced: #2547" in a comment): their PRs cite the card, not this number.
  const sliced = slicedIssues(issue.comments, n);
  const slices = [];
  for (const m of sliced.numbers.slice(0, 14)) {
    try {
      const s = gh(root, ["issue", "view", String(m), "--json", "number,title,state,labels,url"]);
      if (!/\/pull\//.test(s.url || "")) slices.push(s); // "Sliced the follow-up from #2491" names a PR
    } catch {
      /* a PR number or a deleted issue */
    }
  }
  // Commits after the issue was filed whose message names its classes.
  const symbolCommits = [];
  for (const sym of keySymbols(issue.body, 4)) {
    const s = run("git", ["log", "origin/main", "-F", `--grep=${sym}`, "--no-merges", `--since=${issue.createdAt || "2020-01-01"}`, "--format=%H%x09%s", "-n", "5"], { cwd: root, timeout: 30000 });
    for (const l of (s.status === 0 ? s.stdout : "").split("\n").filter(Boolean)) {
      const sha = l.split("\t")[0];
      const hit = symbolCommits.find((c) => c.sha === sha);
      if (hit) hit.symbols.push(sym);
      else symbolCommits.push({ sha, subject: l.split("\t").slice(1).join("\t"), symbols: [sym] });
    }
  }
  return { issue, prs, commits, similar, named, slices, slicesPartial: sliced.partial, symbolCommits, skipList };
}

export async function main(args, { root = process.cwd() } = {}) {
  const nums = args.filter((a) => /^#?[0-9]+$/.test(a)).map((a) => Number(a.replace("#", "")));
  if (!nums.length) {
    console.log("usage: tools\\cli issue-check <N> [<N>...] [--skip-list <file>] [--json]");
    return 2;
  }
  const si = args.indexOf("--skip-list");
  const listFile = si >= 0 ? args[si + 1] : process.env.LF_STALE_LIST;
  const skipList = listFile && existsSync(listFile) ? skipListNumbers(readFileSync(listFile, "utf8")) : new Set();
  const out = [];
  for (const n of nums) {
    try {
      const facts = gather(root, n, { skipList });
      const v = verdict(facts);
      out.push({ n, title: facts.issue.title, ...v });
      if (!args.includes("--json")) console.log(render(n, facts.issue.title, v));
    } catch (e) {
      out.push({ n, level: "ERROR", reasons: [e.message] });
      if (!args.includes("--json")) console.log(`#${n} ERROR: ${e.message}`);
    }
  }
  if (args.includes("--json")) console.log(JSON.stringify(out, null, 1));
  const counts = ["LIKELY DONE", "CHECK", "CLEAR", "ERROR"].map((l) => `${l.toLowerCase().replace(" ", "-")}=${out.filter((o) => o.level === l).length}`).join(" ");
  console.log(`SUMMARY issue-check ok ${counts}`);
  return 0;
}
