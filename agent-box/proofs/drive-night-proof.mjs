import { mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { ROOT, arrive, clientBox, parseState, sessionDir, sessionPaths } from "../session.mjs";
import { allServerLogs } from "./_island-kit.mjs";
import { enterWhenThere, kv, yes } from "./drive-proof.mjs";

// Driving at 22:00 and a police pursuit, two players on `up --mission island --clients 2 --profile full --hold 20`:
//   node agent-box/session.mjs drive-night-proof [--phase courier,pursuit,night] [--speed kmh]
// Client 1 is the driver: buys the cheapest car, drives a parcel run depot to drop, then the car is reported stolen and
// driven to the districts garage while client 2, an on-duty officer, follows in the police car, runs the
// plate and stops behind it with the siren on. Then client 1 drives by road, headlights on, from the truck yard to the
// east compound's smelter and on to the north outpost (the last 150 m off any road). A picture from the driver's seat
// every `--shot-every` seconds. Every step prints PASS or FAIL; a non-PASS is a finding, not a proof bug, unless a
// harness line says so.
const FIT = "Fit_saint_philippe_";
const LOT = `${FIT}carlot_vehicle_lot`;
const LOT_PAD = [4390.0, 10639.2];
const DEPOT = `${FIT}parcel_job_depot`;
const DROP = `${FIT}districts_parcel_drops_drop_b34`;
const DESK = `${FIT}police_police_kit_duty_desk`;
const POLICE_CAR = `${FIT}police_cars_car_b32_3`;
const POLICE_SEAT = "veh_LF_UAZ469_Police";
const YARD_LOT = "Fit_ne_compound_truckyard_lot";
const YARD_GATE = "Fit_ne_compound_truckyard_sign_gate";
// East compound smelter (counter: the furnace's trader row) and the north outpost's gate (opening x 7657 to 7664 at z 8388.7).
const SMELTER = [6442, 6486];
const SMELTER_ENTITY = "Fit_eastcompound_smelter_smelter_furnace";
const OUTPOST_STRIP = [7688, 8240];
const OUTPOST_GATE = [7660.5, 8384];
const OUTPOST_QM = "Fit_outpost_north_camp_outpost_npc_quartermaster";
const RADAR = "{C7C5D20DD66658B4}Prefabs/Items/Police/LF_RadarGun.et";

const arg = (name, dflt) => {
  const i = process.argv.indexOf(name);
  return i > 0 ? process.argv[i + 1] : dflt;
};

async function run() {
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout ?? 10000);
    console.log(`client ${n}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${String(r.detail ?? "").slice(0, 260)}`);
    return r;
  };
  const results = [];
  const step = (name, ok, detail) => {
    results.push({ name, ok });
    console.log(`${ok ? "PASS" : "FAIL"}  ${name}${detail ? `: ${detail}` : ""}`);
    return ok;
  };
  const phases = String(arg("--phase", "courier,pursuit,night")).split(",");
  const speed = arg("--speed", "40");
  const shotEvery = Number(arg("--shot-every", "40"));
  const log = () => allServerLogs(dir);
  const state = (n) => parseState(on(n, { verb: "state" }).detail);
  const veh = (n) => kv(on(n, { verb: "vehicle", words: ["state"] }).detail);
  const posOfVeh = (n) => {
    const m = /^(-?[\d.]+),(-?[\d.]+)/.exec(String(veh(n).pos ?? ""));
    return m ? [Number(m[1]), Number(m[2])] : null;
  };
  const shots = [];
  const shot = (n, name) => {
    const r = on(n, { verb: "capture", mode: "ui", name }, 30000);
    shots.push(name);
    return r;
  };
  const dist = (a, b) => Math.hypot(a[0] - b[0], a[1] - b[1]);
  const legs = [];

  // A drive that takes pictures from the seat while it runs. Returns the `drive wait` answer's key=values.
  const drive = (n, label, how, [tx, tz], { tol = 3, spd = speed, timeout = 400, shots: shotName = "", tick = null } = {}) => {
    let started = on(n, { verb: "drive", words: [how, `${tx},${tz}`, `speed=${spd}`, `timeout=${timeout}`, `tol=${tol}`, "mode=input"] }, 20000);
    if (started.status !== "ok" && how === "road") {
      console.log(`drive road refused (${started.detail}); straight drive`);
      started = on(n, { verb: "drive", words: ["to", `${tx},${tz}`, `speed=${spd}`, `timeout=${timeout}`, `tol=${tol}`, "mode=input"] }, 20000);
    }
    if (started.status !== "ok") return { status: "not_started", why: started.detail };
    const t0 = Date.now();
    let lastShot = 0;
    let s = {};
    while (Date.now() - t0 < (timeout + 30) * 1000) {
      wait(2000);
      s = veh(n);
      console.log(`${label} t=${((Date.now() - t0) / 1000).toFixed(0)}s pos=${s.pos} speed=${s.speed} drive=${s.drive} driven=${s.driven} lights=${s.lights}`);
      if (tick) tick(s);
      if (shotName && (Date.now() - lastShot) / 1000 >= shotEvery) {
        shot(n, `${shotName}_${shots.length}`);
        lastShot = Date.now();
      }
      if (!/^(driving|braking|stop)$/.test(s.drive ?? "")) break;
    }
    const done = on(n, { verb: "drive", words: ["wait"] }, 30000);
    const end = kv(done.detail);
    end.status = String(done.detail ?? "").split(" ")[0];
    end.route = started.detail;
    legs.push({ label, ...end });
    console.log(`LEG ${label}: ${end.status} driven=${end.driven} m time=${end.time} s err=${end.err} max=${end.max} backups=${end.backups}`);
    return end;
  };

  // ---- setup: both players, money, licence, a car at the lot, an officer on duty -------------------------------------
  for (const n of [1, 2]) arrive(on, n, wait);
  for (const n of [1, 2]) {
    wait(2500);
    on(n, { verb: "click", widget: "WelcomeGuideDismissButton" }, 10000);
    on(n, { verb: "guide", words: ["close"] });
  }
  // The flagship owner config (police.alphaInstantApproval approves a desk application on submit).
  const ownerFile = join(sessionPaths(dir).server, "profile", "LifeFramework", "config.json");
  mkdirSync(dirname(ownerFile), { recursive: true });
  writeFileSync(ownerFile, JSON.stringify(JSON.parse(readFileSync(join(ROOT, "server", "flagship", "config.json"), "utf8")), null, 2));
  on(1, { verb: "configreload" }, 10000);
  wait(3500);
  on(1, { verb: "time", hour: 22 }, 15000);
  on(1, { verb: "checkpoint", words: ["10"] }, 20000);
  wait(3500);
  on(1, { verb: "licence", name: "DRIVING_LICENCE" }, 10000);
  wait(1500);
  on(1, { verb: "stand", entityName: LOT });
  wait(2000);
  on(1, { verb: "use", className: "LF_VehicleBuyAction", entityName: LOT }, 10000);
  wait(2000);
  const widget = /Car\d+_Action/.exec(String(on(1, { verb: "ui" }).detail))?.[0] ?? "Car0_Action";
  on(1, { verb: "click", widget });
  wait(4000);
  on(1, { verb: "click", widget: "LotClose" });
  const bought = state(1);
  step("0 a car is bought at the lot", !!bought.parked && bought.parked !== "none", `parked=${bought.parked}`);
  const enter1 = enterWhenThere((spec, t) => on(1, spec, t), wait, LOT_PAD, 8);
  wait(1500);
  step("0a the driver sits at the wheel and owns the car", enter1.status === "ok" && yes(veh(1).owner), `${enter1.status} ${enter1.detail}`);
  on(1, { verb: "vehicle", words: ["lights", "on"] });

  // Officer: joins the police and clocks in (the island's desk approves an application on submit).
  on(2, { verb: "resetcrime" }, 10000);
  on(2, { verb: "stand", entityName: DESK });
  wait(2500);
  on(2, { verb: "look" });
  let duty = state(2);
  if (duty.duty !== "1") {
    if (duty.faction !== "POLICE") {
      on(2, { verb: "use", className: "LF_PoliceApplyAction", entityName: DESK }, 10000);
      wait(1500);
      on(2, { verb: "type", widget: "PolAppName", text: "Bob_Officer" });
      on(2, { verb: "type", widget: "PolAppWhy", text: "Run_the_plates" });
      on(2, { verb: "type", widget: "PolAppAvailability", text: "Evenings" });
      on(2, { verb: "click", widget: "PolAppSubmit" });
      wait(3000);
      on(2, { verb: "look" });
    }
    on(2, { verb: "use", className: "LF_DutyAction", entityName: DESK }, 10000);
    wait(3000);
    duty = state(2);
  }
  step("0b client 2 is an officer on duty", duty.duty === "1", `faction=${duty.faction} duty=${duty.duty}`);

  // ---- courier: a parcel run by car, depot to the districts drop -------------------------------------------------------
  if (phases.includes("courier")) {
    const c0 = Number(state(1).cash || 0);
    const toDepot = drive(1, "lot_to_depot", "road", [4421.5, 10748], { tol: 6, spd: "30" });
    step("1a the car is driven from the lot to the post office depot", /arrived/.test(toDepot.status), `${toDepot.status} ${toDepot.driven} m in ${toDepot.time} s, err ${toDepot.err}`);
    on(1, { verb: "vehicle", words: ["stop"] });
    wait(1000);
    on(1, { verb: "vehicle", words: ["exit", "force"] }, 10000);
    wait(1500);
    on(1, { verb: "stand", entityName: DEPOT, side: "e" });
    wait(2000);
    on(1, { verb: "use", className: "LF_JobSignUpAction", entityName: DEPOT }, 10000);
    wait(1500);
    const startRun = on(1, { verb: "use", className: "LF_JobStartRunAction", entityName: DEPOT }, 10000);
    wait(2000);
    step("1b the parcel run starts at the depot counter", startRun.status === "ok", `${startRun.status} ${startRun.detail}`);
    const seat = on(1, { verb: "vehicle", words: ["enter", "driver", "r=20"] }, 15000);
    wait(1500);
    if (startRun.status === "ok" && seat.status === "ok") {
      const toDrop = drive(1, "depot_to_drop", "road", [4750, 10722], { tol: 6, spd: "30", shots: "night_courier" });
      step("1c the parcel is driven to the districts drop", /arrived/.test(toDrop.status), `${toDrop.status} ${toDrop.driven} m in ${toDrop.time} s, err ${toDrop.err}`);
      on(1, { verb: "vehicle", words: ["stop"] });
      wait(1000);
      on(1, { verb: "vehicle", words: ["exit", "force"] }, 10000);
      wait(1500);
      on(1, { verb: "stand", entityName: DROP });
      wait(1500);
      const drop = on(1, { verb: "use", className: "LF_JobDropoffAction", entityName: DROP }, 10000);
      wait(2500);
      const paid = Number(state(1).cash || 0) - c0;
      step("1d the drop pays (night rate after 21:00)", drop.status === "ok" && paid > 0, `${drop.status} ${drop.detail}; cash +$${paid}`);
      shot(1, "night_courier_paid");
      on(1, { verb: "vehicle", words: ["enter", "driver", "r=20"] }, 15000);
      wait(1500);
    }
  }

  // ---- pursuit ---------------------------------------------------------------------------------------------------
  if (phases.includes("pursuit")) {
    on(1, { verb: "markstolen", words: [] }, 10000);
    const plateA = /plate=(\S+\s?\S*\s?\S*)/.exec(String(on(1, { verb: "state" }).detail))?.[1] ?? "";
    // The officer unlocks the police car, sits at the wheel and gets the radar gun for later.
    on(2, { verb: "stand", entityName: POLICE_CAR, side: "w" });
    wait(3000);
    on(2, { verb: "use", className: "LF_VehicleLockAction" }, 10000);
    wait(1500);
    const seatB = on(2, { verb: "vehicle", words: ["enter", "driver", "r=10", POLICE_SEAT] }, 15000);
    wait(2000);
    step("2a the officer sits at the wheel of the police car", seatB.status === "ok" && veh(2).seat === "pilot", `${seatB.status} ${seatB.detail}`);
    on(2, { verb: "vehicle", words: ["lights", "on"] });
    on(2, { verb: "siren", entityName: POLICE_CAR, mode: 1 }, 10000);
    // A drives to the truck yard; B chases whatever A's position is, re-aiming every 3 s.
    const startA = on(1, { verb: "drive", words: ["road", "4687.6,10751.7", "speed=30", "timeout=300", "tol=8", "mode=input"] }, 20000);
    step("2b the driver sets off for the NE compound truck yard", startA.status === "ok", `${startA.status} ${startA.detail}`);
    let closest = 9999;
    let plateRead = "";
    let ranPlate = false;
    const tA = Date.now();
    let startedB = false;
    while (startA.status === "ok" && Date.now() - tA < 240000) {
      wait(2500);
      const a = veh(1);
      const pa = /^(-?[\d.]+),(-?[\d.]+)/.exec(String(a.pos ?? ""));
      const pb = posOfVeh(2);
      if (!pa) break;
      const target = [Number(pa[1]), Number(pa[2])];
      const d = pb ? dist(pb, target) : 9999;
      closest = Math.min(closest, d);
      console.log(`pursuit t=${((Date.now() - tA) / 1000).toFixed(0)}s A=${a.pos} ${a.speed}km/h ${a.drive}  B=${pb?.map((v) => v.toFixed(0)).join(",")} gap=${d.toFixed(0)} m`);
      if (!startedB || d > 30) {
        on(2, { verb: "drive", words: ["to", `${target[0].toFixed(1)},${target[1].toFixed(1)}`, "speed=60", "timeout=60", "tol=10", "mode=input"] }, 15000);
        startedB = true;
      }
      if (d < 28 && !ranPlate) {
        const before = (log().match(/run plate by /g) || []).length;
        const rp = on(2, { verb: "use", className: "LF_RunPlateAction" }, 10000);
        wait(2000);
        const lines = log().match(/run plate by [^\n]*/g) || [];
        plateRead = lines.length > before ? lines[lines.length - 1] : "";
        ranPlate = true;
        step("2c the officer runs the plate on the car ahead while driving", rp.status === "ok" && !!plateRead, `${rp.status} ${rp.detail}; ${plateRead.slice(0, 160)}`);
        shot(2, "night_pursuit_runplate");
      }
      if (!/^(driving|braking)$/.test(a.drive ?? "")) break;
    }
    const endA = on(1, { verb: "drive", words: ["wait"] }, 30000);
    // A pulls over and stops (what a driver does for a siren); B comes up behind and stops.
    on(1, { verb: "vehicle", words: ["stop"] });
    const pa = posOfVeh(1);
    if (pa) on(2, { verb: "drive", words: ["to", `${(pa[0] - 6).toFixed(1)},${pa[1].toFixed(1)}`, "speed=30", "timeout=30", "tol=10", "mode=input"] }, 15000);
    wait(6000);
    on(2, { verb: "vehicle", words: ["stop"] });
    wait(1500);
    const fa = posOfVeh(1);
    const fb = posOfVeh(2);
    const gap = fa && fb ? dist(fa, fb) : 9999;
    step("2d the police car is behind the stopped car, within 15 m, both stopped", gap < 15 && Number(veh(1).speed) < 2 && Number(veh(2).speed) < 2, `gap ${gap.toFixed(1)} m, closest approach while driving ${closest.toFixed(0)} m, A ${veh(1).speed} km/h B ${veh(2).speed} km/h, A end ${String(endA.detail).slice(0, 80)}`);
    shot(2, "night_pursuit_stop_b");
    shot(1, "night_pursuit_stop_a");
    // The pager (client 2) after the plate read.
    const seenB = String(on(2, { verb: "seen", words: ["30"] }).detail);
    console.log(`B sees: ${seenB.slice(0, 260)}`);
    // Radar gun, on foot beside the road: only a moving car reads, so A drives a stretch past B.
    on(2, { verb: "itemgive", prefab: RADAR }, 10000);
    on(2, { verb: "vehicle", words: ["exit", "force"] }, 10000);
    wait(1500);
    const held = on(2, { verb: "hold", item: "RadarGun" }, 10000);
    wait(1500);
    step("2e the officer takes the radar gun in hand", held.status === "ok", `${held.status} ${held.detail}`);
    const radar0 = (log().match(/radar/gi) || []).length;
    on(1, { verb: "drive", words: ["to", `${(pa?.[0] ?? 5000) + 90},${pa?.[1] ?? 10840}`, "speed=45", "timeout=30", "tol=6", "mode=input"] }, 15000);
    wait(5000);
    const rr = on(2, { verb: "use", className: "LF_RadarGunAction" }, 10000);
    wait(2000);
    const radarLines = (log().match(/[^\n]*radar[^\n]*/gi) || []).slice(-3).join(" | ");
    const radarNoCar = /LF-Radar_NoCar/.test(log().slice(-6000));
    step("2f the radar gun reads a speed (not `Radar_NoCar`)", rr.status === "ok" && (log().match(/radar/gi) || []).length > radar0 && !radarNoCar, `${rr.status} ${rr.detail}; ${radarLines.slice(0, 200)}`);
    on(1, { verb: "drive", words: ["wait"] }, 30000);
    on(1, { verb: "vehicle", words: ["stop"] });
  }

  // ---- night drives from the truck yard -----------------------------------------------------------------------------
  if (phases.includes("night")) {
    on(1, { verb: "time", hour: 22 }, 15000);
    on(1, { verb: "vehicle", words: ["lights", "on"] });
    step("3a the headlights are on", /^(1|on|true)$/i.test(String(veh(1).lights ?? "")), `lights=${veh(1).lights}`);
    if (veh(1).seat === "pilot") { on(1, { verb: "vehicle", words: ["stop"] }); on(1, { verb: "vehicle", words: ["exit", "force"] }, 10000); wait(1500); }
    on(1, { verb: "stand", entityName: LOT });
    wait(1500);
    on(1, { verb: "parkat", words: [LOT, "10"] }, 10000);
    wait(3000);
    on(1, { verb: "vehicle", words: ["enter", "driver", "r=40"] }, 15000);
    wait(1500);
    on(1, { verb: "vehicle", words: ["lights", "on"] });
    shot(1, "night_town_start");
    const east = drive(1, "town_to_east_compound", "road", SMELTER, { tol: 12, spd: speed, timeout: 600, shots: "night_east" });
    step("3b the car is driven by road to the east compound's smelter at night", /arrived/.test(east.status), `${east.status} ${east.driven} m in ${east.time} s, err ${east.err}, ${east.backups} backups; route ${String(east.route).slice(0, 80)}`);
    on(1, { verb: "vehicle", words: ["stop"] });
    wait(1500);
    shot(1, "night_east_arrived");
    on(1, { verb: "vehicle", words: ["exit", "force"] }, 10000);
    wait(1500);
    on(1, { verb: "stand", entityName: SMELTER_ENTITY });
    wait(2000);
    const lookE = String(on(1, { verb: "look" }).detail);
    step("3c the smelter counter offers its action to a player at night", /Action/.test(lookE), lookE.slice(0, 160));
    shot(1, "night_east_counter");
    on(1, { verb: "vehicle", words: ["enter", "driver", "r=60"] }, 15000);
    wait(1500);
    on(1, { verb: "vehicle", words: ["lights", "on"] });
    const strip = drive(1, "east_to_outpost_strip", "road", OUTPOST_STRIP, { tol: 15, spd: speed, timeout: 600, shots: "night_outpost" });
    step("3d the car is driven by road to the strip below the north outpost", /arrived|distance/.test(strip.status), `${strip.status} ${strip.driven} m in ${strip.time} s, err ${strip.err}; route ${String(strip.route).slice(0, 80)}`);
    const gate = drive(1, "strip_to_outpost_gate", "to", OUTPOST_GATE, { tol: 6, spd: "20", timeout: 120, shots: "night_outpost_gate" });
    step("3e the last 150 m off road to the outpost gate", /arrived/.test(gate.status), `${gate.status} ${gate.driven} m in ${gate.time} s, err ${gate.err}, ${gate.backups} backups`);
    on(1, { verb: "vehicle", words: ["stop"] });
    wait(1500);
    shot(1, "night_outpost_gate_arrived");
    on(1, { verb: "vehicle", words: ["exit", "force"] }, 10000);
    wait(1500);
    on(1, { verb: "stand", entityName: OUTPOST_QM });
    wait(2000);
    const lookO = String(on(1, { verb: "look" }).detail);
    step("3f the quartermaster offers talk to a player at night", /Action|Talk/i.test(lookO), lookO.slice(0, 160));
    shot(1, "night_outpost_counter");
  }

  console.log(`PICTURES (client 1 and 2 capture names): ${shots.join(", ")}`);
  for (const l of legs) console.log(`LEG ${l.label}: ${l.status} ${l.driven} m ${l.time} s`);
  const failed = results.filter((r) => !r.ok);
  console.log(failed.length ? `drive-night-proof: FAIL (${failed.length} of ${results.length}: ${failed.map((r) => r.name).join("; ")})` : `drive-night-proof: PASS (${results.length} of ${results.length})`);
  return failed.length ? 1 : 0;
}

export default {
  name: "drive-night-proof",
  usage: "drive-night-proof [--phase courier,pursuit,night] [--speed kmh] [--shot-every s]   (on `up --mission island --clients 2 --profile full`; client 1 drives, client 2 is the officer)",
  run: () => run(),
};
