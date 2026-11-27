// Proves the on-foot gathering loops sit on the pay ladder (pay audit 2026-11-27, the maintainer's order: police duty is
// the lowest pay): the model rolls only the species that bite at each hour, boosts the bait's species, prices sizes by
// the table's weights, times the bite (LF_FishingRules.BiteSeconds) and, once a spot is fished out, fills the rest of
// the hour with open-water small fish; then the shipped `tools\cli economy --json` report has every fishing spot of
// the live world and the orchard juice on the civilian entry rung ($400/h or more, above a typical patrol hour) and
// at most the $600/h mid rung.
// inputs: tools/lib/economy-gather.mjs, tools/lib/economy.mjs, tools/lib/economy-lumber.mjs, tools/commands/economy.mjs, tools/economy/profit-loops.mjs, addons/LifeFramework/Configs/Fishing/LF_Fishing.conf, addons/LF_Everon1989/Configs/Fishing/LF_Coves_Everon1989.conf, addons/LF_Everon1989/Prefabs/Trader/LF_WholesaleBuyer_Everon1989.et, addons/LF_Everon1989/Prefabs/Trader/LF_UniversalTrader_Everon1989.et, addons/LifeFramework/Configs/Resources/ResourceChains.conf, addons/LF_Everon1989/Worlds/MainWorld/MainWorld_Layers/Place_saint-philippe_pond.layer, addons/LF_Everon1989/Worlds/MainWorld/MainWorld_Layers/Place_fishermansbay_cove.layer, addons/LF_Everon1989/Worlds/MainWorld/MainWorld_Layers/Place_blacklake_shore.layer, addons/LF_Everon1989/Worlds/MainWorld/MainWorld_Layers/Place_saint-philippe_orchard.layer, tools/validation/economy-gather-selftest.mjs
// red-proof: on the 0.2.1 pond prices (trout 4/8/16, perch 3/7/13, carp 5/9/18) this printed "FAIL fishing black-lake
// $362/h steady is under the $400/h entry rung" and the same for spring-pond; with the 2026-11-27 pond medium and large
// rows (trout 12/22, perch 11/19, carp 13/24) both are $434/h and it prints OK.
import { spawnSync } from "node:child_process";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { fishingModel, meanBiteSeconds, orchardLoop, spotLoop, spotsOf, tableValue, traderPrices } from "../lib/economy-gather.mjs";
import { LADDER } from "../lib/economy-lumber.mjs";

const root = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
const FLOOR = LADDER.entry.min, CEILING = LADDER.mid.max;
const fails = [];
const expect = (ok, msg) => { if (!ok) fails.push(`FAIL ${msg}`); };
const duty = { police: 350, medic: 480 };

