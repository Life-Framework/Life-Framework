// Proves the merge-drivers include stays ONE line in the shared git config, on throwaway repos.
// inputs: tools/merge/**, tools/validation/**, .githooks/**
// Never touches this repo's config.
//
//   one-line      register from the main checkout and from two worktrees -> exactly one include,
//                 the main checkout's file, never a worktree path; re-running adds nothing.
//   prune         640-style stale lines (worktree paths, a backslash path, a relative path, a
//                 duplicate of the right one) collapse to one; an unrelated include survives.
//   busy          config.lock held by another process: exit 0, config untouched, no throw.
//   hook          .githooks/pre-commit calls register-drivers.mjs and never adds an include itself.
//   callers       no other script in the repo writes a drivers include.path.
//
// red-proof (2026-11-14): with registerDrivers adding `<cwd>/tools/merge/drivers.gitconfig` (the old
//   per-worktree behaviour) one-line printed "worktree A: include is <worktree>/... not the main
//   checkout's" for each worktree and prune left the worktree path; with `git config --local --add include.path
//   "$DRIVERS"` put back in the hook, hook printed "pre-commit adds an include.path itself".
import { spawnSync } from "node:child_process";
import { mkdirSync, mkdtempSync, readFileSync, readdirSync, statSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join, relative } from "node:path";
import { fileURLToPath } from "node:url";
import { safeRemoveTree } from "../link-safety.mjs";
import { driversIncludes, mainCheckout, registerDrivers } from "../merge/register-drivers.mjs";

const ROOT = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
const SCRIPT = join(ROOT, "tools", "merge", "register-drivers.mjs");
const SUFFIX = "tools/merge/drivers.gitconfig";
const failures = [];
const fail = (m) => failures.push(m);
const fwd = (p) => p.split("\\").join("/");

function git(dir, args) {
  const r = spawnSync("git", args, { cwd: dir, encoding: "utf8", windowsHide: true });
  if (r.status !== 0) throw new Error(`git ${args.join(" ")} in ${dir}: ${r.stderr}`);
  return r.stdout.trim();
}
const registerCli = (dir) => spawnSync("node", [SCRIPT], { cwd: dir, encoding: "utf8", windowsHide: true });

