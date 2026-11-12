import { MEDIC_RADIO_KHZ, arrive, clientBox, clientSees, judgeRadio, sessionDir } from "../session.mjs";

// M2 #258 on a running `up --clients 2` session: client 2 clocks in at the hospital desk and
// is a medic on duty (the medic kit, the service routing); the hospital board, closed to the
// public, opens for them; clock-out returns the civilian. Screenshots: the desk, the clock-in,
// the open board, the public's closed board.
// Client side (#522): client 2 reads its own replicated account state (MEDIC, on duty, then CIVILIAN) and
// its own carried radio on the medic frequency; client 1 reads its radio too; the board is judged by
// whether its Close button is on the client's screen.
async function runMedicProof() {
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
  const boards = {};
  const desk = (n) => {
    on(n, { verb: "stand", entityName: "Roadmap_Hospital_Duty_Desk" });
    wait(2500);
    on(n, { verb: "look" });
    const r = on(n, { verb: "use", className: "LF_DutyAction", entityName: "Roadmap_Hospital_Duty_Desk" }, 10000);
    wait(2500);
    return r;
  };
  const board = (n, name) => {
    on(n, { verb: "stand", entityName: "Roadmap_Hospital_DispatchBoard" });
    wait(2500);
    on(n, { verb: "look" });
    const r = on(n, { verb: "use", className: "LF_WantedBoardReadAction", entityName: "Roadmap_Hospital_DispatchBoard" }, 10000);
    wait(2500);
    on(n, { verb: "capture", mode: "ui", name }, 25000);
    const screen = on(n, { verb: "ui" });
    boards[n] = screen.detail;
    on(n, { verb: "click", widget: "Close" });
    wait(800);
    return r;
  };

  for (const n of [1, 2]) arrive(on, n, wait);
  wait(8000);
  const start2 = state(2);
  board(1, "m2_258_board_public_client1");

  on(2, { verb: "stand", entityName: "Roadmap_Hospital_Duty_Desk" });
  wait(2500);
  on(2, { verb: "capture", mode: "world", name: "m2_258_desk", x: 259.4, y: 1.0, z: 256.4, ox: 0, oy: 1.2, oz: -4.5 }, 25000);
  const clockIn = desk(2);
  const onDuty = state(2);
  const medicSeen = clientSees(on, wait, 2, (s) => (s.faction === "MEDIC" && s.duty === "1" ? [] : [`account ${s.faction}/${s.duty}, expected MEDIC/1`]), { label: "after clock-in" });
  const medicRadio = on(2, { verb: "radio" }, 10000).detail;
  const civilianRadio = on(1, { verb: "radio" }, 10000).detail;
  for (const f of judgeRadio(medicRadio, civilianRadio)) fails.push(`client side: ${f}`);
  fails.push(...medicSeen.fails);
  on(2, { verb: "capture", mode: "ui", name: "m2_258_clocked_in_client2" }, 25000);
  if (clockIn.status !== "ok") fails.push(`clock in returned ${clockIn.status} ${clockIn.detail}`);
  if (!onDuty.includes("faction=MEDIC") || !onDuty.includes("duty=1")) fails.push(`client 2 after clock-in: ${onDuty}`);
  board(2, "m2_258_board_medic_client2");

  const clockOut = desk(2);
  const offDuty = state(2);
  const civilianSeen = clientSees(on, wait, 2, (s) => (s.faction === "CIVILIAN" && s.duty === "0" ? [] : [`account ${s.faction}/${s.duty}, expected CIVILIAN/0`]), { label: "after clock-out" });
  fails.push(...civilianSeen.fails);
  if (clockOut.status !== "ok") fails.push(`clock out returned ${clockOut.status} ${clockOut.detail}`);
  if (!offDuty.includes("faction=CIVILIAN") || !offDuty.includes("duty=0")) fails.push(`client 2 after clock-out: ${offDuty}`);

  console.log("");
  console.log(`start     client 2: ${start2}`);
  console.log(`clocked in client 2: ${onDuty}`);
  console.log(`client sees: ${medicSeen.line} | radio ${medicRadio} (want ${MEDIC_RADIO_KHZ}) | after clock-out ${civilianSeen.line}`);
  console.log(`client sees: public board screen ${boards[1]} | medic board screen ${boards[2]}`);
  console.log(`clock out client 2: ${offDuty}`);
  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "medic proof: FAIL" : "medic proof: PASS (clock in at the hospital desk, the medic board opens, clock out)");
  return fails.length ? 1 : 0;
}

export default {
  name: "medic-proof",
  usage: "medic-proof",
  run: (ctx) => runMedicProof(),
};
