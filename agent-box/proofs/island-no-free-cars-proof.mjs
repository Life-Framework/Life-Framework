import { readFileSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ROOT, arrive, clientBox, newestLog, sessionDir, sessionPaths } from "../session.mjs";

// No free cars on the real island (2026-11-22 playtest), two players on a dedicated server:
//   node agent-box/session.mjs up --mission island --clients 2 --wait
//   node agent-box/session.mjs island-no-free-cars-proof --phase before
//   node agent-box/session.mjs restart            (then `rejoin 2`: client 2 joins the restarted server late)
//   node agent-box/session.mjs island-no-free-cars-proof --phase after
// Client 2 (B) is refused at a parked car, a car lot display car and a police car: the get-in action is not
// performed, and a forced seat at the wheel (what a modified client does) is undone by the server guard
// (`[LFDebug:VehicleLock] entry refused`). Phase "before" also has client 1 (A) buy a car, unlock it with the key
// and start it. Phase "after" repeats B's three refusals on the restarted server. Every step prints PASS or FAIL.
const CARS = [
  ["the car lot display UAZ469", "Fit_saint_philippe_carlot_lot_display_uaz469"],
  ["a police car (B is a civilian)", "Fit_saint_philippe_police_cars_car_b32_3"],
  ["a parked car", "Fit_saint_philippe_parked_cars_car_b12"],
];