const temp = mkdtempSync(join(tmpdir(), "lf-drivers-include-"));
try {
  const main = join(temp, "main");
  mkdirSync(join(main, "tools", "merge"), { recursive: true });
  writeFileSync(join(main, SUFFIX), "# fixture\n");
  git(main, ["init", "-q", "-b", "main"]);
  git(main, ["config", "user.email", "t@example.com"]);
  git(main, ["config", "user.name", "t"]);
  git(main, ["add", "."]);
  git(main, ["commit", "-q", "-m", "init"]);
  const wtA = join(temp, "wt-a");
  const wtB = join(temp, "wt-b");
  git(main, ["worktree", "add", "-q", "-b", "a", wtA]);
  git(main, ["worktree", "add", "-q", "-b", "b", wtB]);
  const canonical = `${fwd(mainCheckout(main))}/${SUFFIX}`;
  if (canonical.includes("wt-")) fail(`main checkout resolved to ${canonical}`);

  // one-line
  for (const [label, dir] of [["worktree A", wtA], ["worktree B", wtB], ["main", main], ["worktree A again", wtA]]) {
    registerCli(dir);
    const got = driversIncludes(main);
    if (got.length !== 1) fail(`after ${label} there are ${got.length} includes, expected 1: ${got.join(" | ")}`);
    else if (got[0] !== canonical) fail(`${label}: include is ${got[0]}, not the main checkout's ${canonical}`);
  }
  if (driversIncludes(wtA).length !== 1) fail("worktree A sees a different include list than main");

  // prune
  const stales = [
    `${fwd(wtA)}/${SUFFIX}`,
    `${fwd(wtB)}/${SUFFIX}`,
    `${fwd(temp)}/gone-1/${SUFFIX}`,
    `${fwd(temp)}/gone-2/${SUFFIX}`,
    `${temp}\\gone-3\\tools\\merge\\drivers.gitconfig`,
    `../${SUFFIX}`,
    canonical,
  ];
  for (const stale of stales) git(main, ["config", "--local", "--add", "include.path", stale]);
  git(main, ["config", "--local", "--add", "include.path", "/somewhere/else/other.gitconfig"]);
  if (driversIncludes(main).length < 8) fail(`prune fixture only built ${driversIncludes(main).length} includes`);
  registerCli(wtB);
  const pruned = driversIncludes(main);
  if (pruned.length !== 1 || pruned[0] !== canonical) fail(`prune left ${pruned.length} drivers includes: ${pruned.join(" | ")}`);
  const all = git(main, ["config", "--local", "--get-all", "include.path"]).split(/\r?\n/);
  if (!all.includes("/somewhere/else/other.gitconfig")) fail("prune removed an unrelated include.path");

  // busy: a stale line is present and config.lock is held by "another process"
  git(main, ["config", "--local", "--add", "include.path", `${fwd(temp)}/gone-4/${SUFFIX}`]);
  const configFile = join(main, ".git", "config");
  const snapshot = readFileSync(configFile, "utf8");
  writeFileSync(`${configFile}.lock`, "");
  const busy = registerCli(wtA);
  if (busy.status !== 0) fail(`busy config made register-drivers exit ${busy.status}`);
  if (readFileSync(configFile, "utf8") !== snapshot) fail("busy config was modified");
  safeRemoveTree(`${configFile}.lock`);
  registerCli(wtA);
  if (driversIncludes(main).length !== 1) fail(`after the lock cleared there are ${driversIncludes(main).length} includes`);

  // a correct config is not rewritten
  const r = registerDrivers(wtA);
  if (r.removed || r.added) fail(`a correct config was rewritten: ${JSON.stringify(r)}`);
} catch (e) {
  fail(`scenario threw: ${e.stack || e.message}`);
} finally {
  safeRemoveTree(temp);
}

// hook
const hook = readFileSync(join(ROOT, ".githooks", "pre-commit"), "utf8");
if (!hook.includes("tools/merge/register-drivers.mjs")) fail("pre-commit does not call tools/merge/register-drivers.mjs");
const hookCode = hook.split("\n").filter((l) => !l.trim().startsWith("#")).join("\n");
if (/include\.path/.test(hookCode)) fail("pre-commit adds an include.path itself");

// callers: nothing else may write a drivers include.path
const ALLOWED = new Set(["tools/merge/register-drivers.mjs", "tools/merge/drivers.gitconfig", "tools/prs.mjs"]);
function walk(dir, out) {
  for (const name of readdirSync(dir)) {
    if (name === "node_modules" || name === "mcp" || name === ".git") continue;
    const p = join(dir, name);
    if (statSync(p).isDirectory()) walk(p, out);
    else if (/\.(mjs|js|ps1|sh|cmd)$/.test(name)) out.push(p);
  }
}
const files = [];
for (const d of ["tools", "agent-box"]) walk(join(ROOT, d), files);
for (const f of files) {
  const rel = fwd(relative(ROOT, f));
  if (ALLOWED.has(rel) || rel.startsWith("tools/validation/")) continue;
  readFileSync(f, "utf8").split(/\r?\n/).forEach((line, i) => {
    if (/include\.path/.test(line) && /(--add|--replace-all|--unset|"config")/.test(line)) {
      fail(`${rel}:${i + 1} writes include.path; only register-drivers.mjs may: ${line.trim()}`);
    }
  });
}

if (failures.length) {
  console.log("drivers-include-selftest: FAIL");
  for (const f of failures) console.log(`  - ${f}`);
  process.exit(1);
}
console.log("drivers-include-selftest: OK (one shared include from main and two worktrees; stale lines pruned, unrelated kept; busy config left alone; hook and callers never add one)");
