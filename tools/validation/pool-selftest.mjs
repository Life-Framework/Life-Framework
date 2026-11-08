// Proves the test pool's reclaim and re-plan rules without booting a game or
// an ssh session (tools/remote/pool.mjs, docs/worktrees.md "Test pool"):
//   1. a remote slot whose owner pid on this PC is dead is reclaimed;
//   2. a waiting ticket re-reads lf.local.json, so a re-enabled machine counts;
//   3. a slot held past the hang limit with no server alive is reclaimed,
//      logged in runs.jsonl, and the owner gets a notice.
// red-proof: with remoteOwnerGone returning null it printed "dead owner pid on
// this PC: no reclaim verdict"; with the mtime key removed from localConfig it
// printed "config flip: local still disabled after re-enable"; with the hang
// branch removed from tryLocal it printed "hung local holder: slot not reclaimed".
import { spawnSync } from "node:child_process";
import { existsSync, mkdirSync, mkdtempSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { hostname, tmpdir } from "node:os";
import { join } from "node:path";
import * as pool from "../remote/pool.mjs";
import * as tb from "../remote/testbox.mjs";

const failures = [];
const check = (ok, msg) => {
  if (!ok) failures.push(msg);
};

const deadPid = spawnSync(process.execPath, ["-e", ""]).pid;
const ago = (min) => new Date(Date.now() - min * 60000).toISOString();

// 1. dead owner of a remote slot
const remote = { name: "server-2001", pid: 4242, token: "t", rDir: "C:/LF/x", slug: "s", purpose: "test only", origin: hostname(), at: ago(1), serverAlive: true };
check(pool.remoteBusyVerdict({ ...remote, originPid: deadPid }, [])?.kind === "dead-owner", "dead owner pid on this PC: no reclaim verdict");
check(pool.remoteBusyVerdict({ ...remote, originPid: process.pid }, []) === null, "live owner pid: reclaim verdict anyway");
check(pool.remoteBusyVerdict({ ...remote, origin: "some-other-pc", originPid: deadPid }, []) === null, "owner on another PC: judged by this PC's pids");

// 3a. hang verdicts (remote)
const hung = { ...remote, originPid: process.pid, at: ago(30), serverAlive: false };
check(pool.remoteBusyVerdict(hung, [])?.kind === "hang", "hung remote holder (30 min, no server): no hang verdict");
check(pool.remoteBusyVerdict({ ...hung, serverAlive: true }, []) === null, "remote holder with a live server: hang verdict anyway");
check(pool.remoteBusyVerdict({ ...hung, serverAlive: undefined }, []) === null, "holder that did not report its server: hang verdict on a guess");
check(pool.remoteBusyVerdict({ ...hung, at: ago(3) }, []) === null, "3 min hold: under the 5 min floor but judged hung");
const slowRuns = Array.from({ length: 5 }, () => ({ purpose: "test only", result: "pass", durS: 240 }));
check(pool.hangLimitMs(slowRuns, "test only") === 2400000, "hang limit is not 10x the median run");
check(pool.remoteBusyVerdict(hung, slowRuns) === null, "30 min hold under a 40 min limit judged hung");

// The holder script still fits one ssh command line.
try {
  tb.sshPsArgs("h", pool.holderScript({ main: "C:/LF/Life-Framework", token: "0123456789abcdef", slots: [{ lock: "server-2001", dir: "C:/LF/Life-Framework-rt-x" }], dir: "C:/LF/Life-Framework-rt-x", candidates: [] }));
} catch (e) {
  failures.push(`holder script: ${e.message}`);
}

const dir = mkdtempSync(join(tmpdir(), "lf-pool-selftest-"));
try {
  spawnSync("git", ["init", "-q", dir], { windowsHide: true });

  // 2. config flip while waiting
  const cfg = (disabled) => writeFileSync(join(dir, "lf.local.json"), JSON.stringify({ pool: { machines: [{ name: "local", disabled }] } }));
  cfg(true);
  check(!pool.resolveMachines(dir, { mode: "any" }).some((m) => m.name === "local"), "config flip: local enabled while disabled");
  cfg(false);
  check(pool.resolveMachines(dir, { mode: "any" }).some((m) => m.name === "local"), "config flip: local still disabled after re-enable");

  // 3b. hung local holder: alive pid (this process), 30 min, no server
  const local = { name: "local", via: "local", slots: 1 };
  const lockDir = join(dir, "tmp", "wt", "locks");
  const lock = join(lockDir, "server-2001.lock");
  const holdLock = (minAgo) => {
    mkdirSync(lockDir, { recursive: true });
    writeFileSync(lock, JSON.stringify({ slug: "victim", purpose: "test only", root: dir, pid: process.pid, at: ago(minAgo), name: "server-2001" }));
  };
  holdLock(30);
  check(pool.tryLocal(dir, local, 1, {}, { runs: [], serverAlive: () => true }) === null, "local holder with a live server: reclaimed anyway");
  holdLock(2);
  let scanned = false;
  check(pool.tryLocal(dir, local, 1, {}, { runs: [], serverAlive: () => (scanned = true) }) === null && !scanned, "young local holder: reclaimed or scanned");
  holdLock(30);
  const s = pool.tryLocal(dir, local, 1, { slug: "next", purpose: "test only", root: dir }, { runs: [], serverAlive: () => false });
  check(!!s, "hung local holder: slot not reclaimed");
  const runsFile = join(dir, "tmp", "wt", "pool", "runs.jsonl");
  const logged = existsSync(runsFile) && readFileSync(runsFile, "utf8").includes('"kind":"hang"');
  check(logged, "hung local holder: reclaim not logged in runs.jsonl");
  const notice = join(dir, "tmp", "wt", "pool", "reclaimed", `${process.pid}-server-2001.json`);
  check(existsSync(notice), "hung local holder: no notice for its owner");
  if (s) s.release();
  check(!existsSync(notice), "owner release: notice not shown");
} finally {
  rmSync(dir, { recursive: true, force: true });
}

if (failures.length) {
  console.log("pool-selftest: FAIL");
  for (const f of failures) console.log(`  ${f}`);
  process.exit(1);
}
console.log("pool-selftest: OK (dead remote owner reclaimed, config re-read per poll, hung holder reclaimed + logged)");
