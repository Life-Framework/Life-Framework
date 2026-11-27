// Proves the stale git lock self-heal (tools/lib/stale-git-lock.mjs): an index.lock / HEAD.lock a killed git left
// behind is removed and the call re-run, in the guard's git wrapper (tools/main-guard.mjs) and in wt.git (the warm
// pool's claim and fill); a young lock, a lock a live git could hold, an unreadable process list and any other
// lock file are left alone.
// inputs: tools/lib/stale-git-lock.mjs, tools/wt.mjs, tools/main-guard.mjs, tools/lib/main-guard.mjs
// red-proof 2026-11-27: with runGitHealing returning run() before the heal this printed "FAIL a stale index.lock is healed and
// the checkout runs again", "FAIL one log line names the lock" and "FAIL the retried checkout moved HEAD"; with the age
// check removed it printed "FAIL a lock younger than the limit is kept" (twice).
import { existsSync, mkdirSync, realpathSync, utimesSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { gitSync } from "../lib/git-retry.mjs";
import { useHermeticGit, removeScratch } from "../lib/selftest-git.mjs";
import { scratchDir } from "../lib/scratch.mjs";
import * as lock from "../lib/stale-git-lock.mjs";
import * as wt from "../wt.mjs";
import { GUARD_CODE } from "../lib/main-guard.mjs";

useHermeticGit();
let failed = 0;
const check = (c, m) => { if (!c) { console.log(`FAIL  ${m}`); failed++; } };
const base = realpathSync(scratchDir("lf-stale-lock-"));
const g = (cwd, args) => {
  const r = gitSync(["-c", "user.name=t", "-c", "user.email=t@t", ...args], { cwd, windowsHide: true, encoding: "utf8" });
  if (r.status !== 0) throw new Error(`git ${args.join(" ")}: ${r.stderr}`);
  return r.stdout.trim();
};
const MIN = 60 * 1000;
const age = (file, minutes) => { const t = new Date(Date.now() - minutes * MIN); utimesSync(file, t, t); };
const mk = (file, minutes) => { mkdirSync(join(file, ".."), { recursive: true }); writeFileSync(file, ""); age(file, minutes); return file; };

try {
  // Pure rules, in a folder that is no repo.
  const d = join(base, "pure");
  const noGit = [];
  const old = mk(join(d, "index.lock"), 20);
  check(lock.lockHolderReason(old, { procs: noGit }) === "", "an old lock with no git running is abandoned");
  const young = mk(join(d, "HEAD.lock"), 1);
  check(lock.lockHolderReason(young, { procs: noGit }) !== "", "a lock younger than the limit is kept");
  const mtime = Date.now() - 20 * MIN;
  check(lock.lockHolderReason(old, { procs: [{ pid: 1, startedMs: mtime - 5 * MIN, cmd: "git checkout" }] }) !== "", "a git started before the lock was written may hold it");
  check(lock.lockHolderReason(old, { procs: [{ pid: 1, startedMs: null, cmd: "git" }] }) !== "", "a git with an unknown start may hold it");
  check(lock.lockHolderReason(old, { procs: [{ pid: 1, startedMs: mtime + 10 * MIN, cmd: "git status" }] }) === "", "a git started after the lock was written is its victim, not its holder");
  check(lock.lockHolderReason(old, { procs: [{ pid: 1, startedMs: mtime - 60 * MIN, cmd: "git fsmonitor--daemon run" }] }) === "", "the fsmonitor daemon never holds a lock");
  check(lock.lockHolderReason(old, { procs: null }) !== "", "an unreadable process list keeps the lock");
  const ref = mk(join(d, "refs", "heads", "x.lock"), 60);
  const packed = mk(join(d, "packed-refs.lock"), 60);
  check(lock.lockHolderReason(ref, { procs: noGit }) !== "" && lock.lockHolderReason(packed, { procs: noGit }) !== "", "only index.lock and HEAD.lock are healed");
  check(lock.lockFileFromError({ stderr: "fatal: Unable to create 'C:/x/.git/worktrees/w/index.lock': File exists.\n" }).endsWith("index.lock"), "the lock path is read from git's error");
  check(lock.lockFileFromError({ stderr: "fatal: bad revision" }) === "", "another error names no lock");
  check(Array.isArray(lock.listGitProcesses() ?? []), "the real process lister answers");
  check(GUARD_CODE.includes("tools/lib/stale-git-lock.mjs"), "a change to the heal restarts a stale guard (GUARD_CODE)");

  // A real repo, a linked worktree, a lock in the worktree's admin dir (the 2026-11-27 shape).
  const origin = join(base, "origin.git");
  const main = join(base, "Life-Framework");
  g(base, ["init", "-q", "--bare", "-b", "main", origin]);
  g(base, ["clone", "-q", origin, main]);
  writeFileSync(join(main, "a.txt"), "one\n");
  g(main, ["add", "a.txt"]);
  g(main, ["commit", "-q", "-m", "one"]);
  writeFileSync(join(main, "a.txt"), "two\n");
  g(main, ["commit", "-q", "-am", "two"]);
  const sha = g(main, ["rev-parse", "HEAD~1"]);
  const tree = join(base, "Life-Framework-ws-run");
  g(main, ["worktree", "add", "-q", "--detach", tree, "HEAD"]);
  const adminDir = g(tree, ["rev-parse", "--absolute-git-dir"]);
  const indexLock = join(adminDir, "index.lock");
  const checkout = () => gitSync(["-C", tree, "checkout", "-q", "--detach", sha], { encoding: "utf8", windowsHide: true });

  mk(indexLock, 20);
  const plain = checkout();
  check(plain.status !== 0 && /index\.lock/.test(plain.stderr), `a leftover index.lock fails the checkout (scenario holds): ${plain.status} ${plain.stderr}`);

  // Young: kept, the failure comes back.
  age(indexLock, 1);
  const logs = [];
  const kept = lock.runGitHealing(checkout, { procs: noGit, log: (l) => logs.push(l) });
  check(kept.status !== 0 && existsSync(indexLock) && logs.length === 0, "a lock younger than the limit is kept");

  // A git that could hold it: kept.
  age(indexLock, 20);
  const heldRes = lock.runGitHealing(checkout, { procs: [{ pid: 7, startedMs: Date.now() - 30 * MIN, cmd: "git reset" }], log: (l) => logs.push(l) });
  check(heldRes.status !== 0 && existsSync(indexLock), "a lock a live git could hold is kept");

  // Stale: removed, logged once, the call runs again.
  const healed = lock.runGitHealing(checkout, { procs: noGit, log: (l) => logs.push(l) });
  check(healed.status === 0 && !existsSync(indexLock), "a stale index.lock is healed and the checkout runs again");
  check(logs.length === 1 && /self-heal/.test(logs[0]) && /index\.lock/.test(logs[0]), `one log line names the lock: ${JSON.stringify(logs)}`);
  check(g(tree, ["rev-parse", "HEAD"]) === sha, "the retried checkout moved HEAD");

  // wt.git (the warm pool's git) heals the same way; the pool's process list is injected.
  g(tree, ["checkout", "-q", "--detach", "main"]);
  mk(indexLock, 20);
  lock.setProcessLister(() => []);
  const viaWt = wt.git(tree, ["checkout", "-q", "--detach", sha]);
  lock.setProcessLister(null);
  check(viaWt.status === 0 && !existsSync(indexLock), "wt.git heals a stale lock in a worktree");

  // A HEAD.lock too.
  const headLock = join(adminDir, "HEAD.lock");
  mk(headLock, 20);
  const viaHead = lock.runGitHealing(() => gitSync(["-C", tree, "checkout", "-q", "--detach", "main"], { encoding: "utf8", windowsHide: true }), { procs: noGit });
  check(viaHead.status === 0 && !existsSync(headLock), "a stale HEAD.lock is healed");
} catch (e) {
  console.log(`FAIL  selftest crashed: ${e.stack ?? e.message}`);
  failed++;
} finally {
  removeScratch(base);
}

if (failed) {
  console.log(`stale-git-lock-selftest: FAIL (${failed})`);
  process.exit(1);
}
console.log("stale-git-lock-selftest: OK (age, live holder, daemon, unreadable list, other locks, guard wrapper, wt.git, HEAD.lock)");
