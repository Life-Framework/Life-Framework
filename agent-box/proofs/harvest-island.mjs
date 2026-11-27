import { readFileSync } from "node:fs";
import { join } from "node:path";
import { arrive, clientBox, newestLog, parseState, readSession, sessionDir, sessionPaths } from "../session.mjs";
import { AMOUNT, parseMoney } from "./_money.mjs";
import { parseKv, parsePos } from "./hitch-sync.mjs";

// The whole harvester loop on the island (#3302 and its cards), one dedicated session, one or two clients:
//   1 cash        `resetplayer` gives the cash the reaper needs
//   2 buy         the farm supply till sells the reaper ($1,200): cash drops, `LF_OwnedImplements` logs the purchase
//   3 rack        a plain UAZ469 stands within 8 m of the rack (`hitch rackspawn`); *Take out reaper* hitches it
//   4 hitch       client 1 sits at the wheel; the reaper is a proxy child of the UAZ behind it, on client 2 too
//   5 cut         the rig crosses the wheat field at 3 m/s down the gate column: cells cut, hopper fills, client 2 sees the cut
//   6 sell        at the weighbridge *Sell grain* pays whole 50 kg lots, the remainder stays in the hopper
//   7 bag         *Bag grain* puts the remainder into sacks in the vehicle
// The harness cannot drive, so the server pushes the empty UAZ with physics velocity and steers it between waypoints
// (`hitch goto`, LF_ImplementHitchHarness.c). A leg that stalls on a building is set down at its end with `hitch warp`
// and the proof says so: a stalled leg is a WARP line, never a silent pass.
//
//   session up --mission island --clients 2 --profile full --wait ; session harvest-island [out dir]
//   harvest-island --selftest      the judge functions on made-up text, no game
const REACH = { till: "Fit_saint_philippe_farm_supply_farm_supply_till", rack: "Fit_saint_philippe_farm_supply_implement_rack" };
const FIELD = { unload: "Fit_saint_philippe_wheatfield_wheat_field_unload", haystack: "Fit_saint_philippe_wheatfield_wheat_field_haystack_1" };
const REAPER_PRICE = 1200;
const START_CASH = 3000;
const LOT_KG = 50;
// Waypoints (x, z) from the rack to the field's north edge (a corridor between the supply barn and the barn east of it),
// then the one long leg down the gate column (x 4435, the fence gap), over the field, through the gate to the weighbridge.
const TO_FIELD = [[4448, 10622], [4448, 10611], [4436, 10590], [4435, 10566]];
const FIELD_LEG = [4435, 10501];
const FIELD_SPEED = 3;
const FIELD_NORTH_Z = 10558;
const FIELD_SOUTH_Z = 10518;

export function lots(kg) {
  return Math.floor(kg / LOT_KG);
}

export function judgeSale(before, after, hopperBefore, hopperAfter, soldLine) {
  const fails = [];
  const n = lots(hopperBefore);
  if (n < 1) return [`hopper ${hopperBefore} kg is under one lot: nothing to sell`];
  const paid = after.cash - before.cash;
  if (paid <= 0) fails.push(`cash did not rise (${before.cash} -> ${after.cash})`);
  if (hopperAfter !== hopperBefore - n * LOT_KG) fails.push(`hopper ${hopperBefore} -> ${hopperAfter} kg, wanted ${hopperBefore - n * LOT_KG} (whole lots go, the remainder stays)`);
  const m = new RegExp(String.raw`grain sold: (\d+) kg for \$(${AMOUNT})`).exec(soldLine ?? "");
  if (!m) fails.push("no `grain sold` line in the server log");
  else {
    if (Number(m[1]) !== n * LOT_KG) fails.push(`the server sold ${m[1]} kg, wanted ${n * LOT_KG}`);
    if (parseMoney(m[2]) !== paid) fails.push(`the server paid $${m[2]} but cash moved $${paid}`);
  }
  return fails;
}

