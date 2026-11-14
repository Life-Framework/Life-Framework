// Proves `tools\cli usage --accounts|--pick` (tools/agents/usage-accounts.mjs, usage-scan.mjs) on fixture logs
// inputs: tools/**, agent-box/**, .githooks/**
// written to a temp home (a Personal and a Gaming config dir, plus a made-up laptop scan):
//   1. a turn written as two records with one message.id counts once at the shared cost weighting, a
//      <synthetic> record is not a turn, and the oauthAccount is read masked (dir/.claude.json, or
//      ~/.claude.json for the default dir);
//   2. several error records of one hit are one hit; a log written in the last 10 min is an active session;
//   3. two machines with the same login are one account, another login stays separate;
//   4. the window counts only what was spent since the last reset, the observed limit is what was spent
//      in the block behind the last hit, the percentage and headroom are hand-worked, an account that
//      never hit a limit borrows the other's limit scaled by plan tier;
//   5. pick scores headroom x plan weight / (active + 1), so Personal (max_20x) counts 4 x Gaming (max_5x) and
//      busy accounts lose; never a blocked account, nor one whose last 5h limit hit is under 5 h old (it prints
//      "limited until HH:MM" even when its logged reset passed and its usage looks empty); the pick line says why;
//   6. an unreachable laptop is said plainly and the PC accounts still print: one line each plus the pick.
// red-proof: 2026-11-14, with the clip at the last reset removed the window counted the 14:30 turn and it
// printed "FAIL usage-accounts: gaming window usage: 12300 != 2300". Pick, same day: with the 5h rule set to 0
// it printed "FAIL usage-accounts: limited until the hit plus 5 h: 0 != ..."; with the plan weight forced to 1,
// "FAIL usage-accounts: plan weight: 50% of a max_20x beats 100% of a max_5x: Gaming != Personal"; with the
// active count dropped from the score, "FAIL usage-accounts: same plan, same headroom: fewer active agents wins: a != b".
import { mkdirSync, mkdtempSync, rmSync, utimesSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { analyze, accountLine, clusterHits, main, mergeAccounts, parseWindow, pickAccount, pickLine, scanRemote } from "../agents/usage-accounts.mjs";
import { cost, maskEmail, scanConfigDir } from "../agents/usage-scan.mjs";

const failures = [];
const eq = (got, want, msg) => {
  if (got !== want) failures.push(`${msg}: ${got} != ${want}`);
};
const near = (got, want, msg) => {
  if (Math.abs(got - want) > 0.01) failures.push(`${msg}: ${got} != ${want}`);
};

const NOW = Date.parse("2026-11-14T18:00:00Z");
const at = (hhmm) => `2026-11-14T${hhmm}:00Z`;
const home = mkdtempSync(join(tmpdir(), "lf-usage-accounts-"));
const turn = (id, ts, usage, extra = {}) => JSON.stringify({ type: "assistant", timestamp: ts, message: { id, role: "assistant", model: "claude-opus-5-5", usage: { output_tokens: 0, ...usage }, content: [] }, ...extra });
const synth = (id, ts) => turn(id, ts, { input_tokens: 0, output_tokens: 0 }, {}).replace("claude-opus-5-5", "<synthetic>");
const limitHit = (id, ts, resetsAt, type) =>
  JSON.stringify({
    type: "assistant",
    timestamp: ts,
    message: { id, role: "assistant", model: "<synthetic>", usage: { input_tokens: 0, output_tokens: 0 }, content: [{ type: "text", text: "You've hit your limit" }] },
    error: "rate_limit",
    apiError: "usage_limit_reached",
    isApiErrorMessage: true,
    quotaLimits: { rateLimitType: type, resetsAt: Math.floor(resetsAt / 1000) },
  });
const write = (rel, lines, mtime) => {
  const p = join(home, rel);
  mkdirSync(join(p, ".."), { recursive: true });
  writeFileSync(p, lines.join("\n") + "\n");
  utimesSync(p, new Date(mtime), new Date(mtime));
};
const login = (path, uuid, email, tier) => {
  mkdirSync(join(path, ".."), { recursive: true });
  writeFileSync(path, JSON.stringify({ oauthAccount: { accountUuid: uuid, emailAddress: email, organizationRateLimitTier: tier } }));
};

// Gaming: block 10:50-15:50 (resetsAt 15:50), hit first seen 14:00 by three agents
const resets = Date.parse(at("15:50"));
login(join(home, ".claude-gaming", ".claude.json"), "uuid-gaming", "kate@gmail.com", "default_claude_max_5x");
write(
  ".claude-gaming/projects/p/s1.jsonl",
  [
    turn("pre", at("09:30"), { input_tokens: 7000 }), // before the block: not in the limit behind the hit
    turn("t0", at("11:00"), { input_tokens: 5000 }),
    turn("t1", at("12:55"), { input_tokens: 100, cache_creation_input_tokens: 1000, cache_read_input_tokens: 10000, output_tokens: 10 }),
    turn("t1", at("12:55"), { input_tokens: 100, cache_creation_input_tokens: 1000, cache_read_input_tokens: 10000, output_tokens: 200 }), // same id again, more output
    synth("syn", at("12:56")),
    limitHit("e1", "2026-11-14T14:00:00Z", resets, "five_hour"),
    limitHit("e2", "2026-11-14T14:00:30Z", resets, "five_hour"),
    limitHit("e3", "2026-11-14T14:01:00Z", resets, "five_hour"),
    turn("t3", at("14:30"), { input_tokens: 9999 }), // between the hit and the reset: before the new block
    turn("t2", at("16:10"), { input_tokens: 1000, output_tokens: 100 }),
    turn("t4", at("17:30"), { input_tokens: 500 }),
  ],
  NOW - 5 * 60000, // written 5 min ago: an active session
);
write(".claude-gaming/projects/p/s2.jsonl", [turn("old", at("15:00"), { input_tokens: 1 })], NOW - 3 * 3600e3); // idle
// Personal: default dir, login in ~/.claude.json; never hit a limit
login(join(home, ".claude.json"), "uuid-personal", "jon@gmail.com", "default_claude_max_20x");
write(".claude/projects/p/s3.jsonl", [turn("p1", at("17:00"), { input_tokens: 1000 })], NOW - 2 * 60000);

const g = await scanConfigDir({ dir: join(home, ".claude-gaming"), home, lookbackMs: 8 * 24 * 3600e3, now: NOW });
const p = await scanConfigDir({ dir: join(home, ".claude"), home, lookbackMs: 8 * 24 * 3600e3, now: NOW });

// 1: weighting, dedupe, identity
eq(cost({ input_tokens: 100, cache_creation_input_tokens: 1000, cache_read_input_tokens: 10000, output_tokens: 200 }), 3350, "cost weighting");
eq(g.minutes[Date.parse(at("12:55"))][0], 3350, "one turn, written twice, counted once at its largest output");
eq(g.minutes[Date.parse(at("12:55"))][1], 1, "message count for that minute");
eq(g.minutes[Date.parse(at("12:56"))], undefined, "synthetic record is not a turn");
eq(maskEmail("kate@gmail.com"), "k***@gmail.com", "masked email");
eq(g.identity.email, "k***@gmail.com", "gaming identity masked");
eq(p.identity.email, "j***@gmail.com", "default dir reads ~/.claude.json");
eq(JSON.stringify(g).includes("kate"), false, "the scan never carries the address");
// 2: hits and active sessions
eq(g.hits.length, 3, "three error records");
const cl = clusterHits(g.hits);
eq(cl.length, 1, "one hit");
eq(cl[0].count, 3, "records in that hit");
eq(cl[0].first, Date.parse(at("14:00")), "hit first seen");
eq(cl[0].type, "five_hour", "hit type");
eq(g.active, 1, "active sessions on gaming (s2 is 3 h old)");
eq(p.active, 1, "active sessions on personal");

// 3: merge machines by login
const laptop = { minutes: { [Date.parse(at("17:45"))]: [300, 1] }, hits: [], active: 3, files: 3, identity: { uuid: "uuid-gaming", email: "k***@gmail.com", tier: "default_claude_max_5x" } };
const accounts = mergeAccounts([
  { machine: "pc", id: "claudeAgent", label: "Personal", scan: p },
  { machine: "pc", id: "claudeAgent_claude_gaming", label: "Gaming", scan: g },
  { machine: "laptop", id: "laptop", label: "Laptop", scan: laptop },
]);
eq(accounts.length, 2, "laptop on the Gaming login is not a third account");
const gam = accounts.find((a) => a.label === "Gaming");
eq(gam.ids.join(","), "claudeAgent_claude_gaming,laptop", "Gaming ids");
const stranger = mergeAccounts([{ machine: "pc", id: "claudeAgent_claude_gaming", label: "Gaming", scan: g }, { machine: "laptop", id: "laptop", label: "Laptop", scan: { ...laptop, identity: { uuid: "uuid-other", email: "x***@gmail.com", tier: "" } } }]);
eq(stranger.length, 2, "a laptop on another login stays its own account");

// 4: window since the last reset, observed limit, percentage
const rows = analyze(accounts, { now: NOW, windowMs: 5 * 3600e3 });
const G = rows.find((r) => r.label === "Gaming");
const P = rows.find((r) => r.label === "Personal");
near(G.usage, 1500 + 500 + 300, "gaming window usage"); // 16:10 + 17:30 + laptop 17:45; not the 14:30 turn
eq(G.messages, 3, "gaming window messages");
eq(G.active, 1 + 3, "gaming active sessions, PC plus laptop");
near(G.baseline, 5000 + 3350, "observed limit: the block behind the hit, not the 09:30 turn");
near(G.pct, (100 * 2300) / 8350, "gaming percentage");
near(G.headroom, 100 - (100 * 2300) / 8350, "gaming headroom");
eq(G.blocked, null, "gaming not blocked after the reset");
eq(G.lastHit.last, Date.parse("2026-11-14T14:01:00Z"), "last hit time");
near(P.baseline, (8350 * 20) / 5, "personal borrows the limit scaled by plan");
eq(P.baselineFrom, "scaled from Gaming by plan", "and says so");
near(P.pct, (100 * 1000) / 33400, "personal percentage");
const open = analyze(mergeAccounts([{ machine: "pc", id: "claudeAgent_claude_gaming", label: "Gaming", scan: g }]), { now: Date.parse(at("15:00")), windowMs: 5 * 3600e3 });
eq(open[0].blocked && open[0].blocked.until, resets, "a hit whose reset is still ahead is blocked");
eq(open[0].headroom, 0, "blocked headroom is 0");
eq(parseWindow("90m"), 5400e3, "window 90m");
eq(parseWindow("7d"), 7 * 86400e3, "window 7d");
const line = accountLine(G, { now: NOW });
if (!/â‰ˆ28% of last observed limit/.test(line) || !/estimate/.test(line) || !/pc 1, laptop 3/.test(line)) failures.push(`account line: ${line}`);

// 5: pick
const M5 = "default_claude_max_5x";
const M20 = "default_claude_max_20x";
const mk = (label, headroom, active, o = {}) => ({ label, ids: [label], headroom, active, blocked: null, limitedUntil: 0, tier: M5, pct: 100 - headroom, ...o });
eq(pickAccount([mk("a", 40, 2), mk("b", 60, 2)]).label, "b", "same plan, same load: most headroom wins");
eq(pickAccount([mk("a", 60, 5), mk("b", 60, 2)]).label, "b", "same plan, same headroom: fewer active agents wins");
// plan size: Personal (max_20x) has 4 x Gaming's capacity, so half-used Personal still beats an empty Gaming
eq(pickAccount([mk("Gaming", 100, 0, { tier: M5 }), mk("Personal", 50, 0, { tier: M20 })]).label, "Personal", "plan weight: 50% of a max_20x beats 100% of a max_5x");
eq(pickAccount([mk("Gaming", 100, 0, { tier: M5 }), mk("Personal", 20, 0, { tier: M20 })]).label, "Gaming", "plan weight: 20% of a max_20x loses to 100% of a max_5x");
// active agents share the capacity
eq(pickAccount([mk("Gaming", 100, 0, { tier: M5 }), mk("Personal", 100, 12, { tier: M20 })]).label, "Gaming", "twelve agents on Personal leave an idle Gaming the larger share");
eq(pickAccount([mk("Gaming", 100, 5, { tier: M5 }), mk("Personal", 100, 12, { tier: M20 })]).label, "Personal", "but five on Gaming and twelve on Personal is Personal");
// limits
const lim = { type: "five_hour", until: 1 };
eq(pickAccount([mk("a", 0, 1, { blocked: lim }), mk("b", 5, 9)]).label, "b", "blocked is skipped");
eq(pickAccount([mk("a", 100, 0, { limitedUntil: NOW + 3 * 3600e3 }), mk("b", 5, 9)]).label, "b", "an account inside its 5h limit is skipped whatever its headroom says");
eq(pickAccount([mk("a", 0, 1, { blocked: lim }), mk("b", 0, 9, { blocked: lim })]).label, "a", "all blocked still names one");
eq(pickAccount([mk("a", 100, 0, { limitedUntil: NOW + 3 * 3600e3 }), mk("b", 100, 0, { limitedUntil: NOW + 1 * 3600e3 })]).label, "b", "all limited: the one that is back first");
// the real case: a 5h limit hit 2 min ago whose reset time already passed; usage is low because it was blocked
const hitAgo = { ts: NOW - 2 * 60000, type: "five_hour", resetsAt: NOW - 60000 };
const freshHit = { label: "Gaming", email: "", tier: M5, ids: ["claudeAgent_claude_gaming"], sources: [{ machine: "pc", active: 2, files: 2 }], minutes: {}, hits: [hitAgo] };
const idle = { label: "Personal", email: "", tier: M20, ids: ["claudeAgent"], sources: [{ machine: "pc", active: 9, files: 9 }], minutes: { [NOW - 600e3]: [20000, 4] }, hits: [] };
const real = analyze([freshHit, idle], { now: NOW, windowMs: 5 * 3600e3 });
const rg = real.find((r) => r.label === "Gaming");
const rp = real.find((r) => r.label === "Personal");
eq(rg.limitedUntil, hitAgo.ts + 5 * 3600e3, "limited until the hit plus 5 h");
eq(rg.blocked, null, "the logged reset time is past, so only the 5 h rule holds it");
eq(rg.headroom, 100, "and its headroom looks full, which is the bug");
eq(rp.limitedUntil, 0, "an account with no hit is not limited");
eq(pickAccount(real).label, "Personal", "the pick skips the account hit 2 min ago");
if (!/limited until \d\d:\d\d|limited until \d+\/\d+ \d\d:\d\d/.test(accountLine(rg, { now: NOW }))) failures.push(`account line has no "limited until": ${accountLine(rg, { now: NOW })}`);
const reason = pickLine(pickAccount(real), { rows: real, now: NOW });
if (!/plan x20/.test(reason) || !/9 active/.test(reason) || !/score/.test(reason) || !/skipped Gaming limited until/.test(reason)) failures.push(`pick reason: ${reason}`);
eq(analyze([freshHit], { now: hitAgo.ts + 5 * 3600e3 + 1000, windowMs: 5 * 3600e3 })[0].limitedUntil, 0, "5 h after the hit the account is available again");
eq(analyze([{ ...freshHit, hits: [{ ...hitAgo, type: "seven_day" }] }], { now: NOW, windowMs: 5 * 3600e3 })[0].limitedUntil, 0, "only a 5-hour hit sets the 5 h rule");
eq(pickLine(G.ids ? G : null).startsWith('pick: claudeAgent_claude_gaming (this PC) or "laptop"'), true, "pick shows the PC provider id and laptop for Gaming");
eq(pickLine(P).startsWith("pick: claudeAgent "), true, "pick shows the Personal provider id");

// 6: unreachable laptop, through the real command
const bad = await scanRemote({ lookbackMs: 1000, timeoutMs: 5000, ssh: "lf-no-such-ssh-binary" });
eq(bad.ok, false, "missing ssh is a result, not a throw");
const out = [];
const log = console.log;
const prevSsh = process.env.LF_USAGE_SSH;
process.env.LF_USAGE_SSH = "lf-no-such-ssh-binary";
console.log = (...a) => out.push(a.join(" "));
let code;
try {
  code = await main(["--accounts", "--home", home, "--history", "30d"]);
} finally {
  console.log = log;
  if (prevSsh === undefined) delete process.env.LF_USAGE_SSH;
  else process.env.LF_USAGE_SSH = prevSsh;
}
eq(code, 0, "exit code with the laptop down");
eq(out.length, 4, `output is one line per account, the laptop note and the pick: ${out.join(" / ")}`);
if (!out.some((l) => l.startsWith("laptop: unreachable"))) failures.push(`no unreachable note: ${out.join(" / ")}`);
if (!out.some((l) => l.startsWith("pick: "))) failures.push("no pick line");
if (out.join("\n").includes("jon@") || out.join("\n").includes("kate@")) failures.push("an unmasked address was printed");

rmSync(home, { recursive: true, force: true });
if (failures.length) {
  for (const f of failures) console.error(`FAIL usage-accounts: ${f}`);
  process.exit(1);
}
console.log("OK usage-accounts selftest");
