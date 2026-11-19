// Market-floor loops (#2848, framework #762). A good on the market curve (Configs/Market/LF_Market.conf, plus
// any pack overlay under addons/<pack>/Configs/Market/) is sold at a price that climbs to its `m_iCeiling`.
// If a shop asks less than that ceiling, a player buys it there and sells it to the market: a loop.
// The rule: for every curved good, the cheapest shop buy price >= the highest ceiling any file gives it. A ceiling is
// a price per LOT (`m_iLotSize` units, 1 when unset, #3622), so the unit a player sells for is ceiling / lot.
// Called from checkProfitLoops in profit-loops.mjs (src.market = the files to read, core first).
import { readFileSync, readdirSync, existsSync } from "node:fs";
import { basename, join, relative } from "node:path";
import { parseConf, guidOf, resolvePrice, ROOT } from "./profit-loops.mjs";

const pathOf = (res) => String(res || "").replace(/^\{[0-9A-Fa-f]+\}/, "");
const keyOf = (res) => guidOf(res) || pathOf(res).toUpperCase();
const nameOf = (res) => pathOf(res).replace(/^.*\//, "").replace(/\.et$/, "");
const rel = (f) => {
  const r = relative(ROOT, f).replace(/\\/g, "/");
  return r.startsWith("..") ? basename(f) : r;
};

/** The core market table, then every pack's `Configs/Market/*.conf` overlay, in a stable order. */
export function marketFiles(root = ROOT) {
  const addons = join(root, "addons");
  const out = [join(addons, "LifeFramework", "Configs", "Market", "LF_Market.conf")];
  if (!existsSync(addons)) return out.filter(existsSync);
  for (const a of readdirSync(addons).sort()) {
    if (a === "LifeFramework") continue;
    const dir = join(addons, a, "Configs", "Market");
    if (existsSync(dir)) for (const f of readdirSync(dir).sort()) if (f.endsWith(".conf")) out.push(join(dir, f));
  }
  return out.filter(existsSync);
}

/** The shop's purchasable goods: price rules and extra items, keyed like the other readers. */
function shopGoods(shop) {
  const all = new Map();
  for (const r of [...shop.extra, ...shop.rules.map((x) => x.prefab)]) if (r && !all.has(keyOf(r))) all.set(keyOf(r), r);
  return all;
}

export function findMarketLoops(shop, src, catalogue = []) {
  const files = (src.market || []).filter(existsSync);
  if (files.length === 0) return [];
  const goods = new Map(); // key -> { res, ceiling (per unit), file }; the highest per-unit ceiling wins, the first file on a tie
  for (const file of files) {
    const conf = parseConf(readFileSync(file, "utf8"));
    for (const g of conf?.props.m_aGoods?.list || []) {
      const lot = typeof g.props.m_iLotSize === "number" && g.props.m_iLotSize > 1 ? g.props.m_iLotSize : 1;
      const ceiling = typeof g.props.m_iCeiling === "number" ? g.props.m_iCeiling / lot : null;
      if (ceiling === null) continue;
      const k = keyOf(g.props.m_sGood);
      if (!goods.has(k) || ceiling > goods.get(k).ceiling) goods.set(k, { res: g.props.m_sGood, ceiling, file });
    }
  }
  const catalog = shopGoods(shop);
  const out = [];
  for (const [k, g] of goods) {
    const offers = [];
    if (catalog.has(k)) {
      const pr = resolvePrice(shop, g.res);
      if (!pr.hidden) offers.push([`${rel(src.shop)} shop`, pr.buy]);
    }
    for (const it of catalogue) if (keyOf(it.prefab) === k && it.price > 0) offers.push([`catalogue outlet ${it.id}`, it.price]);
    if (offers.length === 0) continue;
    const [where, buy] = offers.reduce((a, b) => (b[1] < a[1] ? b : a));
    if (buy >= g.ceiling) continue;
    const name = nameOf(g.res);
    const cents = (n) => Math.round(n * 1000) / 1000;
    out.push({
      kind: "market", id: `market:${name}`, cost: buy, revenue: cents(g.ceiling), margin: cents(g.ceiling - buy),
      path: `buy ${name} ${buy} (${where}), sell to the market at its ceiling ${cents(g.ceiling)} a unit (${rel(g.file)})`,
    });
  }
  return out;
}
