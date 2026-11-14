// tools/remote/pool.mjs - the test pool: every machine that can boot a test
// server, one slot per concurrent server, a fair queue across agents.
//
// Machines come from tools/pool.json, overridden per machine by the "pool" key
// of the gitignored lf.local.json (docs/setup.md "Machine-local settings").
// A slot is a lock: slot 1 is the machine's own server-2001 lock (the one
// serve/host/session already take), slot k > 1 is server-2001-s<k>. A machine
// that can run more servers at once (separate network namespaces, a VM per
// slot) raises "slots"; nothing else changes.
//
// acquireSlot() writes a ticket into <hub>/pool/queue, then loops: a ticket
// may try a machine only when no older live ticket is waiting for that machine
// (FIFO per machine), the local machine first, then every reachable remote.
// First free slot wins. Tickets and locks whose owner pid is gone are stolen.
//
// A remote slot is held by a "holder": one ssh session that creates the lock
// file on the remote machine (pid = the holder's PowerShell), prepares the
// remote run folder, then watches its own ancestor processes. When this
// process ends (normal exit, ctrl-c, crash) the local ssh dies, sshd drops the
// connection, the holder's ancestors vanish, and within ~2 s it stops every
// process still running out of the run folder and deletes the lock. A lock
// whose holder pid is dead is reclaimed by the next acquirer on that machine.

import { spawn, spawnSync } from "node:child_process";
import { randomBytes } from "node:crypto";
import { appendFileSync, existsSync, mkdirSync, readdirSync, readFileSync, renameSync, unlinkSync, writeFileSync } from "node:fs";
import { freemem, hostname, totalmem } from "node:os";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import * as wt from "../wt.mjs";
import * as q from "../lib/pool-queue.mjs";
import * as gc from "../lib/session-gc.mjs";
import { lastActiveMs, markReaped, noteHoldWarning, readLease, sessionBase, sessionDirOf } from "../lib/session-lease.mjs";
import { localConfig, localSetting } from "../lib/local-config.mjs";
import * as tb from "./testbox.mjs";
import { trimProcs } from "../lib/ws-trim.mjs";
import { statusLine } from "../lib/wb-lock.mjs";

const POOL_JSON = join(dirname(fileURLToPath(import.meta.url)), "..", "pool.json");
const HEALTH_TTL_MS = 60000;
const REMOTE_RETRY_MS = 4000;
export const SLOT_ENV = "LF_SERVER_SLOT_HELD";

export const slotLockName = q.slotLockName;
export const { lanesFor, laneKind, portForLock, normalizePriority } = q;

// Settings: pool.json top level, overridden by the "pool" key of lf.local.json,
// then by env (tests use LF_POOL_*): sessionIdleMin (a session idle this long
// while others queue is closed), promoteMin (a queued job moves up one
// priority level per this many minutes), minFreeMb (RAM floor for every server
// start), roomyMb (headroom past which speed, not RAM, picks a machine), sessionHoldMin / sessionGraceMin / sessionHoldMaxMin (the
// hold cap, below).
export function poolSettings(root) {
  const base = readJson(POOL_JSON, {});
  const over = localConfig(root).pool ?? {};
  const pick = (k, env) => {
    const e = process.env[env];
    const v = e !== undefined && e !== "" ? Number(e) : Number(over[k] ?? base[k]);
    return Number.isFinite(v) && v > 0 ? v : q.DEFAULTS[k];
  };
  return {
    sessionIdleMin: pick("sessionIdleMin", "LF_POOL_SESSION_IDLE_MIN"),
    sessionIdleMaxMin: pick("sessionIdleMaxMin", "LF_POOL_SESSION_IDLE_MAX_MIN"),
    promoteMin: pick("promoteMin", "LF_POOL_PROMOTE_MIN"),
    starveMin: pick("starveMin", "LF_POOL_STARVE_MIN"),
    minFreeMb: pick("minFreeMb", "LF_POOL_MIN_FREE_MB"),
    roomyMb: pick("roomyMb", "LF_POOL_ROOMY_MB"),
    bootBudgetS: pick("bootBudgetS", "LF_POOL_BOOT_BUDGET_S"),
    sessionHoldMin: pick("sessionHoldMin", "LF_POOL_SESSION_HOLD_MIN"),
    sessionGraceMin: pick("sessionGraceMin", "LF_POOL_SESSION_GRACE_MIN"),
    sessionHoldMaxMin: pick("sessionHoldMaxMin", "LF_POOL_SESSION_HOLD_MAX_MIN"),
    sessionKeepHours: pick("sessionKeepHours", "LF_POOL_SESSION_KEEP_H"),
    minFreeDiskGb: pick("minFreeDiskGb", "LF_POOL_MIN_FREE_DISK_GB"),
    sessionReserveServerMb: pick("sessionReserveServerMb", "LF_POOL_SESSION_RESERVE_SERVER_MB"),
    sessionReserveClientMb: pick("sessionReserveClientMb", "LF_POOL_SESSION_RESERVE_CLIENT_MB"),
    // Per profile: pool.json / lf.local.json "sessionReserve": { "lowspec": { "serverMb", "clientMb" } }.
    sessionReserve: { ...q.DEFAULTS.sessionReserve, ...(base.sessionReserve ?? {}), ...(over.sessionReserve ?? {}) },
  };
}

// Run and boot budgets (seconds) for a purpose on a machine: learned from
// runs.jsonl, seeded by defaults, times the machine's multiplier; --budget wins.
export function budgetsFor(root, m, purpose, override) {
  const settings = poolSettings(root);
  const b = q.budgetSeconds(readRuns(root), purpose, m.name, { multiplier: m.budgetMultiplier ?? 1, override: Number(override) });
  return { ...b, bootS: Math.ceil(settings.bootBudgetS * (m.budgetMultiplier ?? 1)) };
}

// The priority of a run: --priority, then LF_PRIORITY, then the worktree's
// recorded one (wt new --priority), then normal.
export function resolvePriority(root, flag) {
  const fromMarker = (() => {
    try {
      return wt.readMarker(root)?.priority;
    } catch {
      return null;
    }
  })();
  for (const v of [flag, process.env.LF_PRIORITY, fromMarker]) {
    const p = q.normalizePriority(v);
    if (p) return p;
  }
  return "normal";
}

// Every ssh to a pool machine is bounded: ConnectTimeout 4 s plus these overall limits.
export const PROBE_TIMEOUT_MS = 12000; // probe, lock read
export const HOLDER_FIRST_BYTE_MS = 40000; // no output at all from the holder
export const HOLDER_TIMEOUT_MS = 180000; // holder never reports a slot, busy, or error

// Queue poll interval; the selftest shortens it (LF_POOL_POLL_MS) so waits in it cost 0.1 s, not 1 s.
const pollMs = () => Number(process.env.LF_POOL_POLL_MS) || 1000;

function sleep(ms) {
  return new Promise((r) => setTimeout(r, ms));
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

function readJson(file, fallback) {
  try {
    return JSON.parse(readFileSync(file, "utf8").replace(/^\uFEFF/, ""));
  } catch {
    return fallback;
  }
}

// ---------------------------------------------------------------- reclaim rules

// A remote lock this PC's run took (origin = this host) whose local owner
// process is gone: the holder outlived it (Windows sshd can keep the session
// and the holder alive after the local ssh dies). Returns a reason or null.
export function remoteOwnerGone(info, { self = hostname(), alive = pidAlive } = {}) {
  if (!info || !info.origin || !info.originPid) return null;
  if (String(info.origin).toLowerCase() !== String(self).toLowerCase()) return null;
  if (alive(Number(info.originPid))) return null;
  return `owner pid ${info.originPid} on ${info.origin} is gone`;
}

// How long a pool run may hold a slot with no server process before the hang
// guard takes it: max(5 min, 10x the median run of that purpose).
const HANG_MIN_MS = 300000;
export function hangLimitMs(runs, purpose) {
  const d = runs
    .filter((r) => r.purpose === purpose && (r.result === "pass" || r.result === "fail"))
    .slice(-30)
    .map((r) => Number(r.durS) * 1000)
    .filter((x) => x > 0)
    .sort((a, b) => a - b);
  const median = d.length ? d[Math.floor(d.length / 2)] : 0;
  return Math.max(HANG_MIN_MS, 10 * median);
}

// A pool-taken lock (it names a purpose; serve/host/session locks do not)
// held past the limit while no server runs for it. Returns a reason or null.
export function hangReason(info, { now = Date.now(), limitMs, serverAlive }) {
  if (!info || !info.purpose || serverAlive !== false) return null;
  const age = now - Date.parse(info.at);
  if (!Number.isFinite(age) || age <= limitMs) return null;
  return `held ${Math.round(age / 60000)} min (limit ${Math.round(limitMs / 60000)} min) with no server process alive`;
}

function noticeFile(root, pid, lockName) {
  const d = join(poolDir(root), "reclaimed");
  mkdirSync(d, { recursive: true });
  return join(d, `${pid}-${lockName}.json`);
}

// Logs a reclaim in runs.jsonl (pool stats counts them) and leaves a notice
// the owner prints when it releases or loses the slot.
function noteReclaim(root, m, info, kind, reason) {
  const rec = {
    at: new Date().toISOString(),
    slug: info.slug ?? "?",
    purpose: "reclaim",
    victimPurpose: info.purpose ?? null,
    machine: m.name,
    slot: info.name ?? null,
    kind,
    reason,
    result: "reclaimed",
  };
  try {
    appendFileSync(runsFile(root), JSON.stringify(rec) + "\n");
  } catch {}
  const owner = m.via === "local" ? info.pid : String(info.origin ?? "").toLowerCase() === hostname().toLowerCase() ? info.originPid : null;
  if (owner) {
    try {
      writeFileSync(noticeFile(root, owner, info.name), JSON.stringify(rec));
    } catch {}
  }
  if (kind === "idle-session" || kind === "hold-cap") console.log(`pool: closed the session of ${info.slug ?? "another worktree"}: ${reason}; 'node agent-box/session.mjs up' starts a new one`);
  else console.log(`pool: reclaimed ${m.name} ${info.name} from ${info.slug ?? "?"} ${info.purpose ?? ""}: ${reason}`);
}

// The owner's side: print (once) why the pool took this process's slot.
export function takeNotice(root, lockName, pid = process.pid) {
  const f = noticeFile(root, pid, lockName);
  const n = readJson(f, null);
  if (!n) return null;
  try {
    unlinkSync(f);
  } catch {}
  console.log(`WARN  pool: the scheduler reclaimed this run's slot ${n.machine} ${lockName}: ${n.reason}`);
  return n;
}

export function poolDir(root) {
  const d = join(wt.hubDirOf(root), "pool");
  mkdirSync(join(d, "queue"), { recursive: true });
  return d;
}

// ---------------------------------------------------------------- registry

function normalize(m) {
  const via = m.via === "ssh" ? "ssh" : "local";
  return {
    name: String(m.name),
    via,
    host: via === "ssh" ? String(m.host || m.name) : null,
    root: via === "ssh" ? String(m.root || "C:/LF/Life-Framework").replace(/\\/g, "/").replace(/\/$/, "") : null,
    slots: Math.max(1, Number(m.slots) || 1),
    // lanes: { test: N, session: K } gives tests their own servers beside the session servers
    // (ports in pool-queue.mjs, "Lane ports"). Without it the machine keeps one shared lane (slots).
    // Scales the seeded run budgets for a slower machine (the laptop).
    hostname: m.hostname ? String(m.hostname) : null,
    budgetMultiplier: Number(m.budgetMultiplier) > 0 ? Number(m.budgetMultiplier) : 1,
    lanes: laneSetting(m),
    laneNotes: laneNotes(m),
    caps: Array.isArray(m.caps) ? m.caps.map(String) : ["server"],
    // Heat: a folder of CSV logs with a thrm_c column (the laptop's thermal logger). While its newest
    // reading is fresh and at or above thermalHoldC, no new job starts there.
    // The most game clients one session may run there (heat; none set means no cap).
    maxClients: Number(m.maxClients) > 0 ? Number(m.maxClients) : null,
    thermalLog: m.thermalLog ? String(m.thermalLog) : null,
    thermalHoldC: Number(m.thermalHoldC) > 0 ? Number(m.thermalHoldC) : 90,
    disabled: !!m.disabled,
  };
}

const isObj = (v) => !!v && typeof v === "object";

// The lane counts a machine runs: lanesV2 when set, else lanes. No ceiling but the port range
// (q.MAX_TEST_LANES / q.MAX_SESSION_LANES); the RAM gate and thermal hold decide what starts.
// lanesV2 exists for older pool code (before 2026-11-14), which reads only "lanes", caps its session
// lanes at 4 and puts a 10th test lane on 2011, the second session lane's port: keep "lanes" at
// most 9 test + 4 session and put the full count in "lanesV2".
export function laneSetting(m) {
  const src = isObj(m.lanesV2) ? m.lanesV2 : isObj(m.lanes) ? m.lanes : null;
  if (!src) return null;
  const n = (v, cap) => Math.max(1, Math.min(cap, Math.floor(Number(v)) || 1));
  return { test: n(src.test, q.MAX_TEST_LANES), session: n(src.session, q.MAX_SESSION_LANES) };
}

// Warnings about a machine's lane setting, printed by pool status.
export function laneNotes(m) {
  const out = [];
  if (isObj(m.lanes) && Number(m.lanes.test) > q.LEGACY_TEST_LANES) out.push(`lanes.test ${m.lanes.test} > ${q.LEGACY_TEST_LANES}: pool code older than 2026-11-14 puts test lane 10 on session port 2011; set lanes.test to ${q.LEGACY_TEST_LANES} or less and the full count in lanesV2`);
  const src = isObj(m.lanesV2) ? m.lanesV2 : isObj(m.lanes) ? m.lanes : null;
  if (src && Number(src.test) > q.MAX_TEST_LANES) out.push(`test lanes ${src.test} cut to ${q.MAX_TEST_LANES} (the lane port range)`);
  if (src && Number(src.session) > q.MAX_SESSION_LANES) out.push(`session lanes ${src.session} cut to ${q.MAX_SESSION_LANES} (the lane port range)`);
  return out;
}

// Committed registry, then lf.local.json "pool": { "machines": [ { "name", ... } ] }
// entries merge by name ({"name": "lf-testbox", "disabled": true} turns one off).
// LF_TESTBOX / LF_TESTBOX_ROOT keep working: they add or retarget that host.
export function loadMachines(root) {
  const base = readJson(POOL_JSON, { machines: [{ name: "local", via: "local", slots: 1 }] });
  const byName = new Map();
  for (const m of base.machines ?? []) byName.set(m.name, { ...m });
  const over = localConfig(root).pool;
  for (const m of over?.machines ?? []) {
    if (!m?.name) continue;
    byName.set(m.name, { ...(byName.get(m.name) ?? {}), ...m });
  }
  const box = localSetting(root, "LF_TESTBOX");
  if (box && ![...byName.values()].some((m) => m.via === "ssh" && (m.host || m.name) === box)) {
    byName.set(box, { name: box, via: "ssh", host: box, slots: 1, caps: ["server"] });
  }
  const boxRoot = localSetting(root, "LF_TESTBOX_ROOT");
  const out = [...byName.values()].map(normalize).filter((m) => !m.disabled);
  if (boxRoot) for (const m of out) if (m.via === "ssh" && m.host === (box || m.host)) m.root = boxRoot.replace(/\\/g, "/").replace(/\/$/, "");
  // On a machine that is itself a pool member (the box), its own ssh entry is
  // this machine: drop it so a run there never ssh-es to itself.
  const self = hostname().toLowerCase();
  // An entry whose "hostname" is this machine's is this machine: it becomes the local machine (its own
  // lanes and RAM gate; this PC's "local" entry would give the box this PC's lane config).
  const mine = out.find((m) => m.via === "ssh" && m.hostname && m.hostname.toLowerCase() === self);
  if (mine) return [{ ...mine, via: "local", host: null, root: null }, ...out.filter((m) => m.via === "ssh" && m !== mine && m.host.toLowerCase() !== self)];
  return out.filter((m) => !(m.via === "ssh" && m.host.toLowerCase() === self));
}

export function findMachine(machines, name) {
  const n = String(name).toLowerCase();
  return machines.find((m) => m.name.toLowerCase() === n || (m.host && m.host.toLowerCase() === n)) ?? null;
}

// ---------------------------------------------------------------- health

function healthFile(root) {
  return join(poolDir(root), "health.json");
}

// { reachable, build, at, ms }. Remote answers are cached for a minute in the
// hub, so a queue of waiting agents does not ssh a dead box every poll.
export function probe(root, m, { fresh = false } = {}) {
  if (m.via === "local") return { reachable: true, build: tb.serverBuildId(tb.localServerDir(root)), at: Date.now(), ms: 0 };
  const file = healthFile(root);
  const cache = readJson(file, {});
  const hit = cache[m.name];
  if (!fresh && hit && Date.now() - hit.at < HEALTH_TTL_MS) return hit;
  const t0 = Date.now();
  const res = tb.sshPs(m.host, [`$main = ${tb.psQuote(m.root)}`, tb.PS_BUILDID, "Write-Host 'lf-pool: ok'"].join("\n"), { timeout: PROBE_TIMEOUT_MS, connectTimeout: 4 });
  const out = res.stdout ?? "";
  const h = { reachable: /lf-pool: ok/.test(out), build: /lf-remote: buildid (\d+)/.exec(out)?.[1] ?? null, at: Date.now(), ms: Date.now() - t0 };
  const now = readJson(file, {});
  now[m.name] = h;
  try {
    writeFileSync(file, JSON.stringify(now, null, 2));
  } catch {}
  return h;
}

function markHealth(root, m, h) {
  const hf = healthFile(root);
  const hc = readJson(hf, {});
  const prev = hc[m.name] ?? {};
  hc[m.name] = { ...prev, ...h, build: h.build ?? (h.reachable ? prev.build : null) ?? null, at: Date.now() };
  try {
    writeFileSync(hf, JSON.stringify(hc, null, 2));
  } catch {}
}

// ---------------------------------------------------------------- queue

function queueDir(root) {
  return join(poolDir(root), "queue");
}

export function liveTickets(root) {
  const dir = queueDir(root);
  const out = [];
  for (const f of readdirSync(dir).filter((x) => x.endsWith(".json")).sort()) {
    const t = readJson(join(dir, f), null);
    if (!t || !pidAlive(t.pid)) {
      try {
        unlinkSync(join(dir, f));
      } catch {}
      continue;
    }
    out.push({ ...t, file: f });
  }
  return out;
}

export function writeTicket(root, t, atMs = Date.now()) {
  const f = `${String(atMs).padStart(15, "0")}-${String(process.pid).padStart(7, "0")}-${randomBytes(3).toString("hex")}.json`;
  writeFileSync(join(queueDir(root), f), JSON.stringify(t));
  return f;
}

export function dropTicket(root, f) {
  try {
    unlinkSync(join(queueDir(root), f));
  } catch {}
}

// ---------------------------------------------------------------- local slots

function localLockFile(root, name) {
  return join(wt.hubDirOf(root), "locks", `${name}.lock`);
}

// Servers a dead lock owner left behind still hold UDP 2001; stop the ones
// running out of that owner's worktree before the slot is handed on.
function stopLocalOrphans(ownerRoot) {
  if (!ownerRoot) return;
  const ps = `$a = ${tb.psQuote(join(ownerRoot, "server", "profile").replace(/\//g, "\\"))}; Get-CimInstance Win32_Process -Filter "Name LIKE 'ArmaReforgerServer%'" | Where-Object { $_.CommandLine -and $_.CommandLine.IndexOf($a, [StringComparison]::OrdinalIgnoreCase) -ge 0 } | ForEach-Object { Write-Output \"pool: stopping orphan server pid $($_.ProcessId)\"; taskkill /T /F /PID $_.ProcessId | Out-Null }`;
  const r = spawnSync("powershell", ["-NoProfile", "-NonInteractive", "-Command", ps], { windowsHide: true, encoding: "utf8", timeout: 30000 });
  if (r.stdout?.trim()) console.log(r.stdout.trim());
}

// Whether an ArmaReforgerServer runs out of that worktree's server/profile.
function localServerAlive(ownerRoot) {
  if (!ownerRoot) return false;
  const ps = `$a = ${tb.psQuote(join(ownerRoot, "server", "profile").replace(/\//g, "\\"))}; $n = @(Get-CimInstance Win32_Process -Filter "Name LIKE 'ArmaReforgerServer%'" | Where-Object { $_.CommandLine -and $_.CommandLine.IndexOf($a, [StringComparison]::OrdinalIgnoreCase) -ge 0 }).Count; Write-Output "lf-pool: servers $n"`;
  const r = spawnSync("powershell", ["-NoProfile", "-NonInteractive", "-Command", ps], { windowsHide: true, encoding: "utf8", timeout: 30000 });
  const mm = /lf-pool: servers (\d+)/.exec(r.stdout ?? "");
  // No answer counts as alive: the guard never takes a slot on a guess.
  return mm ? Number(mm[1]) > 0 : true;
}

// Servers running on this PC under a pool lane lock (live owner pid).
function runningLocalServers(root, m) {
  let n = 0;
  for (const name of q.allLaneLocks(m)) {
    const info = readJson(localLockFile(root, name), null);
    if (info && pidAlive(info.pid)) n++;
  }
  return n;
}

// The pids a live session recorded (server and clients), for the working-set trim.
export function sessionPids(s) {
  if (!s || s.stoppedAt) return [];
  return [s.serverPid, ...(s.clients ?? []).map((c) => c.pid)].filter((p) => Number.isInteger(p) && p > 0 && pidAlive(p));
}

let lastTrimAt = 0;

// The RAM gate is about to refuse: trim the working sets of every live session's processes (recorded
// pids only). A session waiting for its next harness command holds about 1.8 GB resident and needs
// ~0.13 GB of it (docs/worktrees.md "Session profiles"); the pages go to the standby list, which
// counts as free to the gate, and fault back when the session works again. At most once per 20 s.
// Returns how many processes were trimmed.
export function trimLiveSessions(root, m, opts = {}) {
  if (Date.now() - lastTrimAt < 20000) return 0;
  const pids = [];
  for (const l of lanesFor(m, "session")) {
    const info = readJson(localLockFile(root, l.lock), null);
    if (!info || !pidAlive(info.pid) || info.purpose !== "session") continue;
    const dir = sessionDirOf(info.root ?? root, { LOCALAPPDATA: process.env.LOCALAPPDATA, USERPROFILE: process.env.USERPROFILE });
    pids.push(...sessionPids(readJson(join(dir, "session.json"), null)));
  }
  if (!pids.length) return 0;
  lastTrimAt = Date.now();
  (opts.trim ?? trimProcs)(pids);
  console.log(`pool: RAM is short; trimmed the working sets of ${pids.length} session process(es) (their pages stay cached, free to the gate)`);
  Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, 2500);
  return pids.length;
}

