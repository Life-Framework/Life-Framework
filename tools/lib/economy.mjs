// The newcomer's money ladder (#3122): what a new character earns and can buy by hour 1, 2 and 4 on one
// mission header, read from the merged configs with no game. Layers, lowest first: the core .conf, the
// owner's server/flagship/config.json, then the header's pack overlays; the pack wins (the maintainer's
// rule for this report), the owner file fills what no pack sets. List sections go through the same offline
// merge `pack check` and `pack explain` use (tools/pack/layers.mjs); nothing here parses a .conf itself.
//
// Three numbers live in script, not in a config, and are read from the script text: the opening bank
// (LF_SpawnLogic.m_iStartingBank) and the run pay (LF_CivilianJob.PAY_PARCEL / PAY_FUEL / PAY_TAXI). A run's
// length is not in any file either, so the hourly figure uses RUN_MINUTES, an assumption the report prints.
import { existsSync, readFileSync, readdirSync } from "node:fs";
import { join } from "node:path";
import { parseConf, guidOf } from "../economy/profit-loops.mjs";
import { SECTIONS, loadOwner, loadPackOverlays, mergeList, metaIndex, sectionLayers } from "../pack/layers.mjs";

/** Minutes a courier, fuel or taxi run takes on the real map (an assumption; `--run-minutes` overrides). */
export const RUN_MINUTES = { parcel: 6, fuel: 8, taxi: 10 };
/** The job level each run type needs (LF_CivilianJob.LevelFor): parcel 1, fuel 2, taxi 3. */
const RUN_LEVEL = { parcel: 1, fuel: 2, taxi: 3 };
const PAY_CONST = { parcel: "PAY_PARCEL", fuel: "PAY_FUEL", taxi: "PAY_TAXI" };
const HOURS = [1, 2, 4];

