// Git merge driver for generated files.
// Language confs resolve by tools\cli regen-localization.
// LF_TestRegistrations.generated.c resolves by tools\cli regen-tests.
// The driver runs those commands. It does not hand-edit the generated text.
//
// The driver runs mid-merge, so it must never write into the worktree: a test file that only the
// incoming side has would land as an untracked file and git would then refuse to write its own copy
// ("untracked working tree files would be overwritten by merge"). The inputs are materialised in a
// scratch git repo under os.tmpdir(), the generator runs there, and only %A is written.
//
//   node tools/merge/regen-generated.mjs %O %A %B %P

import { spawnSync } from "node:child_process";
import { copyFileSync, existsSync, mkdirSync, mkdtempSync, readdirSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { mergeStringTables } from "./union-string-table.mjs";

// The core's string table and each pack's (#325): the driver unions every one, then regenerates all confs.
const ST_PATHS = [
  "addons/LifeFramework/Language/everonlife_localization.st",
  "addons/LF_Everon1989/Language/lf_everon1989_localization.st",
];
const TOOLS_ROOT = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
const TESTS_PREFIX = "addons/LifeFramework/Scripts/Game/Tests";
const REGISTRY_NAME = "LF_TestRegistrations.generated.c";

function git(args) {
  return spawnSync("git", args, { windowsHide: true, encoding: "utf8" });
}

function gitPath(rel) {
  const result = git(["rev-parse", "--git-path", rel]);
  if (result.status !== 0) return null;
  return result.stdout.trim();
}

export function regenKind(pathname) {
  const path = String(pathname ?? "").replace(/\\/g, "/");
  if (path.endsWith(`/${REGISTRY_NAME}`) || path === REGISTRY_NAME) return "tests";
  if (path.includes("/Language/") && path.endsWith(".conf")) return "localization";
  if (/everonlife_localization\.[A-Za-z0-9_]+\.conf$/.test(path)) return "localization";
  return null;
}

export function regenPlan(pathname) {
  const kind = regenKind(pathname);
  if (kind === "tests") {
    return { kind, script: "tools/validation/gen-test-registry.ps1", args: ["-Write"] };
  }
  if (kind === "localization") {
    return { kind, script: "tools/validation/regen-localization.ps1", args: ["-Write"] };
  }
  return null;
}

export function resolveSides() {
  const head = git(["rev-parse", "HEAD"]);
  if (head.status !== 0) return null;
  const ours = head.stdout.trim();
  let theirs = null;
  let baseFromParent = false;
  const rebase = gitPath("rebase-merge");
  if (rebase && existsSync(join(rebase, "done"))) {
    const done = readFileSync(join(rebase, "done"), "utf8");
    const picks = [...done.matchAll(/^pick\s+([0-9a-f]{7,40})/gim)];
    if (picks.length) {
      theirs = picks[picks.length - 1][1];
      baseFromParent = true;
    }
  }
  if (!theirs) {
    const apply = gitPath("rebase-apply");
    if (apply && existsSync(join(apply, "original-commit"))) {
      theirs = readFileSync(join(apply, "original-commit"), "utf8").trim();
      baseFromParent = true;
    }
  }
  if (!theirs) {
    for (const key of Object.keys(process.env)) {
      const match = /^GITHEAD_([0-9a-f]{40})$/i.exec(key);
      if (match) {
        theirs = match[1];
        break;
      }
    }
  }
  if (!theirs) {
    for (const name of ["MERGE_HEAD", "CHERRY_PICK_HEAD"]) {
      const path = gitPath(name);
      if (path && existsSync(path)) {
        const text = readFileSync(path, "utf8").trim();
        if (text) {
          theirs = text;
          if (name === "CHERRY_PICK_HEAD") baseFromParent = true;
          break;
        }
      }
    }
  }
  if (!theirs) return null;
  const base = baseFromParent ? git(["rev-parse", `${theirs}^`]) : git(["merge-base", ours, theirs]);
  if (base.status !== 0) return null;
  return { ours, theirs, base: base.stdout.trim() };
}

function showPath(rev, path) {
  const result = git(["-c", "core.autocrlf=false", "show", `${rev}:${path}`]);
  if (result.status !== 0) return null;
  return result.stdout;
}

function listTestSources(rev) {
  const result = git(["ls-tree", "-r", "--name-only", rev, "--", TESTS_PREFIX]);
  if (result.status !== 0) return [];
  return result.stdout
    .split(/\n/)
    .map((line) => line.trim())
    .filter((line) => line.endsWith(".c") && !line.endsWith(REGISTRY_NAME));
}

function normalize(text) {
  if (text == null) return null;
  return text.replace(/\r\n/g, "\n");
}

function pickThree(base, ours, theirs) {
  const baseNorm = normalize(base);
  const oursNorm = normalize(ours);
  const theirsNorm = normalize(theirs);
  if (oursNorm === theirsNorm) return { ok: true, text: ours ?? theirs };
  if (oursNorm === baseNorm) return { ok: true, text: theirs };
  if (theirsNorm === baseNorm) return { ok: true, text: ours };
  return { ok: false };
}

function writeInto(root, rel, text) {
  const full = join(root, rel);
  if (text == null) {
    if (existsSync(full)) rmSync(full);
    return;
  }
  mkdirSync(dirname(full), { recursive: true });
  writeFileSync(full, text);
}

// The generators ask git for the work tree root, so run them in a scratch repo with a clean git env.
function runPowershell(scratch, script, args) {
  const powershell = process.platform === "win32" ? "powershell.exe" : "pwsh";
  const env = {};
  for (const [key, value] of Object.entries(process.env)) {
    if (!/^GIT_(DIR|WORK_TREE|INDEX_FILE|PREFIX|COMMON_DIR|OBJECT_DIRECTORY|ALTERNATE_OBJECT_DIRECTORIES|NAMESPACE)$/i.test(key)) env[key] = value;
  }
  const init = spawnSync("git", ["init", "-q"], { windowsHide: true, cwd: scratch, encoding: "utf8", env });
  if (init.status !== 0) return init;
  return spawnSync(
    powershell,
    ["-NoProfile", "-ExecutionPolicy", "Bypass", "-File", join(TOOLS_ROOT, script), ...args],
    { windowsHide: true, cwd: scratch, encoding: "utf8", env },
  );
}

function materializeStringTable(scratch, sides) {
  // regen-localization.ps1 sorts the .st files with tools/merge/sort-string-table.mjs (and its imports) under its root.
  mkdirSync(join(scratch, "tools", "merge"), { recursive: true });
  for (const name of readdirSync(join(TOOLS_ROOT, "tools", "merge")).filter((entry) => entry.endsWith(".mjs"))) {
    copyFileSync(join(TOOLS_ROOT, "tools", "merge", name), join(scratch, "tools", "merge", name));
  }
  for (const stPath of ST_PATHS) {
    const base = showPath(sides.base, stPath) ?? "";
    const ours = showPath(sides.ours, stPath) ?? "";
    const theirs = showPath(sides.theirs, stPath) ?? "";
    if (!base && !ours && !theirs) continue;
    const merged = mergeStringTables(base, ours, theirs);
    if (!merged.ok) return merged;
    writeInto(scratch, stPath, merged.text);
  }
  return { ok: true };
}

function materializeTests(scratch, sides) {
  const paths = new Set([
    ...listTestSources(sides.base),
    ...listTestSources(sides.ours),
    ...listTestSources(sides.theirs),
  ]);
  for (const path of paths) {
    const picked = pickThree(showPath(sides.base, path), showPath(sides.ours, path), showPath(sides.theirs, path));
    if (!picked.ok) {
      return { ok: false, error: `lf-regen-tests: ${path} conflicts; resolve that test file, then tools\\cli regen-tests` };
    }
    writeInto(scratch, path, picked.text);
  }
  return { ok: true };
}

function runDriver() {
  const oursPath = process.argv[3];
  const pathname = process.argv[5] || "";
  const plan = regenPlan(pathname);
  if (!oursPath || !plan) {
    console.error("lf-regen: expected a language conf or LF_TestRegistrations.generated.c");
    process.exit(1);
  }
  const sides = resolveSides();
  if (!sides) {
    console.error("lf-regen: cannot identify the merge sides");
    process.exit(1);
  }
  if (process.env.LF_MERGE_REGEN_MODE === "plan") {
    writeFileSync(oursPath, `regen:${plan.kind}\n`);
    process.exit(0);
  }
  const scratch = mkdtempSync(join(tmpdir(), "lf-regen-"));
  try {
    const prepared = plan.kind === "tests" ? materializeTests(scratch, sides) : materializeStringTable(scratch, sides);
    if (!prepared.ok) {
      console.error(prepared.error);
      process.exitCode = 1;
      return;
    }
    const result = runPowershell(scratch, plan.script, plan.args);
    if (result.status !== 0) {
      if (result.stdout) process.stderr.write(result.stdout);
      if (result.stderr) process.stderr.write(result.stderr);
      process.exitCode = result.status || 1;
      return;
    }
    const outputRel = plan.kind === "tests"
      ? `${TESTS_PREFIX}/${REGISTRY_NAME}`
      : pathname.replace(/\\/g, "/").replace(/^\.\//, "");
    writeFileSync(oursPath, readFileSync(join(scratch, outputRel)));
  } finally {
    rmSync(scratch, { recursive: true, force: true });
  }
}

const invoked = process.argv[1] && process.argv[1].replace(/\\/g, "/").includes("regen-generated.mjs");
if (invoked) runDriver();
