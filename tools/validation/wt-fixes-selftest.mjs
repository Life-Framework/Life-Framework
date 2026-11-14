// Selftest for `wt ship --refs` parsing and `wt prune` merge detection (tools/wt.mjs).
// inputs: tools/**
// Scratch git repos with a bare origin and a stubbed gh lookup; no game, no network.
// red-proof: see the PR body (parseIssueFlag accepting 0, and branchMerged ignoring the merged-PR head).
import { mkdirSync, mkdtempSync, writeFileSync, utimesSync, rmSync } from "node:fs";
import { spawnSync } from "node:child_process";
import { tmpdir } from "node:os";
import { join } from "node:path";
import * as wt from "../wt.mjs";
import * as pq from "../lib/pr-quality.mjs";
import { refsBody } from "../lib/release-gates.mjs";

const failures = [];
const ok = (c, m) => { if (!c) failures.push(m); };
const sh = (cwd, ...a) => {
  const r = spawnSync("git", a, { cwd, encoding: "utf8", windowsHide: true });
  if (r.status !== 0) throw new Error(`git ${a.join(" ")}: ${r.stderr}`);
  return r.stdout.trim();
};

// --- 1. --refs / --closes
const bad = (flags) => { try { wt.parseIssueFlag(flags); return false; } catch (e) { return /issue number/.test(e.message); } };
ok(wt.parseIssueFlag([]) === null, "no flag: no issue");
ok(wt.parseIssueFlag(["--skip-gate"]) === null, "other flags: no issue");
ok(wt.parseIssueFlag(["--refs", "42"]) === "42", "--refs 42");
ok(wt.parseIssueFlag(["--closes", "7"]) === "7", "--closes 7");
ok(bad(["--refs", "0"]), "--refs 0 rejected");
ok(bad(["--closes", "00"]), "--closes 00 rejected");
ok(bad(["--refs", "abc"]), "--refs abc rejected");
ok(bad(["--refs"]), "--refs with no value rejected");
ok(bad(["--refs", "--skip-gate"]), "--refs followed by a flag rejected");
ok(!/Refs #/.test(refsBody("Body text\n", null)), "no issue: no Refs line is added");
ok(refsBody("Body\n", "5").startsWith("Refs #5"), "issue 5: Refs line added");
const body = "## Summary\nA tooling change.\n\n## Manual testing\nNone: tooling only.\n";
ok(pq.bodyProblems({ body, files: ["tools/wt.mjs"], hasBodyFile: false }).length === 0, "PR-quality gate accepts a body with no Refs line");

// --- 2. prune merge detection
const dir = mkdtempSync(join(tmpdir(), "lf-wtfix-"));
const origin = join(dir, "origin.git");
const repo = join(dir, "repo");
try {
  sh(dir, "init", "--bare", "-b", "main", origin);
  sh(dir, "clone", origin, repo);
  sh(repo, "config", "user.email", "t@t"); sh(repo, "config", "user.name", "t");
  writeFileSync(join(repo, "a.txt"), "1\n");
  sh(repo, "add", "."); sh(repo, "commit", "-m", "init"); sh(repo, "push", "origin", "HEAD:main");
  const noPr = () => [];

  // ancestor (fast-forward merge)
  sh(repo, "checkout", "-b", "ws/ff"); writeFileSync(join(repo, "ff.txt"), "x\n");
  sh(repo, "add", "."); sh(repo, "commit", "-m", "ff"); sh(repo, "push", "origin", "ws/ff:main");
  // squash merge: main gets a different commit with the same files; the branch tip is not an ancestor
  sh(repo, "checkout", "-b", "ws/sq", "origin/main"); writeFileSync(join(repo, "sq.txt"), "y\n");
  sh(repo, "add", "."); sh(repo, "commit", "-m", "sq work");
  const sqTip = sh(repo, "rev-parse", "HEAD");
  sh(repo, "checkout", "-b", "tmp-squash", "origin/main"); writeFileSync(join(repo, "sq.txt"), "y\n");
  sh(repo, "add", "."); sh(repo, "commit", "-m", "squashed (#1)"); sh(repo, "push", "origin", "tmp-squash:main");
  // unmerged work
  sh(repo, "checkout", "-b", "ws/open", "origin/main"); writeFileSync(join(repo, "open.txt"), "z\n");
  sh(repo, "add", "."); sh(repo, "commit", "-m", "open");
  const openTip = sh(repo, "rev-parse", "HEAD");
  // merged PR head that no longer matches the tip (more commits after the merge)
  sh(repo, "checkout", "-b", "ws/more", "origin/main"); writeFileSync(join(repo, "more.txt"), "m\n");
  sh(repo, "add", "."); sh(repo, "commit", "-m", "more1");
  const oldHead = sh(repo, "rev-parse", "HEAD");
  writeFileSync(join(repo, "more.txt"), "m2\n"); sh(repo, "commit", "-am", "more2");
  sh(repo, "fetch", "origin");

  ok(wt.branchMerged(repo, "ws/ff", "main", { mergedHeads: noPr }).merged, "fast-forward merge counts as merged");
  // the squashed branch's file content matches main, so prove the PR-head path by hiding the file match:
  // use a branch whose content differs from main but whose PR head equals its tip
  const viaPr = wt.branchMerged(repo, "ws/open", "main", { mergedHeads: () => [openTip] });
  ok(viaPr.merged && viaPr.via === "merged PR", "merged PR with the tip's SHA counts as merged");
  ok(!wt.branchMerged(repo, "ws/open", "main", { mergedHeads: noPr }).merged, "unmerged work is refused");
  ok(!wt.branchMerged(repo, "ws/open", "main", { mergedHeads: () => ["deadbeef"] }).merged, "a merged PR with another head SHA is refused");
  ok(!wt.branchMerged(repo, "ws/more", "main", { mergedHeads: () => [oldHead] }).merged, "commits after the merged head are refused");
  ok(wt.branchMerged(repo, "ws/sq", "main", { mergedHeads: noPr }).merged, "squash merge with identical files counts as merged");
  ok(sqTip.length > 0, "sanity");
  ok(!wt.branchMerged(repo, "ws/nope", "main", { mergedHeads: noPr }).merged, "missing branch is not merged");

  // --merged candidates: hand-written hub state beside the scratch repo
  const wtPath = join(dir, "wt-sq");
  sh(repo, "worktree", "add", wtPath, "ws/sq");
  const openPath = join(dir, "wt-open");
  sh(repo, "worktree", "add", openPath, "ws/open");
  mkdirSync(join(repo, "tmp", "wt", "locks"), { recursive: true });
  const state = { version: 1, nextPortIndex: 2, worktrees: {
    sq: { slug: "sq", branch: "ws/sq", path: wtPath, ports: { gamePort: 1, a2sPort: 2 } },
    open: { slug: "open", branch: "ws/open", path: openPath, ports: { gamePort: 3, a2sPort: 4 } },
  } };
  writeFileSync(join(repo, "tmp", "wt", "state.json"), JSON.stringify(state));
  const later = Date.now() + 40 * 60 * 1000;
  let plan = wt.pruneCandidates(repo, { now: later, mergedHeads: noPr });
  ok(plan.take.map((t) => t.slug).join() === "sq", `only the merged worktree qualifies, got ${JSON.stringify(plan)}`);
  ok(plan.skip.some((s) => s.slug === "open" && /not merged/.test(s.why)), "unmerged worktree is kept");
  plan = wt.pruneCandidates(repo, { now: Date.now(), mergedHeads: noPr });
  ok(!plan.take.length && plan.skip.some((s) => s.slug === "sq" && /touched/.test(s.why)), "a worktree touched under 30 min ago is kept");
  writeFileSync(join(wtPath, "dirty.txt"), "d\n");
  plan = wt.pruneCandidates(repo, { now: later, mergedHeads: noPr });
  ok(plan.skip.some((s) => s.slug === "sq" && /uncommitted/.test(s.why)), "a dirty worktree is kept");
  rmSync(join(wtPath, "dirty.txt"));
  writeFileSync(join(repo, "tmp", "wt", "locks", "server-9.lock"), JSON.stringify({ pid: process.pid, root: wtPath, name: "server-9" }));
  plan = wt.pruneCandidates(repo, { now: later, mergedHeads: noPr });
  ok(plan.skip.some((s) => s.slug === "sq" && /lock/.test(s.why)), "a worktree with a live lock is kept");
} catch (e) {
  failures.push(`selftest crashed: ${e.stack || e.message}`);
} finally {
  spawnSync("git", ["-C", repo, "worktree", "prune"], { windowsHide: true });
  try { rmSync(dir, { recursive: true, force: true }); } catch { /* temp dir, no junctions inside */ }
}

if (failures.length) {
  console.log(`FAIL  wt-fixes-selftest: ${failures.length} failure(s)`);
  for (const f of failures) console.log(`  - ${f}`);
  process.exit(1);
}
console.log("PASS  wt-fixes-selftest");
