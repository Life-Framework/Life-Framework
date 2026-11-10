// Proves the test pool's reclaim and re-plan rules without booting a game or
// an ssh session (tools/remote/pool.mjs, docs/worktrees.md "Test pool"):
//   1. a remote slot whose owner pid on this PC is dead is reclaimed;
//   2. a waiting ticket re-reads lf.local.json, so a re-enabled machine counts;
//   3. a slot held past the hang limit with no server alive is reclaimed,
//      logged in runs.jsonl, and the owner gets a notice;
//   4. lanes (test lanes on 2002.., session lane on 2001), priority order and
//      aging through the real queue, the RAM gate, the self-deadlock fail-fast,
//      the idle-session reaper (also end to end with a short timeout);
//   5. runtime budgets: learned 3x p95 with a 30 s floor, seeded defaults,
//      machine multiplier, self-raise after timeouts, --budget.
// red-proof: with remoteOwnerGone returning null it printed "dead owner pid on
// this PC: no reclaim verdict"; with the mtime key removed from localConfig it
// printed "config flip: local still disabled after re-enable"; with the hang
// branch removed from tryLocal it printed "hung local holder: slot not reclaimed".
// red-proof (lanes): with the priority comparison disabled it printed "a queued
// low job was not overtaken by a normal one"; with promotion disabled "a low job
// waiting past the promote limit was not promoted"; without the queue check in
// reapIdleSessions "reaper closed a session nobody waits for"; with selfDeadlock
// returning null "own session on the only slot: no fail-fast"; without the 30 s
// floor "3x p95 of 9 s runs is under the 30 s floor but was not floored"; with the
// RAM verdict always ok "RAM gate: a second server started with 0.9 GB free".
import { spawn, spawnSync } from "node:child_process";
import { existsSync, mkdirSync, mkdtempSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { hostname, tmpdir } from "node:os";
import { join } from "node:path";
import * as q from "../lib/pool-queue.mjs";
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

  // 4. lanes, priority order, aging, RAM gate, self-deadlock, lease reaper.
  const lanes = { name: "local", via: "local", slots: 1, lanes: { test: 2, session: 1 } };
  check(q.lanesFor(lanes, "test").map((l) => `${l.lock}:${l.port}`).join() === "server-test-1:2002,server-test-2:2003", "test lanes are not server-test-k on 2002..");
  check(q.lanesFor(lanes, "session").map((l) => `${l.lock}:${l.port}`).join() === "server-2001:2001", "session lane is not server-2001 on 2001");
  check(q.lanesFor({ name: "x", via: "ssh", slots: 2 }, "test").map((l) => l.lock).join() === "server-2001,server-2001-s2", "a machine without lanes lost its shared slots");
  check(q.portForLock("server-test-2") === 2003 && q.portForLock("server-2001") === 2001, "portForLock");
  const T0 = Date.parse("2026-01-01T00:00:00Z");
  const tk = (file, priority, minAgo, lane = "test") => ({ file, priority, lane, at: new Date(T0 - minAgo * 60000).toISOString(), machines: ["local"] });
  const min20 = 20 * 60000;
  const lowOld = tk("1-low", "low", 5);
  const normalNew = tk("2-normal", "normal", 1);
  check(q.queueOrder([lowOld, normalNew], T0, min20).map((t) => t.file).join() === "2-normal,1-low", "a queued low job was not overtaken by a normal one");
  check(q.queueOrder([normalNew, tk("3-high", "high", 0)], T0, min20)[0].file === "3-high", "high does not go first");
  check(q.queueOrder([tk("b", "normal", 2), tk("a", "normal", 3)], T0, min20)[0].file === "a", "equal priority is not FIFO");
  const lowAged = tk("1-low", "low", 25);
  check(q.queueOrder([lowAged, normalNew], T0, min20).map((t) => t.file).join() === "1-low,2-normal", "a low job waiting past the promote limit was not promoted");
  check(q.effectiveRank(tk("x", "low", 45), T0, min20) === 2 && q.effectiveRank(tk("x", "high", 99), T0, min20) === 2, "promotion is not one level per period, capped at high");
  check(q.blocks(tk("s", "normal", 9, "session"), tk("t", "normal", 1, "test"), lanes, T0, min20) === false, "a session ticket blocks a test on separate lanes");
  check(q.blocks(tk("s", "normal", 9, "session"), tk("t", "normal", 1, "test"), { ...lanes, lanes: null }, T0, min20) === true, "a session ticket does not block a test on a shared lane");
  check(q.blocks({ ...tk("old", "normal", 9), lane: undefined }, tk("z", "normal", 1, "session"), lanes, T0, min20) === true, "a ticket from a CLI without lanes is not a server-2001 contender");
  check(q.ramVerdict({ running: 0, freeMb: 100, minFreeMb: 1500 }).ok && !q.ramVerdict({ running: 1, freeMb: 1000, minFreeMb: 1500 }).ok && q.ramVerdict({ running: 1, freeMb: 2000, minFreeMb: 1500 }).ok, "RAM gate verdicts");

  const rdir = mkdtempSync(join(tmpdir(), "lf-pool-lanes-"));
  const live = spawn(process.execPath, ["-e", "setTimeout(()=>{},120000)"], { stdio: "ignore", windowsHide: true });
  const rLockDir = join(rdir, "tmp", "wt", "locks");
  mkdirSync(rLockDir, { recursive: true });
  spawnSync("git", ["init", "-q", rdir], { windowsHide: true });
  writeFileSync(join(rdir, "lf.local.json"), JSON.stringify({ pool: { machines: [{ name: "lf-testbox", disabled: true }, { name: "local", lanes: { test: 1 } }] } }));
  const hold = (name, extra = {}) => writeFileSync(join(rLockDir, `${name}.lock`), JSON.stringify({ pid: live.pid, at: new Date().toISOString(), name, root: rdir, slug: "holder", ...extra }));
  const envKeys = ["LF_POOL_MIN_FREE_MB", "LF_POOL_PROMOTE_MIN", "LF_POOL_SESSION_IDLE_MIN"];
  try {
    // RAM gate on a real lane: a second concurrent server waits under the floor.
    hold("server-2001", { purpose: "session" });
    const lane1 = q.lanesFor(lanes, "test")[0];
    const blocked = {};
    check(pool.tryLocal(rdir, lanes, lane1, {}, { freeMb: 900, blocked }) === null && !!blocked.ram, "RAM gate: a second server started with 0.9 GB free");
    const t1 = pool.tryLocal(rdir, lanes, lane1, { slug: "t" }, { freeMb: 4000 });
    check(!!t1 && pool.heldLocalSlot() === t1 && t1.port === 2002, "test lane slot: wrong lock, port, or not recorded as held");
    if (t1) t1.release();
    check(pool.heldLocalSlot() === null, "release did not clear the held slot");

    // Self-deadlock: the only slot held by the caller's own session fails fast; lanes and other worktrees do not.
    const shared = { name: "local", via: "local", slots: 1, lanes: null };
    check(/own session/.test(pool.selfDeadlock(rdir, [shared], "test") ?? ""), "own session on the only slot: no fail-fast");
    check(pool.selfDeadlock(rdir, [lanes], "test") === null, "own session with free test lanes: fail-fast anyway");
    hold("server-2001", { purpose: "session", root: join(rdir, "other") });
    check(pool.selfDeadlock(rdir, [shared], "test") === null, "another worktree's session: fail-fast anyway");
    rmSync(join(rLockDir, "server-2001.lock"), { force: true });

    // Priority order through the real queue (one test lane): low queued first, normal second.
    process.env.LF_POOL_MIN_FREE_MB = "1";
    process.env.LF_POOL_PROMOTE_MIN = "60";
    hold("server-test-1", { purpose: "test only" });
    const order = [];
    const run = async (label, priority) => {
      const slot = await pool.acquireSlot(rdir, { mode: "local", purpose: "test only", priority, waitMs: 60000 });
      order.push(label);
      await new Promise((r) => setTimeout(r, 300));
      slot.release();
    };
    const pLow = run("low", "low");
    await new Promise((r) => setTimeout(r, 400));
    const pNormal = run("normal", "normal");
    await new Promise((r) => setTimeout(r, 1500));
    rmSync(join(rLockDir, "server-test-1.lock"), { force: true });
    await Promise.all([pLow, pNormal]);
    check(order.join() === "normal,low", `priority: a queued low went before a later normal (${order.join()})`);

    // Promotion: the low job waited past the limit, so a later normal does not overtake it.
    process.env.LF_POOL_PROMOTE_MIN = "0.02";
    hold("server-test-1", { purpose: "test only" });
    order.length = 0;
    const pLow2 = run("low", "low");
    await new Promise((r) => setTimeout(r, 1800));
    const pNormal2 = run("normal", "normal");
    await new Promise((r) => setTimeout(r, 1500));
    rmSync(join(rLockDir, "server-test-1.lock"), { force: true });
    await Promise.all([pLow2, pNormal2]);
    check(order.join() === "low,normal", `promotion: a low job past the promote limit was overtaken (${order.join()})`);

    // Lease reaper: idle past the limit while a job waits is closed; driven or leased is not.
    const sdir = join(rdir, "session");
    mkdirSync(sdir, { recursive: true });
    const oldIso = new Date(Date.now() - 20 * 60000).toISOString();
    writeFileSync(join(sdir, "session.json"), JSON.stringify({ startedAt: oldIso, readyAt: oldIso }));
    hold("server-2001", { purpose: "session", at: oldIso });
    const waiting = [{ pid: process.pid, lane: "session", waitingOn: ["server-2001"] }];
    const closed = [];
    const reap = (tickets, extra = {}) => pool.reapIdleSessions(rdir, [lanes], tickets, { sessionIdleMin: 15, promoteMin: 20, minFreeMb: 1 }, { sessionDir: sdir, close: (i) => closed.push(i.pid), ...extra });
    check(reap([]).length === 0, "reaper closed a session nobody waits for");
    writeFileSync(join(sdir, "lease.json"), JSON.stringify({ lastCommandAt: new Date().toISOString() }));
    check(reap(waiting).length === 0, "reaper closed a session that just ran a command");
    writeFileSync(join(sdir, "lease.json"), JSON.stringify({ lastCommandAt: oldIso, leaseUntil: new Date(Date.now() + 600000).toISOString() }));
    check(reap(waiting).length === 0, "reaper closed a session under an explicit lease");
    writeFileSync(join(sdir, "lease.json"), JSON.stringify({ lastCommandAt: oldIso }));
    check(reap(waiting, { now: Date.now() - 19 * 60000 }).length === 0, "reaper closed a session under the idle limit");
    check(reap(waiting).length === 1 && closed.length === 1, "reaper did not close a session idle 20 min (limit 15) with a job waiting");
    check(existsSync(join(sdir, "reaped.json")), "reaper left no note for the session's agent");

    // Short timeout end to end: a queued session request closes an idle session and takes the slot.
    process.env.LF_POOL_SESSION_IDLE_MIN = "0.03";
    const idleIso = new Date().toISOString();
    writeFileSync(join(sdir, "session.json"), JSON.stringify({ startedAt: idleIso, readyAt: idleIso }));
    rmSync(join(sdir, "lease.json"), { force: true });
    hold("server-2001", { purpose: "session", at: idleIso, root: join(rdir, "nosuch") });
    const t0 = Date.now();
    const got = pool.acquireSessionLaneSync(rdir, { waitMs: 40000, meta: { slug: "next" } });
    const waited = Date.now() - t0;
    check(waited > 1500 && waited < 25000, `idle session reaped after ${waited} ms (expected a few seconds)`);
    if (got) {
      const lockInfo = JSON.parse(readFileSync(join(rLockDir, "server-2001.lock"), "utf8"));
      check(lockInfo.pid === process.pid && lockInfo.purpose === "session", "the session slot is not held by the new request");
      rmSync(join(rLockDir, "server-2001.lock"), { force: true });
    }
    check(readFileSync(join(rdir, "tmp", "wt", "pool", "runs.jsonl"), "utf8").includes('"kind":"idle-session"'), "idle-session reap not logged in runs.jsonl");
  } catch (e) {
    failures.push(`lanes section threw: ${e.message}`);
  } finally {
    for (const k of envKeys) delete process.env[k];
    try {
      live.kill();
    } catch {}
    rmSync(rdir, { recursive: true, force: true });
  }

  // 5. runtime budgets: learned from history, floor, seeded default, self-correcting.
  const passRuns = (kind, durS, n, machine = "local") => Array.from({ length: n }, () => ({ purpose: kind, machine, result: "pass", durS }));
  check(q.budgetSeconds([], "test only", "local").runS === 30 && q.budgetSeconds([], "test all", "local").runS === 90 && q.budgetSeconds([], "test persistence", "local").runS === 120, "seeded budgets are not 30/90/120 s");
  check(q.budgetSeconds([], "test only", "lf-testbox", { multiplier: 2 }).runS === 60, "machine multiplier does not scale the seeded budget");
  check(q.budgetSeconds(passRuns("test only", 9, 20), "test only", "local").runS === 30, "3x p95 of 9 s runs is under the 30 s floor but was not floored");
  check(q.budgetSeconds(passRuns("test all", 20, 20), "test all", "local").runS === 60, "budget is not 3x the p95");
  check(q.budgetSeconds(passRuns("test only", 9, 4), "test only", "local").source === "default", "4 passes already count as history");
  check(q.budgetSeconds(passRuns("e2e", 40, 6), "e2e", "local").runS === 60, "e2e budget is not per pass");
  check(q.budgetSeconds(passRuns("test only", 9, 20, "lf-testbox"), "test only", "local").source === "default", "another machine's history was used");
  const flaky = [...passRuns("test only", 9, 40), ...Array.from({ length: 3 }, () => ({ purpose: "test only", machine: "local", result: "timeout", durS: 30 }))];
  check(q.budgetSeconds(flaky, "test only", "local").runS === 60, "a 7% timeout rate did not raise the budget");
  check(q.budgetSeconds(passRuns("test only", 9, 20), "test only", "local", { override: 200 }).runS === 200, "--budget does not override");
  // priority resolution: flag, then LF_PRIORITY, then the worktree marker, then normal
  mkdirSync(join(dir, "tmp"), { recursive: true });
  writeFileSync(join(dir, "tmp", "wt.json"), JSON.stringify({ slug: "x", priority: "low" }));
  delete process.env.LF_PRIORITY;
  check(pool.resolvePriority(dir) === "low", "worktree marker priority ignored");
  process.env.LF_PRIORITY = "high";
  check(pool.resolvePriority(dir) === "high" && pool.resolvePriority(dir, "normal") === "normal", "LF_PRIORITY / --priority order wrong");
  delete process.env.LF_PRIORITY;
  rmSync(join(dir, "tmp", "wt.json"));
  check(pool.resolvePriority(dir) === "normal", "default priority is not normal");
  check(pool.budgetsFor(dir, { name: "local", budgetMultiplier: 1 }, "test only").bootS === 30, "boot budget is not 30 s");
} finally {
  rmSync(dir, { recursive: true, force: true });
}

if (failures.length) {
  console.log("pool-selftest: FAIL");
  for (const f of failures) console.log(`  ${f}`);
  process.exit(1);
}
console.log("pool-selftest: OK (dead remote owner reclaimed, config re-read per poll, hung holder reclaimed + logged, lanes, priority order + promotion, RAM gate, self-deadlock, idle-session reaper, runtime budgets)");
