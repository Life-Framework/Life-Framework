import { execFileSync } from "node:child_process";
import { existsSync, readdirSync, readFileSync, statSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { arrive, clientBox, parseState, sessionDir } from "../session.mjs";
import { AMOUNT, parseMoney } from "./_money.mjs";

// The truck run on the hardwood stand, played the way a player plays it (#4886). On a running island session that
// already holds a bought truck (`truck-check-proof` buys the first row of the yard, now the Cargo truck; `tools\cli session up --mission island --profile
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
const IMPOUND_DESK = process.env.IMPOUND_DESK ?? "Fit_saint_philippe_carlot_impound_desk";
const BUYER = process.env.BUYER ?? "Fit_saint_philippe_woodlot_buyer";
const WHOLESALE = process.env.WHOLESALE ?? "Fit_saint_philippe_woodlot_wholesale";
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

// Every server console log of this session, oldest first: after `session restart` the purchase is in the log of the
// server before the restart (#4954).
function serverLog() {
  const dir = join(sessionDir(), "server", "logs");
  if (!existsSync(dir)) return "";
  const runs = readdirSync(dir).filter((d) => existsSync(join(dir, d, "console.log")));
  runs.sort((a, b) => statSync(join(dir, a, "console.log")).mtimeMs - statSync(join(dir, b, "console.log")).mtimeMs);
  return runs.map((d) => readFileSync(join(dir, d, "console.log"), "utf8")).join("\n");
}

// Where the owned truck stands now, from `state`'s parkedat=x,y,z (#4954). Returns [x, y, z] as strings, or null.
export function parkedAt(detail) {
  const m = /(?:^|\s)parkedat=(-?[\d.]+),(-?[\d.]+),(-?[\d.]+)/.exec(String(detail));
  return m ? [m[1], m[2], m[3]] : null;
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
  const m = new RegExp(String.raw`LOOP truck hardwood -> boards, firewood:[^\n]*?steady \$(${AMOUNT})\/h, first hour \$(${AMOUNT})\/h`).exec(String(text));
  return m ? { steady: parseMoney(m[1]), first: parseMoney(m[2]) } : null;
}

export function dollarsPerHour(dollars, minutes) {
  return minutes > 0 ? Math.round((dollars * 60) / minutes) : 0;
}

// The units on the bed from the server's last `Cargo] loaded/unloaded ... (u/12 units)` line.
export function lastUnits(text) {
  const all = [...String(text).matchAll(/\[LFDebug:Cargo\] (?:un)?loaded \w+ \((\d+)\/\d+ units\)/g)];
  return all.length ? Number(all[all.length - 1][1]) : 0;
}

// The lock flag ("0" open, "1" locked, "?" no vehicle) of the nearest vehicle in a `seen` reply: the entries are joined by
// "|", nearest first, and the lot's display trucks stand locked a few metres away.
export function nearestLock(text) {
  return /veh=[^ ]*?:lock=(\d)/.exec(String(text))?.[1] ?? "?";
}

// Where the truck the buyer just bought was spawned, from the server log. The folder of the catalog prefab on the last
// `[LFDebug:Vehicle] purchased <prefab> owner=` line (M923A1 for the Cargo truck, Ural4320 for the Ural: the lot's first
// row is not always the same truck) names the family, and the nearest `Init entity ... <family> ... at <x y z>` line
// before the purchase is the spawn. Returns [x, y, z] as strings, or null with no purchase line.
export function boughtTruckSpawn(text) {
  const lines = String(text).split(/\r?\n/);
  let bought = -1;
  let family = "";
  lines.forEach((line, i) => {
    const m = /\[LFDebug:Vehicle\] purchased (\S+) owner=/.exec(line);
    const folder = m ? /([^/\\]+)[/\\][^/\\]*$/.exec(m[1]) : null;
    if (folder) {
      bought = i;
      family = folder[1];
    }
  });
  if (!family) return null;
  const spawn = new RegExp(`Init entity [^\\n]*${family.replace(/[.*+?^${}()|[\]\\]/g, "\\$&")}[^\\n]* at <(-?[\\d.]+) (-?[\\d.]+) (-?[\\d.]+)>`);
  for (let i = bought; i >= 0; i--) {
    const m = spawn.exec(lines[i]);
    if (m) return [m[1], m[2], m[3]];
  }
  return null;
}

function selftest() {
  const fails = [];
  const spawnLine = (family, x) => `01:11:12.597     ENTITY       : Init entity @"ENTITY:279" ('EL_${family}', Vehicle) at <${x}.5 14.1 10632.1> @"{E1A923BE1A000001}Prefabs/Vehicles/Wheeled/${family}/EL_${family}_transport.et"\n`;
  const boughtLine = (family) => `01:11:13.000 SCRIPT: [LFDebug:Vehicle] purchased {F1FBD0972FA5FE09}Prefabs/Vehicles/Wheeled/${family}/${family}_transport.et owner=u1 id=7 plate=CD MU 3665 cash=0 bank=9600\n`;
  const cargo = spawnLine("Ural4320", 4000) + spawnLine("M923A1", 4333) + boughtLine("M923A1");
  if (boughtTruckSpawn(cargo)?.join(",") !== "4333.5,14.1,10632.1") fails.push("the Cargo truck's spawn is read, not the Ural row's");
  if (boughtTruckSpawn(spawnLine("M923A1", 4333) + boughtLine("Ural4320") + spawnLine("Ural4320", 4400)) !== null) fails.push("a spawn after the purchase is not the bought truck");
  if (boughtTruckSpawn(spawnLine("Ural4320", 4000) + boughtLine("Ural4320"))?.[0] !== "4000.5") fails.push("a bought Ural is still found");
  if (boughtTruckSpawn(spawnLine("M923A1", 4333)) !== null) fails.push("no purchase line, no truck");
  if (parkedAt("jail=0 garage=0 parked=UI_OZ_2619 stash=-1 parkedat=4333.5,14.1,10632.1")?.join(",") !== "4333.5,14.1,10632.1") fails.push("parkedat is read");
  if (parkedAt("parked=none stash=-1 parkedat=none") !== null) fails.push("parkedat=none is no position");
  if (nearestLock("veh=EL_M923A1_transport:d=4.6:eng=0:lock=0:ign=0|Ural:d=9:eng=0:lock=1 door=none") !== "0") fails.push("nearestLock reads the first vehicle");
  if (nearestLock("veh=EL_M923A1_transport:d=2:eng=0:lock=1|Ural:d=9:lock=0") !== "1" || nearestLock("veh=none") !== "?") fails.push("nearestLock locked and none");
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
  let stateLine = String(on({ verb: "state" }).detail);
  const parked = /parked=(\S+)/.exec(stateLine)?.[1] ?? "none";
  // A restart impounds every car left on the street (LF_VehicleOwnership.ImpoundStreetVehicles): the truck is still
  // owned (parked=<plate>) but has no entity (parkedat=none). Collect it at the impound desk, as a player does (#4954).
  if (parked !== "none" && !parkedAt(stateLine)) {
    on({ verb: "stand", entityName: IMPOUND_DESK });
    wait(1500);
    const collect = on({ verb: "use", className: "LF_ImpoundCollectAction", entityName: IMPOUND_DESK }, 10000);
    wait(3000);
    stateLine = String(on({ verb: "state" }).detail);
    step("0a the truck impounded at the restart is collected at the desk", !!parkedAt(stateLine), `${collect.status} ${collect.detail}; parkedat=${parkedAt(stateLine)?.join(",") ?? "none"}`);
  }
  step("0 the player owns a truck in the world", parked !== "none", `parked=${parked}`);
  if (parked === "none") return finish(results);

  // The lot left the bought truck locked and beside the buyer: unlock it at its door with the key, the way a player does
  // (the cargo bed follows the vehicle lock), then set it beside the woodlot saw.
  // The truck stands where the lot spawned it (the server logged the spawn); walk to its door, one side then the other.
  // Where it stands now first (it may have moved, or come back from a save), else the spawn the server logged.
  const spawn = parkedAt(stateLine) ?? boughtTruckSpawn(serverLog());
  let seen = "";
  let unlock = { status: "no_spawn", detail: "neither state nor the server logs place the truck" };
  // The nearest vehicle's lock: `seen` lists vehicles nearest first, and the lot's display trucks stand locked nearby.
  // Camera offsets from the spawn point, in world axes: the spawn is the truck's origin, not a door, and the truck's heading
  // is unknown to the proof (the Ural's doors were 2.6 east and 2.2 south of it; the Cargo truck is longer and wider, its
  // doors are further out). Two rings round the origin, nearest ring first, stop at the first offset that unlocks it.
  const OFFSETS = [[2.6, 1.6, 2.2], [-2.6, 1.6, 2.2]]; // where the lot spawns it beside the buyer: the first two tries
  for (const r of [2.8, 4.2]) for (let deg = 0; deg < 360; deg += 45) OFFSETS.push([Number((r * Math.cos(deg * Math.PI / 180)).toFixed(2)), 0.3, Number((r * Math.sin(deg * Math.PI / 180)).toFixed(2))]);
  for (const [ox, oy, oz] of OFFSETS) {
    if (!spawn) break;
    on({ verb: "capture", mode: "world", name: "truck_haul_door", x: Number(spawn[0]), y: Number(spawn[1]), z: Number(spawn[2]), ox, oy, oz, hour: 9 }, 30000);
    wait(1500);
    seen = String(on({ verb: "seen", words: ["25"] }).detail);
    if (nearestLock(seen) === "0") break;
    unlock = on({ verb: "use", className: "LF_VehicleLockAction" }, 10000);
    wait(1500);
    seen = String(on({ verb: "seen", words: ["25"] }).detail);
    if (nearestLock(seen) === "0") break;
  }
  step("0b the owner unlocks the truck at its door", nearestLock(seen) === "0", `${unlock.status} ${unlock.detail}; ${seen.slice(0, 200)}`);
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

  // `--cycles N` (#4898): N cycles in a row to the same buyers, so the premium purse falls the way it does over an hour.
  const ci = args.indexOf("--cycles");
  const cycles = Math.max(1, Math.min(12, ci >= 0 ? Number(args[ci + 1]) || 1 : 1));
  const runs = [];
  for (let c = 1; c <= cycles; c++) {
    const r = cycle(c, c === 1);
    runs.push(r);
    console.log(`CYCLE ${c}  ${r.logsLoaded} log(s) loaded, ${r.sawn} sawn, ${r.premium} premium + ${r.wholesale} wholesale sale(s): +$${r.earned} in ${r.adjMin.toFixed(1)} min ($${dollarsPerHour(r.earned, r.adjMin)}/h)`);
    if (!r.logsLoaded || !r.earned) break;
  }

  function cycle(c, first) {
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
    for (let tree = 0; tree < 5 && logsLoaded * HARDWOOD_UNITS < BED_UNITS; tree++) {
      on({ verb: "stand", entityName: SAW });
      wait(1500);
      const sawAt = feetAt();
      const found = on({ verb: "treefall", words: ["find", "300", "hardwood"] }, 20000);
      const m = /key=(\S+) pos=(-?[\d.e+-]+) (-?[\d.e+-]+) (-?[\d.e+-]+)/.exec(String(found.detail));
      if (!m) {
        step(`1a a standing hardwood tree is found (cycle ${c}, tree ${tree + 1})`, false, String(found.detail).slice(0, 120));
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
      if (first) step(`1a a hardwood tree is felled (${tree + 1})`, /HARDWOOD/.test(felled), felled || "no `Lumber] felled` line");
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
        if (logsLoaded === 0 && first) {
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
    step(`1c the bed holds the felled logs (cycle ${c})`, logsLoaded >= 1 && units === logsLoaded * HARDWOOD_UNITS, `${logsLoaded} log(s), ${units}/${BED_UNITS} units, ${felledTrees} tree(s)`);

    // 2. A full bed refuses with words (the first cycle only; the refused log rides on the shoulder to the saw).
    if (first && units + HARDWOOD_UNITS > BED_UNITS) {
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
          const said = clientLog().slice(before) + serverLog().slice(beforeServer);
          const reason = /cannot_perform reason=(\S+)/.exec(serverLog().slice(beforeServer))?.[1] ?? "";
          step("2 a full bed refuses with its reason in words", reason === "#LF-Cargo_Full" && /offer refused on the client: the bed is full/.test(said), `reason=${reason || "none"}; ${said.split("\n").filter((l) => /Cargo\]/.test(l)).slice(-1).join("") || "no Cargo line on the client"}`);
        }
      }
    }

    // 3. Deliver: unload log after log at the saw, saw each.
    let sawn = 0;
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
      if (k === 0 && first) step("3a the saw takes the unloaded hardwood log", true, out.slice(0, 160));
    }
    step(`3b the bed is empty after the delivery (cycle ${c})`, units > 0 && /\[LFDebug:Cargo\] unloaded/.test(serverLog().slice(serverMark)), `${sawn} log(s) sawn`);

    // Sell what the saw dropped at the premium buyer beside the saw; what its spent purse refuses goes to the wholesale
    // counter beside it (the player reads "they have enough" and walks the 3 m).
    const cashBefore = cash();
    let premium = 0;
    let wholesale = 0;
    let trips = 0;
    for (const item of ["HardwoodBoard", "WoodPlank", "Firewood"]) {
      for (let p = 0; p < 16; p++) {
        on({ verb: "pickup", item }, 15000);
        wait(700);
        if (carried(item) < 1) break;
        for (const [buyer, isPremium] of [[BUYER, true], [WHOLESALE, false]]) {
          if (carried(item) < 1) break;
          on({ verb: "stand", entityName: buyer });
          wait(1200);
          trips++;
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
    const demand = [...serverLog().slice(serverMark).matchAll(/\[LFDebug:Market\] demand hardware\/[^\n]*HardwoodBoard[^\n]* (\d+)\/(\d+) \(sold (\d+), refused (\d+), paid (\d+)\)/g)].pop();
    if (demand) console.log(`CYCLE ${c}  premium purse after the cycle: ${demand[1]}/${demand[2]} boards`);

    const loopMin = minutes(startedAt);
    // The truck drives out to the stand and back with the load (the harness parks it at the saw and teleports), the logs
    // are carried 6 m to it, and the walk between a tree's logs is real; the harness's own waits stay in the wall clock.
    const walkMin = (walkM / JOG_MPS + loadedM / LOADED_MPS + driveM / TRUCK_MPS) / 60;
    const chopMin = chopSeconds / 60;
    const adjMin = loopMin + walkMin + chopMin;
    console.log(`CYCLE ${c}  ${loopMin.toFixed(1)} min wall clock + ${walkMin.toFixed(1)} min movement (${Math.round(walkM)} m on foot, ${Math.round(loadedM)} m under a log, ${Math.round(driveM)} m by truck) + ${chopMin.toFixed(1)} min felling holds; ${felledTrees} tree(s), ${trips} buyer trip(s)`);
    return { logsLoaded, sawn, premium, wholesale, earned, adjMin, loopMin };
  }

  // 4. Minutes and dollars over every cycle.
  let econ = null;
  try {
    econ = parseEconomy(execFileSync("node", [join(root, "../../tools/cli.mjs"), "economy"], { encoding: "utf8", timeout: 120000 }));
  } catch (e) {
    console.log(`economy: ${e.message.split("\n")[0]}`);
  }
  const earned = runs.reduce((s, r) => s + r.earned, 0);
  const adjMin = runs.reduce((s, r) => s + r.adjMin, 0);
  const loopMin = runs.reduce((s, r) => s + r.loopMin, 0);
  const logs = runs.reduce((s, r) => s + r.sawn, 0);
  console.log(`RUN  ${runs.length} cycle(s), ${logs} log(s) sawn, +$${earned} in ${adjMin.toFixed(1)} min (${loopMin.toFixed(1)} wall clock): $${dollarsPerHour(earned, adjMin)}/h with the walk and the holds (truck at ${TRUCK_MPS} m/s), ${(adjMin / runs.length).toFixed(1)} min a cycle`);
  console.log(`RUN  per cycle: ${runs.map((r) => `$${r.earned}`).join(", ")}`);
  console.log(`RUN  tools\\cli economy hardwood by truck: ${econ ? `steady $${econ.steady}/h, first hour $${econ.first}/h` : "row not read"}`);
  step("4 the run's minutes and dollars are measured", earned > 0 && adjMin > 0, `${adjMin.toFixed(1)} min, +$${earned}, $${dollarsPerHour(earned, adjMin)}/h`);
  return finish(results);
}

function finish(results) {
  const failed = results.filter((r) => !r.ok);
  console.log(failed.length ? `truck-haul-proof: FAIL (${failed.length} of ${results.length}: ${failed.map((r) => r.name).join("; ")})` : `truck-haul-proof: PASS (${results.length} of ${results.length})`);
  return failed.length ? 1 : 0;
}

export default {
  name: "truck-haul-proof",
  // Straight after truck-check-proof the character holds the key of the truck it bought: the per-proof reset would clear
  // the inventory (the key goes with it) and the owner could not unlock the truck.
  noReset: true,
  usage: "truck-haul-proof [--cycles N] [--selftest]   (after truck-check-proof on `up --mission island --profile full`)",
  run: (ctx) => run(ctx?.args ?? []),
};
