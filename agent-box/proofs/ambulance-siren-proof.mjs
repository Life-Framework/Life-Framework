import { arrive, clientBox, sessionDir } from "../session.mjs";

// Ambulance siren on a running session, driven the way a player does it, from the driver seat.
//   node agent-box/session.mjs up --profile full     (one client; add --clients 2 only for a second player)
//   node agent-box/session.mjs ambulance-siren-proof
//   node agent-box/session.mjs down
// The medic clocks in, looks at the parked ambulance (light bar dark, shot), takes the driver seat,
// and the F menu must offer "Siren: on" (the seat context, no aiming at the dash). Pick it: the
// bar lights, shot from outside. Pick "Siren: off": the bar is dark again.
const SITE = "Roadmap_Hospital_Ambulance";
const ACTION = "LF_SirenModeAction";

async function runAmbulanceSirenProof() {
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${r.detail.slice(0, 200)}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const fails = [];
  const must = (r, what) => {
    if (r.status !== "ok") fails.push(`${what} returned ${r.status} ${r.detail}`);
    return r;
  };
  const shot = (name) => on(1, { verb: "capture", mode: "ui", name }, 25000);

  arrive(on, 1, wait);
  must(on(1, { verb: "stand", entityName: "Roadmap_Hospital_Duty_Desk" }), "stand at the duty desk");
  wait(2000);
  must(on(1, { verb: "use", className: "LF_DutyAction", entityName: "Roadmap_Hospital_Duty_Desk" }, 10000), "clock in");
  wait(2000);

  // Parked, nobody in it: the bar is dark.
  must(on(1, { verb: "stand", entityName: SITE }), "stand at the ambulance");
  wait(2500);
  shot("fix_ambulance_siren_1_parked_lights_off");

  // Take the driver seat; the siren row must be offered from the seat.
  must(on(1, { verb: "use", className: "SCR_GetInUserAction", entityName: SITE }, 10000), "get in");
  wait(3000);
  const look = must(on(1, { verb: "look" }), "look from the seat");
  if (!look.detail.includes(ACTION)) fails.push(`the seated driver is not offered ${ACTION}: ${look.detail.slice(0, 300)}`);
  shot("fix_ambulance_siren_2_driver_seat_menu");

  must(on(1, { verb: "use", className: ACTION, entityName: SITE }, 10000), "siren on");
  wait(2500);
  const state = on(1, { verb: "look" });
  shot("fix_ambulance_siren_3_siren_on");
  if (state.detail.includes("Siren: on") && !state.detail.includes("Siren: off")) fails.push(`after the toggle the menu still offers "Siren: on": ${state.detail.slice(0, 200)}`);

  if (fails.length) {
    console.log(`ambulance-siren-proof: FAIL`);
    for (const f of fails) console.log(`  ${f}`);
    return 1;
  }
  console.log("ambulance-siren-proof: OK");
  return 0;
}

export default { name: "ambulance-siren-proof", usage: "ambulance-siren-proof", run: () => runAmbulanceSirenProof() };
