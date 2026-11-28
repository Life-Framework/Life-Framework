// tools/lib/atomic-file.mjs - write a state file through a temp file and a rename, and read one back,
// without losing to a concurrent reader on Windows.
//
// renameSync over a file another process has open (a status poll, a virus scanner,
// the search indexer) throws EPERM, EBUSY or EACCES on Windows for as long as the other handle lives, which
// is a few milliseconds. The writers here retry that for about a second with a synchronous sleep; the
// readers retry the same codes on open. Anything else (ENOENT on the temp file, a full disk) throws at once.

import { readFileSync, renameSync, unlinkSync, writeFileSync } from "node:fs";

export const TRANSIENT = ["EPERM", "EBUSY", "EACCES"];

// Block the thread for ms without spinning (Atomics.wait on a private buffer).
export function sleepSync(ms) {
  Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, Math.max(1, ms));
}

// Run fn(); on a transient error (EPERM, EBUSY, EACCES) wait and run it again, up to `tries` times in all,
// the wait growing from `baseMs` (10, 20, 40 ... capped at `maxMs`). The last error is thrown.
export function retryTransient(fn, { tries = 10, baseMs = 10, maxMs = 200, sleep = sleepSync } = {}) {
  for (let i = 1; ; i++) {
    try {
      return fn();
    } catch (e) {
      if (!TRANSIENT.includes(e?.code) || i >= tries) throw e;
      sleep(Math.min(maxMs, baseMs * 2 ** (i - 1)));
    }
  }
}

// rename(from, to) that survives a reader holding `to` open.
export function renameRetry(from, to, opts) {
  return retryTransient(() => renameSync(from, to), opts);
}

// Write `data` to `file` whole: temp file, then renameRetry. The temp file is removed when the rename
// never lands, so a failed write leaves no `.tmp` behind. Throws when every try failed.
export function writeFileAtomic(file, data, { tmp = `${file}.${process.pid}.tmp`, encoding = "utf8", ...opts } = {}) {
  writeFileSync(tmp, data, encoding);
  try {
    renameRetry(tmp, file, opts);
  } catch (e) {
    try {
      unlinkSync(tmp);
    } catch {}
    throw e;
  }
}

// readFileSync that retries a transient open failure (a writer mid-rename on Windows).
export function readFileRetry(file, encoding = "utf8", opts) {
  return retryTransient(() => readFileSync(file, encoding), opts);
}
