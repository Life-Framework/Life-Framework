// The export-order line of `tools\cli economy` (#4826, WK-W36; the board is WK-W24, #4410 to #4412): what a hauler
// earns an hour filling the dock's export orders instead of selling to the town buyers.
//
// Read from config: the order book (Configs/Jobs/LF_ExportOrders_*.conf, the last overlay wins): each template's goods,
// count, wholesale and order bonus, and the book's premium percent. A template with no wholesale of its own (0 or unset,
// #4844) is priced from the market row of its goods: the row's base / lot size, the last market file that sets it wins
// (LF_ExportOrderConfig.WholesaleFor). An order pays count x wholesale x (100 + premium) / 100, rounded half up, plus the
// bonus (LF_ExportOrderConfig.PayForTemplate); the order's XP is printed beside it.
// Read from the built report: each goods' source loop (its units an hour and its reagent and fuel cost an hour). An hour
// on export orders is that loop's whole output sold at the order's unit pay, less the loop's cost.
// The line's headline is the best order kind a hauler can fill, as the plan's ladder reads it (a player takes the best
// order on the board); every kind is printed, and one outside the $650 to $800 band says so.
import { existsSync, readFileSync } from "node:fs";
import { join } from "node:path";
import { parseConf, guidOf } from "../economy/profit-loops.mjs";

export const EXPORT_BAND = { min: 650, max: 800 };

// Goods GUID -> the loop that makes it (the loop id in report.mining or report.lumber, and the lumber good name).
export const EXPORT_SOURCES = {
  BCD1E8CD7F975074: { section: "mining", loop: "iron-pickup" },
  C2AE9B7E62F0369F: { section: "mining", loop: "coal-feeder" },
  "45D1B36F99DD2FC3": { section: "lumber", loop: "hand-planks", good: "plank" },
};

/** What an order of `count` at `wholesale` pays (LF_ExportOrderConfig.PayFor). */
export function orderPay(count, wholesale, premiumPct) {
  return Math.floor((count * wholesale * (100 + premiumPct) + 50) / 100);
}

/** Per-unit market base (base / lot) of each good, keyed by GUID: the core LF_Market.conf, then the pack's market overlays. */
export function readMarketBase(addonsDir, pack, overlays = []) {
  const files = [join(addonsDir, "LifeFramework", "Configs", "Market", "LF_Market.conf")];
  for (const rel of overlays) if (rel.startsWith("Configs/Market/")) files.push(join(addonsDir, pack, rel));
  const base = new Map();
  for (const file of files) {
    if (!existsSync(file)) continue;
    const top = parseConf(readFileSync(file, "utf8"));
    if (top?.cls !== "LF_MarketConfig") continue;
    for (const g of top.props?.m_aGoods?.list ?? []) {
      const key = guidOf(g.props.m_sGood ?? "");
      if (!key) continue;
      if (g.props.m_sOp === "remove") { base.delete(key); continue; }
      const prev = base.get(key) ?? { base: 0, lot: 1 };
      const next = { base: typeof g.props.m_iBase === "number" ? g.props.m_iBase : prev.base, lot: typeof g.props.m_iLotSize === "number" ? Math.max(1, g.props.m_iLotSize) : prev.lot };
      base.set(key, next);
    }
  }
  const out = new Map();
  for (const [k, v] of base) out.set(k, Math.floor(v.base / v.lot));
  return out;
}

/** The order book from the overlay files (later files override earlier ones). Null when no overlay holds one. */
export function readOrderBook(addonsDir, pack, overlays = []) {
  let book = null;
  const market = readMarketBase(addonsDir, pack, overlays);
  for (const rel of overlays) {
    if (!/LF_ExportOrders?_/i.test(rel)) continue;
    const file = join(addonsDir, pack, rel);
    if (!existsSync(file)) continue;
    const top = parseConf(readFileSync(file, "utf8"));
    if (!top?.props) continue;
    const p = top.props;
    const templates = (p.m_aTemplates?.list ?? []).map((t) => {
      const goods = guidOf(t.props.m_Goods ?? "");
      const own = Number(t.props.m_iWholesale) || 0;
      return {
        id: t.props.m_sId,
        goods,
        name: t.props.m_sGoodsName ?? t.props.m_sId,
        count: Number(t.props.m_iCount) || 0,
        wholesale: own > 0 ? own : market.get(goods) ?? 0,
        wholesaleFrom: own > 0 ? "template" : "market",
        bonus: Number(t.props.m_iBonus) || 0,
      };
    });
    book = { file: rel, premiumPct: Number(p.m_iPremiumPct ?? 15), xp: Number(p.m_iXp ?? 0), minLevel: Number(p.m_iMinLevel ?? 0), templates };
  }
  return book;
}

