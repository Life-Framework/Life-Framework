// Proves the lumber loops of `tools\cli economy` (#3270, LB12): the pay ladder (#4178: a $300/h loop under the $350
// Cadet shift is flagged, $350 and $450 are not), the premium buyer's demand cap (first hour from the full purse, steady hour
// from the refill only), the wholesale curve (linear drop, floor, carry-over), and the loops read from planted configs
// (yield rows, sawmill outputs, a pack-less repo falling back to the design note's buyers and saying so).
// inputs: tools/lib/economy-lumber.mjs, tools/lib/economy.mjs, tools/commands/economy.mjs, tools/validation/economy-lumber-selftest.mjs
// red-proof: before tools/lib/economy-lumber.mjs existed this selftest died on the missing import and exited 1; with
// the band check (DUTY_BAND.max raised to 3.5) the "$300 flagged" check printed "FAIL 300/h flagged"; with the demand cap ignored
// (premium = units) the "cap limits premium" checks printed FAIL; restored, it prints "economy-lumber-selftest: OK".
// #4875: with a buyer trip costing nothing (`trip = () => 0`) five checks printed FAIL ("one plank a trip to buyers in town is
// below the rung (got 466/h)", "the premium planks take the near trip", ...); restored, OK.
import { mkdirSync, writeFileSync } from "node:fs";
import { AMOUNT } from "../../agent-box/proofs/_money.mjs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { FALLBACK_BUYERS, FALLBACK_CURVES, LUMBER_ASSUMPTIONS, rungCheck, buildLumber, computeLoops, handHaulHour, logSpeedCapOf, lumberFlags, plankBundleOf, renderLumber, sellWholesale, simulateProduct, tripsPerLog } from "../lib/economy-lumber.mjs";
import { buildReport, renderReport } from "../lib/economy.mjs";
import { scratchDir } from "../lib/scratch.mjs";

const here = dirname(fileURLToPath(import.meta.url));
const root = join(here, "..", "..");
const fails = [];
const expect = (ok, msg) => { if (!ok) fails.push(`FAIL ${msg}`); };
const near = (a, b, tol = 0.01) => Math.abs(a - b) <= tol;

// 1. The ladder: a loop under the lowest duty shift (the Cadet's $350) is flagged; at or above it is not.
const low = rungCheck(300, { police: 350, medic: 480 });
expect(low.flagged && low.floor === 350 && near(low.ratios[0].ratio, 1.17), `300/h flagged under $350 (got ${JSON.stringify(low)})`);
const mid = rungCheck(450, { police: 350, medic: 480 });
expect(!mid.flagged, `450/h not flagged (got ${JSON.stringify(mid)})`);
expect(!rungCheck(350, { police: 350 }).flagged && !rungCheck(1000, { police: 350 }).flagged, "the Cadet shift and a master loop are on the ladder");
expect(rungCheck(0, { police: 350 }).flagged, "a loop paying nothing is flagged");

// 2. The demand cap limits the premium units in the steady hour (and the first hour holds the full purse).
const buyer = { price: 10, cap: 40, refillPerHour: 20 };
const flat = { base: 4, floor: 4, dropPerUnitPct: 0, recoverPctPerHour: 0 };
const s = simulateProduct(70, buyer, flat);
expect(s.premiumFirst === 40, `first hour sells the full purse of 40 premium (got ${s.premiumFirst})`);
expect(s.premiumSteady === 20, `steady hour sells only the refill, 20 premium (got ${s.premiumSteady})`);
expect(s.first === 40 * 10 + 30 * 4 && s.steady === 20 * 10 + 50 * 4, `cash: first 520, steady 400 (got ${s.first}, ${s.steady})`);
const few = simulateProduct(5, buyer, flat);
expect(few.premiumSteady === 5 && few.steady === 50, "fewer units than the cap all sell premium");

