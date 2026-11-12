// Pure rules of the test pool queue (tools/remote/pool.mjs): lanes, priority
// order with aging, the RAM gate, and the session lease. No I/O, so
// tools/validation/pool-selftest.mjs proves them without a game.

export const PRIORITIES = ["low", "normal", "high"];
export const DEFAULTS = {
  sessionIdleMin: 15,
  promoteMin: 20, starveMin: 20,
  minFreeMb: 1500,
  // Memory a second concurrent session takes: the server, plus each game client (MB; measured
  // 2026-11-10, docs/worktrees.md "Session lanes"). The second session starts only while free RAM
  // minus this still clears minFreeMb.
  sessionReserveServerMb: 600,
  sessionReserveClientMb: 850,
  // Per session profile (session up --profile): the same two numbers, measured per profile. A profile
  // missing here uses the full numbers above. tools/pool.json "sessionReserve" overrides these.
  sessionReserve: { lowspec: { serverMb: 600, clientMb: 800 } },
  bootBudgetS: 30,
  // A session older than sessionHoldMin while another session request queues is warned, then
  // closed sessionGraceMin later. session up --hold raises its own cap, to sessionHoldMaxMin at most.
  sessionHoldMin: 20,
  sessionGraceMin: 5,
  sessionHoldMaxMin: 45,
  // Session folders older than this many hours (not in use, not film) are removed; a session or
  // test refuses to start with less free disk than minFreeDiskGb (docs/worktrees.md "Disk hygiene").
  sessionKeepHours: 24,
  minFreeDiskGb: 5,
};

// "high" | "normal" | "low" (any case), or null when it is not one.
export function normalizePriority(v) {
  const s = String(v ?? "").trim().toLowerCase();
  return PRIORITIES.includes(s) ? s : null;
}

export function priorityRank(p) {
  const i = PRIORITIES.indexOf(normalizePriority(p) ?? "normal");
  return i < 0 ? 1 : i;
}

// A job that waited N minutes moves up one level per N (low -> normal -> high),
// so a stream of higher work cannot starve it: after 2N a low job is "high" and,
// having arrived before every newer job, is served first. Running jobs are never touched.
export function effectiveRank(ticket, now, promoteMs) {
  const base = priorityRank(ticket.priority);
  const waited = now - Date.parse(ticket.at);
  const steps = promoteMs > 0 && Number.isFinite(waited) && waited > 0 ? Math.floor(waited / promoteMs) : 0;
  return Math.min(PRIORITIES.length - 1, base + steps);
}

// True when ticket a is served before ticket b: higher effective priority,
// then arrival (ticket file names sort by arrival time).
export function servedBefore(a, b, now, promoteMs) {
  const ra = effectiveRank(a, now, promoteMs);
  const rb = effectiveRank(b, now, promoteMs);
  if (ra !== rb) return ra > rb;
  return String(a.file) < String(b.file);
}

export function queueOrder(tickets, now, promoteMs) {
  return [...tickets].sort((a, b) => (servedBefore(a, b, now, promoteMs) ? -1 : servedBefore(b, a, now, promoteMs) ? 1 : 0));
}

// Lane locks of a machine. A machine with "lanes" in pool.json gets separate
// test and session lanes; one without keeps the single shared lane (slot k is
// server-2001, server-2001-s<k>).
export function slotLockName(i) {
  return i === 1 ? "server-2001" : `server-2001-s${i}`;
}

export const SESSION_PORT = 2001;
// Session lane k listens on 2001 + 10 (k - 1), lock server-<port>; test lanes use 2002.. beside them.
export const SESSION_LANE_STEP = 10;

export function sessionLane(i) {
  const port = SESSION_PORT + SESSION_LANE_STEP * (i - 1);
  return { lock: i === 1 ? slotLockName(1) : `server-${port}`, index: i, port };
}

