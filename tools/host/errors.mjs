// tools/host/errors.mjs - follow a live console.log and print only its errors (#1500).
//
// `tools\cli host errors` (tools/host/commands/errors.mjs) is the front end. This file is the
// logic, with the file, clock, sleep and output injected so the selftest drives it with a fake
// clock and a log grown in a few writes (tools/validation/host-errors-selftest.mjs).
//
// An error is what tools/lib/log-triage.mjs calls a line of interest, minus the warnings: an
// `[LFDebug:*]` error, a `SCRIPT (E)` line, or a VME. Each is marked known (a docs/debug-log-triage.md
// row matches, `matchKnown`) or new. The matcher is the one `host triage` uses; nothing is
// re-implemented here.

import { closeSync, existsSync, openSync, readSync, statSync } from "node:fs";
import { collectLines, matchKnown } from "../lib/log-triage.mjs";

export const DEFAULT_FOR_MIN = 60;
export const POLL_SEC = 2;
const MAX_LEN = 300;

/** `--for <min>` (default 60). Returns { forMin } or { error }. */
export function parseArgs(args) {
  const i = args.indexOf("--for");
  if (i < 0) return { forMin: DEFAULT_FOR_MIN };
  const raw = args[i + 1];
  const n = Number(raw);
  if (raw === undefined || raw === "" || !Number.isFinite(n) || n <= 0) {
    return { error: `--for needs a number of minutes above 0 (got ${raw === undefined ? "nothing" : raw})` };
  }
  return { forMin: n };
}

/** The error lines among complete log lines: [{ line, kind, body, known, verdict, row }]. */
export function errorLines(text, rows) {
  const out = [];
  for (const l of collectLines(text)) {
    if (l.kind === "LFDebug" && l.level !== "E") continue;
    const hit = matchKnown(l.body, rows);
    out.push({ line: l.line, kind: l.kind, body: l.body, known: !!hit, verdict: hit ? hit.row.verdict : "", row: hit ? hit.row.line : 0 });
  }
  return out;
}

/** One printed line: `KNOWN [FIXED] <text>` or `NEW   <text>`. */
export function formatError(e) {
  const body = e.body.length > MAX_LEN ? `${e.body.slice(0, MAX_LEN)}...` : e.body;
  return e.known ? `KNOWN [${e.verdict}] ${body}` : `NEW   ${body}`;
}

/** Bytes appended to `file` since `offset`; a file that shrank is read from its start. */
function readNew(file, offset) {
  if (!file || !existsSync(file)) return { chunk: Buffer.alloc(0), offset: 0, reset: offset > 0 };
  const size = statSync(file).size;
  const reset = size < offset;
  const from = reset ? 0 : offset;
  if (size === from) return { chunk: Buffer.alloc(0), offset: from, reset };
  const buf = Buffer.alloc(size - from);
  const fd = openSync(file, "r");
  try {
    const n = readSync(fd, buf, 0, buf.length, from);
    return { chunk: buf.subarray(0, n), offset: from + n, reset };
  } finally {
    closeSync(fd);
  }
}

/**
 * Follow `file` (a path, or a function returning the newest log path) for `forSec` seconds.
 * Each poll reads what was appended, holds back an unfinished last line until its newline arrives,
 * and calls `out(error)` once per error line. Returns { errors, known, fresh, reason: "span" }.
 */
export function follow({ file, rows, forSec, pollSec = POLL_SEC, out, sleep, now }) {
  const start = now();
  const totals = { errors: 0, known: 0, fresh: 0, reason: "span" };
  let current = null;
  let offset = 0;
  let pending = Buffer.alloc(0);
  for (;;) {
    const path = typeof file === "function" ? file() : file;
    if (path !== current) {
      current = path;
      offset = 0;
      pending = Buffer.alloc(0);
    }
    const r = readNew(current, offset);
    offset = r.offset;
    if (r.reset) pending = Buffer.alloc(0);
    if (r.chunk.length) {
      const all = Buffer.concat([pending, r.chunk]);
      const cut = all.lastIndexOf(0x0a) + 1;
      pending = all.subarray(cut);
      if (cut > 0) {
        for (const e of errorLines(all.subarray(0, cut).toString("utf8"), rows)) {
          totals.errors++;
          if (e.known) totals.known++;
          else totals.fresh++;
          out(e);
        }
      }
    }
    if (now() - start >= forSec * 1000) return totals;
    sleep(pollSec * 1000);
  }
}
