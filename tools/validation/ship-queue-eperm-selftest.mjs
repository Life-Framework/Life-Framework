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
// No wall-clock hold: the holder keeps the file until the test signals it (a release file), and the signal comes from
// the writer's own retry hook (writeQueue's `sleep` option), i.e. once the rename was seen failing. The old 700 ms
// timer raced the ~1 s retry budget: under load the holder lived past it ("survives a reader ..., after 1230 ms").
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

// Holds `file` open with the given FileShare ("Read": others read, nobody writes or replaces it; "None": nobody
// opens it) until release() is called (or 60 s pass, a leak guard). `ready` resolves once the handle is held;
// release() lets go and returns once the handle is really closed; `done` resolves when the process has exited.
let holdN = 0;
function holdOpen(file, share) {
  const rel = join(dir, `hold-${++holdN}.rel`);
  const script = `$f=[System.IO.File]::Open($env:LF_HOLD,'Open','Read','${share}'); [Console]::Out.WriteLine('ready'); [Console]::Out.Flush(); $i=0; while(-not (Test-Path -LiteralPath $env:LF_REL) -and $i -lt 3000){Start-Sleep -Milliseconds 20; $i++}; $f.Close(); Set-Content -LiteralPath ($env:LF_REL + '.closed') -Value x`;
  const child = spawn("powershell.exe", ["-NoProfile", "-NonInteractive", "-Command", script], { env: { ...process.env, LF_HOLD: file, LF_REL: rel }, windowsHide: true, stdio: ["ignore", "pipe", "ignore"] });
  const done = new Promise((r) => child.on("exit", r));
  const ready = new Promise((res, rej) => {
    const t = setTimeout(() => rej(new Error("the holder never reported ready")), 60000);
    child.stdout.on("data", (d) => {
      if (String(d).includes("ready")) {
        clearTimeout(t);
        res();
      }
    });
    child.on("error", rej);
  });
  let released = false;
  // Synchronous on purpose: the writer's retry hook is sync (the writers block the thread with Atomics.wait).
  const release = () => {
    if (released) return;
    released = true;
    writeFileSync(rel, "go");
    for (let i = 0; i < 3000 && !existsSync(`${rel}.closed`); i++) af.sleepSync(20);
  };
  return { ready, done, child, release };
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
      const h = holdOpen(file, "Read");
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
      h.release();
      await h.done;
    }

    // the fix: the write waits the reader out. The reader lets go on the writer's first retry (so the rename
    // was seen failing), however slow the machine is.
    {
      const h = holdOpen(file, "Read");
      await h.ready;
      let retries = 0;
      let ok = true;
      const t0 = Date.now();
      try {
        sq.writeQueue(hub, { ...q0, seq: 2 }, { sleep: (ms) => { retries++; h.release(); af.sleepSync(ms); } });
      } catch (e) {
        ok = false;
        console.log(`      ${e.code} ${e.message}`);
      }
      const took = Date.now() - t0;
      check(ok && retries >= 1, `writeQueue survives a reader holding the file open (EPERM), after ${retries} retry and ${took} ms`);
      h.release();
      await h.done;
      check(JSON.parse(readFileSync(file, "utf8")).seq === 2, "the new queue is on disk");
      check(!readdirSync(hub).some((f) => f.endsWith(".tmp")), "no .tmp file is left behind");
    }

    // a reader that cannot open the file: an error, never an empty queue for withQueue to write back
    {
      const h = holdOpen(file, "None");
      await h.ready;
      let r = null;
      let code = null;
      try {
        r = sq.readQueue(hub);
      } catch (e) {
        code = e.code;
      }
      check(r === null && ["EPERM", "EBUSY", "EACCES"].includes(code), `readQueue throws, not an empty queue, while the file is locked (${code ?? `got ${r?.entries?.length} entries`})`);
      h.release();
      await h.done;
      check(sq.readQueue(hub).seq === 2, "readQueue reads it again once the lock is gone");
    }

    // 3. update(): a bookkeeping write never throws, whatever the file does
    spawnSync("git", ["init", "-q", dir], { windowsHide: true });
    sq.writeQueue(hub, q0);
    {
      const logs = [];
      const h = holdOpen(file, "Read");
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
      h.release();
      await h.done;
    }
    {
      // The first attempt fails for good (the reader holds past one write's whole retry budget); the reader lets
      // go in update()'s pause between attempts, so the second attempt lands. No timer decides the order.
      const h = holdOpen(file, "Read");
      await h.ready;
      let pauses = 0;
      const r = update(dir, "a-1", { state: "passed" }, "passed", { attempts: 3, pauseMs: 1, sleep: () => { pauses++; h.release(); }, log: () => {} });
      check(r?.state === "passed" && pauses === 1, `update() lands on its second attempt once the reader lets go (${pauses} pause)`);
      h.release();
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
