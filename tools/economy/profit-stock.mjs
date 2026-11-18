// Stock-row loops. A stocked shop (a `LF_ShopStock` row in Configs/Shop/LF_ShopStock*.conf, core or pack
// overlay: the cafe menu, the general store, the supermarket) charges the row's own `m_iPrice`, not the
// shop price table's buy price. If that price is below what any outlet pays for the same good, a player
// buys at the row and sells at the outlet: the apple sold at $2 at the cafe while the market paid up to $5.
// The rule: every stock row price >= the best payout for its good, taken from
//   market     the highest `m_iCeiling` any market file gives the good (the trader / stall curve)
//   outlet     the catalogue outlets' `m_iSellPrice`
// The price table is not read: a stocked shop never buys back through it (LF_ShopComponent.GetSellPrice), and an owner
// preset that raises a table sell price (tools/test/config-fixtures/presets/shop-prices.json) is not a stock loop;
// LF_Test_EveronVillageStock holds the buy-back rule in game.
// The same good at the same price on several shops is one loop per stock id, so a fix lands per shop.
// A stock overlay replaces a core stock of the same id, as LF_ShopStockConfig.Merge does.
// Called from checkProfitLoops in profit-loops.mjs (src.stocks = the files to read, core first).
import { readFileSync, readdirSync, existsSync } from "node:fs";
import { basename, join, relative } from "node:path";
import { parseConf, guidOf, ROOT } from "./profit-loops.mjs";

const pathOf = (res) => String(res || "").replace(/^\{[0-9A-Fa-f]+\}/, "");
const keyOf = (res) => guidOf(res) || pathOf(res).toUpperCase();
const nameOf = (res) => pathOf(res).replace(/^.*\//, "").replace(/\.et$/, "");
const rel = (f) => {
  const r = relative(ROOT, f).replace(/\\/g, "/");
  return r.startsWith("..") ? basename(f) : r;
};

/** The core stock table, then every pack's `Configs/Shop/*Stock*.conf` overlay, in a stable order. */
export function stockFiles(root = ROOT) {
  const addons = join(root, "addons");
  const out = [join(addons, "LifeFramework", "Configs", "Shop", "LF_ShopStock.conf")];
  if (!existsSync(addons)) return out.filter(existsSync);
  for (const a of readdirSync(addons).sort()) {
    if (a === "LifeFramework") continue;
    const dir = join(addons, a, "Configs", "Shop");
    if (existsSync(dir)) for (const f of readdirSync(dir).sort()) if (/Stock.*\.conf$/.test(f)) out.push(join(dir, f));
  }
  return out.filter(existsSync);
}

/** Merged stocks: id -> { file, items: [{ res, price }] }; a later file's stock replaces an earlier one. */
export function loadStocks(files) {
  const stocks = new Map();
  for (const file of files.filter(existsSync)) {
    const conf = parseConf(readFileSync(file, "utf8"));
    for (const s of conf?.props.m_aStocks?.list || []) {
      const id = s.props.m_sId;
      if (!id) continue;
      const items = (s.props.m_aItems?.list || []).map((i) => ({ res: i.props.m_Prefab, price: typeof i.props.m_iPrice === "number" ? i.props.m_iPrice : 0 }))
        .filter((i) => i.res);
      stocks.set(id, { file, items });
    }
  }
  return stocks;
}

/** The best payout per good across the market files and the catalogue outlets. */
function payouts(src, catalogue) {
  const best = new Map(); // key -> { pay, label }
  const offer = (res, pay, label) => {
    if (!(pay > 0)) return;
    const k = keyOf(res);
    if (!best.has(k) || pay > best.get(k).pay) best.set(k, { pay, label });
  };
  for (const file of (src.market || []).filter(existsSync)) {
    const conf = parseConf(readFileSync(file, "utf8"));
    for (const g of conf?.props.m_aGoods?.list || []) {
      if (typeof g.props.m_iCeiling === "number") offer(g.props.m_sGood, g.props.m_iCeiling, `the market at its ceiling (${rel(file)})`);
    }
  }
  for (const it of catalogue) offer(it.prefab, it.sell, `catalogue outlet ${it.id}`);
  return best;
}

export function findStockLoops(shop, src, catalogue = []) {
  const files = (src.stocks || []).filter(existsSync);
  if (files.length === 0) return [];
  const best = payouts(src, catalogue);
  const out = [];
  for (const [id, stock] of loadStocks(files)) {
    for (const row of stock.items) {
      const pay = best.get(keyOf(row.res));
      if (!pay || row.price <= 0 || row.price >= pay.pay) continue;
      const name = nameOf(row.res);
      out.push({
        kind: "stock", id: `stock:${id}:${name}`, cost: row.price, revenue: pay.pay, margin: pay.pay - row.price,
        path: `buy ${name} ${row.price} (stock "${id}", ${rel(stock.file)}), sell to ${pay.label} ${pay.pay}`,
      });
    }
  }
  return out;
}
