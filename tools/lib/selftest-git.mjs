// Shared plumbing for the selftests that build throwaway git repos (pr-keeper, prs-heal-safety, promote,
// quorum, pool, ...). Three things went wrong on the Windows CI runner, each with an empty or missing message:
//   1. Git ran with whatever global and system config the machine had (identity, default branch, gpg
//      signing, fsmonitor, auto-gc). `useHermeticGit()` pins every one of them for this process and its children.
//   2. Auto-gc / auto-maintenance, spawned by a commit or a push, outlived the git call in the scratch repo
//      and held the folder, so the selftest's own cleanup died with EBUSY (prs-heal-safety, 2026-11-16/17).
//      Auto-maintenance is off here, and `removeScratch` waits for a holder to let go, then warns instead of
//      failing a check whose assertions already ran.
//   3. A failed git call showed `stderr || stdout`, which is empty when git is ended without a word.
//      `gitMust` and `describeRun` always say the exit code, the signal and what git printed.
//   4. Still on the runner (PR #2587 `git commit -q`, PR #2563 `git checkout -b`: exit 1, no output, a rerun
//      passes), so git is now run through `runGit`: a failure that printed nothing is re-run at once with
//      GIT_TRACE on, the lock files and the repo state are read, and the evidence goes to stderr (the CI log);
//      a re-run that passes is accepted, since a failed git call changed nothing. A parent process's GIT_DIR,
//      GIT_INDEX_FILE and friends (set when validate runs under a hook) are removed from the environment, and
//      hooks are pinned off, so only the scratch repo decides.
import { spawnSync } from "node:child_process";
import { existsSync, mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { safeRemoveTree } from "../link-safety.mjs";
import { describeRun } from "../wt.mjs";
import { scratchDir } from "./scratch.mjs";

export { describeRun };

// Command-scope config (GIT_CONFIG_COUNT) beats every file, the repo's own included.
const PINNED_CONFIG = [
  ["gc.auto", "0"],
  ["maintenance.auto", "false"],
  ["receive.autogc", "false"],
  ["core.fsmonitor", "false"],
  ["core.untrackedCache", "false"],
  ["init.defaultBranch", "main"],
  ["commit.gpgsign", "false"],
  ["tag.gpgsign", "false"],
  ["core.autocrlf", "false"],
  ["advice.detachedHead", "false"],
  ["core.hooksPath", ""], // the value is an empty folder, set in hermeticGitEnv
];

// Variables a parent git (a hook, a worktree command) exports; inherited, they point a scratch repo's git at
// someone else's repository or index.
const LEAKED = [
  "GIT_DIR", "GIT_WORK_TREE", "GIT_INDEX_FILE", "GIT_OBJECT_DIRECTORY", "GIT_ALTERNATE_OBJECT_DIRECTORIES",
  "GIT_COMMON_DIR", "GIT_PREFIX", "GIT_NAMESPACE", "GIT_INDEX_VERSION", "GIT_QUARANTINE_PATH", "GIT_PUSH_OPTION_COUNT",
  "GIT_TRACE", "GIT_TRACE2", "GIT_TRACE2_EVENT", "GIT_TRACE_SETUP", "GIT_FLUSH", "GIT_TEST_FSMONITOR",
];

const IDENTITY = { GIT_AUTHOR_NAME: "t", GIT_AUTHOR_EMAIL: "t@t", GIT_COMMITTER_NAME: "t", GIT_COMMITTER_EMAIL: "t@t" };

// The environment a selftest's git runs in: no user or system config file, a fixed identity and default
// branch, no signing, no auto-gc, no prompts, and no optional locks from a status that merely reads.
let emptyConfig = null;
let noHooks = null;
export function hermeticGitEnv(base = process.env) {
  if (!emptyConfig) {
    const folder = scratchDir("lf-gitcfg-");
    emptyConfig = join(folder, "empty.gitconfig");
    writeFileSync(emptyConfig, "");
    noHooks = join(folder, "no-hooks");
    mkdirSync(noHooks);
  }
  const empty = emptyConfig;
  const env = { ...base, ...IDENTITY, GIT_CONFIG_NOSYSTEM: "1", GIT_CONFIG_GLOBAL: empty, GIT_TERMINAL_PROMPT: "0", GIT_OPTIONAL_LOCKS: "0", GIT_CONFIG_COUNT: String(PINNED_CONFIG.length) };
  PINNED_CONFIG.forEach(([k, v], i) => {
    env[`GIT_CONFIG_KEY_${i}`] = k;
    env[`GIT_CONFIG_VALUE_${i}`] = k === "core.hooksPath" ? noHooks : v;
  });
  for (const k of LEAKED) delete env[k];
  return env;
}

// Applies the hermetic environment to this process, so the code under test (wt.run, prs.mjs, pool.mjs) and
// every plain spawnSync("git") in the selftest inherit it. Returns the environment.
export function useHermeticGit() {
  const env = hermeticGitEnv();
  for (const [k, v] of Object.entries(env)) process.env[k] = v;
  for (const k of LEAKED) delete process.env[k];
  return env;
}

const nap = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
const quiet = (r) => !String(r.stderr ?? "").trim() && !String(r.stdout ?? "").trim();

// What a silent failure left behind: lock files, the repo state, and the trace of a re-run.
function silentEvidence(cwd, label, first, second) {
  const probe = (a) => {
    const r = spawnSync("git", a, { cwd, encoding: "utf8", windowsHide: true });
    return `${a.join(" ")} -> ${describeRun(r)}`;
  };
  const lines = [`SILENT-GIT ${label}: first run ${describeRun(first)}`];
  for (const lock of ["index.lock", "HEAD.lock", "config.lock", "packed-refs.lock"]) {
    const found = [join(cwd, ".git", lock), join(cwd, lock)].find((f) => existsSync(f));
    if (found) lines.push(`  lock present: ${found}`);
  }
  lines.push(`  ${probe(["status", "--short", "--branch"])}`);
  lines.push(`  ${probe(["rev-parse", "--git-dir", "--show-toplevel"])}`);
  if (second) {
    lines.push(`  traced re-run: ${describeRun(second)}`);
    const trace = String(second.trace ?? "").trim().split("\n").slice(-25).join("\n    ");
    if (trace) lines.push(`    ${trace}`);
  }
  return lines.join("\n");
}

// Runs git and returns the spawnSync result. A failure that printed nothing (exit 1 and no message, seen on the
// Windows runner under `-q`) is re-run up to twice with GIT_TRACE on, the evidence printed to stderr each time;
// the first re-run that passes is returned, because a git call that failed changed nothing. A failure that did
// print something is returned as it is. Callers decide whether a nonzero status is an error.
export function runGit(cwd, args, { retries = 2, command = ["git"] } = {}) {
  const opts = { cwd, encoding: "utf8", windowsHide: true };
  const exec = (o) => spawnSync(command[0], [...command.slice(1), ...args], o);
  let result = exec(opts);
  if (result.status === 0 || result.error || !quiet(result)) return result;
  const first = result;
  for (let i = 0; i < retries; i++) {
    nap(300 * (i + 1));
    const traceFile = join(tmpdir(), `lf-git-trace-${process.pid}-${i}.log`);
    const traced = exec({ ...opts, env: { ...process.env, GIT_TRACE: traceFile, GIT_TRACE_SETUP: traceFile } });
    traced.trace = existsSync(traceFile) ? readFileSync(traceFile, "utf8") : "";
    console.error(silentEvidence(cwd, `git ${args.join(" ")} (retry ${i + 1})`, first, traced));
    result = traced;
    if (traced.status === 0 || !quiet(traced)) break;
  }
  if (result.status !== 0 && quiet(result)) result = { ...result, stderr: silentEvidence(cwd, `git ${args.join(" ")}`, first, null) };
  return result;
}

// Runs git; a failure throws a message that is never empty and, when git was silent, carries the evidence.
export function gitMust(cwd, args) {
  const r = runGit(cwd, args);
  if (r.error || r.status !== 0) throw new Error(`git ${args.join(" ")} in ${cwd}: ${describeRun(r)}`);
  return String(r.stdout ?? "").trim();
}

// Removes a scratch folder. A process still holding it (a git that outlived its command, a scanner) is
// waited out for up to `waitMs`; if it never lets go the folder is left for the OS temp cleaner and a
// warning names it, because the checks have already run and a leftover temp folder is not a failure.
// Links are still unlinked before anything is deleted (safeRemoveTree). Returns true when it is gone.
export function removeScratch(dir, { waitMs = 20000 } = {}) {
  const until = Date.now() + waitMs;
  let last;
  for (;;) {
    try {
      safeRemoveTree(dir);
      return true;
    } catch (e) {
      last = e;
      if (e?.message?.startsWith("refusing to delete")) throw e; // a link under the folder is a bug, not a busy folder
      if (Date.now() >= until) break;
      nap(500);
    }
  }
  console.error(`WARN  selftest scratch folder left behind: ${dir} (${last?.code ?? ""} ${last?.message ?? last})`);
  return false;
}
