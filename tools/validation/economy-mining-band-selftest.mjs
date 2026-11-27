// Proves the tuned mining chains (#3246, E2 of #3206): the real core + 1989 pack configs, run through `tools\cli economy`,
// put iron by pickup, stone by truck and the coal feeder on the pay ladder (the copper chain left with the copper rock, #5069) (at or above the lowest duty
// shift, the Cadet's: #4178), iron near the design's $520 an hour, stone by truck on the high-tier rung near $656 (#4826, the truck is the climb), and flag only the foot haul of stone,
// which stays unviable on purpose (a 280 minute walk for 10 limestone).
// inputs: tools/lib/economy-mining.mjs, tools/economy/mining-assumptions.json, addons/LF_Everon1989/Configs/Market/LF_Market_Everon1989.conf, addons/LF_Everon1989/Configs/Market/LF_BuyerDemand_Everon1989.conf, addons/LifeFramework/Configs/Resources/ResourceChains.conf, tools/validation/economy-mining-band-selftest.mjs
// red-proof (#4869): with the ladder's bandProblems made to return [] the three bandProblems cases print FAIL; with the real check it prints OK.
// red-proof: on the #3240 rows (iron premium $26, copper wholesale 50 / 25 and premium $62, coal 3 / 2) this printed
// "FAIL iron by pickup ..." for iron $675/h and copper $667/h (1.3x and 1.4x, too rich) and "FAIL coal feeder" for $159/h
// (5.7x, too thin) against the old 1.5 to 2.5x band; #4178 replaced the band with the ladder; with the tuned rows it prints "economy-mining-band-selftest: OK".
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { buildReport } from "../lib/economy.mjs";
import { bandProblems } from "../lib/economy-mining.mjs";

const root = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
const fails = [];
const rep = buildReport(root, {});
const loops = rep.mining?.loops ?? [];
const by = (id) => loops.find((l) => l.id === id);
const want = { "iron-pickup": 520, "stone-truck": 656 };

if (rep.error || loops.length === 0) fails.push(`FAIL no mining loops in the report (${rep.error ?? rep.mining?.problems?.join("; ")})`);
for (const id of ["iron-pickup", "stone-truck", "coal-feeder"]) {
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
if (by("copper-car")) fails.push("FAIL the island has no copper rock (#5069): the report must carry no copper chain");
const iron = by("iron-pickup"), stone = by("stone-truck");
if (iron && stone && !(stone.steady > iron.steady)) fails.push("FAIL the chains must differ: stone by truck (the truck rung, #4826) pays most");

// #4869: the smelter's level steps (#4861) keep the iron loop in its band. The real ladder: every step level has a row, the
// hourly never falls as the level rises, and no row passes the top of the rung the base loop sits on or the crime ceiling.
const ladder = (rep.mining?.ladders ?? []).find((x) => x.id === "iron-pickup");
if (!ladder) fails.push("FAIL no smelting ladder for iron by pickup (the smelter-iron machine has no level steps in the report)");
else {
  if (ladder.rows.length < 3) fails.push(`FAIL the smelting ladder has ${ladder.rows.length} rows, expected the base and every step level`);
  for (let i = 1; i < ladder.rows.length; i++) if (ladder.rows[i].steady < ladder.rows[i - 1].steady) fails.push(`FAIL the smelting ladder falls at level ${ladder.rows[i].level}`);
  for (const p of ladder.problems) fails.push(`FAIL smelting band: ${p}`);
}
// The band check itself: a made-up step that makes a run instant and huge must be flagged (the red path).
const bad = bandProblems([{ level: 1, steady: 515, full: 658, rung: "mid" }, { level: 9, steady: 560, full: 960, rung: "mid" }]);
if (bad.length !== 1 || !/crime ceiling/.test(bad[0])) fails.push(`FAIL bandProblems must flag full perks at 960 past the mid crime ceiling and nothing else, got ${JSON.stringify(bad)}`);
if (bandProblems([{ level: 1, steady: 515, full: 658, rung: "mid" }, { level: 5, steady: 590, full: 760, rung: "mid" }]).length !== 0) fails.push("FAIL bandProblems flags a step that stays on the mid rung");
if (bandProblems([{ level: 1, steady: 515, full: 658, rung: "mid" }, { level: 5, steady: 610, full: 700, rung: "high" }]).length !== 1) fails.push("FAIL bandProblems must flag a step that climbs past the top of the base rung");
if (fails.length) {
  for (const f of fails) console.error(f);
  process.exit(1);
}
console.log("economy-mining-band-selftest: OK");
