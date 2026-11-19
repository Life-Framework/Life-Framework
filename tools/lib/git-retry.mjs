// The one place a scratch-repo git call is retried (#3616). On the Windows CI runner a git call has exited 1
// with no output and a rerun passes (PR #2563 `git checkout -b`, PR #2587 `git commit -q`, main runs d4c47e373
// and c702353fd: merge-drivers-selftest and pr-keeper-selftest). A git call that failed without a word changed
// nothing, so it is run again, up to GIT_ATTEMPTS times in all with a short backoff; a failure that printed
// something (a conflict, a bad ref) is real and comes back at once, and so does a timeout or spawn error.
// A probe that answers "no" with exit 1 and nothing printed is not a flake: `rev-parse -q --verify <ref>` of an
// absent ref, `merge-base --is-ancestor`, `diff --quiet`, `cat-file -e`, `config --get` of an unset key. That exit
// comes back at once, never re-run and never reported as SILENT-GIT (main run 19f958076: an absent MERGE_HEAD was
// run three times behind a backoff and logged as a failure). `expectedGitExits(args)` knows the git probes; any
// other call names its own with `{ expectExit: [1] }`. A probe that dies another way (128, a crash) is still re-run.
// When every attempt fails silently the result says so: exit code, signal and "no output" on stderr, plus one
// `SILENT-GIT` line on this process's stderr (the CI log).
//
// `gitSync(args, spawnOptions)` is spawnSync("git", args, spawnOptions) with the retry: a drop-in for the plain
// calls in the selftests. `spawnRetrying` is the same for any command (the merge driver's bounded runner,
// runGit's traced re-runs). No imports beyond node: the merge driver loads this inside `git merge`.
//
// Red proof: LF_GIT_FAIL_SILENTLY=<n> makes the first n attempts of EVERY call fail silently, so a selftest
// child can force one flake (a retry must pass) or more than GIT_ATTEMPTS (the message must say why).
import { spawnSync } from "node:child_process";

export const GIT_ATTEMPTS = 3;
export const FAIL_SILENTLY_ENV = "LF_GIT_FAIL_SILENTLY";
const BACKOFF_MS = 150;

// What a process did, never empty: the spawn error, the signal, the exit code and what it printed.
export function describeRun(res) {
  const parts = [];
  if (res.error) parts.push(`spawn error ${res.error.code ?? ""} ${res.error.message}`.replace(/\s+/g, " "));
  if (res.signal) parts.push(`ended by ${res.signal}`);
  parts.push(`exit ${res.status ?? "none"}`);
  const err = String(res.stderr ?? "").trim();
  const out = String(res.stdout ?? "").trim();
  if (err) parts.push(`stderr: ${err}`);
  if (out) parts.push(`stdout: ${out}`);
  if (!err && !out) parts.push("no output");
  return parts.join("; ");
}

// Exit codes that are a legitimate answer for this git command: [1] for a probe, else [].
export function expectedGitExits(args) {
  const words = [];
  for (let i = 0; i < args.length; i++) {
    if (words.length === 0 && (args[i] === "-C" || args[i] === "-c")) i++; // a global option and its value
    else if (words.length || !args[i].startsWith("-")) words.push(args[i]);
  }
  const [sub, ...rest] = words;
  const has = (...flags) => flags.some((f) => rest.includes(f));
  switch (sub) {
    case "rev-parse": return has("--verify") && has("-q", "--quiet") ? [1] : [];
    case "show-ref": return has("-q", "--quiet", "--exists") ? [1] : [];
    case "merge-base": return has("--is-ancestor") ? [1] : [];
    case "diff": case "diff-index": case "diff-files": case "diff-tree": return has("--quiet", "--exit-code") ? [1] : [];
    case "cat-file": return has("-e") ? [1] : [];
    case "symbolic-ref": return has("-q", "--quiet") ? [1] : [];
    case "config": return has("--get", "--get-all", "--get-regexp") ? [1] : [];
    case "check-ref-format": return [1];
    default: return [];
  }
}

export const nap = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);

// A failed run that captured both streams and found both empty. A run whose output was not captured (stdio
// ignore or inherit) cannot be told apart from a real failure, so it is never retried.
export function isSilentFailure(res, expectExit = []) {
  if (res.error || res.status === 0 || expectExit.includes(res.status)) return false;
  if (res.stdout == null || res.stderr == null) return false;
  return String(res.stdout).trim() === "" && String(res.stderr).trim() === "";
}

function injectedFailures(env) {
  const n = Number(env[FAIL_SILENTLY_ENV]);
  return Number.isFinite(n) && n > 0 ? Math.floor(n) : 0;
}

// Runs the command; a silent failure is re-run until it passes or `attempts` runs have failed.
//   expectExit   exit codes that are an answer, not a failure ([1] for a probe): returned at once, no re-run
//   label        names the call in the final message (default: the command and its first words)
//   optionsFor   (attempt, options) => options for attempt 2.. (runGit turns GIT_TRACE on)
//   onAttempt    (attempt, result, first) after each re-run, pass or fail (runGit prints its evidence)
export function spawnRetrying(command, args, options = {}, { attempts = GIT_ATTEMPTS, label, optionsFor, onAttempt, expectExit = [] } = {}) {
  const name = label ?? `${command} ${args.slice(0, 3).join(" ")}`.trim();
  let injected = attempts > 1 ? injectedFailures(process.env) : 0; // a call that is not retried (attempts 1) is not git: no hook
  let first = null;
  let result = null;
  for (let attempt = 1; attempt <= attempts; attempt++) {
    if (attempt > 1) nap(BACKOFF_MS * (attempt - 1));
    if (injected > 0) {
      injected--;
      result = { status: 1, signal: null, stdout: options.encoding ? "" : Buffer.alloc(0), stderr: options.encoding ? "" : Buffer.alloc(0), output: null, pid: 0 };
    } else {
      result = spawnSync(command, args, attempt > 1 && optionsFor ? optionsFor(attempt, options) : options);
    }
    if (attempt > 1) onAttempt?.(attempt, result, first);
    if (!isSilentFailure(result, expectExit)) return result;
    first ??= result;
  }
  const message = `lf-git: ${name} failed ${attempts} times (exit ${result.status ?? "none"}, signal ${result.signal ?? "none"}) with no output\n`;
  console.error(`SILENT-GIT ${message.trim()}`);
  return { ...result, stderr: options.encoding ? message : Buffer.from(message), silentFailure: true };
}

// spawnSync("git", args, options) that survives the runner's silent failures. `label` is optional.
export function gitSync(args, options = {}, extra = {}) {
  return spawnRetrying("git", args, options, { label: `git ${args.slice(0, 3).join(" ")}`, expectExit: expectedGitExits(args), ...extra });
}

// execFileSync("git", args, options) with the retry: returns stdout as a string and throws, with a message that
// is never empty, when git still fails. For the selftests that wrote `execFileSync("git", ...)` in a scratch repo.
export function execGit(args, options = {}) {
  const r = gitSync(args, { encoding: "utf8", ...options });
  if (r.error || r.status !== 0) throw new Error(`git ${args.join(" ")}: ${describeRun(r)}`);
  return r.stdout ?? "";
}
