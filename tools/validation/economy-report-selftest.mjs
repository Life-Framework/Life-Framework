// Proves `tools\cli economy` (#3122) on a small planted fixture tree: the exact ladder lines, the flags it must
// raise (a cement buy-low sell-high loop that only appears once the pack overlay raises the sell price, a weighted
// fish with no prefab, a zero-pay taxi run, a plot out of reach) and the flags it must not (an eel with weight 0).
// inputs: tools/commands/economy.mjs, tools/lib/economy.mjs, tools/test/economy-fixtures/**, tools/pack/layers.mjs, tools/economy/profit-loops.mjs, tools/validation/economy-report-selftest.mjs
// red-proof: before tools/lib/economy.mjs existed this selftest died on the missing import and exited 1; with the
// pack overlay left out of the merge (layers.mjs sectionLayers skipping it) the cement loop flag went missing and
// "FAIL loop flag" printed; restored, it prints "economy-report-selftest: OK".
import { cpSync, rmSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { buildReport, renderReport } from "../lib/economy.mjs";
import { scratchDir } from "../lib/scratch.mjs";

const here = dirname(fileURLToPath(import.meta.url));
const fixture = join(here, "..", "test", "economy-fixtures", "planted");
const fails = [];
const expect = (ok, msg) => { if (!ok) fails.push(`FAIL ${msg}`); };

const report = buildReport(fixture, { mission: "FixturePack" });
expect(!report.error, `report error: ${report.error}`);
const lines = renderReport(report);
const has = (line) => lines.includes(line);

// The exact ladder, worked by hand: 5 parcel runs (6 min) open fuel (8 min); the taxi pays 0 so it never opens.
for (const line of [
  "start: bank $1000, cash $0",
  "  PARCEL  pay $40  unlock 0 runs  ~6 min  $400/h",
  "  FUEL    pay $60  unlock 5 runs  ~8 min  $450/h",
  "  TAXI    pay $0  unlock 15 runs  ~10 min  $0/h",
  "benefit: $100 per 1200 s below $500 = $300/h",
  "duty pay: police $1800/h, medic $1440/h",
  "  car      $1000  (S105_beige)",
  "  plot     $5000  (plot-a)",
  "  permit   $150  (firearm_permit)",
  "  clothing $45  (jacket_denim_brown)",
  "  hour 1: 8 runs (5 parcel, 3 fuel), earned $380, total $1380; can afford: car yes, plot no, permit yes, clothing yes",
  "  hour 2: 16 runs (5 parcel, 11 fuel), earned $860, total $1860; can afford: car yes, plot no, permit yes, clothing yes",
  "  hour 4: 31 runs (5 parcel, 26 fuel), earned $1760, total $2760; can afford: car yes, plot no, permit yes, clothing yes",
]) expect(has(line), `ladder line missing: "${line}"`);

const ids = report.flags.map((f) => f.id);
expect(ids.includes("shop:CementBag"), "loop flag: cement (core sells 40 buys 15; the pack patch buys at 60) not raised");
expect(ids.includes("fish:pike"), "fish flag: pike has no size prefab but a weight");
expect(!ids.includes("fish:eel"), "fish flag: eel has weight 0 and must not be flagged");
expect(!ids.includes("fish:trout"), "fish flag: trout has prefabs and must not be flagged");
expect(ids.includes("TAXI"), "zero-pay flag: the taxi run pays 0");
expect(ids.includes("slow:plot"), "slow flag: the 5000 plot is out of reach in 4 hours");
expect(!ids.some((i) => i.startsWith("sink:")), "sink flag: the fixture has income, so no sink-with-no-faucet");
expect(ids.length === 4, `exactly four flags expected, got ${ids.join(", ")}`);

// The overlay is what makes the loop: the same tree without the pack's patch has none (pack wins, core alone is clean).
const scratch = scratchDir("lf-economy-");
cpSync(fixture, scratch, { recursive: true });
rmSync(join(scratch, "addons", "FixturePack", "Configs", "Shop", "LF_ShopPrices_Fixture.conf"));
const bare = buildReport(scratch, { mission: "FixturePack" });
expect(!bare.flags.some((f) => f.id === "shop:CementBag"), "without the pack overlay the cement loop must not appear");

// A mission that names nothing is an error, not an empty report.
expect(!!buildReport(fixture, { mission: "NoSuchPack" }).error, "an unknown mission must be an error");

// --run-minutes changes the hourly pay and the ladder.
const slow = buildReport(fixture, { mission: "FixturePack", runMinutes: { parcel: 12 } });
expect(slow.runs[0].perHour === 200, `parcel at 12 min should pay $200/h, got ${slow.runs[0].perHour}`);

if (fails.length) {
  for (const f of fails) console.error(f);
  process.exit(1);
}
console.log("economy-report-selftest: OK");
