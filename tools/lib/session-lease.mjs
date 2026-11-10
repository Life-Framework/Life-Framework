// A session's lease: when its last harness command ran, and an optional
// explicit extension. The pool closes a session idle past the limit while
// others queue (tools/remote/pool.mjs reapIdleSessions). Kept in lease.json
// beside session.json so a harness command never rewrites the session record.

import { existsSync, mkdirSync, readFileSync, renameSync, rmSync, statSync, writeFileSync } from "node:fs";
import { basename, dirname, join, resolve } from "node:path";

export function sessionBase(env = process.env) {
  const base = env.LOCALAPPDATA || join(env.USERPROFILE || ".", "AppData", "Local");
  return join(base, "LifeFramework", "session");
}

// One dir per checkout (named after the checkout folder).
export function sessionDirOf(root, env = process.env) {
  if (env.LF_SESSION_DIR) return env.LF_SESSION_DIR;
  return join(sessionBase(env), basename(resolve(root)));
}

const leaseFile = (dir) => join(dir, "lease.json");

export function readLease(dir) {
  try {
    return JSON.parse(readFileSync(leaseFile(dir), "utf8"));
  } catch {
    return {};
  }
}

function writeLease(dir, lease) {
  try {
    mkdirSync(dir, { recursive: true });
    const f = leaseFile(dir);
    writeFileSync(`${f}.${process.pid}.tmp`, JSON.stringify(lease));
    renameSync(`${f}.${process.pid}.tmp`, f);
  } catch {}
}

// A harness or session command ran now. Never throws.
export function touchLease(dir, verb = "") {
  if (!dir || !existsSync(dir)) return;
  const lease = readLease(dir);
  writeLease(dir, { ...lease, lastCommandAt: new Date().toISOString(), lastVerb: String(verb).slice(0, 40) });
}

// `session.mjs lease [minutes]`: this session stays up for that long even idle.
export function extendLease(dir, minutes) {
  const lease = readLease(dir);
  const until = new Date(Date.now() + minutes * 60000).toISOString();
  writeLease(dir, { ...lease, leaseUntil: until, lastCommandAt: new Date().toISOString(), lastVerb: "lease" });
  return until;
}

// A client box is <session dir>/client<n>/profile/agent-box; null when it is not one.
export function sessionDirOfBox(box) {
  const profile = dirname(box);
  const client = dirname(profile);
  if (basename(box) !== "agent-box" || basename(profile) !== "profile" || !/^client\d+$/.test(basename(client))) return null;
  return dirname(client);
}

export function markReaped(dir, why) {
  try {
    writeFileSync(join(dir, "reaped.json"), JSON.stringify({ at: new Date().toISOString(), why }));
  } catch {}
}

export function readReaped(dir) {
  try {
    return JSON.parse(readFileSync(join(dir, "reaped.json"), "utf8"));
  } catch {
    return null;
  }
}

// When the session was last active, as epoch ms: the newest of its last
// command, its ready/start time, and the lock time (a session still coming up).
export function lastActiveMs(dir, lockAt) {
  let session = null;
  try {
    session = JSON.parse(readFileSync(join(dir, "session.json"), "utf8"));
  } catch {}
  const lease = readLease(dir);
  const t = [lease.lastCommandAt, session?.readyAt, session?.startedAt, lockAt].map((x) => Date.parse(x)).filter(Number.isFinite);
  // A harness of before the lease (no lease.json) still writes command.txt / result.txt in
  // each client's box on every command: their mtimes show a session is being driven.
  for (let n = 1; n <= 3; n++) {
    for (const f of ["command.txt", "result.txt"]) {
      try {
        t.push(statSync(join(dir, `client${n}`, "profile", "agent-box", f)).mtimeMs);
      } catch {}
    }
  }
  return t.length ? Math.max(...t) : NaN;
}

// A fresh `session up`: nothing of the previous session's lease or closure carries over.
export function resetLease(dir) {
  for (const f of ["lease.json", "reaped.json"]) {
    try {
      rmSync(join(dir, f), { force: true });
    } catch {}
  }
}
