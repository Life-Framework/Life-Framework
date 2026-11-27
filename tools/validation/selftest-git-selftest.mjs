// Proves tools/lib/selftest-git.mjs, the plumbing under the selftests that build throwaway git repos (the CI
// flakes: pr-keeper, prs-heal-safety, pool and promote failed on the Windows runner, some with an empty message).
// inputs: tools/**
//   1. hermetic git: with a user config that signs every commit, renames the default branch and sets no identity,
//      a plain git commit fails and the hermetic one succeeds on branch main (the machine's config cannot decide);
//   2. a scratch folder another process holds (a git that outlived its command, the runner's scanner) is waited
//      out by removeScratch, where safeRemoveTree alone throws EBUSY; one that never lets go warns and returns
//      false instead of failing the check (Windows only: elsewhere a held folder deletes fine);
//   3. describeRun says exit code, signal and output for every way a process can end, so a message is never empty;
//   4. the validate runner's note for a failing check that printed nothing.
//   5. runGit: a silent failure (rev-parse -q of a missing ref) comes back with SILENT-GIT evidence and a traced re-run;
//      one that is silent only once (a flaky script) is re-run to a pass; a leaked GIT_DIR / GIT_INDEX_FILE cannot
//      steer a scratch repo (red: the leak child fails without the LEAKED scrub, the flaky script fails without the re-run).
//   6. FastStream + fastImport (2026-11-27): a base, a side commit and a merge commit built by one git process have the right parents,
//      subject, tree and refs, and the marks come back as shas. red: with the merge parents dropped, "the merge commit's parents" failed.
// red-proof (2026-11-17): under the hostile config the plain git child's commit exits nonzero (the gpg program is
//   missing) where the hermetic child's exits 0 on main; safeRemoveTree on the held folder throws EBUSY where
//   removeScratch removes it once the holder exits. Both are asserted above, so each fixture is its own red.
import { spawn, spawnSync } from "node:child_process";
import { existsSync, mkdirSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { pathToFileURL } from "node:url";
import { describeRun, FastStream, fastImport, gitMust as must, removeScratch, runGit } from "../lib/selftest-git.mjs";
import { silentFailureNote } from "../lib/silent-failure.mjs";
import { safeRemoveTree } from "../link-safety.mjs";
import * as wt from "../wt.mjs";
import { scratchDir } from "../lib/scratch.mjs";

const failures = [];
const check = (ok, msg) => {
  if (!ok) failures.push(msg);
};
const helperUrl = pathToFileURL(join(import.meta.dirname, "..", "lib", "selftest-git.mjs")).href;
const root = scratchDir("lf-selftest-git-");

try {
  // 1. A hostile machine config.
  const home = join(root, "home");
  mkdirSync(home);
  writeFileSync(join(home, ".gitconfig"), "[init]\n\tdefaultBranch = trunk\n[commit]\n\tgpgsign = true\n[gpg]\n\tprogram = lf-no-such-gpg\n");
  const hostile = { ...process.env, HOME: home, USERPROFILE: home, XDG_CONFIG_HOME: home, GIT_CONFIG_NOSYSTEM: "1", GIT_CONFIG_GLOBAL: join(home, ".gitconfig") };
  for (const k of ["GIT_AUTHOR_NAME", "GIT_AUTHOR_EMAIL", "GIT_COMMITTER_NAME", "GIT_COMMITTER_EMAIL", "GIT_CONFIG_COUNT"]) delete hostile[k];
  const child = (hermetic, repo) => {
    mkdirSync(repo);
    const code = [
      `import { spawnSync } from "node:child_process";`,
      hermetic ? `const { useHermeticGit } = await import(${JSON.stringify(helperUrl)}); useHermeticGit();` : "",
      `const g = (...a) => spawnSync("git", a, { cwd: ${JSON.stringify(repo)}, encoding: "utf8", windowsHide: true });`,
      `const init = g("init", "-q");`,
      `const commit = g("commit", "-q", "--allow-empty", "-m", "x");`,
      `const branch = g("symbolic-ref", "--short", "HEAD").stdout.trim();`,
      `console.log(JSON.stringify({ init: init.status, commit: commit.status, branch }));`,
    ].join("\n");
    const r = spawnSync(process.execPath, ["--input-type=module", "-e", code], { encoding: "utf8", windowsHide: true, env: hostile });
    try {
      return JSON.parse(r.stdout.trim().split("\n").at(-1));
    } catch {
      return { error: describeRun(r) };
    }
  };
  const plain = child(false, join(root, "plain"));
  check(plain.commit !== 0, `plain git unexpectedly committed under the hostile config (${JSON.stringify(plain)}); the fixture proves nothing`);
  const pinned = child(true, join(root, "pinned"));
  check(pinned.commit === 0 && pinned.branch === "main", `hermetic git did not commit on main under the hostile config: ${JSON.stringify(pinned)}`);

  // 1b. A parent process's GIT_DIR / GIT_INDEX_FILE must not steer a scratch repo (validate runs under hooks).
  {
    const repo = join(root, "leak");
    mkdirSync(repo);
    const code = [
      `import { spawnSync } from "node:child_process";`,
      `const { useHermeticGit } = await import(${JSON.stringify(helperUrl)}); useHermeticGit();`,
      `const g = (...a) => spawnSync("git", a, { cwd: ${JSON.stringify(repo)}, encoding: "utf8", windowsHide: true });`,
      `g("init", "-q");`,
      `const c = g("commit", "-q", "--allow-empty", "-m", "x");`,
      `console.log(JSON.stringify({ commit: c.status, top: g("rev-parse", "--show-toplevel").stdout.trim() }));`,
    ].join("\n");
    const leaky = { ...process.env, GIT_DIR: join(root, "elsewhere.git"), GIT_INDEX_FILE: join(root, "elsewhere.index"), GIT_WORK_TREE: root };
    const r = spawnSync(process.execPath, ["--input-type=module", "-e", code], { encoding: "utf8", windowsHide: true, env: leaky });
    let out;
    try {
      out = JSON.parse(r.stdout.trim().split("\n").at(-1));
    } catch {
      out = { error: describeRun(r) };
    }
    check(out.commit === 0 && String(out.top).replaceAll("\\", "/").endsWith("/leak"), `leaked GIT_DIR/GIT_INDEX_FILE steered the scratch repo: ${JSON.stringify(out)}`);
  }

  // 1c. runGit: a real silent failure (rev-parse -q --verify of a missing ref) is reported with evidence, and a
  // failure that is silent only the first time (a flake) is re-run and passes.
  {
    const repo = join(root, "silent");
    mkdirSync(repo);
    spawnSync("git", ["init", "-q"], { cwd: repo, windowsHide: true });
    spawnSync("git", ["commit", "-q", "--allow-empty", "-m", "base"], { cwd: repo, windowsHide: true });
    const realError = console.error;
    const seen = [];
    console.error = (...a) => seen.push(a.join(" "));
    let real, flaky;
    try {
      // (a probe's silent exit 1, such as `rev-parse -q --verify` of a missing ref, is an answer since #3616's
      // expectExit; the stub stands in for a call that must have printed something)
      const dead = join(root, "dead.mjs");
      writeFileSync(dead, "process.exit(1);\n");
      real = runGit(repo, [], { retries: 1, command: [process.execPath, dead] });
      const probe = runGit(repo, ["rev-parse", "-q", "--verify", "refs/heads/lf-nope"], { retries: 1 });
      check(probe.status === 1 && !probe.silentFailure, `a probe's silent exit 1 was reported as a failure: ${describeRun(probe)}`);
      const flag = join(root, "flaky.flag");
      const script = join(root, "flaky.mjs");
      writeFileSync(script, `import { existsSync, writeFileSync } from "node:fs";
if (!existsSync(${JSON.stringify(flag)})) { writeFileSync(${JSON.stringify(flag)}, "x"); process.exit(1); }
console.log("ok");
`);
      flaky = runGit(repo, [], { command: [process.execPath, script] });
    } finally {
      console.error = realError;
    }
    check(real.status !== 0 && String(real.stderr).includes("SILENT-GIT"), `a silent git failure carries no evidence: ${describeRun(real)}`);
    check(seen.some((l) => l.includes("SILENT-GIT") && l.includes("traced re-run")), "the silent failure printed no traced re-run to stderr");
    check(flaky.status === 0 && String(flaky.stdout).includes("ok"), `runGit did not re-run a silent first failure: ${describeRun(flaky)}`);
  }

  // 2. A held scratch folder (a process whose working directory is inside it holds it on Windows).
  if (process.platform === "win32") {
    // The holder prints once it runs: its working directory is only held after the process has started up.
    const hold = async (dir, ms) => {
      const c = spawn(process.execPath, ["-e", `console.log("up"); setTimeout(() => {}, ${ms})`], { cwd: dir, stdio: ["ignore", "pipe", "ignore"], windowsHide: true });
      await new Promise((res, rej) => (c.stdout.once("data", res), c.once("error", rej)));
      return c;
    };
    const heldDir = join(root, "held");
    mkdirSync(heldDir);
    const holder = await hold(heldDir, 4500);
    let threw = "";
    try {
      safeRemoveTree(heldDir); // the retries (8 x 250 ms) end before the holder does
    } catch (e) {
      threw = e.code ?? e.message;
    }
    check(threw !== "", "safeRemoveTree removed a folder a process holds; the fixture proves nothing about the wait");
    const gone = removeScratch(heldDir, { waitMs: 20000 });
    check(gone && !existsSync(heldDir), "removeScratch did not wait out the holder");
    holder.kill();

    const stuckDir = join(root, "stuck");
    mkdirSync(stuckDir);
    const stuck = await hold(stuckDir, 60000);
    const realError = console.error;
    const warned = [];
    console.error = (...a) => warned.push(a.join(" "));
    let result;
    try {
      result = removeScratch(stuckDir, { waitMs: 1500 });
    } finally {
      console.error = realError;
    }
    check(result === false && warned.some((w) => w.includes(stuckDir)), `a folder that never frees must warn with its path and return false, not throw (result ${result}; warned ${JSON.stringify(warned)})`);
    stuck.kill();
  }

  // 3. describeRun, for every way a run ends.
  const silent = describeRun({ status: 1, stdout: "", stderr: "" });
  check(/exit 1/.test(silent) && /no output/.test(silent), `describeRun of a silent failure: "${silent}"`);
  const killed = describeRun({ status: null, signal: "SIGKILL", stdout: "", stderr: "" });
  check(/SIGKILL/.test(killed) && /exit none/.test(killed), `describeRun of a signal: "${killed}"`);
  const spawnFail = describeRun(spawnSync("lf-no-such-binary-for-selftest", [], { encoding: "utf8", windowsHide: true }));
  check(/ENOENT/.test(spawnFail), `describeRun of a missing binary: "${spawnFail}"`);
  const said = describeRun({ status: 128, stdout: "out line", stderr: "fatal: bad" });
  check(said.includes("exit 128") && said.includes("stderr: fatal: bad") && said.includes("stdout: out line"), `describeRun keeps both streams: "${said}"`);
  const viaRun = wt.run(process.execPath, ["-e", "process.exit(7)"]);
  check(viaRun.status === 7 && /exit 7/.test(wt.describeRun(viaRun)), `wt.run + describeRun of exit 7: "${wt.describeRun(viaRun)}"`);

  // 4. The validate runner's note.
  check(silentFailureNote("x.mjs", { status: 1, stdout: "", stderr: " \n" }).includes("printed nothing and ended with exit code 1"), "silent failure gets no note");
  check(silentFailureNote("x.mjs", { status: 1, signal: "SIGTERM", stdout: "", stderr: "" }).includes("signal SIGTERM"), "a signal is not in the note");
  check(silentFailureNote("x.mjs", { status: 124, timedOut: true, stdout: "", stderr: "" }).includes("time budget"), "a timeout is not in the note");
  check(silentFailureNote("x.mjs", { status: 1, stdout: "FAIL thing", stderr: "" }) === "", "a failure that explained itself got a note");
  check(silentFailureNote("x.mjs", { status: 0, stdout: "", stderr: "" }) === "", "a pass got a note");

  // 6. FastStream + fastImport: a base, a side commit and a --no-ff style merge commit in ONE git process, with the
  //    merge's two parents, its subject, its tree, and the marks returned as shas (promote and prs-heal-safety build on it).
  {
    const repo = join(root, "fast.git");
    must(root, ["init", "-q", "--bare", "-b", "main", repo]);
    const fs = new FastStream();
    fs.commit("refs/heads/main", { mark: 1, message: "base", files: { "base.txt": "base\n" } });
    fs.commit("refs/tmp/pr7", { mark: 2, from: 1, message: "work", files: { "a.txt": "pr7\n" } });
    fs.commit("refs/heads/main", { mark: 3, from: 1, merge: [2], message: "Merge pull request #7 from x/ws/pr7", files: { "a.txt": "pr7\n" } });
    fs.reset("refs/heads/stable", 1);
    const sha = fastImport(repo, fs);
    check(Object.keys(sha).length === 3 && /^[0-9a-f]{40,64}$/.test(sha[3] ?? ""), `fastImport marks: ${JSON.stringify(sha)}`);
    check(must(repo, ["rev-parse", "main"]) === sha[3] && must(repo, ["rev-parse", "stable"]) === sha[1], "fastImport refs do not point at the marked commits");
    check(must(repo, ["rev-list", "--parents", "-n", "1", "main"]).split(" ").slice(1).join() === [sha[1], sha[2]].join(), "the merge commit's parents are base then the side commit");
    check(must(repo, ["log", "-1", "--format=%s", "main"]) === "Merge pull request #7 from x/ws/pr7", "the merge commit's subject");
    check(must(repo, ["ls-tree", "--name-only", "main"]).split("\n").join() === "a.txt,base.txt", "the merge commit's tree is base plus the merged file");
    check(must(repo, ["log", "--first-parent", "--format=%s", "main"]).split("\n").join("|") === "Merge pull request #7 from x/ws/pr7|base", "first-parent history is the merge, then base");
  }
} catch (e) {
  failures.push(`selftest threw: ${e.stack || e.message}`);
} finally {
  removeScratch(root);
}

if (failures.length) {
  console.log("selftest-git-selftest: FAIL");
  for (const f of failures) console.log(`  - ${f}`);
  process.exit(1);
}
console.log("selftest-git-selftest: OK (hostile git config cannot decide a selftest's commit or branch; a held scratch folder is waited out, a stuck one warns; describeRun and the validate note are never empty)");
