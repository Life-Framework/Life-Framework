// The mining loops of `tools\cli economy` (#3240, E1 of #3206): what a miner earns an hour on each mining chain, net of
// the reagent bought and the fuel burned, with the haul counted, and whether the chain pays at least the lowest duty
// shift (police and medic rank 0, the Cadet's): the 0.3.0 pay ladder, 2026-11-22 (LADDER in economy-lumber.mjs).
//
// Read from config (core, then the pack overlays through tools/pack/layers.mjs): the plant recipes (inputs, outputs,
// maxBatches, processSeconds in Configs/Resources/ResourceChains.conf), the wholesale curves (market.goods, price per
// LOT of m_iLotSize units) and the hardware store's premium rows (buyerDemand.rows, a purse of `cap` units that refills
// `refillPerHour` an hour). Read from tools/economy/mining-assumptions.json (no config holds them): the gather rate,
// the load a vehicle carries (sacks, #3263 and #3265), the km between mine, plant and buyer, the reagent price and fuel.
//
// One trip: gather the ore, drive to the plant, run the batches (whole runs of maxBatches, processSeconds each), drive
// to the buyer, drive back, plus load and unload time. Trips an hour = 60 / trip minutes. A trip's product sells first
// into the premium purse, the rest on the wholesale curve (linear drop per lot, held at the floor). The FIRST hour sells
// from a full purse, the STEADY hour (the loop run for hours on end) from the refill only. Without --wholesale-carry
// every hour opens at the curve's base price (the lumber and wheat convention).
import { existsSync, readdirSync, readFileSync } from "node:fs";
import { join } from "node:path";
import { parseConf, guidOf } from "../economy/profit-loops.mjs";
import { SECTIONS, mergeList, sectionLayers } from "../pack/layers.mjs";
import { rungCheck, rungText } from "./economy-lumber.mjs";
import { RUNGS, fullPerksHourly, rungOf } from "./economy-perks.mjs";

export const MINING_ASSUMPTIONS_FILE = ["tools", "economy", "mining-assumptions.json"];
const num = (v, d = 0) => (typeof v === "number" ? v : d);
const round1 = (n) => Math.round(n * 10) / 10;

/**
 * One hour of selling `units` at a per-lot curve now at `price`: { cash, price (after) }. Linear drop per lot, held at
 * the floor. Lots are fractional: a slow chain (a trip a few hours long) still earns its share of a lot an hour, where
 * the wheat reader would round it to nothing.
 */
export function sellCurve(curve, price, units) {
  const lot = Math.max(1, curve.lotSize || 1);
  const lots = Math.max(units, 0) / lot;
  const drop = (curve.base * curve.dropPerUnitPct) / 100;
  let cash = 0, p = price, left = lots;
  while (left > 1e-9) {
    const part = Math.min(1, left);
    cash += p * part;
    p = Math.max(curve.floor, p - drop * part);
    left -= part;
  }
  return { cash, price: p, lots };
}

const recover = (curve, price) => Math.min(curve.base, price + (curve.base - price) * curve.recoverPctPerHour / 100);

/**
 * Hours of selling `units` a hour: the premium purse first (whole premium lots, the purse in units), the rest on the
 * wholesale curve. Returns the cash and the premium units of hour 1 and of the steady hour.
 */
export function simulateSales(units, premium, curve, carry = false, hours = 24) {
  const pLot = premium ? Math.max(1, premium.lotSize || 1) : 1;
  let remaining = premium ? premium.cap : 0, price = curve.base;
  let first = 0, steady = 0, premiumFirst = 0, premiumSteady = 0;
  for (let h = 1; h <= hours; h++) {
    let pUnits = 0;
    if (premium) pUnits = Math.min(units, remaining);
    const sold = sellCurve(curve, price, units - pUnits);
    const cash = (pUnits / pLot) * (premium ? premium.price : 0) + sold.cash;
    if (h === 1) { first = cash; premiumFirst = pUnits; }
    steady = cash; premiumSteady = pUnits;
    if (premium) remaining = Math.min(premium.cap, remaining - pUnits + premium.refillPerHour);
    price = carry ? recover(curve, sold.price) : curve.base;
  }
  return { first, steady, premiumFirst, premiumSteady, wholesaleSteady: units - premiumSteady };
}