// The A2S query port of a session lane's server: the engine default for lane 1 (null, no flag),
// 17777 + 10 x the port offset beside it.
export function sessionA2sPort(port) {
  return port === SESSION_PORT ? null : 17777 + (port - SESSION_PORT) * 10;
}

// Memory (MB) a session of n clients adds, from the reserve settings; profile is the session's
// resource profile ("full" or "lowspec"), whose own measured numbers win when settings carry them.
export function sessionReserveMb(settings, clients, profile = "full") {
  const own = settings.sessionReserve?.[profile];
  const server = own?.serverMb ?? settings.sessionReserveServerMb ?? DEFAULTS.sessionReserveServerMb;
  const client = own?.clientMb ?? settings.sessionReserveClientMb ?? DEFAULTS.sessionReserveClientMb;
  return server + client * Math.max(0, Number(clients) || 0);
}

export function laneKind(purpose) {
  return /^(session|host|serve)/.test(String(purpose ?? "")) ? "session" : "test";
}

// [{ lock, index, port }] for the lane kind.
export function lanesFor(m, kind) {
  if (!m.lanes) return Array.from({ length: m.slots }, (_, i) => ({ lock: slotLockName(i + 1), index: i + 1, port: SESSION_PORT }));
  if (kind === "session") return Array.from({ length: m.lanes.session ?? 1 }, (_, i) => sessionLane(i + 1));
  return Array.from({ length: m.lanes.test }, (_, i) => ({ lock: `server-test-${i + 1}`, index: i + 1, port: SESSION_PORT + 1 + i }));
}

export function allLaneLocks(m) {
  return [...new Set([...lanesFor(m, "session"), ...lanesFor(m, "test")].map((l) => l.lock))];
}

// The UDP port a server booted under that lock listens on.
export function portForLock(name) {
  const mm = /^server-test-(\d+)$/.exec(String(name));
  if (mm) return SESSION_PORT + Number(mm[1]);
  const sm = /^server-(\d+)$/.exec(String(name));
  return sm ? Number(sm[1]) : SESSION_PORT;
}

// Whether `other` makes `mine` wait on machine m: it is served first and its
// lane shares a lock with mine. A ticket without a lane (an older CLI) is a
// session-lane ticket: it contends for server-2001.
// Backfill: an `other` that already tried and cannot start (t.stuck "ram" or "lane") does
// not block, so the next job that can start right now goes. Anti-starvation: once such a
// head has waited ctx.starveMs it holds its place again as soon as the RAM it needs is there
// (ctx.freeMb >= t.needMb), or at once when it only lacks a lane.
// ctx: { starveMs, freeMb } (both optional).
export function blocks(other, mine, m, now, promoteMs, ctx = {}) {
  if (!(other.machines ?? []).includes(m.name)) return false;
  const a = new Set(lanesFor(m, other.lane ?? "session").map((l) => l.lock));
  if (!lanesFor(m, mine.lane ?? "test").some((l) => a.has(l.lock))) return false;
  if (!servedBefore(other, mine, now, promoteMs)) return false;
  // A head that lacks only a lane is raced for it when that lane frees, so it keeps its turn
  // unless mine can also use a lane the head cannot.
  if (other.stuck === "lane" && lanesFor(m, mine.lane ?? "test").every((l) => a.has(l.lock))) return true;
  return !canBackfill(other, now, ctx);
}

// True when a ticket that is served first is stuck (RAM or lane) and not yet owed a reservation.
export function canBackfill(t, now, { starveMs = DEFAULTS.starveMin * 60000, freeMb } = {}) {
  if (t.stuck !== "ram" && t.stuck !== "lane") return false;
  const starved = starveMs > 0 && now - Date.parse(t.at) > starveMs;
  if (!starved) return true;
  if (t.stuck === "ram") return !(Number.isFinite(freeMb) && Number.isFinite(t.needMb) && freeMb >= t.needMb);
  return false;
}

