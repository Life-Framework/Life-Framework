import { existsSync, readFileSync } from "node:fs";
import { newestLog, readSession, sessionDir, sessionPaths } from "../session.mjs";

// #1489 (B2PASS, parent #1006): a server whose join password is set refuses a client that sends the
// wrong one (the engine says so in the client's console.log) and lets one with the right one in.
//   password-gate-proof              (up --clients 2 with a join password; client 1 sends a wrong one, client 2 the right one)
//   password-gate-proof --selftest   (the judge against fixture logs; no session needed)
//
// The password never appears here: a session records only `joinPassword: true` in session.json and keeps
// the value in its own profile copy (SECRETS #1463). Today `session up` cannot set it: a session boots
// `-server <scenario> -addons ...` and neither game exe has a password parameter (checked 2026-11-15 on the
// installed client and server exes), while `-config` refuses `-addons`. So the run stops at "mechanism" with
// the card to file; the judge below is what runs once a session can carry the password.

// red-proof 2026-11-15: with the `refused` check in judgePasswordGate disabled, the selftest case "no refusal line and no entry" failed.
// The engine's refusal text in a client console.log (found in the client exe: "maybe wrong password").
export const REFUSAL = /wrong password|password mismatch|SERVER_PASSWORD_MISMATCH/i;
// A client that got in reaches the world: the harness `state` verb answers and the log has no refusal.
export const ENTERED = /Entered online game state|Connected to server|Player connected/i;

// Pure. wrong / right: the console.log text of the client that sent each password. Returns the failing checks.
export function judgePasswordGate({ wrong = "", right = "" }) {
  const fails = [];
  if (!REFUSAL.test(wrong)) fails.push('refused: the wrong-password client log has no refusal line (the server may have no join password set)');
  if (ENTERED.test(wrong)) fails.push("refused: the wrong-password client reached the game");
  if (REFUSAL.test(right)) fails.push("joined: the right-password client was refused");
  if (!ENTERED.test(right)) fails.push("joined: the right-password client never reached the game");
  return fails;
}

function selftest() {
  const refusedLog = "Connecting to 127.0.0.1\nConnection failed: wrong password\n";
  const joinedLog = "Connecting to 127.0.0.1\nEntered online game state.\n";
  const cases = [
    ["gate on, both clients behave", { wrong: refusedLog, right: joinedLog }, null],
    // The password removed from server.json: the wrong client walks in, so the proof fails at "refused".
    ["password removed", { wrong: joinedLog, right: joinedLog }, "refused:"],
    ["no refusal line and no entry", { wrong: "", right: joinedLog }, "refused:"],
    ["right password refused", { wrong: refusedLog, right: refusedLog }, "joined:"],
    ["right client never arrived", { wrong: refusedLog, right: "" }, "joined:"],
  ];
  const bad = [];
  for (const [name, logs, expect] of cases) {
    const fails = judgePasswordGate(logs);
    const ok = expect === null ? fails.length === 0 : fails.length > 0 && fails.every((f) => f.startsWith(expect));
    if (!ok) bad.push(`${name}: got ${JSON.stringify(fails)}`);
  }
  for (const b of bad) console.log(`FAIL  ${b}`);
  console.log(bad.length ? "password-gate selftest: FAIL" : `password-gate selftest: PASS (${cases.length} cases)`);
  return bad.length ? 1 : 0;
}

function clientLog(dir, n) {
  const f = newestLog(sessionPaths(dir).clients[n - 1].logs);
  return f && existsSync(f) ? readFileSync(f, "utf8") : "";
}

async function run(args) {
  if (args.includes("--selftest")) return selftest();
  const dir = sessionDir();
  const s = readSession(dir);
  if (!s || s.stoppedAt) {
    console.log("password-gate: BLOCKED  no live session; node agent-box/session.mjs up --clients 2 (with a join password)");
    return 2;
  }
  if (s.joinPassword !== true || (s.clients ?? []).length < 2) {
    console.log("password-gate: BLOCKED  mechanism: this session has no join password for a client to send. `session up` cannot set one yet (no password parameter on either exe; -config refuses -addons). Needs the harness card: up --join-password, client 1 wrong, client 2 right, session.json joinPassword: true.");
    return 2;
  }
  const fails = judgePasswordGate({ wrong: clientLog(dir, 1), right: clientLog(dir, 2) });
  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "password-gate: FAIL" : "password-gate: PASS (the wrong password was refused, the right one joined)");
  return fails.length ? 1 : 0;
}

export default {
  name: "password-gate-proof",
  noReset: true,
  usage: "password-gate-proof [--selftest]   (up --clients 2 with a join password: client 1 wrong, client 2 right)",
  run: (ctx) => run(ctx.args ?? []),
};
