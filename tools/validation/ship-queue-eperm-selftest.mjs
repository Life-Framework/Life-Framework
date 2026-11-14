// Proves the ship queue's state-file write survives a reader on Windows (tools/lib/atomic-file.mjs,
// tools/lib/ship-queue.mjs writeQueue, tools/ship-queue.mjs update): a gate that ran for 11 minutes was failed by
// `EPERM: operation not permitted, rename '...ship-queue.json.tmp' -> '...ship-queue.json'` because a
// `ship-queue status` poll had the file open. A child process holds the file open (FileShare.Read, which is
// what a status reader without FILE_SHARE_DELETE does) while the parent writes it.
// inputs: tools/lib/atomic-file.mjs, tools/lib/ship-queue.mjs, tools/ship-queue.mjs, tools/lib/test-shards.mjs, tools/lib/session-lease.mjs
// red-proof 2026-11-15 (shipq-eperm), each broken alone: writeFileAtomic calling renameSync once (no retry) "writeQueue
// survives a reader holding the file open (EPERM)" and "renameRetry with tries:1 throws EPERM/EBUSY while the file is
// held" stays as the control; update() without its try/catch "update() does not throw when the write keeps failing";
// readQueue returning an empty queue on a read error "readQueue throws, not an empty queue, while the file is locked".
// The held-open checks run on Windows only (elsewhere a rename over an open file just works); the retry rules are pure.

