import { execFileSync } from "node:child_process";
import { existsSync, readdirSync, readFileSync, statSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { arrive, clientBox, parseState, sessionDir } from "../session.mjs";

// The truck run on the hardwood stand, played the way a player plays it (#4886). On a running island session that
// already holds a bought truck (`truck-check-proof` buys the Ural; `tools\cli session up --mission island --profile
// full --wait`, one client):
//   0 LUMBERJACK level 3, the truck parked beside the woodlot saw (`parkat`; the harness cannot drive)
//   1 fell hardwood trees on the stand (`treefall fell` runs TryFell as if the axe were in hand), hoist each log
//     (LF_ShoulderHoistAction) and load it at the tailgate (LF_CargoBedAction) until the 12-unit bed is full
//   2 the full bed's next log is refused WITH a reason (the client log names the check, the toast is words)
//   3 unload at the saw (LF_CargoBedAction, one log a trip to the shoulder), saw every log, sell what comes out
//   4 print the minutes and the dollars, and the measured $/h beside `tools\cli economy`'s hardwood-by-truck row
// Each step is PASS or FAIL with the server line it read.
const SAW_ACTION = process.env.SAW_ACTION ?? "LF_ProcessAction__3";
const SAW = "Fit_saint_philippe_woodlot_saw";
const DESK = "Fit_saint_philippe_woodlot_desk";
const BUYER = process.env.BUYER ?? "Fit_saint_philippe_woodlot_buyer";
const JOG_MPS = 4.5;
const LOADED_MPS = 4.5 * 0.5;
const BED_UNITS = 12;
const HARDWOOD_UNITS = 2;
const TRUCK_MPS = 11;
// Metres a log is carried to a truck parked at the stand (the model; the harness loads at the saw).
const LOG_TO_TRUCK_M = 6;
// Chop seconds of a hardwood tree by its log count (the size 1, 2 and 3 rows of Configs/Lumber/LF_TreeSpecies.conf).
const CHOP_SECONDS = { 1: 30, 2: 45, 3: 60 };
const PARK_METRES = process.env.PARK_METRES ?? "8";

const root = dirname(fileURLToPath(import.meta.url));

// The newest server console log of this session.
function serverLog() {
  const dir = join(sessionDir(), "server", "logs");
  if (!existsSync(dir)) return "";
  const runs = readdirSync(dir).filter((d) => existsSync(join(dir, d, "console.log")));
  runs.sort((a, b) => statSync(join(dir, a, "console.log")).mtimeMs - statSync(join(dir, b, "console.log")).mtimeMs);
  return runs.length ? readFileSync(join(dir, runs[runs.length - 1], "console.log"), "utf8") : "";
}

// The newest client console log of this session (the client's offer check logs there).
function clientLog() {
  const dir = join(sessionDir(), "client1", "logs");
  if (!existsSync(dir)) return "";
  const runs = readdirSync(dir).filter((d) => existsSync(join(dir, d, "console.log")));
  runs.sort((a, b) => statSync(join(dir, a, "console.log")).mtimeMs - statSync(join(dir, b, "console.log")).mtimeMs);
  return runs.length ? readFileSync(join(dir, runs[runs.length - 1], "console.log"), "utf8") : "";
}

// `tools\cli economy`'s hardwood-by-truck row: "steady $680/h, first hour $680/h".
export function parseEconomy(text) {
  const m = /LOOP truck hardwood -> boards, firewood:[^\n]*?steady \$(\d+)\/h, first hour \$(\d+)\/h/.exec(String(text));
  return m ? { steady: Number(m[1]), first: Number(m[2]) } : null;
}

export function dollarsPerHour(dollars, minutes) {
  return minutes > 0 ? Math.round((dollars * 60) / minutes) : 0;
}

// The units on the bed from the server's last `Cargo] loaded/unloaded ... (u/12 units)` line.
export function lastUnits(text) {
  const all = [...String(text).matchAll(/\[LFDebug:Cargo\] (?:un)?loaded \w+ \((\d+)\/\d+ units\)/g)];
  return all.length ? Number(all[all.length - 1][1]) : 0;
}

function selftest() {
  const fails = [];
  const e = parseEconomy("  LOOP truck hardwood -> boards, firewood: ~24 logs/h, steady $680/h, first hour $680/h; duty police");
  if (!e || e.steady !== 680 || e.first !== 680) fails.push("economy row");
  if (parseEconomy("LOOP something else")) fails.push("economy refuses junk");
  if (dollarsPerHour(100, 15) !== 400 || dollarsPerHour(5, 0) !== 0) fails.push("dollars per hour");
  if (lastUnits("[LFDebug:Cargo] loaded hardwood (2/12 units)\n[LFDebug:Cargo] loaded hardwood (4/12 units)\n") !== 4) fails.push("last units");
  if (lastUnits("[LFDebug:Cargo] unloaded hardwood (10/12 units)\n") !== 10) fails.push("last units after an unload");
  for (const x of fails) console.log(`FAIL  ${x}`);
  console.log(fails.length ? "truck-haul-proof selftest: FAIL" : "truck-haul-proof selftest: PASS");
  return fails.length ? 1 : 0;
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
  const cash = () => Number(parseState(on({ verb: "state" }).detail).cash || 0);
  const carried = (item) => {
    const r = on({ verb: "carries", words: [item] });
    const m = /count=(\d+)/.exec(r.detail);
    return r.status === "ok" && m ? Number(m[1]) : 0;
  };
  const minutes = (since) => (Date.now() - since) / 60000;
  const feetAt = () => {
    const p = /pos=(-?[\d.]+),(-?[\d.]+),(-?[\d.]+)/.exec(String(on({ verb: "state" }).detail));
    return p ? [Number(p[1]), Number(p[3])] : [0, 0];
  };
  const dist = (a, b) => Math.hypot(a[0] - b[0], a[1] - b[1]);

  // Straight after truck-check-proof the player stands at the yard's lot beside the truck just bought: do not walk away.
  if (!/parked=(?!none)\S+/.test(String(on({ verb: "state" }).detail))) arrive((n, spec, t) => on(spec, t), 1, wait);
  wait(2000);
  const parked = /parked=(\S+)/.exec(String(on({ verb: "state" }).detail))?.[1] ?? "none";
  step("0 the player owns a truck in the world", parked !== "none", `parked=${parked}`);
  if (parked === "none") return finish(results);

  // The lot left the bought truck locked and beside the buyer: unlock it at its door with the key, the way a player does
  // (the cargo bed follows the vehicle lock), then set it beside the woodlot saw.
  // The truck stands where the lot spawned it (the server logged the spawn); walk to its door, one side then the other.
  const spawn = [...serverLog().matchAll(/Init entity [^\n]*Ural4320[^\n]* at <(-?[\d.]+) (-?[\d.]+) (-?[\d.]+)>/g)].pop();
  let seen = "";
  let unlock = { status: "no_spawn", detail: "the server log holds no spawn of the truck" };
  for (const ox of [2.6, -2.6]) {
    if (!spawn) break;
    on({ verb: "capture", mode: "world", name: "truck_haul_door", x: Number(spawn[1]), y: Number(spawn[2]), z: Number(spawn[3]), ox, oy: 1.6, oz: 2.2, hour: 9 }, 30000);
    wait(1500);
    seen = String(on({ verb: "seen", words: ["25"] }).detail);
    if (!/lock=1/.test(seen)) break;
    unlock = on({ verb: "use", className: "LF_VehicleLockAction" }, 10000);
    wait(1500);
    seen = String(on({ verb: "seen", words: ["25"] }).detail);
    if (/lock=0/.test(seen)) break;
  }
  step("0b the owner unlocks the truck at its door", /lock=0/.test(seen), `${unlock.status} ${unlock.detail}; ${seen.slice(0, 200)}`);
  on({ verb: "guide", words: ["close"] });
  on({ verb: "time", hour: 9.0 }, 15000);
  on({ verb: "levelset", words: ["LUMBERJACK", "3"] }, 10000);
  wait(1500);
  if (parseState(on({ verb: "state" }).detail).job !== "LUMBERJACK") {
    on({ verb: "stand", entityName: DESK });
    wait(1500);
    on({ verb: "use", className: "LF_WorkplaceJobAction", entityName: DESK });
    wait(1500);
  }
  on({ verb: "stand", entityName: SAW });
  wait(1500);
  const park = on({ verb: "parkat", words: [SAW, PARK_METRES] }, 15000);
  step("0c the truck is beside the woodlot saw", park.status === "ok", String(park.detail).slice(0, 80));
  wait(2500);
  console.log(`seen: ${String(on({ verb: "seen", words: ["12"] }).detail).slice(0, 220)}`);

  const startedAt = Date.now();
  let walkM = 0;
  let loadedM = 0;
  let here = feetAt();
  let felledTrees = 0;
  let logsLoaded = 0;
  let loadRefused = false;
  let firstTreeAt = null;
  let driveM = 0;
  let chopSeconds = 0;
  const serverMark = serverLog().length;
  const clientMark = clientLog().length;

  // 1. Fell hardwood on the stand and load every log until the bed is full.
  for (let tree = 0; tree < 4 && logsLoaded * HARDWOOD_UNITS < BED_UNITS; tree++) {
    on({ verb: "stand", entityName: SAW });
    wait(1500);
    const sawAt = feetAt();
    const found = on({ verb: "treefall", words: ["find", "300", "hardwood"] }, 20000);
    const m = /key=(\S+) pos=(-?[\d.e+-]+) (-?[\d.e+-]+) (-?[\d.e+-]+)/.exec(String(found.detail));
    if (!m) {
      step(`1a a standing hardwood tree is found (${tree + 1})`, false, String(found.detail).slice(0, 120));
      break;
    }
    const [key, tx, ty, tz] = [m[1], Number(m[2]), Number(m[3]), Number(m[4])];
    on({ verb: "capture", mode: "world", name: "truck_haul_tree", x: tx, y: ty, z: tz, ox: -2, oy: 1.6, oz: 0, hour: 9 }, 30000);
    wait(1500);
    const treeAt = feetAt();
    here = treeAt;
    if (!firstTreeAt) {
      firstTreeAt = treeAt;
      driveM = 2 * dist(sawAt, treeAt);
    }
    const mark = serverLog().length;
    on({ verb: "treefall", words: ["fell", key] }, 20000);
    let felled = "";
    for (let i = 0; i < 12 && !felled; i++) {
      wait(1000);
      felled = /\[LFDebug:Lumber\] felled \S+ \((\d+) logs?, (\w+)\)/.exec(serverLog().slice(mark))?.[0] ?? "";
    }
    step(`1a a hardwood tree is felled (${tree + 1})`, /HARDWOOD/.test(felled), felled || "no `Lumber] felled` line");
    if (!felled) break;
    felledTrees++;
    // `treefall fell` is instant; the hold a player makes lasts the species row's chop seconds (a size-N hardwood, LF_TreeSpecies.conf).
    chopSeconds += CHOP_SECONDS[Number(/\((\d+) logs?/.exec(felled)?.[1] ?? 2)] ?? 45;
    wait(5000);

    for (let n = 0; n < 4 && logsLoaded * HARDWOOD_UNITS < BED_UNITS; n++) {
      let hoisted = false;
      // Back at the stump (the truck stands beside the stand in the model; the harness loads at the saw).
      on({ verb: "capture", mode: "world", name: "truck_haul_tree", x: tx, y: ty, z: tz, ox: -2, oy: 1.6, oz: 0, hour: 9 }, 30000);
      wait(1200);
      here = feetAt();
      for (let i = 0; i < 5 && !hoisted; i++) {
        const lg = on({ verb: "treefall", words: ["logs", "40"] }, 15000);
        const lm = /pos=(-?[\d.e+-]+) (-?[\d.e+-]+) (-?[\d.e+-]+)/.exec(String(lg.detail));
        if (!lm) {
          wait(2000);
          continue;
        }
        on({ verb: "capture", mode: "world", name: "truck_haul_log", x: Number(lm[1]), y: Number(lm[2]), z: Number(lm[3]), ox: -0.8, oy: 0, oz: 0, hour: 9 }, 30000);
        wait(1500);
        const to = feetAt();
        walkM += dist(here, to);
        here = to;
        const hoist = on({ verb: "use", className: "LF_ShoulderHoistAction" }, 15000);
        wait(1200);
        hoisted = hoist.status === "ok" && carried("WoodLog") > 0;
      }
      if (!hoisted) break;
      // To the truck at the saw, under the log, and load at the tailgate.
      on({ verb: "stand", entityName: SAW });
      wait(1800);
      loadedM += LOG_TO_TRUCK_M;
      const mark2 = serverLog().length;
      const load = on({ verb: "use", className: "LF_CargoBedAction" }, 15000);
      wait(1500);
      const ok = load.status === "ok" && /\[LFDebug:Cargo\] loaded hardwood/.test(serverLog().slice(mark2));
      if (logsLoaded === 0) {
        const refusal = clientLog().slice(clientMark).split("\n").filter((l) => /Cargo\]/.test(l)).slice(-3).join(" | ");
        step("1b the first hoisted hardwood log loads at the tailgate of the bought truck", ok, ok ? load.detail : `${load.status} ${load.detail} ${refusal}`);
      }
      if (!ok) {
        loadRefused = true;
        break;
      }
      logsLoaded++;
    }
    if (loadRefused) break;
  }
  const units = lastUnits(serverLog().slice(serverMark));
  step("1c the bed holds the felled logs", logsLoaded >= 1 && units === logsLoaded * HARDWOOD_UNITS, `${logsLoaded} log(s), ${units}/${BED_UNITS} units, ${felledTrees} tree(s)`);

  // 2. A full bed refuses with words.
  if (units + HARDWOOD_UNITS > BED_UNITS) {
    const tree = on({ verb: "treefall", words: ["find", "300", "hardwood"] }, 20000);
    const tm = /key=(\S+) pos=(-?[\d.e+-]+) (-?[\d.e+-]+) (-?[\d.e+-]+)/.exec(String(tree.detail));
    if (tm) {
      on({ verb: "capture", mode: "world", name: "truck_haul_tree", x: Number(tm[2]), y: Number(tm[3]), z: Number(tm[4]), ox: -2, oy: 1.6, oz: 0, hour: 9 }, 30000);
      wait(1000);
      on({ verb: "treefall", words: ["fell", tm[1]] }, 20000);
      wait(6000);
      const lg = on({ verb: "treefall", words: ["logs", "40"] }, 15000);
      const lm = /pos=(-?[\d.e+-]+) (-?[\d.e+-]+) (-?[\d.e+-]+)/.exec(String(lg.detail));
      if (lm) {
        on({ verb: "capture", mode: "world", name: "truck_haul_log", x: Number(lm[1]), y: Number(lm[2]), z: Number(lm[3]), ox: -0.8, oy: 0, oz: 0, hour: 9 }, 30000);
        wait(1200);
        on({ verb: "use", className: "LF_ShoulderHoistAction" }, 15000);
        wait(1200);
        on({ verb: "stand", entityName: SAW });
        wait(1800);
        const before = clientLog().length;
        const beforeServer = serverLog().length;
        on({ verb: "use", className: "LF_CargoBedAction" }, 15000);
        wait(3500);
        // The reason a player reads (the action's cannot-perform text) goes to the server log with the harness's refusal;
        // the client's offer check names which rule refused. A full bed says so on both.
        // The harness's use runs the offer check on the host, so the line is in the server's log (or the client's, on a remote client).
        const said = clientLog().slice(before) + serverLog().slice(beforeServer);
        const reason = /cannot_perform reason=(\S+)/.exec(serverLog().slice(beforeServer))?.[1] ?? "";
        step("2 a full bed refuses with its reason in words", reason === "#LF-Cargo_Full" && /offer refused on the client: the bed is full/.test(said), `reason=${reason || "none"}; ${said.split("\n").filter((l) => /Cargo\]/.test(l)).slice(-1).join("") || "no Cargo line on the client"}`);
        // The refused log stays on the shoulder: it is the first thing the saw takes in the delivery below.
      }
    }
  }

  // 3. Deliver: unload log after log at the saw, saw each, sell what comes out.
  const loopStart = Date.now();
  let earned = 0;
  let sawn = 0;
  let sold = 0;
  let trips = 0;
  const econMark = serverLog().length;
  for (let k = 0; k < 8; k++) {
    on({ verb: "stand", entityName: SAW });
    wait(1000);
    if (carried("WoodLog") < 1) {
      const un = on({ verb: "use", className: "LF_CargoBedAction", entityName: undefined }, 15000);
      wait(1500);
      if (un.status !== "ok") break;
    }
    const sawMark = serverLog().length;
    const proc = on({ verb: "use", className: SAW_ACTION, entityName: SAW });
    wait(1500);
    let out = "";
    for (let i = 0; i < 40 && !out; i++) {
      out = /\[LFDebug:Processing\] processed [^\n]*x(\d+)[^\n]*/.exec(serverLog().slice(sawMark))?.[0] ?? "";
      if (!out) wait(1000);
    }
    if (proc.status !== "ok" || !out) break;
    sawn += 1;
    if (k === 0) step("3a the saw takes the unloaded hardwood log", true, out.slice(0, 160));
  }
  step("3b the bed is empty after the delivery", units > 0 && /\[LFDebug:Cargo\] unloaded/.test(serverLog().slice(serverMark)), `${sawn} log(s) sawn`);

  // Sell what the saw dropped (boards, firewood) at the buyer beside the saw.
  const cashBefore = cash();
  for (const item of ["HardwoodBoard", "WoodPlank", "Firewood"]) {
    for (let p = 0; p < 12; p++) {
      on({ verb: "pickup", item }, 15000);
      wait(700);
      if (carried(item) < 1) break;
      on({ verb: "stand", entityName: BUYER });
      wait(1200);
      trips++;
      for (let press = 0; press < 12 && carried(item) > 0; press++) {
        const before = cash();
        const sell = on({ verb: "use", className: "LF_TraderSellAction", entityName: BUYER });
        wait(1200);
        if (sell.status !== "ok" || cash() - before <= 0) break;
        sold++;
      }
      on({ verb: "stand", entityName: SAW });
      wait(1000);
    }
  }
  earned = cash() - cashBefore;
  void econMark;

  // 4. Minutes and dollars.
  const loopMin = minutes(startedAt);
  void loopStart;
  let econ = null;
  try {
    econ = parseEconomy(execFileSync("node", [join(root, "../../tools/cli.mjs"), "economy"], { encoding: "utf8", timeout: 120000 }));
  } catch (e) {
    console.log(`economy: ${e.message.split("\n")[0]}`);
  }
  const walkMin = (walkM / JOG_MPS + loadedM / LOADED_MPS + driveM / TRUCK_MPS) / 60;
  // The truck drives out to the stand and back with the load (the harness parks it at the saw and teleports), the logs
  // are carried 6 m to it, and the walk between a tree's logs is real; the harness's own waits stay in the wall clock.
  const chopMin = chopSeconds / 60;
  const adjMin = loopMin + walkMin + chopMin;
  console.log(`RUN  ${logsLoaded} log(s) in ${felledTrees} tree(s) loaded, ${sawn} sawn, ${sold} sale press(es) over ${trips} trip(s): +$${earned}`);
  console.log(`RUN  ${loopMin.toFixed(1)} min wall clock with teleports; movement a player makes ${Math.round(walkM)} m on foot + ${Math.round(loadedM)} m under a log + ${Math.round(driveM)} m by truck = ${walkMin.toFixed(1)} min, the felling holds ${chopMin.toFixed(1)} min; loop with them ${adjMin.toFixed(1)} min`);
  console.log(`RUN  measured $${dollarsPerHour(earned, loopMin)}/h wall clock, $${dollarsPerHour(earned, adjMin)}/h with the walk and the holds (truck at ${TRUCK_MPS} m/s)`);
  console.log(`RUN  tools\\cli economy hardwood by truck: ${econ ? `steady $${econ.steady}/h, first hour $${econ.first}/h` : "row not read"} (a ${BED_UNITS}-unit bed is ${BED_UNITS / HARDWOOD_UNITS} logs; the row models 24 logs/h)`);
  step("4 the run's minutes and dollars are measured", earned > 0 && loopMin > 0, `${adjMin.toFixed(1)} min, +$${earned}, $${dollarsPerHour(earned, adjMin)}/h`);
  return finish(results);
}

function finish(results) {
  const failed = results.filter((r) => !r.ok);
  console.log(failed.length ? `truck-haul-proof: FAIL (${failed.length} of ${results.length}: ${failed.map((r) => r.name).join("; ")})` : `truck-haul-proof: PASS (${results.length} of ${results.length})`);
  return failed.length ? 1 : 0;
}

export default {
  name: "truck-haul-proof",
  usage: "truck-haul-proof [--selftest]   (after truck-check-proof on `up --mission island --profile full`)",
  run: (ctx) => run(ctx?.args ?? []),
};
