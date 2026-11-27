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
//   4. (#3616) The retry itself lives in tools/lib/git-retry.mjs (gitSync, execGit, spawnRetrying; the hook
//      LF_GIT_FAIL_SILENTLY=<n> forces n silent failures per call for a red proof). Every selftest and the merge
//      driver run git through it; runGit adds the trace evidence on top.
//   5. Still on the runner (PR #2587 `git commit -q`, PR #2563 `git checkout -b`: exit 1, no output, a rerun
//      passes), so git is now run through `runGit`: a failure that printed nothing is re-run at once with
//      GIT_TRACE on, the lock files and the repo state are read, and the evidence goes to stderr (the CI log);
//      a re-run that passes is accepted, since a failed git call changed nothing. A parent process's GIT_DIR,
//      GIT_INDEX_FILE and friends (set when validate runs under a hook) are removed from the environment, and
//      hooks are pinned off, so only the scratch repo decides.
import { spawnSync } from "node:child_process";
import { existsSync, mkdirSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { safeRemoveTree } from "../link-safety.mjs";
import { describeRun, expectedGitExits, nap, spawnRetrying } from "./git-retry.mjs";
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
// Windows runner under `-q`) is re-run up to `retries` times through the shared retry (tools/lib/git-retry.mjs)
// with GIT_TRACE on, the evidence printed to stderr each time; the first re-run that passes is returned, because
// a git call that failed changed nothing. A failure that did print something is returned as it is. Callers
// decide whether a nonzero status is an error.
export function runGit(cwd, args, { retries = 2, command = ["git"], expectExit } = {}) {
  const opts = { cwd, encoding: "utf8", windowsHide: true };
  const label = `git ${args.join(" ")}`;
  const traceFile = (attempt) => join(tmpdir(), `lf-git-trace-${process.pid}-${attempt - 2}.log`);
  let result = spawnRetrying(command[0], [...command.slice(1), ...args], opts, {
    attempts: retries + 1,
    label,
    expectExit: expectExit ?? (command[0] === "git" ? expectedGitExits(args) : []), // a probe's "no" is an answer
    optionsFor: (attempt, o) => ({ ...o, env: { ...process.env, GIT_TRACE: traceFile(attempt), GIT_TRACE_SETUP: traceFile(attempt) } }),
    onAttempt: (attempt, traced, first) => {
      const file = traceFile(attempt);
      traced.trace = existsSync(file) ? readFileSync(file, "utf8") : "";
      console.error(silentEvidence(cwd, `${label} (retry ${attempt - 1})`, first, traced));
    },
  });
  if (result.silentFailure) result = { ...result, stderr: silentEvidence(cwd, label, result, null) };
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

// ---- fast history (one `git fast-import` spawn instead of one git call per commit) ----------------------------
// A selftest that needs "main with merge commits for PRs 11, 12, 13" used to run init, add, commit, checkout, merge
// and push for each of them: 6 to 7 spawns each, and a git spawn costs 0.3 s on the Windows runner and the dev
// boxes. FastStream writes the same commits (same trees, same merge-commit messages) as one fast-import stream.
//
//   const fs = new FastStream();
//   fs.commit("refs/heads/main", { mark: 1, message: "base", files: { "base.txt": "base\n" } });
//   fs.commit("refs/tmp/pr11", { mark: 2, from: 1, message: "work", files: { "a.txt": "pr11\n" } });
//   fs.commit("refs/heads/main", { mark: 3, from: 1, merge: [2], message: "Merge pull request #11", files: { "a.txt": "pr11\n" } });
//   fs.reset("refs/heads/stable", 3);
//   const sha = fastImport(originDir, fs);   // sha[1], sha[2], sha[3] by mark
// A merge commit's tree starts from its first parent (`from`), so it lists the files the merged side brought in.
export class FastStream {
  constructor({ start = Math.floor(Date.now() / 1000) - 100000 } = {}) {
    this.time = start;
    this.parts = [];
  }
  data(text) {
    return `data ${Buffer.byteLength(text, "utf8")}\n${text}\n`;
  }
  commit(ref, { mark, from, merge = [], message, files = {} }) {
    this.time += 60;
    const who = `t <t@t> ${this.time} +0000`;
    let s = `commit ${ref}\n`;
    if (mark !== undefined) s += `mark :${mark}\n`;
    s += `author ${who}\ncommitter ${who}\n${this.data(message)}`;
    if (from !== undefined) s += `from ${typeof from === "number" ? `:${from}` : from}\n`;
    for (const m of merge) s += `merge ${typeof m === "number" ? `:${m}` : m}\n`;
    for (const [path, text] of Object.entries(files)) s += `M 100644 inline ${path}\n${this.data(text)}`;
    this.parts.push(`${s}\n`);
    return this;
  }
  reset(ref, from) {
    this.parts.push(`reset ${ref}\nfrom ${typeof from === "number" ? `:${from}` : from}\n\n`);
    return this;
  }
  toString() {
    return this.parts.join("");
  }
}

// Feeds a FastStream (or a string) to `git fast-import` in `repo` (a bare or a normal repo) and returns { mark: sha }.
export function fastImport(repo, stream) {
  const marksFile = join(repo, `lf-fast-import-marks-${process.pid}`);
  const r = spawnSync("git", ["fast-import", "--quiet", `--export-marks=${marksFile}`], { cwd: repo, input: String(stream), encoding: "utf8", windowsHide: true });
  if (r.error || r.status !== 0) throw new Error(`git fast-import in ${repo}: ${describeRun(r)}`);
  const sha = {};
  for (const line of readFileSync(marksFile, "utf8").split("\n")) {
    const m = /^:(\d+) ([0-9a-f]{40,64})$/.exec(line.trim());
    if (m) sha[Number(m[1])] = m[2];
  }
  try { rmSync(marksFile, { force: true }); } catch { /* the folder is removed with the scratch repo anyway */ }
  return sha;
}
