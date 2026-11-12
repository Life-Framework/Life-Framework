import { readFileSync } from "node:fs";
import { join } from "node:path";
import { arrive, clientBox, newestLog, parseState, sessionDir, sessionPaths } from "../session.mjs";

// M2 #419 on a running `up --clients 2` session (write the roster for client 2 before `up`): client 2
// clocks in as an officer, client 1 is handed a contraband crate (dev gate) and is searched at the
// duty desk until the server finds it (the find is a roll, up to 6 tries). The server log must show
// `person search seized` and a `crime report ... Carrying contraband goods` line for client 1; a
// second search then finds nothing. Shots of both clients.
async function runSearchProof() {
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${r.detail}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const serverLog = () => {
    const f = newestLog(join(sessionPaths(dir).server, "logs"));
    return f ? readFileSync(f, "utf8") : "";
  };
  const count = (re) => (serverLog().match(re) || []).length;
  const fails = [];
  for (const n of [1, 2]) arrive(on, n, wait);
  wait(8000);
  on(2, { verb: "stand", entityName: "Roadmap_Police_Desk" });
  wait(2500);
  on(2, { verb: "look" });
  const clockIn = on(2, { verb: "use", className: "LF_DutyAction", entityName: "Roadmap_Police_Desk" }, 10000);
  wait(3000);
  const other = `Player${parseState(on(1, { verb: "state" }, 10000).detail).player}`;
  on(2, { verb: "stand", entityName: other });
  wait(2500);
  on(2, { verb: "look" });
  const cleanBefore = count(/person search found nothing/g);
  on(2, { verb: "use", className: "LF_ConfiscateAction", entityName: other }, 10000);
  wait(1500);
  const clean = count(/person search found nothing/g) - cleanBefore;
  const give = on(1, { verb: "crate" }, 10000);
  wait(1500);
  on(1, { verb: "capture", mode: "ui", name: "m2_419_carrying_client1" }, 25000);
  let seized = 0;
  for (let i = 0; i < 6 && !seized; i++) {
    // Each search starts the per-person cooldown; the proof is about the roll, so end it first.
    on(1, { verb: "resetcrime" }, 10000);
    on(2, { verb: "use", className: "LF_ConfiscateAction", entityName: other }, 10000);
    wait(1500);
    seized = count(/person search seized/g);
  }
  on(2, { verb: "capture", mode: "ui", name: "m2_419_found_client2" }, 25000);
  const reports = count(/Carrying contraband goods|Offence_ChainGoods/g);
  const againBefore = count(/person search found nothing/g);
  on(1, { verb: "resetcrime" }, 10000);
  on(2, { verb: "use", className: "LF_ConfiscateAction", entityName: other }, 10000);
  wait(1500);
  const again = count(/person search found nothing/g) - againBefore;
  if (clockIn.status !== "ok") fails.push(`clock in returned ${clockIn.status} ${clockIn.detail}`);
  if (clean < 1) fails.push("the first search of a clean player did not log 'found nothing'");
  if (give.status !== "ok" || !give.detail.includes("added=1")) fails.push(`crate not given: ${give.status} ${give.detail}`);
  if (!seized) fails.push("six searches never seized the crate");
  if (!reports) fails.push("no chain-goods crime report in the server log");
  if (again < 1) fails.push("the search after the seizure still found something");
  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "search proof: FAIL" : "search proof: PASS (clean search found nothing, crate found and seized, offence reported, nothing left to find)");
  return fails.length ? 1 : 0;
}

export default {
  name: "search-proof",
  usage: "search-proof",
  run: (ctx) => runSearchProof(),
};
