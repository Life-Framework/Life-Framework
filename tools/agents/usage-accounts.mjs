// `tools\cli usage --accounts` / `--pick`: usage per Claude ACCOUNT (not per machine) over a rolling
// window, a headroom estimate, and which provider instance the next agent should use.
//
//   ~/.claude          Personal account   T3 provider claudeAgent
//   ~/.claude-gaming   Gaming account     T3 provider claudeAgent_claude_gaming
//   the laptop         signed in to Gaming; read in place over ssh (agents there log to its ~/.claude)
//
// An account is identified by the oauthAccount in each config dir's .claude.json (shown masked), so
// two machines on the same login add up into one row. The real limit is neither published nor
// logged; the only evidence is the usage we saw in the window before each past rate-limit hit, so
// every percentage here is an estimate.
import { readFileSync } from "node:fs";
import { homedir } from "node:os";
import { dirname, join } from "node:path";
import { spawn } from "node:child_process";
import { fileURLToPath } from "node:url";
import { scanConfigDir } from "./usage-scan.mjs";

export const PROVIDERS = [
  { dir: ".claude", id: "claudeAgent", label: "Personal" },
  { dir: ".claude-gaming", id: "claudeAgent_claude_gaming", label: "Gaming" },
];
export const LAPTOP = { host: "lf-testbox", id: "laptop" };
const HOUR = 3600e3;
const DAY = 24 * HOUR;
const HISTORY = 7 * DAY;

export function parseWindow(s) {
  const m = /^(\d+(?:\.\d+)?)([mhd])$/.exec(String(s || ""));
  if (!m) throw new Error(`--window: expected 90m, 5h or 7d, got ${s}`);
  return Number(m[1]) * { m: 60e3, h: HOUR, d: DAY }[m[2]];
}

// ---- reading the laptop -------------------------------------------------------------------------

// Runs the scan module on the laptop through `ssh <host> node --input-type=module -`; the source goes in
// on stdin, one JSON line comes back. Bounded by timeoutMs; any failure is returned, never thrown.
export function scanRemote({ host = LAPTOP.host, lookbackMs, timeoutMs = 90000, ssh = "ssh" }) {
  return new Promise((done) => {
    let src;
    try {
      src = readFileSync(join(dirname(fileURLToPath(import.meta.url)), "usage-scan.mjs"), "utf8");
    } catch (e) {
      return done({ ok: false, error: `cannot read usage-scan.mjs: ${e.message}` });
    }
    src += `\nconsole.log("LFSCAN " + JSON.stringify(await scanConfigDir({ dir: join(homedir(), ".claude"), lookbackMs: ${Math.round(lookbackMs)} })));\n`;
    let out = "";
    let err = "";
    let finished = false;
    const end = (r) => {
      if (finished) return;
      finished = true;
      clearTimeout(timer);
      done(r);
    };
    let p;
    try {
      p = spawn(ssh, ["-o", "BatchMode=yes", "-o", "ConnectTimeout=8", host, "node --input-type=module -"], { windowsHide: true, stdio: ["pipe", "pipe", "pipe"] });
    } catch (e) {
      return done({ ok: false, error: e.message });
    }
    const timer = setTimeout(() => {
      try {
        p.kill();
      } catch {}
      end({ ok: false, error: `timed out after ${Math.round(timeoutMs / 1000)}s` });
    }, timeoutMs);
    p.stdout.on("data", (d) => (out += d));
    p.stderr.on("data", (d) => (err += d));
    p.on("error", (e) => end({ ok: false, error: e.code === "ENOENT" ? `${ssh} not found` : e.message }));
    p.on("close", (code) => {
      const line = out.split(/\r?\n/).find((l) => l.startsWith("LFSCAN "));
      if (!line) return end({ ok: false, error: `ssh exit ${code}: ${(err.trim().split(/\r?\n/).pop() || "no output").slice(0, 120)}` });
      try {
        end({ ok: true, scan: JSON.parse(line.slice(7)) });
      } catch (e) {
        end({ ok: false, error: `bad reply: ${e.message}` });
      }
    });
    p.stdin.on("error", () => {});
    p.stdin.end(src);
  });
}

// ---- merging machines into accounts -------------------------------------------------------------

