// The on-foot gathering loops of `tools\cli economy` (pay audit 2026-11-27): fishing at each placed spot and the
// orchard apples pressed to juice. Read from config and the live world's layers, no game needed:
//   fishing: Configs/Fishing/LF_Fishing.conf plus the pack's LF_FishingConfig overlays (species, bite hours, bait
//            boost, table weights), every LF_FishingSpot.et in the pack's MainWorld layers (table, stock, regrow), and
//            the highest price any pack trader prefab pays for each fish (the dock wholesale buyer).
//   orchard: the apple trees in the MainWorld layers, the apple node's regrow (Configs/Resources/ResourceChains.conf),
//            the juicer's apples a bottle, and the juice price at the pack's traders.
// A spot's steady catch is its regrow (one fish every m_fRegrowSeconds): a player with bait lands fish faster than that,
// so the spot, not the rod, sets the hour. The fish value is averaged over a whole day, because a species bites only in
// its hours and a 2 h day cycles every hour of the clock inside one session. A loop under the lowest duty shift is
// flagged, like the lumber, wheat and mining loops.
import { existsSync, readFileSync, readdirSync, statSync } from "node:fs";
import { join } from "node:path";
import { parseConf, guidOf } from "../economy/profit-loops.mjs";
import { LADDER, rungCheck, rungText } from "./economy-lumber.mjs";

