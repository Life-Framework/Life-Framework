import { existsSync, readFileSync } from "node:fs";
import { join } from "node:path";
import { ROB_TARGET, arrive, clientBox, sessionDir, newestLog, sessionPaths, sessionUid } from "../session.mjs";

// #564: the proof needs client 2 on the police roster, which the server reads on first use. A run
// without `session.mjs roster 2` before `up` (the `session measure` variants) used to clock client 2
// in as a civilian and fail on a radio that was never on 41000. Say so up front instead.
// Client n's newest console log as text ("" when none).
export function clientLog(dir, n) {
  const f = newestLog(sessionPaths(dir, n).clients[n - 1].logs);
  return f ? readFileSync(f, "utf8") : "";
}

export function rosterHas(dir, playerId) {
  const f = join(sessionPaths(dir).server, "profile", "LifeFramework", "whitelist.txt");
  return existsSync(f) && readFileSync(f, "utf8").includes(sessionUid(playerId));
}

// M2 #253 on a running `up --clients 2` session with client 2 on the roster (`session.mjs roster 2`
// before up): client 2 clocks in (its radio reads the police frequency); client 1, a civilian, is
// handed a scanner (dev gate; the fence path is covered by tests) and robs the till; the dispatch
// reaches the officer's radio chirp and the civilian's scanner. The server log carries
// `[ELDebug:Scanner] ... heard on a scanner` for client 1; shots of both clients.
async function runScannerProof() {
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${r.detail}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const fails = [];
  const mark = clientLog(dir, 1).length;
  if (!rosterHas(dir, 2)) {
    console.log("scanner proof: FAIL  client 2 is not on the police roster; run `session.mjs down`, `session.mjs roster 2`, then `up --clients 2` again");
    return 1;
  }
  for (const n of [1, 2]) arrive(on, n, wait);
  wait(8000);
  on(2, { verb: "stand", entityName: "Roadmap_Police_Desk" });
  wait(2500);
  on(2, { verb: "look" });
  const clockIn = on(2, { verb: "use", className: "LF_DutyAction", entityName: "Roadmap_Police_Desk" }, 10000);
  wait(3000);
  const officerRadio = on(2, { verb: "radio" }, 10000).detail;
  const give = on(1, { verb: "scanner" }, 10000);
  wait(1500);
  on(1, { verb: "stand", entityName: ROB_TARGET });
  wait(2500);
  on(1, { verb: "look" });
  const rob = on(1, { verb: "use", className: "LF_RobAction", entityName: ROB_TARGET }, 10000);
  wait(1000);
  // Client side (#522): the civilian's own client received the scanner toast, not just the server log.
  on(1, { verb: "capture", mode: "ui", name: "m2_253_scanner_client1" }, 25000);
  on(2, { verb: "capture", mode: "ui", name: "m2_253_dispatch_client2" }, 25000);
  if (clockIn.status !== "ok") fails.push(`clock in returned ${clockIn.status} ${clockIn.detail}`);
  if (give.status !== "ok" || !give.detail.includes("added=1")) fails.push(`scanner not given: ${give.status} ${give.detail}`);
  if (rob.status !== "ok") fails.push(`rob returned ${rob.status} ${rob.detail}`);
  if (!officerRadio.includes("f=41000")) fails.push(`the officer radio is not on 41000: ${officerRadio}`);
  if (!clientLog(dir, 1).slice(mark).includes("toast received: #LF-Scanner_Heard")) fails.push("client 1 never received the scanner toast (client log)");
  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "scanner proof: FAIL" : "scanner proof: PASS (officer radio on 41000, scanner given, till robbed; confirm the 'heard on a scanner' server log line)");
  return fails.length ? 1 : 0;
}

export default {
  name: "scanner-proof",
  usage: "scanner-proof",
  run: (ctx) => runScannerProof(),
};