// For a start that bypasses the gate (a late client): trims the live sessions when free RAM is under needMb.
export function trimIfShort(root, needMb) {
  const m = loadMachines(root).find((x) => x.via === "local");
  if (!m || Math.round(freemem() / 1048576) >= needMb) return 0;
  return trimLiveSessions(root, m);
}

// `session join` RAM gate (#563), like `up`: when the client would breach the floor, trim idle
// sessions first, then re-check once (the trim already pauses 2.5 s). Still short: refuse with a
// one-line reason. opts.freeMb / opts.trim are test seams. Returns { ok, reason?, trimmed }.
export function gateJoin(root, clientMb, opts = {}) {
  const minFreeMb = poolSettings(root).minFreeMb;
  const free = () => (opts.freeMb ? opts.freeMb() : Math.round(freemem() / 1048576));
  let v = q.joinVerdict({ freeMb: free(), minFreeMb, clientMb });
  if (v.ok) return { ...v, trimmed: 0 };
  const trimmed = (opts.trim ?? ((need) => trimIfShort(root, need)))(v.needMb) || 0;
  v = q.joinVerdict({ freeMb: free(), minFreeMb, clientMb });
  return { ...v, trimmed };
}

// The other session lanes of this PC with a live holder: { lock, pendingMb } where pendingMb is the
// memory a session still booting is expected to take that free RAM does not show yet (0 once it
// is ready, or after ten minutes).
function otherSessionLanes(root, m, except) {
  const out = [];
  for (const l of lanesFor(m, "session")) {
    if (l.lock === except) continue;
    const info = readJson(localLockFile(root, l.lock), null);
    if (!info || !pidAlive(info.pid) || info.purpose !== "session") continue;
    let pendingMb = 0;
    if (Date.now() - Date.parse(info.at) < 600000) {
      const dir = sessionDirOf(info.root ?? root, { LOCALAPPDATA: process.env.LOCALAPPDATA, USERPROFILE: process.env.USERPROFILE });
      const s = readJson(join(dir, "session.json"), null);
      if (!s || (!s.readyAt && !s.stoppedAt)) pendingMb = q.sessionReserveMb(poolSettings(root), s?.expectClients ?? info.clients ?? 1, s?.profile ?? info.profile ?? "full");
    }
    out.push({ lock: l.lock, pendingMb });
  }
  return out;
}

// The slot this process holds on this PC (set by tryLocal, cleared on release):
// cli.mjs reads the lane's port from it when it boots the server.
let heldHere = null;
export function heldLocalSlot() {
  return heldHere;
}

// opts.serverAlive / opts.runs / opts.freeMb are test seams; opts.blocked
// receives "ram" when the RAM gate (not a lock) held the slot back.
// `lane` is { lock, index, port } or a 1-based slot number (legacy shared lane).
export function tryLocal(root, m, lane, meta, opts = {}) {
  if (typeof lane === "number") lane = { lock: slotLockName(lane), index: lane, port: q.SESSION_PORT };
  const i = lane.index;
  const name = lane.lock;
  const file = localLockFile(root, name);
  if (existsSync(file)) {
    const info = readJson(file, null);
    if (!info) return null;
    if (pidAlive(info.pid)) {
      // Hang guard: the cheap age check first, the process scan only past it.
      const limitMs = hangLimitMs(opts.runs ?? readRuns(root), info.purpose);
      // Sessions are long by design: the lease reaper (reapIdleSessions) judges them, not the hang guard.
      if (!info.purpose || info.purpose === "session" || !(Date.now() - Date.parse(info.at) > limitMs)) return null;
      const serverAlive = (opts.serverAlive ?? localServerAlive)(info.root);
      const reason = hangReason(info, { limitMs, serverAlive });
      if (!reason) return null;
      noteReclaim(root, m, { ...info, name }, "hang", reason);
      try {
        unlinkSync(file);
      } catch {}
    } else stopLocalOrphans(info.root);
  }
  const settings = poolSettings(root);
  // A session reserves its own memory (the first one on the machine too: it must fit what is free),
  // and every start counts the sessions still booting, whose memory free RAM does not show yet.
  const others = otherSessionLanes(root, m, name);
  const reserveMb = meta?.purpose === "session" ? q.sessionReserveMb(settings, meta.clients ?? 1, meta.profile ?? "full") : 0;
  const pendingMb = others.reduce((n, s) => n + s.pendingMb, 0);
  const verdict = () => q.ramVerdict({ running: runningLocalServers(root, m), freeMb: opts.freeMb ?? Math.round(freemem() / 1048576), minFreeMb: settings.minFreeMb, reserveMb, pendingMb });
  let ram = opts.noRam ? { ok: true } : verdict();
  // Before refusing for RAM, trim what idle sessions hold resident and look again (not under test seams).
  if (!ram.ok && opts.freeMb === undefined && !opts.noTrim && trimLiveSessions(root, m) > 0) ram = verdict();
  if (!ram.ok) {
    if (opts.blocked) {
      opts.blocked.ram = ram.reason;
      opts.blocked.needMb = ram.needMb;
    }
    return null;
  }
  try {
    wt.acquireLock(root, name, { waitMs: 0, meta });
  } catch {
    return null;
  }
  const slot = {
    machine: m,
    index: i,
    lockName: name,
    port: lane.port,
    local: true,
    release: () => {
      if (heldHere === slot) heldHere = null;
      takeNotice(root, name);
      wt.releaseLock(root, name);
    },
  };
  if (!opts.noHeld) heldHere = slot;
  return slot;
}

// Inside a remote run the holder already owns the box's slot: the env names
// it and its token; the lock file must still carry that token.
export function heldSlotFromEnv(root) {
  const v = process.env[SLOT_ENV];
  if (!v) return null;
  const [name, token] = v.split(":");
  const info = readJson(localLockFile(root, name), null);
  if (!info || info.token !== token || !pidAlive(info.pid)) {
    throw new Error(`${SLOT_ENV}=${name}: the pool holder no longer owns that lock (released or reclaimed)`);
  }
  return { machine: { name: "local", via: "local" }, index: 1, lockName: name, port: q.portForLock(name), local: true, inherited: true, release: () => {} };
}

// ---------------------------------------------------------------- remote slots

export function remoteRunDir(m, slug, i = 1, suffix = "") {
  return `${dirname(m.root).replace(/\\/g, "/")}/Life-Framework-rt-${slug}${suffix}${i > 1 ? `-s${i}` : ""}`;
}

// The holder: acquire (when cfg.slots is non-empty), prepare the run folder,
// report what the folder already has, then hold until its ssh session ends.
// Stops processes whose command line names a run folder (never deletes files,
// so a junction under it is never followed).
const PS_STOP_UNDER = String.raw`
function Stop-Under($dir) {
  if (-not $dir) { return }
  $a = $dir.Replace('/', '\').TrimEnd('\') + '\'
  $b = $a.Replace('\', '/')
  Get-CimInstance Win32_Process | Where-Object { $_.ProcessId -ne $PID -and $_.CommandLine -and ($_.CommandLine.IndexOf($a, [StringComparison]::OrdinalIgnoreCase) -ge 0 -or $_.CommandLine.IndexOf($b, [StringComparison]::OrdinalIgnoreCase) -ge 0) } | ForEach-Object { Write-Host "lf-pool: stopping leftover $($_.Name) pid $($_.ProcessId)"; & taskkill /T /F /PID $_.ProcessId 2>&1 | Out-Null }
}
function Test-Server($dir) {
  if (-not $dir) { return $false }
  $a = $dir.Replace('/', '\').TrimEnd('\') + '\'
  $b = $a.Replace('\', '/')
  return [bool](@(Get-CimInstance Win32_Process -Filter "Name LIKE 'ArmaReforgerServer%'" | Where-Object { $_.CommandLine -and ($_.CommandLine.IndexOf($a, [StringComparison]::OrdinalIgnoreCase) -ge 0 -or $_.CommandLine.IndexOf($b, [StringComparison]::OrdinalIgnoreCase) -ge 0) }).Count)
}
`;

// Sets $thermC to the newest thrm_c reading of the CSV logs in $thermDir when the newest file was
// written in the last two minutes, else $null (no logger, a stale log, or no such column).
export const PS_THERMAL = String.raw`
$thermC = $null
if ($thermDir) {
  $tf = Get-ChildItem -LiteralPath $thermDir -Filter '*.csv' -ErrorAction SilentlyContinue | Sort-Object LastWriteTime | Select-Object -Last 1
  if ($tf -and ((Get-Date) - $tf.LastWriteTime).TotalSeconds -lt 120) {
    $hdr = (Get-Content -LiteralPath $tf.FullName -TotalCount 1).Split(',')
    $k = [array]::IndexOf($hdr, 'thrm_c')
    $last = Get-Content -LiteralPath $tf.FullName -Tail 1
    if ($k -ge 0 -and $last) { try { $thermC = [double]::Parse($last.Split(',')[$k], [Globalization.CultureInfo]::InvariantCulture) } catch {} }
  }
}
`;

// The holder's RAM gate, before it takes a lock: the verdict of q.ramVerdict on the box's live free
// RAM, its live lane locks and its sessions still booting (younger than 3 min). Needs $cfg, $ld,
// $slots and Test-Alive. Exported for the selftest.
export const PS_RAM_GATE = String.raw`
if ($cfg.ram -and $slots.Count -gt 0) {
  $os = Get-CimInstance Win32_OperatingSystem
  $free = [int]($os.FreePhysicalMemory / 1024)
  $running = 0; $pending = 0
  foreach ($n in @($cfg.ram.locks)) {
    $f = Join-Path $ld ($n + '.lock')
    if (-not (Test-Path -LiteralPath $f)) { continue }
    try { $h = [IO.File]::ReadAllText($f) | ConvertFrom-Json } catch { continue }
    if (-not (Test-Alive $h.pid)) { continue }
    $running++
    if ($h.purpose -eq 'session') { try { if (((Get-Date) - [datetime]$h.at).TotalSeconds -lt 180) { $c = 1; if ($h.clients) { $c = [int]$h.clients }; $pending += $cfg.ram.serverMb + $cfg.ram.clientMb * $c } } catch {} }
  }
  $need = $cfg.ram.reserveMb + $pending + $cfg.ram.floorMb
  # Only a lane that is free (or stale) makes RAM the reason; all lanes taken is plain busy.
  $anyFree = $false
  foreach ($s in $slots) { $h = $null; try { $h = [IO.File]::ReadAllText((Join-Path $ld ($s.lock + '.lock'))) | ConvertFrom-Json } catch {}; if (-not $h -or -not (Test-Alive $h.pid)) { $anyFree = $true } }
  if ($anyFree -and $free -lt $need) { Write-Host "lf-pool: ram $free $need $running"; exit 0 }
  if ($anyFree -and $cfg.ram.thermal) {
    $thermDir = $cfg.ram.thermal.dir
${PS_THERMAL}
    if ($thermC -ne $null -and $thermC -ge $cfg.ram.thermal.holdC) { Write-Host ("lf-pool: hot " + $thermC.ToString([Globalization.CultureInfo]::InvariantCulture)); exit 0 }
  }
}
`;

