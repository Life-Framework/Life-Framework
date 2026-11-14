// Pure rules of the test pool queue (tools/remote/pool.mjs): lanes, priority
// order with aging, the RAM gate, and the session lease. No I/O, so
// tools/validation/pool-selftest.mjs proves them without a game.

export const PRIORITIES = ["low", "normal", "high"];
export const DEFAULTS = {
  // Sessions flow like a river: a lane is held for the window it is used. A session idle this long
  // while a job waits for its lane is closed; one idle sessionIdleMaxMin is closed with nobody
  // waiting (it holds RAM and client windows). Any harness or session command resets the clock.
  sessionIdleMin: 3,
  sessionIdleMaxMin: 5,
  promoteMin: 20, starveMin: 20,
  minFreeMb: 1500,
  // RAM headroom past which a machine counts as roomy: the speed (budgetMultiplier) then decides
  // between machines, not more RAM (rankMachines).
  roomyMb: 6144,
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
  sessionHoldMin: 10,
  sessionGraceMin: 2,
  sessionHoldMaxMin: 20,
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

// Lane ports. The only ceiling on lanes in code is the port range below; the machine's lanes
// setting, the live RAM gate, the thermal hold and maxClients decide what runs (docs/worktrees.md
// "Lanes"). The first lanes keep the names and ports pool code of before 2026-11-14 uses (it runs
// beside this code on older branches and shares the lock dirs): session lanes 1-4 on 2001, 2011,
// 2021, 2031 and test lanes 1-9 (server-test-<k>) on 2002-2010. Later lanes sit in blocks no older
// code uses, one lane per 5 ports, each lock named server-<port> so older code that only sees the
// lock name (portForLock, a remote run of an older snapshot) still binds the right port. Every
// lane port ends in 3 or 8: never a worktree game port (2001 + 10 i, ends in 1), never an A2S port
// (17777 + 10 i, ends in 7), and below 17777. A block session lane's A2S port is its game port + 1
// (ends in 4 or 9; the block reserves it for test lanes too). Test runs keep passing the worktree's
// A2S port. Pool selftest "Lane ports" enumerates all of them.
export const LEGACY_TEST_LANES = 9;
export const LEGACY_SESSION_LANES = 4;
export const MAX_TEST_LANES = 64;
export const MAX_SESSION_LANES = 32;
export const TEST_BLOCK_PORT = 2403; // test lanes 10..64: 2403..2678
export const SESSION_BLOCK_PORT = 2803; // session lanes 5..32: 2803..2938
const BLOCK_STEP = 5;

export function laneCap(kind) {
  return kind === "session" ? MAX_SESSION_LANES : MAX_TEST_LANES;
}

export function sessionLane(i) {
  if (i > LEGACY_SESSION_LANES) {
    const port = SESSION_BLOCK_PORT + BLOCK_STEP * (i - LEGACY_SESSION_LANES - 1);
    return { lock: `server-${port}`, index: i, port };
  }
  const port = SESSION_PORT + SESSION_LANE_STEP * (i - 1);
  return { lock: i === 1 ? slotLockName(1) : `server-${port}`, index: i, port };
}

export function testLane(i) {
  if (i > LEGACY_TEST_LANES) {
    const port = TEST_BLOCK_PORT + BLOCK_STEP * (i - LEGACY_TEST_LANES - 1);
    return { lock: `server-${port}`, index: i, port };
  }
  return { lock: `server-test-${i}`, index: i, port: SESSION_PORT + i };
}

// The A2S query port of a session lane's server: the engine default for lane 1 (null, no flag),
// 17777 + 10 x the port offset for lanes 2-4 (as before), the game port + 1 for a block lane.
// (Observed 2026-11-14 on lf-cloud: a -server boot without -config binds only its game port.)
export function sessionA2sPort(port) {
  if (port === SESSION_PORT) return null;
  if (port >= TEST_BLOCK_PORT) return port + 1;
  return 17777 + (port - SESSION_PORT) * 10;
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
  if (kind === "session") return Array.from({ length: Math.min(MAX_SESSION_LANES, m.lanes.session ?? 1) }, (_, i) => sessionLane(i + 1));
  return Array.from({ length: Math.min(MAX_TEST_LANES, m.lanes.test) }, (_, i) => testLane(i + 1));
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
// not block, so the next job that can start right now goes (a lowspec one-client session
// passes a full two-client one that waits for RAM). Anti-starvation: once such a head has
// waited ctx.starveMs it holds its place: a lane-stuck head at once; a RAM-stuck head drains
// the machine for every lane (tests too, they eat the same RAM) as long as draining can help,
// that is its need fits in what is free plus what the pool's running jobs there hold
// (ctx.heldMb). A head that cannot fit even on an empty machine never blocks.
// ctx: { starveMs, freeMb, heldMb } (all optional; per machine m).
export function blocks(other, mine, m, now, promoteMs, ctx = {}) {
  if (!(other.machines ?? []).includes(m.name)) return false;
  if (!servedBefore(other, mine, now, promoteMs)) return false;
  const a = new Set(lanesFor(m, other.lane ?? "session").map((l) => l.lock));
  const sharesLane = lanesFor(m, mine.lane ?? "test").some((l) => a.has(l.lock));
  if (!sharesLane) return other.stuck === "ram" && starved(other, now, ctx) && drainHelps(other, ctx);
  // A head that lacks only a lane is raced for it when that lane frees, so it keeps its turn
  // unless mine can also use a lane the head cannot.
  if (other.stuck === "lane" && lanesFor(m, mine.lane ?? "test").every((l) => a.has(l.lock))) return true;
  return !canBackfill(other, now, ctx);
}

function starved(t, now, { starveMs = DEFAULTS.starveMin * 60000 } = {}) {
  return starveMs > 0 && now - Date.parse(t.at) > starveMs;
}

// Whether holding everyone else back can ever let t start: its need fits in free RAM plus what
// the pool's own running jobs on the machine hold. Unknown numbers count as "it can".
function drainHelps(t, { freeMb, heldMb } = {}) {
  if (!Number.isFinite(t.needMb) || !Number.isFinite(freeMb)) return true;
  return t.needMb <= freeMb + (Number.isFinite(heldMb) ? heldMb : Infinity);
}

// True when a ticket that is served first is stuck (RAM or lane) and may be passed: it has not
// waited past the starvation limit, or (RAM) draining the machine could not help it anyway.
export function canBackfill(t, now, ctx = {}) {
  if (t.stuck !== "ram" && t.stuck !== "lane") return false;
  if (!starved(t, now, ctx)) return true;
  if (t.stuck === "ram") return !drainHelps(t, ctx);
  return false;
}

// Why a ticket waits, for `pool status`: "ram 2-client full: needs 3.8 GB free, local has 1.1 GB",
// "lane", "behind N", plus "starved: holds the machine" once it reserves. freeMbOf(name) gives a
// machine's live free RAM (MB) when known.
export function waitReason(t, ahead = 0, { now = Date.now(), starveMs, freeMbOf, otherMachines = [] } = {}) {
  const hold = (t.stuck === "ram" || t.stuck === "lane") && starved(t, now, { starveMs }) ? "; starved, so it holds its place" : "";
  if (t.stuck === "ram") {
    const what = `ram${t.clients ? ` ${t.clients}-client` : ""}${t.profile && t.clients ? ` ${t.profile}` : ""}`;
    // A pinned job (--local / --machine) will not move: say which machine it is not allowed to use has room.
    const roomy = t.pinned ? roomElsewhere(t.needMb, t.machines, otherMachines.map((n) => ({ name: n, freeMb: freeMbOf?.(n) }))) : [];
    const pin = roomy.length ? ` (pinned; ${roomy.join(", ")} ${roomy.length > 1 ? "have" : "has"} room)` : "";
    if (!Number.isFinite(t.needMb)) return `${what}${pin}${hold}`;
    const gb = (x) => (x / 1024).toFixed(1);
    const busy = t.busyOn ?? [];
    const have = (t.machines ?? []).filter((n) => !busy.includes(n)).map((n) => [n, freeMbOf?.(n)]).filter(([, f]) => Number.isFinite(f)).map(([n, f]) => `${n} has ${gb(f)}`);
    return `${what}: needs ${gb(t.needMb)} GB free${have.length ? `, ${have.join(", ")}` : ""}${busy.length ? `; every session lane taken on ${busy.join(", ")}` : ""}${pin}${hold}`;
  }
  if (t.stuck === "lane") return `lane${hold}`;
  return ahead > 0 ? `behind ${ahead}` : "";
}

// When the job holding a lane is expected to let go (epoch ms): its start plus the typical hold of
// its kind; one that has outrun that is given half a typical hold more, but never past its cap
// (a session's hold cap plus grace while someone waits). typicalMs and capMs may be absent.
export function projectedRelease({ atMs, now, typicalMs, capMs }) {
  if (!Number.isFinite(atMs)) return now + (typicalMs ?? 0);
  const cap = Number.isFinite(capMs) ? atMs + capMs : Infinity;
  const typ = Number.isFinite(typicalMs) && typicalMs > 0 ? typicalMs : 0;
  let end = atMs + typ;
  if (end < now) end = now + typ / 2;
  return Math.max(now, Math.min(end, cap < now ? now + 60000 : cap));
}

// Estimated start of a waiting job (epoch ms): the (ahead+1)-th lane release among the lanes it can
// use (a free lane releases "now"); with more jobs ahead than lanes, each further round of lanes
// adds cycleMs (a typical hold). null when it can use no lane at all.
export function estimateStart(releases, ahead = 0, cycleMs = 0) {
  const r = [...releases].filter(Number.isFinite).sort((x, y) => x - y);
  if (!r.length) return null;
  return r[ahead % r.length] + Math.floor(ahead / r.length) * (Number.isFinite(cycleMs) ? cycleMs : 0);
}

// Median of the positive numbers in values (0 when none).
export function median(values) {
  const v = values.filter((x) => Number.isFinite(x) && x > 0).sort((a, b) => a - b);
  return v.length ? v[Math.floor((v.length - 1) / 2)] : 0;
}

// RAM gate. A job reserves what it is expected to take (reserveMb: a session's server and
// clients; 0 for a test, which the floor covers) and sessions still booting count with what they
// will take (pendingMb), since free RAM does not show it yet. Every start, the first server on a
// machine included, must leave minFreeMb free after its reserve: a machine below floor plus
// reserve takes no new job (2026-11-14: this PC sat at 0.4 GB free and its first lane kept taking
// runs that then timed out while the laptop idled). `running` only words the reason.
export function ramVerdict({ running = 0, freeMb, minFreeMb, reserveMb = 0, pendingMb = 0 }) {
  const gb = (x) => (x / 1024).toFixed(1);
  if (freeMb - pendingMb - reserveMb >= minFreeMb) return { ok: true };
  const needMb = minFreeMb + reserveMb + pendingMb;
  if (reserveMb > 0 || pendingMb > 0) {
    return { ok: false, needMb, reason: `free RAM ${gb(freeMb)} GB minus ${gb(reserveMb + pendingMb)} GB for the session${pendingMb > 0 ? "(s) starting" : ""} would be under ${gb(minFreeMb)} GB` };
  }
  return { ok: false, needMb, reason: `free RAM ${gb(freeMb)} GB under ${gb(minFreeMb)} GB${running > 0 ? ` with ${running} server(s) running` : ""}` };
}

// ---------------------------------------------------------------- machine choice
// Which machine a job tries first. Candidates are the machines that may take it (a pin leaves one);
// the ones with no free lane of the job's kind or without the needed cap are not passed in or fail
// the lane check when tried, so this only orders them.
//   headroom = free RAM - sessions still booting - the job's reserve        (MB, null when unread)
//   room     = headroom >= minFreeMb   (the same floor the RAM gate applies to every start)
//   score    = min(headroom, roomyMb) / budgetMultiplier
// A machine with room goes before one without. Among those with room the higher score wins: headroom
// counts up to roomyMb (6 GB: past that more RAM changes nothing a test or session can use), divided by
// the machine's budgetMultiplier (1 = this PC, 2 = the laptop, about twice as slow), so two machines that
// are both roomy go to the faster one, and a slower one wins only with more than twice the headroom
// below the cap. Equal scores keep the faster machine, then registry order. A machine whose RAM could
// not be read goes after the ones with room and before those without. Those without room come last,
// most headroom first (the gate refuses them anyway; the order only decides what `pool status` names).
// cands: [{ name, freeMb, pendingMb?, multiplier? }]. Returns rows { name, room, headroomMb, score }
// best first, in the order given for ties.
export function rankMachines(cands, { minFreeMb = DEFAULTS.minFreeMb, reserveMb = 0, roomyMb = DEFAULTS.roomyMb } = {}) {
  const rows = cands.map((c, i) => {
    const known = Number.isFinite(c.freeMb);
    const headroomMb = known ? c.freeMb - (Number(c.pendingMb) || 0) - reserveMb : null;
    const multiplier = Number(c.multiplier) > 0 ? Number(c.multiplier) : 1;
    const room = known ? headroomMb >= minFreeMb : null;
    const score = known ? Math.min(headroomMb, roomyMb) / multiplier : 0;
    return { name: c.name, room, headroomMb, score, multiplier, i };
  });
  const tier = (r) => (r.room === true ? 0 : r.room === null ? 1 : 2);
  rows.sort((a, b) => {
    if (tier(a) !== tier(b)) return tier(a) - tier(b);
    if (tier(a) === 0 && a.score !== b.score) return b.score - a.score;
    if (tier(a) === 2 && a.headroomMb !== b.headroomMb) return b.headroomMb - a.headroomMb;
    return a.multiplier - b.multiplier || a.i - b.i;
  });
  return rows.map(({ i, multiplier, ...r }) => r);
}

// Machines outside `have` (a ticket's machines) that have room for the job's need (MB: floor plus
// reserve), for the pinned-job hint of `pool status`. others: [{ name, freeMb }].
export function roomElsewhere(needMb, have, others) {
  return others.filter((o) => !(have ?? []).includes(o.name) && Number.isFinite(o.freeMb) && Number.isFinite(needMb) && o.freeMb >= needMb).map((o) => o.name);
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
