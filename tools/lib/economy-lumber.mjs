// The lumber loops of `tools\cli economy` (#3270, LB12 of #3169): what a lumberjack earns an hour on the three
// loops docs/roadmap/lumber.md "Money ladder" names, from the merged configs, and whether each loop pays at
// least the lowest duty shift (police and medic rank 0, the Cadet's): the 0.3.0 pay ladder, 2026-11-22 (LADDER below).
//
// Read from config (core, then the pack overlays through tools/pack/layers.mjs): the tree yield rows
// (Configs/Lumber/LF_TreeSpecies.conf), the sawmill outputs per log (Configs/Resources/ResourceChains.conf), the
// premium buyers' demand rows (buyerDemand.rows) and the market curves (market.goods). A premium or wholesale row
// the merge does not hold yet falls back to the design note's number and the loop says so ("assumed"), so the tool
// still prints a ladder before the pack rows land and switches to the real rows the moment they do.
//
// Not in any config, so stated here and printed: the route (hand haul: a 150 m walk with a log at 1.1 m/s and a
// 3.4 m/s run back; truck: 6 logs a cycle, 4 cycles an hour), the mix of products a hand hauler cuts, and the sawing
// time per log. A curve falls LINEARLY: one sold unit takes dropPerUnitPct of the base off the price, never below the
// floor (LF_PriceCurve.DropPerUnit); an hour closes recoverPctPerHour of the gap back to the base. A premium purse
// holds `cap` units and refills `refillPerHour` an hour; the FIRST hour sells from a full purse, the STEADY hour
// (the loop run for hours on end) from the refill only.
import { existsSync, readFileSync } from "node:fs";
import { join } from "node:path";
import { parseConf, guidOf } from "../economy/profit-loops.mjs";
import { SECTIONS, mergeList, sectionLayers } from "../pack/layers.mjs";

/**
 * The 0.3.0 pay ladder (the maintainer's rule, 2026-11-22; docs/roadmap/next-release-0.3.0.md "The economy table"):
 * police duty is the lowest steady pay, medics sit on the mid-tier rung, civilian trades climb above duty with skill
 * and vehicle tier. `entry` and `mid` are the civilian rungs in $ an hour. A police shift stays at or under the entry
 * rung's top and, with its bonus cap, at or under `policeCeiling` (the plan's $520 mid-tier floor); a medic shift sits on the mid-tier rung and, with
 * its bonus cap, at or under `medicCeiling`. A civilian loop pays at least the lowest duty shift (the Cadet's).
 * `bonusCaps` are the per-rank outcome bonus caps per shift hour (the rows PO-D4 reads; no config holds them yet).
 */
export const LADDER = {
  entry: { min: 400, max: 480 },
  mid: { min: 480, max: 600 },
  policeCeiling: 520,
  medicCeiling: 600,
  bonusCaps: {
    police: { patrol: 100, senior: 100, senior_patrol: 100, sergeant: 95, lieutenant: 70 },
    medic: { orderly: 40, emt: 50, paramedic: 50, doctor: 40 },
  },
};

/** A civilian loop's steady $/h against the duty shifts: ratio per duty job, and flagged when it pays under the lowest. */
export function rungCheck(perHour, duty) {
  const ratios = Object.entries(duty).map(([job, pay]) => ({ job, pay, ratio: perHour > 0 ? Math.round((pay / perHour) * 100) / 100 : Infinity }));
  const floor = ratios.length ? Math.min(...ratios.map((r) => r.pay)) : 0;
  return { ratios, floor, flagged: !(perHour > 0) || perHour < floor };
}

/** What a civilian loop's rung check prints: "on the ladder" or "BELOW THE RUNG". */
export function rungText(x) {
  return x.flagged ? `BELOW THE RUNG (under the $${x.floor}/h Cadet shift)` : `on the ladder (at or above the $${x.floor}/h Cadet shift)`;
}

/**
 * Every duty rank against its rung: duty is the report's { police: { ranks: [{ id, perHour }] }, medic: ... }.
 * Returns { rows: [{ job, id, shift, cap, most, typical, problems }], flags } (one "duty-rung" flag per rank off it).
 */