// 3. The wholesale curve: linear drop of dropPerUnitPct of the base, held at the floor.
const w = sellWholesale({ base: 10, floor: 8, dropPerUnitPct: 10, recoverPctPerHour: 0 }, 10, 4);
expect(near(w.cash, 10 + 9 + 8 + 8) && w.price === 8, `10, 9, 8, 8 at the floor (got ${w.cash}, ${w.price})`);
const curve = { base: 6, floor: 3, dropPerUnitPct: 5, recoverPctPerHour: 10 };
const noPurse = { price: 0, cap: 0, refillPerHour: 0 };
const fresh = simulateProduct(30, noPurse, curve, false);
const carried = simulateProduct(30, noPurse, curve, true);
expect(near(fresh.first, fresh.steady) && carried.steady < carried.first, "carry-over depresses the steady hour, a fresh curve does not");

// 4. The loops from inputs: three loops, steady within the first hour, flagged under the lowest duty shift.
const inp = {
  assumptions: { ...LUMBER_ASSUMPTIONS, plankBundle: 4 }, duty: { police: 350, medic: 480 },
  yields: { softwood: { chopSecondsPerLog: 12.5 }, hardwood: { chopSecondsPerLog: 22.5 } },
  outputs: { plank: 4, post: 2, firewood: 4, board: 2, hardFirewood: 5 }, buyers: FALLBACK_BUYERS, curves: FALLBACK_CURVES,
};
const loops = computeLoops(inp);
expect(loops.length === 3, `three lumber loops (got ${loops.length})`);
for (const l of loops) expect(l.steady > 0 && l.steady <= l.first, `${l.id}: steady ${l.steady} within first hour ${l.first}`);
expect(near(loops[0].logsPerHour, 20.7, 0.3), `hand haul with bundles of 4 and the yard's buyers is about 21 logs an hour (got ${loops[0].logsPerHour})`);
// 4b. The plank trips (#4875): a trip per bundle, so one plank a trip to buyers in town is the ~$262 hour the proof measured.
expect(tripsPerLog(4, 4) === 1 && tripsPerLog(4, 1) === 4 && tripsPerLog(5, 4) === 2 && tripsPerLog(0, 4) === 0, "a log's planks take a trip per bundle");
const town = { ...inp.assumptions, plankBundle: 1, premiumMetres: 187, wholesaleMetres: 170 };
const single = computeLoops({ ...inp, assumptions: town });
expect(single[0].flagged && single[0].steady < loops[0].steady / 2, `one plank a trip to buyers in town is below the rung (got ${single[0].steady}/h)`);
const bundled = computeLoops({ ...inp, assumptions: { ...town, plankBundle: 4 } });
expect(bundled[0].steady > single[0].steady && bundled[0].steady < loops[0].steady, `bundles help, the yard's buyers help more (${single[0].steady} < ${bundled[0].steady} < ${loops[0].steady})`);
expect(loops[0].steady >= 400 && loops[0].steady <= 480, `the hand-haul planks hour is on the $400 to $480 entry rung (got ${loops[0].steady})`);
const h = handHaulHour(100, 4, 1, 20, { runMps: 2, premiumMetres: 10, wholesaleMetres: 100 });
expect(near(h.logs, 5 + (3600 - 5 * 110) / 200, 0.001) && near(h.trips, h.logs, 0.001), `the premium planks take the near trip, the rest the far one (got ${JSON.stringify(h)})`);
expect(plankBundleOf('LF_QuantityComponent "{5AD2B81F97B9381B}" {\n   m_iMaxQuantity 4\n  }') === 4 && plankBundleOf("MeshObject {}") === null, "the plank bundle is read from the prefab");
expect(logSpeedCapOf("   m_fSpeedCap 0.7\n") === 0.7 && logSpeedCapOf("") === null, "the log speed cap is read from the prefab");
expect(near(loops[2].logsPerHour, 45.6, 0.05), `the truck loop is 6 logs a 7.9 min cycle = 45.6 logs an hour (#4898, got ${loops[2].logsPerHour})`);
expect(loops[0].detail[0].premiumSteady === 20 && loops[0].detail[0].premiumFirst === 40, "the plank purse caps premium at 20 steady, 40 first");
expect(loops.every((l) => !l.flagged), `every fixture loop pays at least the $350 Cadet shift (got ${loops.map((l) => l.steady).join(", ")})`);
const thin = computeLoops({ ...inp, duty: { police: 2000, medic: 2000 } });
expect(thin.every((l) => l.flagged), "a $2000 duty shift puts every loop below the rung");
const flagged = lumberFlags({ loops: thin });
expect(flagged.length === thin.length && flagged[0].kind === "lumber-duty" && /Cadet shift/.test(flagged[0].text), "one lumber-duty flag per loop below the rung");

