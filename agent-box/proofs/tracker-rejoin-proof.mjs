import { spawnSync } from "node:child_process";
import { readFileSync } from "node:fs";
import { join } from "node:path";
import { ROOT, arrive, clientBox, newestLog, readSession, sessionDir, sessionPaths } from "../session.mjs";

// #3198: a running contract's tracker line comes back after a real rejoin on a dedicated server (#3144).
//   node agent-box/session.mjs up --mission island --wait
//   node agent-box/session.mjs tracker-rejoin-proof
//   node agent-box/session.mjs down
// Three steps, one client:
//  1. the client takes the harbour crates contract the way a player does (Known standing through `repgrant`,
//     Marcel Roux, the crates reply, the take reply);
//  2. it waits 30 s, then `session rejoin 1` (the real client process quits and joins the dedicated server again);
//  3. the client's OWN log (`[LFDebug:Reputation] contract line received: ...`, written by
//     LF_ContractTracker.Receive) shows the line again with less than the full limit left, and holds no line for
//     a contract that is not the running one (a contract that has ended is cleared, never shown).
// The server's journal of the send was never enough: this reads the receiving client.
const MARCEL = "Resident_dock_harbour_master";
const MARCEL_PLACE = "Fit_saint_philippe_harbour_Place";
const TALK = "LF_DialogueTalkAction";
const CONTRACT = "harbour_crates";
const WAIT_BEFORE_REJOIN_S = 30;

const RECEIVED = /\[LFDebug:Reputation\] contract line received: (?:(\S+) limit=(\d+) left=(\d+)|cleared)/;

// Pure: every contract line a client logged, in order ({ id, limit, left }, or { cleared: true }).
export function receivedLines(text) {
  const out = [];
  for (const l of String(text ?? "").split(/\r?\n/)) {
    const m = RECEIVED.exec(l);
    if (!m) continue;
    out.push(m[1] ? { id: m[1], limit: Number(m[2]), left: Number(m[3]) } : { cleared: true });
  }
  return out;
}

// Pure: the verdict. `before` and `after` are receivedLines() of the client log from before and after the
// rejoin; `waitedS` is the pause before it. Returns the failure messages (none is a pass).
export function judgeTrackerRejoin(before, after, contract, waitedS) {
  const fails = [];
  const first = before.find((x) => x.id === contract);
  if (!first) fails.push(`the client never received the ${contract} line when the contract was taken`);
  if (first && first.left !== first.limit) fails.push(`the line sent when the contract was taken had ${first.left} of ${first.limit} s left, not the full limit`);
  const back = after.filter((x) => x.id === contract);
  if (!back.length) {
    fails.push(`after the rejoin the client did not receive the ${contract} line again`);
    return fails;
  }
  const line = back[back.length - 1];
  if (!(line.left < line.limit)) fails.push(`the line after the rejoin has the full limit left (${line.left} of ${line.limit}): the time spent was lost`);
  if (line.left <= 0) fails.push(`the line after the rejoin has no time left (${line.left})`);
  if (first && line.left > first.limit - waitedS + 5) fails.push(`the line after the rejoin has ${line.left} s left; ${waitedS} s had passed of ${first.limit}`);
  const stray = after.filter((x) => x.id && x.id !== contract);
  if (stray.length) fails.push(`after the rejoin the client was sent a line for ${stray.map((x) => x.id).join(", ")}, which is not the running contract`);
  return fails;
}

export function selftestTrackerRejoin() {
  const f = [];
  const ok = { id: CONTRACT, limit: 600, left: 600 };
  if (judgeTrackerRejoin([ok], [{ ...ok, left: 560 }], CONTRACT, 30).length) f.push("a resent line with less time was failed");
  if (!judgeTrackerRejoin([ok], [], CONTRACT, 30).length) f.push("no line after the rejoin passed");
  if (!judgeTrackerRejoin([ok], [ok], CONTRACT, 30).length) f.push("a full-limit line after the rejoin passed");
  if (!judgeTrackerRejoin([ok], [{ id: "other", limit: 60, left: 50 }, { ...ok, left: 560 }], CONTRACT, 30).length) f.push("a line for another contract passed");
  if (receivedLines("x [LFDebug:Reputation] contract line received: harbour_crates limit=600 left=555\n[LFDebug:Reputation] contract line received: cleared")[1]?.cleared !== true) f.push("a cleared line was not read");
  return f;
}

