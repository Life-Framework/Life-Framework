// tools/lib/wb-lock.mjs - the machine-wide limit on heavy headless Workbench steps (the
// validate-scripts compile, build, look). Each one takes one of `limit` lock files
// <hub>/locks/workbench-<k>.lock (wt.acquireLock: exclusive create, a dead holder pid is
// reclaimed). Waiters queue FIFO through tickets in <hub>/wb-queue: a waiter may take a free
// lock only while fewer older live tickets wait than locks are free. One status line while queued.
// The limit: "workbenchLimit" in tools/pool.json, overridden by the "pool" key of lf.local.json,
// then LF_WB_LIMIT; default 1 (seven parallel compiles exhausted a 15.6 GB PC on 2026-11-14).

import { mkdirSync, readdirSync, readFileSync, unlinkSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import * as wt from "../wt.mjs";
import { localConfig } from "./local-config.mjs";

export const DEFAULT_LIMIT = 1;
const POOL_JSON = join(dirname(fileURLToPath(import.meta.url)), "..", "pool.json");
const PREFIX = "workbench-";

function readJson(file, fallback) {
  try {
    return JSON.parse(readFileSync(file, "utf8"));
  } catch {
    return fallback;
  }
}

function pidAlive(pid) {
  if (!pid) return false;
  try {
    process.kill(pid, 0);
    return true;
  } catch (e) {
    return e.code === "EPERM";
  }
}

// Pure: env first, then lf.local.json pool, then pool.json, then the default.
export function pickLimit({ env, local, base }) {
  for (const v of [env, local, base]) {
    const n = Number(v);
    if (v !== undefined && v !== "" && v !== null && Number.isInteger(n) && n > 0) return n;
  }
  return DEFAULT_LIMIT;
}

export function wbLimit(root) {
  let local;
  try {
    local = localConfig(root).pool?.workbenchLimit;
  } catch {}
  return pickLimit({ env: process.env.LF_WB_LIMIT, local, base: readJson(POOL_JSON, {}).workbenchLimit });
}

export function lockNames(limit) {
  return Array.from({ length: limit }, (_, i) => `${PREFIX}${i + 1}`);
}

// Pure FIFO rule: `tickets` are the live tickets oldest first, `freeCount` the free locks.
// A ticket may take a lock when fewer older tickets wait than locks are free.
export function mayTake(tickets, myId, freeCount) {
  const pos = tickets.findIndex((t) => t.id === myId);
  const ahead = pos < 0 ? tickets.length : pos;
  return { ok: freeCount > 0 && ahead < freeCount, ahead };
}

function queueDir(root) {
  const d = join(wt.hubDirOf(root), "wb-queue");
  mkdirSync(d, { recursive: true });
  return d;
}

// Live tickets oldest first; tickets of dead pids are dropped.
export function liveTickets(root, alive = pidAlive) {
  const d = queueDir(root);
  const out = [];
  for (const f of readdirSync(d).filter((n) => n.endsWith(".json")).sort()) {
    const t = readJson(join(d, f), null);
    if (!t || !alive(t.pid)) {
      try {
        unlinkSync(join(d, f));
      } catch {}
      continue;
    }
    out.push({ ...t, id: f });
  }
  return out;
}

// The lock files: { name, info, alive } per configured lock (and any beyond the limit still held).
export function holders(root, limit = wbLimit(root), alive = pidAlive) {
  const dir = join(wt.ensureHub(root), "locks");
  const names = new Set(lockNames(limit));
  for (const f of readdirSync(dir)) if (f.startsWith(PREFIX) && f.endsWith(".lock")) names.add(f.slice(0, -5));
  return [...names].sort().map((name) => {
    const info = readJson(join(dir, `${name}.lock`), null);
    return { name, info, alive: !!info && alive(info.pid) };
  });
}

// This process already holds one (a build inside `ci --wb-only` that holds it for both steps).
let heldHere = 0;

// Sync and async share one step: { lock } when taken, else { ahead, busy }.
function tryTake(root, limit, myId, meta) {
  const hs = holders(root, limit).filter((h) => lockNames(limit).includes(h.name));
  const free = hs.filter((h) => !h.alive);
  const { ok, ahead } = mayTake(liveTickets(root), myId, free.length);
  if (ok) {
    for (const h of free) {
      try {
        wt.acquireLock(root, h.name, { waitMs: 0, meta });
        return { lock: h.name };
      } catch {}
    }
  }
  return { ahead, busy: hs.filter((h) => h.alive).map((h) => h.info) };
}

function describe(busy) {
  return busy.map((i) => `${i?.purpose ?? "?"} ${i?.slug ?? ""} pid ${i?.pid}`.replace(/\s+/g, " ").trim()).join("; ");
}

function begin(root, purpose) {
  const limit = wbLimit(root);
  const slug = String(root).replace(/\\/g, "/").split("/").pop();
  const meta = { purpose, slug, root };
  const id = `${Date.now().toString().padStart(15, "0")}-${process.pid}-${Math.random().toString(36).slice(2, 6)}.json`;
  const file = join(queueDir(root), id);
  writeFileSync(file, JSON.stringify({ pid: process.pid, purpose, slug, at: new Date().toISOString() }));
  return { limit, meta, id, file };
}

function finish(root, ctx, name, waitedMs, log) {
  try {
    unlinkSync(ctx.file);
  } catch {}
  heldHere++;
  if (waitedMs > 1000) log(`workbench: got ${name} after ${Math.round(waitedMs / 1000)} s`);
  let done = false;
  return {
    name,
    release: () => {
      if (done) return;
      done = true;
      heldHere--;
      wt.releaseLock(root, name);
    },
  };
}

function dropTicket(ctx) {
  try {
    unlinkSync(ctx.file);
  } catch {}
}

const NOOP = { name: null, release: () => {} };

// Blocks (async) until a Workbench lock is free here. waitMs bounds the queue (default 1 h).
export async function acquireWorkbench(root, { purpose = "workbench", waitMs = 3600000, pollMs = 2000, log = console.log } = {}) {
  if (heldHere > 0 || process.env.LF_WB_HELD === "1") return NOOP;
  const ctx = begin(root, purpose);
  const t0 = Date.now();
  let said = false;
  for (;;) {
    const r = tryTake(root, ctx.limit, ctx.id, { ...ctx.meta, pid: process.pid });
    if (r.lock) return finish(root, ctx, r.lock, Date.now() - t0, log);
    if (!said) {
      said = true;
      log(`workbench: queued for ${purpose} (limit ${ctx.limit} on this PC; ${r.ahead} ahead; busy: ${describe(r.busy) || "none"})`);
    }
    if (Date.now() - t0 > waitMs) {
      dropTicket(ctx);
      throw new Error(`workbench: no free Workbench slot within ${Math.round(waitMs / 1000)} s (busy: ${describe(r.busy)})`);
    }
    await new Promise((res) => setTimeout(res, pollMs));
  }
}

// Same, blocking the thread (for sync callers such as look).
export function acquireWorkbenchSync(root, { purpose = "workbench", waitMs = 3600000, pollMs = 2000, log = console.log } = {}) {
  if (heldHere > 0 || process.env.LF_WB_HELD === "1") return NOOP;
  const ctx = begin(root, purpose);
  const t0 = Date.now();
  let said = false;
  const cell = new Int32Array(new SharedArrayBuffer(4));
  for (;;) {
    const r = tryTake(root, ctx.limit, ctx.id, { ...ctx.meta, pid: process.pid });
    if (r.lock) return finish(root, ctx, r.lock, Date.now() - t0, log);
    if (!said) {
      said = true;
      log(`workbench: queued for ${purpose} (limit ${ctx.limit} on this PC; ${r.ahead} ahead; busy: ${describe(r.busy) || "none"})`);
    }
    if (Date.now() - t0 > waitMs) {
      dropTicket(ctx);
      throw new Error(`workbench: no free Workbench slot within ${Math.round(waitMs / 1000)} s (busy: ${describe(r.busy)})`);
    }
    Atomics.wait(cell, 0, 0, pollMs);
  }
}

// One line for `pool status`.
export function statusLine(root) {
  const limit = wbLimit(root);
  const hs = holders(root, limit).filter((h) => h.alive);
  const qd = liveTickets(root);
  const now = Date.now();
  const who = hs.map((h) => `${h.info.purpose ?? "?"} ${h.info.slug ?? ""} ${Math.round((now - Date.parse(h.info.at)) / 60000)}m`.replace(/\s+/g, " ")).join(", ");
  return `workbench (this PC): ${hs.length}/${limit} busy${who ? ` (${who})` : ""}${qd.length ? `, ${qd.length} queued` : ""}`;
}

export function isWbLockFile(name) {
  return name.startsWith(PREFIX);
}

