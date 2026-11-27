// Proves the mining chains of `tools\cli economy` (#3240, E1 of #3206): a fixture recipe and curve give an exact $/h
// (trips an hour x product, less reagent and fuel), the pay ladder (#4178) flags a chain paying under the lowest duty
// shift, a premium purse is spent before the wholesale curve and refills (first hour full, steady hour the
// refill), a lot-priced curve pays per lot (fractionally), a load over what the vehicle carries is a problem, the
// chains read from planted configs, and the shipped report carries all five chains. It also proves the three mining
// recipes in `validate-profit-loops`: a missing recipe fails, an all-shop-bought recipe and a shop-to-trader reagent loop
// are found.
// inputs: tools/lib/economy-mining.mjs, tools/lib/economy-mining-recipes.mjs, tools/lib/economy.mjs, tools/economy/mining-assumptions.json, tools/economy/profit-loops.mjs, tools/validation/economy-mining-selftest.mjs
// red-proof: before tools/lib/economy-mining.mjs existed this selftest died on the missing import and exited 1; with the
// reagent cost left out of computeMiningLoop the "net $400/h" check printed "FAIL net 500/h"; with the band check (DUTY_BAND.max
// raised to 3.5) the "3.0x is too thin" check printed FAIL; restored, it prints "economy-mining-selftest: OK".
import { mkdirSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { MINING_RECIPES, findMiningRecipeLoops, sellPrices } from "../lib/economy-mining-recipes.mjs";
import { buildMining, computeMiningLoop, miningFlags, recipeOf, renderMining, sellCurve, simulateSales, tripCapacity } from "../lib/economy-mining.mjs";
import { buildReport, renderReport } from "../lib/economy.mjs";
import { DEFAULT_SOURCES, parseConf, loadShop } from "../economy/profit-loops.mjs";
import { scratchDir } from "../lib/scratch.mjs";

const here = dirname(fileURLToPath(import.meta.url));
const root = join(here, "..", "..");
const fails = [];
const expect = (ok, msg) => { if (!ok) fails.push(`FAIL ${msg}`); };
const near = (a, b, tol = 0.01) => Math.abs(a - b) <= tol;

// 1. The wholesale curve: linear drop per lot, held at the floor, fractional lots pay their share.
const drop = { base: 10, floor: 8, dropPerUnitPct: 10, recoverPctPerHour: 0, lotSize: 1 };
expect(near(sellCurve(drop, 10, 4).cash, 10 + 9 + 8 + 8), `10, 9, 8, 8 at the floor (got ${sellCurve(drop, 10, 4).cash})`);
expect(near(sellCurve({ base: 10, floor: 10, dropPerUnitPct: 0, lotSize: 5 }, 10, 1).cash, 2), "1 unit of a 5 unit lot at $10 is $2");
expect(near(sellCurve({ base: 10, floor: 10, dropPerUnitPct: 0, lotSize: 2 }, 10, 50).cash, 250), "50 units of 2 unit lots at $10 is $250");

// 2. The premium purse: first hour from the full purse, steady hour from the refill, the rest wholesale.
const flat = { base: 10, floor: 10, dropPerUnitPct: 0, recoverPctPerHour: 0, lotSize: 1 };
const purse = simulateSales(50, { price: 20, cap: 20, refillPerHour: 10, lotSize: 1 }, flat);
expect(purse.first === 20 * 20 + 30 * 10 && purse.steady === 10 * 20 + 40 * 10, `purse: first 700, steady 600 (got ${purse.first}, ${purse.steady})`);
expect(purse.premiumFirst === 20 && purse.premiumSteady === 10, "purse: 20 premium units first hour, 10 steady");
const carry = simulateSales(50, null, { base: 10, floor: 5, dropPerUnitPct: 2, recoverPctPerHour: 10, lotSize: 1 }, true);
expect(carry.steady < carry.first, "carry-over depresses the steady hour");

// 3. One chain from inputs. Gather 20 ore at 4 a minute (5 min), 2 ore + 1 reagent -> 1 bar, 5 batches a run, 60 s a run:
// 10 batches, 2 runs, 2 min; 1 + 2 + 1 km by car, 4 min; 1 min overhead: a 12 min trip, 5 trips and 50 bars an hour.
const a = {
  minutesPerKm: { car: 1, foot: 15 }, sackUnits: { ore: 10 },
  vehicles: { car: { sacks: 0, looseUnits: 20 }, pickup: { sacks: 6 }, foot: { sacks: 2 } },
};
const chain = {
  id: "fx", label: "fixture", vehicle: "car", gather: { item: "ore", units: 20, perMinute: 4 },
  reagents: [{ item: "acid", unitCost: 2 }], km: { mineToPlant: 1, plantToBuyer: 2, returnToMine: 1 }, overheadMinutes: 1, fuelPerHour: 0,
};
const recipe = { gatheredPerBatch: 2, outputPerBatch: 1, reagentPerBatch: { acid: 1 }, maxBatches: 5, processSeconds: 60 };
const loop = computeMiningLoop({ chain, assumptions: a, recipe, curve: flat, premium: null, duty: { police: 350, medic: 480 } });
expect(loop.tripMinutes === 12 && loop.tripsPerHour === 5, `a 12 minute trip, 5 trips an hour (got ${loop.tripMinutes}, ${loop.tripsPerHour})`);
expect(loop.productPerTrip === 10 && loop.unitsPerHour === 50, `10 bars a trip, 50 an hour (got ${loop.productPerTrip}, ${loop.unitsPerHour})`);
expect(loop.grossSteady === 500 && loop.costPerHour === 100, `gross 500, reagent 5 trips x 10 batches x 2 = 100 (got ${loop.grossSteady}, ${loop.costPerHour})`);
expect(loop.steady === 400 && loop.first === 400, `net 400/h (got ${loop.steady}/${loop.first})`);
expect(near(loop.ratios[0].ratio, 0.88) && !loop.flagged && loop.floor === 350, `$400 is above the $350 Cadet shift (got ${JSON.stringify(loop.ratios)})`);
expect(near(loop.ratios[1].ratio, 1.2), `480 / 400 is 1.2x (got ${loop.ratios[1].ratio})`);
const withFuel = computeMiningLoop({ chain: { ...chain, fuelPerHour: 40 }, assumptions: a, recipe, curve: flat, premium: null, duty: { police: 900 } });
expect(withFuel.steady === 360, `fuel $40 an hour comes off the net (got ${withFuel.steady})`);
const thin = computeMiningLoop({ chain, assumptions: a, recipe, curve: flat, premium: null, duty: { police: 1200 } });
expect(thin.flagged && near(thin.ratios[0].ratio, 3), `$400 under a $1200 shift is below the rung (got ${thin.ratios[0].ratio})`);
expect(!computeMiningLoop({ chain, assumptions: a, recipe, curve: flat, premium: null, duty: { police: 400 } }).flagged, "$400 at a $400 shift is on the ladder");
expect(miningFlags({ loops: [thin, loop] }).length === 1 && miningFlags({ loops: [thin] })[0].kind === "mining-duty" && /Cadet shift/.test(miningFlags({ loops: [thin] })[0].text), "one mining-duty flag per chain below the rung");
const slow = computeMiningLoop({ chain: { ...chain, vehicle: "foot", km: { mineToPlant: 5, plantToBuyer: 5, returnToMine: 5 } }, assumptions: a, recipe, curve: flat, premium: null, duty: { police: 900 } });
expect(slow.tripsPerHour < 1 && slow.steady > 0 && slow.steady < 100, `a long walk earns a fraction of a trip an hour, not zero (got ${slow.steady}, ${slow.tripsPerHour} trips)`);
const premium = computeMiningLoop({ chain, assumptions: a, recipe, curve: flat, premium: { price: 20, cap: 20, refillPerHour: 10, lotSize: 1 }, duty: { police: 900 } });
expect(premium.first === 700 - 100 && premium.steady === 600 - 100 && premium.premiumSteady === 10, `purse: first 600, steady 500 net (got ${premium.first}, ${premium.steady})`);

// 4. Loads: sacks (#3265) or a car's loose boot; over the load is a problem.
expect(tripCapacity("pickup", "ore", a) === 60 && tripCapacity("car", "ore", a) === 20 && tripCapacity("foot", "ore", a) === 20, "pickup 6 sacks of 10, car boot 20, foot 2 sacks");
const heavy = computeMiningLoop({ chain: { ...chain, gather: { item: "ore", units: 21, perMinute: 4 } }, assumptions: a, recipe, curve: flat, premium: null, duty: { police: 900 } });
expect(heavy.problems.length === 1 && /carries 20/.test(heavy.problems[0]), `21 ore in a car boot of 20 is a problem (got ${heavy.problems})`);

// 5. Read from planted configs: the recipe, the curve and the premium row come from files.
const dir = scratchDir("lf-economy-mining-");
const put = (rel, text) => { mkdirSync(dirname(join(dir, rel)), { recursive: true }); writeFileSync(join(dir, rel), text); };
const machine = `  LF_ResourceMachineDef "{B000000000000001}" {\n   m_sMachineId "smelter-fx"\n   m_aInputs {\n    LF_ProcessingInput "{B100000000000001}" {\n     m_InputPrefab "{1111111111111111}Prefabs/Ore.et"\n     m_iInputAmount 2\n    }\n    LF_ProcessingInput "{B100000000000002}" {\n     m_InputPrefab "{2222222222222222}Prefabs/Acid.et"\n     m_iInputAmount 1\n    }\n   }\n   m_aOutputs {\n    LF_ProcessingOutput "{B200000000000001}" {\n     m_OutputPrefab "{3333333333333333}Prefabs/Bar.et"\n     m_iOutputAmount 1\n    }\n   }\n   m_iMaxBatches 5\n   m_iProcessSeconds 60\n  }\n`;
const chainsConf = `LF_ResourceChainConfig {\n m_aMachines {\n${machine} }\n}\n`;
put("addons/LifeFramework/Configs/Resources/ResourceChains.conf", chainsConf);
put("addons/LifeFramework/Configs/Market/LF_Market.conf", `LF_MarketConfig {\n m_aGoods {\n  LF_MarketGoodDef "{4000000000000001}" {\n   m_sGood "{3333333333333333}Prefabs/Bar.et"\n   m_iBase 10\n   m_iFloor 10\n   m_iCeiling 10\n   m_iDropPerUnitPct 0\n   m_iRecoverPctPerHour 0\n  }\n }\n}\n`);
const rec = recipeOf(parseConf(chainsConf), "smelter-fx", { reagents: [{ item: "acid", guidName: "Acid" }] });
expect(rec && rec.gatheredPerBatch === 2 && rec.reagentPerBatch.acid === 1 && rec.maxBatches === 5 && rec.processSeconds === 60 && rec.outputPerBatch === 1, `recipe read from the machine (got ${JSON.stringify(rec)})`);
const assumptionsFile = join(dir, "mining.json");
writeFileSync(assumptionsFile, JSON.stringify({ ...a, chains: [{ ...chain, recipe: "smelter-fx", reagents: [{ item: "acid", guidName: "Acid", unitCost: 2 }], buyer: null, wholesaleGood: "3333333333333333" }] }));
const got = buildMining({ root: dir, addonsDir: join(dir, "addons"), packLayers: [], owner: null, duty: { police: 350, medic: 480 }, opts: { miningAssumptions: assumptionsFile } });
expect(got.problems.length === 0 && got.loops.length === 1 && got.loops[0].steady === 400, `planted configs give the fixture's $400/h (problems: ${got.problems.join("; ")}, steady ${got.loops[0]?.steady})`);
expect(renderMining(got).some((l) => /LOOP fixture: steady \$400\/h net, first hour \$400\/h; duty .* on the ladder/.test(l)), "the loop line prints net steady, first hour and the rung");
writeFileSync(assumptionsFile, JSON.stringify({ ...a, chains: [{ ...chain, recipe: "gone", reagents: [], buyer: null, wholesaleGood: "3333333333333333" }] }));
const gone = buildMining({ root: dir, addonsDir: join(dir, "addons"), packLayers: [], owner: null, duty: { police: 900 }, opts: { miningAssumptions: assumptionsFile } });
expect(gone.loops.length === 0 && /no machine 'gone'/.test(gone.problems.join()), "a recipe the config lacks prints a problem, not a crash");
const none = buildMining({ root: join(dir, "nowhere"), addonsDir: join(dir, "nowhere", "addons"), packLayers: [], owner: null, duty: { police: 900 } });
expect(none.loops.length === 0 && /mining loops skipped/.test(none.problems.join()), "no assumptions file prints a problem, not a crash");

// 6. The shipped report: the island chains (the copper chain left with the copper rock, #5069), flags exactly for the ones below the rung, and every chain has a haul and a plant.
const rep = buildReport(root, {});
const ids = (rep.mining?.loops ?? []).map((l) => l.id);
expect(!rep.error && ["iron-pickup", "stone-truck", "stone-foot", "coal-feeder"].every((i) => ids.includes(i)) && !ids.includes("copper-car"), `the real report has the island mining chains and no copper chain (#5069: the island has no copper rock) (got ${ids.join(", ")}; ${rep.mining?.problems?.join("; ")})`);
expect((rep.mining?.problems ?? []).length === 0, `no mining problem in the real report (${rep.mining?.problems?.join("; ")})`);
expect(renderReport(rep).some((l) => l.startsWith("mining loops")), "the report prints the mining section");
expect(rep.flags.filter((f) => f.kind === "mining-duty").length === rep.mining.loops.filter((l) => l.flagged).length, "a mining-duty flag per chain below the rung");
for (const l of rep.mining?.loops ?? []) expect(l.tripMinutes > 0 && l.driveMinutes > 0 && Number.isFinite(l.steady), `${l.id}: a real trip with a haul`);
const truck = rep.mining?.loops.find((l) => l.id === "stone-truck");
expect(truck && truck.batchesPerTrip === 15 && truck.productPerTrip === 30, `a truck trip of 60 limestone is 15 batches and 30 bags (got ${truck?.batchesPerTrip}, ${truck?.productPerTrip})`);

// 7. validate-profit-loops: the three recipes exist, and a shop-to-trader loop on them is found.
const shop = loadShop(parseConf(`LF_ShopPriceConfig {\n m_iDefaultBuyPrice 50\n m_iDefaultSellPrice 25\n m_aPriceRules {\n  LF_ShopPriceRule "{5000000000000001}" {\n   m_Prefab "{1111111111111111}Prefabs/Ore.et"\n   m_iBuyPrice 3\n   m_iSellPrice 1\n  }\n  LF_ShopPriceRule "{5000000000000002}" {\n   m_Prefab "{2222222222222222}Prefabs/Acid.et"\n   m_iBuyPrice 8\n   m_iSellPrice 1\n  }\n }\n}\n`));
const chains = parseConf(chainsConf);
const prices = new Map([["3333333333333333", 30]]);
const all = findMiningRecipeLoops(shop, chains, prices, ["smelter-fx"]);
expect(all.length === 1 && all[0].id === "mining:smelter-fx" && all[0].cost === 14 && all[0].margin === 16, `every input shop-bought (2 x 3 + 8 = 14), 1 bar sold at 30: +16 (got ${JSON.stringify(all)})`);
expect(findMiningRecipeLoops(shop, chains, new Map([["3333333333333333", 10]]), ["smelter-fx"]).length === 0, "a bar worth 10 against 14 of inputs is no loop");
const reagent = findMiningRecipeLoops(shop, chains, new Map([["3333333333333333", 1], ["2222222222222222", 9]]), ["smelter-fx"]);
expect(reagent.length === 1 && reagent[0].id === "mining:smelter-fx:Acid" && reagent[0].margin === 1, `acid bought at 8 and sold to the market at 9 is a +1 loop (got ${JSON.stringify(reagent)})`);
const missing = findMiningRecipeLoops(shop, chains, prices, ["smelter-fx", "refinery-copper"]);
expect(missing.some((l) => l.id === "mining:refinery-copper" && /missing/.test(l.path)), "a recipe the chains lack fails");
expect(MINING_RECIPES.join() === "smelter-iron,refinery-copper,cement-works", "the three recipes the chain design names");
expect(DEFAULT_SOURCES.miningRecipes.join() === MINING_RECIPES.join(), "profit-loops checks the same three recipes");
expect(sellPrices([], []).size === 0, "no market files: no prices");

if (fails.length) {
  for (const f of fails) console.error(f);
  process.exit(1);
}
console.log("economy-mining-selftest: OK");