const read = (p) => (existsSync(p) ? readFileSync(p, "utf8") : "");
const num = (v, d = 0) => (typeof v === "number" ? v : d);
const fieldOf = (entry, name) => entry.fields.get(name)?.value;
const nameOfRes = (res) => String(res ?? "").replace(/^\{[0-9A-Fa-f]+\}/, "").replace(/^.*\//, "").replace(/\.et$/, "");

/** The addon folder a mission names: the folder itself, or the one holding Missions/<name>.conf. */
export function resolveMission(addonsDir, mission) {
  if (existsSync(join(addonsDir, mission, "Missions"))) return mission;
  for (const a of existsSync(addonsDir) ? readdirSync(addonsDir) : []) if (existsSync(join(addonsDir, a, "Missions", `${mission}.conf`))) return a;
  return null;
}

/** Scalar members of one config class across the core file, the owner section and the pack overlays (pack wins). */
function scalars(core, ownerSection, overlays, cls, ownerKeys = {}) {
  const out = {};
  const take = (props, source) => {
    for (const [k, v] of Object.entries(props ?? {})) if (typeof v === "number" || typeof v === "string") out[k] = { value: v, source };
  };
  take(core?.props, "core");
  if (ownerSection) {
    const mapped = {};
    for (const [conf, json] of Object.entries(ownerKeys)) if (ownerSection[json] !== undefined) mapped[conf] = ownerSection[json];
    take(mapped, "owner");
  }
  for (const o of overlays) if (o.root.cls === cls) take(o.root.props, "pack");
  return out;
}

/** `static const int NAME = 40;` or an `[Attribute("1000", ...)] protected int m_x;` default, from script text. */
const constIn = (text, name) => {
  const m = new RegExp(`\\b${name}\\s*=\\s*(-?\\d+)\\s*;`).exec(text);
  return m ? Number(m[1]) : null;
};
const attributeDefault = (text, member) => {
  const m = new RegExp(`\\[Attribute\\("(-?\\d+)"[^\\]]*\\]\\s*(?:protected\\s+|private\\s+)?int\\s+${member}\\b`).exec(text);
  return m ? Number(m[1]) : null;
};

/** Merged list section -> [{ id, f(name) }] of the entries a layer still has. */
function listOf(addonsDir, listName, packLayers, owner) {
  const def = SECTIONS[listName];
  const merged = mergeList(sectionLayers(addonsDir, def, packLayers, owner));
  return [...merged.entries.values()].filter((e) => e.present).map((e) => ({ id: e.id, f: (n) => fieldOf(e, n) }));
}

/** Runs needed to reach `level` on the DELIVERY track (the courier path), from the merged thresholds. */
function runsToLevel(level, thresholds, xpPerRun) {
  if (level <= 1) return 0;
  const need = thresholds[level - 2];
  if (typeof need !== "number" || xpPerRun <= 0) return null;
  return Math.ceil(need / xpPerRun);
}

/** The whole report as data. `root` holds addons/ and server/flagship/config.json. */
export function buildReport(root, opts = {}) {
  const addonsDir = join(root, "addons");
  const mission = opts.mission ?? "LF_Everon1989";
  const pack = resolveMission(addonsDir, mission);
  if (!pack) return { error: `no mission '${mission}': no addons/${mission}/Missions and no addon with Missions/${mission}.conf` };
  const meta = metaIndex(addonsDir);
  const loaded = loadPackOverlays(addonsDir, pack, meta);
  const packLayers = [{ pack, overlays: loaded.overlays }];
  const ownerFile = opts.owner ?? join(root, "server", "flagship", "config.json");
  const ownerRes = existsSync(ownerFile) ? loadOwner(ownerFile) : { json: null };
  if (ownerRes.error) return { error: ownerRes.error };
  const owner = ownerRes.json ? { json: ownerRes.json, file: "config.json" } : null;
  const cfg = (...p) => join(addonsDir, "LifeFramework", "Configs", ...p);
  const core = (...p) => { const t = read(cfg(...p)); return t ? parseConf(t) : null; };
  const flags = [];
  const runMinutes = { ...RUN_MINUTES, ...(opts.runMinutes ?? {}) };

  // ---- Faucets ------------------------------------------------------------------------------
  const spawnText = read(join(addonsDir, "LifeFramework", "Scripts", "Game", "Feature", "Character", "Spawning", "LF_SpawnLogic.c"));
  const jobText = read(join(addonsDir, "LifeFramework", "Scripts", "Game", "Feature", "Jobs", "LF_CivilianJob.c"));
  const startBank = attributeDefault(spawnText, "m_iStartingBank");
  const jobs = scalars(core("Jobs", "LF_Jobs.conf"), owner?.json?.jobs, loaded.overlays, "LF_JobsConfig", {
    m_fPoliceDutyPayPerSecond: "policeDutyPayPerSecond", m_iBenefitAmount: "benefitAmount", m_fBenefitCooldownSeconds: "benefitCooldownSeconds",
  });
  const jv = (k, d = 0) => num(jobs[k]?.value, d);
  const tracks = listOf(addonsDir, "jobs.tracks", packLayers, owner);
  const awards = listOf(addonsDir, "jobs.xpAwards", packLayers, owner);
  const courierTrack = tracks.find((t) => t.id === "DELIVERY");
  // A one-line number list parses as one string ("500 1500 3500"): split it.
  const thresholds = (courierTrack?.f("levelThresholds")?.list ?? []).flatMap((x) => String(x).split(/\s+/)).filter(Boolean).map(Number);
  const xpPerRun = num(awards.find((a) => a.id === "delivery_run")?.f("xp"), 0);

  const runs = Object.keys(RUN_MINUTES).map((type) => {
    const pay = constIn(jobText, PAY_CONST[type]);
    const unlock = runsToLevel(RUN_LEVEL[type], thresholds, xpPerRun);
    const minutes = runMinutes[type];
    return { job: type, pay: pay ?? 0, payKnown: pay !== null, unlockRuns: unlock, minutes, perHour: Math.round((pay ?? 0) * 60 / minutes) };
  });
  for (const r of runs) if (r.pay <= 0) flags.push({ kind: "zero-pay", id: r.job.toUpperCase(), text: `the ${r.job.toUpperCase()} run pays ${r.pay}${r.payKnown ? "" : " (no PAY_ constant found)"}` });

  const benefit = { amount: jv("m_iBenefitAmount"), cooldownSeconds: jv("m_fBenefitCooldownSeconds"), threshold: jv("m_iBenefitBalanceThreshold") };
  benefit.perHour = benefit.cooldownSeconds > 0 ? Math.round(benefit.amount * 3600 / benefit.cooldownSeconds) : 0;
  const duty = {
    police: Math.round(jv("m_fPoliceDutyPayPerSecond") * 3600),
    medic: Math.round(jv("m_fMedicDutyPayPerSecond") * 3600),
  };
  if (benefit.amount <= 0) flags.push({ kind: "zero-pay", id: "benefit", text: "the unemployment benefit pays 0" });
  for (const [k, v] of Object.entries(duty)) if (v <= 0) flags.push({ kind: "zero-pay", id: `${k}-duty`, text: `${k} duty pays 0 per hour` });

  // ---- Sinks --------------------------------------------------------------------------------
  const veh = scalars(core("Vehicles", "LF_VehiclePrices.conf"), owner?.json?.vehicles, loaded.overlays, "LF_VehiclePricesConfig", { m_iBuyPercent: "buyPercent" });
  const buyPercent = num(veh.m_iBuyPercent?.value, 100);
  const cars = listOf(addonsDir, "vehicles.vehicles", packLayers, owner)
    .map((v) => ({ id: nameOfRes(v.f("prefab")) || v.id, price: Math.round(num(v.f("basePrice")) * buyPercent / 100) })).filter((c) => c.price > 0);
  const plots = listOf(addonsDir, "property.plots", packLayers, owner).map((p) => ({ id: p.id, price: num(p.f("price")) })).filter((p) => p.price > 0);
  const items = listOf(addonsDir, "catalogue.items", packLayers, owner);
  const offers = listOf(addonsDir, "catalogue.offers", packLayers, owner);
  const permitItem = items.find((i) => /permit/i.test(i.id) && num(i.f("price")) > 0);
  const permitOffer = offers.find((o) => /permit/i.test(o.id) && num(o.f("price")) > 0);
  // The licence desk's fee: the owner's licences.permitFee, else LF_LicencesConfig.DEFAULT_PERMIT_FEE (script).
  const licenceText = read(join(addonsDir, "LifeFramework", "Scripts", "Game", "Feature", "License", "LF_LicencesConfig.c"));
  const deskFee = typeof owner?.json?.licences?.permitFee === "number" ? owner.json.licences.permitFee : constIn(licenceText, "DEFAULT_PERMIT_FEE");
  const permit = deskFee > 0 ? { id: "licence desk fee", price: deskFee }
    : permitOffer ? { id: permitOffer.id, price: num(permitOffer.f("price")) }
      : permitItem ? { id: permitItem.id, price: num(permitItem.f("price")) } : null;
  const clothes = items.filter((i) => i.f("category") === "clothing" && num(i.f("price")) > 0).map((i) => ({ id: i.id, price: num(i.f("price")) }));
  const cheapest = (list) => (list.length ? list.reduce((a, b) => (b.price < a.price ? b : a)) : null);
  const sinks = { car: cheapest(cars), plot: cheapest(plots), permit, clothing: cheapest(clothes) };

  // ---- The ladder: a courier who always takes the best-paying unlocked run -------------------
  const startCash = 0;
  const ladder = HOURS.map((h) => {
    let minutes = 0, done = 0, earned = 0;
    const count = {};
    for (;;) {
      const open = runs.filter((r) => r.pay > 0 && r.unlockRuns !== null && r.unlockRuns <= done).sort((a, b) => b.perHour - a.perHour);
      if (!open.length || minutes + open[0].minutes > h * 60) break;
      minutes += open[0].minutes; earned += open[0].pay; done += 1; count[open[0].job] = (count[open[0].job] ?? 0) + 1;
    }
    const total = (startBank ?? 0) + startCash + earned;
    const buys = Object.fromEntries(Object.entries(sinks).map(([k, s]) => [k, s ? s.price <= total : null]));
    return { hour: h, runs: done, byJob: count, earned, total, buys };
  });

  // ---- Flags --------------------------------------------------------------------------------
  // A buy-low sell-high loop: a shop (or the catalogue) pays more for a good than it charges for it.
  const defaults = scalars(core("Shop", "ShopPrices.conf"), owner?.json?.shops, loaded.overlays, "LF_ShopPricesConfig", { m_iDefaultBuyPrice: "defaultBuyPrice", m_iDefaultSellPrice: "defaultSellPrice" });
  const defBuy = num(defaults.m_iDefaultBuyPrice?.value, 50), defSell = num(defaults.m_iDefaultSellPrice?.value, 25);
  for (const r of listOf(addonsDir, "shops.priceRules", packLayers, owner)) {
    if (r.f("hidden")) continue;
    const buy = num(r.f("buyPrice"), defBuy), sell = num(r.f("sellPrice"), defSell);
    const what = r.f("prefab") ? nameOfRes(r.f("prefab")) : r.f("find") || r.id;
    if (sell > buy) flags.push({ kind: "loop", id: `shop:${what}`, text: `buy-low sell-high loop: the shop sells ${what} for ${buy} and buys it back for ${sell} (+${sell - buy} per round)` });
  }
  for (const i of items) {
    const price = num(i.f("price")), sell = num(i.f("sellPrice"));
    if (price > 0 && sell > price) flags.push({ kind: "loop", id: `catalogue:${i.id}`, text: `buy-low sell-high loop: catalogue item ${i.id} costs ${price} and sells for ${sell}` });
  }
  const rules = listOf(addonsDir, "shops.priceRules", packLayers, owner);
  for (const g of listOf(addonsDir, "market.goods", packLayers, owner)) { // a good's id is its prefab GUID
    const rule = rules.find((r) => guidOf(r.f("prefab")) === g.id);
    const buy = rule ? num(rule.f("buyPrice"), defBuy) : null;
    const what = rule ? nameOfRes(rule.f("prefab")) : g.id;
    if (buy !== null && num(g.f("ceiling")) > buy) flags.push({ kind: "loop", id: `market:${what}`, text: `buy-low sell-high loop: ${what} is sold by a shop for ${buy}; the market pays up to ${num(g.f("ceiling"))}` });
  }

  // A weighted fish with no prefab: a species a table can roll whose every size prefab is empty.
  const fishRoots = [parseConf(read(cfg("Fishing", "LF_Fishing.conf"))), ...loaded.overlays.filter((o) => o.root.cls === "LF_FishingConfig").map((o) => o.root)].filter(Boolean);
  const species = new Map();
  const weighted = new Set();
  for (const fr of fishRoots) {
    for (const s of fr.props.m_aSpecies?.list ?? []) species.set(s.props.m_sId, (s.props.m_aSizePrefabs?.list ?? []).filter((p) => /\{[0-9A-Fa-f]{16}\}/.test(String(p)))); // a one-line list is one string: count the GUIDs, not the quotes
    for (const t of fr.props.m_aTables?.list ?? []) for (const w of t.props.m_aSpecies?.list ?? []) if (num(w.props.m_iWeight) > 0) weighted.add(w.props.m_sSpecies);
  }
  for (const id of [...weighted].sort()) {
    if (species.has(id) && species.get(id).length === 0) flags.push({ kind: "fish", id: `fish:${id}`, text: `weighted fish with no prefab: ${id} can be rolled but has no size prefab, the catch is empty` });
  }

  // A sink with no faucet: something priced to buy, and no income that could ever reach it.
  const income = runs.some((r) => r.pay > 0) || benefit.amount > 0 || duty.police > 0 || duty.medic > 0;
  for (const [k, s] of Object.entries(sinks)) {
    if (s && !income) flags.push({ kind: "sink", id: `sink:${k}`, text: `sink with no faucet: the cheapest ${k} costs ${s.price} and nothing pays` });
  }
  const last = ladder[ladder.length - 1];
  for (const [k, s] of Object.entries(sinks)) {
    if (s && income && last.buys[k] === false) flags.push({ kind: "slow", id: `slow:${k}`, text: `the cheapest ${k} (${s.id}, ${s.price}) is out of reach after ${last.hour} h on the best run (${last.total} banked)` });
  }

  return {
    mission, pack, overlays: loaded.overlays.map((o) => o.file), problems: loaded.problems.map((p) => p.msg), ownerFile: owner ? ownerFile : null,
    start: { bank: startBank, cash: startCash }, runs, benefit, duty, sinks, ladder, flags, assumptions: { runMinutes },
  };
}

const money = (n) => `$${n}`;

/** The report as the lines `tools\cli economy` prints. */
export function renderReport(r) {
  const L = [];
  L.push(`economy: mission ${r.mission} (core + ${r.pack}${r.overlays.length ? `, ${r.overlays.length} overlays` : ""}${r.ownerFile ? " + server config.json" : ""})`);
  for (const p of r.problems) L.push(`  problem: ${p}`);
  L.push(`start: bank ${r.start.bank === null ? "?" : money(r.start.bank)}, cash ${money(r.start.cash)}`);
  L.push("runs (pay per run, unlock after N courier runs, ~min per run is an assumption):");
  for (const x of r.runs) L.push(`  ${x.job.toUpperCase().padEnd(7)} pay ${money(x.pay)}  unlock ${x.unlockRuns === null ? "never" : x.unlockRuns + " runs"}  ~${x.minutes} min  ${money(x.perHour)}/h`);
  L.push(`benefit: ${money(r.benefit.amount)} per ${r.benefit.cooldownSeconds} s below ${money(r.benefit.threshold)} = ${money(r.benefit.perHour)}/h`);
  L.push(`duty pay: police ${money(r.duty.police)}/h, medic ${money(r.duty.medic)}/h`);
  const sink = (k, label) => {
    const s = r.sinks[k];
    L.push(`  ${label.padEnd(8)} ${s ? `${money(s.price)}  (${s.id})` : "none priced"}`);
  };
  L.push("cheapest purchase:");
  sink("car", "car"); sink("plot", "plot"); sink("permit", "permit"); sink("clothing", "clothing");
  L.push("ladder (a courier taking the best unlocked run, banking everything):");
  for (const h of r.ladder) {
    const jobsDone = Object.entries(h.byJob).map(([j, n]) => `${n} ${j}`).join(", ") || "none";
    const buys = Object.entries(h.buys).filter(([, v]) => v !== null).map(([k, v]) => `${k} ${v ? "yes" : "no"}`).join(", ");
    L.push(`  hour ${h.hour}: ${h.runs} runs (${jobsDone}), earned ${money(h.earned)}, total ${money(h.total)}; can afford: ${buys}`);
  }
  if (r.flags.length === 0) L.push("flags: none");
  else {
    L.push("flags:");
    for (const f of r.flags) L.push(`  FLAG ${f.kind}: ${f.text}`);
  }
  return L;
}
