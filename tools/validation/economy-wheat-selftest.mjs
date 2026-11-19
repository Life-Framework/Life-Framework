// Proves the wheat loops of `tools\cli economy` and the per-lot market loop check (#3622): a curve row and a premium
// row price a LOT of kg (grain $9 a 200 kg lot = $0.045 a kg), the curve falls per lot sold, a stack below one lot
// sells nothing, the loops read the lot size from planted configs, and the market-floor loop check compares the
// per-unit ceiling (ceiling / lot) to a shop price.
// inputs: tools/lib/economy-wheat.mjs, tools/economy/profit-market.mjs, tools/validation/economy-wheat-selftest.mjs
// red-proof: with the lot ignored in sellLots (lots = kg) the "13 lots" check printed FAIL and the granary loop paid
// $4 a kg; with the lot ignored in profit-market.mjs the "$0.045 a kg is under the $1 shop price" check printed FAIL
// because a $9 ceiling hid the loop; restored, it prints "economy-wheat-selftest: OK".
import { mkdirSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { WHEAT_ASSUMPTIONS, computeWheatLoops, sellLots, simulateWheat, wheatFlags } from "../lib/economy-wheat.mjs";
import { findMarketLoops } from "../economy/profit-market.mjs";
import { scratchDir } from "../lib/scratch.mjs";

const fails = [];
const expect = (ok, msg) => { if (!ok) fails.push(`FAIL ${msg}`); };
const near = (a, b, tol = 0.01) => Math.abs(a - b) <= tol;

// 1. The curve falls per lot: 2600 kg is 13 lots of 200, a part lot sells nothing.
const flat = { base: 9, floor: 9, dropPerUnitPct: 0, recoverPctPerHour: 0, lotSize: 200 };
expect(sellLots(flat, 9, 2600).lots === 13 && near(sellLots(flat, 9, 2600).cash, 117), `13 lots at 9 pay 117 (got ${JSON.stringify(sellLots(flat, 9, 2600))})`);
expect(sellLots(flat, 9, 199).lots === 0 && sellLots(flat, 9, 199).cash === 0, "199 kg fills no lot");
expect(sellLots(flat, 9, 450).lots === 2, "450 kg is two lots");
const falling = { base: 10, floor: 8, dropPerUnitPct: 10, recoverPctPerHour: 0, lotSize: 5 };
const f = sellLots(falling, 10, 20);
expect(near(f.cash, 10 + 9 + 8 + 8) && f.price === 8, `four 5 kg lots pay 10, 9, 8, 8 (got ${f.cash}, ${f.price})`);
expect(sellLots({ ...falling, lotSize: undefined }, 10, 3).lots === 3, "a curve with no lot prices every unit");

// 2. The loops: the bakery takes flour in lots up to its purse, the rest of the grain goes wholesale.
const a = { ...WHEAT_ASSUMPTIONS };
const curve = { base: 9, floor: 7, dropPerUnitPct: 1, recoverPctPerHour: 26, lotSize: 200 };
const flour = { price: 1, cap: 1000, refillPerHour: 500, lotSize: 5 };
const milled = simulateWheat(10400, curve, flour, a);
expect(milled.flourFirst === 750 && milled.flourSteady === 500, `flour first hour 750 kg (the mill), steady 500 kg (the refill) (got ${milled.flourFirst}, ${milled.flourSteady})`);
expect(milled.grainSteady === 9400, `9,400 kg grain steady (got ${milled.grainSteady})`);
const grain = simulateWheat(10400, curve, null, a);
expect(grain.flourSteady === 0 && grain.steady > 0, "the grain-only loop sells no flour");
expect(milled.steady > grain.steady, "milling pays more than raw grain");
const duty = { police: 900, medic: 792 };
const loops = computeWheatLoops({ assumptions: a, curve, flour, duty });
expect(loops.length === 2 && loops.every((l) => !l.flagged), `both loops are inside the band (got ${loops.map((l) => `${l.id} $${l.steady}`).join(", ")})`);
const cheap = computeWheatLoops({ assumptions: a, curve: { ...curve, base: 3, floor: 3, ceiling: 3 }, flour: null, duty });
expect(cheap[0].flagged && wheatFlags({ loops: cheap }).length === 1, "a curve at $3 a lot is out of the band and flagged");

// 3. The market-floor loop check compares the per-unit ceiling (ceiling / lot) to the shop price.
const root = scratchDir("lf-wheat-");
mkdirSync(root, { recursive: true });
const confFile = join(root, "market.conf");
const confText = `LF_MarketConfig {
 m_aGoods {
  LF_MarketGoodDef "{AAAA000000000001}" {
   m_sGood "{7B2E5A1C93D40F11}Prefabs/Items/Harvest/LF_GrainSack.et"
   m_iBase 9
   m_iFloor 7
   m_iCeiling 9
   m_iLotSize 200
  }
 }
}
`;
writeFileSync(confFile, confText);
const shop = { defBuy: 50, defSell: 25, rules: [{ find: "", guid: "7B2E5A1C93D40F11", prefab: "{7B2E5A1C93D40F11}Prefabs/Items/Harvest/LF_GrainSack.et", type: 0, typed: false, buy: 1, sell: 0, hidden: false }], extra: [] };
expect(findMarketLoops(shop, { market: [confFile], shop: join(root, "shop.conf") }).length === 0, "$0.045 a kg is under the $1 shop price: no loop");
const wholeFile = join(root, "whole.conf");
writeFileSync(wholeFile, confText.replace("m_iLotSize 200", "m_iLotSize 1"));
const whole = findMarketLoops(shop, { market: [wholeFile], shop: join(root, "shop.conf") });
expect(whole.length === 1 && whole[0].revenue === 9 && whole[0].margin === 8, `the same row at lot 1 pays 9 a unit, so a $1 shop price is a loop (got ${JSON.stringify(whole)})`);
const lotLoop = findMarketLoops({ ...shop, rules: [{ ...shop.rules[0], buy: 0 }] }, { market: [confFile], shop: join(root, "shop.conf") });
expect(lotLoop.length === 1 && near(lotLoop[0].revenue, 0.045, 0.0005), `a free grain sack still sells at 0.045 a kg (got ${JSON.stringify(lotLoop)})`);

if (fails.length) { console.error(fails.join("\n")); process.exit(1); }
console.log("economy-wheat-selftest: OK");
