// Git merge driver for generated files.
// Language confs resolve by tools\cli regen-localization.
// EL_TestRegistrations.generated.c resolves by tools\cli regen-tests.
// The driver runs those commands. It does not hand-edit the generated text.
//
//   node tools/merge/regen-generated.mjs %O %A %B %P

import { spawnSync } from "node:child_process";
import { existsSync, mkdirSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { mergeStringTables } from "./union-string-table.mjs";

const ST_PATH = "addons/LifeFramework/Language/everonlife_localization.st";
const TESTS_PREFIX = "addons/LifeFramework/Scripts/Game/Tests";
const REGISTRY_NAME = "EL_TestRegistrations.generated.c";

function git(args) {
  return spawnSync("git", args, { encoding: "utf8" });
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

function repoRoot() {
  const result = git(["rev-parse", "--show-toplevel"]);
  if (result.status !== 0) return process.cwd();
  return result.stdout.trim();
}

function runPowershell(root, script, args) {
  const powershell = process.platform === "win32" ? "powershell.exe" : "pwsh";
  return spawnSync(
    powershell,
    ["-NoProfile", "-ExecutionPolicy", "Bypass", "-File", join(root, script), ...args],
    { cwd: root, encoding: "utf8" },
  );
}

function snapshotWrite(root, rel, text) {
  const full = join(root, rel);
  const existed = existsSync(full);
  const previous = existed ? readFileSync(full) : null;
  if (text == null) {
    if (existed) rmSync(full);
  } else {
    mkdirSync(dirname(full), { recursive: true });
    writeFileSync(full, text);
  }
  return { full, existed, previous };
}

function restoreAll(saved) {
  for (const item of saved.reverse()) {
    if (!item.existed) {
      if (existsSync(item.full)) rmSync(item.full);
    } else {
      writeFileSync(item.full, item.previous);
    }
  }
}

function materializeStringTable(root, sides) {
  const base = showPath(sides.base, ST_PATH) ?? "";
  const ours = showPath(sides.ours, ST_PATH) ?? "";
  const theirs = showPath(sides.theirs, ST_PATH) ?? "";
  const merged = mergeStringTables(base, ours, theirs);
  if (!merged.ok) return merged;
  const saved = [snapshotWrite(root, ST_PATH, merged.text)];
  return { ok: true, saved };
}

function materializeTests(root, sides) {
  const paths = new Set([
    ...listTestSources(sides.base),
    ...listTestSources(sides.ours),
    ...listTestSources(sides.theirs),
  ]);
  const saved = [];
  for (const path of paths) {
    const picked = pickThree(showPath(sides.base, path), showPath(sides.ours, path), showPath(sides.theirs, path));
    if (!picked.ok) {
      return { ok: false, error: `lf-regen-tests: ${path} conflicts; resolve that test file, then tools\\cli regen-tests` };
    }
    saved.push(snapshotWrite(root, path, picked.text));
  }
  return { ok: true, saved };
}

function runDriver() {
  const oursPath = process.argv[3];
  const pathname = process.argv[5] || "";
  const plan = regenPlan(pathname);
  if (!oursPath || !plan) {
    console.error("lf-regen: expected a language conf or EL_TestRegistrations.generated.c");
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
  const root = repoRoot();
  const prepared = plan.kind === "tests" ? materializeTests(root, sides) : materializeStringTable(root, sides);
  if (!prepared.ok) {
    console.error(prepared.error);
    process.exit(1);
  }
  const registryRel = `${TESTS_PREFIX}/${REGISTRY_NAME}`;
  if (plan.kind === "tests") prepared.saved.push(snapshotWrite(root, registryRel, showPath(sides.ours, registryRel) ?? ""));
  try {
    const result = runPowershell(root, plan.script, plan.args);
    if (result.status !== 0) {
      if (result.stdout) process.stderr.write(result.stdout);
      if (result.stderr) process.stderr.write(result.stderr);
      process.exit(result.status || 1);
    }
    const outputRel = plan.kind === "tests" ? registryRel : pathname.replace(/\\/g, "/").replace(/^\.\//, "");
    const outputName = outputRel.split("/").pop();
    const outputPath = plan.kind === "tests"
      ? join(root, registryRel)
      : join(root, "addons/LifeFramework/Language", outputName);
    writeFileSync(oursPath, readFileSync(outputPath));
  } finally {
    restoreAll(prepared.saved);
  }
}

const invoked = process.argv[1] && process.argv[1].replace(/\\/g, "/").includes("regen-generated.mjs");
if (invoked) runDriver();
