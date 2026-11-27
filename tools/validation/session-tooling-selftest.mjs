// Proves the three session-tooling faults of the third X-6 dry run (docs/playtest/rounds/2026-11-27-x6-dryrun3.md) are gone:
//   1. `harness.mjs flow <name>` with no --client on a live session sent to the default profile (nobody reads it), every
//      step waited out its timeout and nothing printed for 10 minutes: it now defaults to client 1 (one stderr line when
//      the session has more clients) and a flow stops at once when no session answers;
//   2. `session up --clients 4 --wait` answered "no session lane freed within 90 min" after a second: on the laptop (the
//      local machine, maxClients 2) the maxClients heat cap left no candidate, and "no candidate" came back as the same
//      null as "waited and nothing freed". The local machine is not capped (as without --wait) and no machine at all is a
//      refusal that says so;
//   3. sessions closed as idle a few minutes after `ready` (the client-less flow above never stamped the lease; `--hold`
//      only raises the hold cap, never the idle rule): a running proof or restart now has a heartbeat, the pool counts
//      it as activity and the hold cap waits for it (the reaper cases are in pool-selftest "A proof or restart in progress").
// inputs: agent-box/harness.mjs, agent-box/session.mjs, tools/remote/pool.mjs, tools/lib/session-lease.mjs, tools/lib/session-heartbeat.mjs, tools/lib/pool-queue.mjs
// red-proof: 2026-11-27, on origin/main defaultClientArgs, flowUnanswered, busyDefersClose and session-heartbeat.mjs did not exist
// (the imports failed), `sessionMachineCandidates(root, 4)` on hostname PC1 returned [] and chooseSessionMachine(--wait) resolved null.
import { existsSync, mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { hostname } from "node:os";
import { join } from "node:path";
import { spawn } from "node:child_process";
import { scratchDir } from "../lib/scratch.mjs";
import { defaultClientArgs, flowUnanswered } from "../../agent-box/harness.mjs";
import * as pool from "../remote/pool.mjs";
import { busyDefersClose } from "../lib/pool-queue.mjs";
import { lastActiveMs, readLease, touchLease } from "../lib/session-lease.mjs";
import { runBeats, startHeartbeat } from "../lib/session-heartbeat.mjs";

const failures = [];
const check = (ok, msg) => { if (!ok) failures.push(msg); };
const sleep = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);

// ---- 1. a harness command with no --client
const root = "C:/LF/worktrees/Life-Framework-ws-x";
const live = (clients, extra = {}) => ({ id: "abc123", root, serverPid: 111, clients: Array.from({ length: clients }, (_, i) => ({ n: i + 1, pid: 200 + i })), expectClients: clients, ...extra });
const alive = (p) => p === 111 || (p >= 200 && p < 210);
const go = (argv, session, env = {}) => defaultClientArgs(argv, session, root, alive, env);
{
  const two = go(["flow", "playtest-030-first-two-hours"], live(2));
  check(two.argv.join(" ") === "--client 1 flow playtest-030-first-two-hours", `a flow on a 2-client session got '${two.argv.join(" ")}'`);
  check(/2 clients/.test(two.note) && /--client/.test(two.note), `the 2-client note is wrong: '${two.note}'`);
  const one = go(["state"], live(1));
  check(one.argv.join(" ") === "--client 1 state" && one.note === "", `a 1-client session should default quietly: '${one.argv.join(" ")}' '${one.note}'`);
  check(go(["--client", "2", "flow", "x"], live(2)).argv.join(" ") === "--client 2 flow x", "an explicit --client was changed");
  check(go(["flow", "x"], null).argv.join(" ") === "flow x", "no session: the argv must stay as typed (Workbench or default profile)");
  check(go(["flow", "x"], live(2, { stoppedAt: "2026-11-27T01:00:00Z" })).argv[0] === "flow", "a stopped session was addressed");
  check(go(["flow", "x"], live(2, { root: "C:/LF/worktrees/Life-Framework-ws-other" })).argv[0] === "flow", "another checkout's session was addressed");
  check(go(["flow", "x"], { ...live(2), serverPid: 5, clients: [{ n: 1, pid: 6 }] }).argv[0] === "flow", "a session whose processes are all gone was addressed");
  check(go(["flow", "x"], live(2), { LF_HARNESS_NO_DEFAULT_CLIENT: "1" }).argv[0] === "flow", "LF_HARNESS_NO_DEFAULT_CLIENT=1 did not turn the default off");
  for (const cmd of ["selftest", "status", "setup", "launch", "play", "town", "layers"]) check(go([cmd], live(2)).argv[0] === cmd, `${cmd} must not be aimed at a client`);
  check(go(["chains"], live(2)).argv[0] === "chains" && go(["chains", "--run"], live(2)).argv[0] === "--client", "chains only goes to a client with --run");
  check(go(["--local", "flow", "x"], live(2)).argv.join(" ") === "--client 1 --local flow x" || go(["--local", "flow", "x"], live(2)).argv[0] === "--client", "a flag before the command lost the default");
}
check(flowUnanswered({ status: "error", detail: "no_session" }), "a timed-out state was not 'unanswered'");
check(flowUnanswered({ status: "error", detail: "no_live_session (the pool closed it: idle 6.0 min)" }), "a closed session was not 'unanswered'");
check(flowUnanswered({ status: "error", detail: "session_owned_by_other_checkout C:/x" }), "another checkout's session was not 'unanswered'");
check(!flowUnanswered({ status: "ok", detail: "cash=100" }) && !flowUnanswered({ status: "error", detail: "no_widget" }), "a game answer counted as unanswered");

