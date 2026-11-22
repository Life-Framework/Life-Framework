import { ROB_TARGET, arrive, clientBox, clientSeesPeer, parseState, sessionDir } from "../session.mjs";

// #2494: the police menu had blank Search and Arrest buttons (removed, #2509/#2535). The real police
// path is the player interaction menu on the suspect: Search (LF_ConfiscateAction) and Arrest
// (LF_JailArrestAction). Client 1 robs the fuel till (wanted), client 2 clocks in, stands beside them,
// the menu is photographed with both entries, then Search and Arrest run and the suspect is in the cell.
// `roster 2` before `up --clients 2 --profile full`. `police-arrest-proof [--name <shot prefix>]`
// `--escort [--car <entity>]` (#3431) adds the car step before the arrest: the officer detains the suspect
// (LF_JailEscortArrestAction), puts them in the police car (LF_JailPutInVehicleAction), BOTH clients read the
// suspect seated, then takes them out (LF_JailTakeOutOfVehicleAction) and both read them out of the seat. On a
// dedicated server with a remote client this is the proof that the seat is the engine's, not a host-only effect.
async function runPoliceArrestProof() {
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const flag = (name, dflt) => { const i = process.argv.indexOf(name); return i > 0 ? process.argv[i + 1] : dflt; };
  const prefix = flag("--name", "m5_2494_police");
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${r.detail}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const fails = [];
  const must = (r, what) => {
    if (r.status !== "ok") fails.push(`${what} returned ${r.status} ${r.detail}`);
    return r;
  };

  for (const n of [1, 2]) arrive(on, n, wait);
  wait(8000);

  const DESK = "Roadmap_Police_Desk"; // town-layout-ok: playtest-town desk the session proofs clock in at
  on(2, { verb: "stand", entityName: DESK });
  wait(2500);
  on(2, { verb: "look" });
  must(on(2, { verb: "use", className: "LF_DutyAction", entityName: DESK }, 10000), "clock in");
  wait(2000);

  on(1, { verb: "stand", entityName: ROB_TARGET });
  wait(2500);
  on(1, { verb: "look" });
  must(on(1, { verb: "use", className: "LF_RobAction", entityName: ROB_TARGET }, 10000), "rob");
  wait(7000);
  const wanted = parseState(on(1, { verb: "state" }, 10000).detail).wanted;
  if (!(Number(wanted) > 0)) fails.push(`client 1 is not wanted (${wanted}); nothing to arrest`);

  on(2, { verb: "stand", entityName: "Player1" });
  wait(4000);
  const look = on(2, { verb: "look" }, 10000);
  wait(1500);
  console.log(`officer menu on the suspect: ${look.detail}`);
  // Booking (LF_JailArrestAction) is offered only at the jail since 2026-11-22: arrest-flow-proof walks it.
  for (const label of ["LF_ConfiscateAction"]) {
    if (!look.detail.includes(label)) fails.push(`the interaction menu does not offer ${label}`);
  }
  on(2, { verb: "capture", mode: "ui", name: `${prefix}_menu` }, 25000);

  must(on(2, { verb: "use", className: "LF_ConfiscateAction", entityName: "Player1" }, 10000), "search");
  wait(2500);
  on(2, { verb: "capture", mode: "ui", name: `${prefix}_search` }, 25000);

  if (process.argv.includes("--escort")) {
    const car = flag("--car", "Annex_Escort_Car");
    on(2, { verb: "look" });
    must(on(2, { verb: "use", className: "LF_JailEscortArrestAction", entityName: "Player1" }, 10000), "detain for escort");
    wait(1500);
    // Each takes a side of the car so the two capsules never overlap (#522).
    on(1, { verb: "stand", entityName: car, side: "s" });
    wait(2500);
    on(2, { verb: "stand", entityName: car, side: "n" });
    wait(2500);
    on(2, { verb: "look" });
    must(on(2, { verb: "use", className: "LF_JailPutInVehicleAction", entityName: car }, 10000), "put in car");
    for (const n of [1, 2]) {
      const r = clientSeesPeer(on, wait, n, 1, (p) => (p.seat === "1" ? [] : [`seat=${p.seat}`]), { label: "seated", budgetMs: 15000 });
      console.log(`client ${n}: suspect ${r.fails.length ? "not " : ""}seated after ${r.seconds.toFixed(1)} s`);
      for (const x of r.fails) fails.push(`put in car, ${x}`);
    }
    on(2, { verb: "capture", mode: "ui", name: `${prefix}_in_car` }, 25000);
    on(2, { verb: "look" });
    must(on(2, { verb: "use", className: "LF_JailTakeOutOfVehicleAction", entityName: car }, 10000), "take out of car");
    for (const n of [1, 2]) {
      const r = clientSeesPeer(on, wait, n, 1, (p) => (p.seat === "0" ? [] : [`seat=${p.seat}`]), { label: "out of the seat", budgetMs: 15000 });
      console.log(`client ${n}: suspect ${r.fails.length ? "still " : "out of the seat after "}${r.seconds.toFixed(1)} s`);
      for (const x of r.fails) fails.push(`take out of car, ${x}`);
    }
  }

  if (process.argv.includes("--escort")) {
    on(2, { verb: "stand", entityName: "Player1" });
    wait(3000);
  }

  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "police arrest proof: FAIL" : "police arrest proof: PASS (menu offered Search, search ran; booking is arrest-flow-proof)");
  return fails.length ? 1 : 0;
}

export default {
  name: "police-arrest-proof",
  usage: "police-arrest-proof [--name <shot prefix>]",
  run: (ctx) => runPoliceArrestProof(),
};