const PS_HOLDER = String.raw`
$cfg = $cfgJson | ConvertFrom-Json
$main = $cfg.main
$ld = Join-Path $main 'tmp\wt\locks'
New-Item -ItemType Directory -Force -Path $ld | Out-Null
function Test-Alive($p) { if (-not $p) { return $false }; return [bool](Get-Process -Id $p -ErrorAction SilentlyContinue) }
${PS_STOP_UNDER}
$enc = New-Object System.Text.UTF8Encoding($false)
$got = -1
$slots = @($cfg.slots)
${PS_RAM_GATE}
for ($i = 0; $i -lt $slots.Count -and $got -lt 0; $i++) {
  $s = $slots[$i]
  $f = Join-Path $ld ($s.lock + '.lock')
  for ($try = 0; $try -lt 3; $try++) {
    $fs = $null
    try { $fs = [IO.File]::Open($f, [IO.FileMode]::CreateNew, [IO.FileAccess]::Write) } catch { $fs = $null }
    if ($fs) {
      $info = [ordered]@{ pid = $PID; at = (Get-Date).ToUniversalTime().ToString('o'); name = $s.lock; token = $cfg.token; rDir = $s.dir; slug = $cfg.slug; purpose = $cfg.purpose; origin = $cfg.origin; originPid = $cfg.originPid; root = $s.dir }
      $bytes = $enc.GetBytes(($info | ConvertTo-Json -Compress))
      $fs.Write($bytes, 0, $bytes.Length); $fs.Close()
      $got = $i; break
    }
    $raw = $null; try { $raw = [IO.File]::ReadAllText($f) } catch {}
    $held = $null; if ($raw) { try { $held = $raw | ConvertFrom-Json } catch {} }
    if ($held -and (Test-Alive $held.pid)) { Write-Host ('lf-pool: holder ' + $raw.Trim()); Write-Host ('lf-pool: holder-srv ' + (Test-Server $held.rDir)); break }
    if (-not $held) { $age = 0; try { $age = ((Get-Date) - (Get-Item -LiteralPath $f -ErrorAction Stop).LastWriteTime).TotalSeconds } catch { continue }; if ($age -lt 60) { Write-Host 'lf-pool: holder {}'; break } }
    if ($held) { Write-Host "lf-pool: reclaiming stale $($s.lock) (pid $($held.pid) gone)"; Stop-Under $held.rDir }
    Remove-Item -LiteralPath $f -Force -ErrorAction SilentlyContinue
  }
}
if ($slots.Count -gt 0 -and $got -lt 0) { Write-Host 'lf-pool: busy'; exit 0 }
$dir = $cfg.dir
$lockFile = $null
if ($got -ge 0) { $dir = $slots[$got].dir; $lockFile = Join-Path $ld ($slots[$got].lock + '.lock'); Write-Host "lf-pool: acquired $($slots[$got].lock) $($got + 1)" }
try {
  if (-not (Test-Path -LiteralPath (Join-Path $main '.git'))) { Write-Host "lf-pool: error no clone at $main (tools\cli testbox provision)"; return }
  git -C $main cat-file -e "$($cfg.base)^{commit}" 2>$null
  if ($LASTEXITCODE) { git -C $main fetch -q origin 2>&1 | Out-Null; Write-Host 'lf-pool: fetched origin' }
  if (-not (Test-Path -LiteralPath (Join-Path $dir '.git'))) {
    git -C $main worktree add -q --detach $dir $cfg.base 2>&1 | Out-Null
    if ($LASTEXITCODE) { Write-Host "lf-pool: error worktree add $dir failed"; return }
    Write-Host 'lf-pool: new run folder'
  }
  New-Item -ItemType Directory -Force -Path (Join-Path $dir 'tmp\remote-in') | Out-Null
  foreach ($c in @($cfg.candidates)) { if ($c) { git -C $dir cat-file -e "$c^{commit}" 2>$null; if (-not $LASTEXITCODE) { Write-Host "lf-pool: has $c" } } }
  foreach ($ad in @(Get-ChildItem -LiteralPath (Join-Path $dir 'addons') -Directory -ErrorAction SilentlyContinue)) {
    $rdb = Join-Path $ad.FullName 'resourceDatabase.rdb'
    if (Test-Path -LiteralPath $rdb) { Write-Host "lf-pool: rdb $($ad.Name) $((Get-FileHash -Algorithm SHA1 -LiteralPath $rdb).Hash.ToLower())" }
  }
  $marker = [ordered]@{ origin = $cfg.origin; slug = $cfg.slug; worktree = $cfg.worktree; at = (Get-Date).ToUniversalTime().ToString('o') }
  [IO.File]::WriteAllText((Join-Path $dir 'tmp\pool-owner.json'), ($marker | ConvertTo-Json -Compress), $enc)
  Write-Host 'lf-pool: ready'
  if ($got -lt 0) { return }
  $anc = @(); $p = $PID
  for ($k = 0; $k -lt 6; $k++) {
    $x = Get-CimInstance Win32_Process -Filter "ProcessId=$p"
    if (-not $x) { break }
    $pp = [int]$x.ParentProcessId
    $px = Get-Process -Id $pp -ErrorAction SilentlyContinue
    if (-not $px) { break }
    $anc += [pscustomobject]@{ Id = $pp; Start = $px.StartTime }
    if ($px.ProcessName -like 'sshd*') { break }
    $p = $pp
  }
  while ($true) {
    Start-Sleep -Seconds 2
    $gone = @($anc | Where-Object { $q = Get-Process -Id $_.Id -ErrorAction SilentlyContinue; (-not $q) -or ($q.StartTime -ne $_.Start) })
    if ($gone.Count -gt 0) { break }
    try { [Console]::Out.WriteLine('lf-pool: hb'); [Console]::Out.Flush() } catch { break }
  }
} finally {
  if ($lockFile) {
    Stop-Under $dir
    try { $h = [IO.File]::ReadAllText($lockFile) | ConvertFrom-Json; if ($h.token -eq $cfg.token) { Remove-Item -LiteralPath $lockFile -Force } } catch {}
  }
}
`;

export function holderScript(cfg) {
  return `$cfgJson = @'\n${JSON.stringify(cfg)}\n'@\n$main = ${tb.psQuote(cfg.main)}\n${tb.PS_BUILDID}\n${PS_HOLDER}`;
}

const liveHolders = new Set();
let exitHooked = false;
function hookExit() {
  if (exitHooked) return;
  exitHooked = true;
  // Killing the local ssh ends the remote session; the holder notices and frees the slot.
  process.on("exit", () => {
    for (const c of liveHolders) {
      try {
        c.kill();
      } catch {}
    }
  });
  for (const sig of ["SIGINT", "SIGTERM", "SIGBREAK", "SIGHUP"]) {
    process.on(sig, () => {
      console.log(`\npool: ${sig}; releasing slots`);
      process.exit(130);
    });
  }
}

// Starts a holder. Resolves { slot } once it holds a slot and the run folder
// is ready, { busy: [holder json...] } when every slot there is taken, or
// { error } when the machine could not be used.
export function startHolder(root, m, cfg, { onLost } = {}) {
  hookExit();
  return new Promise((resolvePromise) => {
    const child = spawn("ssh", tb.sshPsArgs(m.host, holderScript(cfg), { connectTimeout: 4 }), { windowsHide: true, stdio: ["pipe", "pipe", "pipe"] });
    liveHolders.add(child);
    const prep = { has: new Set(), rdbs: {}, fetched: false, fresh: false };
    const busy = [];
    let acquired = null;
    let settled = false;
    let buf = "";
    let errText = "";
    const done = (v) => {
      if (settled) return;
      settled = true;
      clearTimeout(timer);
      clearTimeout(silent);
      resolvePromise(v);
    };
    const timer = setTimeout(() => {
      child.kill();
      done({ error: `${m.name}: no answer from the holder within ${HOLDER_TIMEOUT_MS / 1000} s` });
    }, HOLDER_TIMEOUT_MS);
    // A box that takes the connection and then says nothing is as good as down.
    const silent = setTimeout(() => {
      child.kill();
      done({ error: `${m.name}: ssh connected but nothing came back within ${HOLDER_FIRST_BYTE_MS / 1000} s` });
    }, HOLDER_FIRST_BYTE_MS);
    const onLine = (line) => {
      clearTimeout(silent);
      const t = line.trim();
      let mm;
      if ((mm = /^lf-pool: acquired (\S+) (\d+)/.exec(t))) acquired = { lockName: mm[1], index: Number(mm[2]) };
      else if ((mm = /^lf-pool: has ([0-9a-f]{40})/.exec(t))) prep.has.add(mm[1]);
      else if ((mm = /^lf-pool: rdb (\S+) ([0-9a-f]{40})/.exec(t))) prep.rdbs[mm[1]] = mm[2];
      else if (t === "lf-pool: fetched origin") prep.fetched = true;
      else if ((mm = /^lf-remote: buildid (\d+)/.exec(t))) prep.build = mm[1];
      else if (t === "lf-pool: new run folder") prep.fresh = true;
      else if ((mm = /^lf-pool: holder-srv (True|False)$/.exec(t))) {
        if (busy.length) busy[busy.length - 1].serverAlive = mm[1] === "True";
      } else if ((mm = /^lf-pool: holder (.*)$/.exec(t))) busy.push(readJsonText(mm[1]));
      else if (t === "lf-pool: busy") done({ busy });
      else if ((mm = /^lf-pool: hot ([\d.]+)$/.exec(t))) done({ hot: Number(mm[1]) });
      else if ((mm = /^lf-pool: ram (\d+) (\d+) (\d+)$/.exec(t))) done({ ram: { freeMb: Number(mm[1]), needMb: Number(mm[2]), running: Number(mm[3]) } });
      else if ((mm = /^lf-pool: error (.*)$/.exec(t))) done({ error: `${m.name}: ${mm[1]}` });
      else if (/^lf-pool: (reclaiming|stopping)/.test(t)) console.log(`pool: ${m.name}: ${t.slice(9)}`);
      else if (t === "lf-pool: ready") {
        const dir = acquired ? cfg.slots[acquired.index - 1].dir : cfg.dir;
        const slot = {
          machine: m,
          index: acquired?.index ?? 0,
          lockName: acquired?.lockName ?? null,
          local: false,
          token: cfg.token,
          rDir: dir,
          prep,
          child,
          lost: false,
          release: () => {
            liveHolders.delete(child);
            try {
              child.kill();
            } catch {}
          },
        };
        done({ slot });
        if (!acquired) slot.release();
        else child.on("close", () => {
          if (liveHolders.has(child)) {
            slot.lost = true;
            takeNotice(root, acquired.lockName);
            onLost?.(slot);
          }
        });
      } else if (t && t !== "lf-pool: hb" && !settled) errText += `${t}\n`;
    };
    const feed = (chunk) => {
      buf += chunk.toString("utf8");
      let nl;
      while ((nl = buf.indexOf("\n")) >= 0) {
        onLine(buf.slice(0, nl).replace(/\r$/, ""));
        buf = buf.slice(nl + 1);
      }
    };
    child.stdout.on("data", feed);
    child.stderr.on("data", feed);
    child.on("error", (e) => done({ error: `${m.name}: ssh ${e.message}` }));
    child.on("close", (code) => {
      liveHolders.delete(child);
      if (buf) onLine(buf);
      done({ error: `${m.name}: holder exited (${code}) ${errText.trim().split(/\r?\n/).slice(-3).join(" | ")}` });
    });
  });
}

// Takes a remote slot back from a holder the rules above condemned: only when
// the lock still names that holder (pid + token) and the pid is still that
// PowerShell (not a reused pid), kill the holder's tree by pid, stop what runs
// out of its run folder, delete the lock. Returns true when the lock is gone.
export function reclaimRemote(root, m, info, kind, reason) {
  if (!info?.name || !info.pid || !info.token) return false;
  const ps = [
    PS_STOP_UNDER,
    `$f = Join-Path ${tb.psQuote(m.root)} ${tb.psQuote(`tmp\\wt\\locks\\${info.name}.lock`)}`,
    "$h = $null; try { $h = [IO.File]::ReadAllText($f) | ConvertFrom-Json } catch {}",
    `if (-not $h -or $h.token -ne ${tb.psQuote(info.token)} -or [int]$h.pid -ne ${Number(info.pid)}) { Write-Host 'lf-pool: reclaim skipped (lock changed)'; exit 0 }`,
    `$p = Get-CimInstance Win32_Process -Filter "ProcessId=${Number(info.pid)}"`,
    "if ($p -and $p.Name -like 'powershell*') { Write-Host \"lf-pool: stopping holder pid $($p.ProcessId)\"; & taskkill /T /F /PID $p.ProcessId 2>&1 | Out-Null }",
    "Stop-Under $h.rDir",
    "Remove-Item -LiteralPath $f -Force -ErrorAction SilentlyContinue",
    "if (-not (Test-Path -LiteralPath $f)) { Write-Host 'lf-pool: reclaimed' }",
  ].join("\n");
  const r = tb.sshPs(m.host, ps, { timeout: 60000, connectTimeout: 4 });
  const out = r.stdout ?? "";
  for (const l of out.split(/\r?\n/)) if (/^lf-pool: (stopping|reclaim skipped)/.test(l.trim())) console.log(`pool: ${m.name}: ${l.trim().slice(9)}`);
  if (!/lf-pool: reclaimed/.test(out)) return false;
  noteReclaim(root, m, info, kind, reason);
  return true;
}

// The verdict on one busy remote holder: { kind, reason } to reclaim, or null.
export function remoteBusyVerdict(info, runs, { self = hostname(), alive = pidAlive, now = Date.now() } = {}) {
  const gone = remoteOwnerGone(info, { self, alive });
  if (gone) return { kind: "dead-owner", reason: gone };
  const hang = hangReason(info, { now, limitMs: hangLimitMs(runs, info?.purpose), serverAlive: info?.serverAlive });
  if (hang) return { kind: "hang", reason: hang };
  return null;
}

function readJsonText(s) {
  try {
    return JSON.parse(s);
  } catch {
    return {};
  }
}

// Commits the box may already have, cheapest delta first: the snapshot itself,
// what this worktree last sent there, and origin/main.
function syncStateFile(root) {
  return join(root, "tmp", "remote", "pool-sync.json");
}

export function readSyncState(root) {
  return readJson(syncStateFile(root), {});
}

export function writeSyncState(root, machine, rec) {
  const s = readSyncState(root);
  s[machine] = { ...(s[machine] ?? {}), ...rec, at: new Date().toISOString() };
  mkdirSync(dirname(syncStateFile(root)), { recursive: true });
  writeFileSync(syncStateFile(root), JSON.stringify(s, null, 2));
}

function originMain(root) {
  const r = spawnSync("git", ["-C", root, "rev-parse", "-q", "--verify", "origin/main"], { windowsHide: true, encoding: "utf8" });
  return r.status === 0 ? r.stdout.trim() : null;
}

export function slugFor(root) {
  return wt.slugOfRoot(root) || root.replace(/[\\/]+$/, "").split(/[\\/]/).pop().replace(/^Life-Framework-ws-/, "").toLowerCase().replace(/[^a-z0-9-]/g, "-");
}

// slots: how many of the machine's lanes of this kind the holder may try (all of them by default;
// 0 = no lock, sync only). ram: the RAM gate the holder applies there before it takes a lock.
export function holderConfig(root, m, snap, { slots, purpose = "test", suffix = "", settings = poolSettings(root) } = {}) {
  const laneCount = lanesFor(m, laneKind(purpose)).length;
  if (slots === undefined) slots = laneCount;
  const slug = slugFor(root);
  const base = originMain(root) ?? snap.head;
  const last = readSyncState(root)[m.name]?.sha;
  const slotList = lanesFor(m, laneKind(purpose))
    .slice(0, slots)
    .map((l) => ({ lock: l.lock, dir: remoteRunDir(m, slug, l.index, suffix) }));
  return {
    main: m.root,
    token: randomBytes(8).toString("hex"),
    slug,
    purpose,
    origin: hostname(),
    originPid: process.pid,
    worktree: root.replace(/\\/g, "/"),
    base,
    candidates: [...new Set([snap.sha, snap.head, last, base].filter(Boolean))],
    slots: slotList,
    dir: remoteRunDir(m, slug, 1, suffix),
    ram: { ...(m.thermalLog ? { thermal: { dir: m.thermalLog, holdC: m.thermalHoldC } } : {}), locks: q.allLaneLocks(m), floorMb: settings.minFreeMb, reserveMb: laneKind(purpose) === "session" ? q.sessionReserveMb(settings, 1, "full") : 0, serverMb: settings.sessionReserveServerMb, clientMb: settings.sessionReserveClientMb },
  };
}