/** Pure: each order kind's hour from the book and the report's loops. */
export function computeExport(book, report) {
  const orders = [], problems = [];
  for (const t of book?.templates ?? []) {
    const src = EXPORT_SOURCES[t.goods];
    if (!src) { problems.push(`${t.id}: no loop makes ${t.name}`); continue; }
    const loop = (report[src.section]?.loops ?? []).find((l) => l.id === src.loop);
    if (!loop) { problems.push(`${t.id}: loop ${src.loop} is not in the report`); continue; }
    const units = src.good ? (loop.detail ?? []).find((d) => d.good === src.good)?.unitsPerHour ?? 0 : loop.unitsPerHour ?? 0;
    if (!(t.wholesale > 0)) { problems.push(`${t.id}: no wholesale price for ${t.name} (template or market row)`); continue; }
    const pay = orderPay(t.count, t.wholesale, book.premiumPct) + t.bonus;
    const unitPay = t.count > 0 ? pay / t.count : 0;
    const steady = Math.round(units * unitPay - (loop.costPerHour ?? 0));
    const inBand = steady >= EXPORT_BAND.min && steady <= EXPORT_BAND.max;
    orders.push({ id: t.id, name: t.name, count: t.count, wholesale: t.wholesale, wholesaleFrom: t.wholesaleFrom, bonus: t.bonus, pay, source: loop.label, sourceSteady: loop.steady, unitsPerHour: units, ordersPerHour: t.count > 0 ? Math.round((units / t.count) * 100) / 100 : 0, steady, inBand });
  }
  const best = orders.reduce((a, o) => (!a || o.steady > a.steady ? o : a), null);
  // One player holds one order at a time and the board opens one new order per refill (LF_ExportOrderBook), so the
  // orders an hour printed here are the loop's pace, not a promise the board keeps up with it.
  const flags = orders.filter((o) => !o.inBand).map((o) => ({ kind: "export-band", id: `export:${o.id}`, text: `export order ${o.id} pays $${o.steady}/h steady, outside the $${EXPORT_BAND.min} to $${EXPORT_BAND.max} band` }));
  return { flags, file: book?.file ?? null, premiumPct: book?.premiumPct ?? 0, xp: book?.xp ?? 0, minLevel: book?.minLevel ?? 0, orders, best: best?.id ?? null, steady: best?.steady ?? 0, problems };
}

/** The export-order section of a built report: { file, orders, steady, ... } or null with no order book. */
export function buildExport(report, addonsDir) {
  const book = readOrderBook(addonsDir, report.pack, report.overlays ?? []);
  return book ? computeExport(book, report) : null;
}

/** The lines `tools\cli economy` prints for export orders. */
export function renderExport(x) {
  if (!x) return ["export orders: no order book in the mission"];
  const L = [`export orders (${x.file}: count x wholesale +${x.premiumPct}% plus the order bonus, ${x.xp} XP an order, Hauling ${x.minLevel}; the source loop's whole hour sold on orders, less its cost; band $${EXPORT_BAND.min} to $${EXPORT_BAND.max}):`];
  L.push(`  EXPORT orders: best ${x.best ?? "none"} steady $${x.steady}/h`);
  for (const o of x.orders) {
    const band = o.inBand ? "in the band" : o.steady > EXPORT_BAND.max ? `ABOVE the $${EXPORT_BAND.max} band top` : `under the $${EXPORT_BAND.min} band`;
    L.push(`  ORDER ${o.id}: ${o.count} ${o.name} at $${o.wholesale} (${o.wholesaleFrom}) + $${o.bonus} bonus = $${o.pay}, from ${o.source} (${o.unitsPerHour} an hour, ${o.ordersPerHour} orders/h, $${o.sourceSteady}/h at the town buyers) steady $${o.steady}/h, ${band}`);
  }
  for (const p of x.problems) L.push(`  problem: ${p}`);
  return L;
}
