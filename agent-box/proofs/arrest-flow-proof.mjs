import { ROB_TARGET, arrive, clientBox, clientSeesPeer, parseState, sessionDir } from "../session.mjs";

// The whole arrest as a new officer plays it (police arrest flow review, 2026-11-22), timed per step:
// clock in at the duty desk (the kit has the cuffs) -> the suspect robs the till (wanted) -> the officer
// downs and cuffs them (the booking waypoint lands on the officer's client) -> Escort -> Put in vehicle
// -> Take out of vehicle (at the station) -> walk to the cell door -> both booking options on the
// suspect's action list -> Book (full) -> the suspect is in the cell with the jail clock on their HUD ->
// the clock runs out (`--wait`) and they stand at the release point. `session.mjs roster 1` (client 1 is
// the officer) then `up --clients 2 --profile full`. Flags: --desk, --car, --door (entity names;
// DebugWorld defaults), --wait <s> to watch the release, --name <shot prefix>.
async function runArrestFlowProof() {
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const flag = (name, dflt) => { const i = process.argv.indexOf(name); return i > 0 ? process.argv[i + 1] : dflt; };
  const prefix = flag("--name", "arrest_flow");
  const DESK = flag("--desk", "Roadmap_Police_Desk"); // town-layout-ok: playtest-town desk the session proofs clock in at
  const CAR = flag("--car", "Roadmap_Escort_Car"); // town-layout-ok: playtest-town escort car
  const DOOR = flag("--door", "Roadmap_Jail_Door"); // town-layout-ok: playtest-town cell door
  const waitRelease = Number(flag("--wait", "0"));
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${r.detail}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const fails = [];
  const need = (cond, text) => { if (!cond) fails.push(text); };
  const times = [];
  let t0 = Date.now();
  const step = (name) => { const now = Date.now(); times.push(`${name} ${((now - t0) / 1000).toFixed(1)} s`); t0 = now; };
  const shot = (n, name) => on(n, { verb: "capture", mode: "ui", name: `${prefix}_${name}` }, 25000);

  for (const n of [1, 2]) arrive(on, n, wait);
  wait(8000);
  t0 = Date.now();

  on(1, { verb: "stand", entityName: DESK, side: "s" });
  wait(2500);
  on(1, { verb: "look" });
  need(on(1, { verb: "use", className: "LF_DutyAction", entityName: DESK }, 10000).status === "ok", "clock in");
  wait(4000);
  shot(1, "1_clocked_in");
  step("clock in + kit");

  on(2, { verb: "stand", entityName: ROB_TARGET });
  wait(2500);
  on(2, { verb: "look" });
  on(2, { verb: "use", className: "LF_RobAction", entityName: ROB_TARGET }, 10000);
  wait(7000);
  const s2 = parseState(on(2, { verb: "state" }, 10000).detail);
  need(Number(s2.wanted) > 0, `the suspect is not wanted (${s2.wanted})`);
  const victim = s2.player;
  const body = `Player${victim}`;
  step("suspect wanted");

  on(1, { verb: "stand", entityName: body });
  wait(3000);
  const look1 = on(1, { verb: "look" }, 10000);
  need(!look1.detail.includes("LF_JailArrestAction"), "booking is offered in the field (it should only be at the jail)");
  shot(1, "2_field_menu");
  const down = on(1, { verb: "shoot", playerId: Number(victim) }, 10000);
  need(/downed=1/.test(down.detail), `the hit returned "${down.detail}", expected downed=1`);
  wait(2500);
  on(1, { verb: "stand", entityName: body });
  wait(2000);
  on(1, { verb: "hold", item: "Handcuffs" }, 10000);
  wait(1500);
  on(1, { verb: "cuffing", playerId: Number(victim), kind: "cuff", mode: "pin" }, 10000);
  wait(2500);
  need(on(1, { verb: "cuffing", playerId: Number(victim), kind: "cuff", mode: "finish" }, 10000).status === "ok", "cuff");
  wait(3000);
  const seen1 = on(1, { verb: "seen" }, 10000).detail;
  need(/waypoint=jail-booking@/.test(seen1), `no booking waypoint on the officer's client: ${seen1.slice(0, 160)}`);
  shot(1, "3_cuffed_waypoint");
  step("cuff + waypoint");

  on(1, { verb: "look" });
  need(on(1, { verb: "use", className: "LF_JailEscortArrestAction", entityName: body }, 10000).status === "ok", "escort");
  wait(1500);
  on(2, { verb: "stand", entityName: CAR, side: "s" });
  wait(2500);
  on(1, { verb: "stand", entityName: CAR, side: "n" });
  wait(2500);
  on(1, { verb: "look" });
  need(on(1, { verb: "use", className: "LF_JailPutInVehicleAction", entityName: CAR }, 10000).status === "ok", "put in vehicle");
  const seated = clientSeesPeer(on, wait, 1, Number(victim), (p) => (p.seat === "1" ? [] : [`seat=${p.seat}`]), { label: "seated", budgetMs: 15000 });
  fails.push(...seated.fails.map((x) => `put in vehicle, ${x}`));
  shot(1, "4_in_car");
  step("escort + put in car");

  on(1, { verb: "look" });
  need(on(1, { verb: "use", className: "LF_JailTakeOutOfVehicleAction", entityName: CAR }, 10000).status === "ok", "take out of vehicle");
  wait(3000);
  const out = parseState(on(2, { verb: "state" }, 10000).detail);
  need(!(Number(out.jail) > 0), `take-out booked the suspect silently (jail=${out.jail})`);
  step("take out at the station");

  on(2, { verb: "stand", entityName: DOOR, side: "n" });
  wait(2500);
  on(1, { verb: "stand", entityName: body });
  wait(3000);
  const look2 = on(1, { verb: "look" }, 10000);
  need(look2.detail.includes("LF_JailArrestAction"), "no booking option on the suspect at the jail");
  shot(1, "5_booking_menu");
  let book;
  for (let i = 0; i < 3; i++) {
    book = on(1, { verb: "use", className: "LF_JailArrestAction", entityName: body }, 10000);
    if (book.status === "ok") break;
    wait(2000);
    on(1, { verb: "look" });
  }
  need(book.status === "ok", `book returned ${book.status} ${book.detail}`);
  wait(5000);
  const jailed = parseState(on(2, { verb: "state" }, 10000).detail);
  need(Number(jailed.jail) > 0, `the suspect is not serving after booking (jail=${jailed.jail})`);
  const seen2 = on(2, { verb: "seen" }, 10000).detail;
  need(/jailclock=shown@/.test(seen2), `no jail clock on the prisoner's HUD: ${seen2.slice(-80)}`);
  shot(2, "6_prisoner_cell_clock");
  shot(1, "7_officer_booked");
  step("walk to the cell + book");

  if (waitRelease > 0) {
    wait(waitRelease * 1000);
    const freed = parseState(on(2, { verb: "state" }, 10000).detail);
    need(!(Number(freed.jail) > 0), `still serving after ${waitRelease} s (jail=${freed.jail})`);
    const seen3 = on(2, { verb: "seen" }, 10000).detail;
    need(/jailclock=none/.test(seen3), `the jail clock is still up after release: ${seen3.slice(-60)}`);
    shot(2, "8_released");
    step("sentence served + release");
  }

  console.log(`steps: ${times.join(" | ")}`);
  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "arrest flow proof: FAIL" : "arrest flow proof: PASS (clock in, wanted, cuff, waypoint, escort, car, booking at the cell, held with the HUD clock)");
  return fails.length ? 1 : 0;
}

export default {
  name: "arrest-flow-proof",
  usage: "arrest-flow-proof [--desk E] [--car E] [--door E] [--wait <s>] [--name <shot prefix>]",
  run: (ctx) => runArrestFlowProof(),
};