const read = (p) => (existsSync(p) ? readFileSync(p, "utf8") : "");
const num = (v, d = 0) => (typeof v === "number" && Number.isFinite(v) ? v : d);
/** A conf list as values: a one-line list parses as one string ("6 3 1", "{A}x.et\" \"{B}y.et"), so split it. */
const words = (node) => (node?.list ?? []).flatMap((x) => String(x).split(/"\s+"|\s+/)).map((s) => s.replace(/"/g, "")).filter(Boolean);

/** Assumptions the configs do not hold. */
export const GATHER_ASSUMPTIONS = {
  bait: true, // the manual's loop: rod and bait from the seed and feed store
  rodCost: 60, // a worn-out rod replaced roughly once a long session; left out of the $/h (a few dollars an hour)
  handleSeconds: 5, // cast, strike and the fish into the pack, on top of the bite wait
};

function walk(dir, out = []) {
  if (!existsSync(dir)) return out;
  for (const name of readdirSync(dir)) {
    const p = join(dir, name);
    if (statSync(p).isDirectory()) walk(p, out);
    else if (name.endsWith(".layer")) out.push(p);
  }
  return out;
}

/** The live world's layers of a pack: Worlds/MainWorld, or every world when the pack has none. */
export function liveLayers(addonsDir, pack) {
  const main = join(addonsDir, pack, "Worlds", "MainWorld");
  return walk(existsSync(main) ? main : join(addonsDir, pack, "Worlds"));
}

/** Every fishing spot in the layer texts: [{ id, table, stock, regrowSeconds, file }] (prefab defaults filled in). */
export function spotsOf(layerTexts) {
  const spots = [];
  for (const { file, text } of layerTexts) {
    const re = /^(\S+) (\S+) : "\{[0-9A-Fa-f]{16}\}Prefabs\/Systems\/Fishing\/LF_FishingSpot\.et" \{([\s\S]*?)\n\}/gm;
    for (const m of text.matchAll(re)) {
      const body = m[3];
      const get = (k) => body.match(new RegExp(`\\b${k}\\s+("?)([^"\\n]*)\\1`))?.[2];
      spots.push({
        id: get("m_sId") || m[2], table: get("m_sTable") || "harbour",
        stock: Number(get("m_iStock") ?? 12), regrowSeconds: Number(get("m_fRegrowSeconds") ?? 120), file,
      });
    }
  }
  return spots;
}

/** The highest unit price any trader prefab text pays per prefab GUID: Map<GUID, $>. */
export function traderPrices(prefabTexts) {
  const best = new Map();
  for (const text of prefabTexts) {
    for (const m of text.matchAll(/m_ItemPrefab "\{([0-9A-Fa-f]{16})\}[^"]*"\s+m_ValuePerItem (\d+)/g)) {
      const g = m[1].toUpperCase();
      best.set(g, Math.max(best.get(g) ?? 0, Number(m[2])));
    }
  }
  return best;
}

/** The fishing config merged from roots (core first, overlays after): { species: Map, tables: Map, baitBoost, boosted }. */
export function fishingModel(roots) {
  const species = new Map();
  const tables = new Map();
  let baitBoost = 1;
  const boosted = new Set();
  // LF_FishingConfig defaults (LF_FishingConfig.c attributes) unless a root sets them.
  const timing = { biteMin: 15, biteMax: 60, emptySlowdown: 1.5, open: "open_water", rodSpeed: 1, baitSpeed: 1 };
  for (const r of roots) {
    const p = r.props;
    if (typeof p.m_fBaitBoost === "number") baitBoost = p.m_fBaitBoost;
    if (typeof p.m_fBiteMinSeconds === "number") timing.biteMin = p.m_fBiteMinSeconds;
    if (typeof p.m_fBiteMaxSeconds === "number") timing.biteMax = p.m_fBiteMaxSeconds;
    if (typeof p.m_fEmptySpotSlowdown === "number") timing.emptySlowdown = p.m_fEmptySpotSlowdown;
    if (typeof p.m_sOpenWaterTable === "string" && p.m_sOpenWaterTable) timing.open = p.m_sOpenWaterTable;
    // The starter rod and bait: the first rows (the seed and feed store sells them).
    const rod = p.m_aRods?.list?.[0]?.props;
    if (rod && typeof rod.m_fBiteSpeed === "number") timing.rodSpeed = rod.m_fBiteSpeed;
    const bait = p.m_aBaits?.list?.[0]?.props;
    if (bait && typeof bait.m_fBiteSpeed === "number") timing.baitSpeed = bait.m_fBiteSpeed;
    for (const b of p.m_aBaits?.list ?? []) for (const s of words(b.props.m_aBoostSpecies)) boosted.add(s);
    for (const s of p.m_aSpecies?.list ?? []) {
      const sizes = words(s.props.m_aSizePrefabs).map(guidOf);
      const hours = words(s.props.m_aHours).map(Number);
      const water = words(s.props.m_aWater);
      species.set(s.props.m_sId, { id: s.props.m_sId, sizes, hours, water, minLevel: num(s.props.m_iMinFisherLevel) });
    }
    for (const t of r.props.m_aTables?.list ?? []) {
      const entries = (t.props.m_aSpecies?.list ?? []).map((w) => ({ species: w.props.m_sSpecies, weight: num(w.props.m_iWeight) }));
      tables.set(t.props.m_sId, { id: t.props.m_sId, entries, sizeWeights: words(t.props.m_aSizeWeights).map(Number) });
    }
  }
  return { species, tables, baitBoost, boosted, timing };
}

/** The water kinds a table's species live in (a pond table is POND water): the spot's water for its open-water casts. */
export function tableWater(model, tableId) {
  const t = model.tables.get(tableId);
  return [...new Set((t?.entries ?? []).flatMap((e) => model.species.get(e.species)?.water ?? []))];
}

/**
 * Mean seconds to a bite (LF_FishingRules.BiteSeconds): min + span x roll, divided by rod and bait speed, times the
 * empty-spot slowdown, clamped to [min, max]; the roll is uniform, so this integrates it.
 */
export function meanBiteSeconds(timing, bait, empty) {
  const speed = timing.rodSpeed * (bait ? timing.baitSpeed : 1);
  const n = 1000;
  let sum = 0;
  for (let i = 0; i < n; i++) {
    let s = (timing.biteMin + (timing.biteMax - timing.biteMin) * ((i + 0.5) / n)) / speed;
    if (empty) s *= timing.emptySlowdown;
    sum += Math.min(timing.biteMax, Math.max(timing.biteMin, s));
  }
  return sum / n;
}

/**
 * A table's fish value averaged over the 24 hours of the clock: each hour rolls only the species that bite then
 * (LF_FishingRules.PickSpecies), bait-boosted species weigh baitBoost more, and the size comes from the table's weights.
 * Returns { perFish, deadHours } (deadHours: hours with nothing eligible, counted as no catch).
 */
export function tableValue(model, tableId, prices, bait = true, water = null) {
  const t = model.tables.get(tableId);
  if (!t) return { perFish: 0, deadHours: 24 };
  const sw = t.sizeWeights.length === 3 ? t.sizeWeights : [1, 0, 0];
  const swTotal = sw.reduce((a, b) => a + b, 0) || 1;
  const valueOf = (s) => s.sizes.reduce((a, g, i) => a + (prices.get(g) ?? 0) * (sw[i] ?? 0), 0) / swTotal;
  let sum = 0;
  let dead = 0;
  for (let h = 0; h < 24; h++) {
    let w = 0;
    let v = 0;
    for (const e of t.entries) {
      const s = model.species.get(e.species);
      if (!s || e.weight <= 0 || (s.hours.length && !s.hours.includes(h))) continue;
      if (water && s.water.length && !s.water.some((k) => water.includes(k))) continue; // LivesIn: a pond never gives an ocean fish
      const weight = e.weight * (bait && model.boosted.has(s.id) ? model.baitBoost : 1);
      w += weight;
      v += weight * valueOf(s);
    }
    if (w > 0) sum += v / w;
    else dead++;
  }
  // perFish is the mean over the hours that land a fish; deadHours land nothing (spotLoop scales the hour by them).
  return { perFish: dead < 24 ? Math.round((sum / (24 - dead)) * 100) / 100 : 0, deadHours: dead };
}

/**
 * One spot's hour. A stocked spot bites every `spotBite + handle` seconds; once it is fished out each regrown fish is
 * caught at that pace and the rest of the hour rolls the open-water table (small fish, `openBite + handle` a bite).
 * steady: the spot's regrow plus open water; first: the stock too. v = { perFish, deadHours }, o = the open-water one.
 */
export function spotLoop(spot, v, o, t, duty) {
  const live = (24 - v.deadHours) / 24;
  const spotCycle = t.spotBite + t.handle;
  const openCycle = t.openBite + t.handle;
  const hour = (stockFish) => {
    const spotFish = Math.min(3600 / spotCycle, stockFish + (spot.regrowSeconds > 0 ? 3600 / spot.regrowSeconds : 0)) * live;
    const openFish = Math.max(0, 3600 - spotFish * spotCycle) / openCycle * ((24 - o.deadHours) / 24);
    return { spotFish, openFish, dollars: spotFish * v.perFish + openFish * o.perFish };
  };
  const s = hour(0);
  const f = hour(spot.stock);
  return {
    kind: "fishing", id: spot.id, table: spot.table, perFish: v.perFish, openPerFish: o.perFish,
    fishPerHour: Math.round(s.spotFish * 10) / 10, openFishPerHour: Math.round(s.openFish * 10) / 10,
    steady: Math.round(s.dollars), first: Math.round(f.dollars), rung: rungCheck(Math.round(s.dollars), duty),
  };
}

/** The orchard: trees x (3600 / regrow) apples an hour, pressed `input` to a bottle, sold at `price`. */
export function orchardLoop({ trees, regrowSeconds, maxGathers, input, output, price, duty }) {
  const apples = regrowSeconds > 0 ? trees * (3600 / regrowSeconds) : 0;
  const perApple = input > 0 ? (output * price) / input : 0;
  const steady = Math.round(apples * perApple);
  const first = Math.round((apples + trees * maxGathers) * perApple);
  return { kind: "orchard", id: "apples-to-juice", trees, applesPerHour: Math.round(apples), perApple: Math.round(perApple * 100) / 100, steady, first, rung: rungCheck(steady, duty) };
}

/** Build the gather section from the repo: { fishing: [...], orchard, flags }. */
export function buildGather({ addonsDir, pack, overlays = [], duty }) {
  const layers = liveLayers(addonsDir, pack).map((file) => ({ file, text: read(file) }));
  const traderDir = join(addonsDir, pack, "Prefabs", "Trader");
  const traderTexts = existsSync(traderDir) ? readdirSync(traderDir).filter((n) => n.endsWith(".et")).map((n) => read(join(traderDir, n))) : [];
  const prices = traderPrices(traderTexts);
  const core = parseConf(read(join(addonsDir, "LifeFramework", "Configs", "Fishing", "LF_Fishing.conf")));
  const roots = [core, ...overlays.filter((o) => o.root?.cls === "LF_FishingConfig").map((o) => o.root)].filter(Boolean);
  const model = fishingModel(roots);
  const bait = GATHER_ASSUMPTIONS.bait;
  const t = { spotBite: meanBiteSeconds(model.timing, bait, false), openBite: meanBiteSeconds(model.timing, bait, true), handle: GATHER_ASSUMPTIONS.handleSeconds };
  const fishing = spotsOf(layers).map((s) => {
    const water = tableWater(model, s.table);
    const v = tableValue(model, s.table, prices, bait, water);
    const o = tableValue(model, model.timing.open, prices, bait, water);
    return spotLoop(s, v, o, t, duty);
  });

  const chains = parseConf(read(join(addonsDir, "LifeFramework", "Configs", "Resources", "ResourceChains.conf")));
  const apple = (chains?.props.m_aNodes?.list ?? []).find((n) => n.props.m_sNodeId === "apple");
  const juicer = (chains?.props.m_aMachines?.list ?? []).find((n) => n.props.m_sMachineId === "juicer");
  let orchard = null;
  if (apple && juicer) {
    const inp = juicer.props.m_aInputs?.list?.[0]?.props ?? {};
    const out = juicer.props.m_aOutputs?.list?.[0]?.props ?? {};
    // The apple tree prefab: every tree entity whose prefab is a t_malus_domestica variant with the gather action.
    const trees = layers.reduce((a, l) => a + (l.text.match(/^Tree \S+ : "\{[0-9A-Fa-f]{16}\}Prefabs\/Vegetation\/Tree\/t_malus_domestica_2s\.et"/gm) ?? []).length, 0);
    orchard = orchardLoop({
      trees, regrowSeconds: num(apple.props.m_fRegrowMs) / 1000, maxGathers: num(apple.props.m_iMaxGathers),
      input: num(inp.m_iInputAmount, 1), output: num(out.m_iOutputAmount, 1), price: prices.get(guidOf(out.m_OutputPrefab)) ?? 0, duty,
    });
  }
  return { fishing, orchard, flags: gatherFlags({ fishing, orchard }) };
}

/**
 * A flag for a gathering loop under the lowest duty shift, under the civilian entry rung (police are the lowest paid,
 * so an entry trade pays at least a typical patrol hour), or past the mid rung (a no-vehicle loop above medic pay).
 */
export function gatherFlags(g) {
  const flags = [];
  for (const x of [...(g.fishing ?? []), ...(g.orchard ? [g.orchard] : [])]) {
    if (x.rung.flagged) flags.push({ kind: "gather-duty", id: `gather:${x.kind}:${x.id}`, text: `${x.kind} ${x.id} pays $${x.steady}/h steady, under the $${x.rung.floor}/h Cadet shift` });
    else if (x.steady < LADDER.entry.min) flags.push({ kind: "gather-rung", id: `gather:${x.kind}:${x.id}`, text: `${x.kind} ${x.id} pays $${x.steady}/h steady, under the $${LADDER.entry.min}/h entry rung` });
    else if (x.steady > LADDER.mid.max) flags.push({ kind: "gather-rung", id: `gather:${x.kind}:${x.id}`, text: `${x.kind} ${x.id} pays $${x.steady}/h steady on foot, past the $${LADDER.mid.max}/h mid rung` });
  }
  return flags;
}

/** The lines `tools\cli economy` prints for the gather section. */
export function renderGather(g) {
  const L = ["gathering loops (on foot; steady = the spot or tree regrow, first = the full stock too; fish value averaged over the bite hours, with bait; a fished-out spot gives open-water small fish):"];
  for (const x of g.fishing ?? []) L.push(`  FISH ${x.id} (${x.table} table): ${x.fishPerHour} spot fish/h at $${x.perFish} and ${x.openFishPerHour} open-water fish/h at $${x.openPerFish}, steady $${x.steady}/h, first hour $${x.first}/h; ${rungText(x.rung)}`);
  if (g.orchard) {
    const o = g.orchard;
    L.push(`  ORCHARD apples -> juice: ${o.trees} trees, ${o.applesPerHour} apples/h at $${o.perApple} an apple as juice, steady $${o.steady}/h, first hour $${o.first}/h; ${rungText(o.rung)}`);
  }
  return L;
}
