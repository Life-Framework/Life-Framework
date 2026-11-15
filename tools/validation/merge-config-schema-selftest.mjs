// Proves the config schema merge driver (#1258) in a temporary git repo: two branches that each add
// a config field merge cleanly, and docs/config/config.schema.json and config.example.json hold both.
// A control merge without the .gitattributes lines leaves conflict markers, so the fixture is real.
// inputs: tools/merge/**, tools/config/**, tools/economy/profit-loops.mjs, .gitattributes
// Does not boot the game and does not merge this worktree.
import { spawnSync } from "node:child_process";
import { cpSync, mkdirSync, mkdtempSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { pickThree } from "../merge/regen-config-schema.mjs";

const ROOT = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
const failures = [];
const fail = (m) => failures.push(m);

// Unit: a three-way pick takes the changed side, line-merges disjoint edits, refuses overlapping ones.
const lines = (...l) => l.join("\n") + "\n";
const base3 = lines("a", "b", "c", "d", "e");
if (pickThree(base3, base3, "A\nb\nc\nd\ne\n").text !== "A\nb\nc\nd\ne\n") fail("one-sided change should take that side");
const disjoint = pickThree(base3, "A\nb\nc\nd\ne\n", "a\nb\nc\nd\nE\n");
if (!disjoint.ok || disjoint.text !== "A\nb\nc\nd\nE\n") fail(`disjoint edits should line-merge, got ${JSON.stringify(disjoint)}`);
if (pickThree(base3, "a\nX\nc\nd\ne\n", "a\nY\nc\nd\ne\n").ok) fail("overlapping edits should report a conflict");

const SERVICE = "addons/LifeFramework/Scripts/Game/Core/Config/LF_ConfigService.c";
const DEMO = "addons/LifeFramework/Scripts/Game/Feature/Demo/LF_DemoConfig.c";
const field = (name) => `\t[Attribute("1", UIWidgets.Auto, "The ${name} knob", params: "0 100")]\n\tint m_i${name};\n\n`;
const demo = (...names) => `//! Demo tuning.
[BaseContainerProps(configRoot: true)]
class LF_DemoConfig
{
\tstatic const string SECTION = "demo";

${names.map(field).join("")}}
`;

function run(cwd, cmd, args, env = {}) {
  return spawnSync(cmd, args, { cwd, encoding: "utf8", windowsHide: true, env: { ...process.env, ...env } });
}
function git(cwd, ...args) {
  return run(cwd, "git", ["-c", "user.name=t", "-c", "user.email=t@t", "-c", "core.autocrlf=false", ...args]);
}

function makeRepo(withAttributes) {
  const dir = mkdtempSync(join(tmpdir(), "lf-cfg-schema-"));
  git(dir, "init", "-q", "-b", "main");
  cpSync(join(ROOT, "tools", "merge"), join(dir, "tools", "merge"), { recursive: true });
  mkdirSync(join(dir, "tools", "config"), { recursive: true });
  for (const f of ["schema.mjs", "pack-sections.mjs"]) cpSync(join(ROOT, "tools", "config", f), join(dir, "tools", "config", f));
  mkdirSync(join(dir, "tools", "economy"), { recursive: true });
  cpSync(join(ROOT, "tools", "economy", "profit-loops.mjs"), join(dir, "tools", "economy", "profit-loops.mjs"));
  writeFileSync(join(dir, "tools", "config", "schema-overrides.json"), "{}\n");
  for (const [rel, text] of [[SERVICE, "class LF_ConfigService\n{\n\tstatic const int SCHEMA_VERSION = 1;\n}\n"], [DEMO, demo("Base")]]) {
    mkdirSync(dirname(join(dir, rel)), { recursive: true });
    writeFileSync(join(dir, rel), text);
  }
  writeFileSync(
    join(dir, ".gitattributes"),
    withAttributes ? "docs/config/config.schema.json merge=lf-regen-config-schema\ndocs/config/config.example.json merge=lf-regen-config-schema\n" : "",
  );
  git(dir, "config", "--local", "--add", "include.path", join(ROOT, "tools", "merge", "drivers.gitconfig"));
  regen(dir);
  git(dir, "add", "-A");
  git(dir, "commit", "-q", "-m", "base");
  return dir;
}

const regen = (dir) => run(dir, process.execPath, ["tools/config/schema.mjs", "--write"]);

function branchAdding(dir, branch, names) {
  git(dir, "checkout", "-q", "-b", branch, "main");
  writeFileSync(join(dir, DEMO), demo(...names));
  regen(dir);
  git(dir, "commit", "-q", "-am", `add ${branch}`);
}

function scenario(withAttributes) {
  const dir = makeRepo(withAttributes);
  try {
    // Alpha is added first in the class and beta last, so the .c files line-merge cleanly.
    branchAdding(dir, "alpha", ["Alpha", "Base"]);
    branchAdding(dir, "beta", ["Base", "Beta"]);
    git(dir, "checkout", "-q", "alpha");
    const merge = git(dir, "merge", "--no-edit", "beta");
    const read = (f) => readFileSync(join(dir, "docs", "config", f), "utf8");
    return {
      merge,
      schema: read("config.schema.json"),
      example: read("config.example.json"),
      stale: run(dir, process.execPath, ["tools/config/schema.mjs", "--check"]).status,
    };
  } finally {
    rmSync(dir, { recursive: true, force: true });
  }
}

const control = scenario(false);
if (control.merge.status === 0 || !(control.schema + control.example).includes("<<<<<<<")) {
  fail("control: without the driver the two config edits should conflict; the fixture proves nothing");
}

const out = scenario(true);
if (out.merge.status !== 0) fail(`two branches that each add a field should merge clean:\n${out.merge.stdout}${out.merge.stderr}`);
for (const key of ["alpha", "base", "beta"]) {
  if (!out.schema.includes(`"${key}"`)) fail(`merged schema lost the "${key}" field`);
  if (!out.example.includes(`"${key}"`)) fail(`merged example lost the "${key}" field`);
}
if (/<<<<<<<|>>>>>>>/.test(out.schema + out.example)) fail("merged files hold conflict markers");
if (out.stale !== 0) fail("merged schema and example are not what the generator writes for the merged classes");

// The shipped config declares the driver and the slice planner treats both files as shared.
const attrs = readFileSync(join(ROOT, ".gitattributes"), "utf8");
for (const f of ["config.schema.json", "config.example.json"]) {
  if (!attrs.includes(`docs/config/${f} merge=lf-regen-config-schema`)) fail(`.gitattributes does not route ${f} to lf-regen-config-schema`);
}
if (!readFileSync(join(ROOT, "tools", "merge", "drivers.gitconfig"), "utf8").includes("[merge \"lf-regen-config-schema\"]")) {
  fail("drivers.gitconfig does not define lf-regen-config-schema");
}
const { SHARED } = await import("../lib/slice.mjs");
for (const f of ["docs/config/config.schema.json", "docs/config/config.example.json"]) {
  if (!SHARED.includes(f)) fail(`slice.mjs SHARED does not list ${f}`);
}
// LF_StaticReset.c is no longer shared: each feature registers its reset in its own file (AGENTS.md rule 10).
if (SHARED.includes("addons/LifeFramework/Scripts/Game/Core/LF_StaticReset.c")) fail("slice.mjs SHARED still lists LF_StaticReset.c; resets are per-feature hooks now");

if (failures.length) {
  for (const f of failures) console.log(`FAIL ${f}`);
  process.exit(1);
}
console.log("merge-config-schema: ok");
