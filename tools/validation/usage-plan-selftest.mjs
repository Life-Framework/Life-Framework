// Proves `tools\cli usage --plan` (tools/agents/usage-plan.mjs) on a recorded fake of the OAuth usage reply:
// inputs: tools/**, agent-box/**, .githooks/**
//   1. normalize reads the server's limits[] rows (session, weekly_all, weekly_scoped with the model name) and falls
//      back to five_hour / seven_day / seven_day_opus / seven_day_sonnet when a reply has no rows;
//   2. the pace math is hand-worked: share of the week gone, burn rate from same-window samples, projected % at reset;
//      a sample of an older window is ignored; with no 20-minute span the week average stands in;
//   3. advice: over pace holds until the 5h reset, a near-full 5h window holds whatever the week says, a low
//      projection says how many agents to start (per-agent burn = rate / agents seen), the 5h window caps that number
//      and can turn it into "wait", and a week on target is "on pace";
//   4. the call: right URL, Bearer + anthropic-beta headers; 401, 429, a network error and a timeout each give one
//      "plan usage unavailable (why)" line, never a throw;
//   5. the token never appears: not in the success line, not in any error line, not in the saved samples file, and
//      redact() strips a token-shaped string and a Bearer header; an expired token makes no request at all;
//   6. samples are added at most every 3 min and dropped after 48 h.
// red-proof: 2026-11-17, four breaks, each caught: redact() returning its input printed "FAIL usage-plan: network
// error text with the token in it: secret leaked"; the week projection using the 5h rate printed "FAIL usage-plan:
// projected at reset: 30 != 198"; the over-pace branch removed from advise() printed "FAIL usage-plan: over pace: kind:
// on != over"; the 4 h look-back removed printed "FAIL usage-plan: a sample older than 4 h is not used: 5 != 2".
import { mkdirSync, mkdtempSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { addSample, advise, burnRate, clock, fetchPlanUsage, normalize, pace, planFor, planLine, readCredentials, redact, SAMPLE_KEEP_MS } from "../agents/usage-plan.mjs";

const failures = [];
const eq = (got, want, msg) => {
  if (got !== want) failures.push(`${msg}: ${got} != ${want}`);
};
const near = (got, want, msg, tol = 0.01) => {
  if (!(Math.abs(got - want) <= tol)) failures.push(`${msg}: ${got} != ${want}`);
};
const has = (text, part, msg) => {
  if (!String(text).includes(part)) failures.push(`${msg}: "${part}" not in "${text}"`);
};
const lacks = (text, part, msg) => {
  if (String(text).includes(part)) failures.push(`${msg}: secret leaked`);
};

const H = 3600e3;
const NOW = Date.parse("2026-11-17T12:00:00Z");
const SECRET = "sk-ant-oat01-SECRETSECRETSECRET-abcdef0123456789";
const REFRESH = "sk-ant-ort01-REFRESHREFRESH-0123456789abcdef";

// A reply recorded from the endpoint (Gaming, 2026-11-17), the figures changed to hand-workable ones.
const REPLY = {
  five_hour: { utilization: 20.0, resets_at: "2026-11-17T15:00:00.000000+00:00", limit_dollars: null },
  seven_day: { utilization: 10.0, resets_at: "2026-11-21T00:00:00.000000+00:00", limit_dollars: null },
  seven_day_opus: null,
  seven_day_sonnet: null,
  extra_usage: { is_enabled: false },
  limits: [
    { kind: "session", group: "session", percent: 20, severity: "normal", resets_at: "2026-11-17T15:00:00.000000+00:00", scope: null, is_active: true },
    { kind: "weekly_all", group: "weekly", percent: 10, severity: "normal", resets_at: "2026-11-21T00:00:00.000000+00:00", scope: null, is_active: false },
    { kind: "weekly_scoped", group: "weekly", percent: 4, severity: "normal", resets_at: "2026-11-21T00:00:00+00:00", scope: { model: { id: null, display_name: "Fable" }, surface: null }, is_active: false },
  ],
};
const okRes = (body, status = 200, headers = {}) => ({ ok: status >= 200 && status < 300, status, headers: { get: (k) => headers[k.toLowerCase()] ?? null }, json: async () => body });

// ---- 1. normalize
{
  const p = normalize(REPLY);
  eq(p.fiveHour.pct, 20, "session row");
  eq(p.fiveHour.resetsAt, Date.parse("2026-11-17T15:00:00Z"), "session reset");
  eq(p.weekly.pct, 10, "weekly_all row");
  eq(p.scoped.length, 1, "one scoped row");
  eq(p.scoped[0].name, "Fable", "scoped model name");
  eq(p.scoped[0].pct, 4, "scoped percent");
  const old = normalize({ five_hour: { utilization: 7.5, resets_at: "2026-11-17T15:00:00+00:00" }, seven_day: { utilization: 33, resets_at: "2026-11-21T00:00:00+00:00" }, seven_day_opus: { utilization: 12, resets_at: "2026-11-21T00:00:00+00:00" }, seven_day_sonnet: null });
  eq(old.fiveHour.pct, 7.5, "fallback five_hour");
  eq(old.weekly.pct, 33, "fallback seven_day");
  eq(old.scoped.length, 1, "fallback opus row");
  eq(old.scoped[0].name, "Opus", "fallback opus name");
  const none = normalize(null);
  eq(none.fiveHour, null, "null reply");
}

// ---- 2. pace math. Week resets in 84 h of 168 -> 50% of the week gone.
const plan = (five, week, fiveLeftH = 3, weekLeftH = 84) => ({
  fiveHour: { pct: five, resetsAt: NOW + fiveLeftH * H },
  weekly: { pct: week, resetsAt: NOW + weekLeftH * H },
  scoped: [],
});
const sample = (agoH, five, week, plan0, agents = null) => ({ t: NOW - agoH * H, five, fiveReset: plan0.fiveHour.resetsAt, week, weekReset: plan0.weekly.resetsAt, agents });
{
  const pl = plan(20, 30);
  const p = pace(pl, { samples: [sample(3, 14, 24, pl)], now: NOW });
  near(p.weekElapsedPct, 50, "share of week gone");
  near(p.weekRate, 2, "weekly rate (30 - 24) / 3 h");
  near(p.weekProjected, 30 + 2 * 84, "projected at reset");
  near(p.fiveRate, 2, "5h rate (20 - 14) / 3 h");
  near(p.fiveProjected, 20 + 2 * 3, "5h projected");
  near(p.weekRequired, 70 / 84, "rate needed to land on 100%");
  // a sample 6 h old is outside the 4 h look-back: the rate still comes from the 3 h sample
  near(pace(pl, { samples: [sample(6, 0, 0, pl), sample(3, 14, 24, pl)], now: NOW }).weekRate, 2, "a sample older than 4 h is not used");
  // a sample of an older window (its reset 5 days earlier) is not used
  const stale = { ...sample(3, 14, 5, pl), weekReset: pl.weekly.resetsAt - 5 * 24 * H };
  const q = pace(pl, { samples: [stale], now: NOW });
  eq(q.weekBasis, "week average", "an old window's sample is ignored");
  near(q.weekRate, 30 / 84, "week average rate = % / hours into the week");
  // a 10-minute span is too short for a rate
  eq(burnRate([sample(1 / 6, 14, 29.9, pl)], "week", "weekReset", NOW, { pct: 30, resetsAt: pl.weekly.resetsAt }), null, "span under 20 min");
}

// ---- 3. advice
{
  const pl = plan(20, 30);
  const over = advise(pace(pl, { samples: [sample(3, 14, 24, pl)], now: NOW }), pl, { now: NOW });
  eq(over.kind, "over", "over pace: kind");
  has(over.text, "over pace: hold new agents until", "over pace text");
  has(over.text, `until ${clock(NOW + 3 * H, NOW)}`, "over pace names the 5h reset");
}
{
  const pl = plan(92, 10);
  const a = advise(pace(pl, { samples: [], now: NOW }), pl, { now: NOW });
  eq(a.kind, "five-tight", "5h near full holds");
  has(a.text, "hold new agents", "5h hold text");
}
{
  // week 10%, 0.5%/h over 3 h with 3 agents -> projects 52%; need (90/84 - 0.5) = 0.5714 %/h; per agent 0.5/3
  const pl = plan(20, 10);
  const p = pace(pl, { samples: [sample(3, 14, 8.5, pl, 3), sample(1.5, 17, 9.25, pl, 3)], agents: 3, now: NOW });
  near(p.weekRate, 0.5, "rate over the samples");
  near(p.weekProjected, 10 + 0.5 * 84, "projected");
  const a = advise(p, pl, { now: NOW });
  eq(a.kind, "under", "under-using: kind");
  eq(a.n, 4, "under-using: ceil(0.5714 / (0.5 / 3)) agents");
  eq(a.assumed, false, "per-agent burn is measured");
  has(a.text, "under-using: start ~4 more agents", "under-using text");
}
{
  // the same week but the 5h window at 60% with a 10%/h burn and 3 agents: projects 90% -> room 0 -> wait
  const pl = plan(60, 10);
  const p = pace(pl, { samples: [sample(3, 30, 8.5, pl, 3)], agents: 3, now: NOW });
  const a = advise(p, pl, { now: NOW });
  eq(a.kind, "five-tight", "5h window full: under-using turns into wait");
  has(a.text, "wait", "wait text");
}
{
  // no samples, no agents known: an assumed per-agent burn, flagged
  const pl = plan(2, 0);
  const a = advise(pace(pl, { samples: [], agents: 0, now: NOW }), pl, { now: NOW });
  eq(a.kind, "under", "nothing used yet: under-using");
  eq(a.assumed, true, "assumed burn flagged");
  has(a.text, "assumed", "assumed text");
}
{
  // a tiny week-average rate with 19 agents must not become a silly per-agent figure: the assumed burn stands in
  const pl = plan(3, 1);
  const a = advise(pace(pl, { samples: [], agents: 19, now: NOW }), pl, { now: NOW });
  eq(a.kind, "under", "week-average basis: kind");
  eq(a.assumed, true, "week-average basis: assumed burn");
  eq(a.n, 3, "week-average basis: ceil((99/84 - 1/84) / 0.4) agents");
}
{
  const pl = plan(20, 50);
  const p = pace(pl, { samples: [sample(3, 14, 48.2, pl, 2)], agents: 2, now: NOW });
  const a = advise(p, pl, { now: NOW });
  near(p.weekProjected, 50 + 0.6 * 84, "on-target projection");
  eq(a.kind, "on", "on pace");
  eq(a.text, "on pace", "on pace text");
  const line = planLine("Gaming", pl, p, a, { now: NOW });
  has(line, "week 50%", "line: weekly");
  has(line, "PACE 50% used vs 50% of week gone, projects 100% at reset (0.60%/h last 3.0h)", "line: pace");
  has(line, "ADVICE on pace", "line: advice");
}

// ---- 4. the call
{
  const seen = [];
  const fetchImpl = async (url, init) => {
    seen.push({ url, init });
    return okRes(REPLY);
  };
  const r = await fetchPlanUsage({ token: SECRET, refreshToken: REFRESH, fetchImpl });
  eq(r.five_hour.utilization, 20, "reply parsed");
  eq(seen[0].url, "https://api.anthropic.com/api/oauth/usage", "URL");
  eq(seen[0].init.headers.Authorization, `Bearer ${SECRET}`, "bearer header");
  eq(seen[0].init.headers["anthropic-beta"], "oauth-2025-04-20", "beta header");
}

// ---- 4/5. fail soft and secrets, through planFor with a temp home
const home = mkdtempSync(join(tmpdir(), "lf-usage-plan-"));
const cfg = join(home, ".claude-gaming");
mkdirSync(cfg, { recursive: true });
const writeCreds = (expiresAt) => writeFileSync(join(cfg, ".credentials.json"), JSON.stringify({ claudeAiOauth: { accessToken: SECRET, refreshToken: REFRESH, expiresAt, scopes: ["user:profile"] } }));
const samplesFile = join(home, "tmp", "usage-plan-samples.json");
const run = (fetchImpl) => planFor("gaming", { home, now: NOW, agents: 3, samplesFile, fetchImpl });
writeCreds(NOW + 5 * H);
{
  const ok = await run(async () => okRes(REPLY));
  eq(ok.ok, true, "success");
  has(ok.line, "Gaming: 5h 20%", "success line");
  has(ok.line, "Fable week 4%", "scoped limit in the line");
  lacks(ok.line, SECRET, "success line");
  lacks(JSON.stringify(ok), SECRET, "result object");
  lacks(JSON.stringify(ok), REFRESH, "result object (refresh token)");
  lacks(readFileSync(samplesFile, "utf8"), SECRET, "samples file");
  lacks(readFileSync(samplesFile, "utf8"), REFRESH, "samples file (refresh token)");
  eq(JSON.parse(readFileSync(samplesFile, "utf8")).gaming.length, 1, "one sample saved");
  const cases = [
    ["401", async () => okRes({}, 401), "token rejected (HTTP 401)"],
    ["429", async () => okRes({}, 429, { "retry-after": "300" }), "HTTP 429"],
    ["500", async () => okRes({}, 500), "HTTP 500"],
    ["network", async () => { throw Object.assign(new TypeError("fetch failed"), { cause: { code: "ENOTFOUND" } }); }, "network: ENOTFOUND"],
    ["timeout", async () => { throw Object.assign(new Error("aborted"), { name: "TimeoutError" }); }, "no answer in"],
    ["token in the error text", async () => { throw new Error(`connect failed with Authorization: Bearer ${SECRET} and ${REFRESH}`); }, "<redacted>"],
    ["empty reply", async () => okRes({}), "no 5h or weekly figure"],
  ];
  for (const [name, impl, want] of cases) {
    const r = await run(impl);
    eq(r.ok, false, `${name}: not ok`);
    has(r.line, "Gaming: plan usage unavailable (", `${name}: soft line`);
    has(r.line, want, `${name}: reason`);
    lacks(r.line, SECRET, `${name}: line`);
    lacks(r.line, REFRESH, `${name}: line (refresh token)`);
  }
  // the thrown message of fetchPlanUsage itself
  try {
    await fetchPlanUsage({ token: SECRET, refreshToken: REFRESH, fetchImpl: async () => { throw new Error(`boom ${SECRET}`); } });
    failures.push("fetchPlanUsage did not throw");
  } catch (e) {
    lacks(e.message, SECRET, "network error text with the token in it");
  }
  // an expired token: no request, one line, still no secret
  writeCreds(NOW - H);
  let called = false;
  const ex = await run(async () => {
    called = true;
    return okRes(REPLY);
  });
  eq(called, false, "expired token makes no request");
  has(ex.line, "OAuth token expired", "expired line");
  lacks(JSON.stringify(ex), SECRET, "expired result");
  const none = await planFor("personal", { home, now: NOW, agents: 0, fetchImpl: async () => okRes(REPLY) });
  has(none.line, "Personal: plan usage unavailable (no .credentials.json", "missing credentials file");
  eq(readCredentials(cfg, NOW).expired, true, "readCredentials flags an expired token");
}
// redact() on its own
{
  lacks(redact(`x ${SECRET} y`, [SECRET]), SECRET, "redact by value");
  lacks(redact("Authorization: Bearer abcdefghijklmnopqrstuvwxyz0123"), "abcdefghijklmnop", "redact a Bearer header");
  lacks(redact("token sk-ant-oat01-ZZZZZZZZZZZZZZ here"), "ZZZZZZZZZZZZZZ", "redact a token-shaped string");
  eq(redact("plain text"), "plain text", "plain text untouched");
}

// ---- 6. samples
{
  let list = addSample([], { t: NOW, week: 1 }, NOW);
  list = addSample(list, { t: NOW + 60e3, week: 2 }, NOW + 60e3);
  eq(list.length, 1, "a reading under 3 min after the last is skipped");
  list = addSample(list, { t: NOW + 4 * 60e3, week: 3 }, NOW + 4 * 60e3);
  eq(list.length, 2, "a reading 4 min later is kept");
  list = addSample(list, { t: NOW + SAMPLE_KEEP_MS + 10 * 60e3, week: 4 }, NOW + SAMPLE_KEEP_MS + 10 * 60e3);
  eq(list.length, 1, "readings older than 48 h are dropped");
}

rmSync(home, { recursive: true, force: true });
if (failures.length) {
  for (const f of failures) console.error(`FAIL usage-plan: ${f}`);
  process.exit(1);
}
console.log("OK usage-plan selftest");