// Why a ticket waits, for `pool status`: "ram 3-client", "lane", "behind N", or "".
export function waitReason(t, ahead = 0) {
  if (t.stuck === "ram") return `ram${t.clients ? ` ${t.clients}-client` : ""}`;
  if (t.stuck === "lane") return "lane";
  return ahead > 0 ? `behind ${ahead}` : "";
}

// RAM gate: a server beyond the first waits while free memory is under the
// floor, instead of letting Windows page.
// A second session also needs reserveMb (its own expected memory) on top of the floor, and
// pendingMb counts sessions still booting, whose memory has not shown in freeMb yet.
export function ramVerdict({ running, freeMb, minFreeMb, reserveMb = 0, pendingMb = 0 }) {
  if (running <= 0) return { ok: true };
  const after = freeMb - pendingMb - reserveMb;
  if (after >= minFreeMb) return { ok: true };
  const gb = (x) => (x / 1024).toFixed(1);
  const needMb = minFreeMb + reserveMb + pendingMb;
  if (reserveMb > 0 || pendingMb > 0) {
    return { ok: false, needMb, reason: `free RAM ${gb(freeMb)} GB minus ${gb(reserveMb + pendingMb)} GB for the session${pendingMb > 0 ? "(s) starting" : ""} would be under ${gb(minFreeMb)} GB` };
  }
  return { ok: false, needMb, reason: `free RAM ${gb(freeMb)} GB under ${gb(minFreeMb)} GB with ${running} server(s) running` };
}

// RAM gate for a late client (`session join`): the session already runs, so only the client's own
// reserve has to fit above the floor.
export function joinVerdict({ freeMb, minFreeMb, clientMb }) {
  const needMb = minFreeMb + clientMb;
  if (freeMb >= needMb) return { ok: true, needMb };
  const gb = (x) => (x / 1024).toFixed(1);
  return { ok: false, needMb, reason: `free RAM ${gb(freeMb)} GB minus ${gb(clientMb)} GB for the client would be under ${gb(minFreeMb)} GB` };
}

// Session lease: idle time since the last harness command (or the session's
// start), and whether an explicit lease still covers it.
export function leaseVerdict({ lastActiveAt, leaseUntil, now, idleMin }) {
  const idleMs = Number.isFinite(lastActiveAt) ? Math.max(0, now - lastActiveAt) : 0;
  const leased = Number.isFinite(leaseUntil) && leaseUntil > now;
  return { idleMs, leased, expired: !leased && Number.isFinite(lastActiveAt) && idleMs > idleMin * 60000 };
}

// Hold cap: how long a session has held the lane, against its cap, while another
// session request queues. A session that never got the cap (started before it existed)
// is never judged. Phases: ok | warn (past the cap, not warned yet) | grace (warned,
// the grace runs) | close (grace over). warnedAt is epoch ms or NaN.
export function clampHoldMin(v, { holdMin, maxMin }) {
  const n = Number(v);
  if (!Number.isFinite(n) || n <= 0) return holdMin;
  return Math.min(maxMin, Math.max(holdMin, Math.round(n)));
}

export function holdVerdict({ heldSinceMs, capMin, graceMin, warnedAtMs, waiting, now, capped = true }) {
  const heldMs = Number.isFinite(heldSinceMs) ? Math.max(0, now - heldSinceMs) : 0;
  if (!capped || !waiting || !Number.isFinite(heldSinceMs) || heldMs <= capMin * 60000) return { phase: "ok", heldMs };
  if (!Number.isFinite(warnedAtMs)) return { phase: "warn", heldMs };
  const left = warnedAtMs + graceMin * 60000 - now;
  return left > 0 ? { phase: "grace", heldMs, leftMs: left } : { phase: "close", heldMs };
}

