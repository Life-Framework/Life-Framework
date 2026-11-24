// Buy-sell loops (#4180, island playtest round 7): a good a player can BUY somewhere for less than some
// buyer PAYS for it is free money with no work. The cafe sold apple juice for $6 while the island trader
// paid a flat $18 a bottle with no demand cap: buy, walk to the market, sell, repeat.
//
// profit-loops.mjs (validate-profit-loops) already compares the price table, the stock rows, the
// market ceilings and the catalogue outlets. It never read the two places this loop lived: the trader
// rows inside prefabs and layers (`LF_TraderItem.m_ValuePerItem`) and the premium-buyer demand rows
// (`LF_BuyerDemandRow.m_iPrice`). This check takes every way to buy and every way to sell and pairs them
// per good, with no baseline: a loop here is always a bug.
//
// Ways to buy (the cheapest wins):
//   stock      a `LF_ShopStock` row (core + every pack's Configs/Shop/*Stock*.conf, a later stock id replacing an earlier one)
//   table      a price-table shop: every `LF_ShopItem` prefab in a prefab or layer, and the table's extra items,
//              at LF_ShopPriceResolver's buy price (core ShopPrices.conf, then every pack LF_ShopPricesConfig overlay)
//   outlet     a catalogue outlet's `m_iPrice` (core + pack LF_ItemCatalogueConfig overlays, by item id)
// Ways to sell (the best wins):
//   trader     an `LF_TraderItem` row in any prefab or layer; a row with no prefab or value inherits them from the
//              row of the same id in its parent prefab. A good on the market curve pays the curve instead (below)
//   market     the per-unit ceiling (`m_iCeiling / m_iLotSize`) of the merged market files (add, patch, remove by good)
//   demand     a premium buyer's `LF_BuyerDemandRow.m_iPrice` (core + pack LF_BuyerDemandConfig files)
//   outlet     a catalogue outlet's `m_iSellPrice`
//   table      a price-table shop's buy-back (`m_iSellPrice`) for a good that shop sells
// Out of scope: a stocked shop's buyback (a fraction of street value, below what that stock charges by
// LF_ShopStockRules), owner config.json overrides (validate-profit-loops --owner covers the table), and
// recipes and chains (validate-profit-loops). Labour is free here as there: a loop needs no gathering at all.
import { readFileSync, readdirSync, existsSync } from "node:fs";
import { join, relative } from "node:path";
import { parseConf, guidOf, loadShop, resolvePrice } from "./profit-loops.mjs";
import { loadStocks } from "./profit-stock.mjs";

