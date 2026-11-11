import { readFileSync } from "node:fs";
import { join } from "node:path";
import { arrive, clientBox, newestLog, sessionDir, sessionPaths } from "../session.mjs";

// #600 on a running `up --clients 2` session (write the roster for client 2 before `up`): client 2
// clocks in as an officer at the duty desk, walks to the back room (Shop_BlackMarket_Debug) and
// searches it. The server log must show `back room 'X' searched` with the stock seized and a
// `back room seizure` locker line; a second search is refused (`already shut`). Client 1 (a
// civilian) only arrives. Shots are of the officer's prompt and the toast.
async function runBackRoomPoliceProof() {
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
  // The roster names client 2, but a remote session may not carry it: on a fresh save the first
  // player to join is the officer. Whoever the duty desk accepts is the officer.
  let cop = 2;
  let clockIn;
  for (const n of [2, 1]) {
    on(n, { verb: "stand", entityName: "Roadmap_Police_Desk" });
    wait(2500);
    on(n, { verb: "look" });
    clockIn = on(n, { verb: "use", className: "LF_DutyAction", entityName: "Roadmap_Police_Desk" }, 10000);
    cop = n;
    if (clockIn.status === "ok") break;
  }
  wait(3000);
  on(cop, { verb: "stand", entityName: "Shop_BlackMarket_Debug" });
  wait(2500);
  on(cop, { verb: "look" });
  on(cop, { verb: "capture", mode: "ui", name: "m2_600_cop_at_backroom" }, 25000);
  const search = on(cop, { verb: "use", className: "LF_BackRoomSearchAction", entityName: "Shop_BlackMarket_Debug" }, 10000);
  wait(1500);
  on(cop, { verb: "capture", mode: "ui", name: "m2_600_cop_seized" }, 25000);
  const raided = count(/back room '.*' searched by/g);
  const logged = count(/back room seizure #\d+ logged/g);
  on(cop, { verb: "use", className: "LF_BackRoomSearchAction", entityName: "Shop_BlackMarket_Debug" }, 10000);
  wait(1500);
  const shut = count(/back room search refused: ALREADY_SHUT/g);
  if (clockIn.status !== "ok") fails.push(`clock in returned ${clockIn.status} ${clockIn.detail}`);
  if (search.status !== "ok") fails.push(`search returned ${search.status} ${search.detail}`);
  if (!raided) fails.push("no 'back room ... searched by' line in the server log");
  if (!logged) fails.push("no 'back room seizure' locker line in the server log");
  if (!shut) fails.push("the second search was not refused as already shut");
  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "backroom police proof: FAIL" : "backroom police proof: PASS (officer searched the back room, stock logged as evidence, second search refused)");
  return fails.length ? 1 : 0;
}

export default {
  name: "backroom-police-proof",
  usage: "backroom-police-proof",
  run: () => runBackRoomPoliceProof(),
};