// Whether a snapshot's own CLI honours LF_SERVER_SLOT_HELD (older branches do
// not; their remote run would queue on the lock the holder owns).
function snapshotSupportsPool(root, sha) {
  const r = spawnSync("git", ["-C", root, "show", `${sha}:tools/cli.mjs`], { windowsHide: true, encoding: "utf8", maxBuffer: 64 * 1024 * 1024 });
  return r.status === 0 && r.stdout.includes(SLOT_ENV);
}

// ---------------------------------------------------------------- lanes, leases, deadlock

const sameRoot = (a, b) => !!a && !!b && resolve(String(a)).toLowerCase() === resolve(String(b)).toLowerCase();

// Fail-fast message when every slot this run could take on this PC is held by
// the caller's own worktree (a session it forgot to close): waiting would
// never end. A reachable remote, or any free lane, means no deadlock.
export function selfDeadlock(root, machines, kind, usable = () => true) {
  if (machines.some((m) => m.via !== "local" && usable(m))) return null;
  const locals = machines.filter((m) => m.via === "local");
  if (!locals.length) return null;
  const own = [];
  for (const m of locals) {
    for (const l of lanesFor(m, kind)) {
      const info = readJson(localLockFile(root, l.lock), null);
      // Only a long-lived holder (session, host, serve) deadlocks the caller; its own test run ends by itself.
      if (!info || !pidAlive(info.pid) || !sameRoot(info.root, root) || (info.purpose && laneKind(info.purpose) !== "session")) return null;
      own.push({ ...info, lock: l.lock });
    }
  }
  if (!own.length) return null;
  const first = own[0];
  const slots = own.map((o) => o.lock).join(", ");
  return `pool: this worktree's own ${first.purpose ?? "run"} (pid ${first.pid}) holds ${slots}, the only slot a ${kind} run can use here; close it (node agent-box/session.mjs down) or give tools/pool.json "lanes" so tests get their own slot`;
}

// What a queued ticket waits on. A ticket from a CLI before the lanes (no lane field)
// waits on server-2001: it is a real waiter during the rollout.
function waitsOn(t) {
  return t.waitingOn ?? (t.lane ? [] : [slotLockName(1)]);
}

function closeSession(info) {
  const script = info.root ? join(info.root, "agent-box", "session.mjs") : "";
  const env = { ...process.env };
  delete env.LF_SESSION_DIR;
  if (script && existsSync(script)) {
    const r = spawnSync(process.execPath, [script, "down"], { cwd: info.root, env, windowsHide: true, encoding: "utf8", timeout: 120000 });
    if (r.status === 0) return true;
  }
  spawnSync("taskkill", ["/T", "/F", "/PID", String(info.pid)], { windowsHide: true });
  return false;
}

// Two reasons close a session while a session request queues (both only act
// while someone waits):
//   idle: no harness or session command for sessionIdleMin. An explicit lease
//     (session.mjs lease) or any command resets it, so a driven session is safe.
//   hold cap: older than its cap (sessionHoldMin, 20; session up --hold raises
//     its own, to sessionHoldMaxMin) and another session request queues. The
//     holder is warned once (its next harness or session command prints one
//     line, session-lease.mjs takeHoldWarning), and closed sessionGraceMin (5)
//     later. Only a session that up stamped with holdCap is judged, so sessions
//     started before the cap existed run on as they did.
// The idle limit is judged by the lease only; opts: now, close(info, dir),
// sessionDir (test seams). Returns the sessions closed.
export function reapIdleSessions(root, locals, tickets, settings = poolSettings(root), opts = {}) {
  const now = opts.now ?? Date.now();
  const out = [];
  for (const [m, name] of locals.flatMap((x) => lanesFor(x, "session").map((l) => [x, l.lock]))) {
    const info = readJson(localLockFile(root, name), null);
    if (!info || info.purpose !== "session" || !pidAlive(info.pid)) continue;
    const queued = tickets.filter((t) => t.pid !== info.pid && waitsOn(t).some((w) => w === name || w === "ram"));
    const dir = opts.sessionDir ?? sessionDirOf(info.root ?? root, { LOCALAPPDATA: process.env.LOCALAPPDATA, USERPROFILE: process.env.USERPROFILE });
    const lease = readLease(dir);
    const lastActiveAt = lastActiveMs(dir, info.at);
    // With nobody waiting only the long idle limit applies (a session left open holds RAM and windows).
    if (!queued.length) {
      const lv = q.leaseVerdict({ lastActiveAt, leaseUntil: Date.parse(lease.leaseUntil), now, idleMin: settings.sessionIdleMaxMin });
      if (!lv.expired) continue;
    }
    const v = q.leaseVerdict({ lastActiveAt, leaseUntil: Date.parse(lease.leaseUntil), now, idleMin: queued.length ? settings.sessionIdleMin : settings.sessionIdleMaxMin });
    const sess = readJson(join(dir, "session.json"), null);
    let reason = "";
    let kind = "";
    if (v.expired) {
      reason = queued.length ? `idle ${(v.idleMs / 60000).toFixed(1)} min (limit ${settings.sessionIdleMin}) while ${queued.length} job(s) wait` : `idle ${(v.idleMs / 60000).toFixed(1)} min (limit ${settings.sessionIdleMaxMin} with nobody waiting)`;
      kind = "idle-session";
    } else {
      const waiters = queued.filter((t) => (t.lane ?? "session") === "session" && waitsOn(t).includes(name));
      const heldSinceMs = Date.parse(sess?.heldSince);
      const capMin = q.clampHoldMin(sess?.holdMin, { holdMin: settings.sessionHoldMin, maxMin: settings.sessionHoldMaxMin });
      // A warning whose waiters stopped polling for a minute is void.
      const live = Number.isFinite(Date.parse(lease.holdSeenAt)) && now - Date.parse(lease.holdSeenAt) <= 60000;
      const hv = q.holdVerdict({ heldSinceMs, capMin, graceMin: settings.sessionGraceMin, warnedAtMs: live ? Date.parse(lease.holdWarnedAt) : NaN, waiting: waiters.length > 0, now, capped: !!sess?.holdCap });
      if (hv.phase === "warn" || hv.phase === "grace") {
        const w = noteHoldWarning(dir, { waiting: waiters.length, graceMin: settings.sessionGraceMin, now });
        if (w.fresh) console.log(`pool: the session of ${info.slug ?? "another worktree"} is past its ${capMin} min cap with ${waiters.length} waiting; warned, it closes in ${settings.sessionGraceMin} min`);
      }
      if (hv.phase !== "close") continue;
      reason = `held ${Math.round(hv.heldMs / 60000)} min (cap ${capMin}) with ${waiters.length} session request(s) waiting, warned ${settings.sessionGraceMin} min ago`;
      kind = "hold-cap";
    }
    markReaped(dir, `${reason}. Any harness command keeps an idle session; 'node agent-box/session.mjs lease [min]' holds it; session up --hold <min> raises the hold cap (max ${settings.sessionHoldMaxMin})`, kind === "hold-cap" ? "hold-cap" : "lease");
    (opts.close ?? closeSession)(info, dir);
    noteReclaim(root, m, { ...info, name }, kind, reason);
    recordSession(root, { slug: info.slug, priority: sess?.priority, waitS: sess?.waitS, heldSinceMs: Date.parse(sess?.heldSince ?? info.at), endMs: now, boots: sess?.boots, bootS: sess?.bootS, clients: sess?.expectClients, mission: sess?.mission, closedBy: kind === "hold-cap" ? "hold-cap" : "lease", machine: m.name });
    out.push({ info, reason, kind });
  }
  return out;
}

// The idle rule on a pool machine, judged from this PC: one ssh read of each live session lock there
// and its session dir's activity (lease, ready/start times, the clients' command/result mtimes, as
// lastActiveMs reads them on this PC); a session idle past sessionIdleMin while a job here wants that
// machine, or past sessionIdleMaxMin with nobody waiting, and not under a lease, is closed there with
// its own `session.mjs down` (reaped.json says why). At most once a minute per machine. opts:
// waiting (a job here wants m), now, read/close (test seams). Returns the sessions closed.
const remoteReapAt = new Map();
export function reapRemoteIdleSessions(root, m, settings = poolSettings(root), opts = {}) {
  const now = opts.now ?? Date.now();
  if (!opts.read && now - (remoteReapAt.get(m.name) ?? 0) < 60000) return [];
  remoteReapAt.set(m.name, now);
  const rows = (opts.read ?? readRemoteSessions)(m) ?? [];
  const out = [];
  for (const r of rows) {
    const t = [r.lease?.lastCommandAt, r.readyAt, r.startedAt, r.lock?.at].map((x) => Date.parse(x)).filter(Number.isFinite);
    for (const ms of r.mtimes ?? []) if (Number.isFinite(Number(ms))) t.push(Number(ms));
    const lastActiveAt = t.length ? Math.max(...t) : NaN;
    const idleMin = opts.waiting ? settings.sessionIdleMin : settings.sessionIdleMaxMin;
    const v = q.leaseVerdict({ lastActiveAt, leaseUntil: Date.parse(r.lease?.leaseUntil), now, idleMin });
    if (!v.expired) continue;
    const reason = `idle ${(v.idleMs / 60000).toFixed(1)} min on ${m.name} (limit ${idleMin}${opts.waiting ? " while a job here waits for it" : " with nobody waiting"})`;
    console.log(`pool: closing the session of ${r.lock?.slug ?? r.lock?.root ?? "?"} on ${m.name}: ${reason}`);
    (opts.close ?? closeRemoteSession)(m, r, reason);
    noteReclaim(root, m, { ...r.lock, name: r.name }, "idle-session", reason);
    out.push({ name: r.name, reason });
  }
  return out;
}

export function readRemoteSessions(m) {
  const names = lanesFor(m, "session").map((l) => l.lock);
  const ps = [
    `$ld = Join-Path ${tb.psQuote(m.root)} 'tmp\\wt\\locks'`,
    "$base = Join-Path $env:LOCALAPPDATA 'LifeFramework\\session'",
    `foreach ($n in @(${names.map(tb.psQuote).join(",")})) {`,
    "  $f = Join-Path $ld ($n + '.lock'); if (-not (Test-Path -LiteralPath $f)) { continue }",
    "  try { $h = [IO.File]::ReadAllText($f) | ConvertFrom-Json } catch { continue }",
    "  if ($h.purpose -ne 'session' -or -not $h.root -or -not (Get-Process -Id $h.pid -ErrorAction SilentlyContinue)) { continue }",
    "  $dir = Join-Path $base (Split-Path -Leaf ($h.root.TrimEnd('\\','/')))",
    "  $lease = $null; try { $lease = [IO.File]::ReadAllText((Join-Path $dir 'lease.json')) | ConvertFrom-Json } catch {}",
    "  $sess = $null; try { $sess = [IO.File]::ReadAllText((Join-Path $dir 'session.json')) | ConvertFrom-Json } catch {}",
    "  $mt = @(); foreach ($k in 1..3) { foreach ($x in 'command.txt','result.txt') { $p = Join-Path $dir \"client$k\\profile\\agent-box\\$x\"; if (Test-Path -LiteralPath $p) { $mt += [int64]((Get-Item -LiteralPath $p).LastWriteTimeUtc - [datetime]'1970-01-01').TotalMilliseconds } } }",
    "  $o = [ordered]@{ name = $n; lock = $h; lease = $lease; readyAt = $sess.readyAt; startedAt = $sess.startedAt; mtimes = $mt; dir = $dir }",
    "  Write-Host ('lf-pool: sess ' + ($o | ConvertTo-Json -Compress -Depth 4))",
    "}",
  ].join("\n");
  const r = tb.sshPs(m.host, ps, { timeout: PROBE_TIMEOUT_MS, connectTimeout: 4 });
  if (r.status !== 0) return null;
  return parseRemoteSessions(r.stdout ?? "");
}

export function parseRemoteSessions(text) {
  const out = [];
  for (const l of String(text).split(/\r?\n/)) {
    const mm = /^lf-pool: sess (.*)$/.exec(l.trim());
    if (!mm) continue;
    const o = readJsonText(mm[1]);
    if (o?.name) out.push(o);
  }
  return out;
}

function closeRemoteSession(m, r, reason) {
  const why = JSON.stringify({ at: new Date().toISOString(), why: `${reason}. Any harness command keeps an idle session; 'node agent-box/session.mjs lease [min]' holds it`, kind: "lease" });
  const ps = [
    `try { [IO.File]::WriteAllText((Join-Path ${tb.psQuote(r.dir)} 'reaped.json'), ${tb.psQuote(why)}) } catch {}`,
    `Set-Location -LiteralPath ${tb.psQuote(r.lock.root)}`,
    "& node agent-box/session.mjs down --machine local --force",
    "exit $LASTEXITCODE",
  ].join("\n");
  return tb.sshPs(m.host, ps, { timeout: 180000 }).status === 0;
}

// One line in runs.jsonl per session (kind "session"), written when it ends:
// who, how long it waited and held the lane, how many boots, and who closed it
// (owner: session down; lease: idle reaper; hold-cap: the cap). pool status
// and pool stats read them.
export function recordSession(root, r) {
  const rec = {
    at: new Date(r.endMs ?? Date.now()).toISOString(),
    kind: "session",
    slug: r.slug ?? slugFor(root),
    purpose: "session",
    machine: r.machine ?? "local",
    slot: r.slot ?? slotLockName(1),
    priority: r.priority ?? "normal",
    waitS: Math.round(Number(r.waitS ?? 0) * 10) / 10,
    heldS: Number.isFinite(r.heldSinceMs) ? Math.round(((r.endMs ?? Date.now()) - r.heldSinceMs) / 100) / 10 : 0,
    boots: r.boots ?? 1,
    bootS: r.bootS ?? [],
    clients: r.clients ?? null,
    mission: r.mission ?? null,
    closedBy: r.closedBy ?? "owner",
    result: "closed",
  };
  try {
    appendFileSync(runsFile(root), JSON.stringify(rec) + "\n");
  } catch {}
  return rec;
}

// Disk hygiene (#675): remove session folders older than sessionKeepHours (never one in use, this
// checkout's own, or named for film; link-safe delete), then refuse with a clear message when free
// disk on the session volume is under minFreeDiskGb. Throws a poolHeld error; otherwise never throws.
export function hygieneGate(root, { log = console.log, base = sessionBase(), freeGb } = {}) {
  const settings = poolSettings(root);
  try {
    const r = gc.runSessionGc(base, { keepHours: settings.sessionKeepHours, alive: wt.pidAlive, skip: [sessionDirOf(root)], orphan: (name) => /-ws-/.test(name) && !existsSync(join(dirname(wt.mainRootOf(root)), name)) });
    if (r.removed.length) log(`pool: removed ${r.removed.length} session folder(s) older than ${settings.sessionKeepHours} h or with no worktree (${r.removed.slice(0, 5).join(", ")}${r.removed.length > 5 ? ", ..." : ""})`);
    for (const f of r.failed) log(`WARN  pool: could not remove session folder ${f.name}: ${f.error}`);
  } catch (e) {
    log(`WARN  pool: session folder cleanup failed (${String(e?.message ?? e).split("\n")[0]})`);
  }
  const msg = gc.diskVerdict(freeGb ?? gc.freeDiskGb(base), settings.minFreeDiskGb);
  if (msg) {
    const e = new Error(msg);
    e.poolHeld = true;
    e.refusal = true;
    throw e;
  }
}

// Session requests queued for the session lane (the holder's own ticket excluded).
export function sessionQueue(root) {
  try {
    return liveTickets(root).filter((t) => (t.lane ?? "session") === "session" && waitsOn(t).includes(slotLockName(1)) && t.pid !== process.pid);
  } catch {
    return [];
  }
}

// Queue age memory for the session lane: slug -> { at, seenAt }. A new request from a slug
// whose last one was still queued within QUEUED_KEEP_MS inherits that request's `at`.
const QUEUED_KEEP_MS = 15 * 60000;
function queuedFile(root) {
  return join(poolDir(root), "queued.json");
}

function queuedSince(root, slug, now) {
  const e = readJson(queuedFile(root), {})[slug];
  const at = Date.parse(e?.at);
  return Number.isFinite(at) && now - Date.parse(e.seenAt) <= QUEUED_KEEP_MS ? at : now;
}