const pathOf = (res) => String(res || "").replace(/^\{[0-9A-Fa-f]+\}/, "");
const keyOf = (res) => guidOf(res) || pathOf(res).toUpperCase();
const nameOf = (res) => pathOf(res).replace(/^.*\//, "").replace(/\.et$/, "");
const num = (v) => (typeof v === "number" ? v : null);

/** Every file under `dir` whose name passes `keep`, in a stable order. */
function walk(dir, keep, out = []) {
  if (!existsSync(dir)) return out;
  for (const e of readdirSync(dir, { withFileTypes: true }).sort((a, b) => a.name.localeCompare(b.name))) {
    const p = join(dir, e.name);
    if (e.isDirectory()) walk(p, keep, out);
    else if (keep(e.name)) out.push(p);
  }
  return out;
}

/** The addon folders, core first, then the packs by name. */
function addonDirs(addonsDir) {
  if (!existsSync(addonsDir)) return [];
  const packs = readdirSync(addonsDir, { withFileTypes: true }).filter((e) => e.isDirectory() && e.name !== "LifeFramework").map((e) => e.name).sort();
  return ["LifeFramework", ...packs].map((a) => join(addonsDir, a)).filter(existsSync);
}

/** Every Configs/**.conf whose root class is `cls`, core first; Configs/Tests holds test overlays, not shipped prices. */
function confsOf(addonsDir, cls) {
  const out = [];
  for (const a of addonDirs(addonsDir)) {
    for (const f of walk(join(a, "Configs"), (n) => n.endsWith(".conf")).filter((p) => !/[\\/]Configs[\\/]Tests[\\/]/.test(p))) {
      const text = readFileSync(f, "utf8");
      if (new RegExp(`^\\s*${cls}\\b`).test(text)) out.push({ file: f, root: parseConf(text) });
    }
  }
  return out;
}

/** Every prefab and layer text in the addons (the places trader rows and shop items live). */
function entityTexts(addonsDir) {
  const out = [];
  for (const a of addonDirs(addonsDir)) {
    for (const f of walk(a, (n) => n.endsWith(".et") || n.endsWith(".layer"))) out.push({ file: f, text: readFileSync(f, "utf8") });
  }
  return out;
}

/**
 * The flat blocks `<Class> "{ID}" {` ... `}` of one class in a text: [{ id, fields }], one construct per line.
 * Trader rows and shop items hold no nesting; a value such as `"{GUID}path"` holds braces, so the block ends
 * at the first line that is only `}`, never at a brace inside a value.
 */
export function blocksOf(text, cls) {
  const out = [];
  const head = new RegExp(`^\\s*${cls}\\s+"\\{([0-9A-Fa-f]+)\\}"\\s*\\{\\s*$`);
  let cur = null;
  for (const line of text.split("\n")) {
    if (!cur) {
      const m = line.match(head);
      if (m) cur = { id: m[1].toUpperCase(), fields: {} };
      continue;
    }
    const t = line.trim();
    if (t === "}") {
      out.push(cur);
      cur = null;
      continue;
    }
    const kv = t.match(/^(\w+)\s+(.+)$/);
    if (!kv || kv[2].endsWith("{")) continue;
    const v = kv[2].trim();
    cur.fields[kv[1]] = /^-?\d+(\.\d+)?$/.test(v) ? Number(v) : v.replace(/^"(.*)"$/, "$1");
  }
  return out;
}

/** The merged price table: the core file, then each pack overlay's rules and extra items after it (later rules win). */
export function loadPriceTable(addonsDir) {
  const tables = confsOf(addonsDir, "LF_ShopPricesConfig");
  if (tables.length === 0) return null;
  const shop = loadShop(tables[0].root);
  for (const t of tables.slice(1)) {
    const extra = loadShop(t.root);
    for (const r of extra.rules) {
      const at = shop.rules.findIndex((x) => (r.guid ? x.guid === r.guid : !x.guid && x.find === r.find));
      if (at >= 0) shop.rules[at] = r; // a pack row with the same key replaces the core row in place
      else shop.rules.push(r);
    }
    for (const p of extra.extra) if (!shop.extra.includes(p)) shop.extra.push(p);
    if (typeof t.root.props.m_iDefaultBuyPrice === "number") shop.defBuy = t.root.props.m_iDefaultBuyPrice;
    if (typeof t.root.props.m_iDefaultSellPrice === "number") shop.defSell = t.root.props.m_iDefaultSellPrice;
  }
  return shop;
}

/**
 * Catalogue items by id across core and pack files (a later file's item replaces the earlier one), kept only
 * when some outlet offers them: an outlet sells and buys back only its offers (LF_ItemCatalogue.OfferFor).
 */
function loadCatalogueItems(addonsDir) {
  const items = new Map();
  const offered = new Set();
  for (const c of confsOf(addonsDir, "LF_ItemCatalogueConfig")) {
    for (const o of c.root.props.m_aOffers?.list || []) {
      const op = String(o.props.m_sOp || "");
      if (o.props.m_sItem && op !== "remove" && op !== "delete") offered.add(o.props.m_sItem);
    }
    for (const it of c.root.props.m_aItems?.list || []) {
      const id = it.props.m_sId;
      if (!id) continue;
      const prev = items.get(id) || { id, prefab: "", price: 100, sell: 50, removed: false, file: c.file };
      items.set(id, {
        id, file: c.file,
        prefab: it.props.m_Prefab || prev.prefab,
        price: num(it.props.m_iPrice) ?? prev.price,
        sell: num(it.props.m_iSellPrice) ?? prev.sell,
        removed: it.props.m_bRemoved === undefined ? prev.removed : !!it.props.m_bRemoved,
      });
    }
  }
  return [...items.values()].filter((it) => !it.removed && it.prefab && offered.has(it.id));
}

/**
 * The good-word perk (LF_PerkPrice, #4409): a trader pays a perk holder GOOD_WORD_PERCENT more for the goods
 * of a work trade, by prefab folder. Read from the script so this check moves with it; 0 when it is gone.
 */
export function perkFor(addonsDir) {
  const f = walk(join(addonsDir, "LifeFramework", "Scripts"), (n) => n === "LF_PerkPrice.c")[0];
  const text = f ? readFileSync(f, "utf8") : "";
  const pct = Number(text.match(/GOOD_WORD_PERCENT\s*=\s*(\d+)/)?.[1] ?? 0);
  const folders = [...text.matchAll(/path\.Contains\("([^"]+)"\)/g)].map((m) => m[1]);
  return { pct, folders };
}

/** Every way to buy and every way to sell, per good key: { buys: Map<key, offer[]>, sells: Map<key, offer[]> }. */
export function collectOffers(addonsDir) {
  const rel = (f) => relative(join(addonsDir, ".."), f).replace(/\\/g, "/");
  const buys = new Map(), sells = new Map(), names = new Map();
  // A sale at a trader (its own row, the curve, a premium buyer) pays the good-word share on top.
  const perk = perkFor(addonsDir);
  const atTrader = (res, price, where) => {
    if (!(perk.pct > 0) || !(price > 0) || !perk.folders.some((f) => pathOf(res).includes(f))) return add(sells, res, price, where);
    const paid = Math.round(price * (100 + perk.pct)) / 100;
    add(sells, res, paid, `${where}, ${price} plus the ${perk.pct}% good-word perk`);
  };
  const add = (into, res, price, where) => {
    if (!(price > 0) || !res) return;
    const k = keyOf(res);
    if (!names.has(k)) names.set(k, nameOf(res));
    if (!into.has(k)) into.set(k, []);
    into.get(k).push({ price, where });
  };

  // ---- Market curve: a curved good pays its ceiling at a trader, whatever the trader row says. The files merge
  // per good in load order, core first: "remove" drops the good, "patch" (and "add" over an existing good) sets the
  // fields it names, so a pack that removes the core gold-bar curve or lowers a ceiling is read as the game reads it.
  const goods = new Map(); // key -> { res, fields, file }
  for (const m of confsOf(addonsDir, "LF_MarketConfig")) {
    for (const g of m.root.props.m_aGoods?.list || []) {
      if (!g.props.m_sGood) continue;
      const k = keyOf(g.props.m_sGood);
      const op = String(g.props.m_sOp || "");
      if (op === "remove" || op === "delete") {
        goods.delete(k);
        continue;
      }
      const prev = goods.get(k);
      if (op === "patch" && !prev) continue; // a patch with nothing to patch changes nothing
      goods.set(k, { res: g.props.m_sGood, fields: { ...(prev?.fields || {}), ...g.props }, file: m.file });
    }
  }
  const curve = new Map();
  for (const [k, g] of goods) {
    const ceiling = num(g.fields.m_iCeiling);
    if (ceiling === null) continue;
    const lot = num(g.fields.m_iLotSize) > 1 ? g.fields.m_iLotSize : 1;
    const unit = Math.round((ceiling / lot) * 1000) / 1000;
    curve.set(k, { unit, file: g.file });
    atTrader(g.res, unit, `the market at its ceiling (${rel(g.file)})`);
  }

  // ---- Stock rows.
  const stockFiles = [];
  for (const a of addonDirs(addonsDir)) stockFiles.push(...walk(join(a, "Configs", "Shop"), (n) => /Stock.*\.conf$/.test(n)));
  for (const [id, stock] of loadStocks(stockFiles)) {
    for (const row of stock.items) add(buys, row.res, row.price, `stock "${id}" (${rel(stock.file)})`);
  }

  // ---- Prefabs and layers: trader rows (with their parent's row of the same id) and price-table shop items.
  const texts = entityTexts(addonsDir);
  const rowBase = new Map(); // row id -> { prefab, value } from the first file that sets them
  const traderRows = [];
  const tableItems = new Map(); // key -> { res, file }
  for (const { file, text } of texts) {
    for (const b of blocksOf(text, "LF_TraderItem")) {
      traderRows.push({ file, ...b });
      const base = rowBase.get(b.id) || {};
      if (!base.prefab && b.fields.m_ItemPrefab) base.prefab = b.fields.m_ItemPrefab;
      if (base.value === undefined && typeof b.fields.m_ValuePerItem === "number") base.value = b.fields.m_ValuePerItem;
      rowBase.set(b.id, base);
    }
    for (const b of blocksOf(text, "LF_ShopItem")) {
      const res = b.fields.m_ItemPrefab;
      if (res && !tableItems.has(keyOf(res))) tableItems.set(keyOf(res), { res, file });
    }
  }
  for (const r of traderRows) {
    const res = r.fields.m_ItemPrefab || rowBase.get(r.id)?.prefab;
    const value = typeof r.fields.m_ValuePerItem === "number" ? r.fields.m_ValuePerItem : rowBase.get(r.id)?.value;
    if (!res || !(value > 0)) continue;
    if (curve.has(keyOf(res))) continue; // the curve pays instead; its ceiling is already a sell offer
    atTrader(res, value, `trader row ${r.id} (${rel(r.file)})`);
  }

  // ---- Premium buyers.
  for (const d of confsOf(addonsDir, "LF_BuyerDemandConfig")) {
    for (const row of d.root.props.m_aRows?.list || []) {
      const op = String(row.props.m_sOp || "");
      if (op === "remove" || op === "delete") continue;
      atTrader(row.props.m_sGood, num(row.props.m_iPrice), `buyer "${row.props.m_sBuyer}" demand row (${rel(d.file)})`);
    }
  }

  // ---- Catalogue outlets.
  for (const it of loadCatalogueItems(addonsDir)) {
    add(buys, it.prefab, it.price, `catalogue outlet item ${it.id} (${rel(it.file)})`);
    add(sells, it.prefab, it.sell, `catalogue outlet item ${it.id} (${rel(it.file)})`);
  }

  // ---- The price table, for every good a price-table shop lists. Only a curated `m_aShopItems` row is sold
  // (LF_ShopComponent.IsItemSoldHere, #1762); the table's extra items are names and prices, not stock.
  const shop = loadPriceTable(addonsDir);
  if (shop) {
    for (const { res, file } of tableItems.values()) {
      const pr = resolvePrice(shop, res);
      if (pr.hidden) continue;
      const where = `the price table (shop item in ${rel(file)})`;
      add(buys, res, pr.buy, where);
      add(sells, res, pr.sell, `${where} buy-back`);
    }
  }
  return { buys, sells, names };
}

/** Every good bought for less than some buyer pays: [{ id, good, cost, revenue, margin, path }], largest margin first. */
export function findBuySellLoops(addonsDir) {
  const { buys, sells, names } = collectOffers(addonsDir);
  const out = [];
  for (const [k, offers] of buys) {
    const pays = sells.get(k);
    if (!pays) continue;
    const buy = offers.reduce((a, b) => (b.price < a.price ? b : a));
    const sell = pays.reduce((a, b) => (b.price > a.price ? b : a));
    if (sell.price <= buy.price) continue;
    const margin = Math.round((sell.price - buy.price) * 1000) / 1000;
    out.push({
      id: `buy-sell:${names.get(k)}`, good: k, cost: buy.price, revenue: sell.price, margin,
      path: `buy ${names.get(k)} ${buy.price} at ${buy.where}, sell ${sell.price} to ${sell.where}`,
    });
  }
  return out.sort((a, b) => b.margin - a.margin || a.id.localeCompare(b.id));
}
