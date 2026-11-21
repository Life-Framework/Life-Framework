// The wheat loops of `tools\cli economy` (#3622, H10 of #3282): what a harvester earns an hour selling grain to the
// granary (a wholesale curve) and what a harvester with a mill earns when the bakery also buys flour (a premium
// row), from the merged configs, and whether duty pay (police and medic rank 0) still sits 1.5 to 2.5 times above
// it (the maintainer's pay band, 2026-11-18).
//
// A price here is per LOT (#3622): grain is counted in kg and priced per `lotSize` kg, so a granary lot of 200 kg at
// $9 is $0.045 a kg and a bakery lot of 5 kg at $1 is $0.20 a kg. The curve falls per lot sold, not per kg.
//
// Read from config (core, then the pack overlays through tools/pack/layers.mjs): the grain curve (market.goods) and
// the bakery's flour row (buyerDemand.rows). With either missing the wheat loops are skipped and the report says so.
// Not in any config, so stated here and printed (docs spike section 6): a 2,600 kg field (260 cells of 10 kg) cut in
// about 15 minutes, so 4 fields an hour; the mill turns 2 kg of grain into 1 kg of flour, 50 kg a 2 minute batch
// (1,500 kg of grain an hour). A premium purse holds `cap` kg and refills `refillPerHour` kg an hour: the FIRST hour
// sells from a full purse, the STEADY hour from the refill only. Flour the purse does not take is not milled.
import { SECTIONS, mergeList, sectionLayers } from "../pack/layers.mjs";
import { DUTY_BAND, bandCheck } from "./economy-lumber.mjs";

export const GRAIN_GUID = "7B2E5A1C93D40F11";
export const FLOUR_GUID = "7B2E5A1C93D40F12";
export const WHEAT_ASSUMPTIONS = {
  fieldKg: 2600, fieldsPerHour: 4, millKgPerHour: 1500, grainPerFlour: 2,
  /** false: every hour opens at the wholesale base price (the lumber convention). true: the curve carries over and only recovers. */
  carryCurve: false,
};
const num = (v, d = 0) => (typeof v === "number" ? v : d);

/** One hour of selling `kg` at a per-lot curve now at `price`: { cash, price (after), lots }. Linear drop per lot, held at the floor. */
export function sellLots(curve, price, kg) {
  const lot = Math.max(1, curve.lotSize || 1);
  const lots = Math.floor(Math.max(kg, 0) / lot);
  const drop = (curve.base * curve.dropPerUnitPct) / 100;
  let cash = 0, p = price;
  for (let i = 0; i < lots; i++) {
    cash += p;
    p = Math.max(curve.floor, p - drop);
  }
  return { cash, price: p, lots };
}

/** One hour closes `recoverPctPerHour` of the gap to the base. */
const recover = (curve, price) => Math.min(curve.base, price + (curve.base - price) * curve.recoverPctPerHour / 100);

/**
 * Hour by hour: the harvest `kg` an hour, `milledKg` of it (at most the mill's capacity) turned into flour for the
 * bakery's purse, the rest of the grain sold at the granary curve. Returns the cash of hour 1 and of the steady hour.
 * `flour` is null for the grain-only loop.
 */
export function simulateWheat(kg, curve, flour, a, hours = 24) {
  let remaining = flour ? flour.cap : 0, price = curve.base;
  let first = 0, steady = 0, flourFirst = 0, flourSteady = 0;
  for (let h = 1; h <= hours; h++) {
    let flourKg = 0;
    if (flour) {
      const lot = Math.max(1, flour.lotSize || 1);
      const makeable = Math.min(a.millKgPerHour, kg) / a.grainPerFlour;
      flourKg = Math.floor(Math.min(makeable, remaining) / lot) * lot;
    }
    const grainKg = kg - flourKg * a.grainPerFlour;
    const sold = sellLots(curve, price, grainKg);
    const flourCash = flour ? (flourKg / Math.max(1, flour.lotSize || 1)) * flour.price : 0;
    const cash = flourCash + sold.cash;
    if (h === 1) { first = cash; flourFirst = flourKg; }
    steady = cash; flourSteady = flourKg;
    if (flour) remaining = Math.min(flour.cap, remaining - flourKg + flour.refillPerHour);
    price = a.carryCurve ? recover(curve, sold.price) : curve.base;
  }
  return { first, steady, flourFirst, flourSteady, grainSteady: kg - flourSteady * a.grainPerFlour };
}