function noteQueued(root, slug, since) {
  const all = readJson(queuedFile(root), {});
  const now = Date.now();
  all[slug] = { at: new Date(since).toISOString(), seenAt: new Date(now).toISOString() };
  for (const k of Object.keys(all)) if (now - Date.parse(all[k].seenAt) > QUEUED_KEEP_MS) delete all[k];
  try {
    writeFileSync(queuedFile(root), JSON.stringify(all));
  } catch {}
}

function forgetQueued(root, slug) {
  const all = readJson(queuedFile(root), {});
  if (!all[slug]) return;
  delete all[slug];
  try {
    writeFileSync(queuedFile(root), JSON.stringify(all));
  } catch {}
}

// The session lane (server-2001) for `session up`: joins the same queue as
// test runs (lane "session", FIFO within priority), blocks the calling
// thread, and returns with the lock held by this process (the caller hands it
// to its server with wt.transferLock). Throws when it cannot get it.
export function acquireSessionLaneSync(root, opts = {}) {
  hygieneGate(root);
  try {
    return acquireSessionLane(root, opts);
  } catch (e) {
    if (e.poolHeld) throw e;
    console.log(`WARN  pool: session queue bookkeeping failed (${String(e?.message ?? e).split("\n")[0]}); taking server-2001 directly as before`);
    return wt.acquireLock(root, slotLockName(1), { waitMs: opts.waitMs ?? 0, meta: opts.meta ?? {} });
  }
}

// A newer request from the same worktree replaced this one: its queue entry is gone.
function supersededError(slug) {
  const e = new Error(`a newer session request from ${slug} replaced this one; not queuing twice`);
  e.poolHeld = true;
  e.refusal = true;
  return e;
}

// Drops this worktree's older queued session tickets (a retry whose first request still lives);
// the older process finds its ticket gone and exits. Returns how many were collapsed.
export function collapseRetries(root, ticket, slug) {
  try {
    const tickets = liveTickets(root);
    const { drop } = q.supersededTickets(tickets, tickets.find((t) => t.file === ticket));
    for (const f of drop) dropTicket(root, f);
    if (drop.length) console.log(`session: collapsed ${drop.length} earlier queued request(s) from ${slug} into this one`);
    return drop.length;
  } catch {
    return 0;
  }
}

function acquireSessionLane(root, { waitMs = 0, meta = {}, priority, callerPids = [] } = {}) {
  const m = loadMachines(root).find((x) => x.via === "local") ?? normalize({ name: "local", via: "local" });
  const laneList = lanesFor(m, "session");
  const slug = meta.slug ?? slugFor(root);
  const prio = q.normalizePriority(priority) ?? resolvePriority(root);
  const started = Date.now();
  // A retry keeps the age of the request it replaces (a timed-out or interrupted `up --wait`
  // would otherwise start over and never age to the front).
  const since = queuedSince(root, slug, started);
  let waitingOn = laneList.map((l) => l.lock);
  // stuck: "ram" | "lane" once a try failed for that reason (others may then backfill); needMb: free RAM it needs.
  let stuck = "";
  let needMb;
  const body = () => ({ pid: process.pid, at: new Date(since).toISOString(), slug, purpose: "session", root, pinned: true, lane: "session", priority: prio, waitingOn, machines: [m.name], clients: meta.clients ?? 1, profile: meta.profile ?? "full", ...(stuck ? { stuck } : {}), ...(needMb ? { needMb } : {}) });
  const ticket = writeTicket(root, body(), since);
  collapseRetries(root, ticket, slug);
  let lastReap = 0;
  let said = false;
  try {
    for (;;) {
      noteQueued(root, slug, since);
      const tickets = liveTickets(root);
      const me = tickets.find((t) => t.file === ticket);
      if (!me) throw supersededError(slug);
      const settings = poolSettings(root);
      const now = Date.now();
      if (now - lastReap > 10000) {
        lastReap = now;
        reapIdleSessions(root, [m], tickets, settings);
      }
      const ctx = machineCtx(root, m, settings);
      const blockers = me ? tickets.filter((t) => t.file !== ticket && q.blocks(t, me, m, now, settings.promoteMin * 60000, ctx)) : [];
      const blocked = {};
      // With several session lanes the k-th request in line may take a free lane: it waits only
      // when as many requests are ahead as there are lanes.
      if (blockers.length < laneList.length) {
        for (const lane of laneList) {
          const slot = tryLocal(root, m, lane, { ...meta, slug, purpose: "session", root }, { blocked, noHeld: true });
          if (slot) {
            slot.waitedMs = Date.now() - started;
            slot.queuedMs = Date.now() - since;
            forgetQueued(root, slug);
            return slot;
          }
        }
      }
      // A queued `up` whose caller timed out must not take the slot later.
      if (callerPids.some((p) => p && !wt.pidAlive(p))) {
        const gone = new Error("the caller is gone; not queuing any longer");
        gone.poolHeld = true;
        throw gone;
      }
      if (waitMs <= 0 || now - started > waitMs) {
        const held = new Error(blocked.ram ? `not enough free RAM (${blocked.ram}); close apps or retry with --wait` : blockers.length >= laneList.length ? `${blockers.length} job(s) are queued ahead for the session lane(s)` : `session lane${laneList.length > 1 ? "s" : ""} held (port ${laneList.map((l) => l.port).join(", ")})`);
        held.poolHeld = true;
        throw held;
      }
      if (!said) {
        said = true;
        console.log(`session: queued as ${prio} for the session slot (${blockers.length} ahead, up to ${Math.round(waitMs / 60000)} min); it starts by itself`);
      }
      const want = blocked.ram ? [...laneList.map((l) => l.lock), "ram"] : laneList.map((l) => l.lock);
      const wantStuck = blockers.length >= laneList.length ? "" : blocked.ram ? "ram" : "lane";
      if (want.join() !== waitingOn.join() || wantStuck !== stuck) {
        waitingOn = want;
        stuck = wantStuck;
        needMb = blocked.needMb;
        rewriteTicket(root, ticket, body());
      }
      Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, pollMs());
    }
  } finally {
    dropTicket(root, ticket);
  }
}

// Which machine a `session up` without --machine uses: every machine that can run the
// session (this PC first, so a tie goes here), the first with a free session lane, the RAM
// the session needs (read live: freemem here, one ssh read there) and no older ticket ahead
// of this one on it. Joins the shared queue with all of those machines as wants, so a waiter
// lands wherever a lane frees first. A machine that is down when the request starts stays a
// candidate: it is probed again while the request waits (the laptop came back after three
// days off while two requests sat pinned to this PC for an hour, 2026-11-14). Resolves the
// machine, or null when none frees within waitMs. Does not hold the lane: the machine's own
// `session up` takes it; a lost race there exits LANE_BUSY_CODE and the caller asks again.
export const LANE_BUSY_CODE = 75;

export function sessionMachineCandidates(root, clients) {
  return loadMachines(root).filter((m) => m.caps.includes("server") && (clients <= 0 || (m.caps.includes("clients") && !(m.maxClients && clients > m.maxClients))));
}

// Free session lane on a remote machine (locks only): false when unreachable.
export function remoteSessionFree(m, locks = remoteLocks(m)) {
  if (!locks) return false;
  return lanesFor(m, "session").some((l) => !locks[l.lock] || !locks[l.lock].alive);
}

// The live lane locks of this PC (alive holders only).
function localLockInfos(root, m) {
  const out = [];
  for (const name of q.allLaneLocks(m)) {
    const info = readJson(localLockFile(root, name), null);
    if (info && pidAlive(info.pid)) out.push(info);
  }
  return out;
}

// The blocking context of one machine for q.blocks: its free RAM and what the pool's jobs hold there.
export function machineCtx(root, m, settings, remote = null) {
  const starveMs = settings.starveMin * 60000;
  if (m.via === "local") return { starveMs, freeMb: Math.round(freemem() / 1048576), heldMb: heldMbOf(settings, localLockInfos(root, m)) };
  if (!remote) return { starveMs };
  return { starveMs, freeMb: remote.freeMb ?? undefined, heldMb: heldMbOf(settings, Object.values(remote.locks ?? {}).filter((i) => i?.alive)) };
}

export async function chooseSessionMachine(root, { clients = 1, profile = "full", waitMs = 0, priority, meta = {}, log = console.log, opts = {} } = {}) {
  const all = sessionMachineCandidates(root, clients);
  // One machine configured: its own queue (cmdSessionUp) does the waiting.
  if (all.length < 2) return all[0] ?? null;
  const hereBuild = tb.serverBuildId(tb.localServerDir(root));
  const probeOf = opts.probe ?? ((m) => probe(root, m));
  const stateOf = opts.remoteState ?? remoteState;
  const ok = (m) => {
    if (m.via === "local") return true;
    const h = probeOf(m);
    if (!h.reachable) return false;
    return !(h.build && hereBuild && h.build !== hereBuild);
  };
  const slug = meta.slug ?? slugFor(root);
  const prio = q.normalizePriority(priority) ?? resolvePriority(root);
  const started = Date.now();
  const since = queuedSince(root, slug, started);
  let machines = all.filter(ok);
  let waitingOn = [];
  let stuck = "";
  let needMb;
  let busyOn = [];
  const ticketBody = () => ({ pid: process.pid, at: new Date(since).toISOString(), slug, purpose: "session", root, pinned: false, lane: "session", priority: prio, waitingOn, machines: all.map((m) => m.name), clients, profile, ...(stuck ? { stuck } : {}), ...(needMb ? { needMb } : {}), ...(busyOn.length ? { busyOn } : {}) });
  const ticket = writeTicket(root, ticketBody(), since);
  collapseRetries(root, ticket, slug);
  let lastReap = 0;
  let lastProbe = started;
  let said = false;
  const saidDown = new Set();
  const remoteSeen = new Map();
  try {
    for (;;) {
      noteQueued(root, slug, since);
      const now = Date.now();
      const tickets = liveTickets(root);
      const me = tickets.find((t) => t.file === ticket);
      if (!me) throw supersededError(slug);
      const settings = poolSettings(root);
      if (now - lastReap > 10000) {
        lastReap = now;
        reapIdleSessions(root, all.filter((x) => x.via === "local"), tickets, settings);
      }
      // A machine that was down (or came back) counts again within a probe window.
      if (now - lastProbe > (opts.reprobeMs ?? 10000)) {
        lastProbe = now;
        machines = all.filter(ok);
      }
      for (const m of all) {
        if (m.via !== "local" && !machines.includes(m) && !saidDown.has(m.name)) {
          saidDown.add(m.name);
          log(`session: ${m.name} is down or on another server build; asking again while this request waits`);
        }
      }
      const want = [];
      let ramShort = null;
      let laneSeen = false;
      const busy = [];
      // The remote read (6 s cache), shared by the ordering and the verdicts below.
      const seenOf = (m) => {
        if (m.via === "local") return null;
        let st = remoteSeen.get(m.name);
        if (!st || now - st.at > 6000) {
          st = { at: now, state: stateOf(m) };
          remoteSeen.set(m.name, st);
        }
        return st;
      };
      // The machine with the most RAM headroom for this session goes first, weighted by speed
      // (q.rankMachines): a starved machine is not tried while another has room.
      const order = orderMachines(root, machines, settings, { reserveMb: q.sessionReserveMb(settings, clients, profile), stateOf: (m) => seenOf(m)?.state, localFreeMb: opts.localFreeMb });
      for (const m of order) {
        const lanes = lanesFor(m, "session");
        const st = seenOf(m);
        const ctx = machineCtx(root, m, settings, st?.state);
        const blockers = me ? tickets.filter((t) => t.file !== ticket && q.blocks(t, me, m, now, settings.promoteMin * 60000, ctx)) : [];
        for (const l of lanes) want.push(m.via === "local" ? l.lock : `${m.name}:${l.lock}`);
        if (blockers.length >= lanes.length) continue;
        if (m.via === "local") {
          // This PC under its disk floor refuses every session start: leave the request to the others.
          const disk = opts.diskMsg !== undefined ? opts.diskMsg : gc.diskVerdict(gc.freeDiskGb(sessionBase()), settings.minFreeDiskGb);
          if (disk) {
            if (!saidDown.has("disk")) {
              saidDown.add("disk");
              log(`session: this PC refuses sessions for now (${disk}); asking the other machines`);
            }
            continue;
          }
          const blocked = {};
          for (const lane of lanes) {
            const slot = tryLocal(root, m, lane, { ...meta, clients, profile, slug, purpose: "session", root }, { blocked, noHeld: true, ...(opts.localFreeMb !== undefined ? { freeMb: opts.localFreeMb } : {}) });
            if (slot) {
              slot.release();
              forgetQueued(root, slug);
              return m;
            }
          }
          if (blocked.ram) ramShort = ramShort ?? { needMb: blocked.needMb, why: `${m.name}: ${blocked.ram}` };
          else {
            laneSeen = true;
            busy.push(m.name);
          }
          continue;
        }
        const v = remoteSessionVerdict(m, st.state, settings, { clients, profile, now });
        if (v.ok) {
          forgetQueued(root, slug);
          return m;
        }
        if (v.down) {
          // It stopped answering: drop it until the next probe window.
          machines = machines.filter((x) => x !== m);
          markHealth(root, m, { reachable: false, error: "lane read failed" });
          continue;
        }
        if (v.ram) ramShort = ramShort ?? { needMb: v.needMb, why: `${m.name}: ${v.ram}` };
        else {
          laneSeen = true;
          busy.push(m.name);
        }
        // This request waits for that machine: its idle sessions are judged by the 15 min rule.
        if (!opts.remoteState) {
          try {
            if (reapRemoteIdleSessions(root, m, settings, { waiting: true }).length) remoteSeen.delete(m.name);
          } catch {}
        }
      }
      if (waitMs <= 0 || Date.now() - started > waitMs) return null;
      if (!said) {
        said = true;
        log(`session: queued as ${prio} for a session lane on ${all.map((m) => m.name).join(" or ")} (up to ${Math.round(waitMs / 60000)} min); it starts by itself on the first that frees${ramShort ? ` (${ramShort.why})` : ""}`);
      }
      // stuck "ram" when a lane was free somewhere but the RAM was not; "lane" when every lane was taken.
      const wantStuck = ramShort ? "ram" : laneSeen ? "lane" : "";
      const wantNeed = wantStuck === "ram" ? ramShort.needMb : undefined;
      if (want.join() !== waitingOn.join() || wantStuck !== stuck || wantNeed !== needMb || busy.join() !== busyOn.join()) {
        waitingOn = want;
        stuck = wantStuck;
        needMb = wantNeed;
        busyOn = busy;
        rewriteTicket(root, ticket, ticketBody());
      }
      await sleep(opts.pollMs ?? pollMs());
    }
  } finally {
    dropTicket(root, ticket);
  }
}

// The local machine's session lanes, [{ lock, index, port }] (one without "lanes" in pool.json).
export function sessionLanes(root) {
  const m = loadMachines(root).find((x) => x.via === "local") ?? normalize({ name: "local", via: "local" });
  return lanesFor(m, "session");
}

// The UDP port the lane this process holds listens on: the slot taken here, or
// the one a remote pool holder named in LF_SERVER_SLOT_HELD. null when none.
export function currentLanePort() {
  if (heldHere) return heldHere.port;
  const v = process.env[SLOT_ENV];
  return v ? q.portForLock(v.split(":")[0]) : null;
}

// ---------------------------------------------------------------- acquire

// The machines a run may use under the current config (re-read every call).
export function resolveMachines(root, opts = {}) {
  const mode = opts.mode ?? "any";
  const all = loadMachines(root);
  let machines;
  if (opts.machine) {
    const m = findMachine(all, opts.machine);
    if (!m) throw new Error(`pool: no machine '${opts.machine}' (known: ${all.map((x) => x.name).join(", ")}); see tools/pool.json`);
    machines = [m];
  } else if (mode === "local") machines = all.filter((m) => m.via === "local");
  else if (mode === "remote") machines = all.filter((m) => m.via === "ssh");
  else machines = all;
  return machines.filter((m) => m.caps.includes("server"));
}

// Temp file + rename: a reader never sees a half-written ticket (and deletes it).
// A ticket that is gone (a newer request from the same worktree dropped it, or the reaper did) is not
// written again: a waiter that rewrote it after the drop (its ssh read takes seconds) brought the
// replaced request back, so one worktree queued twice. Returns whether it wrote.
export function rewriteTicket(root, f, t) {
  const file = join(queueDir(root), f);
  if (!existsSync(file)) return false;
  try {
    writeFileSync(`${file}.tmp`, JSON.stringify(t));
    renameSync(`${file}.tmp`, file);
    return true;
  } catch {
    return false;
  }
}

// How often a run whose machine went away is queued again (the last try keeps its verdict).
export const FAILOVER_ATTEMPTS = 3;

