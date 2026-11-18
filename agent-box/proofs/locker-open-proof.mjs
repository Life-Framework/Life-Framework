import { readFileSync } from "node:fs";
import { join } from "node:path";
import { arrive, clientBox, newestLog, sessionDir, sessionPaths } from "../session.mjs";

// "Open your locker" on a DEDICATED client (#3369). The server spawns the contents container and, in the
// same call, sends its RplId to the renter's client; the client resolves it once. If the container has not
// streamed in yet the client logs "locker open dropped on the client" and the locker never shows. A listen
// host never sees it, so this runs on a dedicated server with one remote client.
//   tools\cli session up --mission grid --clients 1
//   node agent-box/session.mjs locker-open-proof [cycles]
//   tools\cli session down
// The grid cell slice-3369 holds one locker bank (Grid_slice_3369_R0). The client rents a locker, then opens
// it `cycles` times (default 10). Each open must log "opened the locker" on the server and no "locker open
// dropped" line on the client. The proof reads the logs, not the verb status: `use` returns once the action
// ran on the server, whether or not the client drew anything.
const BANK = "Grid_slice_3369_R0";

async function runLockerOpenProof(ctx) {
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const cycles = Number(ctx?.args?.[0] ?? 10);
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${r.detail.slice(0, 200)}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const paths = sessionPaths(dir);
  const readNewest = (logs) => {
    const f = newestLog(logs);
    return f ? readFileSync(f, "utf8") : "";
  };
  const serverLog = () => readNewest(join(paths.server, "logs"));
  const clientLog = () => readNewest(paths.clients[0].logs);
  const count = (text, re) => (text.match(re) || []).length;
  const results = [];
  const check = (name, pass, detail) => {
    results.push({ name, pass, detail });
    console.log(`${pass ? "PASS" : "FAIL"}  ${name}: ${detail}`);
  };

  arrive(on, 1, wait);
  wait(8000);
  on(1, { verb: "stand", entityName: BANK });
  wait(2500);

  const rentedBefore = count(serverLog(), /rented a locker at/g);
  const rent = on(1, { verb: "use", className: "LF_LockerRentAction", entityName: BANK }, 10000);
  wait(2000);
  const rented = count(serverLog(), /rented a locker at/g) - rentedBefore;
  const alreadyRenting = count(serverLog(), /already rents a locker here/g);
  check("the client holds a locker", rent.status === "ok" && (rented === 1 || alreadyRenting > 0), `use=${rent.status} rented=${rented} already=${alreadyRenting}`);

  const droppedBefore = count(clientLog(), /locker open dropped on the client/g);
  const openedBefore = count(serverLog(), /opened (the|an empty) locker at/g);
  for (let i = 1; i <= cycles; i++) {
    let r = on(1, { verb: "use", className: "LF_LockerOpenAction", entityName: BANK }, 10000);
    // The first command after a rent can come back no_session before the client settles: send it again.
    if (r.status !== "ok") {
      console.log(`cycle ${i}: use returned ${r.status} ${r.detail.slice(0, 80)}; sending it once more`);
      wait(1500);
      r = on(1, { verb: "use", className: "LF_LockerOpenAction", entityName: BANK }, 10000);
    }
    wait(2500);
  }
  const opened = count(serverLog(), /opened (the|an empty) locker at/g) - openedBefore;
  const dropped = count(clientLog(), /locker open dropped on the client/g) - droppedBefore;
  check("the server opened the locker every cycle", opened === cycles, `${opened} of ${cycles}`);
  check("the client dropped none of them", dropped === 0, `${dropped} "locker open dropped" line(s) on the client in ${cycles} cycles`);

  const failed = results.filter((r) => !r.pass);
  console.log(failed.length ? `locker-open-proof: FAIL (${failed.length} of ${results.length})` : `locker-open-proof: PASS (${results.length} checks, ${cycles} cycles)`);
  return failed.length ? 1 : 0;
}

export default {
  name: "locker-open-proof",
  usage: "locker-open-proof [cycles]   (up --mission grid --clients 1)",
  run: (ctx) => runLockerOpenProof(ctx),
};
