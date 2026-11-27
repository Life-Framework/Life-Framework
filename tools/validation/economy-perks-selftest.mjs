// Proves the full-perks line of `tools\cli economy` (WK-W11_2, #4409; docs/roadmap/next-release-0.3.0.md "The economy
// table"): every chain prints its hourly with the passive perks next to the novice line, and a loop is flagged when its
// full-perks hourly drops under the floor of the rung it sits on or passes the crime band of that rung. A planted loop
// that drops, one that passes crime and a sound one prove the three outcomes; the shipped configs flag nothing new
// (stone on foot already has its own flag and keeps it out of the perks flags).
// inputs: tools/lib/economy-perks.mjs, tools/lib/economy.mjs, tools/commands/economy.mjs, tools/validation/economy-perks-selftest.mjs, addons/LifeFramework/Scripts/Game/Feature/Level/LF_PerkEffects.c, addons/LifeFramework/Scripts/Game/Feature/Level/LF_PerkPrice.c
// red-proof: written first, before tools/lib/economy-perks.mjs existed: the import failed and it printed no "OK" line;
// with a stub that returned no lines it printed "FAIL the full-perks line is printed per chain", "FAIL a loop that drops
// under its rung is flagged" and "FAIL a loop that passes crime is flagged". With the lib it prints
// "economy-perks-selftest: OK".
import { readFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { buildReport } from "../lib/economy.mjs";
import * as perks from "../lib/economy-perks.mjs";
import * as cmd from "../commands/economy.mjs";

const root = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
const fails = [];
const expect = (ok, msg) => { if (!ok) fails.push(`FAIL ${msg}`); };

// 1. The shares are the game's own numbers, read from the Enforce Script constants, so the line cannot drift.
const effects = readFileSync(join(root, "addons/LifeFramework/Scripts/Game/Feature/Level/LF_PerkEffects.c"), "utf8");
const price = readFileSync(join(root, "addons/LifeFramework/Scripts/Game/Feature/Level/LF_PerkPrice.c"), "utf8");
const yieldShare = Number(/DOUBLE_YIELD_CHANCE\s*=\s*([\d.]+)/.exec(effects)?.[1]);
const pricePct = Number(/GOOD_WORD_PERCENT\s*=\s*(\d+)/.exec(price)?.[1]);
expect(perks.PERKS.yieldShare === yieldShare, `the yield share is LF_PerkEffects.DOUBLE_YIELD_CHANCE ${yieldShare} (got ${perks.PERKS.yieldShare})`);
expect(perks.PERKS.pricePercent === pricePct, `the price share is LF_PerkPrice.GOOD_WORD_PERCENT ${pricePct} (got ${perks.PERKS.pricePercent})`);
expect(pricePct > 0 && pricePct <= 15, `the price share is inside the +15% cap (got ${pricePct})`);

// 2. The arithmetic: gross and cost are separate, so a mining chain's reagent and fuel are not multiplied.
expect(perks.fullPerksHourly({ steady: 400 }) === Math.round(400 * (1 + yieldShare) * (1 + pricePct / 100)), "a loop with no cost multiplies its steady hourly");
expect(perks.fullPerksHourly({ steady: 300, grossSteady: 500, costPerHour: 200 }) === Math.round(500 * (1 + yieldShare) * (1 + pricePct / 100)) - 200, "a mining loop multiplies the gross and keeps its cost");

// 3. The rung of a loop and the crime band above it (the plan's tables).
expect(perks.rungOf(398).id === "entry" && perks.rungOf(515).id === "mid" && perks.rungOf(650).id === "high" && perks.rungOf(850).id === "master", "rungOf places $398 entry, $515 mid, $650 high and $850 master");

// 4. Planted loops: sound, dropping, passing crime.
const loop = (id, steady, extra = {}) => ({ id, label: id, steady, first: steady, flagged: false, ...extra });
const planted = {
  lumber: { loops: [loop("sound", 450)] },
  wheat: { loops: [loop("drops", 450, { fullOverride: 430 })] },
  mining: { loops: [loop("rich", 700, { fullOverride: 1600 })] },
};
const fx = perks.perksReport(planted);
expect(fx.rows.length === 3, `one full-perks row per chain (got ${fx.rows.length})`);
expect(fx.lines.length >= 4 && fx.lines[0].startsWith("full perks"), "the full-perks line is printed per chain under a heading");
expect(fx.lines.filter((l) => l.includes("novice $")).length === 3, "each chain line prints the novice hourly beside the full-perks hourly");
const kinds = fx.flags.map((f) => f.kind).sort().join(",");
expect(fx.flags.some((f) => f.kind === "perks-rung" && f.id === "perks:wheat:drops"), `a loop that drops under its rung is flagged (flags: ${kinds})`);
expect(fx.flags.some((f) => f.kind === "perks-crime" && f.id === "perks:mining:rich"), `a loop that passes crime is flagged (flags: ${kinds})`);
expect(!fx.flags.some((f) => f.id === "perks:lumber:sound"), "a sound loop is not flagged");

// 5. The shipped configs: each chain gets a line, perked is never under novice, nothing passes crime, and a loop the
// report already flags (stone on foot) gets its line but no second flag.
const rep = buildReport(root, {});
expect(!rep.error, `the report builds (${rep.error})`);
const real = perks.perksReport(rep);
const chains = ["lumber", "wheat", "mining"].reduce((n, k) => n + (rep[k]?.loops?.length ?? 0), 0);
expect(real.rows.length === chains && chains > 0, `a full-perks row for each of the ${chains} chains (got ${real.rows.length})`);
expect(real.rows.every((r) => r.full >= r.novice), "no chain pays less with the perks than without");
expect(real.flags.length === 0, `the shipped chains raise no perks flag (got ${real.flags.map((f) => f.text).join("; ")})`);
expect(typeof cmd.perksSection === "function" && cmd.perksSection(rep).rows.length === real.rows.length, "tools\\cli economy adds the perks section (perksSection)");

if (fails.length) {
  console.error(fails.join("\n"));
  console.error("economy-perks-selftest: FAILED");
  process.exit(1);
}
console.log("economy-perks-selftest: OK");