// After a remote run: true when its machine went away under it (the holder's ssh dropped, or the
// run's ssh failed with 255 and the machine no longer answers). The machine is marked down for the
// health window, so the requeued run lands elsewhere; prints one line. opts.probe is a test seam.
export function machineGone(root, slot, code, opts = {}) {
  if (!slot || slot.local) return false;
  const m = slot.machine;
  if (!slot.lost && code !== 255) return false;
  const h = (opts.probe ?? ((x) => probe(root, x, { fresh: true })))(m);
  if (h.reachable && !slot.lost) return false;
  if (!h.reachable) markHealth(root, m, { reachable: false, error: "went away during a run" });
  console.log(`pool: ${m.name} went away during the run (${slot.lost ? "its slot holder dropped" : "ssh failed"}${h.reachable ? "" : ", it does not answer"}); queuing the run again${h.reachable ? "" : `, without ${m.name} for a minute`}`);
  return true;
}

export function markDown(root, m, error) {
  markHealth(root, m, { reachable: false, error });
}

// opts: mode "any"|"local"|"remote", machine (name or ssh host), purpose,
// waitMs. Returns a slot with .release(), .machine, .waitedMs, and for a
// remote slot .rDir, .prep, .snap.
export async function acquireSlot(root, opts = {}) {
  const purpose = opts.purpose ?? "test";
  const mode = opts.mode ?? "any";
  const started = Date.now();
  if (process.env[SLOT_ENV]) {
    const s = heldSlotFromEnv(root);
    s.waitedMs = 0;
    return s;
  }
  let machines = resolveMachines(root, opts);
  if (machines.length === 0) throw new Error(`pool: no machine for --${mode}`);
  if (machines.some((m) => m.via === "local")) hygieneGate(root);

  // Remote runs ship a snapshot; take it once, when a remote first comes into
  // play (what the worktree is when the run was asked for, or when a remote
  // was enabled while it waited).
  let snap = null;
  let snapTried = false;
  const withSnap = (list) => {
    if (!snapTried && list.some((m) => m.via === "ssh")) {
      snapTried = true;
      try {
        snap = tb.snapshotCommit(root);
        if (!snapshotSupportsPool(root, snap.sha)) {
          console.log(`pool: this snapshot's tools/cli.mjs predates the pool (no ${SLOT_ENV}); merge origin/main to run remotely. Local only.`);
          snap = null;
        }
      } catch (e) {
        console.log(`pool: snapshot failed (${e.message}); local only`);
      }
    }
    return snap ? list : list.filter((m) => m.via === "local");
  };
  machines = withSnap(machines);
  if (machines.length === 0) throw new Error("pool: no machine can take this run");

  const hereBuild = tb.serverBuildId(tb.localServerDir(root));
  const skipNote = new Set();
  const note = (key, msg) => {
    if (skipNote.has(key)) return;
    skipNote.add(key);
    console.log(msg);
  };
  const downNow = new Set();
  const usable = (m) => {
    if (m.via === "local") return true;
    if (downNow.has(m.name)) return false;
    // Only a cached answer here: the holder attempt itself is the probe (it
    // reports the build), so a healthy box costs no extra ssh round trip.
    const h = readJson(healthFile(root), {})[m.name];
    if (!h || Date.now() - h.at > HEALTH_TTL_MS) return true;
    if (!h.reachable) {
      note(`down-${m.name}`, `pool: ${m.name} does not answer ssh (cached ${Math.round((Date.now() - h.at) / 1000)} s); skipping it`);
      return false;
    }
    if (h.build && hereBuild && h.build !== hereBuild) {
      if (opts.machine) {
        note(`build-${m.name}`, `WARN  pool: ${m.name} server build ${h.build} differs from this PC ${hereBuild}; running there because it was asked for`);
        return true;
      }
      note(`build-${m.name}`, `WARN  pool: ${m.name} server build ${h.build} differs from this PC ${hereBuild}; skipping it (tools\\cli testbox provision updates it)`);
      return false;
    }
    return true;
  };
  if (!machines.some((m) => m.via === "local") && !machines.some(usable)) {
    throw new Error(`pool: no reachable machine for this run (${machines.map((m) => m.name).join(", ")})`);
  }

  const slug = slugFor(root);
  const pinned = !!opts.machine || mode !== "any";
  const kind = laneKind(purpose);
  const priority = q.normalizePriority(opts.priority) ?? resolvePriority(root);
  // A pin (--local / --machine) overrides the pool's RAM-aware choice: one line at queue time when
  // another machine has room and would have been chosen (`pool status` says it again while it waits).
  if (pinned) {
    try {
      const st = poolSettings(root);
      const health = readJson(healthFile(root), {});
      const others = loadMachines(root).filter((x) => x.caps.includes("server") && !machines.some((p) => p.name === x.name) && !(x.via !== "local" && health[x.name] && Date.now() - health[x.name].at < HEALTH_TTL_MS && !health[x.name].reachable));
      const flag = opts.machine ? `--machine ${opts.machine}` : mode === "local" ? "--local" : "--remote";
      const w = pinWarning(root, machines, others, st, { flag, reserveMb: kind === "session" ? q.sessionReserveMb(st, 1, "full") : 0, stateOf: (x) => remoteStateCached(x) });
      if (w) console.log(w);
    } catch {}
  }
  let waitingOn = [];
  let stuck = "";
  let needMb;
  const ticketBody = () => ({ pid: process.pid, at: new Date(started).toISOString(), slug, purpose, root, pinned, lane: kind, priority, waitingOn, machines: machines.map((m) => m.name), ...(stuck ? { stuck } : {}), ...(needMb ? { needMb } : {}) });
  const ticket = writeTicket(root, ticketBody());
  let lastReap = 0;
  const lastTry = new Map();
  let degraded = false;
  const degrade = (e) => {
    if (degraded) return;
    degraded = true;
    console.log(`WARN  pool: queue bookkeeping failed (${String(e?.message ?? e).split("\n")[0]}); using the single server-2001 slot as before`);
  };
  // The lanes this run may try on m: its own kind's, or the old shared slot after a fallback.
  const laneList = (m) => (degraded ? [{ lock: slotLockName(1), index: 1, port: q.SESSION_PORT }] : lanesFor(m, kind));
  const budgetCache = new Map();
  const budgetsOf = (m) => {
    if (!budgetCache.has(m.name)) {
      let b = null;
      try {
        b = budgetsFor(root, m, purpose, opts.budget);
      } catch (e) {
        console.log(`WARN  pool: run budget unavailable (${String(e?.message ?? e).split("\n")[0]}); no timeout for this run`);
      }
      budgetCache.set(m.name, b);
    }
    return budgetCache.get(m.name);
  };
  const waitMs = opts.waitMs ?? 3600000;
  let announced = false;
  try {
    for (;;) {
      // Every poll re-reads the pool config: a machine enabled or disabled in
      // lf.local.json while this ticket waits counts now, and the ticket's
      // "wants" follow it (same file, so its place in the queue is kept).
      if (!opts.machine) {
        let now = machines;
        try {
          now = withSnap(resolveMachines(root, opts));
        } catch {}
        if (now.map((m) => m.name).join("|") !== machines.map((m) => m.name).join("|")) {
          console.log(`pool: machines now ${now.map((m) => m.name).join(", ") || "(none enabled)"}`);
          machines = now;
          rewriteTicket(root, ticket, ticketBody());
        }
      }
      // Queue bookkeeping (lanes, priority, leases, RAM gate) must never block or crash a run:
      // if it throws or meets bad state, one warning and the old single server-2001 slot, plain FIFO.
      let tickets = [];
      let me = null;
      let nowMs = Date.now();
      let promoteMs = q.DEFAULTS.promoteMin * 60000;
      const waits = new Set();
      const blockedBy = new Set();
      let ramBlocked = null;
      let triedLocal = false;
      let triedRemote = false;
      let selfStuck = null;
      if (!degraded) {
        try {
          tickets = liveTickets(root);
          me = tickets.find((t) => t.file === ticket) ?? null;
          const settings = poolSettings(root);
          promoteMs = settings.promoteMin * 60000;
          if (nowMs - lastReap > 10000) {
            lastReap = nowMs;
            reapIdleSessions(root, machines.filter((x) => x.via === "local"), tickets, settings);
          }
          selfStuck = selfDeadlock(root, machines, kind, usable);
        } catch (e) {
          degrade(e);
        }
      }
      if (selfStuck) throw new Error(selfStuck);
      if (degraded) {
        try {
          tickets = liveTickets(root);
          me = tickets.find((t) => t.file === ticket) ?? null;
        } catch {}
      }
      const aheadCounts = [];
      // Try the machine with the most RAM headroom first, weighted by speed (q.rankMachines); the
      // remote read is cached, and a machine below the floor is refused by the gate anyway.
      let order = machines;
      try {
        const st = poolSettings(root);
        order = orderMachines(root, machines, st, { reserveMb: kind === "session" ? q.sessionReserveMb(st, 1, "full") : 0, stateOf: (x) => (usable(x) ? remoteStateCached(x) : null) });
      } catch {}
      for (const m of order) {
        // A machine that is down for this run neither blocks nor counts as waited on.
        if (m.via !== "local" && !usable(m)) continue;
        let blockers = [];
        try {
          if (degraded) {
            const mine = tickets.findIndex((t) => t.file === ticket);
            blockers = mine < 0 ? [] : tickets.slice(0, mine).filter((t) => (t.machines ?? []).includes(m.name));
          } else if (me) {
            const ctx = m.via === "local" ? machineCtx(root, m, poolSettings(root)) : { starveMs: poolSettings(root).starveMin * 60000 };
            blockers = tickets.filter((t) => t.file !== ticket && q.blocks(t, me, m, nowMs, promoteMs, ctx));
          }
        } catch (e) {
          degrade(e);
        }
        aheadCounts.push(blockers.length);
        if (blockers.length) {
          // The idle-session reaper reads waitingOn: only this PC's locks count for it.
          for (const l of laneList(m)) waits.add(m.via === "local" ? l.lock : `${m.name}:${l.lock}`);
          continue;
        }
        if (m.via === "local") {
          const blocked = {};
          for (const lane of laneList(m)) {
            let s = null;
            try {
              const bud = budgetsOf(m);
              s = tryLocal(root, m, lane, { slug, purpose, root, priority, ...(bud ? { budgetS: bud.runS } : {}) }, { blocked, noRam: degraded });
              if (s && bud) {
                s.budgetS = bud.runS;
                s.bootS = bud.bootS;
              }
            } catch (e) {
              degrade(e);
            }
            if (s) return finish(s);
            waits.add(lane.lock);
          }
          triedLocal = true;
          if (blocked.ram) {
            ramBlocked = blocked;
            waits.add("ram");
            note("ram", `pool: waiting for RAM (${blocked.ram}); starts by itself when memory frees`);
          }
          continue;
        }
        if (Date.now() - (lastTry.get(m.name) ?? 0) < REMOTE_RETRY_MS) continue;
        lastTry.set(m.name, Date.now());
        if (!usable(m)) continue;
        const cfg = holderConfig(root, m, snap, { purpose });
        const attemptAt = Date.now();
        const r = await startHolder(root, m, cfg, {
          onLost: (s) => console.log(`WARN  pool: lost the ssh session holding ${s.machine.name} ${s.lockName}; the run there is being stopped`),
        });
        if (r.hot !== undefined) {
          markHealth(root, m, { reachable: true });
          ramBlocked = ramBlocked ?? { ram: `${m.name} is at ${r.hot} °C (holds new jobs at ${m.thermalHoldC} °C)` };
          triedRemote = true;
          waits.add(`${m.name}:hot`);
          note(`hot-${m.name}`, `pool: ${m.name} is at ${r.hot} °C, at or over its ${m.thermalHoldC} °C hold; waiting for it to cool or another machine`);
          continue;
        }
        if (r.slot || r.busy || r.ram) markHealth(root, m, { reachable: true, build: r.slot?.prep.build ?? null });
        if (r.ram) {
          // A free lane there, but not the RAM: the same wait as this PC's gate.
          const gb = (x) => (x / 1024).toFixed(1);
          ramBlocked = ramBlocked ?? { ram: `${m.name} has ${gb(r.ram.freeMb)} GB free, needs ${gb(r.ram.needMb)}`, needMb: r.ram.needMb };
          triedRemote = true;
          waits.add(`${m.name}:ram`);
          note(`ram-${m.name}`, `pool: ${m.name} is short of RAM (${gb(r.ram.freeMb)} GB free, ${gb(r.ram.needMb)} GB needed with ${r.ram.running} server(s) there); waiting`);
          continue;
        }
        if (r.busy) triedRemote = true;
        if (r.slot && !usable(m)) {
          r.slot.release();
          continue;
        }
        if (r.slot) {
          r.slot.snap = snap;
          return finish(r.slot, attemptAt);
        }
        // Busy: a holder whose owner on this PC died, or one hung with no
        // server, is reclaimed now and the machine retried on the next poll.
        if (r.busy?.length) {
          const runs = readRuns(root);
          for (const b of r.busy) {
            const v = remoteBusyVerdict(b, runs);
            if (v && reclaimRemote(root, m, b, v.kind, v.reason)) lastTry.delete(m.name);
          }
        }
        if (r.error) {
          // Down for the rest of this run and the health window; the next
          // machine (or this PC) takes the job.
          downNow.add(m.name);
          markHealth(root, m, { reachable: false, error: r.error });
          const next = machines.filter((x) => x.name !== m.name && usable(x)).map((x) => x.name);
          console.log(`pool: ${r.error}; ${m.name} marked down for this run, ${next.length ? `falling back to ${next.join(", ")}` : "no other machine"}`);
          if (!next.length) throw new Error(`pool: no reachable machine for this run (${m.name} is down)`);
        }
      }
      const ahead = aheadCounts.length ? Math.min(...aheadCounts) : 0;
      const wantStuck = !triedLocal && !triedRemote ? "" : ramBlocked ? "ram" : "lane";
      if (waits.size !== waitingOn.length || [...waits].some((w) => !waitingOn.includes(w)) || wantStuck !== stuck) {
        waitingOn = [...waits];
        stuck = wantStuck;
        needMb = ramBlocked?.needMb;
        rewriteTicket(root, ticket, ticketBody());
      }
      if (!announced) {
        announced = true;
        console.log(`pool: ${kind} slots busy; queued as ${priority} (${ahead} ahead), it starts by itself when one frees`);
      }
      if (Date.now() - started > waitMs) throw new Error(`pool: no slot within ${Math.round(waitMs / 60000)} min`);
      await sleep(pollMs());
    }
  } finally {
    dropTicket(root, ticket);
  }

  // waited = time in the queue; the ssh connect and run-folder prep of the
  // winning attempt count as setup, not waiting.
  function finish(s, attemptAt = Date.now()) {
    s.waitedMs = attemptAt - started;
    s.setupMs = Date.now() - attemptAt;
    s.startedAt = attemptAt;
    if (!s.budgetS) {
      const b = budgetsOf(s.machine);
      if (b) {
        s.budgetS = b.runS;
        s.bootS = b.bootS;
      }
    }
    const where = s.local ? `this PC (${hostname()})` : `${s.machine.name} ${s.rDir}`;
    const setup = s.local ? "" : `, holder + prep ${(s.setupMs / 1000).toFixed(1)} s`;
    console.log(`pool: ${purpose} runs on ${where}, slot ${s.lockName}; waited ${(s.waitedMs / 1000).toFixed(1)} s${setup}${s.budgetS ? `; budget ${s.budgetS} s` : ""}`);
    return s;
  }
}

// ---------------------------------------------------------------- run log

function runsFile(root) {
  return join(poolDir(root), "runs.jsonl");
}

export function recordRun(root, slot, { purpose, code, extra = {} }) {
  const durMs = Date.now() - (slot.startedAt ?? Date.now());
  const rec = {
    at: new Date().toISOString(),
    slug: slugFor(root),
    purpose,
    machine: slot.machine.name,
    slot: slot.lockName,
    waitS: Math.round((slot.waitedMs ?? 0) / 100) / 10,
    durS: Math.round(durMs / 100) / 10,
    code,
    result: code === 0 ? "pass" : code === 1 ? "fail" : code === q.TIMEOUT_CODE ? "timeout" : "error",
    ...extra,
  };
  try {
    appendFileSync(runsFile(root), JSON.stringify(rec) + "\n");
  } catch {}
  if (code === q.TIMEOUT_CODE) console.log(`pool: ${purpose} on ${slot.machine.name} timed out and was killed; its slot is free. Rerun it, or pass --budget <s> if this run is known to be long`);
  else console.log(`pool: ${purpose} on ${slot.machine.name}: exit ${code}, waited ${rec.waitS} s, ran ${rec.durS} s`);
  return rec;
}

export function readRuns(root) {
  const f = runsFile(root);
  if (!existsSync(f)) return [];
  return readFileSync(f, "utf8").split(/\r?\n/).filter(Boolean).map((l) => readJsonText(l));
}