/** Units one trip can carry of an item (sacks of `sackUnits`, or loose items in a boot). */
export function tripCapacity(vehicle, item, a) {
  const v = a.vehicles[vehicle];
  if (!v) return 0;
  return num(v.sacks) * num(a.sackUnits[item], 1) + num(v.looseUnits);
}

/**
 * One chain's trip and hourly money from resolved inputs (pure: the selftest feeds it directly).
 * `inp`: { chain (assumptions row), assumptions, recipe ({ gatheredPerBatch, outputPerBatch, maxBatches, processSeconds,
 * reagentPerBatch: [{ item, amount }] } or null), curve, premium (or null), duty, carry }.
 */
export function computeMiningLoop(inp) {
  const { chain: c, assumptions: a, recipe, curve, premium, duty } = inp;
  const gatherMinutes = c.gather.units / c.gather.perMinute;
  let batches = 0, product = c.gather.units, runs = 0, processMinutes = 0, reagentPerTrip = 0;
  if (recipe) {
    batches = Math.floor(c.gather.units / recipe.gatheredPerBatch);
    product = batches * recipe.outputPerBatch;
    runs = Math.ceil(batches / Math.max(1, recipe.maxBatches));
    processMinutes = (runs * recipe.processSeconds) / 60;
    for (const r of c.reagents) reagentPerTrip += batches * num(recipe.reagentPerBatch[r.item], 0) * r.unitCost;
  }
  const km = c.km.mineToPlant + c.km.plantToBuyer + c.km.returnToMine;
  const driveMinutes = km * num(a.minutesPerKm[c.vehicle], 1);
  const tripMinutes = gatherMinutes + processMinutes + driveMinutes + c.overheadMinutes;
  const trips = 60 / tripMinutes;
  const capacity = tripCapacity(c.vehicle, c.gather.item, a);
  const problems = [];
  if (c.gather.units > capacity) problems.push(`${c.id}: a trip gathers ${c.gather.units} ${c.gather.item} but a ${c.vehicle} carries ${capacity}`);
  const unitsPerHour = trips * product;
  const s = simulateSales(unitsPerHour, premium, curve, inp.carry);
  const costPerHour = trips * reagentPerTrip + num(c.fuelPerHour);
  const first = s.first - costPerHour, steady = s.steady - costPerHour;
  return {
    id: c.id, label: c.label, vehicle: c.vehicle, note: c.note, problems,
    tripMinutes: round1(tripMinutes), tripsPerHour: round1(trips), productPerTrip: product, batchesPerTrip: batches, runsPerTrip: runs,
    gatherMinutes: round1(gatherMinutes), driveMinutes: round1(driveMinutes), processMinutes: round1(processMinutes),
    unitsPerHour: round1(unitsPerHour), premiumSteady: round1(s.premiumSteady), premiumFirst: round1(s.premiumFirst),
    grossFirst: Math.round(s.first), grossSteady: Math.round(s.steady), costPerHour: Math.round(costPerHour),
    first: Math.round(first), steady: Math.round(steady), ...rungCheck(steady, duty),
  };
}

const read = (p) => (existsSync(p) ? readFileSync(p, "utf8") : "");

/** The merged list section as [{ id, f(name) }]. */
function listOf(addonsDir, name, packLayers, owner) {
  const merged = mergeList(sectionLayers(addonsDir, SECTIONS[name], packLayers, owner));
  return [...merged.entries.values()].filter((e) => e.present).map((e) => ({ id: e.id, f: (n) => e.fields.get(n)?.value }));
}

