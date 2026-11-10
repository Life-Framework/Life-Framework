import { ROB_TARGET, arrive, clientBox, sessionDir } from "../session.mjs";

// M1-11 on a running `up --clients 2` session: client 2 (on the roster) clocks in at
// the duty desk, client 1 robs the fuel till, client 2 reads the wanted board (police
// view), client 1 reads it too (public view), client 2 clocks out. Prints state lines;
// the poster lines land in each client's log (read them after `down`).
async function runPoliceProof() {
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${r.detail}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const state = (n) => on(n, { verb: "state" }, 10000).detail;
  const fails = [];

  for (const n of [1, 2]) arrive(on, n, wait);
  wait(8000);
  const start1 = state(1);
  const start2 = state(2);

  on(2, { verb: "stand", entityName: "Roadmap_Police_Desk" });
  wait(2500);
  on(2, { verb: "look" });
  const clockIn = on(2, { verb: "use", className: "LF_DutyAction", entityName: "Roadmap_Police_Desk" }, 10000);
  wait(2500);
  const onDuty2 = state(2);
  if (clockIn.status !== "ok") fails.push(`clock in returned ${clockIn.status} ${clockIn.detail}`);
  if (!onDuty2.includes("faction=POLICE") || !onDuty2.includes("duty=1")) fails.push(`client 2 after clock-in: ${onDuty2}`);

  on(1, { verb: "stand", entityName: ROB_TARGET });
  wait(2500);
  on(1, { verb: "look" });
  const rob = on(1, { verb: "use", className: "LF_RobAction", entityName: ROB_TARGET }, 10000);
  wait(2500);
  const robbed1 = state(1);
  if (rob.status !== "ok") fails.push(`rob returned ${rob.status} ${rob.detail}`);
  if (!robbed1.includes("wanted=1")) fails.push(`client 1 after the robbery: ${robbed1}`);

  for (const n of [2, 1]) {
    on(n, { verb: "stand", entityName: "Roadmap_Police_WantedBoard" });
    wait(2500);
    on(n, { verb: "look" });
    const read = on(n, { verb: "use", className: "LF_WantedBoardReadAction", entityName: "Roadmap_Police_WantedBoard" }, 10000);
    if (read.status !== "ok") fails.push(`client ${n} board read returned ${read.status} ${read.detail}`);
    wait(2500);
    on(n, { verb: "capture", mode: "ui", name: `m1_11_board_client${n}` }, 25000);
    on(n, { verb: "ui" });
    on(n, { verb: "click", widget: "Close" });
    wait(800);
  }

  on(2, { verb: "stand", entityName: "Roadmap_Police_Desk" });
  wait(2500);
  on(2, { verb: "look" });
  const clockOut = on(2, { verb: "use", className: "LF_DutyAction", entityName: "Roadmap_Police_Desk" }, 10000);
  wait(2500);
  const offDuty2 = state(2);
  if (clockOut.status !== "ok") fails.push(`clock out returned ${clockOut.status} ${clockOut.detail}`);
  if (!offDuty2.includes("faction=CIVILIAN") || !offDuty2.includes("duty=0")) fails.push(`client 2 after clock-out: ${offDuty2}`);

  console.log("");
  console.log(`start      client 1: ${start1}`);
  console.log(`start      client 2: ${start2}`);
  console.log(`clocked in client 2: ${onDuty2}`);
  console.log(`robbed     client 1: ${robbed1}`);
  console.log(`clock out  client 2: ${offDuty2}`);
  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "police proof: FAIL" : "police proof: PASS (clock in, robbery, board read by police and public, clock out)");
  return fails.length ? 1 : 0;
}

export default {
  name: "police-proof",
  usage: "police-proof",
  film: true,
  run: (ctx) => runPoliceProof(),
};