// ---------------------------------------------------------------- status / stats / prune

function localTime(iso) {
  const d = new Date(iso);
  if (Number.isNaN(d.getTime())) return String(iso);
  const p = (n) => String(n).padStart(2, "0");
  return `${d.getFullYear()}-${p(d.getMonth() + 1)}-${p(d.getDate())} ${p(d.getHours())}:${p(d.getMinutes())}:${p(d.getSeconds())}`;
}

function ago(iso) {
  const s = Math.round((Date.now() - Date.parse(iso)) / 1000);
  if (!Number.isFinite(s)) return "?";
  return s < 120 ? `${s}s` : `${Math.round(s / 60)}m`;
}

function holderText(info) {
  if (!info) return "free";
  const who = info.slug ? `${info.slug}${info.purpose ? ` ${info.purpose}` : ""}` : info.root ? info.root.split(/[\\/]/).pop() : "?";
  const from = info.origin ? ` from ${info.origin}` : "";
  return `busy: ${who}${from} (pid ${info.pid}, ${ago(info.at)})`;
}

// One ssh read of a remote machine: its lane locks (holder json + alive) and its live RAM.
// { locks: { lock: info|null }, freeMb, totalMb }, or null when it does not answer.
export function remoteState(m) {
  const names = q.allLaneLocks(m);
  const ps = [
    `$ld = Join-Path ${tb.psQuote(m.root)} 'tmp\\wt\\locks'`,
    `foreach ($n in @(${names.map(tb.psQuote).join(",")})) { $f = Join-Path $ld ($n + '.lock'); if (Test-Path -LiteralPath $f) { $raw = [IO.File]::ReadAllText($f).Trim(); $alive = $false; try { $i = $raw | ConvertFrom-Json; $alive = [bool](Get-Process -Id $i.pid -ErrorAction SilentlyContinue) } catch {}; Write-Host \"lf-pool: lock $n $alive $raw\" } else { Write-Host \"lf-pool: lock $n free\" } }`,
    "$os = Get-CimInstance Win32_OperatingSystem; Write-Host \"lf-pool: ram $([int]($os.FreePhysicalMemory / 1024)) $([int]($os.TotalVisibleMemorySize / 1024))\"",
    ...(m.thermalLog ? [`$thermDir = ${tb.psQuote(m.thermalLog)}`, PS_THERMAL, "if ($thermC -ne $null) { Write-Host ('lf-pool: thermal ' + $thermC.ToString([Globalization.CultureInfo]::InvariantCulture)) }"] : []),
  ].join("\n");
  const r = tb.sshPs(m.host, ps, { timeout: PROBE_TIMEOUT_MS, connectTimeout: 4 });
  return r.status === 0 ? parseRemoteState(r.stdout ?? "") : null;
}

export function parseRemoteState(text) {
  const locks = {};
  let freeMb = null;
  let totalMb = null;
  let thermalC = null;
  for (const l of String(text).split(/\r?\n/)) {
    const th = /^lf-pool: thermal ([\d.]+)$/.exec(l.trim());
    if (th) thermalC = Number(th[1]);
    const t = l.trim();
    const mm = /^lf-pool: lock (\S+) (free|True|False)(?: (.*))?$/.exec(t);
    if (mm) locks[mm[1]] = mm[2] === "free" ? null : { ...readJsonText(mm[3] ?? "{}"), alive: mm[2] === "True" };
    const rm = /^lf-pool: ram (\d+) (\d+)$/.exec(t);
    if (rm) {
      freeMb = Number(rm[1]);
      totalMb = Number(rm[2]);
    }
  }
  return { locks, freeMb, totalMb, thermalC };
}

function remoteLocks(m) {
  return remoteState(m)?.locks ?? null;
}

// remoteState, remembered for ttlMs (an unreadable machine for downTtlMs), so choosing a machine
// for a run or a session does not ssh on every decision. opts.read / opts.now are test seams.
const stateCache = new Map();
export function remoteStateCached(m, { ttlMs = 8000, downTtlMs = 30000, now = Date.now(), read = remoteState } = {}) {
  const hit = stateCache.get(m.name);
  if (hit && now - hit.at < (hit.state ? ttlMs : downTtlMs)) return hit.state;
  let state = null;
  try {
    state = read(m);
  } catch {}
  stateCache.set(m.name, { at: now, state });
  return state;
}

// The machines in the order a job should try them (q.rankMachines): live free RAM per machine, this
// PC read here, a pool machine from stateOf(m) (its cached ssh read; null when unreadable), less the
// sessions still booting there, less the job's reserve. One machine: as given.
export function orderMachines(root, machines, settings, opts = {}) {
  if (machines.length < 2) return machines;
  return rankedMachines(root, machines, settings, opts).map((r) => machines.find((m) => m.name === r.name));
}

// The rows of q.rankMachines for these machines ({ name, room, headroomMb, score }, best first).
export function rankedMachines(root, machines, settings, { reserveMb = 0, stateOf = () => null, localFreeMb } = {}) {
  const views = machines.map((m) => {
    if (m.via === "local") {
      const pendingMb = otherSessionLanes(root, m, null).reduce((n, s) => n + s.pendingMb, 0);
      return { name: m.name, freeMb: localFreeMb ?? Math.round(freemem() / 1048576), pendingMb, multiplier: m.budgetMultiplier };
    }
    const st = stateOf(m);
    const pendingMb = st ? remotePendingMb(settings, Object.values(st.locks ?? {}).filter((i) => i?.alive)) : 0;
    return { name: m.name, freeMb: st?.freeMb ?? null, pendingMb, multiplier: m.budgetMultiplier };
  });
  return q.rankMachines(views, { minFreeMb: settings.minFreeMb, reserveMb, roomyMb: settings.roomyMb });
}

// One line for a job pinned to `pinned` (--local / --machine / --remote) when the pool, left to
// choose among pinned and others, would send it elsewhere: another machine has room and ranks
// above the pinned ones. null when the pin costs nothing. flag names the pin for the hint.
export function pinWarning(root, pinned, others, settings, { flag = "--local", ...opts } = {}) {
  if (!others.length) return null;
  const rows = rankedMachines(root, [...pinned, ...others], settings, opts);
  const best = rows[0];
  if (!best || best.room !== true || pinned.some((m) => m.name === best.name)) return null;
  const gb = (x) => (x / 1024).toFixed(1);
  return `WARN  pool: pinned to ${pinned.map((m) => m.name).join(", ")} (${flag}); ${best.name} has room (${gb(best.headroomMb)} GB headroom) and would be chosen; drop ${flag} and the pool picks the machine with the most RAM headroom`;
}

// Memory the pool's own running jobs hold on a machine (MB), from their lock records: a session its
// reserve (clients, profile), a test the server share. What draining that machine could free.
export function heldMbOf(settings, infos) {
  let mb = 0;
  for (const i of infos) {
    if (!i || i.alive === false) continue;
    if (i.purpose === "session") mb += q.sessionReserveMb(settings, i.clients ?? 1, i.profile ?? "full");
    else if (i.purpose) mb += settings.sessionReserveServerMb;
  }
  return mb;
}

// Memory the sessions still booting on a remote take that its free RAM does not show yet: the
// reserve of each live session lock younger than three minutes (a box session is ready in ~40 s).
export function remotePendingMb(settings, infos, now = Date.now()) {
  let mb = 0;
  for (const i of infos) {
    if (!i?.alive || i.purpose !== "session") continue;
    if (now - Date.parse(i.at) < 180000) mb += q.sessionReserveMb(settings, i.clients ?? 1, i.profile ?? "full");
  }
  return mb;
}

// Whether a session (clients, profile) may start on a remote right now: a free session lane and
// the same RAM verdict its own `session up` will apply there. { ok, lane, ram?, needMb? }.
export function remoteSessionVerdict(m, st, settings, { clients = 1, profile = "full", now = Date.now() } = {}) {
  if (!st) return { ok: false, lane: false, down: true };
  const lane = lanesFor(m, "session").some((l) => !st.locks[l.lock] || !st.locks[l.lock].alive);
  if (!lane) return { ok: false, lane: false };
  if (Number.isFinite(st.thermalC) && st.thermalC >= (m.thermalHoldC ?? 90)) return { ok: false, lane: true, ram: `${st.thermalC} °C, at or over its ${m.thermalHoldC ?? 90} °C hold`, hot: st.thermalC };
  if (!Number.isFinite(st.freeMb)) return { ok: true, lane: true };
  const infos = Object.values(st.locks).filter((i) => i?.alive);
  const v = q.ramVerdict({ running: infos.length, freeMb: st.freeMb, minFreeMb: settings.minFreeMb, reserveMb: q.sessionReserveMb(settings, clients, profile), pendingMb: remotePendingMb(settings, infos, now) });
  return v.ok ? { ok: true, lane: true } : { ok: false, lane: true, ram: v.reason, needMb: v.needMb };
}

function mins(ms) {
  const m = ms / 60000;
  return m < 1 ? `${Math.max(0, Math.round(ms / 1000))}s` : m < 10 ? `${m.toFixed(1)}m` : `${Math.round(m)}m`;
}

function ramText(root) {
  const free = freemem() / 1073741824;
  return `RAM ${free.toFixed(1)}/${(totalmem() / 1073741824).toFixed(1)} GB free (floor ${(poolSettings(root).minFreeMb / 1024).toFixed(1)})`;
}

// The hold cap of the session holding the lane, for pool status.
function holdNote(dir, info, lease, tickets, lane, settings, now) {
  const sess = readJson(join(dir, "session.json"), null);
  if (!sess?.holdCap) return "no hold cap (started before it)";
  const heldSinceMs = Date.parse(sess.heldSince ?? info.at);
  const capMin = q.clampHoldMin(sess.holdMin, { holdMin: settings.sessionHoldMin, maxMin: settings.sessionHoldMaxMin });
  const waiters = tickets.filter((t) => (t.lane ?? "session") === "session" && waitsOn(t).includes(lane.lock)).length;
  const live = Number.isFinite(Date.parse(lease.holdSeenAt)) && now - Date.parse(lease.holdSeenAt) <= 60000;
  const hv = q.holdVerdict({ heldSinceMs, capMin, graceMin: settings.sessionGraceMin, warnedAtMs: live ? Date.parse(lease.holdWarnedAt) : NaN, waiting: waiters > 0, now });
  if (hv.phase === "grace") return `hold cap ${capMin}m passed, warned: closes in ${mins(hv.leftMs)}`;
  if (hv.phase === "warn" || hv.phase === "close") return `hold cap ${capMin}m passed, ${waiters} waiting: warning at the next poll`;
  const left = capMin * 60000 - hv.heldMs;
  return left > 0 ? `hold cap ${capMin}m (${mins(left)} left; only enforced while a session request waits)` : `hold cap ${capMin}m passed; nobody waits`;
}

// One line per lane: holder, purpose, age, idle (sessions: time since the last
// harness command), and the lease note.
// A free or stale lane row: default pool status folds these into a count on the machine line
// (a 16-lane machine would otherwise print a row per free lane).
export function isIdleLaneRow(r) {
  return /\sfree\s*$/.test(r) || /\sstale\s/.test(r);
}

function laneRow(root, kind, lane, info, tickets, settings, now) {
  const head = `    ${kind.padEnd(8)} ${lane.lock.padEnd(15)}`;
  if (info === undefined) return `${head} unknown`;
  if (!info) return `${head} free`;
  if (info.stale) return `${head} stale (pid ${info.pid} gone; the next acquirer reclaims it)`;
  const who = info.slug ?? (info.root ? info.root.split(/[\\/]/).pop() : "?");
  const purpose = info.purpose ?? "-";
  const age = info.at ? mins(now - Date.parse(info.at)) : "?";
  let idle = "-";
  let note = info.origin ? `from ${info.origin}` : "";
  if (info.budgetS) note = `run ${Math.round((now - Date.parse(info.at)) / 1000)}s of ${info.budgetS}s budget`;
  if (info.purpose === "session" && info.root) {
    const dir = sessionDirOf(info.root, { LOCALAPPDATA: process.env.LOCALAPPDATA, USERPROFILE: process.env.USERPROFILE });
    const last = lastActiveMs(dir, info.at);
    const lease = readLease(dir);
    const v = q.leaseVerdict({ lastActiveAt: last, leaseUntil: Date.parse(lease.leaseUntil), now, idleMin: settings.sessionIdleMin });
    idle = mins(v.idleMs);
    const waiting = tickets.filter((t) => waitsOn(t).some((w) => w === lane.lock || w === "ram")).length;
    note = v.leased
      ? `leased ${mins(Date.parse(lease.leaseUntil) - now)} more`
      : v.expired
        ? waiting ? "idle past the limit: closes at the next poll" : `idle past ${settings.sessionIdleMin}m; closes when a job queues`
        : `closes after ${mins(settings.sessionIdleMin * 60000 - v.idleMs)} more idle if a job waits`;
    note += `; ${holdNote(dir, info, lease, tickets, lane, settings, now)}`;
  }
  return `${head} ${who.padEnd(16).slice(0, 22)} ${purpose.padEnd(10)} age ${age.padStart(4)}  idle ${idle.padStart(4)}  ${note}`.trimEnd();
}

// Projected release time (epoch ms) of every lane of a kind on a machine, from its lock records:
// a free lane now, a session at its typical hold (never past its hold cap plus grace, which applies
// because someone waits), a test at the median run of its kind there plus boot.
export function laneReleases(m, kind, infos, { now = Date.now(), runs = [], settings, sessionCapMin = {} } = {}) {
  const sessions = runs.filter((r) => r.kind === "session");
  const holdMs = q.median(sessions.slice(-50).map((r) => Number(r.heldS) * 1000)) || 10 * 60000;
  return lanesFor(m, kind).map((l) => {
    const i = infos[l.lock];
    if (!i || i.alive === false || i.stale) return now;
    const atMs = Date.parse(i.at);
    if (i.purpose === "session" || !i.purpose) {
      const capMin = sessionCapMin[l.lock] ?? settings.sessionHoldMin;
      return q.projectedRelease({ atMs, now, typicalMs: holdMs, capMs: (capMin + settings.sessionGraceMin) * 60000 });
    }
    const durMs = q.median(runs.filter((r) => r.machine === m.name && r.purpose === i.purpose && r.result === "pass").slice(-30).map((r) => Number(r.durS) * 1000)) || 60000;
    return q.projectedRelease({ atMs, now, typicalMs: durMs + settings.bootBudgetS * 1000 });
  });
}

function clock(ms) {
  const d = new Date(ms);
  return `${String(d.getHours()).padStart(2, "0")}:${String(d.getMinutes()).padStart(2, "0")}`;
}