function selftest() {
  const fails = [];
  if (lots(260) !== 5 || lots(49) !== 0) fails.push("lots");
  const ok = judgeSale({ cash: 1800 }, { cash: 1810 }, 260, 10, "[LFDebug:Harvest] grain sold: 250 kg for $10 from x (10 kg stay in the hopper)");
  if (ok.length) fails.push(`a good sale was judged: ${ok.join("; ")}`);
  if (!judgeSale({ cash: 1800 }, { cash: 1800 }, 260, 10, "grain sold: 250 kg for $10").length) fails.push("a sale with no cash change passed");
  if (!judgeSale({ cash: 1800 }, { cash: 1810 }, 260, 0, "grain sold: 250 kg for $10").length) fails.push("a sale that took the remainder passed");
  if (!judgeSale({ cash: 1800 }, { cash: 1810 }, 260, 10, "").length) fails.push("a sale with no log line passed");
  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "harvest-island selftest: FAIL" : "harvest-island selftest: PASS");
  return fails.length ? 1 : 0;
}

async function runHarvestIsland(args) {
  if (args.includes("--selftest")) return selftest();
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const twoClients = existsClient(dir, 2);
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${r.detail}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const serverLog = () => {
    const f = newestLog(join(sessionPaths(dir).server, "logs"));
    return f ? readFileSync(f, "utf8") : "";
  };
  let logMark = serverLog().length;
  // The [LFDebug:*] lines the server wrote since the last call, each printed once.
  const lines = (re = /\[LFDebug:(Harvest|Shop|Money|Market|Replication)\]/) => {
    const text = serverLog();
    const fresh = text.slice(logMark).split(/\r?\n/).filter((l) => re.test(l));
    logMark = text.length;
    for (const l of fresh) console.log(`  server: ${l.replace(/^.*?(\[LFDebug:)/, "$1")}`);
    return fresh;
  };
  const results = [];
  const step = (name, fails, note = "") => {
    results.push({ name, ok: fails.length === 0 });
    console.log(`STEP ${name}: ${fails.length ? "FAIL" : "PASS"}${note ? ` (${note})` : ""}`);
    for (const f of fails) console.log(`  FAIL  ${f}`);
  };
  const state = (n) => parseState(on(n, { verb: "state" }, 10000).detail);
  const hitch = (n, ...words) => on(n, { verb: "hitch", words: words.map(String) }, 20000);
  const hstate = () => parseKv(hitch(1, "state").detail);
  const fpsOf = (r) => Number(/fps=([\d.]+)/.exec(r.detail ?? "")?.[1] ?? NaN);

  console.log(twoClients ? "two clients: client 2 watches the cut" : "one client: no second-client check (up --clients 2)");
  for (const n of twoClients ? [1, 2] : [1]) arrive(on, n, wait);
  wait(8000);
  lines();

  // 1 cash
  on(1, { verb: "resetplayer", cash: START_CASH }, 20000);
  wait(1500);
  const cash0 = state(1);
  step("1 cash", Number(cash0.cash) === START_CASH ? [] : [`cash is ${cash0.cash}, wanted ${START_CASH}`]);

  // 2 buy the reaper at the till
  on(1, { verb: "stand", entityName: REACH.till });
  wait(1500);
  const buyFails = [];
  const used = on(1, { verb: "use", className: "LF_ShopAction", entityName: REACH.till }, 15000);
  if (used.status !== "ok") buyFails.push(`the till prompt: ${used.status} ${used.detail}`);
  wait(1500);
  on(1, { verb: "click", widget: "Item0Row_Action" });
  wait(400);
  on(1, { verb: "click", widget: "PayButton" });
  wait(2500);
  on(1, { verb: "click", widget: "CloseButton" });
  wait(800);
  const cash1 = state(1);
  if (Number(cash0.cash) - Number(cash1.cash) !== REAPER_PRICE) buyFails.push(`cash ${cash0.cash} -> ${cash1.cash}, wanted -${REAPER_PRICE}`);
  const bought = lines();
  if (!bought.some((l) => /reaper|owned|implement/i.test(l))) buyFails.push("no Harvest/Shop line for the purchase in the server log");
  step("2 buy at the farm supply till", buyFails);

  // 3 rack: a UAZ at the rack, take the reaper out onto it
  on(1, { verb: "stand", entityName: REACH.rack });
  wait(1500);
  const rackFails = [];
  const spawned = hitch(1, "rackspawn");
  if (spawned.status !== "ok") rackFails.push(`rackspawn: ${spawned.status} ${spawned.detail}`);
  else {
    const sp = parseKv(spawned.detail);
    if (!(Number(sp.rackDist) <= 8)) rackFails.push(`the UAZ is ${sp.rackDist} m from the rack (take-out reaches 8 m)`);
    if (sp.found !== "1" && sp.found !== "true") rackFails.push("no empty ground beside the rack; the UAZ was set down over props");
  }
  wait(3000);
  const takeOut = on(1, { verb: "use", className: "LF_ImplementRackAction", entityName: REACH.rack }, 15000);
  if (takeOut.status !== "ok") rackFails.push(`take out: ${takeOut.status} ${takeOut.detail}`);
  wait(3000);
  const out = lines();
  if (!out.some((l) => /implement|hitch|reaper/i.test(l))) rackFails.push("no Harvest line for the take-out in the server log");
  step("3 take the reaper out at the rack", rackFails);

  // 4 hitch: seated, the reaper hangs behind the UAZ, on both clients
  const hitchFails = [];
  const seat = hitch(1, "seat");
  if (seat.status !== "ok") hitchFails.push(`seat: ${seat.status} ${seat.detail}`);
  wait(3000);
  const engine = on(1, { verb: "engine" }, 10000);
  if (engine.status !== "ok") hitchFails.push(`the character is not in the vehicle: ${engine.detail}`);
  const seen1 = parseKv(hitch(1, "seen").detail);
  if (seen1.implements !== "1") hitchFails.push(`client 1 sees ${seen1.implements ?? "?"} implements (wanted 1)`);
  else {
    if (seen1.behind !== "1" && seen1.behind !== "true") hitchFails.push("the reaper is not behind the UAZ");
    if (!(Number(seen1.gap) <= 0.2)) hitchFails.push(`the reaper is ${seen1.gap} m off its hitch point`);
  }
  step("4 reaper hitched behind the UAZ, driver seated", hitchFails);

  // fps away from the field (the rig parked at the rack)
  const fpsAway = fpsOf(on(1, { verb: "perf", seconds: 5, label: "away" }, 30000));

  // 5 cut: drive to the field, then the long leg across it
  const cutFails = [];
  const warps = [];
  let at = parsePos(hstate().at);
  let hopperAtEdge = 0;
  const driveTo = (x, z, speed, label, { warp = true, yaw = 180, stallS = 8, maxS = 90, onTick = () => {} } = {}) => {
    hitch(1, "goto", Math.round(x), Math.round(z), speed);
    let last = at;
    let lastMoved = Date.now();
    const t0 = Date.now();
    for (;;) {
      wait(1000);
      const s = hstate();
      const p = parsePos(s.at);
      if (s.arrived === "1" || s.arrived === "true") return { ok: true, state: s };
      if (p && last && Math.hypot(p[0] - last[0], p[2] - last[2]) > 1) {
        last = p;
        lastMoved = Date.now();
      }
      onTick(s);
      if (Date.now() - lastMoved > stallS * 1000 || Date.now() - t0 > maxS * 1000) {
        hitch(1, "stop");
        if (!warp) return { ok: false, state: s };
        warps.push(`${label} at ${s.at}`);
        console.log(`  WARP leg ${label}: the UAZ stalled at ${s.at}; set down at ${x},${z}`);
        hitch(1, "warp", Math.round(x), Math.round(z), yaw);
        wait(1500);
        return { ok: true, state: hstate(), warped: true };
      }
    }
  };
  for (const [i, [x, z]] of TO_FIELD.entries()) {
    const r = driveTo(x, z, 2, `rack-to-field ${i + 1}`, { yaw: 180 });
    at = parsePos(r.state.at);
  }
  const edge = hstate();
  hopperAtEdge = Number(edge.hopper);
  if (Number(edge.cut) > 0) cutFails.push(`${edge.cut} cells already cut before the field pass (the field is not fresh)`);
  if (twoClients) on(2, { verb: "stand", entityName: FIELD.haystack });
  wait(2000);
  let fpsField = NaN;
  let mid = null;
  const leg = driveTo(FIELD_LEG[0], FIELD_LEG[1], FIELD_SPEED, "field leg", {
    warp: false,
    maxS: 90,
    onTick: (s) => {
      const p = parsePos(s.at);
      if (!mid && p && p[2] < FIELD_NORTH_Z - 12 && p[2] > FIELD_SOUTH_Z + 8) {
        mid = s;
        fpsField = fpsOf(on(1, { verb: "perf", seconds: 4, label: "field" }, 30000));
      }
    },
  });
  if (!leg.ok) cutFails.push(`the rig stalled on the field leg at ${leg.state.at} (hopper ${leg.state.hopper}, cut ${leg.state.cut})`);
  wait(2000);
  const end = hstate();
  const hopper = Number(end.hopper);
  const cut = Number(end.cut);
  if (!(cut >= 20)) cutFails.push(`only ${end.cut} cells were cut across the field`);
  if (hopper !== cut * 10) cutFails.push(`hopper ${hopper} kg is not 10 kg a cut cell (${cut} cells)`);
  if (mid && !(Number(mid.cut) > 0)) cutFails.push("no cell was cut when the rig was mid-field");
  const cutLines = lines();
  if (!cutLines.some((l) => /cut|reaper/i.test(l))) cutFails.push("no Harvest cut line in the server log");
  if (twoClients) {
    let seen2 = "";
    for (let t = 0; t < 8; t++) {
      seen2 = on(2, { verb: "wheatsync", words: ["state", "0"] }, 10000).detail;
      if (Number(parseKv(seen2).cut) === cut) break;
      wait(1000);
    }
    const c2 = parseKv(seen2);
    if (Number(c2.cut) !== cut) cutFails.push(`client 2 sees ${c2.cut} cut cells, the server ${cut}`);
    if (!(Number(c2.stubble) >= cut - 2)) cutFails.push(`client 2 draws ${c2.stubble} stubble patches for ${cut} cut cells`);
  }
  step("5 cut the field, hopper fills, second client sees the cut", cutFails, `cut=${cut} hopper=${hopper} kg${warps.length ? `, WARPED legs: ${warps.join("; ")}` : ", every leg driven"}`);

  // 6 sell at the weighbridge
  const sellFails = [];
  const atBridge = hstate();
  if (atBridge.arrived !== "1" && atBridge.arrived !== "true") sellFails.push(`the rig is not at the weighbridge (at ${atBridge.at})`);
  const cashBefore = state(1);
  const hopperBefore = Number((hstate()).hopper);
  const sold = on(1, { verb: "use", className: "LF_GranaryUnloadAction", entityName: FIELD.unload }, 15000);
  if (sold.status !== "ok") sellFails.push(`Sell grain: ${sold.status} ${sold.detail}`);
  wait(2500);
  const cashAfter = state(1);
  const hopperAfter = Number((hstate()).hopper);
  const soldLines = lines();
  sellFails.push(...judgeSale({ cash: Number(cashBefore.cash) }, { cash: Number(cashAfter.cash) }, hopperBefore, hopperAfter, soldLines.find((l) => /grain sold/.test(l))));
  step("6 sell grain at the weighbridge", sellFails, `hopper ${hopperBefore} -> ${hopperAfter} kg, cash ${cashBefore.cash} -> ${cashAfter.cash}`);

  // 7 bag the remainder
  const bagFails = [];
  if (hopperAfter <= 0) bagFails.push("nothing left in the hopper to bag (the cut was a whole number of lots)");
  else {
    const bagged = on(1, { verb: "use", className: "LF_GranaryUnloadAction__1", entityName: FIELD.unload }, 15000);
    if (bagged.status !== "ok") bagFails.push(`Bag grain: ${bagged.status} ${bagged.detail}`);
    wait(2500);
    const left = Number((hstate()).hopper);
    const bagLines = lines();
    if (!bagLines.some((l) => /grain bagged/.test(l))) bagFails.push("no `grain bagged` line in the server log");
    if (left >= hopperAfter) bagFails.push(`hopper ${hopperAfter} -> ${left} kg: nothing went into sacks`);
  }
  step("7 bag grain once", bagFails);

  hitch(1, "stop");
  const fps = `fps away ${Number.isFinite(fpsAway) ? fpsAway : "?"}, in the field ${Number.isFinite(fpsField) ? fpsField : "?"}`;
  console.log(`frame cost: ${fps} (client 1, one 5 s and one 4 s sample; compare, the session cap hides frames above it)`);
  const failed = results.filter((r) => !r.ok);
  console.log(failed.length ? `harvest-island: FAIL (${failed.map((r) => r.name).join("; ")})` : "harvest-island: PASS (the whole loop ran on the island)");
  return failed.length ? 1 : 0;
}

function existsClient(dir, n) {
  const session = readSession(dir);
  return Array.isArray(session?.clients) && session.clients.some((c) => c.n === n);
}

export default {
  name: "harvest-island",
  noReset: true,
  usage: "harvest-island | --selftest   (session up --mission island --clients 2 --profile full --wait)",
  run: (ctx) => runHarvestIsland(ctx.args ?? []),
};
