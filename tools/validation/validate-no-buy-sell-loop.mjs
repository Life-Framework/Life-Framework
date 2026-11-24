// Fails when a player can buy a good somewhere for less than some buyer pays for it (#4180): the cafe
// inputs: addons/**, tools/economy/**
// sold apple juice for $6 and the island trader paid $18, an endless loop with no work. Every way to buy
// (stock rows, price-table shops, catalogue outlets) is paired with every way to sell (trader rows in
// prefabs and layers, market ceilings, premium-buyer demand rows, catalogue outlets, price-table
// buy-back). No baseline: fix the price. Logic and the source list: tools/economy/buy-sell-loops.mjs.
//   node tools/validation/validate-no-buy-sell-loop.mjs            check the repo
//   node tools/validation/validate-no-buy-sell-loop.mjs --root D    check D/addons instead
//
// red-proof: no-buy-sell-loop-selftest.mjs (a fixture with the cafe juice loop must fail).
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { findBuySellLoops } from "../economy/buy-sell-loops.mjs";

const here = dirname(fileURLToPath(import.meta.url));
const args = process.argv.slice(2);
const at = args.indexOf("--root");
const root = at >= 0 ? args[at + 1] : join(here, "..", "..");
const loops = findBuySellLoops(join(root, "addons"));
for (const l of loops) console.log(`FAIL buy-sell loop ${l.id} (+${l.margin} a unit): ${l.path}`);
console.log(`no-buy-sell-loop: ${loops.length} loop(s)`);
if (loops.length > 0) {
  console.log("no-buy-sell-loop: price the shop at or above the best payout, or lower the payout (tools/economy/buy-sell-loops.mjs).");
  process.exit(1);
}