// Averages over the session records of runs.jsonl (kind "session").
export function sessionStats(sessions) {
  const n = sessions.length;
  const avg = (f) => (n ? sessions.reduce((a, r) => a + (Number(r[f]) || 0), 0) / n : 0);
  const closed = { owner: 0, lease: 0, "hold-cap": 0 };
  for (const r of sessions) closed[r.closedBy] = (closed[r.closedBy] ?? 0) + 1;
  return { n, avgWaitS: avg("waitS"), avgHeldS: avg("heldS"), avgBoots: avg("boots"), closed };
}

// ---------------------------------------------------------------- runtime budgets

export const TIMEOUT_CODE = 124;
export const BUDGET_FLOOR_S = 30;
export const BUDGET_MIN_RUNS = 5;
export const TIMEOUT_RATE_RAISE = 0.02;
export const BOOT_BUDGET_S = 30;
// Starting budgets (seconds) until a kind has history. e2e is per pass.
export const DEFAULT_BUDGET_S = { "test only": 30, "test fast": 45, "test all": 90, "test persistence": 120, e2e: 120 };

export function p95(values) {
  const v = values.filter((x) => Number.isFinite(x) && x > 0).sort((a, b) => a - b);
  return v.length ? v[Math.max(0, Math.ceil(0.95 * v.length) - 1)] : 0;
}

// Budget for one run kind on one machine, in seconds, excluding boot:
//   3x the p95 of that machine's recent passing runs of the kind, never under
//   the 30 s floor; a seeded default (times the machine multiplier) until it
//   has 5 passes. A kind whose recent runs timed out more than 2% of the time
//   gets double, so a budget that is too tight corrects itself.
// runs: the pool's runs.jsonl records. Returns { runS, source, rate }.
export function budgetSeconds(runs, kind, machine, { multiplier = 1, override } = {}) {
  if (Number.isFinite(override) && override > 0) return { runS: Math.round(override), source: "override", rate: 0 };
  const mine = runs.filter((r) => r.purpose === kind && r.machine === machine).slice(-50);
  const perPass = kind === "e2e" ? 2 : 1;
  const passes = mine.filter((r) => r.result === "pass").slice(-30).map((r) => Number(r.durS) / perPass);
  const timeouts = mine.filter((r) => r.result === "timeout").length;
  const rate = mine.length ? timeouts / mine.length : 0;
  let runS;
  let source;
  if (passes.length >= BUDGET_MIN_RUNS) {
    runS = Math.max(BUDGET_FLOOR_S, 3 * p95(passes));
    source = "history";
  } else {
    runS = (DEFAULT_BUDGET_S[kind] ?? 90) * multiplier;
    source = "default";
  }
  if (rate > TIMEOUT_RATE_RAISE) {
    runS *= 2;
    source += "+raised";
  }
  return { runS: Math.ceil(runS), source, rate };
}

// One session request per worktree (issue #542): a retry whose earlier `session up` is still
// queued (its caller timed out but the process lives) must not stack. Given the live tickets and
// the ticket a request just wrote, returns the files of the OTHER session tickets from the same
// worktree (same slug, session lane, other pid) that the new request replaces.
export function supersededTickets(tickets, mine) {
  if (!mine?.slug || (mine.lane ?? "session") !== "session") return { drop: [] };
  const same = tickets.filter((t) => t.file !== mine.file && t.pid !== mine.pid && t.slug === mine.slug && (t.lane ?? "session") === "session");
  return { drop: same.map((t) => t.file) };
}

// Session tickets of one worktree beyond the first (arrival order): what `pool status` marks DUPLICATE.
export function duplicateSessionTickets(tickets) {
  const seen = new Set();
  const dups = new Set();
  for (const t of [...tickets].sort((a, b) => (String(a.file) < String(b.file) ? -1 : 1))) {
    if ((t.lane ?? "session") !== "session" || !t.slug) continue;
    if (seen.has(t.slug)) dups.add(t.file);
    seen.add(t.slug);
  }
  return dups;
}