// sources: [{ machine, id, label, scan }]. Same oauth account uuid -> one account.
export function mergeAccounts(sources) {
  const byKey = new Map();
  for (const s of sources) {
    const uuid = s.scan.identity && s.scan.identity.uuid;
    const key = uuid ? `u:${uuid}` : `d:${s.id}`;
    let a = byKey.get(key);
    if (!a) {
      a = { key, label: s.label, email: (s.scan.identity && s.scan.identity.email) || "", tier: (s.scan.identity && s.scan.identity.tier) || "", ids: [], sources: [], minutes: {}, hits: [] };
      byKey.set(key, a);
    }
    if (!a.email && s.scan.identity) a.email = s.scan.identity.email;
    if (!a.tier && s.scan.identity) a.tier = s.scan.identity.tier;
    a.ids.push(s.id);
    a.sources.push({ machine: s.machine, id: s.id, active: s.scan.active, files: s.scan.files });
    for (const [k, v] of Object.entries(s.scan.minutes)) {
      const b = (a.minutes[k] ||= [0, 0]);
      b[0] += v[0];
      b[1] += v[1];
    }
    a.hits.push(...s.scan.hits);
  }
  return [...byKey.values()];
}

const sumMinutes = (minutes, from, to) => {
  let cost = 0;
  let msgs = 0;
  for (const [k, v] of Object.entries(minutes)) {
    const t = Number(k);
    if (t > from && t <= to) {
      cost += v[0];
      msgs += v[1];
    }
  }
  return { cost, msgs };
};

// One hit is many error records (every blocked agent writes one); they share type and reset time.
export function clusterHits(hits) {
  const g = new Map();
  for (const h of hits) {
    const key = `${h.type}|${h.resetsAt || Math.floor(h.ts / (5 * HOUR))}`;
    const e = g.get(key);
    if (!e) g.set(key, { type: h.type, resetsAt: h.resetsAt, first: h.ts, last: h.ts, count: 1 });
    else {
      e.first = Math.min(e.first, h.ts);
      e.last = Math.max(e.last, h.ts);
      e.count++;
    }
  }
  return [...g.values()].sort((a, b) => a.first - b.first);
}

const tierWeight = (t) => {
  const m = /(\d+)x/.exec(t || "");
  return m ? Number(m[1]) : /pro/i.test(t || "") ? 1 : null;
};

// Per account: window usage, active sessions, last hit, the usage seen before the last relevant hit
// (the "observed limit"), and the share of it used now. An account that never hit a limit borrows
// the other accounts' observed limit scaled by plan tier (max_20x = 4 x max_5x) and says so.
export function analyze(accounts, { now = Date.now(), windowMs = 5 * HOUR } = {}) {
  const relType = windowMs >= 6 * DAY ? "seven_day" : "five_hour";
  const rows = accounts.map((a) => {
    const groups = clusterHits(a.hits);
    // The limit is a block that resets at resetsAt, so usage before the last reset does not count against
    // the current block, and the usage behind a hit is what was spent since its block began.
    const span = relType === "seven_day" ? 7 * DAY : 5 * HOUR;
    const lastReset = Math.max(0, ...groups.filter((g) => g.type === relType && g.resetsAt && g.resetsAt <= now).map((g) => g.resetsAt));
    const w = sumMinutes(a.minutes, Math.max(now - windowMs, lastReset), now);
    const rel = groups
      .filter((g) => g.type === relType)
      .map((g) => ({ ...g, usage: sumMinutes(a.minutes, Math.max(g.first - windowMs, g.resetsAt ? g.resetsAt - span : 0), g.first).cost }));
    const baseline = [...rel].reverse().find((g) => g.usage > 0) || null;
    const lastHit = groups.length ? groups.reduce((x, y) => (y.last > x.last ? y : x)) : null;
    const open = groups.filter((g) => g.resetsAt > now).sort((x, y) => y.resetsAt - x.resetsAt)[0] || null;
    // Usage right after a hit is low because the account is blocked, not because it has room, so the last
    // 5-hour hit keeps the account out of the pick until its logged reset, or hit + 5 h when no reset was
    // logged. A turn logged in a later minute than the hit proves the window reopened: not limited from then on.
    const lastFive = groups.filter((g) => g.type === "five_hour").reduce((x, y) => (!x || y.last > x.last ? y : x), null);
    let limitedUntil = 0;
    if (lastFive) {
      const until = lastFive.resetsAt || lastFive.last + 5 * HOUR;
      const hitMinute = Math.floor(lastFive.last / 60000) * 60000;
      const reopened = Object.keys(a.minutes).some((k) => Number(k) > hitMinute);
      if (until > now && !reopened) limitedUntil = until;
    }
    return {
      label: a.label,
      email: a.email,
      tier: a.tier,
      ids: a.ids,
      sources: a.sources,
      usage: w.cost,
      messages: w.msgs,
      active: a.sources.reduce((s, x) => s + (x.active || 0), 0),
      lastHit,
      blocked: open ? { type: open.type, until: open.resetsAt } : null,
      limitedUntil,
      hits: groups.length,
      baseline: baseline ? baseline.usage : 0,
      baselineAt: baseline ? baseline.first : 0,
      baselineFrom: "",
    };
  });
  // borrow a baseline by plan tier
  for (const r of rows) {
    if (r.baseline > 0) continue;
    const mine = tierWeight(r.tier);
    const donor = rows.find((o) => o !== r && o.baseline > 0 && tierWeight(o.tier) && mine);
    if (donor) {
      r.baseline = (donor.baseline * mine) / tierWeight(donor.tier);
      r.baselineFrom = `scaled from ${donor.label} by plan`;
    }
  }
  for (const r of rows) {
    r.pct = r.baseline > 0 ? (100 * r.usage) / r.baseline : null;
    r.headroom = r.blocked ? 0 : r.pct === null ? 100 : Math.max(0, 100 - r.pct);
  }
  return rows;
}