async function runProof() {
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const flag = (name, dflt) => { const i = process.argv.indexOf(name); return i > 0 ? process.argv[i + 1] : dflt; };
  const phase = flag("--phase", "before");
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${String(r.detail).slice(0, 200)}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const serverLog = () => {
    const f = newestLog(join(sessionPaths(dir).server, "logs"));
    return f ? readFileSync(f, "utf8") : "";
  };
  const refusals = () => (serverLog().match(/\[LFDebug:VehicleLock\][^\n]*entry refused/g) ?? []).length;
  const steps = [];
  const step = (name, ok, why) => { steps.push({ name, ok, why }); console.log(`${ok ? "PASS" : "FAIL"}  ${name}${ok ? "" : `: ${why}`}`); };

  const clients = phase === "before" ? [1, 2] : [2];
  for (const n of clients) arrive(on, n, wait);
  wait(3000);

  for (const [label, name] of CARS) {
    on(2, { verb: "stand", entityName: name, side: "w" });
    // A far car streams in to this client late: wait until its own copy of the world shows a vehicle within 8 m.
    for (let tries = 0; tries < 12; tries++) { wait(2500); if (tries % 3 === 2) on(2, { verb: "stand", entityName: name, side: "w" }); // the first stand after the spawn does not always carry the client
      if (!/veh=none/.test(String(on(2, { verb: "seen", radius: 8 }).detail))) break; }
    if (label === "a parked car") {
      // #4122: a restart put a second, unlocked copy on the marker. Exactly one car stands there, and it is locked.
      const near = String(on(2, { verb: "seen", radius: 4 }).detail).match(/veh=(\S*)/);
      const cars = near && near[1] !== "none" ? near[1].split("|") : [];
      const locked = cars.filter((c) => /:lock=1/.test(c)).length;
      step(`${phase}: exactly one locked car stands on the parked-car marker`, cars.length === 1 && locked === 1, `${cars.length} car(s) within 4 m, ${locked} locked: ${cars.join(" | ")}`);
    }
    on(2, { verb: "look" });
    const getIn = on(2, { verb: "use", className: "SCR_GetInUserAction" }, 10000);
    wait(1500);
    const seatedByAction = on(2, { verb: "engine" }, 10000);
    // A seated character toggles the engine; one standing outside gets not_in_vehicle.
    const outsideAfterAction = seatedByAction.status !== "ok";
    step(`${phase}: B is refused the get-in at ${label}`, outsideAfterAction, `use=${getIn.status} ${getIn.detail}; engine=${seatedByAction.status} ${seatedByAction.detail}`);
    if (!outsideAfterAction) on(2, { verb: "engine", entityName: "out" }, 10000);

    const before = refusals();
    let forced = on(2, { verb: "engine", entityName: "seat" }, 10000);
    // The client streams a far car in a moment after the stand: ask again before calling it a miss.
    for (let tries = 0; tries < 3 && forced.detail === "seat_no_entity"; tries++) { wait(3000); forced = on(2, { verb: "engine", entityName: "seat" }, 10000); }
    wait(4000);
    const after = on(2, { verb: "engine" }, 10000);
    const putOut = after.status !== "ok";
    step(`${phase}: a forced seat at the wheel of ${label} is undone by the server`, putOut && refusals() > before, `seat=${forced.status} ${forced.detail}; after=${after.status} ${after.detail}; entry refused lines ${before} -> ${refusals()}`);
    if (!putOut) on(2, { verb: "engine", entityName: "out" }, 10000);
  }

  if (phase === "before") {
    // A buys a car at the lot, takes it to the garage yard, unlocks it with the key and starts it.
    on(1, { verb: "licence", name: "DRIVING_LICENCE" }, 10000);
    wait(1500);
    on(1, { verb: "stand", entityName: "Fit_saint_philippe_carlot_vehicle_lot" });
    wait(1500);
    on(1, { verb: "use", className: "LF_VehicleBuyAction", entityName: "Fit_saint_philippe_carlot_vehicle_lot" }, 10000);
    wait(1500);
    on(1, { verb: "click", widget: "Car0_Action" });
    wait(2500);
    on(1, { verb: "click", widget: "LotClose" });
    wait(1000);
    const bought = /purchase|bought|sold/i.test((serverLog().match(/\[LFDebug:Vehicle\][^\n]*/g) ?? []).join("\n"));
    const park = on(1, { verb: "park" }, 10000);
    step("before: A buys a car at the lot", park.status === "ok", `park=${park.status} ${park.detail}; vehicle log mentions a sale: ${bought}`);
    // park teleports the car on the server only; Store then Retrieve puts it at the garage where both sides see it.
    on(1, { verb: "stand", entityName: "Fit_saint_philippe_carlot_garage", side: "e5" });
    wait(2500);
    if (/WelcomeGuideDismissButton/.test(String(on(1, { verb: "ui" }).detail))) on(1, { verb: "click", widget: "WelcomeGuideDismissButton" });
    on(1, { verb: "use", className: "LF_VehicleStoreAction", entityName: "Fit_saint_philippe_carlot_garage" }, 10000);
    wait(3000);
    on(1, { verb: "use", className: "LF_VehicleRetrieveAction", entityName: "Fit_saint_philippe_carlot_garage" }, 10000);
    wait(3000);
    on(1, { verb: "stand", entityName: "Fit_saint_philippe_carlot_garage", side: "n12" });
    wait(3500);
    on(1, { verb: "look" });
    on(1, { verb: "use", className: "LF_VehicleLockAction" }, 10000);
    wait(1500);
    const unlock = on(1, { verb: "use", className: "LF_VehicleLockAction" }, 10000); // the second toggle: locked then unlocked with the key
    wait(1500);
    const before = refusals();
    const seat = on(1, { verb: "engine", entityName: "seat" }, 10000);
    wait(4000);
    const start = on(1, { verb: "engine" }, 10000);
    step("before: A unlocks their own car with the key and drives it", seat.status === "ok" && start.status === "ok" && refusals() === before, `unlock=${unlock.status} ${unlock.detail}; seat=${seat.status} ${seat.detail}; engine=${start.status} ${start.detail}; new refusals ${refusals() - before}`);
    on(1, { verb: "engine", entityName: "out" }, 10000);
  }

  const kept = serverLog().split(/\r?\n/).filter((l) => /LFDebug:(VehicleLock|Vehicle|ParkedCar)/.test(l));
  writeFileSync(join(ROOT, "tmp", `island-no-free-cars-${phase}-server.txt`), kept.join("\n"));
  const failed = steps.filter((s) => !s.ok);
  console.log("");
  for (const s of steps) console.log(`${s.ok ? "PASS" : "FAIL"}  ${s.name}`);
  console.log(failed.length ? `island no-free-cars proof (${phase}): FAIL (${failed.length} of ${steps.length})` : `island no-free-cars proof (${phase}): PASS (${steps.length} steps)`);
  return failed.length ? 1 : 0;
}

export default {
  name: "island-no-free-cars-proof",
  usage: "island-no-free-cars-proof [--phase before|after]",
  run: (ctx) => runProof(),
};
