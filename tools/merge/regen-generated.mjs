// Git merge driver for generated files.
// Language confs resolve by tools\cli regen-localization (the driver runs its script; it does not hand-edit the generated text).
//
// The driver runs mid-merge, so it must never write into the worktree: a file that only the
// incoming side has would land as an untracked file and git would then refuse to write its own copy
// ("untracked working tree files would be overwritten by merge"). The inputs are materialised in a
// scratch git repo under os.tmpdir(), the generator runs there, and only %A is written.
//
//   node tools/merge/regen-generated.mjs %O %A %B %P

import { appendFileSync, copyFileSync, existsSync, mkdirSync, mkdtempSync, readdirSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { GIT_ATTEMPTS, expectedGitExits, spawnRetrying } from "../lib/git-retry.mjs";
import { mergeStringTables } from "./union-string-table.mjs";

// The core's string table and each pack's (#325): the driver unions every one, then regenerates all confs.
const ST_PATHS = [
  "addons/LifeFramework/Language/everonlife_localization.st",
  "addons/LF_Everon1989/Language/lf_everon1989_localization.st",
];
const TOOLS_ROOT = join(dirname(fileURLToPath(import.meta.url)), "..", "..");

// Every child runs with a time limit, no stdin and a kill signal, so a stuck git or PowerShell can never
// hang `git merge` (#2549). A timeout or spawn error throws BoundedRunError; runDriver turns it into one stderr
// line and exit 1, which leaves the file conflicted for git and lets the merge finish.
// LF_MERGE_DRIVER_TIMEOUT_MS overrides every limit (the selftest); LF_MERGE_DRIVER_GENERATOR_STUB runs
// `node <script>` in place of PowerShell (the selftest's child that never exits) and
// LF_MERGE_DRIVER_STUB_TIMEOUT_MS bounds only that stub, so the selftest can kill a hung stub in 2 s while
// the driver's own git calls keep their 20 s limit on a loaded box (#2746).
// LF_MERGE_DRIVER_TRACE=<file> appends one line per child process (`<ms> <command>`); the selftest counts them.
// A child costs about 230 ms to spawn on a loaded Windows box, so the driver's git calls are O(1), never one per file.
const GIT_TIMEOUT_MS = 20_000;
const GENERATOR_TIMEOUT_MS = 90_000;

export class BoundedRunError extends Error {}

export function timeoutFor(defaultMs, env = process.env, name = "LF_MERGE_DRIVER_TIMEOUT_MS") {
  const override = Number(env[name]);
  return Number.isFinite(override) && override > 0 ? override : defaultMs;
}

export function runBounded(step, command, args, options, defaultMs, overrideName) {
  const limit = timeoutFor(defaultMs, process.env, overrideName);
  const started = Date.now();
  // A git call that fails with no output is run again (tools/lib/git-retry.mjs: the silent failures of the CI runner,
  // main run d4c47e373 `git init` in the scratch repo left the conf conflicted with no message); a timeout is not.
  const result = spawnRetrying(command, args, {
    windowsHide: true,
    encoding: "utf8",
    maxBuffer: 256 * 1024 * 1024,
    ...options,
    stdio: [options.input === undefined ? "ignore" : "pipe", "pipe", "pipe"],
    timeout: limit,
    killSignal: "SIGKILL",
  }, { attempts: command === "git" ? GIT_ATTEMPTS : 1, label: step, expectExit: command === "git" ? expectedGitExits(args) : [] });
  if (process.env.LF_MERGE_DRIVER_TRACE) {
    try {
      appendFileSync(process.env.LF_MERGE_DRIVER_TRACE, `${Date.now() - started} ${step}\n`);
    } catch {
      // tracing is diagnostics only
    }
  }
  if (result.error) {
    const seconds = ((Date.now() - started) / 1000).toFixed(1);
    const why = result.error.code === "ETIMEDOUT" ? `timed out after ${seconds}s (limit ${Math.round(limit / 1000)}s)` : `failed after ${seconds}s: ${result.error.message}`;
    throw new BoundedRunError(`${step} ${why}`);
  }
  return result;
}

let fixCommand = "tools\\cli regen-localization";

function git(args, options = {}, limitMs = GIT_TIMEOUT_MS) {
  return runBounded(`git ${args.slice(0, 3).join(" ")}`, "git", args, options, limitMs);
}

function gitPath(rel) {
  const result = git(["rev-parse", "--git-path", rel]);
  if (result.status !== 0) return null;
  return result.stdout.trim();
}

export function regenKind(pathname) {
  const path = String(pathname ?? "").replace(/\\/g, "/");
  if (path.includes("/Language/") && path.endsWith(".conf")) return "localization";
  if (/everonlife_localization\.[A-Za-z0-9_]+\.conf$/.test(path)) return "localization";
  return null;
}

export function regenPlan(pathname) {
  const kind = regenKind(pathname);
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
  const init = git(["init", "-q"], { cwd: scratch, env });
  if (init.status !== 0) return init;
  const stub = process.env.LF_MERGE_DRIVER_GENERATOR_STUB;
  if (stub) return runBounded("generator stub", process.execPath, [stub], { cwd: scratch, env }, GENERATOR_TIMEOUT_MS, "LF_MERGE_DRIVER_STUB_TIMEOUT_MS");
  return runBounded(
    `powershell ${script}`,
    powershell,
    ["-NoProfile", "-NonInteractive", "-ExecutionPolicy", "Bypass", "-File", join(TOOLS_ROOT, script), ...args],
    { cwd: scratch, env },
    GENERATOR_TIMEOUT_MS,
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

function runDriver() {
  const oursPath = process.argv[3];
  const pathname = process.argv[5] || "";
  const plan = regenPlan(pathname);
  if (!oursPath || !plan) {
    console.error("lf-regen: expected a language conf");
    process.exit(1);
  }
  try {
    runPlan(plan, oursPath, pathname);
  } catch (error) {
    if (!(error instanceof BoundedRunError)) throw error;
    console.error(`lf-regen: ${error.message}; the file is left conflicted, run ${fixCommand} after the merge`);
    process.exitCode = 1;
  }
}

function runPlan(plan, oursPath, pathname) {
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
    const prepared = materializeStringTable(scratch, sides);
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
    const outputRel = pathname.replace(/\\/g, "/").replace(/^\.\//, "");
    writeFileSync(oursPath, readFileSync(join(scratch, outputRel)));
  } finally {
    rmSync(scratch, { recursive: true, force: true });
  }
}

const invoked = process.argv[1] && process.argv[1].replace(/\\/g, "/").includes("regen-generated.mjs");
if (invoked) runDriver();
