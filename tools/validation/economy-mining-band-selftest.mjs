// Proves the tuned mining chains (#3246, E2 of #3206): the real core + 1989 pack configs, run through `tools\cli economy`,
// put iron by pickup, copper by car, stone by truck and the coal feeder on the pay ladder (at or above the lowest duty
// shift, the Cadet's: #4178), the three plant chains near the design's $520 / $510 / $490 an hour, and flag only the foot haul of stone,
// which stays unviable on purpose (a 280 minute walk for 10 limestone).
// inputs: tools/lib/economy-mining.mjs, tools/economy/mining-assumptions.json, addons/LF_Everon1989/Configs/Market/LF_Market_Everon1989.conf, addons/LF_Everon1989/Configs/Market/LF_BuyerDemand_Everon1989.conf, addons/LifeFramework/Configs/Resources/ResourceChains.conf, tools/validation/economy-mining-band-selftest.mjs
// red-proof: on the #3240 rows (iron premium $26, copper wholesale 50 / 25 and premium $62, coal 3 / 2) this printed
// "FAIL iron by pickup ..." for iron $675/h and copper $667/h (1.3x and 1.4x, too rich) and "FAIL coal feeder" for $159/h
// (5.7x, too thin) against the old 1.5 to 2.5x band; #4178 replaced the band with the ladder; with the tuned rows it prints "economy-mining-band-selftest: OK".
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { buildReport } from "../lib/economy.mjs";

const root = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
const fails = [];
const rep = buildReport(root, {});
const loops = rep.mining?.loops ?? [];
const by = (id) => loops.find((l) => l.id === id);
const want = { "iron-pickup": 520, "copper-car": 510, "stone-truck": 490 };

if (rep.error || loops.length === 0) fails.push(`FAIL no mining loops in the report (${rep.error ?? rep.mining?.problems?.join("; ")})`);
for (const id of ["iron-pickup", "copper-car", "stone-truck", "coal-feeder"]) {
  const l = by(id);
  if (!l) { fails.push(`FAIL ${id} is missing from the report`); continue; }
  if (l.flagged) fails.push(`FAIL ${l.label} is below the rung: $${l.steady}/h under the $${l.floor}/h Cadet shift`);
}
for (const [id, target] of Object.entries(want)) {
  const l = by(id);
  if (l && Math.abs(l.steady - target) > target * 0.15) fails.push(`FAIL ${l.label}: $${l.steady}/h steady is more than 15% from the design's $${target}/h`);
}
const flagged = loops.filter((l) => l.flagged).map((l) => l.id);
if (flagged.join() !== "stone-foot") fails.push(`FAIL only stone-foot may stay below the rung (got ${flagged.join() || "none"})`);
const iron = by("iron-pickup"), copper = by("copper-car"), stone = by("stone-truck");
if (iron && copper && stone && !(copper.steady <= iron.steady + 40 && stone.steady < iron.steady)) fails.push("FAIL the chains must differ: stone (the entry job) pays least, iron and copper within $40 of each other");

if (fails.length) {
  for (const f of fails) console.error(f);
  process.exit(1);
}
console.log("economy-mining-band-selftest: OK");