async function runTrackerRejoinProof() {
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${r.detail.slice(0, 200)}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const readLog = (logsDir) => {
    const f = newestLog(logsDir);
    return f ? readFileSync(f, "utf8") : "";
  };
  const serverLog = () => readLog(join(sessionPaths(dir).server, "logs"));
  const clientLog = () => readLog(sessionPaths(dir).clients[0].logs);
  const scene = () => on(1, { verb: "dialogue" }, 10000).detail;
  const fails = [];
  const untilChoosing = () => {
    let state = "";
    for (let i = 0; i < 40; i++) {
      state = scene();
      if (/phase=choosing/.test(state) || state.startsWith("closed")) return state;
      if (/phase=line/.test(state) && i >= 2) on(1, { verb: "input", action: "MenuSelect" });
      wait(700);
    }
    return state;
  };

  // 1. Take the contract.
  arrive(on, 1, wait);
  on(1, { verb: "input", action: "MenuBack" });
  wait(6000);
  on(1, { verb: "resetplayer", cash: 1000 }, 10000);
  wait(1500);
  on(1, { verb: "repgrant", words: ["TOWN", "100"] }, 10000);
  wait(1500);
  const grant = /harness standing (\S+) TOWN points=(\d+) tier=(-?\d+)/.exec(serverLog());
  const uid = grant ? grant[1] : "";
  if (!uid || grant[3] !== "1") {
    console.log("FAIL  the character is not Known (the repgrant verb did not reach the server)");
    return 1;
  }
  // Marcel is a resident (#4466, #4646): he spawns on his timetable when a player is within 150 m of the harbour place, so the
  // player goes to the place marker first and the clock is set inside his 06:00 to 18:00 day.
  on(1, { verb: "stand", entityName: MARCEL_PLACE, side: "s" }, 20000);
  wait(2000);
  on(1, { verb: "time", hour: 10.0 }, 15000);
  wait(10000);
  on(1, { verb: "stand", entityName: MARCEL, side: "s" }, 10000);
  wait(2500);
  on(1, { verb: "use", className: TALK, entityName: MARCEL }, 10000);
  wait(800);
  untilChoosing();
  wait(1000);
  on(1, { verb: "input", action: "MenuDown" });
  wait(900);
  on(1, { verb: "input", action: "MenuSelect" });
  wait(1200);
  untilChoosing();
  const mark = serverLog().length;
  on(1, { verb: "input", action: "MenuSelect" });
  wait(3000);
  if (!new RegExp(`contract ${CONTRACT} taken by ${uid}`).test(serverLog().slice(mark))) {
    console.log(`FAIL  the ${CONTRACT} contract was not taken (no 'contract ${CONTRACT} taken by ${uid}' line)`);
    return 1;
  }
  on(1, { verb: "input", action: "MenuBack" });
  wait(1500);
  const before = receivedLines(clientLog());
  console.log(`contract taken; the client logged ${before.length} line(s): ${JSON.stringify(before)}`);

  // 2. Wait, then really rejoin.
  console.log(`waiting ${WAIT_BEFORE_REJOIN_S} s with the contract running`);
  wait(WAIT_BEFORE_REJOIN_S * 1000);
  const s0 = readSession(dir)?.clients?.find((c) => c.n === 1)?.identity;
  const r = spawnSync(process.execPath, [join(ROOT, "tools", "cli.mjs"), "session", "rejoin", "1"], { encoding: "utf8", windowsHide: true, env: { ...process.env, LF_SESSION_ROUTED: "1" }, timeout: 420000 });
  process.stdout.write(r.stdout ?? "");
  if (r.status !== 0) fails.push(`session rejoin 1 exited ${r.status}`);
  const now = readSession(dir)?.clients?.find((c) => c.n === 1)?.identity;
  if (s0 && now && now.playerId === s0.playerId) fails.push(`client 1 kept player id ${now.playerId}: it did not really rejoin`);

  // 3. The new client process logs the line again. A returning account spawns without the registration form; the
  // server re-sends on the spawn event (with one retry), so give it time.
  let after = [];
  let text = "";
  for (let i = 0; i < 30; i++) {
    text = clientLog();
    after = receivedLines(text);
    if (after.some((x) => x.id === CONTRACT)) break;
    wait(2000);
  }
  console.log(`after the rejoin the client logged: ${JSON.stringify(after)}`);
  fails.push(...judgeTrackerRejoin(before, after, CONTRACT, WAIT_BEFORE_REJOIN_S));
  const sent = /contract line sent to player (\d+) \(spawn\): '([^']*)'/g;
  let last = null;
  for (let m; (m = sent.exec(serverLog())); ) last = m;
  console.log(last ? `server: re-sent to player ${last[1]}: '${last[2]}'` : "server: no 'contract line sent ... (spawn)' line");
  if (!last) fails.push("the server never logged a re-send on spawn");

  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "tracker rejoin proof: FAIL" : "tracker rejoin proof: PASS (the rejoined client logged the contract line again with the time left)");
  return fails.length ? 1 : 0;
}

export default {
  name: "tracker-rejoin-proof",
  usage: "tracker-rejoin-proof   (session up --mission island; one client takes harbour_crates, rejoins after 30 s)",
  run: () => runTrackerRejoinProof(),
};