export function dutyLadder(duty) {
  const rows = [];
  for (const job of ["police", "medic"]) {
    for (const r of duty?.[job]?.ranks ?? []) {
      const cap = LADDER.bonusCaps[job]?.[r.id] ?? 0;
      const shift = r.perHour, most = shift + cap, problems = [];
      if (job === "police") {
        if (shift > LADDER.entry.max) problems.push(`$${shift}/h shift is above the entry rung ($${LADDER.entry.max})`);
        if (most > LADDER.policeCeiling) problems.push(`$${most}/h with its bonus cap is over the $${LADDER.policeCeiling} mid-tier floor`);
      } else {
        if (shift < LADDER.mid.min || shift > LADDER.mid.max) problems.push(`$${shift}/h shift is off the mid-tier rung ($${LADDER.mid.min} to $${LADDER.mid.max})`);
        if (most > LADDER.medicCeiling) problems.push(`$${most}/h with its bonus cap is over $${LADDER.medicCeiling}`);
      }
      rows.push({ job, id: r.id, shift, cap, most, typical: Math.round(shift + cap / 2), problems });
    }
  }
  const flags = rows.filter((x) => x.problems.length).map((x) => ({ kind: "duty-rung", id: `duty:${x.job}:${x.id}`, text: `${x.job} ${x.id}: ${x.problems.join("; ")}` }));
  return { rows, flags };
}

/** The lines `tools\cli economy` prints for the duty rungs. */
export function renderDutyLadder(d) {
  const L = [`duty rungs (police at or under the $${LADDER.entry.max} entry rung and $${LADDER.policeCeiling} with the bonus; medic on the $${LADDER.mid.min} to $${LADDER.mid.max} mid-tier rung and at most $${LADDER.medicCeiling} with the bonus; typical = half the bonus cap):`];
  for (const x of d.rows) L.push(`  ${x.job.toUpperCase().padEnd(6)} ${x.id} $${x.shift}/h shift, bonus cap $${x.cap}, most $${x.most}/h, typical $${x.typical}/h${x.problems.length ? `  OFF THE RUNG: ${x.problems.join("; ")}` : ""}`);
  return L;
}

/** Item prefab GUIDs (the key every market list uses). */
export const GOODS = {
  plank: { guid: "45D1B36F99DD2FC3", name: "plank" },
  post: { guid: "7EB79C84E76D667D", name: "fence post" },
  firewood: { guid: "F96D821A8E47FBBA", name: "firewood" },
  board: { guid: "D2A8639FA0CA09EF", name: "hardwood board" },
};
const SOFTWOOD_LOG = "32F5A906D8A573BC";
const HARDWOOD_LOG = "FC7D57CC9C0798D7";

/** Route assumptions (docs/roadmap/lumber.md "Money ladder"); `opts.lumber` overrides any of them. */
export const LUMBER_ASSUMPTIONS = {
  walkMps: 1.1, runMps: 3.4, haulMetres: 150, treeSize: 2, sawSeconds: { plank: 10, post: 8, firewood: 6, board: 15 },
  /** Share of the logs a mixed hand hauler cuts into each product. */
  mix: { plank: 0.5, post: 1 / 6, firewood: 1 / 3 },
  truck: { logsPerCycle: 6, cyclesPerHour: 4, firewoodShare: 1 / 6 },
  /** false: every hour opens at the wholesale base price (the design note). true (--wholesale-carry): the curve carries over and only recovers. */
  carryCurve: false,
};
/** Fallback buyer and curve rows (the design note's tables) for a good the merged config does not price yet. */
export const FALLBACK_BUYERS = {
  plank: { price: 8, cap: 40, refillPerHour: 20 },
  board: { price: 12, cap: 24, refillPerHour: 8 },
  post: { price: 11, cap: 12, refillPerHour: 6 },
  firewood: { price: 4, cap: 30, refillPerHour: 30 },
};
export const FALLBACK_CURVES = {
  plank: { base: 6, floor: 4, dropPerUnitPct: 1, recoverPctPerHour: 5 },
  board: { base: 7, floor: 4, dropPerUnitPct: 1, recoverPctPerHour: 5 },
  post: { base: 5, floor: 3, dropPerUnitPct: 2, recoverPctPerHour: 5 },
  firewood: { base: 2, floor: 1, dropPerUnitPct: 2, recoverPctPerHour: 10 },
};

