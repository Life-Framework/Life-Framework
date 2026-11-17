// `tools\cli usage --plan [--account gaming|personal|all] [--json]`: the REAL plan utilization (the numbers
// Claude Code's /usage screen shows), a pace against the week, and advice on how many more agents to start.
//
// Where the numbers come from (read from Claude Code 2.1.319's bundle, not guessed): GET
// https://api.anthropic.com/api/oauth/usage with `Authorization: Bearer <OAuth access token>`, `anthropic-beta:
// oauth-2025-04-20`. The token is in <config dir>/.credentials.json (claudeAiOauth.accessToken; the scope
// user:profile is what the endpoint wants). The reply carries `five_hour` / `seven_day` / `seven_day_opus` /
// `seven_day_sonnet` as { utilization: 0-100, resets_at: ISO } and a `limits[]` list of the server's own rows
// (kind session | weekly_all | weekly_scoped, percent, resets_at, scope.model.display_name); the rows win.
//
// Secrets: the token is read into memory, sent in one header and never printed, logged, stored or returned.
// Every error text goes through redact(). An expired token is NOT refreshed here (a refresh rotates the
// refresh token and would log the real Claude Code out); it is said plainly and the next Claude Code run
// on that account refreshes it.
import { existsSync, mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { homedir } from "node:os";
import { dirname, join } from "node:path";
import { LAPTOP, scanRemote } from "./usage-accounts.mjs";
import { scanConfigDir } from "./usage-scan.mjs";

export const USAGE_URL = "https://api.anthropic.com/api/oauth/usage";
export const OAUTH_BETA = "oauth-2025-04-20";
const H = 3600e3;
const WEEK_H = 168;
const FIVE_H = 5;
export const SAMPLE_KEEP_MS = 48 * H;
export const RATE_LOOKBACK_MS = 4 * H;
export const MIN_RATE_SPAN_MS = 20 * 60e3;
export const SAMPLE_GAP_MS = 3 * 60e3;
// A busy agent's weekly burn, used only until the samples give a measured per-agent figure.
export const ASSUMED_AGENT_WEEKLY_PCT_PER_H = 0.4;
export const HOLD_FIVE_PCT = 90; // the 5-hour window at or past this: hold, however the week looks
export const UNDER_PCT = 90; // projected weekly use at reset below this is under-using
export const OVER_PCT = 110; // above this is over pace
export const MAX_START = 10;

// ---- secrets ------------------------------------------------------------------------------------

export function redact(text, secrets = []) {
  let s = String(text ?? "");
  for (const x of secrets) if (x && String(x).length >= 8) s = s.split(String(x)).join("<redacted>");
  // a token-shaped string that slipped in some other way
  return s.replace(/\b(sk-ant-[A-Za-z0-9_-]{6,})[A-Za-z0-9_-]*/g, "<redacted>").replace(/(Bearer\s+)[A-Za-z0-9._~+/=-]{12,}/gi, "$1<redacted>");
}

// { token, refreshToken, expiresAt } or { error }. The dir is a Claude config dir (~/.claude-gaming).
export function readCredentials(dir, now = Date.now()) {
  const file = join(dir, ".credentials.json");
  if (!existsSync(file)) return { error: "no .credentials.json (not signed in with OAuth here)" };
  let o;
  try {
    o = JSON.parse(readFileSync(file, "utf8")).claudeAiOauth;
  } catch (e) {
    return { error: "credentials file unreadable" };
  }
  if (!o || !o.accessToken) return { error: "no OAuth access token in the credentials" };
  if (o.expiresAt && o.expiresAt <= now + 30e3) return { error: "OAuth token expired (it refreshes the next time Claude Code runs on this account)", token: o.accessToken, refreshToken: o.refreshToken, expired: true };
  return { token: o.accessToken, refreshToken: o.refreshToken, expiresAt: o.expiresAt || null };
}

// ---- the call -----------------------------------------------------------------------------------

// -> the parsed reply. Throws Error(message) with no secret in it.
export async function fetchPlanUsage({ token, refreshToken, fetchImpl = globalThis.fetch, timeoutMs = 8000, version = "2.1.319", url = USAGE_URL }) {
  const secrets = [token, refreshToken];
  let res;
  try {
    res = await fetchImpl(url, {
      headers: { Authorization: `Bearer ${token}`, "anthropic-beta": OAUTH_BETA, "Content-Type": "application/json", "User-Agent": `claude-code/${version}` },
      signal: AbortSignal.timeout(timeoutMs),
    });
  } catch (e) {
    const why = e && (e.name === "TimeoutError" || e.name === "AbortError") ? `no answer in ${Math.round(timeoutMs / 1000)} s` : `network: ${(e && (e.cause && e.cause.code || e.code || e.message)) || "failed"}`;
    throw new Error(redact(why, secrets));
  }
  if (res.status === 401 || res.status === 403) throw new Error(`token rejected (HTTP ${res.status}); sign in again or run Claude Code once on this account`);
  if (res.status === 429) throw new Error(`usage endpoint rate limited (HTTP 429, retry-after ${res.headers && res.headers.get ? res.headers.get("retry-after") || "?" : "?"}s)`);
  if (!res.ok) throw new Error(`HTTP ${res.status}`);
  try {
    return await res.json();
  } catch {
    throw new Error("reply was not JSON");
  }
}

// ---- reading the reply --------------------------------------------------------------------------

const toMs = (iso) => {
  const t = Date.parse(iso || "");
  return Number.isFinite(t) ? t : null;
};
const num = (v) => (typeof v === "number" && Number.isFinite(v) ? v : null);

// -> { fiveHour:{pct,resetsAt}|null, weekly:{pct,resetsAt}|null, scoped:[{name,pct,resetsAt}] }; resetsAt in ms.
export function normalize(raw) {
  const out = { fiveHour: null, weekly: null, scoped: [] };
  if (!raw || typeof raw !== "object") return out;
  const rows = Array.isArray(raw.limits) ? raw.limits : [];
  for (const r of rows) {
    if (!r || typeof r !== "object" || num(r.percent) === null) continue;
    const v = { pct: r.percent, resetsAt: toMs(r.resets_at) };
    if (r.kind === "session" && !out.fiveHour) out.fiveHour = v;
    else if (r.kind === "weekly_all" && !out.weekly) out.weekly = v;
    else if (r.kind === "weekly_scoped" && r.scope && r.scope.model && r.scope.model.display_name) out.scoped.push({ name: r.scope.model.display_name, ...v });
  }
  const win = (w) => (w && typeof w === "object" && num(w.utilization) !== null ? { pct: w.utilization, resetsAt: toMs(w.resets_at) } : null);
  if (!out.fiveHour) out.fiveHour = win(raw.five_hour);
  if (!out.weekly) out.weekly = win(raw.seven_day);
  if (!out.scoped.length) {
    for (const [k, name] of [["seven_day_opus", "Opus"], ["seven_day_sonnet", "Sonnet"]]) {
      const w = win(raw[k]);
      if (w) out.scoped.push({ name, ...w });
    }
  }
  return out;
}

// ---- samples and burn rate ----------------------------------------------------------------------

export function loadSamples(file) {
  try {
    const j = JSON.parse(readFileSync(file, "utf8"));
    return j && typeof j === "object" && !Array.isArray(j) ? j : {};
  } catch {
    return {};
  }
}

export function saveSamples(file, all) {
  try {
    mkdirSync(dirname(file), { recursive: true });
    writeFileSync(file, JSON.stringify(all) + "\n");
  } catch {
    /* a scratch file; the reading still prints */
  }
}

// Adds a reading unless the last one is under SAMPLE_GAP_MS old; drops what is older than SAMPLE_KEEP_MS.
export function addSample(list, s, now = Date.now()) {
  const keep = (Array.isArray(list) ? list : []).filter((x) => x && now - x.t < SAMPLE_KEEP_MS).slice(-400);
  const last = keep[keep.length - 1];
  if (!last || s.t - last.t >= SAMPLE_GAP_MS) keep.push(s);
  return keep;
}

const sameWindow = (a, b) => a != null && b != null && Math.abs(a - b) < 120e3;

// %-points per hour from the samples of the SAME window in the last RATE_LOOKBACK_MS, newest reading included.
// -> { rate, hours, agents } or null when there is no 20-minute span.
export function burnRate(samples, key, resetKey, now, cur) {
  const pts = samples.filter((x) => x && num(x[key]) !== null && sameWindow(x[resetKey], cur.resetsAt) && now - x.t <= RATE_LOOKBACK_MS && x.t <= now);
  pts.push({ t: now, [key]: cur.pct, agents: cur.agents ?? null });
  pts.sort((a, b) => a.t - b.t);
  const first = pts[0];
  const last = pts[pts.length - 1];
  if (last.t - first.t < MIN_RATE_SPAN_MS) return null;
  const hours = (last.t - first.t) / H;
  const withAgents = pts.map((x) => x.agents).filter((a) => a != null);
  const agents = withAgents.length ? withAgents.reduce((s, a) => s + a, 0) / withAgents.length : null;
  return { rate: Math.max(0, (last[key] - first[key]) / hours), hours, agents };
}

// ---- pace and advice ----------------------------------------------------------------------------

// plan: normalize() output; samples: stored readings; agents: active Claude sessions on the account (or null).
export function pace(plan, { samples = [], agents = null, now = Date.now() } = {}) {
  const w = plan.weekly;
  const f = plan.fiveHour;
  const p = { weekPct: w ? w.pct : null, fivePct: f ? f.pct : null, agents };
  if (w && w.resetsAt) {
    p.weekHoursLeft = Math.max(0, (w.resetsAt - now) / H);
    p.weekElapsedPct = Math.min(100, Math.max(0, (100 * (WEEK_H - p.weekHoursLeft)) / WEEK_H));
    const r = burnRate(samples, "week", "weekReset", now, { pct: w.pct, resetsAt: w.resetsAt, agents });
    if (r) {
      p.weekRate = r.rate;
      p.weekBasis = `last ${r.hours.toFixed(1)}h`;
      p.rateAgents = r.agents;
    } else if (w.pct > 0 && p.weekElapsedPct > 1) {
      p.weekRate = w.pct / (WEEK_H - p.weekHoursLeft);
      p.weekBasis = "week average";
    }
    if (p.weekRate !== undefined) p.weekProjected = w.pct + p.weekRate * p.weekHoursLeft;
    p.weekRequired = p.weekHoursLeft > 0.05 ? Math.max(0, 100 - w.pct) / p.weekHoursLeft : null;
  }
  if (f && f.resetsAt) {
    p.fiveHoursLeft = Math.max(0, (f.resetsAt - now) / H);
    const r = burnRate(samples, "five", "fiveReset", now, { pct: f.pct, resetsAt: f.resetsAt, agents });
    if (r) p.fiveRate = r.rate;
    else if (f.pct > 0 && FIVE_H - p.fiveHoursLeft > 0.1) p.fiveRate = f.pct / (FIVE_H - p.fiveHoursLeft);
    if (p.fiveRate !== undefined) p.fiveProjected = f.pct + p.fiveRate * p.fiveHoursLeft;
  }
  return p;
}

// -> { kind: "under"|"on"|"over"|"five-tight"|"unknown", n?, until?, text, perAgent?, assumed? }
export function advise(p, plan, { now = Date.now() } = {}) {
  const fiveReset = plan.fiveHour && plan.fiveHour.resetsAt;
  const until = fiveReset && fiveReset > now ? fiveReset : null;
  const hold = (kind, why) => ({ kind, until, text: `${why}: hold new agents${until ? ` until ${clock(until, now)}` : ""}` });
  if (p.weekPct === null || p.weekPct === undefined) return { kind: "unknown", text: "no weekly reading" };
  if (p.fivePct !== null && (p.fivePct >= HOLD_FIVE_PCT || (p.fiveProjected !== undefined && p.fiveProjected >= 100))) {
    return hold("five-tight", `5h window ${Math.round(p.fivePct)}% used${p.fiveProjected !== undefined ? `, projects ${Math.round(p.fiveProjected)}%` : ""}`);
  }
  if (p.weekProjected !== undefined && p.weekProjected > OVER_PCT) return hold("over", "over pace");
  const under = p.weekProjected !== undefined ? p.weekProjected < UNDER_PCT : p.weekElapsedPct !== undefined && p.weekPct < p.weekElapsedPct - 10;
  if (!under) return { kind: "on", text: "on pace" };
  // How many agents close the gap: the extra %/h needed over the current burn, at the per-agent burn.
  const cur = p.weekRate ?? 0;
  const need = Math.max(0, (p.weekRequired ?? 0) - cur);
  // Per-agent burn is trusted only from sampled rates; the since-week-start average is too coarse right after a plan change.
  const sampled = p.weekBasis !== undefined && p.weekBasis.startsWith("last") && p.weekRate > 0;
  let perAgent = sampled && p.rateAgents > 0 ? p.weekRate / p.rateAgents : sampled && p.agents > 0 ? p.weekRate / p.agents : null;
  const assumed = perAgent === null;
  if (assumed) perAgent = ASSUMED_AGENT_WEEKLY_PCT_PER_H;
  let n = Math.max(1, Math.ceil(need / perAgent));
  n = Math.min(n, MAX_START);
  // The 5-hour window caps it: each new agent adds its share of the 5h burn for the rest of that window.
  let cap = null;
  if (p.fiveProjected !== undefined && p.fiveRate > 0 && p.agents > 0) {
    const per5 = (p.fiveRate / p.agents) * Math.max(p.fiveHoursLeft, 1);
    cap = Math.floor(Math.max(0, HOLD_FIVE_PCT - p.fiveProjected) / per5);
    if (cap < 1) return { kind: "five-tight", until, text: `under-using the week but the 5h window is tight (${Math.round(p.fivePct)}%, projects ${Math.round(p.fiveProjected)}%): wait${until ? ` until ${clock(until, now)}` : ""}` };
    n = Math.min(n, cap);
  }
  const base = p.agents != null ? `, ${p.agents} active` : "";
  const how = assumed ? `, assumed ${ASSUMED_AGENT_WEEKLY_PCT_PER_H}%/h each` : "";
  return { kind: "under", n, perAgent, assumed, text: `under-using: start ~${n} more agent${n === 1 ? "" : "s"} (${base.replace(/^, /, "") || "agents unknown"}${how}${cap !== null && n === cap ? ", capped by the 5h window" : ""})` };
}

// ---- output -------------------------------------------------------------------------------------

const pad2 = (n) => String(n).padStart(2, "0");
const DAYS = ["Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"];
export function clock(t, now = Date.now()) {
  const d = new Date(t);
  const hm = `${pad2(d.getHours())}:${pad2(d.getMinutes())}`;
  return t - now < 20 * H ? hm : `${DAYS[d.getDay()]} ${hm}`;
}
export function span(ms) {
  const m = Math.max(0, Math.round(ms / 60e3));
  if (m < 90) return `${m}m`;
  if (m < 48 * 60) return `${Math.floor(m / 60)}h${pad2(m % 60)}m`;
  return `${Math.floor(m / 1440)}d${Math.round((m % 1440) / 60)}h`;
}
const pct = (x) => (x >= 10 ? String(Math.round(x)) : (Math.round(x * 10) / 10).toString());

export function planLine(label, plan, p, adv, { now = Date.now() } = {}) {
  const f = plan.fiveHour;
  const w = plan.weekly;
  const parts = [];
  parts.push(f ? `5h ${pct(f.pct)}%${f.resetsAt ? ` (resets ${clock(f.resetsAt, now)}, in ${span(f.resetsAt - now)})` : ""}` : "5h ?");
  parts.push(w ? `week ${pct(w.pct)}%${w.resetsAt ? ` (resets ${clock(w.resetsAt, now)}, in ${span(w.resetsAt - now)})` : ""}` : "week ?");
  for (const s of plan.scoped) parts.push(`${s.name} week ${pct(s.pct)}%`);
  if (p.weekElapsedPct !== undefined) {
    const proj = p.weekProjected !== undefined ? `, projects ${pct(Math.min(p.weekProjected, 999))}% at reset (${p.weekRate.toFixed(2)}%/h ${p.weekBasis})` : ", no burn rate yet";
    parts.push(`PACE ${pct(p.weekPct)}% used vs ${pct(p.weekElapsedPct)}% of week gone${proj}`);
  }
  parts.push(`ADVICE ${adv.text}`);
  return `${label}: ${parts.join(" | ")}`;
}

export const unavailableLine = (label, reason) => `${label}: plan usage unavailable (${reason})`;

// ---- one account --------------------------------------------------------------------------------

const ACCOUNTS = {
  gaming: { label: "Gaming", dir: ".claude-gaming", id: "claudeAgent_claude_gaming" },
  personal: { label: "Personal", dir: ".claude", id: "claudeAgent" },
};

// Active Claude sessions on the account: PC logs written in the last 10 min, plus the laptop for Gaming.
async function activeAgents(key, { home, now, laptop, timeoutMs }) {
  const acct = ACCOUNTS[key];
  let n = 0;
  let note = "";
  try {
    const sc = await scanConfigDir({ dir: join(home, acct.dir), home, lookbackMs: 15 * 60e3, now });
    n += sc.active || 0;
  } catch {
    return { n: null, note: "agent count unavailable" };
  }
  if (key === "gaming" && laptop) {
    const r = await scanRemote({ host: laptop, lookbackMs: 15 * 60e3, timeoutMs });
    if (r.ok && !r.scan.missing) n += r.scan.active || 0;
    else note = "laptop agents not counted";
  }
  return { n, note };
}

// -> { account, label, ok, line, plan?, pace?, advice?, error? }. Never throws.
export async function planFor(key, opts = {}) {
  const acct = ACCOUNTS[key];
  const now = opts.now ?? Date.now();
  const home = opts.home ?? homedir();
  const secrets = [];
  try {
    const cred = readCredentials(join(home, acct.dir), now);
    if (cred.token) secrets.push(cred.token, cred.refreshToken);
    if (cred.error) return { account: key, label: acct.label, ok: false, error: cred.error, line: unavailableLine(acct.label, cred.error) };
    const raw = await (opts.fetchPlan || fetchPlanUsage)({ token: cred.token, refreshToken: cred.refreshToken, timeoutMs: opts.timeoutMs ?? 8000, fetchImpl: opts.fetchImpl });
    const plan = normalize(raw);
    if (!plan.fiveHour && !plan.weekly) throw new Error("reply had no 5h or weekly figure");
    const ag = opts.agents !== undefined ? { n: opts.agents, note: "" } : await activeAgents(key, { home, now, laptop: opts.laptop === undefined ? LAPTOP.host : opts.laptop, timeoutMs: opts.laptopTimeoutMs ?? 7000 });
    const all = opts.samplesFile ? loadSamples(opts.samplesFile) : {};
    const prior = Array.isArray(all[key]) ? all[key] : [];
    const p = pace(plan, { samples: prior, agents: ag.n, now });
    const adv = advise(p, plan, { now });
    if (opts.samplesFile) {
      all[key] = addSample(prior, { t: now, five: plan.fiveHour ? plan.fiveHour.pct : null, fiveReset: plan.fiveHour ? plan.fiveHour.resetsAt : null, week: plan.weekly ? plan.weekly.pct : null, weekReset: plan.weekly ? plan.weekly.resetsAt : null, agents: ag.n }, now);
      saveSamples(opts.samplesFile, all);
    }
    let line = planLine(acct.label, plan, p, adv, { now });
    if (ag.note) line += ` (${ag.note})`;
    return { account: key, label: acct.label, ok: true, line, plan, pace: p, advice: adv, agents: ag.n, note: ag.note };
  } catch (e) {
    const reason = redact(e && e.message ? e.message : String(e), secrets).split("\n")[0].slice(0, 140);
    return { account: key, label: acct.label, ok: false, error: reason, line: unavailableLine(acct.label, reason) };
  }
}

// The lines `tools\cli lead` prints: one for Gaming.
export async function leadUsageLines(root, opts = {}) {
  const r = await planFor("gaming", { samplesFile: join(root, "tmp", "usage-plan-samples.json"), timeoutMs: 6000, laptopTimeoutMs: 6000, ...opts });
  return [`USAGE ${r.line}`];
}

// ---- the command --------------------------------------------------------------------------------

export const HELP = `usage: tools\\cli usage --plan [--account gaming|personal|all] [--no-laptop] [--json]
  The plan's REAL utilization (what /usage shows), from Anthropic's OAuth usage endpoint with the account's own
  token (~/.claude-gaming for Gaming, ~/.claude for Personal; the token is never printed or stored). One line per
  account: 5h % and reset, weekly % and reset (plus per-model weekly limits), PACE (weekly % vs the share of the
  week gone, and the % projected at reset from the last few hours of samples in tmp/usage-plan-samples.json), ADVICE
  (under-using: start ~N more agents | on pace | over pace: hold new agents until the 5h reset | 5h tight).
  Goal: 100% of the weekly limit at its reset without hitting the 5h limit hard. Fails soft: "plan usage unavailable (why)".`;

export async function main(args, { root = process.cwd() } = {}) {
  const opt = { accounts: ["gaming"], json: false, laptop: LAPTOP.host, home: homedir(), samples: join(root, "tmp", "usage-plan-samples.json") };
  for (let i = 0; i < args.length; i++) {
    const a = args[i];
    if (a === "--plan") continue;
    else if (a === "--account") {
      const v = String(args[++i] || "").toLowerCase();
      if (v === "all") opt.accounts = Object.keys(ACCOUNTS);
      else if (ACCOUNTS[v]) opt.accounts = [v];
      else {
        console.error(`--account: expected gaming, personal or all, got ${v}`);
        return 2;
      }
    } else if (a === "--json") opt.json = true;
    else if (a === "--no-laptop") opt.laptop = "";
    else if (a === "--home") opt.home = args[++i]; // selftests
    else if (a === "-h" || a === "--help" || a === "help") {
      console.log(HELP);
      return 0;
    } else {
      console.error(`unknown argument ${a}\n${HELP}`);
      return 2;
    }
  }
  const rows = [];
  for (const k of opt.accounts) rows.push(await planFor(k, { home: opt.home, samplesFile: opt.samples, laptop: opt.laptop }));
  if (opt.json) {
    console.log(JSON.stringify(rows.map(({ line, ...r }) => ({ ...r, line })), null, 1));
    return 0; // fail soft: an unavailable reading is a printed line, not an error
  }
  for (const r of rows) console.log(r.line);
  return 0; // fail soft: an unavailable reading is a printed line, not an error
}

