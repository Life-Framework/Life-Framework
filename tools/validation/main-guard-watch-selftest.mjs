// Proves the main-guard watchdog rules (#2754, tools/lib/main-guard.mjs): when the lease says the guard is down, what
// `lead` and `wt new` print, and what one `main-guard watch` tick does (restart a dead worker, never a live,
// stopped-on-purpose or crash-looping one), plus the wiring in wt.mjs, lead.mjs and main-guard.mjs.
// inputs: tools/lib/main-guard.mjs, tools/main-guard.mjs, tools/wt.mjs, tools/commands/lead.mjs, tools/cli.mjs
// red-proof 2026-11-17: before the watchdog rules existed this printed "FAIL cannot load"; with guardHealth's
//   `now - at > downMs` flipped to `<` the run printed "FAIL a 40 min old lease is down" and "FAIL a fresh lease is live".

// red-proof 2026-11-22 (progress, lease): with watchDecision's "renew" branch disabled this printed "FAIL a progressing worker with a
//   65 h old lease: the watchdog renews it, no hung? alert".
import { readFileSync } from "node:fs";
import * as g from "../lib/main-guard.mjs";

const fails = [];
const check = (ok, msg) => { if (!ok) fails.push(msg); };
const T = Date.parse("2026-11-17T23:00:00Z");
const ago = (min) => new Date(T - min * 60000).toISOString();
const held = (min, host = "LAPTOP", pid = 9288) => ({ state: "held", host, pid, at: ago(min) });
const src = (rel) => readFileSync(new URL(rel, import.meta.url), "utf8");

// guardHealth: the staleness rule.
check(g.guardHealth(held(2), { now: T }).state === "live", "a fresh lease is live");
check(g.guardHealth(held(14), { now: T }).state === "live", "a lease 14 min old is still live (one renewal may be late)");
const d40 = g.guardHealth(held(40), { now: T });
check(d40.state === "down" && d40.minutes === 40, `a 40 min old lease is down for 40 min, got ${JSON.stringify(d40)}`);
check(g.guardHealth({ state: "released", host: "LAPTOP", at: ago(5) }, { now: T }).state === "stopped", "a released lease is stopped");
check(g.guardHealth(null, { now: T }).state === "none", "no lease is none");
check(g.guardHealth(undefined, { now: T }).state === "unknown", "gh failure is unknown, not down");
check(g.guardHealth({ state: "held", host: "H", at: "garbage" }, { now: T }).state === "down", "an unreadable renewal time is down");

// guardAlert: the line `lead` and `wt new` print.
const a = g.guardAlert(held(40), { now: T, behind: 158 });
check(/main-guard down for 40 min/.test(a ?? "") && /you are 158 commit\(s\) behind main/.test(a ?? ""), `down alert names minutes and commits behind, got ${a}`);
check(g.guardAlert(held(20), { now: T, alive: 9288 }) === null && g.guardAlert(held(60), { now: T, alive: 9288 }) !== null, "a live local worker with a late lease is busy, one with a 60 min old lease is hung");
check(g.guardAlert(held(2), { now: T, behind: 3 }) === null, "a live guard prints nothing");
check(g.guardAlert(undefined, { now: T }) === null, "an unknown lease prints nothing (no false alarm when gh is down)");
check(!/behind/.test(g.guardAlert(held(40), { now: T }) ?? ""), "no commit count when it is not known");
check(/stopped 5 min ago/.test(g.guardAlert({ state: "released", host: "LAPTOP", at: ago(5) }, { now: T }) ?? ""), "a stopped guard is reported");

// watchDecision: one watchdog tick.
const W = (o) => g.watchDecision({ host: "LAPTOP", now: T, ...o });
check(W({ alive: 9288, lease: held(1) }).action === "none", "a live worker is left alone");
check(W({ alive: 9288, lease: held(10) }).action === "none", "a live worker with a lease 10 min old (long compile) is left alone");
check(W({ alive: 9288, lease: held(60) }).action === "alert", "a live worker with a lease 60 min old is reported hung, never killed");
check(W({ alive: null, lease: held(40) }).action === "restart", "a dead worker with a stale lease is restarted");
check(W({ alive: null, lease: held(2) }).action === "restart", "a dead worker of ours is restarted before its lease ages");
check(W({ alive: null, lease: held(2, "OTHER-PC", 1) }).action === "none", "another machine's fresh lease is respected");
check(W({ alive: null, lease: held(40, "OTHER-PC", 1) }).action === "restart", "another machine's stale lease is taken over");
check(W({ alive: null, lease: { state: "released", host: "LAPTOP", at: ago(5) } }).action === "none", "a guard stopped on purpose is not restarted");
check(W({ alive: null, lease: null }).action === "restart", "a guard that never ran is started");
check(W({ alive: null, lease: undefined }).action === "restart", "gh down and no worker: start on the local lock");
check(W({ off: true, alive: null, lease: held(40) }).action === "none", "LF_MAIN_GUARD=0 is respected");
const recent = [ago(50), ago(30), ago(10)];
check(W({ alive: null, lease: held(40), restarts: recent }).action === "alert", "three restarts in an hour: alert, no fourth restart");
check(W({ alive: null, lease: held(40), restarts: [ago(300), ago(200), ago(100), ago(90)] }).action === "restart", "old restarts do not count");

