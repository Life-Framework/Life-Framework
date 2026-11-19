// The three mining recipes in the no-profit-loop check (#3240, E1 of #3206; `tools/economy/profit-loops.mjs` calls it).
// Design rule (mining-chains.md): "No shop-to-trader loops. Acid is sold only by the hardware store and never bought
// by a trader. Coal, limestone, iron ore and copper ore have no shop offer."
//
// The generic recipe test (`findRecipeLoops`) prices outputs at the SHOP's sell price and skips any recipe with an input
// the shop does not list, so a mining recipe (ore is never shop-priced) was never judged. Here the outputs are priced
// where a miner really sells them, at the market curve's ceiling (a price per LOT of m_iLotSize units, so per unit =
// ceiling / lot) and the premium rows' price, whichever is higher. Three things fail:
//   * a recipe missing from ResourceChains.conf (the chain is gone, the loop list lost a recipe);
//   * every input of a recipe is sold by a shop, so a player buys all of them, processes, and sells at the market;
//   * an input is sold by a shop for less than the market's ceiling pays for the same good (a shop-to-trader loop on
//     a reagent such as acid, coal or sand), reported per reagent with the market file named.
import { existsSync, readFileSync } from "node:fs";
import { guidOf, parseConf, resolvePrice } from "../economy/profit-loops.mjs";

/** The plant recipes the chain design adds; each must stay in ResourceChains.conf. */
export const MINING_RECIPES = ["smelter-iron", "refinery-copper", "cement-works"];

const num = (v, d = 0) => (typeof v === "number" ? v : d);
const pathOf = (res) => String(res || "").replace(/^\{[0-9A-Fa-f]+\}/, "");
const nameOf = (res) => pathOf(res).replace(/^.*\//, "").replace(/\.et$/, "");
const keyOf = (res) => guidOf(res) || pathOf(res).toUpperCase();

/** What a miner is paid for one unit of each curved good: market ceiling per lot unit, the premium rows' price when higher. */
export function sellPrices(marketFiles = [], demandFiles = []) {
  const out = new Map();
  const put = (k, v) => { if (v > (out.get(k) ?? 0)) out.set(k, v); };
  for (const f of marketFiles.filter((x) => x && existsSync(x))) {
    for (const g of parseConf(readFileSync(f, "utf8"))?.props.m_aGoods?.list ?? []) {
      if (typeof g.props.m_iCeiling !== "number") continue;
      const lot = Math.max(1, num(g.props.m_iLotSize, 1));
      put(keyOf(g.props.m_sGood), g.props.m_iCeiling / lot);
    }
  }
  for (const f of demandFiles.filter((x) => x && existsSync(x))) {
    for (const r of parseConf(readFileSync(f, "utf8"))?.props.m_aRows?.list ?? []) {
      if (r.props.m_sOp === "remove") continue;
      put(keyOf(r.props.m_sGood), num(r.props.m_iPrice) / Math.max(1, num(r.props.m_iLotSize, 1)));
    }
  }
  return out;
}

const loop = (id, path, cost, revenue) => ({ kind: "mining", id, path, cost, revenue, margin: revenue - cost });

/** Loops (and a missing-recipe failure) for the mining recipes: `shop` from loadShop, `chains` the parsed ResourceChains.conf. */
export function findMiningRecipeLoops(shop, chains, prices, recipeIds = MINING_RECIPES) {
  const out = [];
  const machines = chains?.props.m_aMachines?.list ?? [];
  for (const id of recipeIds) {
    const m = machines.find((x) => x.props.m_sMachineId === id);
    if (!m) {
      out.push(loop(`mining:${id}`, `recipe '${id}' is missing from ResourceChains.conf: the mining chain lost a recipe (#3240)`, 0, 0));
      continue;
    }
    const ins = (m.props.m_aInputs?.list ?? []).map((i) => ({ res: i.props.m_InputPrefab, n: num(i.props.m_iInputAmount, 1) }));
    const outs = (m.props.m_aOutputs?.list ?? []).map((o) => ({ res: o.props.m_OutputPrefab, n: num(o.props.m_iOutputAmount, 1) }));
    const buyOf = (res) => { const p = resolvePrice(shop, res); return p.hidden ? null : p.buy; };
    const shopSold = (res) => shop.rules.some((r) => (r.guid ? r.guid === guidOf(res) : false) && !r.hidden) || shop.extra.some((e) => keyOf(e) === keyOf(res));
    const revenue = outs.reduce((s, o) => s + (prices.get(keyOf(o.res)) ?? 0) * o.n, 0);
    if (ins.length > 0 && ins.every((i) => shopSold(i.res) && buyOf(i.res) !== null)) {
      const cost = ins.reduce((s, i) => s + buyOf(i.res) * i.n, 0);
      if (revenue > cost) out.push(loop(`mining:${id}`, `buy [${ins.map((i) => `${i.n} ${nameOf(i.res)} @${buyOf(i.res)}`).join(" + ")}] = ${cost} at a shop, process, sell [${outs.map((o) => `${o.n} ${nameOf(o.res)}`).join(" + ")}] to the market = ${Math.round(revenue * 1000) / 1000}`, cost, revenue));
    }
    for (const i of ins) {
      const sells = prices.get(keyOf(i.res));
      if (!shopSold(i.res) || buyOf(i.res) === null || sells === undefined || sells <= buyOf(i.res)) continue;
      out.push(loop(`mining:${id}:${nameOf(i.res)}`, `a shop sells ${nameOf(i.res)} for ${buyOf(i.res)} and the market pays up to ${Math.round(sells * 1000) / 1000} a unit`, buyOf(i.res), sells));
    }
  }
  return out;
}
