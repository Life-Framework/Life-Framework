// tools/validation/heal-hotspots-selftest.mjs - the two old hotspots no longer serialize, and old branches heal.
// inputs: tools/lib/heal-hotspots.mjs, tools/lib/static-reset-hooks.mjs, tools/lib/features-index.mjs, tools/lib/ship-queue.mjs, tools/merge/merge-markdown.mjs, tools/validation/heal-hotspots-selftest.mjs
//
// Builds throwaway git repos under the OS temp dir:
//   1. old main: Core/LF_StaticReset.c with the central list, docs/features.md with two sections.
//   2. main migrates: per-feature hooks (centralFile + addHooks) and docs/features/<area>.md + index.
//   3. Two new-style branches off migrated main each register a reset and add a features row. The ship
//      queue's overlap() finds no shared file (no serialization) and `git merge-tree` merges them cleanly.
//   4. A branch cut from old main appends to the central list and to docs/features.md. A plain merge of
//      main conflicts (the red: that is the DIRTY churn); healBranch merges with no conflict, the hook
//      lands in the class's own file and the row in its area file.
//   5. A branch that still adds the retired `modded class LF_StaticResetHooks` layer (#2034: the chain
//      overflowed the stack) heals into an LF_StaticResetHook subclass.
//
// red-proof: 2026-11-16 healBranch with the hook step removed (calls not placed) printed
// FAIL "heal: LF_Delta's reset is now a hook in Feature/Delta/LF_Delta.c"; step 4's plain merge printing
// "PASS old branch + migrated main: a plain merge conflicts" is the standing red for the problem itself.
import { execFileSync, spawnSync } from "node:child_process";
import { mkdirSync, mkdtempSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join } from "node:path";
import { CENTRAL, addHooks, centralCalls, centralFile, hookedCalls, registryHooks, staticResetProblems } from "../lib/static-reset-hooks.mjs";
import { areaPath, renderIndex, splitSections } from "../lib/features-index.mjs";
import { healBranch } from "../lib/heal-hotspots.mjs";
import { overlap } from "../lib/ship-queue.mjs";

let failed = 0;
const check = (ok, msg) => {
  console.log(`${ok ? "PASS" : "FAIL"}  ${msg}`);
  if (!ok) failed++;
};

const dir = mkdtempSync(join(tmpdir(), "lf-heal-selftest-"));
const g = (...args) => execFileSync("git", args, { cwd: dir, encoding: "utf8", windowsHide: true, stdio: ["ignore", "pipe", "pipe"] });
const put = (rel, text) => {
  mkdirSync(dirname(join(dir, rel)), { recursive: true });
  writeFileSync(join(dir, rel), text);
};
const get = (rel) => readFileSync(join(dir, rel), "utf8");
const changed = (a, b) => g("diff", "--name-only", a, b).split("\n").filter(Boolean);

const GAME = "addons/LifeFramework/Scripts/Game";
const cls = (name) => `class ${name}\n{\n\tprotected static int s_i;\n\n\tstatic void ResetStatics()\n\t{\n\t\ts_i = 0;\n\t}\n}\n`;
const oldCentral = (names) => `class LF_StaticReset\n{\n\tprotected static int s_iRuns;\n\n\tstatic void ResetAll()\n\t{\n${names.map((n) => `\t\t${n}.ResetStatics();\n`).join("")}\n\t\ts_iRuns = s_iRuns + 1;\n\t\tLF_Debug.Log("StaticReset", "reset");\n\t}\n}\n`;
const FEATURES = `# Features

> preamble

## Alpha

### \`LF_Alpha\`
- alpha row 1

## Beta

### \`LF_Beta\`
- beta row 1
`;