// Progress (2026-11-22): a worker whose lease went stale under a live, working worker (GitHub refused every renewal,
// "maximum number of statuses") is renewed by the watchdog; `hung?` fires only when the worker truly makes no progress.
const okLocal = { beatAt: ago(1), activityAt: ago(3) };
check(g.workerProgress(okLocal, T).state === "progressing", "a fresh heartbeat and fresh step output is progressing");
check(g.workerProgress({ beatAt: ago(30), activityAt: ago(1) }, T).state === "hung", "no heartbeat for 30 min is hung");
check(g.workerProgress({ beatAt: ago(1), activityAt: ago(130) }, T).state === "hung", "a step silent for 130 min is hung (its budget is 90)");
check(g.workerProgress({ beatAt: ago(1), activityAt: ago(60) }, T).state === "progressing", "a step silent for 60 min is a long build, not hung");
check(g.workerProgress({}, T).state === "unknown" && g.workerProgress(null, T).state === "unknown", "no heartbeat recorded (an old worker) is unknown");
check(W({ alive: 9288, lease: held(3889), local: okLocal }).action === "renew", "a progressing worker with a 65 h old lease: the watchdog renews it, no hung? alert");
check(W({ alive: 9288, lease: held(20), local: okLocal }).action === "renew", "a progressing worker with a 20 min old lease (3 missed renewals) is renewed too");
check(W({ alive: 9288, lease: held(10), local: okLocal }).action === "none", "a lease 10 min old is within one late renewal: left alone");
const hungW = W({ alive: 9288, lease: held(60), local: { beatAt: ago(40), activityAt: ago(40) } });
check(hungW.action === "alert" && /no progress/.test(hungW.why) && /no heartbeat for 40 min/.test(hungW.why), `no heartbeat: alert names why, got ${JSON.stringify(hungW)}`);
check(W({ alive: 9288, lease: held(60), local: null }).action === "alert" && /hung\?/.test(W({ alive: 9288, lease: held(60) }).why), "an old worker with no heartbeat keeps the old rule (alert past 45 min)");
check(W({ alive: 9288, lease: held(3889, "OTHER-PC", 1), local: okLocal }).action === "none", "never renew another machine's lease");
check(W({ alive: 9288, lease: { state: "released", host: "LAPTOP", at: ago(5) }, local: okLocal }).action === "none", "never renew a lease released on purpose");
check(W({ alive: null, lease: held(3889), local: okLocal }).action === "restart", "a dead worker is still restarted whatever its old heartbeat says");
check(g.guardAlert(held(3889), { now: T, alive: 9288, local: okLocal }) === null, "lead: a live, progressing worker with a stale lease is not a dead guard");
check(g.guardAlert(held(3889), { now: T, alive: 9288, local: { beatAt: ago(40) } }) !== null, "lead: a live worker with no heartbeat and a 65 h lease is shouted about");
check(g.guardAlert(held(3889), { now: T }) !== null, "lead: no live worker, a stale lease: down");

// Wiring.
const wtSrc = src("../wt.mjs");
check(/guardWarn = guardAlert\(readLease\(root\)/.test(wtSrc) && /pinned, pinNote, guardWarn/.test(wtSrc), "wt new computes and returns guardWarn");
check(/r\.guardWarn/.test(src("../cli.mjs")), "wt new prints the warning");
const lead = src("../commands/lead.mjs");
check(/guardAlert\(lease, \{ behind, alive: g\.workerAlive/.test(lead) && /MAIN GUARD DOWN/.test(lead), "lead prints MAIN GUARD DOWN");
const mgSrc = src("../main-guard.mjs");
check(/sub === "watch"/.test(mgSrc) && /mg\.watchDecision\(/.test(mgSrc), "main-guard watch runs watchDecision");
check(/setInterval\(/.test(mgSrc) && /beat\.unref/.test(mgSrc), "the worker renews its lease on a timer, not only between ticks");
check(/watch/.test(src("../commands/main-guard.mjs")), "the command help lists watch");
check(/writeLeaseChecked\(dir, "held", HOST, process\.pid\)/.test(mgSrc) && /lease renewal FAILED/.test(mgSrc), "the worker's renewal checks the post and logs a failure (it was swallowed before)");
check(/setInterval\(\(\) => \{ try \{ writeState\(root, \{ beatAt: now\(\), activityAt: activityNow\(\) \}\)/.test(mgSrc), "the worker writes its heartbeat every minute");
check(/d\.action === "renew"/.test(mgSrc) && /local: \{ beatAt: st\.beatAt, activityAt: st\.activityAt \}/.test(mgSrc), "main-guard watch renews a progressing worker's lease");
check(/pin\.leaseFromStatuses\(/.test(lead) && /local: \{ beatAt: ls\.beatAt/.test(lead), "lead reads every lease context and passes the worker's heartbeat");

if (fails.length) { for (const f of fails) console.log(`FAIL  ${f}`); process.exit(1); }
console.log("PASS  main-guard watchdog selftest");
