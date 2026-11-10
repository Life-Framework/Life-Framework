// Proves the test pool's reclaim and re-plan rules without booting a game or
// an ssh session (tools/remote/pool.mjs, docs/worktrees.md "Test pool"):
//   1. a remote slot whose owner pid on this PC is dead is reclaimed;
//   2. a waiting ticket re-reads lf.local.json, so a re-enabled machine counts;
//   3. a slot held past the hang limit with no server alive is reclaimed,
//      logged in runs.jsonl, and the owner gets a notice;
//   4. lanes (test lanes on 2002.., session lane on 2001), priority order and
//      aging through the real queue, the RAM gate, the self-deadlock fail-fast,
//      the idle-session reaper (also end to end with a short timeout);
//   4b. the session hold cap (warn once, close after the grace, per-session --hold, no stamp = no cap), the session
//      record in runs.jsonl, and a retried request keeping its queue age;
//   5. runtime budgets: learned 3x p95 with a 30 s floor, seeded defaults,
//      machine multiplier, self-raise after timeouts, --budget.
// red-proof 2026-11-10: dropping the hostname branch in loadMachines printed "an entry whose hostname is this machine is not the local machine with its own lanes".
// red-proof: with remoteOwnerGone returning null it printed "dead owner pid on
// this PC: no reclaim verdict"; with the mtime key removed from localConfig it
// printed "config flip: local still disabled after re-enable"; with the hang
// branch removed from tryLocal it printed "hung local holder: slot not reclaimed".
// red-proof (hold cap, queue age): with queuedSince returning now it printed "a retried session request did not keep its
// first age"; with the grace phase treated as close it printed "hold cap closed a session inside the 5 min grace".
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
import { takeHoldWarning } from "../lib/session-lease.mjs";
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
  // Two session lanes: the old lane keeps its name and port, lane 2 is server-2011 on 2011 (A2S 17877).
  const lanes2 = { ...lanes, lanes: { test: 2, session: 2 } };
  check(q.lanesFor(lanes2, "session").map((l) => `${l.lock}:${l.port}`).join() === "server-2001:2001,server-2011:2011", "two session lanes are not server-2001:2001 and server-2011:2011");
  check(q.portForLock("server-2011") === 2011 && q.sessionA2sPort(2001) === null && q.sessionA2sPort(2011) === 17877, "session lane 2 port / A2S port");
  check(!q.lanesFor(lanes2, "test").some((l) => l.port === 2011) && q.allLaneLocks(lanes2).length === 4, "a test lane shares a port with a session lane");
  const ses = { sessionReserveServerMb: 800, sessionReserveClientMb: 1200 };
  check(q.sessionReserveMb(ses, 1) === 2000 && q.sessionReserveMb(ses, 2) === 3200, "session reserve per client count");
  check(q.ramVerdict({ running: 1, freeMb: 3600, minFreeMb: 1500, reserveMb: 2000 }).ok && !q.ramVerdict({ running: 1, freeMb: 3400, minFreeMb: 1500, reserveMb: 2000, pendingMb: 2000 }).ok, "RAM gate ignores the reserve or the sessions still booting");
  check(!q.ramVerdict({ running: 1, freeMb: 3400, minFreeMb: 1500, reserveMb: 2000, pendingMb: 2000 }).ok && /for the session/.test(q.ramVerdict({ running: 1, freeMb: 3000, minFreeMb: 1500, reserveMb: 2000 }).reason), "RAM wait reason lacks the session reserve");
  const T0 = Date.parse("2026-01-01T00:00:00Z");
  const tk = (file, priority, minAgo, lane = "test") => ({ file, priority, lane, at: new Date(T0 - minAgo * 60000).toISOString(), machines: ["local"] });
  const min20 = 20 * 60000;
  check(q.blocks(tk("s", "normal", 9, "session"), tk("t", "normal", 1, "session"), lanes2, Date.parse("2026-01-01T00:00:00Z"), 20 * 60000) === true, "an older session ticket does not count toward the lane queue");
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
  // Backfill: a head job stuck on RAM (3-client session) does not block a small job behind it.
  const bigHead = { ...tk("1-big", "normal", 5, "session"), stuck: "ram", needMb: 5100, clients: 3 };
  const small = tk("2-small", "normal", 1, "session");
  check(q.blocks(bigHead, small, lanes, T0, min20, { starveMs: min20, freeMb: 3400 }) === false, "a RAM-stuck head still blocks a small job behind it");
  check(q.blocks({ ...bigHead, stuck: undefined }, small, lanes, T0, min20) === true, "a head that has not tried must still block (plain FIFO)");
  check(q.blocks({ ...bigHead, stuck: "lane" }, small, lanes, T0, min20, { starveMs: min20, freeMb: 3400 }) === true, "a lane-stuck head loses its turn to a job wanting the same lane");
  check(q.blocks({ ...bigHead, lane: "session", stuck: "lane" }, tk("3-t", "normal", 1, "test"), { ...lanes, lanes: null }, T0, min20, { starveMs: min20, freeMb: 3400 }) === true, "shared-lane head lost its turn");
  // Starvation: after 20 min the head holds its place once the RAM it needs is there, not before.
  const oldHead = { ...bigHead, at: new Date(T0 - 25 * 60000).toISOString() };
  check(q.blocks(oldHead, small, lanes, T0, min20, { starveMs: min20, freeMb: 3400 }) === false, "a starved head reserves the lane before its RAM is there");
  check(q.blocks(oldHead, small, lanes, T0, min20, { starveMs: min20, freeMb: 5200 }) === true, "a starved head with its RAM does not reserve the lane");
  check(q.waitReason(bigHead) === "ram 3-client" && q.waitReason({ stuck: "lane" }) === "lane" && q.waitReason({}, 2) === "behind 2", "waitReason text");
  check(q.ramVerdict({ running: 1, freeMb: 1000, minFreeMb: 1500, reserveMb: 3600 }).needMb === 5100, "ramVerdict does not report needMb");
  check(q.ramVerdict({ running: 0, freeMb: 100, minFreeMb: 1500 }).ok && !q.ramVerdict({ running: 1, freeMb: 1000, minFreeMb: 1500 }).ok && q.ramVerdict({ running: 1, freeMb: 2000, minFreeMb: 1500 }).ok, "RAM gate verdicts");

  // Aging is the fair share: a low job that waited 40 min is "high" and, arriving first, goes ahead of new
  // normal and new high jobs; at 61 min it still does. (The film job's real starvation was the wait cap and the
  // restart of its age on every retry: see queuedSince.)
  check(q.queueOrder([tk("2-high", "high", 0), tk("1-low", "low", 61)], T0, min20)[0].file === "1-low", "a low job waiting 61 min did not go ahead of a new high one");
  check(q.queueOrder([tk("2-normal", "normal", 1), tk("1-low", "low", 61)], T0, min20)[0].file === "1-low", "a low job waiting 61 min did not go ahead of a new normal one");
  check(q.queueOrder([tk("2-high", "high", 0), tk("1-low", "low", 30)], T0, min20)[0].file === "2-high", "a low job waiting 30 min already went ahead of a new high one");
  // Hold verdicts and the --hold clamp.
  const hv = (o) => q.holdVerdict({ heldSinceMs: T0 - 25 * 60000, capMin: 20, graceMin: 5, warnedAtMs: NaN, waiting: true, now: T0, ...o }).phase;
  check(hv({}) === "warn" && hv({ waiting: false }) === "ok" && hv({ capped: false }) === "ok" && hv({ capMin: 30 }) === "ok", "holdVerdict: warn / no waiter / no stamp / under cap");
  check(hv({ warnedAtMs: T0 - 2 * 60000 }) === "grace" && hv({ warnedAtMs: T0 - 5 * 60000 }) === "close", "holdVerdict: grace and close");
  const lim = { holdMin: 20, maxMin: 45 };
  check(q.clampHoldMin(undefined, lim) === 20 && q.clampHoldMin(30, lim) === 30 && q.clampHoldMin(90, lim) === 45 && q.clampHoldMin(5, lim) === 20, "clampHoldMin");
  const st = q.sessionStats([{ waitS: 60, heldS: 600, boots: 1, closedBy: "owner" }, { waitS: 180, heldS: 1800, boots: 3, closedBy: "hold-cap" }]);
  check(st.avgWaitS === 120 && st.avgHeldS === 1200 && st.avgBoots === 2 && st.closed.owner === 1 && st.closed["hold-cap"] === 1, "sessionStats averages");

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

    // Hold cap: a driven (not idle) session past its cap with a session request waiting is warned once,
    // then closed after the grace; no waiter, no holdCap stamp, or a raised --hold means no close.
    {
      const lease = () => JSON.parse(readFileSync(join(sdir, "lease.json"), "utf8"));
      const heldIso = new Date(Date.now() - 25 * 60000).toISOString();
      const setSession = (extra = {}) => writeFileSync(join(sdir, "session.json"), JSON.stringify({ startedAt: heldIso, readyAt: heldIso, heldSince: heldIso, holdCap: true, holdMin: 20, waitS: 12.5, priority: "normal", boots: 2, bootS: [23, 22], expectClients: 1, mission: "playtest", ...extra }));
      const driven = () => writeFileSync(join(sdir, "lease.json"), JSON.stringify({ lastCommandAt: new Date().toISOString() }));
      const sessWait = [{ pid: process.pid, lane: "session", waitingOn: ["server-2001"] }];
      const settings = { sessionIdleMin: 15, promoteMin: 20, minFreeMb: 1, sessionHoldMin: 20, sessionGraceMin: 5, sessionHoldMaxMin: 45 };
      const reapHold = (tickets, extra = {}) => pool.reapIdleSessions(rdir, [lanes], tickets, settings, { sessionDir: sdir, close: (i) => closed.push(i.pid), ...extra });
      hold("server-2001", { purpose: "session", at: heldIso, slug: "holder" });
      closed.length = 0;
      rmSync(join(sdir, "reaped.json"), { force: true });
      setSession();
      driven();
      check(reapHold([]).length === 0 && !existsSync(join(sdir, "reaped.json")), "hold cap closed a session nobody waits for");
      check(reapHold([{ pid: process.pid, lane: "test", waitingOn: ["server-2001"] }]).length === 0 && !lease().holdWarnedAt, "hold cap warned for a test job (only a session request counts)");
      check(reapHold(sessWait).length === 0 && closed.length === 0, "hold cap closed a session on the first pass (it must warn first)");
      check(!!lease().holdWarnedAt, "hold cap did not stamp the warning");
      const w1 = takeHoldWarning(sdir);
      check(/^session: 1 waiting; finish and run .*session.mjs down.* within 5 min/.test(w1), `warning line is wrong: '${w1}'`);
      check(takeHoldWarning(sdir) === "", "the hold warning showed twice");
      // The waiter polls every ~10 s; simulate polls 50 s apart. Not closed inside the grace, closed at 5 min.
      const t0 = Date.now();
      let early = 0;
      for (let k = 1; k <= 5; k++) early += reapHold(sessWait, { now: t0 + k * 50000 }).length;
      check(early === 0 && closed.length === 0, "hold cap closed a session inside the 5 min grace");
      const late = reapHold(sessWait, { now: t0 + 6 * 50000 });
      check(late.length === 1 && late[0].kind === "hold-cap" && closed.length === 1, "hold cap did not close a session 6 min after the warning");
      check(JSON.parse(readFileSync(join(sdir, "reaped.json"), "utf8")).kind === "hold-cap", "reaped.json does not say hold-cap");
      const runsText = readFileSync(join(rdir, "tmp", "wt", "pool", "runs.jsonl"), "utf8");
      const rec = runsText.split(String.fromCharCode(10)).filter(Boolean).map((l) => JSON.parse(l)).filter((r) => r.kind === "session").at(-1);
      check(rec && rec.closedBy === "hold-cap" && rec.boots === 2 && rec.waitS === 12.5 && rec.heldS >= 1490 && rec.slug === "holder", `session record is wrong: ${JSON.stringify(rec)}`);
      check(runsText.includes('"kind":"hold-cap"'), "hold-cap close not logged as a reclaim");
      // No stamp (a session started before the cap): never warned or closed.
      rmSync(join(sdir, "reaped.json"), { force: true });
      setSession({ holdCap: undefined });
      driven();
      check(reapHold(sessWait, { now: Date.now() + 10 * 60000 }).length === 0 && !lease().holdWarnedAt, "hold cap judged a session started before the cap existed");
      // --hold 45: not past its cap at 25 min.
      setSession({ holdMin: 45 });
      driven();
      check(reapHold(sessWait).length === 0 && !lease().holdWarnedAt, "hold cap ignored the session's raised --hold");
      // Waiters left for over a minute: the warning is void and the grace starts again.
      setSession();
      driven();
      reapHold(sessWait);
      const old = lease();
      writeFileSync(join(sdir, "lease.json"), JSON.stringify({ ...old, lastCommandAt: new Date().toISOString(), holdSeenAt: new Date(Date.now() - 5 * 60000).toISOString(), holdWarnedAt: new Date(Date.now() - 10 * 60000).toISOString() }));
      check(reapHold(sessWait).length === 0, "a warning whose waiters left closed the session at once");
    }

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
    // Backfill, end to end: a 3-client head waiting on RAM does not hold up a small request, and once it
    // has waited past the starvation limit (with its RAM there) it does.
    {
      const qd = join(rdir, "tmp", "wt", "pool", "queue");
      mkdirSync(qd, { recursive: true });
      const headFile = join(qd, "000000000000001-0000001-aaaaaa.json");
      const head = (ageMin) => ({ pid: process.pid, at: new Date(Date.now() - ageMin * 60000).toISOString(), slug: "big", purpose: "session", root: rdir, pinned: true, lane: "session", priority: "normal", waitingOn: ["server-2001", "ram"], machines: ["local"], clients: 3, stuck: "ram", needMb: 1 });
      writeFileSync(headFile, JSON.stringify(head(2)));
      let small = null;
      try {
        small = pool.acquireSessionLaneSync(rdir, { waitMs: 5000, meta: { slug: "small" } });
      } catch {}
      check(!!small, "backfill: a small session request waited behind a RAM-stuck head");
      rmSync(join(rLockDir, "server-2001.lock"), { force: true });
      writeFileSync(headFile, JSON.stringify(head(25)));
      let late = null;
      try {
        late = pool.acquireSessionLaneSync(rdir, { waitMs: 1500, meta: { slug: "small2" } });
      } catch {}
      check(!late, "starvation: a starved head's lane was taken by a small request");
      if (late) rmSync(join(rLockDir, "server-2001.lock"), { force: true });
      rmSync(headFile, { force: true });
    }
    // A retried request keeps the age of the one it replaces (a timed-out `up --wait` used to start over), and a
    // request that gets the slot forgets it.
    delete process.env.LF_POOL_SESSION_IDLE_MIN;
    // (the reap above taskkilled `live`; this holder is a fresh process)
    const live2 = spawn(process.execPath, ["-e", "setTimeout(()=>{},60000)"], { stdio: "ignore", windowsHide: true });
    writeFileSync(join(rLockDir, "server-2001.lock"), JSON.stringify({ pid: live2.pid, at: new Date().toISOString(), name: "server-2001", root: join(rdir, "nosuch"), slug: "other", purpose: "session" }));
    const qfile = join(rdir, "tmp", "wt", "pool", "queued.json");
    const tryUp = () => {
      try {
        pool.acquireSessionLaneSync(rdir, { waitMs: 1200, meta: { slug: "retry" } });
        return "got";
      } catch {
        return "timeout";
      }
    };
    check(tryUp() === "timeout", "a session request got a held lane");
    const first = JSON.parse(readFileSync(qfile, "utf8")).retry.at;
    await new Promise((r) => setTimeout(r, 1100));
    check(tryUp() === "timeout" && JSON.parse(readFileSync(qfile, "utf8")).retry.at === first, "a retried session request did not keep its first age");
    const stale = JSON.parse(readFileSync(qfile, "utf8"));
    stale.retry.seenAt = new Date(Date.now() - 20 * 60000).toISOString();
    writeFileSync(qfile, JSON.stringify(stale));
    tryUp();
    check(JSON.parse(readFileSync(qfile, "utf8")).retry.at !== first, "a request last seen 20 min ago kept its old age");
    live2.kill();
    rmSync(join(rLockDir, "server-2001.lock"), { force: true });
    check(tryUp() === "got" && !JSON.parse(readFileSync(qfile, "utf8")).retry, "a request that got the slot kept its queue age");
    rmSync(join(rLockDir, "server-2001.lock"), { force: true });
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
  // A pool entry whose "hostname" is this machine's is the local machine there (the box's own lanes), the PC's entry is dropped.
  writeFileSync(join(dir, "lf.local.json"), JSON.stringify({ pool: { machines: [{ name: "lf-testbox", hostname: hostname() }] } }));
  const selfView = pool.loadMachines(dir);
  check(selfView[0]?.name === "lf-testbox" && selfView[0].via === "local" && selfView[0].lanes?.session === 3 && !selfView.some((m) => m.name === "local"), "an entry whose hostname is this machine is not the local machine with its own lanes");
  writeFileSync(join(dir, "lf.local.json"), "{}");
  check(pool.loadMachines(dir).some((m) => m.name === "local") && pool.loadMachines(dir).find((m) => m.name === "lf-testbox")?.via === "ssh", "this PC lost its local entry");
  check(pool.sessionMachineCandidates(dir, 1).map((m) => m.name).join() === "local,lf-testbox", "session candidates are not this PC first, then the laptop");
} finally {
  rmSync(dir, { recursive: true, force: true });
}

if (failures.length) {
  console.log("pool-selftest: FAIL");
  for (const f of failures) console.log(`  ${f}`);
  process.exit(1);
}
console.log("pool-selftest: OK (dead remote owner reclaimed, config re-read per poll, hung holder reclaimed + logged, lanes, priority order + promotion, RAM gate, self-deadlock, idle-session reaper, session hold cap + warning + record, queue age kept across retries, runtime budgets, a machine's own hostname entry is local there, sessions consider every machine)");