const num = (v, d = 0) => (typeof v === "number" ? v : d);

/** What one hour of selling `units` earns at a wholesale curve now at `price`: { cash, price (after) }. Linear drop. */
export function sellWholesale(curve, price, units) {
  const drop = (curve.base * curve.dropPerUnitPct) / 100;
  let cash = 0, p = price;
  for (let i = 0; i < units; i++) {
    cash += p;
    p = Math.max(curve.floor, p - drop);
  }
  return { cash, price: p };
}

/** One hour closes `recoverPctPerHour` of the gap to the base. */
const recover = (curve, price) => Math.min(curve.base, price + (curve.base - price) * curve.recoverPctPerHour / 100);

/**
 * Hour by hour for one product line: premium purse first (up to what is left), the rest at the wholesale curve.
 * `units` is the product made each hour. `carry` keeps the wholesale curve where the hour left it (it then recovers
 * only recoverPctPerHour of the gap); without it every hour opens at the base price, the design note's "steady hour". Returns the cash of hour 1 and of the steady hour (hour `hours`).
 */
export function simulateProduct(units, buyer, curve, carry = false, hours = 24) {
  let remaining = buyer.cap, price = curve.base;
  let first = 0, steady = 0, premiumFirst = 0, premiumSteady = 0;
  for (let h = 1; h <= hours; h++) {
    const premium = Math.min(units, remaining);
    const sold = sellWholesale(curve, price, units - premium);
    const cash = premium * buyer.price + sold.cash;
    if (h === 1) { first = cash; premiumFirst = premium; }
    steady = cash; premiumSteady = premium;
    remaining = Math.min(buyer.cap, remaining - premium + buyer.refillPerHour);
    price = carry ? recover(curve, sold.price) : curve.base;
  }
  return { first, steady, premiumFirst, premiumSteady, wholesaleSteady: units - premiumSteady };
}

/** Logs an hour from the hand-haul route: chop each log, walk it to the yard, run back, saw it. */
export function handHaulLogsPerHour(chopSecondsPerLog, sawSeconds, a) {
  const cycle = chopSecondsPerLog + a.haulMetres / a.walkMps + a.haulMetres / a.runMps + sawSeconds;
  return 3600 / cycle;
}

/** The loops from resolved inputs (pure: the selftest feeds it directly). */
export function computeLoops(inp) {
  const a = inp.assumptions;
  const buyer = (k) => inp.buyers[k], curve = (k) => inp.curves[k];
  const loops = [];
  const make = (id, label, logsPerHour, lines, note) => {
    let first = 0, steady = 0;
    const detail = [];
    for (const [k, units] of lines) {
      if (units <= 0) continue;
      const s = simulateProduct(units, buyer(k), curve(k), a.carryCurve);
      first += s.first; steady += s.steady;
      detail.push({ good: k, unitsPerHour: Math.round(units * 10) / 10, premiumSteady: Math.round(s.premiumSteady * 10) / 10, premiumFirst: Math.round(s.premiumFirst * 10) / 10 });
    }
    loops.push({ id, label, logsPerHour: Math.round(logsPerHour * 10) / 10, first: Math.round(first), steady: Math.round(steady), detail, note, ...rungCheck(steady, inp.duty) });
  };
  const soft = inp.yields.softwood, hard = inp.yields.hardwood;
  const chopSoft = soft.chopSecondsPerLog, chopHard = hard.chopSecondsPerLog;
  const out = inp.outputs;
  // Hand-haul softwood, planks only.
  const plankLogs = handHaulLogsPerHour(chopSoft, a.sawSeconds.plank, a);
  make("hand-planks", "hand-haul softwood -> planks", plankLogs, [["plank", plankLogs * out.plank]]);
  // Hand-haul softwood, a mix of planks, posts and firewood (the saw time follows the mix).
  const mixSaw = Object.entries(a.mix).reduce((s, [k, w]) => s + w * a.sawSeconds[k], 0);
  const mixLogs = handHaulLogsPerHour(chopSoft, mixSaw, a);
  make("hand-mixed", "hand-haul softwood -> planks, posts, firewood", mixLogs, Object.entries(a.mix).map(([k, w]) => [k, mixLogs * w * out[k]]));
  // Truck hardwood: the cycle count is the stated assumption, boards plus firewood from the offcuts.
  const truckLogs = a.truck.logsPerCycle * a.truck.cyclesPerHour;
  make("truck-hardwood", "truck hardwood -> boards, firewood", truckLogs,
    [["board", truckLogs * (1 - a.truck.firewoodShare) * out.board], ["firewood", truckLogs * a.truck.firewoodShare * out.hardFirewood]],
    `${a.truck.logsPerCycle} logs a cycle, ${a.truck.cyclesPerHour} cycles an hour`);
  void chopHard;
  return loops;
}