// 1. The model on a planted config: a night-only fish counts only in its hours, the bait doubles its species.
const conf = (props) => ({ cls: "LF_FishingConfig", props });
const list = (...xs) => ({ list: xs });
const model = fishingModel([conf({
  m_fBaitBoost: 2,
  m_aBaits: list({ props: { m_aBoostSpecies: list("a") } }),
  m_aSpecies: list(
    { props: { m_sId: "a", m_aSizePrefabs: list("{AAAAAAAAAAAAAAA1}x.et\" \"{AAAAAAAAAAAAAAA2}y.et\" \"{AAAAAAAAAAAAAAA3}z.et") } },
    { props: { m_sId: "b", m_aSizePrefabs: list("{BBBBBBBBBBBBBBB1}x.et\" \"{BBBBBBBBBBBBBBB2}y.et\" \"{BBBBBBBBBBBBBBB3}z.et"), m_aHours: list("0 1 2 3 4 5 6 7 8 9 10 11") } },
  ),
  m_aTables: list({ props: { m_sId: "t", m_aSpecies: list({ props: { m_sSpecies: "a", m_iWeight: 1 } }, { props: { m_sSpecies: "b", m_iWeight: 2 } }), m_aSizeWeights: list("1 0 0") } }),
})]);
const prices = new Map([["AAAAAAAAAAAAAAA1", 4], ["BBBBBBBBBBBBBBB1", 10]]);
// Hours 0-11: a (weight 1 x bait 2) and b (weight 2) split evenly, $7 a fish; hours 12-23: a only, $4. Day average $5.50.
expect(tableValue(model, "t", prices, true).perFish === 5.5, `the day average is $5.50 a fish (got ${tableValue(model, "t", prices, true).perFish})`);
// Without bait: hours 0-11 are (4 + 2 x 10) / 3 = $8; the average is $6.
expect(tableValue(model, "t", prices, false).perFish === 6, `without bait the average is $6 (got ${tableValue(model, "t", prices, false).perFish})`);
// A 30 s cycle at the spot, 45 s in open water: a 90 s regrow is 40 spot fish (1200 s), the other 2400 s give 53.3
// open-water fish. At $10 and $3: $560 steady; a 10-fish stock makes 50 spot fish and 46.7 open: $640 the first hour.
const t = { spotBite: 25, openBite: 40, handle: 5 };
const spot = { id: "s", table: "t", stock: 10, regrowSeconds: 90 };
const loop = spotLoop(spot, { perFish: 10, deadHours: 0 }, { perFish: 3, deadHours: 0 }, t, duty);
expect(loop.fishPerHour === 40 && loop.openFishPerHour === 53.3 && loop.steady === 560 && loop.first === 640, `spot plus open water: 40 + 53.3 fish, $560 steady, $640 first hour (got ${JSON.stringify(loop)})`);
const half = spotLoop(spot, { perFish: 10, deadHours: 12 }, { perFish: 0, deadHours: 24 }, t, duty);
expect(half.fishPerHour === 20 && half.steady === 200, `twelve dead hours halve the spot catch once (got ${half.fishPerHour} fish, $${half.steady})`);
expect(spotLoop(spot, { perFish: 5, deadHours: 0 }, { perFish: 1, deadHours: 0 }, t, duty).rung.flagged, "40 fish at $5 and 53 at $1 ($253) is under the $350 Cadet shift");
// LF_FishingRules.BiteSeconds: 15 + 45 x roll, / 1.25 bait, clamped to 15..60: a mean of 30.1 s; an emptied spot (x1.5) 43.7 s.
const timing = { biteMin: 15, biteMax: 60, emptySlowdown: 1.5, rodSpeed: 1, baitSpeed: 1.25 };
expect(Math.abs(meanBiteSeconds(timing, true, false) - 30.125) < 0.05, `the mean bite with bait is 30.1 s (got ${meanBiteSeconds(timing, true, false)})`);
expect(Math.abs(meanBiteSeconds(timing, true, true) - 43.67) < 0.1, `the mean bite at an emptied spot is 43.7 s (got ${meanBiteSeconds(timing, true, true)})`);
const o = orchardLoop({ trees: 4, regrowSeconds: 180, maxGathers: 10, input: 3, output: 1, price: 18, duty });
expect(o.steady === 480 && o.first === 720, `4 trees at a 180 s regrow, 3 apples a $18 juice: $480 steady, $720 first hour (got ${o.steady}, ${o.first})`);
const spots = spotsOf([{ file: "x.layer", text: 'GenericEntity Pond_spot : "{E8C5BD1716B703E0}Prefabs/Systems/Fishing/LF_FishingSpot.et" {\n components {\n  LF_FishingSpotComponent "{1}" {\n   m_sId "pond-a"\n   m_sTable "pond"\n   m_iStock 9\n  }\n }\n coords 1 2 3\n}\n' }]);
expect(spots.length === 1 && spots[0].id === "pond-a" && spots[0].stock === 9 && spots[0].regrowSeconds === 120, `a spot entity parses with the 120 s prefab default (got ${JSON.stringify(spots)})`);
expect(traderPrices(['m_ItemPrefab "{AAAAAAAAAAAAAAA1}x.et"\n     m_ValuePerItem 4', 'm_ItemPrefab "{AAAAAAAAAAAAAAA1}x.et"\n m_ValuePerItem 6']).get("AAAAAAAAAAAAAAA1") === 6, "the best trader price wins");

// 2. The shipped report: every spot of the live world and the orchard on the ladder.
const run = spawnSync(process.execPath, [join(root, "tools", "cli.mjs"), "economy", "--json"], { cwd: root, encoding: "utf8", windowsHide: true, maxBuffer: 64 * 1024 * 1024 });
let rep = null;
try {
  rep = JSON.parse(run.stdout);
} catch {
  fails.push(`FAIL tools\\cli economy --json did not print a report (exit ${run.status}): ${String(run.stderr).slice(0, 400)}`);
}
if (rep) {
  const fishing = rep.gather?.fishing ?? [];
  expect(fishing.length >= 3, `the live world has its three fishing spots (got ${fishing.length})`);
  for (const x of [...fishing, ...(rep.gather?.orchard ? [rep.gather.orchard] : [])]) {
    expect(x.steady >= FLOOR, `${x.kind} ${x.id} $${x.steady}/h steady is under the $${FLOOR}/h entry rung`);
    expect(x.steady <= CEILING, `${x.kind} ${x.id} $${x.steady}/h steady is past the $${CEILING}/h mid rung for an on-foot loop`);
  }
  expect(Boolean(rep.gather?.orchard), "the orchard apples-to-juice loop is in the report");
}

if (fails.length) {
  for (const f of fails) console.error(f);
  process.exit(1);
}
console.log("economy-gather-selftest: OK");
