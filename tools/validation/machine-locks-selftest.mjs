// Proves a UDP-port slot lock (server-2001, server-test-<k>) is ONE file per PC even when the PC has two main
// checkouts (tools/lib/machine-locks.mjs, wt.lockFileOf). The laptop is the pool box "lf-testbox" for the main PC
// (its remote holder writes C:/LF/Life-Framework/tmp/wt/locks/server-2001.lock) and also hosts local agents in
// C:/LF/agents/Life-Framework, whose hub is another directory. Each side saw its own lock free, so a remote grid
// session and a local session were both granted UDP 2001 and the later one died with
// "Unable to start replication" (slice #2099, 2026-11-17).
// inputs: tools/wt.mjs, tools/lib/machine-locks.mjs, tools/remote/pool.mjs, tools/validation/machine-locks-selftest.mjs
// red-proof: 2026-11-17, with machineLockDir returning null (no redirect, HEAD's behaviour) the run fails: the "agent root sees
// the box lock" check records a failure (acquireLock took server-2001 beside a live remote holder) and the next acquire
// throws, exit 1. Restored.

import { spawn, spawnSync } from "node:child_process";
import { existsSync, mkdirSync, mkdtempSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { pathToFileURL } from "node:url";
import { safeRemoveTree } from "../link-safety.mjs";

const lib = await import(pathToFileURL(join(import.meta.dirname, "..", "lib", "machine-locks.mjs")).href);
const wt = await import(pathToFileURL(join(import.meta.dirname, "..", "wt.mjs")).href);

const scratch = mkdtempSync(join(tmpdir(), "lf-machine-locks-"));
const failures = [];
const check = (ok, msg) => {
  if (!ok) failures.push(msg);
};
const gitInit = (dir) => {
  mkdirSync(dir, { recursive: true });
  const r = spawnSync("git", ["init", "-q", dir], { windowsHide: true });
  if (r.status !== 0) throw new Error(`git init ${dir} failed`);
};
const norm = (p) => String(p).replace(/\\/g, "/").toLowerCase();
const held = (fn) => {
  try {
    fn();
    return false;
  } catch (e) {
    return /is held/.test(e.message);
  }
};

const boxRoot = join(scratch, "box");
const agentRoot = join(scratch, "agents", "main");
gitInit(boxRoot);
gitInit(agentRoot);
mkdirSync(join(boxRoot, "tmp", "wt", "locks"), { recursive: true });
mkdirSync(join(agentRoot, "tools"), { recursive: true });
const boxLocks = join(boxRoot, "tmp", "wt", "locks");
const agentLocks = join(agentRoot, "tmp", "wt", "locks");

// The agent checkout's own tools/pool.json, as the real one: an ssh entry named for this PC's hostname.
const writePool = (hostname, root) =>
  writeFileSync(join(agentRoot, "tools", "pool.json"), JSON.stringify({ machines: [{ name: "local", via: "local" }, { name: "box", via: "ssh", host: "box", hostname, root: root.replace(/\\/g, "/") }] }));
delete process.env.LF_POOL_JSON;
delete process.env.LF_MACHINE_LOCKS;

try {
  writePool("PC-SELFTEST", boxRoot);
  lib.setMachineHostnameForTest("pc-selftest");

  check(lib.isMachineWide("server-2001") && lib.isMachineWide("server-test-1") && !lib.isMachineWide("build") && !lib.isMachineWide("wt"), "only the server-* slot locks are machine-wide");
  check(norm(lib.machineLockDir(agentRoot)) === norm(boxLocks), `agent checkout on the box PC resolves ${lib.machineLockDir(agentRoot)}, want ${boxLocks}`);
  check(norm(wt.lockFileOf(agentRoot, "server-2001")) === norm(join(boxLocks, "server-2001.lock")), "server-2001 is not the box clone's lock file");
  check(norm(wt.lockFileOf(agentRoot, "server-test-2")) === norm(join(boxLocks, "server-test-2.lock")), "server-test-2 is not the box clone's lock file");
  check(norm(wt.lockFileOf(agentRoot, "build")) === norm(join(agentLocks, "build.lock")), "a build lock moved out of the checkout's own hub");

  // The incident: the box's remote holder owns server-2001 (a live pid); a local acquirer must queue, not share.
  writeFileSync(join(boxLocks, "server-2001.lock"), JSON.stringify({ pid: process.pid, at: new Date().toISOString(), name: "server-2001" }));
  check(held(() => wt.acquireLock(agentRoot, "server-2001", { waitMs: 0 })), "agent root sees the box lock: acquireLock took server-2001 while the box's remote holder owns it");
  safeRemoveTree(join(boxLocks, "server-2001.lock"));
  const took = wt.acquireLock(agentRoot, "server-2001", { waitMs: 0 });
  check(norm(took.file) === norm(join(boxLocks, "server-2001.lock")) && existsSync(took.file), "a free lock is not taken in the box clone's directory");
  wt.releaseLock(agentRoot, "server-2001");
  check(!existsSync(took.file), "releaseLock left the shared lock file behind");

  // A holder started by older code wrote the checkout's own file: it still counts until it ends.
  mkdirSync(agentLocks, { recursive: true });
  const sleeper = spawn(process.execPath, ["-e", "setTimeout(()=>{}, 30000)"], { windowsHide: true, stdio: "ignore" });
  writeFileSync(join(agentLocks, "server-2001.lock"), JSON.stringify({ pid: sleeper.pid, at: new Date().toISOString(), name: "server-2001" }));
  check(held(() => wt.acquireLock(agentRoot, "server-2001", { waitMs: 0 })), "a live holder in the checkout's own hub (older code) was ignored");
  sleeper.kill();
  await new Promise((r) => sleeper.once("exit", r));
  const again = wt.acquireLock(agentRoot, "server-2001", { waitMs: 0 });
  check(!!again, "a dead legacy holder still blocks the slot");
  wt.releaseLock(agentRoot, "server-2001");

  // Nothing changes where the PC is not a pool box, the box clone is missing, or it is this checkout.
  lib.setMachineHostnameForTest("some-other-pc");
  check(lib.machineLockDir(agentRoot) === null, "a PC that is not named by the pool entry got a shared lock directory");
  lib.setMachineHostnameForTest("pc-selftest");
  writePool("PC-SELFTEST", join(scratch, "nowhere"));
  check(lib.machineLockDir(agentRoot) === null, "a pool root that does not exist here redirected the locks");
  writePool("PC-SELFTEST", agentRoot);
  check(lib.machineLockDir(agentRoot) === null, "the box root equal to this checkout redirected the locks");
  writePool("PC-SELFTEST", boxRoot);
  process.env.LF_MACHINE_LOCKS = "0";
  check(lib.machineLockDir(agentRoot) === null, "LF_MACHINE_LOCKS=0 did not switch the redirect off");
  delete process.env.LF_MACHINE_LOCKS;
} finally {
  lib.setMachineHostnameForTest(null);
  safeRemoveTree(scratch);
}

if (failures.length) {
  for (const f of failures) console.log(`FAIL  ${f}`);
  console.log("machine-locks-selftest: FAIL");
  process.exit(1);
}
console.log("machine-locks-selftest: OK (a PC that is a pool box and hosts local agents shares one server slot lock directory; build locks stay in the own hub; an older holder still blocks; no redirect when not a box, the root is missing or equals this checkout)");
