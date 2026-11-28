// Proves the crime section of `tools\cli economy` (X-2, #4715; docs/roadmap/next-release-0.3.0.md "The economy table",
// crime). Each crime line's expected clean $/h (gross after the pack's wash cut, less fail share and inputs) must sit in
// its band from the plan, and a line under the civilian rung it sits above is flagged. Fixtures are fed by hand; the
// pack's wash cut (35) and moonshine buyer price (80) are read from the real configs.
// inputs: tools/lib/economy-crime.mjs, tools/lib/economy-perks.mjs (RUNGS),
//         addons/LF_Everon1989/Configs/Crime/LF_Launder_Everon1989.conf, addons/LF_Everon1989/Configs/Crime/LF_BuyerPrices_Everon1989.conf
// red-proof: written first, before tools/lib/economy-crime.mjs existed; the run failed at the import with
// "Cannot find module .../economy-crime.mjs", so no expectation below could pass.
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { CRIME_LINES, cleanPerHour, crimeSection, plannedInputs, readCrimeReadings } from "../lib/economy-crime.mjs";

const root = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
const fails = [];
const expect = (ok, msg) => { if (!ok) fails.push(`FAIL ${msg}`); };

// 1. The arithmetic: $1,000 gross, no fail share, 35% wash, $100 inputs is $550 clean; a 10% fail share first.
expect(cleanPerHour({ gross: 1000, failShare: 0, cost: 100 }, 35) === 550, `$1,000 gross after a 35% wash less $100 is $550 (got ${cleanPerHour({ gross: 1000, failShare: 0, cost: 100 }, 35)})`);
expect(cleanPerHour({ gross: 1000, failShare: 0.1, cost: 100 }, 35) === 485, `a 10% fail share first gives $485 (got ${cleanPerHour({ gross: 1000, failShare: 0.1, cost: 100 }, 35)})`);

// 2. Every plan line is in the table with its band, and the civilian rung it sits above.
const ids = CRIME_LINES.map((l) => l.id).join(",");
expect(ids === "moonshine,chop-shop,run,till,crew,gang", `six crime lines in plan order (got ${ids})`);
expect(CRIME_LINES.find((l) => l.id === "moonshine")?.band.join("-") === "500-650", "moonshine band is $500 to $650");
expect(CRIME_LINES.find((l) => l.id === "crew")?.rung === "master", "the crew evening sits on the master rung");

// 3. A fixture where every line is in band: no flag, every line says in band.
const inBand = {
  moonshine: { gross: 960, failShare: 0.05, cost: 60 }, // 912 x 0.65 - 60 = 533
  "chop-shop": { gross: 1300, failShare: 0.1, cost: 150 }, // 610
  run: { gross: 1900, failShare: 0.15, cost: 250 }, // 800
  till: { gross: 1900, failShare: 0.2, cost: 200 }, // 788
  crew: { gross: 3000, failShare: 0.25, cost: 400 }, // 1062
  gang: { gross: 2800, failShare: 0.3, cost: 300 }, // 974
};
const ok = crimeSection({ cutPct: 35, inputs: inBand });
expect(ok.rows.length === 6, `six rows (got ${ok.rows.length})`);
expect(ok.rows.every((r) => r.inBand), `every fixture line is inside its band (got ${ok.rows.filter((r) => !r.inBand).map((r) => r.id).join(",")})`);
expect(ok.rows.find((r) => r.id === "moonshine")?.clean === 533, `moonshine reads $533 (got ${ok.rows.find((r) => r.id === "moonshine")?.clean})`);
expect(ok.flags.length === 0, `no flag when every line clears its rung (got ${ok.flags.map((f) => f.id).join(",")})`);
expect(ok.lines.some((l) => l.includes("CRIME Moonshine") && l.includes("$533/h")), "the moonshine line prints $533/h");

// 4. A fixture that drops the chop shop under the mid rung ($480): $440 clean is flagged, nothing else is.
const drop = { ...inBand, "chop-shop": { gross: 1000, failShare: 0.1, cost: 200 } }; // 900 x 0.65 - 200 = 385
const d = crimeSection({ cutPct: 35, inputs: drop });
const chop = d.rows.find((r) => r.id === "chop-shop");
expect(chop && chop.clean < 480, `the fixture puts the chop shop under the mid rung (got ${chop?.clean})`);
expect(d.flags.length === 1 && d.flags[0].id === "crime-rung:chop-shop", `only the chop shop is flagged (got ${d.flags.map((f) => f.id).join(",") || "none"})`);
expect(d.lines.some((l) => l.includes("Chop shop") && l.includes("UNDER ITS RUNG")), "the chop shop line says it is under its rung");

// 5. A line with no input is printed as missing, not invented, and is not a flag.
const missing = crimeSection({ cutPct: 35, inputs: { moonshine: inBand.moonshine } });
expect(missing.rows.find((r) => r.id === "crew")?.clean === null, "a line with no input has no clean value");
expect(missing.flags.length === 0, "a missing input is not a rung flag");

// 6. The real configs: the wash cut is 35 and the moonshine buyer price is $80; the planned inputs land every line in band.
const real = readCrimeReadings(root);
expect(real.cutPct === 35, `the pack wash cut reads 35 (got ${real.cutPct})`);
expect(real.moonshinePrice === 80, `the pack moonshine buyer price reads $80 (got ${real.moonshinePrice})`);
const planned = crimeSection({ cutPct: real.cutPct, inputs: plannedInputs(real) });
expect(planned.rows.every((r) => r.inBand), `the planned run puts every line in band (got ${planned.rows.filter((r) => !r.inBand).map((r) => `${r.id}=${r.clean}`).join(",")})`);
expect(planned.flags.length === 0, `the planned run flags nothing (got ${planned.flags.map((f) => f.id).join(",")})`);

if (fails.length) {
  for (const f of fails) console.error(f);
  process.exit(1);
}
console.log("economy-crime-selftest: OK");
