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
import { spawnSync } from "node:child_process";
import { mkdtempSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { safeRemoveTree } from "../link-safety.mjs";
import { describeRun } from "../wt.mjs";

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
];

const IDENTITY = { GIT_AUTHOR_NAME: "t", GIT_AUTHOR_EMAIL: "t@t", GIT_COMMITTER_NAME: "t", GIT_COMMITTER_EMAIL: "t@t" };

// The environment a selftest's git runs in: no user or system config file, a fixed identity and default
// branch, no signing, no auto-gc, no prompts, and no optional locks from a status that merely reads.
let emptyConfig = null;
export function hermeticGitEnv(base = process.env) {
  if (!emptyConfig) {
    emptyConfig = join(mkdtempSync(join(tmpdir(), "lf-gitcfg-")), "empty.gitconfig");
    writeFileSync(emptyConfig, "");
  }
  const empty = emptyConfig;
  const env = { ...base, ...IDENTITY, GIT_CONFIG_NOSYSTEM: "1", GIT_CONFIG_GLOBAL: empty, GIT_TERMINAL_PROMPT: "0", GIT_OPTIONAL_LOCKS: "0", GIT_CONFIG_COUNT: String(PINNED_CONFIG.length) };
  PINNED_CONFIG.forEach(([k, v], i) => {
    env[`GIT_CONFIG_KEY_${i}`] = k;
    env[`GIT_CONFIG_VALUE_${i}`] = v;
  });
  return env;
}

// Applies the hermetic environment to this process, so the code under test (wt.run, prs.mjs, pool.mjs) and
// every plain spawnSync("git") in the selftest inherit it. Returns the environment.
export function useHermeticGit() {
  const env = hermeticGitEnv();
  for (const [k, v] of Object.entries(env)) process.env[k] = v;
  return env;
}

// Runs git; a failure throws a message that is never empty.
export function gitMust(cwd, args) {
  const r = spawnSync("git", args, { cwd, encoding: "utf8", windowsHide: true });
  if (r.error || r.status !== 0) throw new Error(`git ${args.join(" ")} in ${cwd}: ${describeRun(r)}`);
  return String(r.stdout ?? "").trim();
}

const nap = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);

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