// When an account can take work again: 0 when it can now, else the later of an open block and a 5-hour hit + 5 h.
export const availableAt = (r) => Math.max((r.blocked && r.blocked.until) || 0, r.limitedUntil || 0);

// What an account has left to give the next agent: headroom x plan weight (max_20x counts 20, max_5x 5, so
// Personal has 4 x Gaming's capacity), shared among the agents already on it plus the new one.
export function pickScore(r) {
  const weight = tierWeight(r.tier) || 1;
  const headroom = Number.isFinite(r.headroom) ? r.headroom : 100;
  const active = r.active || 0;
  return { weight, headroom, active, score: (headroom / 100) * weight / (active + 1) };
}

// The account the next agent should use: not blocked and not inside a 5-hour limit, then the highest pickScore.
// When every account is out, the one that comes back first.
export function pickAccount(rows) {
  const live = rows.filter((r) => !availableAt(r));
  const byScore = (a, b) => pickScore(b).score - pickScore(a).score;
  if (live.length) return [...live].sort(byScore)[0];
  return [...rows].sort((a, b) => availableAt(a) - availableAt(b) || byScore(a, b))[0] || null;
}

// ---- output -------------------------------------------------------------------------------------

const M = (n) => (n >= 1e6 ? `${(n / 1e6).toFixed(1)}M` : `${Math.round(n / 1000)}k`);
const pad2 = (n) => String(n).padStart(2, "0");
export function ago(ms) {
  const m = Math.round(ms / 60000);
  if (m < 90) return `${m}m ago`;
  if (m < 48 * 60) return `${Math.round(m / 60)}h ago`;
  return `${Math.round(m / 1440)}d ago`;
}
function clock(t, now) {
  const d = new Date(t);
  const hm = `${pad2(d.getHours())}:${pad2(d.getMinutes())}`;
  return d.toDateString() === new Date(now).toDateString() ? hm : `${d.getDate()}/${d.getMonth() + 1} ${hm}`;
}
const typeName = (t) => (t === "five_hour" ? "5h limit" : t === "seven_day" ? "weekly limit" : t || "limit");

export function accountLine(r, { now = Date.now(), windowLabel = "5h" } = {}) {
  const who = `${r.label}${r.email ? ` ${r.email}` : ""}${r.tier ? ` ${r.tier.replace(/^default_claude_/, "")}` : ""}`;
  const split = r.sources.length > 1 ? ` (${r.sources.map((s) => `${s.machine} ${s.active}`).join(", ")})` : "";
  const hit0 = r.lastHit ? `last ${typeName(r.lastHit.type)} hit ${clock(r.lastHit.last, now)} (${ago(now - r.lastHit.last)})` : "no limit hit in the logs";
  const hit = r.limitedUntil ? `${hit0}; limited until ${clock(r.limitedUntil, now)}` : hit0;
  let est;
  if (r.blocked) est = `BLOCKED by ${typeName(r.blocked.type)} until ${clock(r.blocked.until, now)}`;
  else if (r.pct === null) est = "no observed limit to compare, headroom unknown (counted as full)";
  else est = `≈${Math.round(r.pct)}% of last observed limit (${M(r.baseline)}${r.baselineFrom ? `, ${r.baselineFrom}` : ` at ${clock(r.baselineAt, now)}`}; estimate)`;
  return `${who}: ${M(r.usage)} tok/${windowLabel}, ${r.messages} msgs, ${r.active} active${split}; ${hit}; ${est}`;
}

