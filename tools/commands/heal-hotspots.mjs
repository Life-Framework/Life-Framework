// tools\cli heal-hotspots: merge main into a branch cut before the per-feature StaticReset hooks and the
// docs/features/ split, moving the branch's old central edits to their new files (tools/lib/heal-hotspots.mjs).
import { spawnSync } from "node:child_process";
import { existsSync } from "node:fs";
import { join } from "node:path";
import { healBranch } from "../lib/heal-hotspots.mjs";

export const help = "heal-hotspots [<slug>|<worktree path>] [--onto origin/main] [--dry-run] [--no-commit]   in a branch's worktree: merge main and move the branch's LF_StaticReset.c entries into per-feature hooks and its docs/features.md rows into docs/features/<area>.md";

export async function main(args, { root } = {}) {
  const target = args.find((a, i) => !a.startsWith("--") && !(i > 0 && args[i - 1] === "--onto"));
  const cwd = target ? resolveWorktree(root || process.cwd(), target) : root || process.cwd();
  if (!cwd) {
    console.log(`SUMMARY heal-hotspots fail: no worktree for "${target}" (git worktree list shows none on ws/${target} or at that path)`);
    return 2;
  }
  const opt = (k, d) => {
    const i = args.indexOf(k);
    return i >= 0 ? args[i + 1] : d;
  };
  const onto = opt("--onto", "origin/main");
  const dryRun = args.includes("--dry-run");
  if (onto.startsWith("origin/")) {
    const f = spawnSync("git", ["fetch", "-q", "origin", onto.slice("origin/".length)], { cwd, encoding: "utf8", windowsHide: true, timeout: 60000 });
    if (f.status !== 0) console.log(`heal-hotspots: git fetch failed (${(f.stderr || "").trim() || "timeout"}); using the local ${onto}`);
  }
  let r;
  try {
    r = await healBranch({ root: cwd, onto, dryRun, commit: !args.includes("--no-commit") });
  } catch (e) {
    console.log(`SUMMARY heal-hotspots fail: ${e.message}`);
    return 1;
  }
  for (const l of r.log) console.log(`  ${l}`);
  const what = `${r.plan.calls.length} reset entr${r.plan.calls.length === 1 ? "y" : "ies"}, ${r.plan.sections.length} features section(s)`;
  console.log(`SUMMARY heal-hotspots ${r.ok ? "ok" : "fail"} ${dryRun ? "(dry run) " : ""}${what}${r.left.length ? `; unmerged: ${r.left.join(", ")}` : ""}`);
  if (r.ok && !dryRun && r.plan.needed) console.log("next: tools\\cli validate, then push (or `wt ship`)");
  return r.ok ? 0 : 1;
}

// A slug (branch ws/<slug>) or a path to a worktree of this repo. Lets a branch cut before this command
// existed be healed from a checkout that has it: `tools\cli heal-hotspots <slug>`.
export function resolveWorktree(root, target) {
  if (existsSync(join(target, ".git"))) return target;
  const r = spawnSync("git", ["worktree", "list", "--porcelain"], { cwd: root, encoding: "utf8", windowsHide: true });
  let path = null;
  for (const line of (r.stdout || "").split("\n")) {
    if (line.startsWith("worktree ")) path = line.slice(9).trim();
    if (line.trim() === `branch refs/heads/ws/${target}` || line.trim() === `branch refs/heads/${target}`) return path;
  }
  return null;
}