import { spawn, spawnSync } from "node:child_process";
import { existsSync, mkdirSync, mkdtempSync, readFileSync, readdirSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import * as af from "../lib/atomic-file.mjs";
import * as sq from "../lib/ship-queue.mjs";
import { update } from "../ship-queue.mjs";

let failed = 0;
const check = (ok, msg) => {
  if (ok) console.log(`PASS  ${msg}`);
  else {
    failed++;
    console.log(`FAIL  ${msg}`);
  }
};
const err = (code) => Object.assign(new Error(`${code}: simulated`), { code });

// ---- 1. the retry rule, pure
{
  const waits = [];
  let n = 0;
  const out = af.retryTransient(() => (++n < 4 ? (() => { throw err(["EPERM", "EBUSY", "EACCES"][n - 1]); })() : "ok"), { sleep: (ms) => waits.push(ms) });
  check(out === "ok" && n === 4, `retryTransient retries EPERM, EBUSY and EACCES until it works (${n} calls)`);
  check(JSON.stringify(waits) === "[10,20,40]", `the backoff doubles from 10 ms (${JSON.stringify(waits)})`);

  n = 0;
  let thrown = null;
  const slept = [];
  try {
    af.retryTransient(() => { n++; throw err("EPERM"); }, { sleep: (ms) => slept.push(ms) });
  } catch (e) {
    thrown = e;
  }
  const total = slept.reduce((a, b) => a + b, 0);
  check(n === 10 && thrown?.code === "EPERM", `a rename that keeps failing is tried 10 times, then the error is thrown (${n} tries)`);
  check(total >= 700 && total <= 1500, `the default backoff waits about 1 s in all (${total} ms)`);

  n = 0;
  thrown = null;
  try {
    af.retryTransient(() => { n++; throw err("ENOENT"); }, { sleep: () => {} });
  } catch (e) {
    thrown = e;
  }
  check(n === 1 && thrown?.code === "ENOENT", "ENOENT is not retried");
}

// ---- 2. a real file held open by another process
const dir = mkdtempSync(join(tmpdir(), "lf-shipq-eperm-"));
const hubOf = (root) => join(root, "tmp", "wt");

// Holds `file` open for ms with the given FileShare ("Read": others read, nobody writes or replaces it;
// "None": nobody opens it). Resolves once the handle is held; `done` resolves when it is released.
function holdOpen(file, ms, share) {
  const script = `$f=[System.IO.File]::Open($env:LF_HOLD,'Open','Read','${share}'); [Console]::Out.WriteLine('ready'); [Console]::Out.Flush(); Start-Sleep -Milliseconds ${ms}; $f.Close()`;
  const child = spawn("powershell.exe", ["-NoProfile", "-NonInteractive", "-Command", script], { env: { ...process.env, LF_HOLD: file }, windowsHide: true, stdio: ["ignore", "pipe", "ignore"] });
  const done = new Promise((r) => child.on("exit", r));
  const ready = new Promise((res, rej) => {
    const t = setTimeout(() => rej(new Error("the holder never reported ready")), 20000);
    child.stdout.on("data", (d) => {
      if (String(d).includes("ready")) {
        clearTimeout(t);
        res();
      }
    });
    child.on("error", rej);
  });
  return { ready, done, child };
}

try {
  if (process.platform !== "win32") {
    console.log("PASS  held-open checks skipped: a rename over an open file is not blocked on this OS (Windows only)");
  } else {
    const hub = hubOf(dir);
    mkdirSync(hub, { recursive: true });
    const file = sq.statePath(hub);
    const q0 = { version: 1, seq: 1, entries: [{ id: "a-1", slug: "a", seq: 1, state: "gating" }] };
    sq.writeQueue(hub, q0);

    // control: without the retry the rename fails the way the worker log shows
    {
      const h = holdOpen(file, 1500, "Read");
      await h.ready;
      let code = null;
      writeFileSync(`${file}.ctl.tmp`, "{}");
      try {
        af.renameRetry(`${file}.ctl.tmp`, file, { tries: 1 });
      } catch (e) {
        code = e.code;
      }
      check(["EPERM", "EBUSY", "EACCES"].includes(code), `without the retry, a rename over a held file throws (${code})`);
      rmSync(`${file}.ctl.tmp`, { force: true });
      await h.done;
    }

    // the fix: the write waits the reader out
    {
      const h = holdOpen(file, 700, "Read");
      await h.ready;
      const t0 = Date.now();
      let ok = true;
      try {
        sq.writeQueue(hub, { ...q0, seq: 2 });
      } catch (e) {
        ok = false;
        console.log(`      ${e.code} ${e.message}`);
      }
      const took = Date.now() - t0;
      check(ok, `writeQueue survives a reader holding the file open (EPERM), after ${took} ms`);
      await h.done;
      check(JSON.parse(readFileSync(file, "utf8")).seq === 2, "the new queue is on disk");
      check(!readdirSync(hub).some((f) => f.endsWith(".tmp")), "no .tmp file is left behind");
    }

    // a reader that cannot open the file: an error, never an empty queue for withQueue to write back
    {
      const h = holdOpen(file, 2500, "None");
      await h.ready;
      let r = null;
      let code = null;
      try {
        r = sq.readQueue(hub);
      } catch (e) {
        code = e.code;
      }
      check(r === null && ["EPERM", "EBUSY", "EACCES"].includes(code), `readQueue throws, not an empty queue, while the file is locked (${code ?? `got ${r?.entries?.length} entries`})`);
      await h.done;
      check(sq.readQueue(hub).seq === 2, "readQueue reads it again once the lock is gone");
    }

    // 3. update(): a bookkeeping write never throws, whatever the file does
    spawnSync("git", ["init", "-q", dir], { windowsHide: true });
    sq.writeQueue(hub, q0);
    {
      const logs = [];
      const h = holdOpen(file, 6000, "Read");
      await h.ready;
      let r;
      let threw = null;
      try {
        r = update(dir, "a-1", { state: "failed", why: "x" }, "failed: x", { attempts: 2, pauseMs: 1, log: (s) => logs.push(s) });
      } catch (e) {
        threw = e;
      }
      check(!threw && r === null, `update() does not throw when the write keeps failing${threw ? ` (threw ${threw.code})` : ""}`);
      check(logs.length === 1 && /gate result is kept/.test(logs[0]), `...it logs that the gate result is kept (${logs[0] ?? "no log"})`);
      h.child.kill();
      await h.done;
    }
    {
      const h = holdOpen(file, 600, "Read");
      await h.ready;
      const r = update(dir, "a-1", { state: "passed" }, "passed", { attempts: 3, pauseMs: 1, log: () => {} });
      check(r?.state === "passed", "update() lands after a short hold");
      await h.done;
      check(sq.readQueue(hub).entries[0].state === "passed", "...and the entry on disk says so");
    }
  }
} finally {
  rmSync(dir, { recursive: true, force: true });
}

// ---- 4. the other state files use the same retry
for (const f of ["lib/test-shards.mjs", "lib/test-cache.mjs", "lib/session-lease.mjs", "remote/pool.mjs", "visual/look-serve.mjs"]) {
  const s = readFileSync(new URL(`../${f}`, import.meta.url), "utf8");
  check(!/\brenameSync\(/.test(s) && /atomic-file\.mjs/.test(s), `${f} renames through atomic-file.mjs`);
}

console.log(failed ? `\n${failed} check(s) failed` : "\nall checks passed");
process.exit(failed ? 1 : 0);
