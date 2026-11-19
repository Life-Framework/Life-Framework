import { existsSync, readdirSync, readFileSync, statSync } from "node:fs";
import { join } from "node:path";
import { arrive, clientBox, parseState, sessionDir } from "../session.mjs";
import { armAndDraw } from "./fuel-robbery-proof.mjs";

// #3404 on a running `up --mission grid --clients 2` session: the crew till robbery on a dedicated
// server with two real players at LF_CrewTill in the grid cell crew-till. Client 1 alone is refused,
// then both armed start the job (both wanted), client 2 cannot hold, client 1 holds 30 s and takes:
// each player is paid an even share once (cash, or marked notes when dirty money is on, which the
// state verb does not show, so the shares are read from the server log); a second take pays nothing.
// The server log line `crew job ends: take N, paid N among 2 present of 2` is the payout record.
const CELL = "grid:crew-till";
const TILL = "Grid_crew_till_R0";
// LF_RateLimit CRIME lets two presses in ten seconds; every press waits this long after the last.
const PRESS_GAP_MS = 6000;

// The server's console.log of this session (the newest log folder).
function serverLog(dir) {
  const logs = join(dir, "server", "logs");
  if (!existsSync(logs)) return "";
  const newest = readdirSync(logs).map((d) => join(logs, d)).sort((a, b) => statSync(b).mtimeMs - statSync(a).mtimeMs)[0];
  const file = newest && join(newest, "console.log");
  return file && existsSync(file) ? readFileSync(file, "utf8") : "";
}

async function runCrewRobProof() {
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${r.detail}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const state = (n) => parseState(on(n, { verb: "state" }, 10000).detail);
  const press = (n, className) => {
    const r = on(n, { verb: "use", className, entityName: TILL }, 10000);
    wait(PRESS_GAP_MS);
    return r;
  };
  const logStart = serverLog(dir).length; // a rerun on the same session reads only its own lines
  const fails = [];
  const expect = (cond, what) => { if (!cond) fails.push(what); };

  for (const n of [1, 2]) arrive(on, n, wait);
  wait(8000);
  for (const n of [1, 2]) {
    on(n, { verb: "resetcrime" }, 10000);
    on(n, { verb: "teleport", words: [CELL] }, 10000);
  }
  wait(3000);
  for (const n of [1, 2]) armAndDraw(on, n, wait);

  // Alone: client 2 is away, client 1 is refused and nobody is wanted.
  on(2, { verb: "teleport", words: ["grid:atm"] }, 10000);
  on(1, { verb: "stand", entityName: TILL, side: "w" });
  wait(2500);
  press(1, "LF_CrewRobStartAction");
  expect(Number(state(1).wanted) === 0, "a lone start books no wanted level");

  // Both at the till: the job starts and both are wanted.
  on(2, { verb: "teleport", words: [CELL] }, 10000);
  wait(2000);
  on(2, { verb: "stand", entityName: TILL, side: "s" });
  wait(PRESS_GAP_MS);
  const before = { 1: state(1), 2: state(2) };
  press(1, "LF_CrewRobStartAction");
  const wanted = { 1: state(1), 2: state(2) };
  expect(Number(wanted[1].wanted) > 0, `the holder is wanted after the start (got ${wanted[1].wanted})`);
  expect(Number(wanted[2].wanted) > 0, `the lookout is wanted after the start (got ${wanted[2].wanted})`);

  // The lookout cannot hold; the holder holds 30 s and takes.
  press(2, "LF_CrewRobHoldAction");
  press(1, "LF_CrewRobHoldAction");
  wait(30000 - PRESS_GAP_MS);
  press(1, "LF_CrewRobTakeAction");
  const paid = { 1: state(1), 2: state(2) };
  const log = serverLog(dir).slice(logStart);
  const ends = [...log.matchAll(/crew job ends: take (\d+), paid (\d+) among (\d+) present of (\d+)/g)];
  const end = ends[ends.length - 1];
  expect(ends.length === 1, `one crew job paid out (got ${ends.length})`);
  expect(end && end[1] === end[2] && Number(end[1]) > 0 && end[3] === "2" && end[4] === "2", `the whole take is paid to both (got ${end ? end[0] : "no payout line"})`);
  const shares = [...log.matchAll(/robbery \+(\d+) (?:marked notes|cash) to player/g)].map((m) => Number(m[1]));
  expect(shares.length === 2 && Math.abs(shares[0] - shares[1]) <= 1, `two even shares (got ${shares.join(", ")})`);
  const got = (n) => shares[n - 1];

  // Once: a second take pays nothing.
  press(1, "LF_CrewRobTakeAction");
  const after = { 1: state(1), 2: state(2) };
  const again = [...serverLog(dir).slice(logStart).matchAll(/crew job ends: take/g)].length;
  expect(again === ends.length, "a second take writes no second payout");
  expect(after[1].cash === paid[1].cash && after[2].cash === paid[2].cash, `a second take pays nothing (${paid[1].cash}/${paid[2].cash} -> ${after[1].cash}/${after[2].cash})`);

  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "crew rob proof: FAIL" : `crew rob proof: PASS (shares ${got(1)} and ${got(2)}; grep the server log for 'crew job ends')`);
  return fails.length ? 1 : 0;
}

export default {
  name: "crew-rob-proof",
  usage: "crew-rob-proof   (up --mission grid --clients 2)",
  run: () => runCrewRobProof(),
};
