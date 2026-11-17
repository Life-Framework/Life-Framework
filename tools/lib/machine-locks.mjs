// tools/lib/machine-locks.mjs - where the machine-wide server slot locks (server-2001, server-test-<k>,
// server-<port>) live, so every process on one PC sees the same lock for the same UDP port.
//
// A lock is a file in <main checkout>/tmp/wt/locks. Normally one PC has one main checkout, so that is one
// directory. A PC that is ALSO a pool machine for another PC (the laptop: pool entry "lf-testbox" with
// "hostname": "PC1" and "root": "C:/LF/Life-Framework") has two main checkouts: the box clone that the other
// PC's remote holder writes its `server-2001.lock` into (tools/remote/pool.mjs), and the clone its own
// agents work from (C:\LF\agents\Life-Framework). Each took the "same" lock in its own directory, so a
// remote session and a local one were both granted UDP 2001 (2026-11-17: slice #2099's grid session got
// "Unable to start replication" one second after boot, beside slice #2104's session that had held the port
// since 00:34). The cure is one directory: the box clone's, which the remote side cannot move.
//
// machineLockDir(root) is that directory when this PC is a pool machine whose pool entry names a root other
// than this checkout's own, else null (the checkout's own hub, as before).
import { existsSync, readFileSync } from "node:fs";
import { hostname as osHostname } from "node:os";
import { join, resolve } from "node:path";

// The lock names that guard a UDP port; every other lock (build, wt, wb-queue ...) stays in the own hub.
export const MACHINE_WIDE = /^server-/;

export const isMachineWide = (name) => MACHINE_WIDE.test(String(name));

let hostnameOverride = null;
// pool.mjs setHostnameForTest passes its name through, so a selftest never depends on the PC it runs on.
export function setMachineHostnameForTest(name) {
  hostnameOverride = name ? String(name) : null;
}

const norm = (p) => resolve(String(p)).replace(/[\\/]+$/, "").toLowerCase();

function readJson(file) {
  try {
    return JSON.parse(readFileSync(file, "utf8").replace(/^\uFEFF/, ""));
  } catch {
    return null;
  }
}

// The pool entries as pool.mjs loadMachines merges them: tools/pool.json of the main checkout (or
// LF_POOL_JSON), then the "pool" key of the main checkout's lf.local.json, by name.
function poolEntries(mainRoot) {
  const poolFile = process.env.LF_POOL_JSON || join(mainRoot, "tools", "pool.json");
  const byName = new Map();
  for (const m of readJson(poolFile)?.machines ?? []) if (m?.name) byName.set(m.name, { ...m });
  for (const m of readJson(join(mainRoot, "lf.local.json"))?.pool?.machines ?? []) {
    if (m?.name) byName.set(m.name, { ...(byName.get(m.name) ?? {}), ...m });
  }
  return [...byName.values()];
}

// mainRoot: this checkout's main root (wt.mainRootOf). Returns the shared locks directory or null.
// opts: hostname, exists (test seams).
export function machineLockDir(mainRoot, opts = {}) {
  if (process.env.LF_MACHINE_LOCKS === "0") return null;
  const self = String(opts.hostname ?? hostnameOverride ?? osHostname()).toLowerCase();
  const exists = opts.exists ?? existsSync;
  const mine = poolEntries(mainRoot).find((m) => m.via === "ssh" && !m.disabled && m.hostname && String(m.hostname).toLowerCase() === self);
  if (!mine || !mine.root) return null;
  const box = String(mine.root).replace(/\\/g, "/").replace(/\/$/, "");
  if (norm(box) === norm(mainRoot)) return null;
  // The box clone must really be here; a pool entry that names a path this PC lacks changes nothing.
  if (!exists(join(box, ".git")) || !exists(join(box, "tmp", "wt"))) return null;
  return join(box, "tmp", "wt", "locks");
}
