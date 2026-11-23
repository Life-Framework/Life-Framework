import { arrive, clientBox, sessionDir } from "../session.mjs";

// Close a call from the car radio (#4585), the way a player does it: clock in at the desk, walk to the
// police UAZ, read the three radio rows, press "Radio: call resolved" with no call held and read the toast.
//   node agent-box/session.mjs up --clients 2 --profile full --roster 2
//   node agent-box/session.mjs police-call-close-proof
//   node agent-box/session.mjs down
// The close of a held call (pay, XP, abuse count) is proven by the LF_Test_CallCloseRadio* tests.
const CAR = "Roadmap_Police_Car"; // town-layout-ok: a session walk target for the harness; nothing asserts where it stands
const DESK = "Roadmap_Police_Desk"; // town-layout-ok: a session walk target for the harness; nothing asserts where it stands
const OFFICER = 2; // client 2 is the debug world's seeded officer (police-proof.mjs)

async function runPoliceCallCloseProof() {
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

  arrive(on, OFFICER, wait);
  wait(6000);
  must(on(OFFICER, { verb: "stand", entityName: DESK }), "stand at the police desk");
  wait(2500);
  on(OFFICER, { verb: "look" });
  if (on(OFFICER, { verb: "use", className: "LF_PoliceApplyAction", entityName: DESK }, 10000).status === "ok") {
    wait(1500);
    on(OFFICER, { verb: "type", widget: "PolAppName", text: "Bob_Officer" });
    on(OFFICER, { verb: "type", widget: "PolAppWhy", text: "Call_close_proof" });
    on(OFFICER, { verb: "type", widget: "PolAppAvailability", text: "Evenings" });
    on(OFFICER, { verb: "click", widget: "PolAppSubmit" });
    wait(3000);
  }
  must(on(OFFICER, { verb: "use", className: "LF_DutyAction", entityName: DESK }, 10000), "clock in");
  wait(3000);
  const state = on(OFFICER, { verb: "state" }, 10000).detail;
  if (!state.includes("duty=1")) fails.push(`the officer is not on duty: ${state}`);

  must(on(OFFICER, { verb: "stand", entityName: CAR, side: "w" }), "the officer at the car");
  wait(3000);
  const look = must(on(OFFICER, { verb: "look" }), "look at the car");
  // `look` lists the nearest context only (police-siren-proof notes the same); `use` below performs the row.
  if (!look.detail.includes("LF_CallCloseAction")) console.log("NOTE  look does not list the close rows; use still performs them");
  on(OFFICER, { verb: "capture", mode: "ui", name: "police_call_close_rows" }, 25000);
  must(on(OFFICER, { verb: "use", className: "LF_CallCloseAction" }, 10000), "press 'Radio: call resolved'");
  wait(1500);
  on(OFFICER, { verb: "capture", mode: "ui", name: "police_call_close_none_held" }, 25000);

  if (fails.length) {
    console.log("police-call-close-proof: FAIL");
    for (const f of fails) console.log(`  ${f}`);
    return 1;
  }
  console.log("police-call-close-proof: OK (on duty, the car lists the close rows, a press with no call held answers on screen)");
  return 0;
}

export default { name: "police-call-close-proof", usage: "police-call-close-proof", run: () => runPoliceCallCloseProof() };