// Default output is what a decision needs: busy lanes, the queue with reasons and estimated starts, the
// budgets and a one-line history. `pool status --verbose` adds the free and stale lanes, the last runs and
// the last sessions.
export function cmdPoolStatus(root, verbose = process.env.LF_OUTPUT === "full") {
  const settings = poolSettings(root);
  const machines = loadMachines(root);
  const hereBuild = tb.serverBuildId(tb.localServerDir(root));
  const now = Date.now();
  const tickets = liveTickets(root);
  const runs = readRuns(root);
  // The idle rules act on every look, not only while a job polls: a session left open past
  // sessionIdleMaxMin (or past sessionIdleMin while a job waits for its machine) is closed first.
  try {
    reapIdleSessions(root, machines.filter((x) => x.via === "local"), tickets, settings);
    for (const m of machines.filter((x) => x.via === "ssh")) {
      if (probe(root, m).reachable) reapRemoteIdleSessions(root, m, settings, { waiting: tickets.some((t) => (t.lane ?? "session") === "session" && (t.machines ?? []).includes(m.name)) });
    }
  } catch (e) {
    console.log(`WARN  pool: idle-session check failed (${String(e?.message ?? e).split("\n")[0]})`);
  }
  // Per machine: reachable, its lane locks (alive or stale), its live free RAM.
  const seen = new Map();
  for (const m of machines) {
    const h = probe(root, m, { fresh: true });
    const where = m.via === "local" ? `this PC (${hostname()})` : `ssh ${m.host}`;
    const build = h.build ? `build ${h.build}${hereBuild && h.build !== hereBuild ? " (DIFFERS)" : ""}` : "";
    const lanes = m.lanes ? `up to ${m.lanes.test} test + ${m.lanes.session} session lane(s) (port range allows ${q.MAX_TEST_LANES} + ${q.MAX_SESSION_LANES}), as RAM allows` : `${m.slots} shared slot(s)`;
    for (const n of m.laneNotes ?? []) console.log(`WARN  ${m.name}: ${n}`);
    const st = h.reachable && m.via !== "local" ? remoteState(m) : null;
    const heat = st && Number.isFinite(st.thermalC) ? `  ${st.thermalC} °C (holds new jobs at ${m.thermalHoldC})` : m.thermalLog && st ? "  temperature unknown (thermal log stale)" : "";
    const ram = m.via === "local" ? ramText(root) : st && Number.isFinite(st.freeMb) ? `RAM ${(st.freeMb / 1024).toFixed(1)}/${(st.totalMb / 1024).toFixed(1)} GB free (floor ${(settings.minFreeMb / 1024).toFixed(1)})${heat}` : "";
    const head = `${m.name}  ${where}  ${h.reachable ? "up" : "DOWN"}  ${build}  ${ram ? `${ram}  ` : ""}${lanes}`.replace(/ {2,}$/, "");
    if (!h.reachable) {
      console.log(head);
      continue;
    }
    const laneRows = [];
    const infos = {};
    const capMin = {};
    const rows = m.lanes
      ? [...lanesFor(m, "session").map((l) => ["session", l]), ...lanesFor(m, "test").map((l) => ["test", l])]
      : lanesFor(m, "test").map((l) => ["shared", l]);
    for (const [kind, lane] of rows) {
      let info;
      if (m.via === "local") {
        info = readJson(localLockFile(root, lane.lock), null);
        if (info && !pidAlive(info.pid)) info = { ...info, stale: true };
        if (info?.purpose === "session" && info.root) {
          const sess = readJson(join(sessionDirOf(info.root, { LOCALAPPDATA: process.env.LOCALAPPDATA, USERPROFILE: process.env.USERPROFILE }), "session.json"), null);
          if (sess?.holdCap) capMin[lane.lock] = q.clampHoldMin(sess.holdMin, { holdMin: settings.sessionHoldMin, maxMin: settings.sessionHoldMaxMin });
        }
      } else {
        info = st ? st.locks[lane.lock] : undefined;
        if (info && !info.alive) info = { ...info, stale: true };
      }
      infos[lane.lock] = info;
      let row = laneRow(root, kind, lane, info, tickets, settings, now);
      if (m.via !== "local" && info?.origin && remoteOwnerGone(info)) row += `; STALE: owner pid ${info.originPid} gone here`;
      laneRows.push(row);
    }
    const idle = laneRows.filter(isIdleLaneRow);
    const shown = verbose ? laneRows : laneRows.filter((r) => !idle.includes(r));
    console.log(verbose || !idle.length ? head : `${head}  (${idle.length} lane(s) free or stale)`);
    for (const r of shown) console.log(r);
    const freeMb = m.via === "local" ? Math.round(freemem() / 1048576) : st?.freeMb;
    seen.set(m.name, { m, infos, capMin, freeMb });
  }
  const diskGb = gc.freeDiskGb(sessionBase());
  console.log(statusLine(root)); // the Workbench limit of this PC (tools/lib/wb-lock.mjs)
  console.log(`disk: ${diskGb === null ? "unknown" : `${diskGb.toFixed(1)} GB free`} on the session volume (floor ${settings.minFreeDiskGb} GB; session folders older than ${settings.sessionKeepHours} h are removed on the next session or test start)`);
  const promoteMs = settings.promoteMin * 60000;
  const starveMs = settings.starveMin * 60000;
  const ordered = q.queueOrder(tickets, now, promoteMs);
  console.log(`queue: ${ordered.length} waiting (high > normal > low, then arrival; a job moves up a level per ${settings.promoteMin}m waited; one stuck on RAM is passed by jobs that fit until it has waited ${settings.starveMin}m, then it holds its machine)`);
  const perLane = new Map();
  const dupFiles = q.duplicateSessionTickets(tickets);
  const freeMbOf = (name) => seen.get(name)?.freeMb;
  for (const t of ordered) {
    const lane = t.lane ?? "session";
    if (!verbose && (perLane.get(lane) ?? 0) >= 4) {
      perLane.set(lane, (perLane.get(lane) ?? 0) + 1);
      continue;
    }
    const pos = (perLane.get(lane) ?? 0) + 1;
    perLane.set(lane, pos);
    const base = q.normalizePriority(t.priority) ?? "normal";
    const eff = q.PRIORITIES[q.effectiveRank(t, now, promoteMs)];
    const prio = eff === base ? base : `${base}>${eff}`;
    const why = q.waitReason(t, pos - 1, { now, starveMs, freeMbOf, otherMachines: machines.map((x) => x.name) });
    // Estimated start: the (ahead+1)-th projected lane release on the machines it can use. A RAM wait
    // ends when a session on those machines lets go (a test frees little and is gone in seconds).
    const releases = [];
    for (const name of t.machines ?? []) {
      const s = seen.get(name);
      if (!s) continue;
      const r = laneReleases(s.m, t.stuck === "ram" && s.m.lanes ? "session" : lane, s.infos, { now, runs, settings, sessionCapMin: s.capMin });
      releases.push(...(t.stuck === "ram" ? r.filter((x) => x > now) : r));
    }
    const holdMs = q.median(runs.filter(isSession).slice(-50).map((r) => Number(r.heldS) * 1000)) || 10 * 60000;
    const eta = q.estimateStart(releases, t.stuck === "ram" ? 0 : pos - 1, lane === "session" ? holdMs : 60000);
    const etaText = eta === null ? "  est. start unknown (no machine of it answers)" : eta <= now + 30000 ? "  est. start now" : `  est. start ~${clock(eta)} (in ${mins(eta - now)})`;
    console.log(`  ${lane} #${pos}  ${prio.padEnd(11)} ${String(t.slug ?? "?").padEnd(20).slice(0, 24)} ${String(t.purpose ?? "").padEnd(11)} waited ${mins(now - Date.parse(t.at)).padStart(4)}  on ${(t.machines ?? []).join("|")}${t.clients ? ` (${t.clients}-client ${t.profile ?? "full"})` : ""}${t.pinned ? " (pinned)" : ""}`);
    console.log(`        waits: ${why || (t.waitingOn ?? []).join(",") || "its turn"};${etaText}${dupFiles.has(t.file) ? "  DUPLICATE (same worktree already queued)" : ""}`);
  }
  try {
    const all = runs.filter(isTestRun);
    const local = machines.find((x) => x.via === "local");
    if (local) {
      const parts = Object.keys(q.DEFAULT_BUDGET_S).map((k) => {
        const b = q.budgetSeconds(all, k, local.name, { multiplier: local.budgetMultiplier });
        return `${k.replace("test ", "")} ${b.runS}s${b.source.includes("history") ? "" : "*"}${b.source.includes("raised") ? "^" : ""}`;
      });
      const recent = all.slice(-100);
      const to = recent.filter((r) => r.result === "timeout").length;
      console.log(`budgets (per run, boot excluded; * seeded, ^ raised after timeouts): ${parts.join(", ")}; boot ${settings.bootBudgetS}s; timeouts ${to}/${recent.length} recent runs`);
    }
  } catch {}
  const last = runs.filter(isTestRun).slice(-8);
  if (!verbose) {
    const n = (res) => last.filter((r) => r.result === res).length;
    console.log(`last ${last.length} run(s): ${n("pass")} pass, ${last.filter((r) => r.result !== "pass" && r.result !== "timeout").length} other, ${n("timeout")} timeout; --verbose lists them and the sessions`);
    return 0;
  }
  console.log(`last ${last.length} run(s):`);
  for (const r of last) {
    console.log(`  ${localTime(r.at)}  ${String(r.machine).padEnd(11)} ${String(r.slug).padEnd(20)} ${String(r.purpose).padEnd(10)} wait ${String(r.waitS).padStart(6)} s  ran ${String(r.durS).padStart(6)} s  ${r.result}${r.passed !== undefined ? ` ${r.passed}/${r.total}` : ""}`);
  }
  printSessions(runs.filter(isSession), 8);
  return 0;
}

const isSession = (r) => r.kind === "session";
const isTestRun = (r) => r.purpose !== "reclaim" && !isSession(r);

// The session lane's history: the last few sessions and the averages that show whether
// the lane flows (how long a request waited, how long a holder kept it).
function printSessions(sessions, last) {
  if (!sessions.length) {
    console.log("sessions: none recorded yet (each one is written to runs.jsonl when it ends)");
    return;
  }
  const st = q.sessionStats(sessions);
  const c = st.closed;
  console.log(`sessions: ${st.n} recorded; avg wait ${mins(st.avgWaitS * 1000)}, avg hold ${mins(st.avgHeldS * 1000)}, ${st.avgBoots.toFixed(1)} boot(s) each; closed by owner ${c.owner}, lease ${c.lease}, hold-cap ${c["hold-cap"]}`);
  for (const r of sessions.slice(-last)) {
    const boot = (r.bootS ?? []).length ? `  boot ${r.bootS.map((x) => `${Math.round(x)}s`).join("+")}` : "";
    console.log(`  ${localTime(r.at)}  ${String(r.slug).padEnd(20).slice(0, 24)} ${String(r.priority ?? "normal").padEnd(7)} wait ${mins(r.waitS * 1000).padStart(5)}  held ${mins(r.heldS * 1000).padStart(5)}  boots ${r.boots}${boot}  closed by ${r.closedBy}`);
  }
}

export function cmdPoolStats(root) {
  const all = readRuns(root);
  const reclaims = all.filter((r) => r.purpose === "reclaim");
  const runs = all.filter(isTestRun);
  const by = new Map();
  for (const r of runs) {
    const k = r.machine;
    if (!by.has(k)) by.set(k, []);
    by.get(k).push(r);
  }
  console.log(`pool stats: ${runs.length} run(s)`);
  for (const [k, rs] of by) {
    const avg = (f) => (rs.reduce((a, r) => a + (Number(r[f]) || 0), 0) / rs.length).toFixed(1);
    const pass = rs.filter((r) => r.result === "pass").length;
    const rc = reclaims.filter((r) => r.machine === k);
    const rcText = rc.length ? `  reclaimed ${rc.length} (dead owner ${rc.filter((r) => r.kind === "dead-owner").length}, hang ${rc.filter((r) => r.kind === "hang").length})` : "";
    console.log(`  ${k.padEnd(12)} runs ${String(rs.length).padStart(4)}  pass ${pass}  avg wait ${avg("waitS")} s  avg run ${avg("durS")} s${rcText}`);
  }
  printSessions(all.filter(isSession), 5);
  if (reclaims.length) {
    console.log(`reclaimed slots (last ${Math.min(5, reclaims.length)} of ${reclaims.length}):`);
    for (const r of reclaims.slice(-5)) console.log(`  ${localTime(r.at)}  ${String(r.machine).padEnd(11)} ${r.slot}  ${r.slug} ${r.victimPurpose ?? ""}: ${r.kind}, ${r.reason}`);
  }
  return 0;
}

// Removes run folders on the remotes whose origin worktree is gone: the folder
// says which PC and slug it belongs to (tmp/pool-owner.json); only this PC's
// are touched. --unmarked also takes folders from before the marker whose slug
// has no worktree here. Folders behind a live slot lock are kept.
export function cmdPoolPrune(root, flags = []) {
  const dry = flags.includes("--dry-run");
  const unmarked = flags.includes("--unmarked");
  const known = new Set(Object.keys(wt.readState(root).worktrees ?? {}));
  const known2 = new Set(wt.listWorktrees(root).map((w) => w.path.replace(/\\/g, "/").toLowerCase()));
  let fail = 0;
  for (const m of loadMachines(root).filter((x) => x.via === "ssh")) {
    if (!probe(root, m, { fresh: true }).reachable) {
      console.log(`pool prune: ${m.name} down; skipped`);
      continue;
    }
    const list = tb.sshPs(m.host, [
      `$main = ${tb.psQuote(m.root)}`,
      "$busy = @(Get-ChildItem -LiteralPath (Join-Path $main 'tmp\\wt\\locks') -Filter 'server-*.lock' -ErrorAction SilentlyContinue | ForEach-Object { try { $i = Get-Content -Raw -LiteralPath $_.FullName | ConvertFrom-Json; if ($i.rDir -and (Get-Process -Id $i.pid -ErrorAction SilentlyContinue)) { $i.rDir } } catch {} })",
      "git -C $main worktree list --porcelain | Where-Object { $_ -like 'worktree *' } | ForEach-Object { $d = $_.Substring(9); if ((Split-Path -Leaf $d) -like 'Life-Framework-rt-*') { $o = Join-Path $d 'tmp\\pool-owner.json'; $own = ''; if (Test-Path -LiteralPath $o) { $own = (Get-Content -Raw -LiteralPath $o).Trim() }; $b = [bool]($busy | Where-Object { $_.Replace('\\','/').TrimEnd('/') -ieq $d.Replace('\\','/').TrimEnd('/') }); Write-Host \"lf-pool: dir $b $d|$own\" } }",
    ].join("\n"), { timeout: 60000 });
    const victims = [];
    for (const l of (list.stdout ?? "").split(/\r?\n/)) {
      const mm = /^lf-pool: dir (True|False) ([^|]+)\|(.*)$/.exec(l.trim());
      if (!mm) continue;
      const dir = mm[2];
      const own = readJsonText(mm[3] || "null");
      const leaf = dir.replace(/\\/g, "/").split("/").pop();
      let reason = null;
      if (own && own.origin) {
        if (String(own.origin).toLowerCase() !== hostname().toLowerCase()) continue;
        const wtGone = own.worktree ? !known2.has(String(own.worktree).toLowerCase()) && !existsSync(own.worktree) : !known.has(own.slug);
        if (wtGone) reason = `worktree ${own.worktree ?? own.slug} is gone`;
      } else if (unmarked) {
        const slug = leaf.replace(/^Life-Framework-rt-/, "").replace(/-session$/, "").replace(/-s\d+$/, "");
        if (!known.has(slug)) reason = `unmarked, no worktree '${slug}' here`;
      }
      if (!reason) continue;
      if (mm[1] === "True") {
        console.log(`pool prune: ${m.name} ${dir} is in use; kept`);
        continue;
      }
      victims.push(dir);
      console.log(`pool prune: ${m.name} ${dir}: ${reason}${dry ? " (dry run)" : ""}`);
    }
    if (dry || victims.length === 0) continue;
    const code = tb.removeRemoteDirs(m.host, m.root, victims);
    if (code !== 0) fail = 1;
  }
  return fail;
}

export function cmdPool(root, args = []) {
  const [sub, ...rest] = args;
  if (sub === "status" || !sub) return cmdPoolStatus(root, rest.includes("--verbose") || process.env.LF_OUTPUT === "full");
  if (sub === "stats") return cmdPoolStats(root);
  if (sub === "prune") return cmdPoolPrune(root, rest);
  console.log(`usage: tools\\cli pool status|stats|prune [--dry-run] [--unmarked]
  status   machines, reachability, server build, slots and holders, queue, last 20 runs
  stats    runs per machine: count, passes, average wait and run time
  prune    remove remote run folders whose worktree on this PC is gone (junction-safe)
machines: tools/pool.json, overridden by "pool" in lf.local.json`);
  return sub === "help" ? 0 : 2;
}

// Syncs this worktree to <box>/Life-Framework-rt-<slug><suffix> without taking
// a slot, then runs `node <script> <args>` there (sessions: the remote
// `session up` takes the box's server-2001 itself and hands it to its server).
export async function syncAndRun(root, m, { script, args, suffix = "-session", purpose = "session", onSlot }) {
  const snap = tb.snapshotCommit(root);
  const cfg = holderConfig(root, m, snap, { slots: 0, purpose, suffix });
  const r = await startHolder(root, m, cfg);
  if (!r.slot) {
    console.log(`FAIL  ${r.error ?? `${m.name} busy`}`);
    return { code: 3 };
  }
  r.slot.snap = snap;
  onSlot?.(r.slot.rDir);
  const out = await tb.runRemote(root, r.slot, { args, script, writeSyncState: (rt, name, rec) => writeSyncState(rt, `${name}${suffix}`, rec), fetchLog: false });
  return { ...out, rDir: r.slot.rDir };
}

// Runs `node <script> <args>` in an existing run folder, no sync (a live
// session server runs out of it; a checkout would change files under it).
export function runIn(m, dir, script, args) {
  const ps = [`Set-Location -LiteralPath ${tb.psQuote(dir)}`, `& node ${tb.psQuote(script)} ${args.map(tb.psQuote).join(" ")}`, "exit $LASTEXITCODE"].join("\n");
  return tb.sshPsStream(m.host, ps, () => {});
}

// Used by sessions: the first machine with a free slot 1 lock, without
// holding it (session up takes it itself and hands it to its server).
export function freeSlotOn(root, m) {
  if (m.via === "local") {
    const info = readJson(localLockFile(root, slotLockName(1)), null);
    return !info || !pidAlive(info.pid);
  }
  const locks = remoteLocks(m);
  if (!locks) return false;
  const info = locks[slotLockName(1)];
  return !info || !info.alive;
}
