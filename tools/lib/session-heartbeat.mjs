// A running proof or restart keeps its session's lease fresh. The lease is stamped when a harness command starts, and
// every blocking loop in session.mjs and the proofs is synchronous, so a timer in the proof's own process never
// fires. A small detached process does it instead: every BEAT_MS it stamps lease.json with `busyUntil` a few beats
// ahead, and it ends when the proof's process is gone (or after MAX_MIN, so a hung proof cannot hold a lane for ever).
// The pool then sees a session that is busy, not idle (tools/lib/session-lease.mjs lastActiveMs), and the hold cap
// waits for the command to finish (pool-queue.mjs busyDefersClose).

import { spawn } from "node:child_process";
import { existsSync } from "node:fs";
import { fileURLToPath, pathToFileURL } from "node:url";
import { touchLease } from "./session-lease.mjs";

export const BEAT_MS = 30000;
export const BUSY_BEATS = 3;
export const MAX_MIN = 45;

export function pidAlive(pid) {
  try {
    process.kill(pid, 0);
    return true;
  } catch (e) {
    return e.code === "EPERM";
  }
}

// Called by session.mjs for a command that can run long. Returns the child (tests), never throws.
export function startHeartbeat(dir, label, { parentPid = process.pid, beatMs = BEAT_MS, maxMin = MAX_MIN, spawnFn = spawn } = {}) {
  try {
    if (!dir || !existsSync(dir)) return null;
    touchLease(dir, label, beatMs * BUSY_BEATS);
    const child = spawnFn(process.execPath, [fileURLToPath(import.meta.url), "--beat", dir, String(parentPid), label, String(beatMs), String(maxMin)], { detached: true, stdio: "ignore", windowsHide: true });
    child.unref?.();
    return child;
  } catch {
    return null;
  }
}

// The beat loop (the detached child). Exported with its seams for the selftest.
export function runBeats({ dir, parentPid, label, beatMs = BEAT_MS, maxMin = MAX_MIN, alive = pidAlive, touch = touchLease, sleep, now = Date.now }) {
  const until = now() + maxMin * 60000;
  let beats = 0;
  while (alive(parentPid) && now() < until) {
    touch(dir, label, beatMs * BUSY_BEATS);
    beats++;
    sleep(beatMs);
  }
  return beats;
}

if (process.argv[1] && import.meta.url === pathToFileURL(process.argv[1]).href && process.argv[2] === "--beat") {
  const [, , , dir, parentPid, label, beatMs, maxMin] = process.argv;
  runBeats({ dir, parentPid: Number(parentPid), label, beatMs: Number(beatMs), maxMin: Number(maxMin), sleep: (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms) });
}
