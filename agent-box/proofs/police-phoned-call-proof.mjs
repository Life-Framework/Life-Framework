import { arrive, clientBox, sessionDir } from "../session.mjs";

// A player's 911 call taken and closed on the island (#4606), the way players do it: the officer (client 2,
// rostered) clocks in at the Saint-Philippe duty desk, the caller (client 1) phones the police from the payphone
// on the square, the officer takes the call at the dispatch board, walks to the parked police car by the pool bay and
// presses "Radio: call resolved" at its driver door, inside 50 m of the phone.
//   node agent-box/session.mjs up --mission island --clients 2 --profile full --roster 2 --wait
//   node agent-box/session.mjs police-phoned-call-proof
//   node agent-box/session.mjs down
// The server log carries the proof lines: "phoned in by" (PoliceCalls), "taken by", "closed resolved" (CallOutcome).
const DESK = "Fit_saint_philippe_police_police_kit_duty_desk"; // town-layout-ok: a session walk target; nothing asserts where it stands
const BOARD = "Fit_saint_philippe_police_police_kit_dispatch_board"; // town-layout-ok: a session walk target
const PARKED = "Fit_saint_philippe_police_cars_car_b32_1"; // town-layout-ok: the parked police car marker nearest the phone
const PHONE = "Fit_saint_philippe_districts_payphones_drop_b26"; // town-layout-ok: the payphone nearest the car pool bay
const CALLER = 1;
const OFFICER = 2;

async function runPolicePhonedCallProof() {
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${r.detail.slice(0, 300)}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const fails = [];
  const must = (r, what) => {
    if (r.status !== "ok") fails.push(`${what} returned ${r.status} ${r.detail}`);
    return r;
  };

  arrive(on, CALLER, wait);
  arrive(on, OFFICER, wait);
  wait(6000);

  // The officer clocks in (an application first when the roster seed needs one).
  must(on(OFFICER, { verb: "stand", entityName: DESK }), "the officer at the duty desk");
  wait(2500);
  if (on(OFFICER, { verb: "use", className: "LF_PoliceApplyAction", entityName: DESK }, 10000).status === "ok") {
    wait(1500);
    on(OFFICER, { verb: "type", widget: "PolAppName", text: "Bea_Officer" });
    on(OFFICER, { verb: "type", widget: "PolAppWhy", text: "Phoned_call_proof" });
    on(OFFICER, { verb: "type", widget: "PolAppAvailability", text: "Evenings" });
    on(OFFICER, { verb: "click", widget: "PolAppSubmit" });
    wait(3000);
  }
  must(on(OFFICER, { verb: "use", className: "LF_DutyAction", entityName: DESK }, 10000), "clock in");
  wait(3000);
  const state = on(OFFICER, { verb: "state" }, 10000).detail;
  if (!state.includes("duty=1")) fails.push(`the officer is not on duty: ${state}`);

  // The caller phones the police from the payphone.
  must(on(CALLER, { verb: "stand", entityName: PHONE }), "the caller at the payphone");
  wait(2500);
  must(on(CALLER, { verb: "use", className: "LF_PayphoneAction", entityName: PHONE }, 10000), "the caller dials the police");
  wait(2000);
  on(CALLER, { verb: "capture", mode: "ui", name: "phoned_call_payphone" }, 25000);

  // The officer takes it at the dispatch board, from the duty desk spot (the board hangs on the wall above it).
  must(on(OFFICER, { verb: "stand", entityName: DESK }), "the officer at the dispatch board");
  wait(2500);
  must(on(OFFICER, { verb: "use", className: "LF_PoliceCallBoardAction", entityName: BOARD }, 10000), "take a call");
  wait(1500);
  on(OFFICER, { verb: "capture", mode: "ui", name: "phoned_call_taken" }, 25000);

  // The parked police car by the pool bay (the island parks UAZ469 police cars there at world start), inside 50 m of
  // the phone. The S105 and S1203 carry the same rows (pack test everon/police-car-rows).
  must(on(OFFICER, { verb: "stand", entityName: PARKED }), "the officer at the parked police car");
  wait(3000);
  on(OFFICER, { verb: "look" });
  on(OFFICER, { verb: "capture", mode: "ui", name: "phoned_call_car" }, 25000);
  must(on(OFFICER, { verb: "use", className: "LF_CallCloseAction" }, 10000), "press 'Radio: call resolved' at the police car");
  wait(1500);
  on(OFFICER, { verb: "capture", mode: "ui", name: "phoned_call_closed" }, 25000);

  if (fails.length) {
    console.log("police-phoned-call-proof: FAIL");
    for (const f of fails) console.log(`  ${f}`);
    return 1;
  }
  console.log("police-phoned-call-proof: OK (phoned in, taken at the board, closed from the car radio; read the server log lines)");
  return 0;
}

export default { name: "police-phoned-call-proof", usage: "police-phoned-call-proof", run: () => runPolicePhonedCallProof() };
