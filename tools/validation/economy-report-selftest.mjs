// Proves `tools\cli economy` (#3122, #3145) on a small planted fixture tree: the exact ladder lines out to hour 6, the
// flags it must raise (a cement buy-low sell-high loop that only appears once the pack overlay raises the sell price, a
// weighted fish with no prefab, a plot out of reach) and the flags it must not (an eel with weight 0), a police rank row
// that beats the core duty field, a taxi paid from the fare table, the harbour contract as a run line, `--hours`, and
// the driving licence fee as a purchase row.
// inputs: tools/commands/economy.mjs, tools/lib/economy.mjs, tools/test/economy-fixtures/**, tools/pack/layers.mjs, tools/economy/profit-loops.mjs, tools/validation/economy-report-selftest.mjs
// red-proof: before tools/lib/economy.mjs existed this selftest died on the missing import and exited 1; with the
// pack overlay left out of the merge (layers.mjs sectionLayers skipping it) the cement loop flag went missing and
// "FAIL loop flag" printed; for #3145 the new fixtures were written first and failed against the old reader
// (the plot line, taxi pay from PAY_TAXI, no duty ranks, no contract, no hour 6, no driving row); restored, it prints
// "economy-report-selftest: OK".
import { cpSync, rmSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { buildReport, renderReport } from "../lib/economy.mjs";
import { scratchDir } from "../lib/scratch.mjs";

const here = dirname(fileURLToPath(import.meta.url));
const fixture = join(here, "..", "test", "economy-fixtures", "planted");
const fails = [];
const expect = (ok, msg) => { if (!ok) fails.push(`FAIL ${msg}`); };
const cpFixture = () => {
  const dir = scratchDir("lf-economy-");
  cpSync(fixture, dir, { recursive: true });
  return dir;
};
const addons = (dir, ...p) => join(dir, "addons", ...p);

const report = buildReport(fixture, { mission: "FixturePack" });
expect(!report.error, `report error: ${report.error}`);
const lines = renderReport(report);
const has = (line) => lines.includes(line);

// The exact ladder, worked by hand: 5 parcel runs (6 min) open fuel (8 min); 15 runs in, the taxi opens (10 min, a
// $160 fare: floor 80 + 0.04 x 2000 m). It is then the best-paying run, so hours 4 and 6 add taxis only.
for (const line of [
  "start: bank $1000, cash $0",
  "  PARCEL  pay $40  unlock 0 runs  ~6 min  $400/h",
  "  FUEL    pay $60  unlock 5 runs  ~8 min  $450/h",
  "  TAXI    pay $160  unlock 15 runs  ~10 min  $960/h",
  "  CONTRACT harbour_crates  PARCEL x1.4  pay $56  needs standing, not in the ladder  ~6 min  $560/h",
  "benefit: $100 per 1200 s below $500 = $300/h",
  "duty pay: police rank 0 patrol $1080/h (the core field says $1800/h)",
  "  police ranks: patrol $1080/h, senior $1800/h",
  "duty pay: medic rank 0 orderly $720/h (the core field says $1440/h)",
  "  medic ranks: orderly $720/h, doctor $1080/h",
  "  car      $1000  (S105_beige)",
  "  plot     $8000  (plot-a)",
  "  permit   $150  (firearm_permit)",
  "  clothing $45  (jacket_denim_brown)",
  "  driving  $100  (driving licence fee)",
  "  hour 1: 8 runs (5 parcel, 3 fuel), earned $380, total $1380; can afford: car yes, plot no, permit yes, clothing yes, driving yes",
  "  hour 2: 16 runs (5 parcel, 10 fuel, 1 taxi), earned $960, total $1960; can afford: car yes, plot no, permit yes, clothing yes, driving yes",
  "  hour 4: 28 runs (5 parcel, 10 fuel, 13 taxi), earned $2880, total $3880; can afford: car yes, plot no, permit yes, clothing yes, driving yes",
  "  hour 6: 40 runs (5 parcel, 10 fuel, 25 taxi), earned $4800, total $5800; can afford: car yes, plot no, permit yes, clothing yes, driving yes",
]) expect(has(line), `ladder line missing: "${line}"`);
expect(report.ladder.map((h) => h.hour).join() === "1,2,4,6", "the default ladder is hours 1,2,4,6");

const ids = report.flags.map((f) => f.id);
expect(ids.includes("shop:CementBag"), "loop flag: cement (core sells 40 buys 15; the pack patch buys at 60) not raised");
expect(ids.includes("fish:pike"), "fish flag: pike has no size prefab but a weight");
expect(!ids.includes("fish:eel"), "fish flag: eel has weight 0 and must not be flagged");
expect(!ids.includes("fish:trout"), "fish flag: trout has prefabs and must not be flagged");
expect(!ids.includes("TAXI"), "zero-pay flag: the taxi has a fare table, it must not be flagged");
expect(ids.includes("slow:plot"), "slow flag: the 8000 plot is out of reach in 6 hours");
expect(report.flags.some((f) => f.text.includes("is out of reach after 6 h on the best run")), "slow flag text must say 'after 6 h'");
expect(!ids.some((i) => i.startsWith("sink:")), "sink flag: the fixture has income, so no sink-with-no-faucet");
expect(ids.length === 3, `exactly three flags expected, got ${ids.join(", ")}`);

// (a) A pack rank row with its own rate wins over the core duty field; a row that defers (-1) takes the core field.
expect(report.duty.police.rank0 === 1080 && report.duty.police.core === 1800, "(a) police rank 0 is the pack row's 0.3/s, not the core 0.5/s");
expect(report.duty.police.ranks[1].perHour === 1800, "(a) a police row with rate -1 defers to the core field");
{
  const noRows = cpFixture();
  rmSync(addons(noRows, "FixturePack", "Configs", "Police", "LF_Police_Fixture.conf"));
  const r = buildReport(noRows, { mission: "FixturePack" });
  expect(r.duty.police.rank0 === 1800 && r.duty.police.ranks.length === 0, "(a) with no rank rows the core duty field pays");
}

// (b) Taxi pay is the fare for the assumed trip: floor 80 up to the short hop, then floor + 0.04/m, capped at 300.
const taxi = (r) => r.runs.find((x) => x.job === "taxi").pay;
expect(taxi(report) === 160, `(b) default 2000 m taxi pays 160, got ${taxi(report)}`);
expect(taxi(buildReport(fixture, { mission: "FixturePack", taxiMetres: 50 })) === 80, "(b) a short hop pays the floor");
expect(taxi(buildReport(fixture, { mission: "FixturePack", taxiMetres: 20000 })) === 300, "(b) a long trip is capped at 300");
{
  const own = cpFixture();
  writeFileSync(join(own, "server", "flagship", "config.json"), JSON.stringify({ schemaVersion: 1, taxiFares: { floor: 100 }, licences: { driverLicenceFee: 120 } }));
  const r = buildReport(own, { mission: "FixturePack" });
  expect(taxi(r) === 180, `(b) the owner's taxiFares.floor 100 gives a 180 fare, got ${taxi(r)}`);
  // (e) the owner's licences.driverLicenceFee overrides the core 100.
  expect(r.sinks.driving?.price === 120, `(e) the owner's driverLicenceFee 120 wins, got ${r.sinks.driving?.price}`);
}
{
  const noFares = cpFixture();
  rmSync(addons(noFares, "LifeFramework", "Configs", "Jobs", "LF_TaxiFares.conf"));
  const r = buildReport(noFares, { mission: "FixturePack" });
  expect(taxi(r) === 0 && r.flags.some((f) => f.id === "TAXI"), "(b) no fare table: the taxi pays 0 and is flagged");
}

// (c) The harbour contract is a run line at 1.4x a parcel run, kept out of the ladder.
const harbour = report.contracts.find((c) => c.id === "harbour_crates");
expect(!!harbour && harbour.pay === 56 && harbour.perHour === 560 && harbour.job === "PARCEL", "(c) harbour_crates is 1.4 x $40 = $56 on a 6 min run");

// (d) --hours picks the rows.
expect(buildReport(fixture, { mission: "FixturePack", hours: [1, 3] }).ladder.map((h) => h.hour).join() === "1,3", "(d) hours [1,3] print those two rows");

// (e) The driving licence fee is a purchase row only when the licence exists in script.
expect(report.sinks.driving?.price === 100, "(e) the core fee 100 is the driving row");
{
  const noLicence = cpFixture();
  rmSync(addons(noLicence, "LifeFramework", "Scripts", "Game", "Feature", "License", "LF_LicencesConfig.c"));
  expect(buildReport(noLicence, { mission: "FixturePack" }).sinks.driving === null, "(e) no licence in script: no driving row");
}

// The overlay is what makes the loop: the same tree without the pack's patch has none (pack wins, core alone is clean).
const scratch = cpFixture();
rmSync(addons(scratch, "FixturePack", "Configs", "Shop", "LF_ShopPrices_Fixture.conf"));
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