/** The loops from resolved inputs (pure: the selftest feeds it directly). */
export function computeWheatLoops(inp) {
  const a = inp.assumptions;
  const kg = a.fieldKg * a.fieldsPerHour;
  const loops = [];
  const make = (id, label, flour, note) => {
    const s = simulateWheat(kg, inp.curve, flour, a);
    loops.push({
      id, label, kgPerHour: kg, first: Math.round(s.first), steady: Math.round(s.steady),
      flourKgSteady: Math.round(s.flourSteady), grainKgSteady: Math.round(s.grainSteady), note, ...bandCheck(s.steady, inp.duty),
    });
  };
  make("wheat-granary", "harvest -> granary (grain at wholesale)", null, `${a.fieldsPerHour} fields of ${a.fieldKg} kg an hour`);
  if (inp.flour) make("wheat-milled", "harvest -> mill -> bakery flour, rest to the granary", inp.flour, `mill ${a.millKgPerHour} kg grain an hour, ${a.grainPerFlour} kg grain a kg of flour`);
  return loops;
}

/** Everything `buildReport` needs: reads the merged sections, returns { loops, sources, problems }. */
export function buildWheat({ addonsDir, packLayers, owner, duty, opts = {} }) {
  const a = { ...WHEAT_ASSUMPTIONS, ...(opts.wheat ?? {}) };
  if (opts.lumber?.carryCurve) a.carryCurve = true;
  const sources = [], problems = [];
  const goods = listOf(addonsDir, "market.goods", packLayers, owner);
  const demand = listOf(addonsDir, "buyerDemand.rows", packLayers, owner);
  const good = goods.find((r) => r.id === GRAIN_GUID);
  if (!good) return { loops: [], sources, problems: [], assumptions: a };
  const curve = {
    base: num(good.f("base")), floor: num(good.f("floor")), dropPerUnitPct: num(good.f("dropPerUnitPct")),
    recoverPctPerHour: num(good.f("recoverPctPerHour")), lotSize: Math.max(1, num(good.f("lotSize"), 1)),
  };
  sources.push(`grain: wholesale curve ${curve.base} to ${curve.floor} per ${curve.lotSize} kg lot`);
  const row = demand.filter((r) => r.id.split("|")[1] === FLOUR_GUID && num(r.f("price")) > 0).sort((x, y) => num(y.f("price")) - num(x.f("price")))[0];
  let flour = null;
  if (row) {
    flour = { price: num(row.f("price")), cap: num(row.f("cap")), refillPerHour: num(row.f("refillPerHour")), lotSize: Math.max(1, num(row.f("lotSize"), 1)) };
    sources.push(`flour: premium row ${row.id.split("|")[0]}, ${flour.price} per ${flour.lotSize} kg lot`);
  } else problems.push("no premium flour row: the milled loop is skipped");
  const loops = computeWheatLoops({ assumptions: a, curve, flour, duty });
  return { loops, sources, problems, assumptions: a, curve, flour };
}

/** The merged list section as [{ id, f(name) }]. */
function listOf(addonsDir, name, packLayers, owner) {
  const merged = mergeList(sectionLayers(addonsDir, SECTIONS[name], packLayers, owner));
  return [...merged.entries.values()].filter((e) => e.present).map((e) => ({ id: e.id, f: (n) => e.fields.get(n)?.value }));
}

const money = (n) => `$${n}`;

/** The lines `tools\cli economy` prints for the wheat section. */
export function renderWheat(w) {
  const L = [];
  for (const p of w.problems) L.push(`  problem: ${p}`);
  if (!w.loops.length) return L;
  const a = w.assumptions;
  L.push(`wheat loops (a ${a.fieldKg} kg field cut ${a.fieldsPerHour} times an hour; steady = purse refill only, first = full purse, wholesale ${a.carryCurve ? "carries over between hours" : "back at base each hour"}):`);
  for (const x of w.loops) {
    const duty = x.ratios.map((r) => `${r.job} ${money(r.pay)} = ${r.ratio === Infinity ? "inf" : r.ratio.toFixed(1)}x`).join(", ");
    L.push(`  LOOP ${x.label}: steady ${money(x.steady)}/h, first hour ${money(x.first)}/h; duty ${duty} ${x.flagged ? "OUT OF BAND" : "in band"} (${DUTY_BAND.min} to ${DUTY_BAND.max})`);
    L.push(`    sells ${x.grainKgSteady} kg grain and ${x.flourKgSteady} kg flour an hour (steady); ${x.note}`);
  }
  L.push(`  sources: ${w.sources.join("; ")}`);
  return L;
}

/** FLAG rows for the loops outside the band, in the report's flag shape. */
export function wheatFlags(w) {
  return w.loops.filter((x) => x.flagged).map((x) => ({
    kind: "wheat-duty", id: `wheat:${x.id}`,
    text: `wheat loop ${x.label} pays ${money(x.steady)}/h steady; duty pay is ${x.ratios.map((r) => `${r.ratio === Infinity ? "inf" : r.ratio.toFixed(1)}x (${r.job} ${money(r.pay)})`).join(" and ")}, outside ${DUTY_BAND.min} to ${DUTY_BAND.max}`,
  }));
}
