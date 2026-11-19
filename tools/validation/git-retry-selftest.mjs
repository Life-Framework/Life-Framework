// Proves tools/lib/git-retry.mjs, the one retry under every scratch-repo git call (#3616). On the Windows CI runner
// git has exited 1 with no output and a rerun passes (merge-drivers-selftest, pr-keeper-selftest on main); the
// selftests and the merge driver now run git through gitSync / spawnRetrying.
// inputs: tools/**
//   1. a silent failure is re-run and passes: LF_GIT_FAIL_SILENTLY=1 and 2 (the first n attempts of each call fail
//      with no output) still give a real `git init`, `gitSync` and `execGit` alike;
//   2. GIT_ATTEMPTS silent failures in a row (LF_GIT_FAIL_SILENTLY=3) fail with a message that names the call, the
//      attempt count, the exit code and the signal, never an empty one, and execGit throws it;
//   3. a failure that printed something (a bad ref) is NOT retried: the stub counts its runs;
//   4. a failure whose output was not captured (stdio ignore) and a spawn error are not retried either;
//   5. runGit (selftest-git.mjs) rides the same hook, and the merge driver's runBounded retries a silent git
//      failure but not another command.
// red-proof (2026-11-19): with the retry loop cut to one attempt this selftest FAILS on cases 1, 3 and 5 ("1 silent
//   failure(s) were not retried to a pass", "a silent failure ran 1 times, expected 3", runGit and the merge driver's
//   runner likewise); with the loop in it passes. A helper that retried every failure fails case 3 (stub ran 3, not 1).
import { existsSync, mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { GIT_ATTEMPTS, FAIL_SILENTLY_ENV, describeRun, execGit, gitSync, spawnRetrying } from "../lib/git-retry.mjs";
import { runBounded } from "../merge/regen-generated.mjs";
import { removeScratch, runGit, useHermeticGit } from "../lib/selftest-git.mjs";
import { scratchDir } from "../lib/scratch.mjs";

useHermeticGit();
const failures = [];
const check = (ok, msg) => {
  if (!ok) failures.push(msg);
};
const root = scratchDir("lf-git-retry-");
const realError = console.error;
const seen = [];
const inject = (n) => {
  if (n === 0) delete process.env[FAIL_SILENTLY_ENV];
  else process.env[FAIL_SILENTLY_ENV] = String(n);
};
const quietly = (fn) => {
  console.error = (...a) => seen.push(a.join(" "));
  try {
    return fn();
  } finally {
    console.error = realError;
  }
};

try {
  check(GIT_ATTEMPTS === 3, `GIT_ATTEMPTS is ${GIT_ATTEMPTS}, the card says up to 3`);

  // 1. One or two silent failures: the call still passes.
  for (const n of [1, 2]) {
    inject(n);
    const dir = join(root, `flaky-${n}`);
    mkdirSync(dir);
    const r = quietly(() => gitSync(["init", "-q", dir], { windowsHide: true, encoding: "utf8" }));
    check(r.status === 0 && existsSync(join(dir, ".git")), `${n} silent failure(s) were not retried to a pass: ${describeRun(r)}`);
    const dir2 = join(root, `exec-${n}`);
    mkdirSync(dir2);
    try {
      quietly(() => execGit(["init", "-q", dir2]));
      check(existsSync(join(dir2, ".git")), `execGit with ${n} silent failure(s) made no repository`);
    } catch (e) {
      check(false, `execGit with ${n} silent failure(s) threw: ${e.message}`);
    }
  }

  // 2. Three in a row: the failure says why.
  inject(3);
  const dead = join(root, "dead");
  mkdirSync(dead);
  seen.length = 0;
  const failed = quietly(() => gitSync(["init", "-q", dead], { windowsHide: true, encoding: "utf8" }));
  check(failed.status === 1 && !existsSync(join(dead, ".git")), `three silent failures should fail the call: ${describeRun(failed)}`);
  check(/lf-git: git init -q .* failed 3 times \(exit 1, signal none\) with no output/.test(failed.stderr), `the final failure does not say why: "${failed.stderr}"`);
  check(seen.some((l) => l.includes("SILENT-GIT")), "the final failure printed no SILENT-GIT line to stderr");
  let thrown = "";
  try {
    quietly(() => execGit(["init", "-q", dead]));
  } catch (e) {
    thrown = e.message;
  }
  check(/failed 3 times/.test(thrown), `execGit threw "${thrown}"`);
  // Buffer results (no encoding) keep their type.
  const buf = quietly(() => gitSync(["init", "-q", dead], { windowsHide: true }));
  check(Buffer.isBuffer(buf.stderr) && /failed 3 times/.test(buf.stderr.toString()), "a call without an encoding lost its Buffer stderr");

  // 3-4. A real failure, an uncaptured one and a spawn error are not retried.
  inject(0);
  const counter = join(root, "count.txt");
  const stub = join(root, "stub.mjs");
  writeFileSync(stub, `import { appendFileSync } from "node:fs";
appendFileSync(${JSON.stringify(counter)}, "x");
if (process.argv[2] === "say") console.error("fatal: bad ref");
process.exit(1);
`);
  const runs = () => (existsSync(counter) ? readFileSync(counter, "utf8").length : 0);
  const spoke = quietly(() => spawnRetrying(process.execPath, [stub, "say"], { encoding: "utf8", windowsHide: true }));
  check(spoke.status === 1 && runs() === 1, `a failure that printed something ran ${runs()} times, expected 1`);
  writeFileSync(counter, "");
  const silent = quietly(() => spawnRetrying(process.execPath, [stub, "quiet"], { encoding: "utf8", windowsHide: true }));
  check(silent.status === 1 && runs() === GIT_ATTEMPTS, `a silent failure ran ${runs()} times, expected ${GIT_ATTEMPTS}`);
  writeFileSync(counter, "");
  quietly(() => spawnRetrying(process.execPath, [stub, "quiet"], { stdio: "ignore", windowsHide: true }));
  check(runs() === 1, `a failure with no captured output ran ${runs()} times, expected 1`);
  const missing = quietly(() => spawnRetrying("lf-no-such-command", [], { encoding: "utf8", windowsHide: true }));
  check(missing.error && missing.status !== 0, "a missing command did not fail as a spawn error");

  // 5. runGit and the merge driver ride the same retry.
  const repo = join(root, "rungit");
  mkdirSync(repo);
  inject(1);
  const viaRunGit = quietly(() => runGit(repo, ["init", "-q"]));
  check(viaRunGit.status === 0 && existsSync(join(repo, ".git")), `runGit did not retry an injected silent failure: ${describeRun(viaRunGit)}`);
  inject(3);
  const deadRunGit = quietly(() => runGit(root, ["status"]));
  check(deadRunGit.status !== 0 && /SILENT-GIT/.test(deadRunGit.stderr), `runGit's final failure carries no evidence: ${describeRun(deadRunGit)}`);
  const driverRepo = join(root, "driver");
  mkdirSync(driverRepo);
  inject(1);
  const viaDriver = quietly(() => runBounded("git init -q", "git", ["init", "-q"], { cwd: driverRepo }, 20000));
  check(viaDriver.status === 0 && existsSync(join(driverRepo, ".git")), `the merge driver's runner did not retry a silent git failure: ${describeRun(viaDriver)}`);
  writeFileSync(counter, "");
  inject(0);
  const driverStub = quietly(() => runBounded("stub", process.execPath, [stub, "quiet"], { cwd: root }, 20000));
  check(driverStub.status === 1 && runs() === 1, `the merge driver's runner retried a command that is not git (${runs()} runs)`);
} catch (e) {
  failures.push(`selftest threw: ${e.stack || e.message}`);
} finally {
  inject(0);
  console.error = realError;
  removeScratch(root);
}

if (failures.length) {
  console.log("git-retry-selftest: FAIL");
  for (const f of failures) console.log(`  - ${f}`);
  process.exit(1);
}
console.log("git-retry-selftest: OK (a silent git failure is re-run up to 3 times and says why when it never passes; a real failure is not retried; runGit and the merge driver use the same retry)");