const read = (p) => (existsSync(p) ? readFileSync(p, "utf8") : "");

/** The merged list section as [{ id, f(name) }]. */
function listOf(addonsDir, name, packLayers, owner) {
  const merged = mergeList(sectionLayers(addonsDir, SECTIONS[name], packLayers, owner));
  return [...merged.entries.values()].filter((e) => e.present).map((e) => ({ id: e.id, f: (n) => e.fields.get(n)?.value }));
}

/** Seconds of chopping per log for a tree of `size` of one class, from the yield rows (null without a row). */
function chopPerLog(rows, cls, size) {
  const r = rows.find((x) => x.props.m_eClass === cls && x.props.m_iSize === size);
  return r && num(r.props.m_iLogs) > 0 ? { chopSecondsPerLog: num(r.props.m_iChopSeconds) / num(r.props.m_iLogs), logs: num(r.props.m_iLogs), xp: r.props.m_sXpAction } : null;
}

/** Units one log of `inputGuid` becomes when the machine's single output is `outGuid`. */
function outputPerLog(machines, inputGuid, outGuid) {
  for (const m of machines) {
    const ins = (m.props.m_aInputs?.list ?? []).map((i) => guidOf(i.props.m_InputPrefab));
    if (ins.length !== 1 || ins[0] !== inputGuid) continue;
    const need = num((m.props.m_aInputs.list[0]).props.m_iInputAmount, 1);
    for (const o of m.props.m_aOutputs?.list ?? []) if (guidOf(o.props.m_OutputPrefab) === outGuid) return num(o.props.m_iOutputAmount) / need;
  }
  return null;
}