// ---- 2. `up --clients N --wait` routing
{
  const dir = scratchDir("lf-sess-tooling-");
  writeFileSync(join(dir, "lf.local.json"), "{}");
  const names = (clients) => pool.sessionMachineCandidates(dir, clients).map((m) => m.name).join();
  pool.setHostnameForTest("PC1"); // the laptop: its pool entry (maxClients 2) is the local machine
  try {
    check(names(4) === "lf-testbox" && names(2) === "lf-testbox" && names(1) === "lf-testbox", `the laptop's local machine was excluded by its maxClients (${names(4) || "none"} for 4 clients)`);
  } finally {
    pool.setHostnameForTest(hostname());
  }
  pool.setHostnameForTest("PC"); // this PC: local, plus the laptop over ssh
  try {
    check(names(4) === "local" && names(3) === "local" && names(2) === "local,lf-testbox", `heat cap on a pool machine over ssh changed (4 clients: ${names(4)}, 3: ${names(3)}, 2: ${names(2)})`);
    // No machine can run it: a refusal at once with --wait, the old null without.
    writeFileSync(join(dir, "lf.local.json"), JSON.stringify({ pool: { machines: [{ name: "local", caps: ["server"] }, { name: "lf-testbox", disabled: true }] } }));
    check(names(2) === "", `a machine without the clients cap was a candidate for 2 clients (${names(2)})`);
    let refusal = null;
    const quiet = () => {};
    const waited = await pool.chooseSessionMachine(dir, { clients: 2, profile: "lowspec", waitMs: 60000, log: quiet }).catch((e) => { refusal = e; return "threw"; });
    check(waited === "threw" && refusal?.refusal === true && /no machine in the pool can run a session with 2 client/.test(refusal.message), `--wait with no capable machine did not refuse at once (${waited}, ${refusal?.message})`);
    const nowait = await pool.chooseSessionMachine(dir, { clients: 2, profile: "lowspec", waitMs: 0, log: quiet });
    check(nowait === null, "without --wait the router changed (it hands the request to this PC's own `up`)");
  } finally {
    pool.setHostnameForTest(hostname());
  }
}