try {
  g("init", "-q", "-b", "main");
  g("config", "user.email", "selftest@example.invalid");
  g("config", "user.name", "selftest");
  g("config", "commit.gpgsign", "false");
  g("config", "core.autocrlf", "false");

  // 1. old main
  put(CENTRAL, oldCentral(["LF_Alpha"]));
  for (const n of ["Alpha", "Beta", "Gamma", "Delta"]) put(`${GAME}/Feature/${n}/LF_${n}.c`, cls(`LF_${n}`));
  put("docs/features.md", FEATURES);
  g("add", "-A");
  g("commit", "-q", "-m", "old main");
  const oldMain = g("rev-parse", "HEAD").trim();

  // 4a. a branch cut from old main that appends to both central files
  g("checkout", "-q", "-b", "old-pr");
  put(CENTRAL, oldCentral(["LF_Alpha", "LF_Delta"]));
  put("docs/features.md", FEATURES.replace("- beta row 1\n", "- beta row 1\n- beta row 2 (old-pr)\n"));
  g("commit", "-q", "-am", "old-pr: reset LF_Delta, features row");

  // 2. main migrates
  g("checkout", "-q", "main");
  put(CENTRAL, centralFile());
  put(`${GAME}/Feature/Alpha/LF_Alpha.c`, addHooks(get(`${GAME}/Feature/Alpha/LF_Alpha.c`), [{ cls: "LF_Alpha", method: "ResetStatics" }]));
  for (const s of splitSections(FEATURES)) put(areaPath(s.slug), s.body);
  put("docs/features.md", renderIndex(dir));
  g("add", "-A");
  g("commit", "-q", "-m", "migrate");
  check(centralCalls(get(CENTRAL)).length === 0 && hookedCalls(get(`${GAME}/Feature/Alpha/LF_Alpha.c`)).length === 1, "migration: no central call, LF_Alpha hooks itself");
  const alphaArea = areaPath(splitSections(FEATURES).find((s) => /Alpha/.test(s.heading)).slug);
  const betaArea = areaPath(splitSections(FEATURES).find((s) => /Beta/.test(s.heading)).slug);

  // 3. two new-style branches: each registers a reset and adds a row
  const branch = (name, c, area, row) => {
    g("checkout", "-q", "-b", name, "main");
    const f = `${GAME}/Feature/${c}/LF_${c}.c`;
    put(f, addHooks(get(f), [{ cls: `LF_${c}`, method: "ResetStatics" }]));
    put(area, get(area).replace(/\s*$/, `\n- ${row}\n`));
    g("commit", "-q", "-am", name);
    return changed("main", name);
  };
  const filesX = branch("pr-x", "Beta", betaArea, "beta row (pr-x)");
  const filesY = branch("pr-y", "Gamma", alphaArea, "alpha row (pr-y)");
  check(!filesX.includes(CENTRAL) && !filesY.includes(CENTRAL), "neither new-style branch touches LF_StaticReset.c");
  check(overlap(filesX, filesY).length === 0, `ship queue: the two reset PRs share no file, so neither waits (${filesX.join(", ")} | ${filesY.join(", ")})`);
  const mt = spawnSync("git", ["merge-tree", "--write-tree", "pr-x", "pr-y"], { cwd: dir, encoding: "utf8", windowsHide: true });
  check(mt.status === 0, "git merge-tree: pr-x and pr-y merge cleanly");
  g("checkout", "-q", "main");
  g("merge", "-q", "--no-edit", "pr-x");
  g("merge", "-q", "--no-edit", "pr-y");
  const regs = [...["Alpha", "Beta", "Gamma"].flatMap((n) => hookedCalls(get(`${GAME}/Feature/${n}/LF_${n}.c`)).map((c) => c.cls))];
  check(regs.join() === "LF_Alpha,LF_Beta,LF_Gamma", `both merged into main: every reset registered once (${regs.join(", ")})`);

  // 4b. the old branch: a plain merge conflicts (the problem), healBranch does not
  g("checkout", "-q", "old-pr");
  const plain = spawnSync("git", ["merge", "--no-commit", "--no-ff", "main"], { cwd: dir, encoding: "utf8", windowsHide: true });
  const conflicted = g("diff", "--name-only", "--diff-filter=U").split("\n").filter(Boolean);
  check(plain.status !== 0 && conflicted.includes(CENTRAL), `old branch + migrated main: a plain merge conflicts (${conflicted.join(", ")})`);
  g("merge", "--abort");

  const r = await healBranch({ root: dir, onto: "main" });
  if (process.env.LF_HEAL_DEBUG) console.log(r.log.join("\n"), g("status"));
  check(r.ok, `heal: merged with nothing left unmerged${r.ok ? "" : ` (${r.log.join("; ")})`}`);
  check(centralCalls(get(CENTRAL)).length === 0 && get(CENTRAL) === g("show", `main:${CENTRAL}`), "heal: LF_StaticReset.c is main's, with no list");
  check(hookedCalls(get(`${GAME}/Feature/Delta/LF_Delta.c`)).filter((c) => c.cls === "LF_Delta").length === 1, "heal: LF_Delta's reset is now a hook in Feature/Delta/LF_Delta.c");
  check(hookedCalls(get(`${GAME}/Feature/Beta/LF_Beta.c`)).length === 1, "heal: main's LF_Beta hook is kept as is");
  const beta = get(betaArea);
  check(beta.includes("- beta row 2 (old-pr)") && beta.includes("- beta row (pr-x)") && !/^(<{7}|>{7})/m.test(beta), "heal: the old-pr row lands in its area file beside main's row");
  check(get("docs/features.md") === renderIndex(dir) && !/beta row/.test(get("docs/features.md")), "heal: docs/features.md stays the generated index");
  check(g("status", "--porcelain").trim() === "", "heal: committed, worktree clean");
  check(g("merge-base", "--is-ancestor", "main", "HEAD") === "", "heal: main is merged into the branch");
  const again = await healBranch({ root: dir, onto: "main" });
  check(again.ok && !again.plan.needed, "heal again: nothing to do (idempotent)");
  // 5. a branch that still adds the retired modded layer (#2034) heals into a registry hook class
  g("checkout", "-q", "-b", "layer-pr", "main");
  const eps = `${GAME}/Feature/Epsilon/LF_Epsilon.c`;
  put(eps, `${cls("LF_Epsilon")}
modded class LF_StaticResetHooks
{
	override void Run(notnull array<string> done, bool apply)
	{
		super.Run(done, apply);
		done.Insert("LF_Epsilon");
		if (apply)
			LF_Epsilon.ResetStatics();
	}
}
`);
  g("add", "-A");
  g("commit", "-q", "-m", "layer-pr: modded reset layer");
  g("checkout", "-q", "main");
  put("docs/unrelated.md", "main moved on\n");
  g("add", "-A");
  g("commit", "-q", "-m", "main moves");
  g("checkout", "-q", "layer-pr");
  const lr = await healBranch({ root: dir, onto: "main" });
  const healed = get(eps);
  check(lr.ok && !/modded\s+class\s+LF_StaticResetHooks/.test(healed) && staticResetProblems(healed).problems.length === 0, `heal: the modded layer became a registry hook (${lr.log.join("; ")})`);
  check(registryHooks(healed).map((h) => h.owner).join() === "LF_Epsilon" && hookedCalls(healed).length === 1, "heal: LF_Epsilon_StaticResetHook registers LF_Epsilon once");
  void oldMain;
} catch (e) {
  check(false, `selftest threw: ${e.message} ${e.stderr || ""} ${e.stdout || ""}`);
} finally {
  rmSync(dir, { recursive: true, force: true });
}

console.log(failed ? `heal-hotspots-selftest: ${failed} FAILED` : "heal-hotspots-selftest: all passed");
process.exit(failed ? 1 : 0);