export function pickLine(r, { rows = [], now = Date.now() } = {}) {
  if (!r) return "pick: none (no account found)";
  const ids = r.ids.map((i) => (i === LAPTOP.id ? `"laptop"` : i));
  const where = r.ids.length > 1 ? ids.join(" (this PC) or ") : ids[0];
  const s = pickScore(r);
  const skipped = rows.filter((o) => o !== r && availableAt(o) > now).map((o) => `${o.label} limited until ${clock(availableAt(o), now)}`);
  const skip = skipped.length ? `; skipped ${skipped.join(", ")}` : "";
  if (availableAt(r) > now) return `pick: ${where}  [${r.label}: every account is limited, this one is back first at ${clock(availableAt(r), now)}, ${r.active} active${skip}]`;
  const why = r.blocked ? "all accounts blocked, soonest listed" : r.pct === null ? "headroom unknown" : `≈${Math.round(r.headroom)}% headroom, estimate`;
  return `pick: ${where}  [${r.label}: ${why}, plan x${s.weight}, ${r.active} active, score ${s.score.toFixed(2)} = headroom x plan / (active + 1)${skip}]`;
}

// ---- the command --------------------------------------------------------------------------------

const HELP = `usage: tools\\cli usage --accounts [--window 5h|7d|90m] [--history 7d] [--laptop <ssh alias>|--no-laptop] [--timeout <s>] [--json]
       tools\\cli usage --pick [same options]
  Usage per Claude account (Personal = ~/.claude, Gaming = ~/.claude-gaming + the laptop) over the rolling window:
  cost-weighted tokens, messages, active sessions (a log written in the last 10 min), the last rate-limit hit, and
  ≈% of the last observed limit (an estimate from the usage before each past hit). --pick prints the provider
  instance id for the next agent: the highest headroom x plan weight (max_20x counts 4 x max_5x) / (active sessions + 1).
  An account is skipped ("limited until HH:MM") from its last 5h limit hit until the logged reset (hit + 5 h when none was
  logged), unless turns logged after the hit show the window reopened; its usage looks low while it is blocked.`;

export async function main(args) {
  const opt = { windowMs: 5 * HOUR, windowLabel: "5h", history: HISTORY, laptop: LAPTOP.host, timeout: 90, json: false, pick: false, home: homedir(), ssh: process.env.LF_USAGE_SSH || "ssh" };
  for (let i = 0; i < args.length; i++) {
    const a = args[i];
    if (a === "--accounts") continue;
    else if (a === "--pick") opt.pick = true;
    else if (a === "--window") {
      opt.windowLabel = args[++i];
      opt.windowMs = parseWindow(opt.windowLabel);
    } else if (a === "--history") opt.history = parseWindow(args[++i]);
    else if (a === "--laptop") opt.laptop = args[++i];
    else if (a === "--no-laptop") opt.laptop = "";
    else if (a === "--timeout") opt.timeout = Number(args[++i]);
    else if (a === "--home") opt.home = args[++i]; // selftests
    else if (a === "--json") opt.json = true;
    else if (a === "-h" || a === "--help" || a === "help") {
      console.log(HELP);
      return 0;
    } else {
      console.error(`unknown argument ${a}\n${HELP}`);
      return 2;
    }
  }
  const now = Date.now();
  const lookbackMs = opt.history + opt.windowMs;
  const [local, remote] = await Promise.all([
    Promise.all(PROVIDERS.map((p) => scanConfigDir({ dir: join(opt.home, p.dir), home: opt.home, lookbackMs, now }))),
    opt.laptop ? scanRemote({ host: opt.laptop, lookbackMs, timeoutMs: opt.timeout * 1000, ssh: opt.ssh }) : Promise.resolve(null),
  ]);
  const sources = [];
  PROVIDERS.forEach((p, i) => {
    if (!local[i].missing) sources.push({ machine: "pc", id: p.id, label: p.label, scan: local[i] });
  });
  const notes = [];
  if (remote && remote.ok) {
    const sc = remote.scan;
    if (sc.missing) notes.push(`laptop: no Claude Code logs under its ~/.claude`);
    else sources.push({ machine: "laptop", id: LAPTOP.id, label: "Laptop", scan: sc });
  } else if (remote) notes.push(`laptop: unreachable (${remote.error}); its usage is missing from the account totals`);
  if (!sources.length) {
    console.error("no Claude Code logs found");
    return 1;
  }
  const accounts = mergeAccounts(sources);
  // a laptop whose login matches no PC account is its own row, so say it plainly
  const rows = analyze(accounts, { now, windowMs: opt.windowMs });
  const pick = pickAccount(rows);
  if (opt.json) {
    console.log(JSON.stringify({ window: opt.windowLabel, accounts: rows, notes, pick: pick ? pick.ids : null }, null, 1));
    return 0;
  }
  if (opt.pick) {
    for (const n of notes) console.error(n);
    console.log(pickLine(pick, { rows, now }));
    return 0;
  }
  for (const r of rows) console.log(accountLine(r, { now, windowLabel: opt.windowLabel }));
  for (const n of notes) console.log(n);
  console.log(pickLine(pick, { rows, now }));
  return 0;
}
