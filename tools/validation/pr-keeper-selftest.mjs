// Proves `prs heal`'s merge logic on a throwaway repo with a bare origin.
// inputs: tools/**, agent-box/**, .githooks/**
// Never touches this repo, its worktrees, or GitHub.
//
//   ws/registry-only  both sides added a test, so only the generated registry
//                     conflicts -> healed (regenerated), merge committed, pushed.
//   ws/real-conflict  both sides edited one script line -> merge aborted,
//                     HEAD and origin untouched, path reported.
//   ws/mixed          registry plus a real conflict -> aborted like real-conflict.
//
// red-proof (2026-11-08): without `git merge --abort` in healMerge's abort path
//   it printed "real-conflict: merge left in progress"; with the registry
//   unclassified it printed "registry-only: expected healed, got needs-human".
import { mkdirSync, mkdtempSync, readFileSync, readdirSync, rmSync, utimesSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { spawnSync } from "node:child_process";
import { conflictKind, healMerge, isLive, predictConflicts, pushBranch, LIVE_MS } from "../prs.mjs";
import { safeRemoveTree } from "../link-safety.mjs";

const failures = [];
const fail = (m) => failures.push(m);
const TESTS = "addons/LifeFramework/Scripts/Game/Tests";
const REGISTRY = `${TESTS}/LF_TestRegistrations.generated.c`;
const SCRIPT = "addons/LifeFramework/Scripts/Game/Core/LF_Foo.c";

function git(dir, args) {
  return spawnSync("git", args, { cwd: dir, windowsHide: true, encoding: "utf8" });
}
function must(dir, args) {
  const r = git(dir, args);
  if (r.status !== 0) throw new Error(`git ${args.join(" ")} in ${dir}: ${r.stderr || r.stdout}`);
  return r.stdout.trim();
}
function write(dir, rel, text) {
  mkdirSync(join(dir, rel, ".."), { recursive: true });
  writeFileSync(join(dir, rel), text);
}
// Stand-in for gen-test-registry.ps1: one line per test file, sorted.
function regenRegistry(dir) {
  const names = readdirSync(join(dir, TESTS)).filter((n) => n.endsWith(".c") && !n.endsWith(".generated.c")).sort();
  write(dir, REGISTRY, `// GENERATED\n${names.map((n) => `register ${n.replace(/\.c$/, "")}`).join("\n")}\n`);
}
function addTest(dir, name) {
  write(dir, `${TESTS}/${name}.c`, `// tier: LOGIC\nclass ${name} : LF_Test {}\n`);
  regenRegistry(dir);
}
function commit(dir, msg) {
  must(dir, ["add", "-A"]);
  must(dir, ["commit", "-q", "-m", msg]);
}
const regenerate = (dir, kind) => {
  if (kind !== "regen-tests") return { ok: false, error: `unexpected ${kind}` };
  regenRegistry(dir);
  return { ok: true };
};

// Classification.
for (const [p, k] of [
  [REGISTRY, "regen-tests"],
  ["addons/LifeFramework/Language/everonlife_localization.en_us.conf", "regen-localization"],
  ["addons/LifeFramework/Language/everonlife_localization.st", "string-table"],
  ["docs/features.md", "markdown"],
  ["docs/debug-log-triage.md", "markdown"],
  ["tools/validation/enforce-lint-baseline.json", "lint-baseline"],
  [SCRIPT, null],
  [`${TESTS}/LF_Test_A.c`, null],
  ["docs/worktrees.md", null],
]) {
  if (conflictKind(p) !== k) fail(`conflictKind(${p}) = ${conflictKind(p)}, expected ${k}`);
}

const temp = mkdtempSync(join(tmpdir(), "lf-pr-keeper-"));
try {
  const origin = join(temp, "origin.git");
  const seed = join(temp, "seed");
  must(temp, ["init", "-q", "--bare", "-b", "main", origin]);
  must(temp, ["clone", "-q", origin, seed]);
  for (const [k, v] of [["user.email", "pr-keeper@example.com"], ["user.name", "pr-keeper"], ["core.autocrlf", "false"], ["commit.gpgsign", "false"], ["core.hooksPath", join(temp, "no-hooks")]]) {
    must(seed, ["config", k, v]);
  }
  must(seed, ["checkout", "-q", "-b", "main"]);
  addTest(seed, "LF_Test_A");
  write(seed, SCRIPT, "void Foo()\n{\n\tint x = 1;\n}\n");
  commit(seed, "base");
  must(seed, ["push", "-q", "origin", "main"]);

  const branch = (name, fn) => {
    must(seed, ["checkout", "-q", "-b", name, "main"]);
    fn();
    commit(seed, name);
    must(seed, ["push", "-q", "origin", name]);
  };
  branch("ws/registry-only", () => addTest(seed, "LF_Test_B"));
  branch("ws/real-conflict", () => write(seed, SCRIPT, "void Foo()\n{\n\tint x = 2;\n}\n"));
  branch("ws/mixed", () => {
    addTest(seed, "LF_Test_D");
    write(seed, SCRIPT, "void Foo()\n{\n\tint x = 4;\n}\n");
  });
  must(seed, ["checkout", "-q", "main"]);
  addTest(seed, "LF_Test_C");
  write(seed, SCRIPT, "void Foo()\n{\n\tint x = 3;\n}\n");
  commit(seed, "main moves");
  must(seed, ["push", "-q", "origin", "main"]);
  must(seed, ["fetch", "-q", "origin"]);

  const wtOf = (name) => {
    const dir = join(temp, name.replace("/", "-"));
    must(seed, ["worktree", "add", "-q", dir, name]);
    return dir;
  };

  // Dry-run prediction sees the registry conflict and the real one.
  const predicted = predictConflicts(seed, "origin/ws/mixed");
  if (!predicted.includes(REGISTRY) || !predicted.includes(SCRIPT)) fail(`predictConflicts(ws/mixed) = ${predicted.join(", ")}`);

  // Healed: only the generated registry conflicts.
  const a = wtOf("ws/registry-only");
  const r1 = healMerge(a, { regenerate });
  if (r1.status !== "healed") fail(`registry-only: expected healed, got ${r1.status} (${r1.reason ?? ""} ${(r1.conflicted ?? []).join(", ")})`);
  else {
    const reg = readFileSync(join(a, REGISTRY), "utf8");
    for (const t of ["LF_Test_A", "LF_Test_B", "LF_Test_C"]) if (!reg.includes(`register ${t}\n`)) fail(`healed registry misses ${t}:\n${reg}`);
    if (/<{7}|>{7}/.test(reg)) fail("healed registry still has conflict markers");
    if (git(a, ["merge-base", "--is-ancestor", "origin/main", "HEAD"]).status !== 0) fail("healed branch does not contain origin/main");
    if (git(a, ["status", "--porcelain"]).stdout.trim()) fail("healed worktree is dirty");
    if (pushBranch(a, "ws/registry-only").status !== 0) fail("push of the healed branch failed");
    else if (must(seed, ["ls-remote", origin, "refs/heads/ws/registry-only"]).split(/\s/)[0] !== r1.head) fail("origin did not receive the healed head");
  }

  // Reported and untouched: a real code conflict, alone or with the registry.
  for (const name of ["ws/real-conflict", "ws/mixed"]) {
    const dir = wtOf(name);
    const before = must(dir, ["rev-parse", "HEAD"]);
    const remoteBefore = must(seed, ["ls-remote", origin, `refs/heads/${name}`]);
    const r = healMerge(dir, { regenerate });
    const label = name.slice(3);
    if (r.status !== "needs-human") fail(`${label}: expected needs-human, got ${r.status}`);
    if (!r.conflicted.includes(SCRIPT)) fail(`${label}: conflicted paths ${r.conflicted.join(", ")} miss ${SCRIPT}`);
    if (r.conflicted.includes(REGISTRY)) fail(`${label}: reported the generated registry as needing a human`);
    if (must(dir, ["rev-parse", "HEAD"]) !== before) fail(`${label}: HEAD moved`);
    if (git(dir, ["status", "--porcelain"]).stdout.trim()) fail(`${label}: worktree left dirty`);
    if (git(dir, ["rev-parse", "-q", "--verify", "MERGE_HEAD"]).status === 0) fail(`${label}: merge left in progress`);
    if (must(seed, ["ls-remote", origin, `refs/heads/${name}`]) !== remoteBefore) fail(`${label}: origin branch changed`);
  }

  // Legacy worktree: no merge attributes, no driver config, no driver scripts.
  // Both sides append to the inventory table, gallery.json and areas.json;
  // heal must still resolve them through the hub's drivers.
  {
    const legacy = join(temp, "legacy");
    must(temp, ["clone", "-q", origin, legacy]);
    for (const [k, v] of [["user.email", "pr-keeper@example.com"], ["user.name", "pr-keeper"], ["core.autocrlf", "false"], ["commit.gpgsign", "false"], ["core.hooksPath", join(temp, "no-hooks")]]) must(legacy, ["config", k, v]);
    const INV = "docs/asset-inventory.md", GAL = "website/content/gallery.json", AREAS = "agent-box/areas.json";
    const NL = String.fromCharCode(10);
    const doc = (lines) => lines.join(NL) + NL;
    const inv = (...rows) => doc(["# Inventory", "", "| Item | Prefab |", "|---|---|", "| Base | base |", ...rows.map((r) => "| " + r + " | " + r + " |")]);
    const gal = (...ids) => doc(["{", '  "sections": [', ["base", ...ids].map((i) => '    { "id": "' + i + '", "title": "T" }').join("," + NL), "  ]", "}"]);
    const areas = (...names) => doc(["{", '  "areas": [', ["base", ...names].map((n) => '    { "name": "' + n + '", "x": 1 }').join("," + NL), "  ]", "}"]);
    const files = (...n) => { write(legacy, INV, inv(...n)); write(legacy, GAL, gal(...n)); write(legacy, AREAS, areas(...n)); };
    must(legacy, ["checkout", "-q", "-b", "ws/legacy-base", "origin/main"]);
    files();
    commit(legacy, "legacy base");
    must(legacy, ["push", "-q", "origin", "ws/legacy-base:refs/heads/legacy-base"]);
    must(legacy, ["checkout", "-q", "-b", "ws/legacy-a"]);
    files("A");
    commit(legacy, "a appends");
    must(legacy, ["checkout", "-q", "-b", "legacy-main", "ws/legacy-base"]);
    files("B");
    commit(legacy, "b appends");
    must(legacy, ["checkout", "-q", "ws/legacy-a"]);
    const r = healMerge(legacy, { base: "legacy-main", regenerate });
    if (r.status !== "merged" && r.status !== "healed") fail(`legacy append: expected merged, got ${r.status} (${r.reason ?? ""} ${(r.conflicted ?? []).join(", ")})`);
    else {
      for (const f of [INV, GAL, AREAS]) {
        const t = readFileSync(join(legacy, f), "utf8");
        if (!t.includes("A") || !t.includes("B") || /<{7}|>{7}/.test(t)) fail(`legacy append: ${f} lost a side or has markers:
${t}`);
      }
      JSON.parse(readFileSync(join(legacy, GAL), "utf8"));
      JSON.parse(readFileSync(join(legacy, AREAS), "utf8"));
    }
  }

  // Liveness: a fresh change is live now and idle 15 min later.
  write(a, "tmp/note.txt", "agent scratch\n");
  if (!isLive(a).live) fail("a worktree changed just now should be live");
  if (isLive(a, Date.now() + LIVE_MS + 60000).live) fail("a worktree idle for 16 min should not be live");

  // #698: a bulk touch (prune, fetch, sweeps) must not read as LIVE. A stale
  // worktree has an old last commit; every directory and git metadata file is
  // then touched to "now". Red-proof: the old rule (index/HEAD/dir mtimes) read this as live.
  const stale = join(temp, "stale");
  mkdirSync(stale);
  must(stale, ["init", "-q", "-b", "ws/stale"]);
  write(stale, "tmp/keep.txt", "x\n");
  write(stale, "docs/a.md", "a\n");
  must(stale, ["add", "-A", "-f"]);
  const old = new Date(Date.now() - 3 * 3600 * 1000).toISOString();
  spawnSync("git", ["-c", "user.name=t", "-c", "user.email=t@t", "commit", "-q", "-m", "old"], {
    cwd: stale, windowsHide: true, env: { ...process.env, GIT_COMMITTER_DATE: old, GIT_AUTHOR_DATE: old },
  });
  const past = new Date(Date.now() - 3 * 3600 * 1000);
  for (const f of ["tmp/keep.txt", "docs/a.md"]) utimesSync(join(stale, f), past, past);
  const nowD = new Date();
  for (const f of [".", "tmp", "docs", ".git", ".git/index", ".git/HEAD", ".git/logs", ".git/logs/HEAD"]) {
    try { utimesSync(join(stale, f), nowD, nowD); } catch {}
  }
  if (isLive(stale).live) fail("bulk touch of directories and git metadata on a stale worktree must not read as LIVE");
  write(stale, "docs/new.md", "agent edit\n");
  if (!isLive(stale).live) fail("an untracked file written in a stale worktree should read as LIVE");
  rmSync(join(stale, "docs/new.md"));
  if (isLive(stale).live) fail("worktree should be idle again once the edit is gone");
  write(stale, "docs/a.md", "edited\n");
  if (!isLive(stale).live) fail("a modified tracked file should read as LIVE");
  must(stale, ["checkout", "-q", "--", "docs/a.md"]);
  utimesSync(join(stale, "docs/a.md"), past, past);
  if (isLive(stale).live) fail("a reverted worktree with an old commit should be idle");
  git(stale, ["-c", "user.name=t", "-c", "user.email=t@t", "commit", "-q", "--allow-empty", "-m", "fresh"]);
  if (!isLive(stale).live) fail("a fresh commit should read as LIVE");
} catch (e) {
  fail(`scenario threw: ${e.message}`);
} finally {
  safeRemoveTree(temp);
}

if (failures.length) {
  console.log("pr-keeper-selftest: FAIL");
  for (const f of failures) console.log(`  - ${f}`);
  process.exit(1);
}
console.log("pr-keeper-selftest: OK (registry-only conflict healed and pushed; legacy worktree appends to inventory, gallery and areas merged by the hub drivers; real and mixed conflicts aborted, HEAD and origin untouched; liveness window)");
