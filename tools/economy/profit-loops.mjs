// No-profit-loop check (#686). Reads every price the config holds and reports each
// loop that makes money for nothing:
//   shop      an item that buys for less than it sells for (after the owner's `shops` section)
//   recipe    a machine or chain step whose purchasable inputs cost less than its outputs sell for
//   chain     a whole multi-step chain, intermediates netted out, the same test
//   vehicle   a car bought at a lot then sold back, or chopped (cash plus the parts the yard hands over)
// Labour is not priced: a loop with no gathering and no theft still counts, because the check
// lets every input be bought. Gathered nodes, stolen cars, duty pay and quest rewards are labour
// or risk and are out of scope (docs/features.md "No-profit-loop check").
//
// Offline on purpose: every price is a text .conf, a .layer or the owner config.json, and the
// resolver below mirrors LF_ShopPriceResolver, LF_VehiclePriceModel and LF_ChopShopRules.Payout.
// Prices change by design decision, so existing loops go in profit-loop-baseline.json with a reason.
import { readFileSync, existsSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const here = dirname(fileURLToPath(import.meta.url));
export const ROOT = join(here, "..", "..");
const CFG = join(ROOT, "addons", "LifeFramework", "Configs");
const LAYERS = join(ROOT, "addons", "LifeFramework", "Worlds", "DebugWorld", "DebugWorld_Layers");
export const DEFAULT_SOURCES = {
  shop: join(CFG, "Shop", "ShopPrices.conf"),
  chains: join(CFG, "Resources", "ResourceChains.conf"),
  vehicles: join(CFG, "Vehicles", "LF_VehiclePrices.conf"),
  chop: join(CFG, "Crime", "LF_ChopShop.conf"),
  catalogue: join(CFG, "Catalogue", "LF_ItemCatalogue.conf"),
  layers: ["Street_Vehicles.layer", "Annex.layer"].map((f) => join(LAYERS, f)),
  owner: null,
};
// LF_ChopShop.PARTS_PREFAB: what the yard hands over beside the cash.
export const PARTS_GUID = "7A94C1E05D3B2F68";

// ---- Enfusion text parser ---------------------------------------------------------------------
// One construct per line: `Class "{ID}" {`, `key Class "{ID}" {`, `key {`, `key value`, `"string"`, `}`.
export function parseConf(text) {
  const holder = { props: {}, list: [] };
  const stack = [{ node: holder, kind: "object" }];
  const val = (s) => {
    s = s.trim();
    if (/^-?\d+(\.\d+)?$/.test(s)) return Number(s);
    const m = s.match(/^"(.*)"$/);
    return m ? m[1] : s;
  };
  const obj = (cls, id) => ({ cls, id: id ? id.replace(/"/g, "") : "", props: {}, list: [] });
  for (const raw of text.split("\n")) {
    const line = raw.trim();
    if (!line || line.startsWith("//")) continue;
    const cur = stack[stack.length - 1];
    if (line === "}") {
      if (stack.length > 1) stack.pop();
      continue;
    }
    const t = line.match(/"[^"]*"|\S+/g) || [];
    if (t[t.length - 1] === "{") {
      const push = (kind, node, key) => {
        if (key === undefined) cur.node.list.push(node);
        else cur.node.props[key] = node;
        stack.push({ node, kind });
      };
      if (cur.kind === "list") push("object", obj(t[0], t.length > 2 ? t[1] : ""));
      else if (stack.length === 1) push("object", obj(t[0], t.length > 2 ? t[1] : ""));
      else if (t.length === 2) push("list", obj("", ""), t[0]);
      else push("object", obj(t[1], t.length > 3 ? t[2] : ""), t[0]);
      continue;
    }
    if (cur.kind === "list") {
      cur.node.list.push(val(line));
      continue;
    }
    const sp = line.search(/\s/);
    if (sp > 0) cur.node.props[line.slice(0, sp)] = val(line.slice(sp));
  }
  return holder.list[0];
}

export const guidOf = (res) => {
  const m = String(res || "").match(/^\{([0-9A-Fa-f]+)\}/);
  return m ? m[1].toUpperCase() : "";
};
const pathOf = (res) => String(res || "").replace(/^\{[0-9A-Fa-f]+\}/, "");
const keyOf = (res) => guidOf(res) || pathOf(res).toUpperCase();
const nameOf = (res) => pathOf(res).replace(/^.*\//, "").replace(/\.et$/, "");
const num = (v, d = 0) => (typeof v === "number" ? v : d);
const items = (node, key) => node?.props[key]?.list || [];

// ---- Shop prices: mirrors LF_ShopPriceResolver + the owner `shops` section --------------------
export function loadShop(conf) {
  const rules = items(conf, "m_aPriceRules").map((r) => ({
    find: r.props.m_sFind || "", guid: guidOf(r.props.m_Prefab), prefab: r.props.m_Prefab || "",
    type: num(r.props.m_eItemType), typed: false,
    buy: num(r.props.m_iBuyPrice), sell: num(r.props.m_iSellPrice), hidden: !!r.props.m_bHidden,
  }));
  return {
    defBuy: num(conf.props.m_iDefaultBuyPrice, 50), defSell: num(conf.props.m_iDefaultSellPrice, 25),
    rules, extra: [...items(conf, "m_aExtraItemPrefabs")],
  };
}

/** Applies the owner's config.json `shops` section the way LF_ShopPricesConfig.ApplyOwner does. */
export function applyOwnerShops(shop, section) {
  if (!section || typeof section !== "object") return shop;
  if (Number.isInteger(section.defaultBuyPrice)) shop.defBuy = section.defaultBuyPrice;
  if (Number.isInteger(section.defaultSellPrice)) shop.defSell = section.defaultSellPrice;
  for (const [key, rec] of Object.entries(section.priceRules || {})) {
    if (!rec || typeof rec !== "object") continue;
    const typed = key.startsWith("type:");
    let rule = typed ? null : shop.rules.slice().reverse().find((r) => !r.guid && r.find === key);
    if (!rule) {
      rule = { find: typed ? "" : key, guid: "", prefab: "", type: 0, typed, buy: 0, sell: 0, hidden: false };
      shop.rules.push(rule);
    }
    if (Number.isInteger(rec.buyPrice)) rule.buy = rec.buyPrice;
    if (Number.isInteger(rec.sellPrice)) rule.sell = rec.sellPrice;
    if (typeof rec.hidden === "boolean") rule.hidden = rec.hidden;
  }
  for (const p of section.extraItemPrefabs || []) if (!shop.extra.includes(p)) shop.extra.push(p);
  return shop;
}

/** Walks the rules in order, later matches winning, a hidden match ending the walk. */
export function resolvePrice(shop, resource) {
  let buy = shop.defBuy, sell = shop.defSell;
  const g = guidOf(resource), p = pathOf(resource);
  for (const r of shop.rules) {
    let hit;
    if (r.guid) hit = r.guid === g;
    else if (r.find) hit = p.includes(r.find);
    else hit = !r.typed && r.type === 0; // arsenal type rules need the item's arsenal flags: not resolvable offline
    if (!hit) continue;
    if (r.hidden) return { hidden: true, buy: 0, sell: 0 };
    buy = r.buy;
    sell = r.sell;
  }
  return { hidden: false, buy, sell };
}

// ---- Loop discovery ---------------------------------------------------------------------------
const loop = (kind, id, path, cost, revenue) => ({ kind, id, path, cost, revenue, margin: revenue - cost });

function itemCatalog(shop) {
  const all = new Map();
  const add = (r) => { if (r && !all.has(keyOf(r))) all.set(keyOf(r), r); };
  shop.extra.forEach(add);
  shop.rules.forEach((r) => add(r.prefab));
  return all;
}

function ioOf(list) {
  return list.map((x) => ({ res: x.props.m_InputPrefab || x.props.m_OutputPrefab, n: num(x.props.m_iInputAmount ?? x.props.m_iOutputAmount, 1) }));
}

export function findShopLoops(shop) {
  const out = [];
  for (const res of itemCatalog(shop).values()) {
    const pr = resolvePrice(shop, res);
    if (pr.hidden) continue;
    if (pr.sell > pr.buy) out.push(loop("shop", `shop:${nameOf(res)}`, `buy ${nameOf(res)} ${pr.buy}, sell ${pr.sell}`, pr.buy, pr.sell));
  }
  return out;
}

/**
 * A bundle priced at the shop; null when an item is hidden or is not in the shop catalog (an
 * item the shop never lists, such as the contraband crate, has no price to loop on).
 */
function bundle(shop, io, side, catalog) {
  let total = 0;
  const parts = [];
  for (const { res, n } of io) {
    const pr = resolvePrice(shop, res);
    if (pr.hidden || !catalog.has(keyOf(res))) return null;
    const unit = side === "buy" ? pr.buy : pr.sell;
    total += unit * n;
    parts.push(`${n} ${nameOf(res)} @${unit}`);
  }
  return { total, text: parts.join(" + ") };
}

export function findRecipeLoops(shop, chains) {
  const out = [];
  const catalog = itemCatalog(shop);
  const test = (kind, id, ins, outs) => {
    if (ins.length === 0) return;
    const cost = bundle(shop, ins, "buy", catalog), rev = bundle(shop, outs, "sell", catalog);
    if (!cost || !rev) return;
    if (rev.total > cost.total) out.push(loop(kind, id, `buy [${cost.text}] = ${cost.total}, process, sell [${rev.text}] = ${rev.total}`, cost.total, rev.total));
  };
  for (const m of items(chains, "m_aMachines")) {
    test("recipe", `machine:${m.props.m_sMachineId}`, ioOf(items(m, "m_aInputs")), ioOf(items(m, "m_aOutputs")));
  }
  for (const c of items(chains, "m_aChains")) {
    const steps = items(c, "m_aSteps");
    const made = new Map(), resOf = new Map(), bought = new Map();
    for (const s of steps) {
      const ins = ioOf(items(s, "m_aInputs")), outs = ioOf(items(s, "m_aOutputs"));
      test("recipe", `chain:${c.props.m_sChainId}/${s.props.m_sStepId}`, ins, outs);
      for (const i of ins) {
        const k = keyOf(i.res);
        const use = Math.min(made.get(k) || 0, i.n);
        made.set(k, (made.get(k) || 0) - use);
        if (i.n > use) bought.set(k, { res: i.res, n: (bought.get(k)?.n || 0) + i.n - use });
      }
      for (const o of outs) {
        made.set(keyOf(o.res), (made.get(keyOf(o.res)) || 0) + o.n);
        resOf.set(keyOf(o.res), o.res);
      }
    }
    if (steps.length > 1) {
      const finals = [...made.entries()].filter(([, n]) => n > 0).map(([k, n]) => ({ res: resOf.get(k), n }));
      test("chain", `chain:${c.props.m_sChainId}`, [...bought.values()], finals);
    }
  }
  return out;
}

export function loadVehicles(vehConf, chopConf, layerTexts) {
  const v = vehConf?.props || {};
  const buyPct = num(v.m_iBuyPercent, 100), sellPct = num(v.m_iSellBackPercent, 50);
  const chopPct = num(v.m_iChopPercent, 30), chopCap = num(v.m_iChopValueCap, 3000);
  const price = (base, pct, ov, cap = 0) => {
    if (ov >= 0) return ov;
    let n = Math.max(base, 0);
    if (cap > 0) n = Math.min(n, cap);
    return Math.max(Math.floor((n * pct) / 100), 0);
  };
  const cars = new Map(); // spawn-prefab key -> car
  for (const e of items(vehConf, "m_aVehicles")) {
    const base = num(e.props.m_iBasePrice);
    const ov = (k) => (typeof e.props[k] === "number" ? e.props[k] : -1);
    const res = e.props.m_SpawnPrefab || e.props.m_Prefab;
    cars.set(keyOf(res), {
      name: nameOf(res), buy: price(base, buyPct, ov("m_iBuyOverride")), sellBack: price(base, sellPct, ov("m_iSellBackOverride")),
      chop: price(base, chopPct, ov("m_iChopOverride"), chopCap), lot: null, chopYard: null,
    });
  }
  const yardPct = num(chopConf?.props.m_iPayoutPercent, 30), yardCap = num(chopConf?.props.m_iValueCap, 3000);
  for (const e of items(chopConf, "m_aValues")) {
    const k = keyOf(e.props.m_Prefab);
    const car = cars.get(k) || { name: nameOf(e.props.m_Prefab), buy: null, sellBack: null, chop: null, lot: null, chopYard: null };
    const value = num(e.props.m_iValue);
    // LF_ChopShopRules.Payout: the value capped, times the percent, never more than the value.
    car.chopYard = value <= 0 || yardPct <= 0 ? 0 : Math.min(Math.floor((Math.min(yardCap > 0 ? yardCap : value, value) * yardPct) / 100), value);
    cars.set(k, car);
  }
  for (const text of layerTexts) {
    for (const m of text.matchAll(/m_sSpawnPrefab\s+"([^"]+)"\s*\n\s*m_iPrice\s+(\d+)/g)) {
      const car = cars.get(keyOf(m[1]));
      const p = Number(m[2]);
      if (car) car.lot = car.lot === null ? p : Math.min(car.lot, p);
    }
  }
  return { cars: [...cars.values()], parts: num(chopConf?.props.m_iPartsCount, 2) };
}

export function findVehicleLoops(shop, veh) {
  const out = [];
  const partPrice = resolvePrice(shop, `{${PARTS_GUID}}Prefabs/Items/Resources/LF_CarParts.et`);
  const partsCash = partPrice.hidden ? 0 : partPrice.sell * veh.parts;
  for (const c of veh.cars) {
    const buys = [["lot offer", c.lot], ["config buy price", c.buy]].filter(([, p]) => p !== null);
    if (buys.length === 0) continue;
    const [buyLabel, buyCost] = buys.reduce((a, b) => (b[1] < a[1] ? b : a));
    const routes = [
      ["sell-back", "sellback", c.sellBack, 0],
      ["chop (vehicle prices)", "chop", c.chop, partsCash],
      ["chop yard (LF_ChopShop.conf)", "chopyard", c.chopYard, partsCash],
    ];
    for (const [label, tag, pay, extra] of routes) {
      if (pay === null || pay + extra <= buyCost) continue;
      const parts = extra ? ` + ${veh.parts} car parts @${partPrice.sell}` : "";
      out.push(loop("vehicle", `vehicle:${c.name}:${tag}`, `buy ${c.name} ${buyCost} (${buyLabel}), ${label} ${pay}${parts} = ${pay + extra}`, buyCost, pay + extra));
    }
  }
  return out;
}

/** Every loop the sources hold, largest margin first. */
// ---- Item catalogue (#988) --------------------------------------------------------------------
/** The catalogue's live items with prefab, price and sell price; the owner's `catalogue.items` patched by id. */
export function loadCatalogue(conf, ownerSection) {
  const out = new Map();
  if (conf) {
    for (const it of items(conf, "m_aItems")) {
      const id = it.props.m_sId;
      if (!id) continue;
      out.set(id, { id, prefab: it.props.m_Prefab || "", price: num(it.props.m_iPrice, 100), sell: num(it.props.m_iSellPrice, 50), removed: !!num(it.props.m_bRemoved, 0) });
    }
  }
  for (const [id, rec] of Object.entries(ownerSection?.items || {})) {
    if (!rec || typeof rec !== "object") continue;
    const it = out.get(id) ?? (typeof rec.prefab === "string" ? { id, prefab: rec.prefab, price: 100, sell: 50, removed: false } : null);
    if (!it) continue;
    if (Number.isInteger(rec.price)) it.price = rec.price;
    if (Number.isInteger(rec.sellPrice)) it.sell = rec.sellPrice;
    if (typeof rec.removed === "boolean") it.removed = rec.removed;
    out.set(id, it);
  }
  return [...out.values()].filter((it) => !it.removed && it.prefab);
}

/** An outlet that pays more for an item than a price-rule shop asks, or asks less than a shop pays, is a loop. */
export function findCatalogueLoops(shop, catalogue) {
  const out = [];
  for (const it of catalogue) {
    const pr = resolvePrice(shop, it.prefab);
    if (pr.hidden) continue;
    const name = nameOf(it.prefab);
    if (it.sell > pr.buy && pr.buy > 0) out.push(loop("outlet", `outlet-sell:${it.id}`, `buy ${name} at a shop ${pr.buy}, sell to an outlet ${it.sell}`, pr.buy, it.sell));
    if (pr.sell > it.price && it.price > 0) out.push(loop("outlet", `outlet-buy:${it.id}`, `buy ${name} at an outlet ${it.price}, sell to a shop ${pr.sell}`, it.price, pr.sell));
  }
  return out;
}

export function checkProfitLoops(src = DEFAULT_SOURCES) {
  const read = (p) => readFileSync(p, "utf8");
  const maybe = (p) => (p && existsSync(p) ? parseConf(read(p)) : null);
  const shop = loadShop(parseConf(read(src.shop)));
  const owner = src.ownerData ?? (src.owner ? JSON.parse(read(src.owner)) : null);
  if (owner) applyOwnerShops(shop, owner.shops);
  const chains = parseConf(read(src.chains));
  const veh = loadVehicles(maybe(src.vehicles), maybe(src.chop), (src.layers || []).filter(existsSync).map(read));
  const catalogue = loadCatalogue(maybe(src.catalogue), owner?.catalogue);
  return [...findShopLoops(shop), ...findRecipeLoops(shop, chains), ...findVehicleLoops(shop, veh), ...findCatalogueLoops(shop, catalogue)]
    .sort((a, b) => b.margin - a.margin || a.id.localeCompare(b.id));
}

/** Judges loops against a baseline [{id, margin, reason}]: fresh ones, ones that grew, stale entries. */
export function judge(loops, baseline) {
  const known = new Map(baseline.map((b) => [b.id, b]));
  const present = new Set(loops.map((l) => l.id));
  return {
    fresh: loops.filter((l) => !known.has(l.id)),
    grown: loops.filter((l) => known.has(l.id) && l.margin > known.get(l.id).margin),
    stale: baseline.filter((b) => !present.has(b.id)),
  };
}
