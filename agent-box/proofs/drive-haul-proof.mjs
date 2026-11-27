import { execFileSync } from "node:child_process";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { arrive, clientBox, parseState, sessionDir } from "../session.mjs";
import { allServerLogs } from "./_island-kit.mjs";
import { AMOUNT, parseMoney } from "./_money.mjs";
import { driveAndFollow, kv, yes } from "./drive-proof.mjs";

// The truck haul on the hardwood stand with the truck DRIVEN (#5044's `vehicle` and `drive`), not set down by `parkat`.
// Straight after `truck-check-proof` (it buys the first truck row at the truck yard; one client on `up --mission island
// --profile full --hold 20`):
//   node agent-box/session.mjs truck-check-proof
//   node agent-box/session.mjs drive-haul-proof [--trees N] [--no-merchant]
//   0 the truck is driven from the truck yard to the woodlot by road (the first leg, not part of a cycle)
//   1 the truck is driven off the road to a standing hardwood tree, the tree is felled, each log is hoisted and loaded
//   2 the full truck is driven back to the saw by road, unloaded, every log sawn, the boards sold at the woodlot
//     buyer (premium) and the wholesale counter beside it
//   3 the truck is driven with boards in the pockets to the merchant timber counter at the hardware store and the
//     boards are sold there, then driven back
//   4 the minutes (drive legs: the game's own clock; walking, carrying and felling holds: the same model
//     truck-haul-proof uses, the harness teleports) and the dollars an hour beside `tools\cli economy`
const SAW = "Fit_saint_philippe_woodlot_saw";
const DESK = "Fit_saint_philippe_woodlot_desk";
const BUYER = "Fit_saint_philippe_woodlot_buyer";
const WHOLESALE = "Fit_saint_philippe_woodlot_wholesale";
const MERCHANT = "Fit_saint_philippe_merchant_timber_timber_buyer";
const SAW_ACTION = process.env.SAW_ACTION ?? "LF_ProcessAction__3";
// Where a driver parks beside the saw (the woodlot's truck pad; its own placed Ural stands there locked).
const SAW_PARK = [4352, 10640];
// Where the truck parks at the hardware store's timber counter (counter at 4527.1, 10628.8).
const MERCHANT_PARK = [4519, 10640];
// Where a truck bought at the yard stands (Fit_ne_compound_truckyard_lot 5086.6, 10844.3 + m_vOffset -22.5, 4.5),
// and the point just inside the yard gate the one clear line from it reaches.
const TRUCK_PAD = [5064, 10849];
const YARD_GATE_OUT = [5019, 10849];
const JOG_MPS = 4.5;
const LOADED_MPS = 4.5 * 0.5;
const BED_UNITS = 12;
const HARDWOOD_UNITS = 2;
const CHOP_SECONDS = { 1: 30, 2: 45, 3: 60 };
const root = dirname(fileURLToPath(import.meta.url));

export function dollarsPerHour(dollars, minutes) {
  return minutes > 0 ? Math.round((dollars * 60) / minutes) : 0;
}

export function parseEconomy(text) {
  const m = new RegExp(String.raw`LOOP truck hardwood -> boards, firewood:[^\n]*?steady \$(${AMOUNT})\/h, first hour \$(${AMOUNT})\/h`).exec(String(text));
  return m ? { steady: parseMoney(m[1]), first: parseMoney(m[2]) } : null;
}

export function posOf(detail) {
  const m = /pos=(-?[\d.]+),(-?[\d.]+),(-?[\d.]+)/.exec(String(detail));
  return m ? [Number(m[1]), Number(m[2]), Number(m[3])] : null;
}

