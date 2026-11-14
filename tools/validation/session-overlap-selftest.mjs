// tools/validation/session-overlap-selftest.mjs
// inputs: tools/lib/session-overlap.mjs, tools/cli.mjs, agent-box/session.mjs
//
// Proves the logic that starts client 1 with the server (tools/lib/session-overlap.mjs): when the overlap
// is on, and what to do with a client that raced the server (retry once, then stop waiting). Also that
// the CLI really goes through that module and that `LF_SESSION_SERIAL=1` keeps the old order.
// red-proof: with firstClientVerdict returning "wait" for an unconnected client past the grace period,
// this printed `session-overlap-selftest: a client that tried before the server was ready and never
// connected must be retried` and exited 1.
import { readFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { CONNECT_TRY, CONNECTED, GRACE_MS, HARD_MS, firstClientVerdict, overlapPlan } from "../lib/session-overlap.mjs";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "../..");
const fails = [];
const eq = (got, want, what) => {
  if (got !== want) fails.push(`${what}: got ${JSON.stringify(got)}, wanted ${JSON.stringify(want)}`);
};

// Plan: on by default, off for no clients and for the serial switch.
eq(overlapPlan({ clients: 1, env: {} }).early, true, "one client overlaps");
eq(overlapPlan({ clients: 3, env: {} }).early, true, "three clients: client 1 overlaps");
eq(overlapPlan({ clients: 0, env: {} }).early, false, "a server-only session has no client to start");
eq(overlapPlan({ clients: 1, env: { LF_SESSION_SERIAL: "1" } }).early, false, "LF_SESSION_SERIAL=1 is the old order");
eq(overlapPlan({ clients: 1, env: { LF_SESSION_SERIAL: "0" } }).early, true, "LF_SESSION_SERIAL=0 is not the switch");

const T0 = 1_000_000;
const v = (o) => firstClientVerdict({ now: T0 + 100000, serverReadyAt: T0 + 12000, launchedAt: T0, ...o });

// A connected client is connected, however the rest looks.
eq(v({ connected: true, alive: false }), "connected", "connected beats everything");
// The server is still booting: wait, even when the client already tried (judged after it is ready).
eq(firstClientVerdict({ now: T0 + 20000, serverReadyAt: 0, launchedAt: T0, attemptSeenAt: T0 + 15000 }), "wait", "server not ready: wait");
eq(firstClientVerdict({ now: T0 + 400000, serverReadyAt: 0, launchedAt: T0 }), "wait", "server not ready, however late: the boot has its own timeout");
// The normal case: server ready at +12 s, client tries at +28 s, connects within a second.
eq(firstClientVerdict({ now: T0 + 28500, serverReadyAt: T0 + 12000, launchedAt: T0, attemptSeenAt: T0 + 28000 }), "wait", "just tried, inside the grace period");
// The race: the client tried at +9 s, the server was ready at +12 s, nothing connected 20 s after that.
eq(firstClientVerdict({ now: T0 + 12000 + GRACE_MS - 1, serverReadyAt: T0 + 12000, launchedAt: T0, attemptSeenAt: T0 + 9000 }), "wait", "inside the grace period after the server is ready");
eq(firstClientVerdict({ now: T0 + 12000 + GRACE_MS + 1, serverReadyAt: T0 + 12000, launchedAt: T0, attemptSeenAt: T0 + 9000 }), "retry", "a client that tried before the server was ready and never connected must be retried");
// The grace period runs from the later of the two: a late attempt is not retried for being old.
eq(firstClientVerdict({ now: T0 + 40000, serverReadyAt: T0 + 12000, launchedAt: T0, attemptSeenAt: T0 + 35000 }), "wait", "the grace runs from the attempt when that is after the ready line");
// The second failure does not relaunch again: the normal join check reports it.
eq(firstClientVerdict({ now: T0 + 12000 + GRACE_MS + 1, serverReadyAt: T0 + 12000, launchedAt: T0, attemptSeenAt: T0 + 9000, retried: true }), "stuck", "a relaunched client is not relaunched twice");
// A client that never tries at all (stuck in its own boot).
eq(firstClientVerdict({ now: T0 + 12000 + HARD_MS - 1, serverReadyAt: T0 + 12000, launchedAt: T0 }), "wait", "no attempt yet, inside the hard limit");
eq(firstClientVerdict({ now: T0 + 12000 + HARD_MS + 1, serverReadyAt: T0 + 12000, launchedAt: T0 }), "retry", "no attempt past the hard limit: retry");
// A client that died: relaunched once, then left to the join check.
eq(v({ alive: false }), "retry", "a dead client is relaunched");
eq(v({ alive: false, retried: true }), "stuck", "a dead relaunched client is left to the join check");
eq(firstClientVerdict({ now: T0 + 3000, serverReadyAt: 0, launchedAt: T0, alive: false }), "retry", "a client that died before the server was ready is relaunched too");

// The log patterns match what the engine prints (lines from lf-cloud 2026-11-14).
eq(CONNECT_TRY.test("10:41:00.194 NETWORK      : Starting RPL client, number of addresses to try connecting to: 1"), true, "connect-attempt line");
eq(CONNECTED.test("10:41:00.713 RPL          : Client L2 connection established."), true, "connected line");
eq(CONNECTED.test("10:41:00.799 RPL          : Client event Connected"), false, "the event line is not the established line");

// The CLI is wired to the module, and the serial switch is the module's.
const cli = readFileSync(join(ROOT, "tools", "cli.mjs"), "utf8");
if (!/from "\.\/lib\/session-overlap\.mjs"/.test(cli)) fails.push("tools/cli.mjs does not import tools/lib/session-overlap.mjs");
if (!/overlapPlan\(/.test(cli) || !/firstClientVerdict\(/.test(cli)) fails.push("tools/cli.mjs does not call overlapPlan and firstClientVerdict");

if (fails.length) {
  for (const f of fails) console.log(`session-overlap-selftest: ${f}`);
  process.exit(1);
}
console.log("session-overlap-selftest: OK (plan, race retry, hard limit, dead client, log patterns, CLI wiring)");