// ---- 3. the lease, the heartbeat and the reaper's busy rule
{
  const sdir = scratchDir("lf-sess-lease-");
  const iso = (off) => new Date(Date.now() + off).toISOString();
  check(busyDefersClose({ busyUntilMs: Date.now() + 60000, now: Date.now(), heldMs: 25 * 60000, capMin: 20, graceMin: 5, extraMin: 20 }), "a fresh heartbeat did not defer the hold cap");
  check(!busyDefersClose({ busyUntilMs: Date.now() - 1000, now: Date.now(), heldMs: 25 * 60000, capMin: 20, graceMin: 5, extraMin: 20 }), "a stale heartbeat deferred the hold cap");
  check(!busyDefersClose({ busyUntilMs: Date.now() + 60000, now: Date.now(), heldMs: 50 * 60000, capMin: 20, graceMin: 5, extraMin: 20 }), "a heartbeat kept a lane past the extra window");
  check(!busyDefersClose({ busyUntilMs: NaN, now: Date.now(), heldMs: 25 * 60000, capMin: 20, graceMin: 5, extraMin: 20 }), "no heartbeat deferred the hold cap");

  touchLease(sdir, "mining-iron-chain", 90000);
  const lease = readLease(sdir);
  check(Date.parse(lease.busyUntil) > Date.now() + 80000 && lease.lastVerb === "mining-iron-chain", `touchLease(busyMs) wrote ${JSON.stringify(lease)}`);
  check(lastActiveMs(sdir, NaN) >= Date.parse(lease.busyUntil), "a busy session's last activity is before its busyUntil, so it would age while it runs");
  writeFileSync(join(sdir, "lease.json"), JSON.stringify({ lastCommandAt: iso(-60 * 60000) }));
  mkdirSync(join(sdir, "client4", "profile", "agent-box"), { recursive: true });
  writeFileSync(join(sdir, "client4", "profile", "agent-box", "command.txt"), "1 state");
  check(Date.now() - lastActiveMs(sdir, NaN) < 5000, "a command to client 4 did not count as activity");

  // runBeats: stops when the proof's process is gone; stops at its own maximum.
  const touches = [];
  let calls = 0;
  const beats = runBeats({ dir: sdir, parentPid: 7, label: "proof", beatMs: 1000, maxMin: 45, alive: () => ++calls <= 3, touch: (d, l, ms) => touches.push([l, ms]), sleep: () => {} });
  check(beats === 3 && touches.every(([l, ms]) => l === "proof" && ms === 3000), `runBeats: ${beats} beats, touches ${JSON.stringify(touches)}`);
  let clock = 0;
  const capped = runBeats({ dir: sdir, parentPid: 7, label: "proof", beatMs: 60000, maxMin: 5, alive: () => true, touch: () => {}, sleep: (ms) => { clock += ms; }, now: () => clock });
  check(capped === 5, `a hung proof's heartbeat did not stop at its maximum (${capped} beats)`);

  // startHeartbeat names the detached beat process; no session dir, no process.
  const spawned = [];
  startHeartbeat(join(sdir, "nosuch"), "proof", { spawnFn: (...a) => { spawned.push(a); return { unref() {} }; } });
  check(spawned.length === 0, "a heartbeat was started with no session dir");
  startHeartbeat(sdir, "proof", { parentPid: 4242, beatMs: 5000, maxMin: 9, spawnFn: (exe, args, opts) => { spawned.push([exe, args, opts]); return { unref() {} }; } });
  const [, args, opts] = spawned[0] ?? [];
  check(args?.includes("--beat") && args.includes(sdir) && args.includes("4242") && opts?.detached === true && opts?.windowsHide === true, `the beat process was spawned wrongly: ${JSON.stringify(args)}`);

  // Real process: beats while its parent lives, stops after.
  const parent = spawn(process.execPath, ["-e", "setTimeout(() => {}, 2500)"], { stdio: "ignore", windowsHide: true });
  writeFileSync(join(sdir, "lease.json"), JSON.stringify({ lastCommandAt: iso(-60 * 60000) }));
  startHeartbeat(sdir, "restart", { parentPid: parent.pid, beatMs: 200, maxMin: 1 });
  sleep(1200);
  const during = readLease(sdir);
  check(Date.parse(during.busyUntil) > Date.now() && Date.now() - Date.parse(during.lastCommandAt) < 3000 && during.lastVerb === "restart", `the heartbeat did not stamp the lease while its parent ran: ${JSON.stringify(during)}`);
  const deadline = Date.now() + 8000;
  while (Date.now() < deadline && parent.exitCode === null) sleep(100);
  sleep(900); // one more beat interval: it sees the parent gone
  const stopped = readLease(sdir).lastCommandAt;
  sleep(800);
  check(readLease(sdir).lastCommandAt === stopped, "the heartbeat kept stamping the lease after its parent ended");
  check(existsSync(join(sdir, "lease.json")), "no lease file");
}

if (failures.length) {
  for (const f of failures) console.log(`FAIL  ${f}`);
  console.log("session-tooling-selftest: FAIL");
  process.exit(1);
}
console.log("session-tooling-selftest: OK (harness default client, flow stop, --wait routing, lease heartbeat)");