function selftest() {
  const f = [];
  if (dollarsPerHour(100, 15) !== 400 || dollarsPerHour(5, 0) !== 0) f.push("dollars per hour");
  if (parseEconomy("  LOOP truck hardwood -> boards, firewood: ~45.6 logs/h, steady $742/h, first hour $786/h; duty")?.steady !== 742) f.push("economy row");
  if (posOf("seat=pilot pos=1.5,2,-3 speed=0")?.join() !== "1.5,2,-3") f.push("posOf");
  const t = stuckTally([{ status: "arrived" }, { status: "close" }, { status: "stuck" }, { status: "timeout" }]);
  if (t.arrived !== 1 || t.close !== 1 || t.stuck !== 2 || !reached({ status: "close" }) || reached({ status: "stuck" })) f.push("stuck tally");
  for (const x of f) console.log(`FAIL  ${x}`);
  console.log(f.length ? "drive-haul-proof selftest: FAIL" : "drive-haul-proof selftest: PASS");
  return f.length ? 1 : 0;
}

async function run(args) {
  if (args.includes("--selftest")) return selftest();
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const on = (spec, timeout) => {
    h.setBoxes([clientBox(dir, 1)]);
    const r = h.sendCommand(spec, timeout ?? 10000);
    console.log(`client 1: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${String(r.detail ?? "").slice(0, 300)}`);
    return r;
  };
  const results = [];
  const step = (name, ok, detail) => {
    results.push({ name, ok });
    console.log(`${ok ? "PASS" : "FAIL"}  ${name}${detail ? `: ${detail}` : ""}`);
    return ok;
  };
  const log = () => allServerLogs(dir);
  const state = () => parseState(on({ verb: "state" }).detail);
  const cash = () => Number(state().cash || 0);
  const carried = (item) => {
    const r = on({ verb: "carries", words: [item] });
    const m = /count=(\d+)/.exec(String(r.detail));
    return r.status === "ok" && m ? Number(m[1]) : 0;
  };
  const nothingIn = (v) => !v || v === "none";
  const tc = (ci) => ci >= 0 && args[ci + 1] ? Number(args[ci + 1]) : 0;
  const maxTrees = Math.max(1, Math.min(5, tc(args.indexOf("--trees")) || 3));
  const world = (name, x, y, z, ox, oy, oz) => on({ verb: "capture", mode: "world", name, x, y, z, ox, oy, oz, hour: 9 }, 30000);
  const legs = [];
  // One driven leg: enters the truck wherever it stands, drives `mode to|road` there, exits. Records game seconds and metres.
  const driveLeg = (label, how, [tx, tz], tol, speed = 35) => {
    const here = posOf(on({ verb: "vehicle", words: ["state"] }).detail) ?? posOf(on({ verb: "state" }).detail);
    const seated = kv(on({ verb: "vehicle", words: ["state"] }).detail);
    if (seated.seat !== "pilot") {
      const p = truckAt();
      const enter = on({ verb: "vehicle", words: ["enter", "driver", `at=${p[0]},${p[2]}`, "r=12"] }, 15000);
      if (enter.status !== "ok") return { label, status: `enter_${enter.detail}`, driven: 0, time: 0 };
      wait(1500);
    }
    const t0 = Date.now();
    let r = driveAndFollow(on, wait, [how, `${tx},${tz}`, `speed=${speed}`, "timeout=400", `tol=${tol}`, "mode=input"], { label });
    if (r.started.status !== "ok" && how === "road") {
      console.log(`drive road refused (${r.started.detail}); straight drive`);
      r = driveAndFollow(on, wait, ["to", `${tx},${tz}`, `speed=${speed}`, "timeout=400", `tol=${tol}`, "mode=input"], { label });
    }
    const leg = { label, status: r.end.status, driven: parseFloat(r.end.driven ?? "0"), time: parseFloat(r.end.time ?? "0"), err: r.end.err, backups: r.end.backups, approach: r.end.approach, creepbackups: r.end.creepbackups, why: r.end.why, wall: (Date.now() - t0) / 1000, from: here };
    legs.push(leg);
    console.log(`LEG ${label}: ${leg.status} ${leg.driven} m in ${leg.time} s game time (${leg.wall.toFixed(0)} s wall), err ${leg.err} m, backups ${leg.backups}${leg.approach ? `, creep ${leg.approach} m (${leg.creepbackups} creep backups)` : ""}${leg.why ? `, why ${leg.why}` : ""}`);
    on({ verb: "vehicle", words: ["stop"] });
    wait(1500);
    return leg;
  };
  // The owned truck's position now, from `state`'s parkedat (the harness reads the world) or the seated vehicle.
  const truckAt = () => {
    const v = kv(on({ verb: "vehicle", words: ["state"] }).detail);
    const p = posOf(`pos=${v.pos ?? ""}`);
    if (p && v.seat === "pilot") return p;
    const s = String(on({ verb: "state" }).detail);
    const m = /parkedat=(-?[\d.]+),(-?[\d.]+),(-?[\d.]+)/.exec(s);
    return m ? [Number(m[1]), Number(m[2]), Number(m[3])] : null;
  };
  const leave = () => {
    on({ verb: "vehicle", words: ["stop"] });
    wait(1000);
    const out = on({ verb: "vehicle", words: ["exit", "force"] }, 10000);
    wait(1500);
    return out;
  };
  // A standing point from which the truck's tailgate action works: tries two rings round the truck, nearest ring first.
  let gatePoint = null;
  const atTailgate = (truck, action = "LF_CargoBedAction") => {
    const tryAt = (ox, oz) => {
      world("drive_haul_tailgate", truck[0], truck[1], truck[2], ox, 0.4, oz);
      wait(1200);
      const mark = log().length;
      const r = on({ verb: "use", className: action }, 15000);
      wait(1300);
      return r.status === "ok" && /\[LFDebug:Cargo\] (?:un)?loaded/.test(log().slice(mark));
    };
    const pts = [];
    if (gatePoint) pts.push(gatePoint);
    for (const r of [4.2, 5.6, 3.2]) for (let d = 0; d < 360; d += 45) pts.push([Number((r * Math.cos((d * Math.PI) / 180)).toFixed(2)), Number((r * Math.sin((d * Math.PI) / 180)).toFixed(2))]);
    for (const [ox, oz] of pts) {
      if (tryAt(ox, oz)) {
        gatePoint = [ox, oz];
        return true;
      }
    }
    return false;
  };

  // ---- 0. the truck, unlocked, driven from the yard to the woodlot -------------------------------------------------
  const st0 = String(on({ verb: "state" }).detail);
  const parked = /parked=(\S+)/.exec(st0)?.[1] ?? "none";
  step("0 the player owns a truck", !nothingIn(parked) && !/parkedat=none/.test(st0), `parked=${parked}`);
  if (nothingIn(parked)) return finish(results);
  on({ verb: "guide", words: ["close"] });
  on({ verb: "time", hour: 9.0 }, 15000);
  on({ verb: "levelset", words: ["LUMBERJACK", "3"] }, 10000);
  wait(1500);
  if (parseState(on({ verb: "state" }).detail).job !== "LUMBERJACK") {
    on({ verb: "stand", entityName: DESK });
    wait(1500);
    on({ verb: "use", className: "LF_WorkplaceJobAction", entityName: DESK });
    wait(1500);
    on({ verb: "stand", entityName: "Fit_ne_compound_truckyard_lot" });
    wait(1500);
  }
  const spawn = truckAt();
  const enter0 = on({ verb: "vehicle", words: ["enter", "driver", `at=${spawn[0]},${spawn[2]}`, "r=15"] }, 15000);
  wait(1500);
  const seat0 = kv(on({ verb: "vehicle", words: ["state"] }).detail);
  step("0a the character sits at the wheel of the bought truck and owns it", enter0.status === "ok" && seat0.seat === "pilot" && yes(seat0.owner), `${enter0.status} seat=${seat0.seat} owner=${seat0.owner} veh=${seat0.veh} pos=${seat0.pos}`);
  // The key row: unlock it from the seat if the lock row is there (the cargo bed follows the vehicle lock).
  const lockUse = on({ verb: "use", className: "LF_VehicleLockAction" }, 10000);
  wait(1500);
  console.log(`unlock from the seat: ${lockUse.status} ${lockUse.detail}`);
  const nearWood = Math.hypot(spawn[0] - SAW_PARK[0], spawn[2] - SAW_PARK[1]) < 150;
  if (!nearWood) {
    // The truck yard has no AI road inside it and is shut in by sheds, sand piles, light towers, tin walls and the
    // display trucks: the one clear line from the pad runs due west to the gate (the planner's ring scan,
    // 2026-11-27). Out of the gate first, then the road (the nearest road point otherwise lies north across the
    // scrapyard dump, #5053).
    const gate = Math.hypot(spawn[0] - TRUCK_PAD[0], spawn[2] - TRUCK_PAD[1]) < 25 ? driveLeg("yard_to_gate", "to", YARD_GATE_OUT, 4, 15) : null;
    const first = driveLeg("yard_to_woodlot", "road", SAW_PARK, 4);
    if (gate) console.log(`LEG yard_to_gate counted with the road leg: ${gate.status} ${gate.driven} m`);
    step("0b the truck is driven from the truck yard to the woodlot by road", (!gate || reached(gate)) && reached(first) && first.driven > 500, `${gate ? `gate ${gate.status} ${gate.driven} m, ` : ""}${first.status} ${first.driven} m in ${first.time} s, stopped ${first.err} m from the pad`);
  } else console.log("the truck already stands at the woodlot: leg 0 skipped");
  leave();
  const park0 = truckAt();
  if (!park0) return finish(results);
  // The tailgate probe doubles as the unlock test: an unlocked truck loads. Unlock at the door first if it is locked.
  const seenLock = String(on({ verb: "seen", words: ["14"] }).detail);
  if (/lock=1/.test(seenLock.split("|")[0] ?? "")) {
    for (let k = 0; k < 8; k++) {
      const a = (k * Math.PI) / 4;
      world("drive_haul_door", park0[0], park0[1], park0[2], Number((3.0 * Math.cos(a)).toFixed(2)), 0.3, Number((3.0 * Math.sin(a)).toFixed(2)));
      wait(1200);
      on({ verb: "use", className: "LF_VehicleLockAction" }, 10000);
      wait(1300);
      if (/lock=0/.test(String(on({ verb: "seen", words: ["14"] }).detail).split("|")[0] ?? "")) break;
    }
  }
  console.log(`seen after the unlock tries: ${String(on({ verb: "seen", words: ["14"] }).detail).slice(0, 200)}`);

  // ---- 1..3. a full cycle, `--cycles N` in a row --------------------------------------------------------------------
  const cycles = Math.max(1, Math.min(5, tc(args.indexOf("--cycles")) || 1));
  for (let cy = 1; cy <= cycles; cy++) {
  const tag = cycles > 1 ? `c${cy} ` : "";
  const legsBefore = legs.length;
  const startedAt = Date.now();
  let walkM = 0;
  let loadedM = 0;
  let chopSeconds = 0;
  let logsLoaded = 0;
  let felledTrees = 0;
  const serverMark = log().length;
  const handlingMarks = [];
  for (let tree = 0; tree < maxTrees && logsLoaded * HARDWOOD_UNITS < BED_UNITS; tree++) {
    const here = truckAt() ?? park0;
    const found = on({ verb: "treefall", words: ["find", "300", "hardwood"] }, 20000);
    const m = /key=(\S+) pos=(-?[\d.e+-]+) (-?[\d.e+-]+) (-?[\d.e+-]+)/.exec(String(found.detail));
    if (!m) {
      step(`${tag}1a a standing hardwood tree is found (tree ${tree + 1})`, false, String(found.detail).slice(0, 120));
      break;
    }
    const [key, tx, ty, tz] = [m[1], Number(m[2]), Number(m[3]), Number(m[4])];
    // Drive to within 9 m of the trunk: by road to the nearest clear road point, then across the ground (a straight
    // line from the saw runs through the woodlot sheds and wood piles).
    const toTree = driveLeg(`to_tree_${tree + 1}`, "road", [tx - 9, tz], 5, 25);
    if (tree === 0) step(`${tag}1a the truck is driven off the road to a hardwood tree`, reached(toTree), `${toTree.status} ${toTree.driven} m in ${toTree.time} s, tree at ${tx.toFixed(0)},${tz.toFixed(0)}`);
    leave();
    const truck = truckAt() ?? here;
    world("drive_haul_tree", tx, ty, tz, -2, 1.6, 0);
    wait(1500);
    const mark = log().length;
    on({ verb: "treefall", words: ["fell", key] }, 20000);
    let felled = "";
    for (let i = 0; i < 12 && !felled; i++) {
      wait(1000);
      felled = /\[LFDebug:Lumber\] felled \S+ \((\d+) logs?, (\w+)\)/.exec(log().slice(mark))?.[0] ?? "";
    }
    if (tree === 0) step(`${tag}1b a hardwood tree is felled`, /HARDWOOD/.test(felled), felled || "no `Lumber] felled` line");
    if (!felled) break;
    felledTrees++;
    chopSeconds += CHOP_SECONDS[Number(/\((\d+) logs?/.exec(felled)?.[1] ?? 2)] ?? 45;
    wait(5000);
    for (let n = 0; n < 4 && logsLoaded * HARDWOOD_UNITS < BED_UNITS; n++) {
      let hoisted = false;
      world("drive_haul_tree", tx, ty, tz, -2, 1.6, 0);
      wait(1200);
      for (let i = 0; i < 5 && !hoisted; i++) {
        const lg = on({ verb: "treefall", words: ["logs", "40"] }, 15000);
        const lm = /pos=(-?[\d.e+-]+) (-?[\d.e+-]+) (-?[\d.e+-]+)/.exec(String(lg.detail));
        if (!lm) {
          wait(2000);
          continue;
        }
        world("drive_haul_log", Number(lm[1]), Number(lm[2]), Number(lm[3]), -0.8, 0, 0);
        wait(1500);
        const hoist = on({ verb: "use", className: "LF_ShoulderHoistAction" }, 15000);
        wait(1200);
        hoisted = hoist.status === "ok" && carried("WoodLog") > 0;
      }
      if (!hoisted) break;
      loadedM += Math.hypot(tx - truck[0], tz - truck[2]);
      const before = logsLoaded;
      const ok = atTailgate(truck);
      if (!ok) {
        step(`${tag}1c log ${before + 1} loads at the tailgate of the driven truck`, false, "no standing point round the truck loaded it");
        break;
      }
      logsLoaded++;
      if (logsLoaded === 1) step(`${tag}1c the first hoisted log loads at the tailgate of the driven truck`, true, `from offset ${gatePoint.join(",")}`);
    }
  }
  const unitsLine = [...log().slice(serverMark).matchAll(/\[LFDebug:Cargo\] (?:un)?loaded \w+ \((\d+)\/\d+ units\)/g)].pop();
  const units = unitsLine ? Number(unitsLine[1]) : 0;
  step(`${tag}1d the bed holds the felled logs`, logsLoaded >= 1 && units === logsLoaded * HARDWOOD_UNITS, `${logsLoaded} log(s), ${units}/${BED_UNITS} units, ${felledTrees} tree(s)`);

  // 2. back to the saw by road (the way a player drives out of a stand), unload and saw.
  const back = driveLeg("tree_to_saw", "road", SAW_PARK, 4);
  step(`${tag}2a the loaded truck is driven back to the saw (arrived, or close beside the taken pad)`, reached(back), `${back.status} ${back.driven} m in ${back.time} s, ${back.err} m off the pad, ${back.backups} backups`);
  leave();
  const sawTruck = truckAt() ?? park0;
  let sawn = 0;
  for (let k = 0; k < 8 && sawn < logsLoaded; k++) {
    if (carried("WoodLog") < 1) {
      if (!atTailgate(sawTruck)) break;
    }
    on({ verb: "stand", entityName: SAW });
    wait(1000);
    const sawMark = log().length;
    const proc = on({ verb: "use", className: SAW_ACTION, entityName: SAW });
    wait(1500);
    let out = "";
    for (let i = 0; i < 40 && !out; i++) {
      out = /\[LFDebug:Processing\] processed [^\n]*x(\d+)[^\n]*/.exec(log().slice(sawMark))?.[0] ?? "";
      if (!out) wait(1000);
    }
    if (proc.status !== "ok" || !out) break;
    sawn++;
  }
  step(`${tag}2b every log is unloaded at the saw and sawn`, sawn >= 1 && sawn === logsLoaded, `${sawn} of ${logsLoaded} log(s) sawn`);

  // Sell: boards at the woodlot buyer (premium purse), what it refuses at the wholesale counter beside it.
  const cashBefore = cash();
  let premium = 0;
  let wholesale = 0;
  on({ verb: "stand", entityName: SAW });
  wait(1200);
  for (const item of ["HardwoodBoard", "WoodPlank", "Firewood"]) {
    for (let p = 0; p < 16; p++) {
      on({ verb: "pickup", item }, 15000);
      wait(700);
      if (carried(item) < 1) break;
      for (const [buyer, isPremium] of [[BUYER, true], [WHOLESALE, false]]) {
        if (carried(item) < 1) break;
        on({ verb: "stand", entityName: buyer });
        wait(1200);
        for (let press = 0; press < 12 && carried(item) > 0; press++) {
          const before = cash();
          const sell = on({ verb: "use", className: "LF_TraderSellAction", entityName: buyer });
          wait(1200);
          if (sell.status !== "ok" || cash() - before <= 0) break;
          if (isPremium) premium++;
          else wholesale++;
        }
      }
      on({ verb: "stand", entityName: SAW });
      wait(1000);
    }
  }
  const earned = cash() - cashBefore;
  step(`${tag}2c the boards sell at the woodlot buyers`, earned > 0, `+$${earned}: ${premium} premium + ${wholesale} wholesale sale(s)`);
  const demand = [...log().slice(serverMark).matchAll(/\[LFDebug:Market\] demand hardware\/[^\n]*HardwoodBoard[^\n]* (\d+)\/(\d+) \(sold (\d+), refused (\d+), paid (\d+)\)/g)].pop();
  if (demand) console.log(`premium purse after the cycle: ${demand[1]}/${demand[2]} boards`);

  // 3. the merchant timber counter at the hardware store, 190 m east: a second batch of boards in the pockets, driven there.
  let merchantEarned = 0;
  let merchantLeg = null;
  if (!args.includes("--no-merchant")) {
    on({ verb: "itemgive", prefab: "{D2A8639FA0CA09EF}Prefabs/Resources/WoodCutting/HardwoodBoard.et" }, 10000);
    on({ verb: "itemgive", prefab: "{D2A8639FA0CA09EF}Prefabs/Resources/WoodCutting/HardwoodBoard.et" }, 10000);
    on({ verb: "itemgive", prefab: "{D2A8639FA0CA09EF}Prefabs/Resources/WoodCutting/HardwoodBoard.et" }, 10000);
    const have = carried("HardwoodBoard");
    console.log(`boards in the pockets for the merchant: ${have}`);
    const p = sawTruck;
    on({ verb: "vehicle", words: ["enter", "driver", `at=${p[0]},${p[2]}`, "r=12"] }, 15000);
    wait(1500);
    merchantLeg = driveLeg("saw_to_merchant", "road", MERCHANT_PARK, 4);
    step(`${tag}3a the truck is driven from the saw to the timber counter at the hardware store (arrived or close)`, reached(merchantLeg), `${merchantLeg.status} ${merchantLeg.driven} m in ${merchantLeg.time} s, ${merchantLeg.err} m off the pad`);
    leave();
    on({ verb: "stand", entityName: MERCHANT });
    wait(1500);
    const c0 = cash();
    const s1 = on({ verb: "use", className: "LF_TraderSellAction", entityName: MERCHANT });
    wait(1500);
    merchantEarned = cash() - c0;
    step(`${tag}3b the timber counter pays for the boards`, s1.status === "ok" && merchantEarned > 0, `${s1.status} ${s1.detail}; +$${merchantEarned}`);
    const mt = truckAt() ?? p;
    world("drive_haul_merchant", mt[0], mt[1], mt[2], 6, 1.6, 6);
  }

  // 4. minutes and dollars. Drive legs are the game's own clock (the truck is really driven); the rest is the
  // truck-haul-proof model (the harness teleports): walking, carrying under a log, felling holds.
  const cycleLegs = legs.slice(legsBefore).filter((l) => /^(to_tree|tree_to_saw)/.test(l.label));
  const driveSec = cycleLegs.reduce((s, l) => s + l.time, 0);
  const handlingMin = (walkM / JOG_MPS + loadedM / LOADED_MPS + chopSeconds) / 60;
  const cycleMin = driveSec / 60 + handlingMin;
  const total = earned;
  console.log(`RUN  ${tag}${logsLoaded} logs loaded, ${sawn} sawn, ${felledTrees} tree(s); drive legs ${driveSec.toFixed(0)} s (${cycleLegs.map((l) => `${l.label} ${l.driven.toFixed(0)} m ${l.time.toFixed(0)} s`).join(", ")}); handling ${handlingMin.toFixed(1)} min (carry ${Math.round(loadedM)} m, chop ${chopSeconds} s); wall clock ${((Date.now() - startedAt) / 60000).toFixed(1)} min`);
  console.log(`RUN  ${tag}woodlot sales +$${total}, merchant +$${merchantEarned}: cycle ${cycleMin.toFixed(1)} min, $${dollarsPerHour(total + merchantEarned, cycleMin)}/h (woodlot only $${dollarsPerHour(total, cycleMin)}/h)`);
  step(`${tag}4 the run's minutes and dollars are measured`, total > 0 && cycleMin > 0, `${cycleMin.toFixed(1)} min, +$${total + merchantEarned}, $${dollarsPerHour(total + merchantEarned, cycleMin)}/h`);
  }

  let econ = null;
  try {
    econ = parseEconomy(execFileSync("node", [join(root, "../../tools/cli.mjs"), "economy"], { encoding: "utf8", timeout: 120000 }));
  } catch (e) {
    console.log(`economy: ${e.message.split("\n")[0]}`);
  }
  console.log(`RUN  tools\\cli economy hardwood by truck: ${econ ? `steady $${econ.steady}/h, first hour $${econ.first}/h` : "row not read"}`);
  // Every driven leg, and how many ended stuck (or out of time) instead of at the goal or close to it.
  const ends = stuckTally(legs);
  console.log(`STUCK ${ends.stuck} of ${ends.legs} driven legs (${ends.close} close, ${ends.arrived} arrived): ${legs.map((l) => `${l.label}=${l.status}${l.status === "arrived" ? "" : `(${l.err} m)`}`).join(" ")}`);
  return finish(results);
}

// Legs by how they ended: arrived, close (stopped within `near` of a goal it could not reach), or stuck/timeout.
export function stuckTally(legs) {
  const out = { legs: legs.length, arrived: 0, close: 0, stuck: 0 };
  for (const l of legs) {
    if (l.status === "arrived") out.arrived++;
    else if (l.status === "close") out.close++;
    else out.stuck++;
  }
  return out;
}

// A leg that got there: at the goal, or close (the goal's own spot taken, the last metres shut) within its `near`.
export const reached = (leg) => /^(arrived|close)$/.test(String(leg?.status ?? ""));

function finish(results) {
  const failed = results.filter((r) => !r.ok);
  console.log(failed.length ? `drive-haul-proof: FAIL (${failed.length} of ${results.length}: ${failed.map((r) => r.name).join("; ")})` : `drive-haul-proof: PASS (${results.length} of ${results.length})`);
  return failed.length ? 1 : 0;
}

export default {
  name: "drive-haul-proof",
  // Straight after truck-check-proof the character holds the truck's key: the per-proof reset would clear it.
  noReset: true,
  usage: "drive-haul-proof [--cycles N] [--trees N] [--no-merchant] [--selftest]   (after truck-check-proof on `up --mission island --profile full`; the truck is driven)",
  run: (ctx) => run(ctx?.args ?? []),
};
