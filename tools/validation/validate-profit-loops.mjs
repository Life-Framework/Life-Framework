// Fails when any price loop lets a player make money for nothing (#686): a shop item that
// buys for less than it sells, a machine or chain whose bought inputs cost less than its outputs
// sell for, a car bought then sold back or chopped for more. Logic: tools/economy/profit-loops.mjs.
//
// Prices are a design decision, so today's loops are listed in profit-loop-baseline.json with a
// reason. A loop not in the baseline fails; a baselined loop whose margin grows fails; a baseline
// entry whose loop is gone is reported as stale (delete it).
//   node tools/validation/validate-profit-loops.mjs                      check the shipped defaults
//   node tools/validation/validate-profit-loops.mjs --owner config.json  also apply an owner `shops` section
//   node tools/validation/validate-profit-loops.mjs --report             print every loop, baselined or not
//
// red-proof: profit-loop-selftest.mjs (a fixture with planted loops must fail).
import { readFileSync, existsSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { checkProfitLoops, judge, DEFAULT_SOURCES } from "../economy/profit-loops.mjs";

const here = dirname(fileURLToPath(import.meta.url));
export const baselinePath = join(here, "profit-loop-baseline.json");
const args = process.argv.slice(2);
const ownerIdx = args.indexOf("--owner");
const src = { ...DEFAULT_SOURCES, owner: ownerIdx >= 0 ? args[ownerIdx + 1] : null };
const loops = checkProfitLoops(src);
const baseline = existsSync(baselinePath) ? JSON.parse(readFileSync(baselinePath, "utf8")) : [];
const { fresh, grown, stale } = judge(loops, baseline);

if (args.includes("--report")) {
  for (const l of loops) console.log(`${l.kind.padEnd(8)} ${l.id}  margin +${l.margin}  ${l.path}`);
}
for (const l of fresh) console.log(`FAIL profit loop ${l.id}: ${l.path} (margin +${l.margin})`);
for (const l of grown) console.log(`FAIL baselined loop ${l.id} grew to +${l.margin}: ${l.path}`);
for (const b of stale) console.log(`WARN stale baseline entry ${b.id}: the loop is gone, delete it from profit-loop-baseline.json`);
console.log(`profit-loops: ${loops.length} loop(s), ${loops.length - fresh.length - grown.length} baselined, ${fresh.length} new, ${grown.length} grown`);
if (fresh.length + grown.length > 0) {
  console.log("profit-loops: fix the prices, or baseline the loop in tools/validation/profit-loop-baseline.json with a reason (a price change is a design decision).");
  process.exit(1);
}