/** Everything `buildReport` needs: reads the configs, resolves fallbacks, returns { loops, sources, problems }. */
export function buildLumber({ addonsDir, packLayers, owner, duty, opts = {} }) {
  const a = { ...LUMBER_ASSUMPTIONS, ...(opts.lumber ?? {}) };
  const sources = [], problems = [];
  const treeText = read(join(addonsDir, "LifeFramework", "Configs", "Lumber", "LF_TreeSpecies.conf"));
  const chainText = read(join(addonsDir, "LifeFramework", "Configs", "Resources", "ResourceChains.conf"));
  const trees = treeText ? parseConf(treeText) : null;
  const chains = chainText ? parseConf(chainText) : null;
  const yieldRows = trees?.props.m_aYields?.list ?? [];
  const soft = chopPerLog(yieldRows, "SOFTWOOD", a.treeSize), hard = chopPerLog(yieldRows, "HARDWOOD", a.treeSize);
  if (!soft || !hard) return { loops: [], sources, problems: [`no size-${a.treeSize} softwood and hardwood yield row in Configs/Lumber/LF_TreeSpecies.conf: lumber loops skipped`] };
  const machines = chains?.props.m_aMachines?.list ?? [];
  const out = {
    plank: outputPerLog(machines, SOFTWOOD_LOG, GOODS.plank.guid), post: outputPerLog(machines, SOFTWOOD_LOG, GOODS.post.guid),
    firewood: outputPerLog(machines, SOFTWOOD_LOG, GOODS.firewood.guid), board: outputPerLog(machines, HARDWOOD_LOG, GOODS.board.guid),
    hardFirewood: outputPerLog(machines, HARDWOOD_LOG, GOODS.firewood.guid),
  };
  for (const [k, v] of Object.entries(out)) if (v === null) problems.push(`no sawmill choice for ${k} in Configs/Resources/ResourceChains.conf`);
  if (problems.length) return { loops: [], sources, problems: [...problems, "lumber loops skipped"] };

  const demand = listOf(addonsDir, "buyerDemand.rows", packLayers, owner);
  const goods = listOf(addonsDir, "market.goods", packLayers, owner);
  const buyers = {}, curves = {};
  for (const [k, g] of Object.entries(GOODS)) {
    const rows = demand.filter((r) => r.id.split("|")[1] === g.guid && num(r.f("price")) > 0);
    // The best-priced premium row is the buyer the loop sells to (a good with several buyers uses the dearest).
    const row = rows.sort((x, y) => num(y.f("price")) - num(x.f("price")))[0];
    if (row) { buyers[k] = { price: num(row.f("price")), cap: num(row.f("cap")), refillPerHour: num(row.f("refillPerHour")) }; sources.push(`${k}: premium row ${row.id.split("|")[0]}`); }
    else { buyers[k] = FALLBACK_BUYERS[k]; sources.push(`${k}: premium assumed (no buyerDemand row)`); }
    const good = goods.find((r) => r.id === g.guid);
    if (good) curves[k] = { base: num(good.f("base")), floor: num(good.f("floor")), dropPerUnitPct: num(good.f("dropPerUnitPct")), recoverPctPerHour: num(good.f("recoverPctPerHour")) };
    else { curves[k] = FALLBACK_CURVES[k]; sources.push(`${k}: wholesale assumed (no market row)`); }
  }
  const loops = computeLoops({ assumptions: a, yields: { softwood: soft, hardwood: hard }, outputs: out, buyers, curves, duty });
  return { loops, sources, problems, assumptions: a, buyers, curves };
}

const money = (n) => `$${n}`;

/** The lines `tools\cli economy` prints for the lumber section. */
export function renderLumber(l) {
  const L = [];
  for (const p of l.problems) L.push(`  problem: ${p}`);
  if (!l.loops.length) return L;
  const a = l.assumptions;
  L.push(`lumber loops (a size-${a.treeSize} tree; hand haul ${a.haulMetres} m at ${a.walkMps} m/s loaded, ${a.runMps} m/s back; steady = purse refill only, first = full purse, wholesale ${a.carryCurve ? "carries over between hours" : "back at base each hour"}):`);
  for (const x of l.loops) {
    const duty = x.ratios.map((r) => `${r.job} ${money(r.pay)} = ${r.ratio === Infinity ? "inf" : r.ratio.toFixed(1)}x`).join(", ");
    L.push(`  LOOP ${x.label}: ~${x.logsPerHour} logs/h, steady ${money(x.steady)}/h, first hour ${money(x.first)}/h; duty ${duty} ${rungText(x)}`);
    L.push(`    sells ${x.detail.map((d) => `${d.unitsPerHour} ${d.good}/h (${d.premiumSteady} premium steady)`).join(", ")}${x.note ? `; ${x.note}` : ""}`);
  }
  L.push(`  sources: ${l.sources.join("; ")}`);
  return L;
}

/** FLAG rows for the loops below the rung, in the report's flag shape. */
export function lumberFlags(l) {
  return l.loops.filter((x) => x.flagged).map((x) => ({
    kind: "lumber-duty", id: `lumber:${x.id}`,
    text: `lumber loop ${x.label} pays ${money(x.steady)}/h steady; duty pay is ${x.ratios.map((r) => `${r.ratio === Infinity ? "inf" : r.ratio.toFixed(1)}x (${r.job} ${money(r.pay)})`).join(" and ")}, under the $${x.floor}/h Cadet shift`,
  }));
}
