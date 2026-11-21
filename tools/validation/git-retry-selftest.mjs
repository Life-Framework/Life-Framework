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
//      failure but not another command;
//   6. a probe's silent exit 1 (`rev-parse -q --verify` of an absent ref, `cat-file -e`, `config --get`, `show-ref
//      -q`, or any call marked `expectExit: [1]`) returns at once: one run, no backoff, no SILENT-GIT line; a
//      probe that dies another way is still re-run.
// red-proof (2026-11-19, case 6): without expectExit the probes ran three times behind a 450 ms backoff and printed
//   "SILENT-GIT lf-git: git rev-parse -q --verify failed 3 times" (the CI log of main run 19f958076).
// red-proof (2026-11-19): with the retry loop cut to one attempt this selftest FAILS on cases 1, 3 and 5 ("1 silent
//   failure(s) were not retried to a pass", "a silent failure ran 1 times, expected 3", runGit and the merge driver's
//   runner likewise); with the loop in it passes. A helper that retried every failure fails case 3 (stub ran 3, not 1).
import { existsSync, mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { GIT_ATTEMPTS, FAIL_SILENTLY_ENV, describeRun, execGit, expectedGitExits, gitSync, spawnRetrying } from "../lib/git-retry.mjs";
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
  // 6. A probe's silent "no" (exit 1) is an answer: returned at once, never re-run, never a SILENT-GIT line
  //    (main run 19f958076: `rev-parse -q --verify MERGE_HEAD` ran three times and was logged as a failure).
  inject(0);
  const probeRepo = join(root, "probe");
  mkdirSync(probeRepo);
  quietly(() => gitSync(["init", "-q"], { cwd: probeRepo, windowsHide: true, encoding: "utf8" }));
  quietly(() => gitSync(["commit", "-q", "--allow-empty", "-m", "base"], { cwd: probeRepo, windowsHide: true, encoding: "utf8" }));
  const probes = [
    ["rev-parse", "-q", "--verify", "refs/heads/lf-nope"],
    ["rev-parse", "--verify", "--quiet", "MERGE_HEAD"],
    ["cat-file", "-e", "0000000000000000000000000000000000000001"],
    ["config", "--get", "lf.nothing"],
    ["show-ref", "--verify", "-q", "refs/heads/lf-nope"],
  ];
  for (const args of probes) {
    seen.length = 0;
    const r = quietly(() => gitSync(args, { cwd: probeRepo, windowsHide: true, encoding: "utf8" }));
    const viaRunGit = quietly(() => runGit(probeRepo, args));
    check(r.status === 1 && !r.silentFailure, `probe git ${args.join(" ")} did not come back as a plain exit 1: ${describeRun(r)}`);
    check(!seen.some((l) => l.includes("SILENT-GIT")), `probe git ${args.join(" ")} printed a SILENT-GIT line: ${seen.join(" | ")}`);
    check(viaRunGit.status === 1 && !viaRunGit.silentFailure, `runGit of probe git ${args.join(" ")} did not come back as a plain exit 1: ${describeRun(viaRunGit)}`);
  }
  // The same probe through the retry counts its runs: a stub that exits 1 silently runs once under expectExit [1].
  writeFileSync(counter, "");
  const expected = quietly(() => spawnRetrying(process.execPath, [stub, "quiet"], { encoding: "utf8", windowsHide: true }, { expectExit: [1] }));
  check(expected.status === 1 && runs() === 1 && !expected.silentFailure, `expectExit [1] ran the stub ${runs()} times, expected 1`);
  // A probe that dies another way (exit 2 here) is still re-run: only the exit it uses for "no" is an answer.
  writeFileSync(counter, "");
  const stub2 = join(root, "stub2.mjs");
  writeFileSync(stub2, `import { appendFileSync } from "node:fs";
appendFileSync(${JSON.stringify(counter)}, "x");
process.exit(2);
`);
  const other = quietly(() => spawnRetrying(process.execPath, [stub2], { encoding: "utf8", windowsHide: true }, { expectExit: [1] }));
  check(other.status === 2 && runs() === GIT_ATTEMPTS, `a probe's exit 2 ran ${runs()} times, expected ${GIT_ATTEMPTS}`);
  // expectedGitExits knows the probes and leaves the real commands alone.
  check(expectedGitExits(["-C", "x", "rev-parse", "-q", "--verify", "r"]).join() === "1", "expectedGitExits missed rev-parse -q --verify behind -C");
  check(expectedGitExits(["rev-parse", "HEAD"]).length === 0 && expectedGitExits(["commit", "-q", "-m", "x"]).length === 0 && expectedGitExits(["init", "-q"]).length === 0, "expectedGitExits treated a plain command as a probe");
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