// 5. Read from planted configs: yield rows and sawmill outputs come from files; buyers fall back and say so.
const dir = scratchDir("lf-economy-lumber-");
const put = (rel, text) => { mkdirSync(dirname(join(dir, rel)), { recursive: true }); writeFileSync(join(dir, rel), text); };
const yieldRow = (n, cls, size, secs, logs) => `  LF_TreeYieldRow "{AA0000000000000${n}}" {\n   m_eClass ${cls}\n   m_iSize ${size}\n   m_iChopSeconds ${secs}\n   m_iLogs ${logs}\n  }\n`;
put("addons/LifeFramework/Configs/Lumber/LF_TreeSpecies.conf", `LF_TreeFellingConfig {\n m_aYields {\n${yieldRow(1, "SOFTWOOD", 2, 25, 2)}${yieldRow(2, "HARDWOOD", 2, 45, 2)} }\n}\n`);
const machine = (id, input, outs) => ` LF_ResourceMachineDef "{B00000000000000${id}}" {\n  m_sMachineId "m${id}"\n  m_aInputs {\n   LF_ProcessingInput "{B10000000000000${id}}" {\n    m_InputPrefab "{${input}}Prefabs/Log.et"\n    m_iInputAmount 1\n   }\n  }\n  m_aOutputs {\n${outs.map(([g, n], i) => `   LF_ProcessingOutput "{B2000000000000${id}${i}}" {\n    m_OutputPrefab "{${g}}Prefabs/X.et"\n    m_iOutputAmount ${n}\n   }\n`).join("")}  }\n }\n`;
put("addons/LifeFramework/Configs/Resources/ResourceChains.conf", `LF_ResourceChainConfig {\n m_aMachines {\n${machine(1, "32F5A906D8A573BC", [["45D1B36F99DD2FC3", 3]])}${machine(2, "32F5A906D8A573BC", [["7EB79C84E76D667D", 2]])}${machine(3, "32F5A906D8A573BC", [["F96D821A8E47FBBA", 4]])}${machine(4, "FC7D57CC9C0798D7", [["D2A8639FA0CA09EF", 2]])}${machine(5, "FC7D57CC9C0798D7", [["F96D821A8E47FBBA", 5]])} }\n}\n`);
const got = buildLumber({ addonsDir: join(dir, "addons"), packLayers: [], owner: null, duty: { police: 350, medic: 480 } });
expect(got.problems.length === 0 && got.loops.length === 3, `planted configs give three loops (problems: ${got.problems.join("; ")})`);
expect(got.loops[0]?.detail[0].unitsPerHour < 60, `the planted 3-plank sawmill sells fewer planks than the real 4 (got ${got.loops[0]?.detail[0].unitsPerHour})`);
expect(got.sources.some((x) => /plank: premium assumed/.test(x)), "a missing buyerDemand row is reported as assumed");
const lines = renderLumber(got);
expect(lines.some((l) => new RegExp(String.raw`LOOP hand-haul softwood -> planks: .*steady \$${AMOUNT}\/h, first hour \$${AMOUNT}\/h; duty .*(on the ladder|BELOW THE RUNG)`).test(l)), "a loop line prints steady, first hour and the rung");
const none = buildLumber({ addonsDir: join(dir, "nowhere"), packLayers: [], owner: null, duty: { police: 900, medic: 792 } });
expect(none.loops.length === 0 && /lumber loops skipped/.test(none.problems.join()), "no lumber config prints a problem, not a crash");

// 6. The real repo: the whole report carries the lumber section and a flag exactly for each loop below the rung.
const rep = buildReport(root, {});
expect(!rep.error && rep.lumber.loops.length === 3, `the real report has three lumber loops (${rep.error ?? ""})`);
expect(renderReport(rep).some((l) => l.startsWith("lumber loops")), "the report prints the lumber section");
expect(rep.flags.filter((f) => f.kind === "lumber-duty").length === rep.lumber.loops.filter((l) => l.flagged).length, "a lumber-duty flag per loop below the rung");

if (fails.length) {
  for (const f of fails) console.error(f);
  process.exit(1);
}
console.log("economy-lumber-selftest: OK");