/** Every pack's chain overlay (a .conf under addons/<pack>/Configs whose root is LF_ResourceChainConfig), parsed, in path order. */
function packChains(addonsDir) {
  const out = [];
  const walk = (dir) => {
    for (const e of readdirSync(dir, { withFileTypes: true }).sort((a, b) => a.name.localeCompare(b.name))) {
      const p = join(dir, e.name);
      if (e.isDirectory()) walk(p);
      else if (e.name.endsWith(".conf")) {
        const text = read(p);
        if (/^\s*LF_ResourceChainConfig\b/.test(text)) out.push(parseConf(text));
      }
    }
  };
  for (const a of existsSync(addonsDir) ? readdirSync(addonsDir, { withFileTypes: true }) : []) {
    const cfg = join(addonsDir, a.name, "Configs");
    if (a.isDirectory() && a.name !== "LifeFramework" && existsSync(cfg)) walk(cfg);
  }
  return out;
}

const nameOfRes = (res) => String(res ?? "").replace(/^\{[0-9A-Fa-f]+\}/, "").replace(/^.*\//, "").replace(/\.et$/, "");

/** A machine's recipe by id from the parsed chains config: the gathered input, the reagents, the output. */
export function recipeOf(chains, machineId, chain) {
  const m = (chains?.props.m_aMachines?.list ?? []).find((x) => x.props.m_sMachineId === machineId);
  if (!m) return null;
  const ins = (m.props.m_aInputs?.list ?? []).map((i) => ({ name: nameOfRes(i.props.m_InputPrefab), n: num(i.props.m_iInputAmount, 1) }));
  const outs = m.props.m_aOutputs?.list ?? [];
  const reagentNames = new Map(chain.reagents.map((r) => [r.guidName, r.item]));
  const gathered = ins.find((i) => !reagentNames.has(i.name));
  if (!gathered || outs.length === 0) return null;
  const reagentPerBatch = {};
  for (const i of ins) if (reagentNames.has(i.name)) reagentPerBatch[reagentNames.get(i.name)] = i.n;
  return {
    gatheredPerBatch: gathered.n, outputPerBatch: num(outs[0].props.m_iOutputAmount, 1), reagentPerBatch,
    maxBatches: num(m.props.m_iMaxBatches, 1), processSeconds: num(m.props.m_iProcessSeconds, 0),
    // The level steps of the machine (#4861): from m_iLevel in the gate trade a bigger batch cap and/or a shorter run.
    steps: (m.props.m_aSteps?.list ?? []).map((st) => ({ level: num(st.props.m_iLevel), maxBatches: num(st.props.m_iMaxBatches), processSeconds: num(st.props.m_iProcessSeconds) })).sort((x, y) => x.level - y.level),
  };
}

/**
 * The recipe a worker of `level` runs (#4861, LF_ProcessAction.MaxBatchesAt and SecondsAt): the machine's own cap and
 * time, the cap raised and the run cut by every step at or below the level (a step never lowers a cap or lengthens a run).
 */
export function recipeAt(recipe, level) {
  let { maxBatches, processSeconds } = recipe;
  for (const st of recipe.steps ?? []) {
    if (st.level > level) continue;
    if (st.maxBatches > maxBatches) maxBatches = st.maxBatches;
    if (st.processSeconds > 0 && st.processSeconds < processSeconds) processSeconds = st.processSeconds;
  }
  return { ...recipe, maxBatches, processSeconds };
}

/** The band check of a smelting ladder: rows past the top of the base rung, or full perks past the crime ceiling, as text. */
export function bandProblems(rows) {
  const baseRung = RUNGS.find((x) => x.id === rows[0].rung);
  const problems = [];
  for (const r of rows.slice(1)) {
    if (baseRung && r.steady >= baseRung.upTo) problems.push(`level ${r.level}: steady $${r.steady}/h is past the top of the ${baseRung.id} rung ($${baseRung.upTo})`);
    const rung = RUNGS.find((x) => x.id === r.rung);
    if (rung && r.full > rung.crimeCeiling) problems.push(`level ${r.level}: full perks $${r.full}/h is past the ${rung.id} crime ceiling ($${rung.crimeCeiling})`);
  }
  return problems;
}

/**
 * One chain's money at the machine's step levels (the first row is the base machine, level 1): { level, maxBatches,
 * processSeconds, steady, first, full (steady with both perks), tripsPerHour, processMinutes, rung } each. The band is the
 * rung the base loop sits on: a step that lifts the steady hourly past its top, or the full-perks line past the crime
 * ceiling of the rung it then sits on, is flagged (the pay ladder of economy-lumber.mjs LADDER and economy-perks.mjs RUNGS).
 */
export function smeltingLadder(inp) {
  const steps = inp.recipe?.steps ?? [];
  if (steps.length === 0) return null;
  const levels = [1, ...new Set(steps.map((x) => x.level))].sort((x, y) => x - y);
  const rows = levels.map((level) => {
    const r = recipeAt(inp.recipe, level);
    const loop = computeMiningLoop({ ...inp, recipe: r });
    return {
      level, maxBatches: r.maxBatches, processSeconds: r.processSeconds, steady: loop.steady, first: loop.first,
      full: fullPerksHourly(loop), tripsPerHour: loop.tripsPerHour, processMinutes: loop.processMinutes, rung: rungOf(loop.steady)?.id ?? "none",
    };
  });
  const baseRung = RUNGS.find((x) => x.id === rows[0].rung);
  const problems = bandProblems(rows);
  return { id: inp.chain.id, label: inp.chain.label, rows, band: baseRung ? { rung: baseRung.id, floor: baseRung.floor, top: baseRung.upTo } : null, problems };
}

const curveOf = (row) => ({
  base: num(row.f("base")), floor: num(row.f("floor")), dropPerUnitPct: num(row.f("dropPerUnitPct")),
  recoverPctPerHour: num(row.f("recoverPctPerHour")), lotSize: Math.max(1, num(row.f("lotSize"), 1)),
});

/** Reads the configs and the assumptions file, returns { loops, sources, problems, assumptions }. */
export function buildMining({ root, addonsDir, packLayers, owner, duty, opts = {} }) {
  const sources = [], problems = [];
  const file = opts.miningAssumptions ?? join(root, ...MINING_ASSUMPTIONS_FILE);
  if (!existsSync(file)) return { loops: [], sources, problems: [`no ${MINING_ASSUMPTIONS_FILE.join("/")}: mining loops skipped`] };
  let a;
  try { a = JSON.parse(read(file)); } catch (e) { return { loops: [], sources, problems: [`${MINING_ASSUMPTIONS_FILE.join("/")} is not JSON (${e.message}): mining loops skipped`] }; }
  const chainsText = read(join(addonsDir, "LifeFramework", "Configs", "Resources", "ResourceChains.conf"));
  const chains = chainsText ? parseConf(chainsText) : null;
  const goods = listOf(addonsDir, "market.goods", packLayers, owner);
  const demand = listOf(addonsDir, "buyerDemand.rows", packLayers, owner);
  const loops = [], ladders = [];
  for (const c of a.chains) {
    let recipe = null;
    if (c.recipe) {
      recipe = recipeOf(chains, c.recipe, c);
      // A pack adds machines (never rewrites one) in its own LF_ResourceChainConfig overlays (#3450).
      for (const pack of recipe ? [] : packChains(addonsDir)) if (!recipe) recipe = recipeOf(pack, c.recipe, c);
      if (!recipe) { problems.push(`${c.id}: no machine '${c.recipe}' with its inputs and an output in Configs/Resources/ResourceChains.conf or a pack chain overlay: chain skipped`); continue; }
    }
    const goodRow = goods.find((r) => r.id === c.wholesaleGood);
    if (!goodRow) { problems.push(`${c.id}: no market curve for ${c.wholesaleGood} in market.goods: chain skipped`); continue; }
    const curve = curveOf(goodRow);
    sources.push(`${c.id}: wholesale ${curve.base} to ${curve.floor} per ${curve.lotSize} unit lot, drop ${curve.dropPerUnitPct}%, recover ${curve.recoverPctPerHour}%/h`);
    let premium = null;
    if (c.buyer) {
      const row = demand.find((r) => r.id === `${c.buyer}|${c.premiumGood}` && num(r.f("price")) > 0);
      if (row) {
        premium = { price: num(row.f("price")), cap: num(row.f("cap")), refillPerHour: num(row.f("refillPerHour")), lotSize: Math.max(1, num(row.f("lotSize"), 1)) };
        sources.push(`${c.id}: premium row ${c.buyer}, $${premium.price} a unit, purse ${premium.cap} +${premium.refillPerHour}/h`);
      } else sources.push(`${c.id}: no ${c.buyer} premium row, wholesale only`);
    }
    const inp = { chain: c, assumptions: a, recipe, curve, premium, duty, carry: !!opts.lumber?.carryCurve };
    loops.push(computeMiningLoop(inp));
    const ladder = recipe ? smeltingLadder(inp) : null;
    if (ladder) ladders.push(ladder);
  }
  for (const l of loops) problems.push(...l.problems);
  return { loops, ladders, sources, problems, assumptions: a };
}

const money = (n) => `$${n}`;

const ratioText = (r) => (r === Infinity ? "inf" : `${r.toFixed(1)}x`);

/** The lines `tools\cli economy` prints for the mining section. */
export function renderMining(m) {
  const L = [];
  for (const p of m.problems) L.push(`  problem: ${p}`);
  if (!m.loops.length) return L;
  L.push(`mining loops (net of reagent and fuel, haul counted; steady = purse refill only, first = full purse, wholesale back at base each hour):`);
  for (const x of m.loops) {
    const duty = x.ratios.map((r) => `${r.job} ${money(r.pay)} = ${ratioText(r.ratio)}`).join(", ");
    L.push(`  LOOP ${x.label}: steady ${money(x.steady)}/h net, first hour ${money(x.first)}/h; duty ${duty} ${rungText(x)}`);
    L.push(`    trip ${x.tripMinutes} min (gather ${x.gatherMinutes}, plant ${x.processMinutes}, drive ${x.driveMinutes}), ${x.tripsPerHour} trips/h, ${x.unitsPerHour} units/h (${x.premiumSteady} premium steady); gross ${money(x.grossSteady)}/h less ${money(x.costPerHour)} reagent and fuel${x.note ? `; ${x.note}` : ""}`);
  }
  for (const x of m.ladders ?? []) {
    L.push(`smelting ladder (#4861, ${x.label}: the machine's level steps, a worker of that level; band = the ${x.band?.rung} rung, $${x.band?.floor} to $${x.band?.top}/h):`);
    for (const r of x.rows) L.push(`  LEVEL ${r.level}: ${r.maxBatches} bars a run, ${r.processSeconds} s a run, plant ${r.processMinutes} min, ${r.tripsPerHour} trips/h, steady ${money(r.steady)}/h net, full perks ${money(r.full)}/h (${r.rung} rung)`);
  }
  L.push(`  sources: ${m.sources.join("; ")}`);
  return L;
}

/** FLAG rows for the chains below the rung, in the report's flag shape. */
export function miningFlags(m) {
  const band = (m.ladders ?? []).flatMap((x) => x.problems.map((p) => ({ kind: "smelting-band", id: `smelting:${x.id}`, text: `smelting ladder ${x.label}: ${p}` })));
  return band.concat(m.loops.filter((x) => x.flagged).map((x) => ({
    kind: "mining-duty", id: `mining:${x.id}`,
    text: `mining chain ${x.label} pays ${money(x.steady)}/h net steady; duty pay is ${x.ratios.map((r) => `${ratioText(r.ratio)} (${r.job} ${money(r.pay)})`).join(" and ")}, under the $${x.floor}/h Cadet shift`,
  })));
}
