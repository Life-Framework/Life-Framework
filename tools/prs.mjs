// tools/prs.mjs — PR keeper: `tools\cli prs status|heal`.
//
// Agents ship with auto-merge and leave. When main moves, a PR can go
// CONFLICTING (mergeStateStatus DIRTY) or BEHIND, and auto-merge never fires.
// A PR with green checks and auto-merge on has also sat OPEN with
// mergeStateStatus UNKNOWN until merged by hand. `prs heal` fixes both:
//
//   DIRTY/BEHIND, worktree idle  -> merge origin/main in its worktree (or a
//     temporary one). Conflicts only in generated or driver-merged files are
//     resolved (regen, or the tools/merge/ logic on the index stages). Any other
//     conflict aborts the merge and leaves the PR untouched ("needs a human").
//     A clean result runs validate + test --no-build (fast, or all when the PR
//     touches scripts), pushes (never forced), and re-queues auto-merge.
//   green, auto-merge on, mergeable, open > 10 min -> gh pr merge --merge.
//
// A worktree with an agent edit, commit or session log in the last 15 minutes
// (not a directory or git-metadata touch, #698) belongs to a live agent and is never touched.

import { spawnSync } from "node:child_process";
import { existsSync, lstatSync, readdirSync, readFileSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { fileURLToPath } from "node:url";
import { dirname, join, normalize, resolve } from "node:path";
import * as wt from "./wt.mjs";
import { removeWorktree, safeRemoveTree } from "./link-safety.mjs";
import { mergeStringTables } from "./merge/union-string-table.mjs";
import { mergeLines } from "./merge/merge-markdown.mjs";
import { mergeLintBaselines } from "./merge/union-lint-baseline.mjs";

export const LIVE_MS = 15 * 60 * 1000;
export const STUCK_MS = 10 * 60 * 1000;

const REGISTRY = "addons/LifeFramework/Scripts/Game/Tests/LF_TestRegistrations.generated.c";
const STRING_TABLE = "addons/LifeFramework/Language/everonlife_localization.st";
const SCRIPTS_PREFIX = "addons/LifeFramework/Scripts/";

// How a conflicted path is resolved, or null when it needs a human.
export function conflictKind(path) {
  const p = String(path).replace(/\\/g, "/");
  if (p === REGISTRY) return "regen-tests";
  if (/^addons\/LifeFramework\/Language\/everonlife_localization\.[A-Za-z0-9_]+\.conf$/.test(p)) return "regen-localization";
  if (p === STRING_TABLE) return "string-table";
  if (p === "docs/features.md" || p === "docs/debug-log-triage.md") return "markdown";
  if (p === "tools/validation/enforce-lint-baseline.json") return "lint-baseline";
  return null;
}

// Resolution order: sources before what is generated from them.
const KIND_ORDER = ["string-table", "markdown", "lint-baseline", "regen-tests", "regen-localization"];

// ---------------------------------------------------------------- liveness

function newestMtime(paths) {
  let newest = 0;
  for (const p of paths) {
    try {
      const st = lstatSync(p); // lstat: never follow the game-addons junctions
      if (st.mtimeMs > newest) newest = st.mtimeMs;
    } catch {}
  }
  return newest;
}

function childrenOf(dir) {
  try {
    return readdirSync(dir).map((n) => join(dir, n));
  } catch {
    return [];
  }
}

// Newest change by an agent in a worktree: modified/untracked files, the
// timestamp of the last HEAD reflog entry (a real commit, checkout or merge),
// files inside tmp/ and server/profile/*/logs. Directory mtimes and the git
// index/HEAD file mtimes are NOT counted: `git status` itself refreshes the
// index, and prune, fetch and sweeps touch directories, so they fooled LIVE
// (#698). Shallow on purpose: no recursive walk, no junction is followed.
export function lastReflogTime(wtPath) {
  const res = wt.git(wtPath, ["reflog", "-1", "--format=%ct", "HEAD"]);
  const n = res.status === 0 ? Number(res.stdout.trim()) : 0;
  return Number.isFinite(n) ? n * 1000 : 0;
}

export function lastActivity(wtPath) {
  const paths = [];
  const st = wt.git(wtPath, ["status", "--porcelain", "-uall"]);
  for (const line of st.stdout.split(/\r?\n/)) {
    if (line.length < 4) continue;
    let file = line.slice(3);
    if (file.includes(" -> ")) file = file.split(" -> ")[1];
    paths.push(join(wtPath, file.replace(/^"|"$/g, "")));
  }
  paths.push(...childrenOf(join(wtPath, "tmp")));
  for (const scope of childrenOf(join(wtPath, "server", "profile"))) {
    paths.push(...childrenOf(join(scope, "logs")));
  }
  return Math.max(newestMtime(paths), lastReflogTime(wtPath));
}

export function isLive(wtPath, now = Date.now()) {
  const last = lastActivity(wtPath);
  return { live: last > 0 && now - last < LIVE_MS, last };
}

// ---------------------------------------------------------------- merge + heal

function stage(wtPath, n, path) {
  const res = wt.run("git", ["-C", wtPath, "-c", "core.autocrlf=false", "show", `:${n}:${path}`]);
  return res.status === 0 ? res.stdout : null;
}

function unmergedPaths(wtPath) {
  return wt.git(wtPath, ["diff", "--name-only", "--diff-filter=U"]).stdout.split(/\r?\n/).map((s) => s.trim()).filter(Boolean);
}

function resolveByStages(wtPath, path, kind) {
  const base = stage(wtPath, 1, path) ?? "";
  const ours = stage(wtPath, 2, path);
  const theirs = stage(wtPath, 3, path);
  if (ours == null || theirs == null) return { ok: false, error: `${path}: deleted on one side` };
  let merged;
  if (kind === "string-table") merged = mergeStringTables(base, ours, theirs);
  else if (kind === "markdown") merged = mergeLines(base, ours, theirs);
  else merged = mergeLintBaselines(base, ours, theirs);
  if (!merged.ok) return { ok: false, error: merged.error ?? `${path}: both sides changed the same lines` };
  writeFileSync(join(wtPath, path), merged.text);
  return { ok: true };
}

// Production regeneration: the worktree's own CLI, so its generator version is used.
export function regenerateWithCli(wtPath, kind) {
  const cmd = kind === "regen-tests" ? "regen-tests" : "regen-localization";
  const res = wt.run(process.execPath, [join(wtPath, "tools", "cli.mjs"), cmd], { cwd: wtPath });
  return { ok: res.status === 0, error: (res.stdout + res.stderr).trim() };
}

// Merge drivers for a heal, independent of the worktree being healed. A worktree
// cut before the drivers existed has neither their config, their scripts, nor
// .gitattributes lines for them, and the shared include.path list points at
// whichever worktree ran the pre-commit hook last. So the drivers come from
// this checkout's tools/merge (absolute paths, via -c, which beats every
// config file), and its .gitattributes merge lines go in as a global
// attributes file (used for any path the worktree's own .gitattributes
// does not mention).
const MERGE_DIR = join(dirname(fileURLToPath(import.meta.url)), "merge");
const HUB_ATTRIBUTES = join(dirname(MERGE_DIR), "..", ".gitattributes");

export function driverGitArgs() {
  const args = [];
  const cfg = join(MERGE_DIR, "drivers.gitconfig");
  if (existsSync(cfg)) {
    const dir = MERGE_DIR.replace(/\\/g, "/");
    let name = null;
    for (const line of readFileSync(cfg, "utf8").split(/\r?\n/)) {
      const h = line.match(/^\[merge "([^"]+)"\]/);
      if (h) { name = h[1]; continue; }
      const d = name && line.match(/^\s*driver\s*=\s*node tools\/merge\/(\S+)(.*)$/);
      // Generated files are regenerated by healMerge itself; a failing driver leaves the conflict for it.
      if (d && name.startsWith("lf-regen-")) args.push("-c", `merge.${name}.driver=false`);
      else if (d) args.push("-c", `merge.${name}.driver=node "${dir}/${d[1]}"${d[2]}`);
    }
  }
  if (existsSync(HUB_ATTRIBUTES)) {
    const merges = readFileSync(HUB_ATTRIBUTES, "utf8").split(/\r?\n/).filter((l) => /\bmerge=lf-/.test(l));
    const out = join(tmpdir(), "lf-heal-attributes");
    writeFileSync(out, merges.join("\n") + "\n");
    args.push("-c", `core.attributesFile=${out.replace(/\\/g, "/")}`);
  }
  return args;
}

// Merge `base` into the branch checked out at wtPath. Never leaves a half merge:
// a result is "up-to-date", "merged" (clean), "healed" (generated conflicts
// resolved and committed), or "needs-human" (merge aborted, HEAD unchanged).
export function healMerge(wtPath, { base = "origin/main", regenerate = regenerateWithCli } = {}) {
  const pre = wt.git(wtPath, ["rev-parse", "HEAD"]).stdout.trim();
  if (wt.git(wtPath, ["status", "--porcelain"]).stdout.trim()) {
    return { status: "needs-human", reason: "uncommitted changes in the worktree", conflicted: [], pre };
  }
  const merge = wt.git(wtPath, [...driverGitArgs(), "merge", "--no-ff", "--no-edit", base]);
  if (merge.status === 0) {
    const head = wt.git(wtPath, ["rev-parse", "HEAD"]).stdout.trim();
    return { status: head === pre ? "up-to-date" : "merged", conflicted: [], resolved: [], pre, head };
  }
  const conflicted = unmergedPaths(wtPath);
  const abort = (reason, paths) => {
    wt.git(wtPath, ["merge", "--abort"]);
    return { status: "needs-human", reason, conflicted: paths, pre };
  };
  if (conflicted.length === 0) return abort(`merge failed: ${(merge.stderr || merge.stdout).trim()}`, []);
  const human = conflicted.filter((p) => conflictKind(p) === null);
  if (human.length) return abort("conflicts outside generated and driver-merged files", human);
  const ordered = [...conflicted].sort((a, b) => KIND_ORDER.indexOf(conflictKind(a)) - KIND_ORDER.indexOf(conflictKind(b)));
  const regenDone = new Set();
  for (const path of ordered) {
    const kind = conflictKind(path);
    let r;
    if (kind.startsWith("regen-")) {
      r = regenDone.has(kind) ? { ok: true } : regenerate(wtPath, kind);
      regenDone.add(kind);
    } else {
      r = resolveByStages(wtPath, path, kind);
    }
    if (!r.ok) return abort(`could not resolve ${path}: ${r.error}`, [path]);
    const text = readFileSync(join(wtPath, path), "utf8");
    if (/^(<{7}|>{7}) /m.test(text)) return abort(`conflict markers left in ${path}`, [path]);
    wt.git(wtPath, ["add", "--", path]);
  }
  if (unmergedPaths(wtPath).length) return abort("unmerged paths remain after resolution", unmergedPaths(wtPath));
  const commit = wt.git(wtPath, ["commit", "--no-edit"]);
  if (commit.status !== 0) return abort(`merge commit failed: ${(commit.stderr || commit.stdout).trim()}`, conflicted);
  const head = wt.git(wtPath, ["rev-parse", "HEAD"]).stdout.trim();
  return { status: "healed", conflicted: [], resolved: conflicted, pre, head };
}

// Files git would conflict on, without touching any checkout (dry run).
// merge-tree does not run the custom drivers, so driver-merged files show up
// here; conflictKind still classifies them as resolvable.
export function predictConflicts(repo, ours, theirs = "origin/main") {
  const res = wt.git(repo, ["merge-tree", "--write-tree", "--name-only", "--no-messages", ours, theirs]);
  if (res.status === 0) return [];
  if (res.status !== 1) throw new Error(`git merge-tree failed: ${(res.stderr || res.stdout).trim()}`);
  return res.stdout.split(/\r?\n/).slice(1).map((s) => s.trim()).filter(Boolean);
}

export function touchesScripts(wtPath, base = "origin/main") {
  const res = wt.git(wtPath, ["diff", "--name-only", `${base}...HEAD`]);
  return res.stdout.split(/\r?\n/).some((p) => p.startsWith(SCRIPTS_PREFIX));
}

export function pushBranch(wtPath, branch) {
  // Never forced: a push that is not a fast-forward fails and is reported.
  return wt.git(wtPath, ["push", "origin", `HEAD:refs/heads/${branch}`]);
}

// ---------------------------------------------------------------- GitHub

const PR_FIELDS = "number,baseRefName,title,url,headRefName,headRefOid,mergeStateStatus,mergeable,autoMergeRequest,statusCheckRollup,createdAt,isDraft";

function gh(root, args) {
  const bin = wt.ghBin();
  if (!bin) throw new Error("GitHub CLI not found (winget install GitHub.cli, then gh auth login)");
  return wt.run(bin, args, { cwd: root });
}

export function listWsPrs(root) {
  const res = gh(root, ["pr", "list", "--state", "open", "--limit", "100", "--json", PR_FIELDS]);
  if (res.status !== 0) throw new Error(`gh pr list failed: ${(res.stderr || res.stdout).trim()}`);
  return JSON.parse(res.stdout || "[]").filter((p) => p.headRefName.startsWith("ws/"));
}

// UNKNOWN means GitHub has not computed mergeability yet. A REST read of the
// PR asks it to; use the answer when it has one.
function refreshUnknown(root, pr) {
  if (pr.mergeStateStatus !== "UNKNOWN") return pr;
  const or = wt.originOwnerRepo(root);
  if (!or) return pr;
  const res = gh(root, ["api", `repos/${or.owner}/${or.repo}/pulls/${pr.number}`, "--jq", ".mergeable_state"]);
  const s = res.status === 0 ? res.stdout.trim().toUpperCase() : "";
  if (s && s !== "UNKNOWN" && s !== "NULL") pr.mergeStateStatus = s;
  return pr;
}

export function checksOf(pr) {
  const runs = pr.statusCheckRollup ?? [];
  let pass = 0, fail = 0, pending = 0, last = 0;
  for (const c of runs) {
    const state = c.__typename === "StatusContext" ? c.state : (c.status === "COMPLETED" ? c.conclusion : "PENDING");
    if (["SUCCESS", "NEUTRAL", "SKIPPED"].includes(state)) pass++;
    else if (["PENDING", "EXPECTED", "QUEUED", "IN_PROGRESS", null, undefined, ""].includes(state)) pending++;
    else fail++;
    const at = Date.parse(c.completedAt ?? c.startedAt ?? "") || 0;
    if (at > last) last = at;
  }
  let label = "none";
  if (fail) label = `FAIL ${fail}/${runs.length}`;
  else if (pending) label = `pending ${pending}/${runs.length}`;
  else if (runs.length) label = `pass ${pass}`;
  return { green: runs.length > 0 && fail === 0 && pending === 0, failing: fail > 0, pending: pending > 0, label, last };
}

function age(ms) {
  const m = Math.max(0, Math.round(ms / 60000));
  if (m < 60) return `${m}m`;
  const h = Math.floor(m / 60);
  return h < 48 ? `${h}h${String(m % 60).padStart(2, "0")}` : `${Math.floor(h / 24)}d`;
}

function worktreeFor(root, branch) {
  return wt.listWorktrees(root).find((w) => w.branch === branch && existsSync(w.path)) ?? null;
}

function describeWorktree(root, branch, now) {
  const w = worktreeFor(root, branch);
  if (!w) return { w: null, live: false, label: "none" };
  const { live, last } = isLive(w.path, now);
  return { w, live, label: `${live ? "LIVE" : "idle"} ${last ? age(now - last) : "?"}` };
}

// ---------------------------------------------------------------- commands

function table(rows, headers) {
  const widths = headers.map((h, i) => Math.max(h.length, ...rows.map((r) => String(r[i]).length)));
  const fmt = (r) => "  " + r.map((c, i) => String(c).padEnd(widths[i])).join("  ").trimEnd();
  console.log(fmt(headers));
  for (const r of rows) console.log(fmt(r));
}

export function prsStatus(root) {
  const now = Date.now();
  wt.git(root, ["fetch", "origin", "--prune", "--quiet"]);
  const prs = listWsPrs(root).map((p) => refreshUnknown(root, p));
  if (!prs.length) {
    console.log("prs: no open ws/* PRs");
    return 0;
  }
  const rows = prs.map((p) => {
    const d = describeWorktree(root, p.headRefName, now);
    return [
      `#${p.number}`,
      p.mergeStateStatus,
      p.autoMergeRequest ? "on" : "off",
      checksOf(p).label,
      age(now - Date.parse(p.createdAt)),
      d.label,
      p.title.length > 60 ? `${p.title.slice(0, 57)}...` : p.title,
    ];
  });
  table(rows, ["PR", "STATE", "AUTO", "CHECKS", "AGE", "WORKTREE", "TITLE"]);
  console.log("prs: LIVE = an edit, commit or log write in the worktree within 15 min; heal never touches it. Heal: tools\\cli prs heal [--dry-run] [<n>...]");
  return 0;
}

function gateIn(wtPath, log, base = "origin/main") {
  const tier = touchesScripts(wtPath, base) ? "all" : "fast";
  const cli = join(wtPath, "tools", "cli.mjs");
  const step = (args) => {
    log(`    gate: tools\\cli ${args.join(" ")}`);
    const r = spawnSync(process.execPath, [cli, ...args], { cwd: wtPath, windowsHide: true, stdio: "inherit" });
    return r.status ?? 1;
  };
  if (step(["validate"]) !== 0) return { ok: false, what: "validate" };
  if (step(["test", "--no-build", "--tier", tier, "--wait"]) !== 0) return { ok: false, what: `test --tier ${tier}` };
  return { ok: true, tier };
}

function addTempWorktree(root, pr) {
  const dir = join(dirname(wt.mainRootOf(root)), `Life-Framework-heal-${pr.number}`);
  if (existsSync(dir)) throw new Error(`temp worktree dir already exists: ${dir}`);
  const add = wt.git(root, ["worktree", "add", "--detach", dir, pr.headRefOid]);
  if (add.status !== 0) throw new Error(`git worktree add failed: ${(add.stderr || add.stdout).trim()}`);
  return dir;
}

function dropTempWorktree(root, dir) {
  // The safe helper unlinks the game-addons junctions a test boot created first.
  const rm = removeWorktree(wt.mainRootOf(root), dir, { force: true });
  if (rm.status !== 0 && existsSync(dir)) {
    const known = wt.listWorktrees(root).some((w) => normalize(w.path) === normalize(dir));
    if (!known) safeRemoveTree(dir);
  }
}

// Bring the local worktree branch to the PR head. Returns an error string or null.
function alignWithPr(wtPath, pr) {
  const local = wt.git(wtPath, ["rev-parse", "HEAD"]).stdout.trim();
  if (local === pr.headRefOid) return null;
  if (wt.gitOk(wtPath, ["merge-base", "--is-ancestor", local, pr.headRefOid])) {
    const ff = wt.git(wtPath, ["merge", "--ff-only", pr.headRefOid]);
    return ff.status === 0 ? null : `fast-forward to the PR head failed: ${(ff.stderr || ff.stdout).trim()}`;
  }
  return `worktree HEAD ${local.slice(0, 7)} has commits the PR (${pr.headRefOid.slice(0, 7)}) does not; the agent has unpushed work`;
}

function healOne(root, pr, { dryRun, log }) {
  const d = describeWorktree(root, pr.headRefName, Date.now());
  if (d.live) return { kind: "live", msg: `worktree ${d.label}: an agent is working; not touched` };
  const baseRef = "origin/" + (pr.baseRefName || "main");
  const conflicts = predictConflicts(root, pr.headRefOid, baseRef);
  const human = conflicts.filter((p) => conflictKind(p) === null);
  if (dryRun) {
    if (human.length) return { kind: "attention", msg: `would report: needs a human/agent, conflicts in ${human.join(", ")}` };
    const where = d.w ? d.w.path : "a temp worktree";
    const extra = conflicts.length ? ` and resolve ${conflicts.join(", ")}` : "";
    return { kind: "would", msg: `would merge ${baseRef} in ${where}${extra}, gate, push, re-queue auto-merge` };
  }
  if (human.length) return { kind: "attention", msg: `needs a human/agent: conflicts in ${human.join(", ")}`, paths: human };
  if (d.w && wt.currentBranch(d.w.path) !== pr.headRefName) {
    return { kind: "attention", msg: `worktree ${d.w.path} is on '${wt.currentBranch(d.w.path)}', not ${pr.headRefName}` };
  }
  let dir = d.w?.path;
  let temp = false;
  try {
    if (dir) {
      if (wt.git(dir, ["status", "--porcelain"]).stdout.trim()) return { kind: "attention", msg: `worktree ${dir} has uncommitted changes (idle); needs its agent` };
      const err = alignWithPr(dir, pr);
      if (err) return { kind: "attention", msg: err };
    } else {
      dir = addTempWorktree(root, pr);
      temp = true;
      log(`    no worktree for ${pr.headRefName}; using temp ${dir}`);
    }
    const m = healMerge(dir, { base: baseRef });
    if (m.status === "needs-human") return { kind: "attention", msg: `needs a human/agent: ${m.reason}${m.conflicted.length ? ` (${m.conflicted.join(", ")})` : ""}; merge aborted, PR untouched`, paths: m.conflicted };
    if (m.status === "up-to-date") log(`    already contains ${baseRef}`);
    else log(`    merged ${baseRef}${m.resolved?.length ? `, resolved ${m.resolved.join(", ")}` : ""}`);
    const g = gateIn(dir, log, baseRef);
    if (!g.ok) {
      if (!temp && m.status !== "up-to-date") wt.git(dir, ["reset", "--keep", m.pre]); // drop only our merge commit
      return { kind: "attention", msg: `gate failed (${g.what}) after merging ${baseRef}; not pushed, worktree reset to ${m.pre.slice(0, 7)}` };
    }
    if (m.status !== "up-to-date") {
      const push = pushBranch(dir, pr.headRefName);
      if (push.status !== 0) return { kind: "attention", msg: `push failed (not forced): ${(push.stderr || push.stdout).trim()}` };
    }
    const am = gh(root, ["pr", "merge", String(pr.number), "--auto", "--merge"]);
    const amOut = (am.stdout + am.stderr).trim();
    if (am.status !== 0 && !/already/i.test(amOut)) return { kind: "attention", msg: `pushed, but auto-merge failed: ${amOut}` };
    const head = wt.git(dir, ["rev-parse", "HEAD"]).stdout.trim();
    return { kind: "healed", msg: `healed: ${m.status}, gate ${g.tier} green, pushed ${head.slice(0, 7)}, auto-merge queued` };
  } catch (e) {
    return { kind: "attention", msg: `heal failed: ${e.message}` };
  } finally {
    if (temp && dir) dropTempWorktree(root, dir);
  }
}

function mergeStuck(root, pr, { dryRun }) {
  if (dryRun) return { kind: "would", msg: "would run gh pr merge --merge (green, auto-merge on, still open)" };
  const res = gh(root, ["pr", "merge", String(pr.number), "--merge"]);
  const out = (res.stdout + res.stderr).trim();
  if (res.status !== 0) return { kind: "attention", msg: `gh pr merge --merge failed: ${out}` };
  return { kind: "merged", msg: "merged with gh pr merge --merge" };
}

export function prsHeal(root, args) {
  const dryRun = args.includes("--dry-run");
  const only = args.filter((a) => /^#?\d+$/.test(a)).map((a) => parseInt(a.replace("#", ""), 10));
  const log = (s) => console.log(s);
  let lock = null;
  if (!dryRun) {
    try {
      lock = wt.acquireLock(root, "prs-heal");
    } catch (e) {
      console.log(`FAIL  ${e.message}`);
      return 1;
    }
  }
  try {
    wt.git(root, ["fetch", "origin", "--prune", "--quiet"]);
    let prs = listWsPrs(root);
    if (only.length) prs = prs.filter((p) => only.includes(p.number));
    const now = Date.now();
    const results = [];
    for (const pr of prs) {
      refreshUnknown(root, pr);
      const c = checksOf(pr);
      const tag = `#${pr.number} ${pr.headRefName} [${pr.mergeStateStatus}]`;
      let r;
      if (pr.isDraft) r = { kind: "skip", msg: "draft" };
      else if (pr.mergeStateStatus === "DIRTY" || pr.mergeStateStatus === "BEHIND") {
        console.log(`${tag}: ${dryRun ? "checking" : "healing"}`);
        r = healOne(root, pr, { dryRun, log });
      } else if (["CLEAN", "UNKNOWN", "HAS_HOOKS", "UNSTABLE"].includes(pr.mergeStateStatus) && pr.autoMergeRequest && c.green) {
        const since = Math.max(Date.parse(pr.autoMergeRequest.enabledAt ?? "") || 0, c.last);
        if (now - since >= STUCK_MS) r = mergeStuck(root, pr, { dryRun });
        else r = { kind: "ok", msg: `green, auto-merge on; waiting (${age(now - since)} < 10m)` };
      } else if (c.failing) r = { kind: "attention", msg: `checks failing (${c.label})` };
      else if (!pr.autoMergeRequest) r = { kind: "ok", msg: "auto-merge off (left for review)" };
      else r = { kind: "ok", msg: `waiting: checks ${c.label}` };
      results.push({ pr, tag, ...r });
    }
    console.log("");
    console.log(`prs heal${dryRun ? " --dry-run" : ""}: ${results.length} ws/* PR(s)`);
    for (const r of results) console.log(`  ${r.kind.toUpperCase().padEnd(9)} ${r.tag}: ${r.msg}`);
    const attention = results.filter((r) => r.kind === "attention");
    if (attention.length) {
      console.log(`prs: ${attention.length} PR(s) need a human or their agent`);
      return 1;
    }
    console.log("prs: nothing needs attention");
    return 0;
  } catch (e) {
    console.log(`FAIL  ${e.message}`);
    return 1;
  } finally {
    if (lock) wt.releaseLock(root, "prs-heal");
  }
}

export function prsHelp() {
  console.log(`PR keeper (open PRs from ws/* branches)

  tools\\cli prs status                 number, state, auto-merge, checks, age, worktree LIVE/idle
  tools\\cli prs heal [--dry-run] [<n>...]
      DIRTY/BEHIND and idle 15 min -> merge origin/main; resolve conflicts only in generated or
      driver-merged files; gate (validate + test --no-build, fast or all if scripts changed);
      push (never forced); re-queue auto-merge. Other conflicts: merge aborted, reported.
      Green + auto-merge on + still open after 10 min -> gh pr merge --merge.
      Exit 1 when any PR needs a human or its agent.
`);
  return 0;
}

export function cmdPrs(root, args) {
  const [sub, ...rest] = args;
  if (sub === "status") return prsStatus(root);
  if (sub === "heal") return prsHeal(root, rest);
  prsHelp();
  return sub && sub !== "help" ? 1 : 0;
}
