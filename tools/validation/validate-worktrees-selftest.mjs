// Proves validate-worktrees.ps1 survives other agents pruning worktrees while it scans (`lead prune-safe`,
// `wt prune`): a worktree that vanished mid-scan is skipped (it was an ERROR "registered at missing path",
// plus "fatal: not a git repository" spam for half-removed folders), and a transient packed-refs read error
// ("couldn't read .git/packed-refs: Permission denied") is retried once instead of failing the check.
// A `git` shim first on PATH injects both: on the first `for-each-ref` it removes the `gone` slug from
// state.json (the prune) and fails with the packed-refs error (the transient read).
// inputs: tools/validation/validate-worktrees.ps1, tools/validation/validate-worktrees-selftest.mjs
// red-proof: 2026-11-15, with the retry removed from Invoke-Git "a transient packed-refs error is retried once" failed (1 call);
// with Test-StillRegistered always true "a worktree pruned mid-scan is not an error" failed; both restored.
// red-proof: 2026-11-27 (git budget), without the budget check an exhausted budget ran the status call:
// "an exhausted git budget runs no status" failed.

import { spawnSync } from "node:child_process";
import { gitSync } from "../lib/git-retry.mjs";
import { existsSync, mkdirSync, readFileSync, utimesSync, writeFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { safeRemoveTree } from "../link-safety.mjs";
import { scratchDir } from "../lib/scratch.mjs";
import { hermeticGitEnv, useHermeticGit } from "../lib/selftest-git.mjs";

const failures = [];
const check = (ok, name) => {
  if (!ok) failures.push(name);
};

if (process.platform !== "win32") {
  console.log("validate-worktrees-selftest: OK (skipped: the check is PowerShell and runs on Windows)");
  process.exit(0);
}
useHermeticGit();

const ps1 = resolve(dirname(new URL(import.meta.url).pathname.replace(/^\/([A-Za-z]:)/, "$1")), "validate-worktrees.ps1");
const realGit = spawnSync("where", ["git"], { encoding: "utf8", windowsHide: true }).stdout.split(/\r?\n/).find((l) => /git\.exe$/i.test(l.trim()))?.trim();

const base = scratchDir("lf-vwt-");
try {
  const fx = join(base, "main");
  mkdirSync(fx);
  gitSync(["init", "-q", fx], { windowsHide: true });
  const hub = join(fx, "tmp", "wt");
  mkdirSync(hub, { recursive: true });
  const ghost = join(base, "ghost"); // a half-removed worktree: the folder is there, its .git is not
  mkdirSync(join(ghost, "node_modules"), { recursive: true });
  const state = {
    worktrees: {
      gone: { path: join(base, "gone"), branch: "ws/gone", ports: { gamePort: 1, a2sPort: 2 }, status: "active" },
      ghost: { path: ghost, branch: "ws/ghost", ports: { gamePort: 3, a2sPort: 4 }, status: "active" },
    },
  };
  const stateFile = join(hub, "state.json");
  const writeState = () => writeFileSync(stateFile, JSON.stringify(state));

  const shimDir = join(base, "shim");
  mkdirSync(shimDir);
  writeFileSync(join(shimDir, "git.cmd"), '@node "%~dp0git-shim.mjs" %*\r\n');
  writeFileSync(
    join(shimDir, "git-shim.mjs"),
    `import { spawnSync } from "node:child_process";
import { appendFileSync, existsSync, readFileSync, writeFileSync } from "node:fs";
const args = process.argv.slice(2);
const log = process.env.LF_SHIM_LOG;
if (log) appendFileSync(log, args.join(" ") + "\\n");
const flag = process.env.LF_SHIM_FLAG;
if (flag && args.includes("for-each-ref") && !existsSync(flag)) {
  writeFileSync(flag, "1");
  const s = JSON.parse(readFileSync(process.env.LF_SHIM_STATE, "utf8"));
  delete s.worktrees.gone; // the prune lands between the scan's first read and its loop
  writeFileSync(process.env.LF_SHIM_STATE, JSON.stringify(s));
  process.stderr.write("fatal: couldn't read " + process.cwd() + "/.git/packed-refs: Permission denied\\n");
  process.exit(128);
}
const r = spawnSync(process.env.LF_REAL_GIT, args, { stdio: "inherit", windowsHide: true });
process.exit(r.status ?? 1);
`,
  );

  const run = (withShim, tag) => {
    writeState();
    const log = join(base, `git-${tag}.log`);
    const flag = join(base, `flag-${tag}`);
    const env = { ...process.env, LF_SHIM_LOG: log, LF_SHIM_STATE: stateFile, LF_REAL_GIT: realGit };
    if (withShim) {
      env.PATH = `${shimDir};${process.env.PATH ?? process.env.Path}`;
      env.LF_SHIM_FLAG = flag;
    }
    const r = spawnSync("powershell.exe", ["-NoProfile", "-NonInteractive", "-ExecutionPolicy", "Bypass", "-File", ps1], { cwd: fx, env, encoding: "utf8", windowsHide: true, timeout: 120000 });
    const calls = existsSync(log) ? readFileSync(log, "utf8").split(/\r?\n/).filter(Boolean) : [];
    return { code: r.status, out: `${r.stdout}${r.stderr}`, calls };
  };

  // control: no prune, no transient error: the missing path is still a finding, the ghost folder a warning
  const c = run(false, "control");
  check(c.code === 1 && /registered at missing path/.test(c.out), `control: a registered missing path still fails the check (exit ${c.code}: ${c.out.slice(0, 200)})`);
  check(/WARN\s+worktree 'ghost' .* is not a git worktree/.test(c.out), "a registered folder with no .git is a warning, not a crash");
  check(!/fatal: not a git repository/.test(c.out), "no 'fatal: not a git repository' reaches the output");

  const s = run(true, "shim");
  check(s.calls.filter((l) => l.includes("for-each-ref")).length >= 2, `a transient packed-refs error is retried once (for-each-ref calls: ${s.calls.filter((l) => l.includes("for-each-ref")).length})`);
  check(!/ERROR .*gone/.test(s.out), "a worktree pruned mid-scan is not an error");
  check(s.code === 0 && /validate-worktrees: OK/.test(s.out), `the check passes (exit ${s.code}: ${s.out.slice(0, 300)})`);
  check(!/packed-refs/.test(s.out), "the transient error is not printed");
} finally {
  safeRemoveTree(base);
}

// #3126: only the current worktree and the ones with a change in the last 24 h get git reads. A stale worktree is
// counted in one INFO line; a clean merged status is cached per worktree, so a second run runs no status for it.
const live = scratchDir("lf-vwt-live-");
try {
  const fx = join(live, "main");
  const wtPath = (slug) => join(live, `wt-${slug}`);
  const git = (cwd, args) => {
    const r = gitSync(args, { cwd, env: hermeticGitEnv(), encoding: "utf8", windowsHide: true });
    if (r.status !== 0) throw new Error(`git ${args.join(" ")} failed (${r.status}): ${r.stderr}`);
    return r.stdout;
  };
  mkdirSync(fx);
  git(fx, ["init", "-q", "."]);
  writeFileSync(join(fx, "README.md"), "x\n");
  writeFileSync(join(fx, ".gitignore"), "tmp/\n"); // a real worktree ignores its tmp/ (the wt marker lives there)
  git(fx, ["add", "-A"]);
  git(fx, ["commit", "-qm", "init"]);
  git(fx, ["update-ref", "refs/remotes/origin/main", "HEAD"]);
  mkdirSync(join(fx, "tmp", "wt"), { recursive: true });

  const slugs = ["curx", "stalex", "freshx", "cleanx"];
  const worktrees = {};
  slugs.forEach((slug, i) => {
    git(fx, ["worktree", "add", "-q", "-b", `ws/${slug}`, wtPath(slug), "main"]);
    mkdirSync(join(wtPath(slug), "tmp"), { recursive: true });
    writeFileSync(join(wtPath(slug), "tmp", "wt.json"), "{}");
    worktrees[slug] = { path: wtPath(slug), branch: `ws/${slug}`, ports: { gamePort: 9101 + 2 * i, a2sPort: 9102 + 2 * i }, status: "active" };
  });
  // curx is the current worktree and has a commit of its own, so it is not merged
  writeFileSync(join(wtPath("curx"), "curx.txt"), "c\n");
  git(wtPath("curx"), ["add", "-A"]);
  git(wtPath("curx"), ["commit", "-qm", "work"]);
  // freshx and stalex are merged into origin/main and dirty (an untracked file)
  for (const slug of ["freshx", "stalex"]) writeFileSync(join(wtPath(slug), "dirty.txt"), "d\n");
  // stalex: its index and HEAD were last written 48 h ago
  const old = new Date(Date.now() - 48 * 3600 * 1000);
  const adminDir = join(fx, ".git", "worktrees", "wt-stalex");
  for (const f of ["index", "HEAD"]) utimesSync(join(adminDir, f), old, old);
  writeFileSync(join(fx, "tmp", "wt", "state.json"), JSON.stringify({ worktrees }));

  const shimLive = join(live, "shim");
  mkdirSync(shimLive);
  writeFileSync(join(shimLive, "git.cmd"), '@node "%~dp0git-shim.mjs" %*\r\n');
  writeFileSync(
    join(shimLive, "git-shim.mjs"),
    `import { spawnSync } from "node:child_process";
import { appendFileSync } from "node:fs";
const args = process.argv.slice(2);
appendFileSync(process.env.LF_SHIM_LOG, args.join(" ") + "\\n");
const r = spawnSync(process.env.LF_REAL_GIT, args, { stdio: "inherit", windowsHide: true });
process.exit(r.status ?? 1);
`,
  );

  const runLive = (tag, extraEnv = {}) => {
    const log = join(live, `git-${tag}.log`);
    const env = { ...hermeticGitEnv(), ...extraEnv };
    const pathValue = `${shimLive};${process.env.PATH ?? process.env.Path}`;
    env.PATH = pathValue;
    env.Path = pathValue;
    env.LF_SHIM_LOG = log;
    env.LF_REAL_GIT = realGit;
    const r = spawnSync("powershell.exe", ["-NoProfile", "-NonInteractive", "-ExecutionPolicy", "Bypass", "-File", ps1], { cwd: wtPath("curx"), env, encoding: "utf8", windowsHide: true, timeout: 120000 });
    const calls = existsSync(log) ? readFileSync(log, "utf8").split(/\r?\n/).filter(Boolean) : [];
    return { code: r.status, out: `${r.stdout}${r.stderr}`, calls };
  };
  const statusCalls = (r) => r.calls.filter((l) => l.includes(" status --porcelain"));

  const r1 = runLive("one");
  const r2 = runLive("two");
  check(r1.code === 0 && r2.code === 0, `the live check passes on both runs (exit ${r1.code}, ${r2.code}: ${r1.out.slice(0, 300)})`);
  check(!/stalex/.test(r1.out), "a worktree with no change in 24 h gets no warning (it is not checked)");
  check(statusCalls(r1).every((l) => !l.includes("wt-stalex")), "a stale worktree gets no git status");
  check(/INFO\s+1 worktree\(s\) with no change in 24 h not checked/.test(r1.out), "the stale worktree is counted in one INFO line");
  // Merged-by-ancestry is one INFO count, never a WARN per worktree (a fresh worktree is an ancestor too).
  check(!/WARN\s+worktree '(freshx|cleanx)'/.test(r1.out), "a merged-by-ancestry worktree gets no WARN of its own");
  check(/INFO\s+worktree hygiene: 1 clean and 1 edited worktree\(s\)/.test(r1.out), `one INFO line counts the clean and the edited ones (${r1.out.slice(0, 400)})`);
  check(statusCalls(r1).length === 2, `the first run reads status for the two fresh merged worktrees (calls: ${statusCalls(r1).join(" | ")})`);
  check(statusCalls(r2).length === 1 && statusCalls(r2)[0].includes("wt-freshx"), `a second run serves the clean worktree from the cache (calls: ${statusCalls(r2).join(" | ")})`);
  check(/INFO\s+worktree hygiene: 1 clean and 1 edited/.test(r2.out), "a cached clean worktree is still counted");
  // The git budget (main guard 2026-11-24: three 600 s timeouts with hundreds of merged worktrees): with no seconds
  // left, a merged worktree not in the cache gets no status call and is counted in one INFO line; the check still passes.
  const r3 = runLive("three", { LF_VALIDATE_WORKTREES_GIT_BUDGET_S: "0" });
  check(r3.code === 0, `an exhausted git budget still passes (exit ${r3.code}: ${r3.out.slice(0, 300)})`);
  check(statusCalls(r3).length === 0, `an exhausted git budget runs no status (calls: ${statusCalls(r3).join(" | ")})`);
  check(/INFO\s+1 merged worktree\(s\) not status-checked/.test(r3.out), `the skipped worktree is counted in one INFO line (${r3.out.slice(0, 300)})`);
  check(!/freshx/.test(r3.out), "a worktree skipped for budget gets no warning");
} finally {
  safeRemoveTree(live);
}

if (failures.length) {
  console.log(`validate-worktrees-selftest: FAILED\n  ${failures.join("\n  ")}`);
  process.exit(1);
}
console.log("validate-worktrees-selftest: OK (vanished worktree skipped, packed-refs retried once, ghost folder warned)");
