// Proves every export order kind on the dock board pays inside the 0.3.0 plan's $650 to $800/h band (#4844): the shipped
// `tools\cli economy --json` report prints each kind of the pack's order book in the band, with no `export-band` flag and
// no unpriced kind, and every kind is priced from the market's own wholesale (the template leaves `m_iWholesale` unset),
// so a market price change moves the order with it instead of leaving a hand-typed per-unit price behind.
// inputs: tools/commands/economy.mjs, tools/lib/economy.mjs, tools/lib/economy-export.mjs, tools/lib/economy-mining.mjs, tools/lib/economy-lumber.mjs, tools/economy/profit-loops.mjs, tools/economy/mining-assumptions.json, addons/LifeFramework/Configs/Market/LF_Market.conf, addons/LF_Everon1989/Configs/Market/LF_Market_Everon1989.conf, addons/LF_Everon1989/Configs/Market/LF_BuyerDemand_Everon1989.conf, addons/LF_Everon1989/Configs/Jobs/LF_ExportOrders_Everon1989.conf, addons/LifeFramework/Configs/Resources/ResourceChains.conf, addons/LifeFramework/Configs/Lumber/LF_TreeSpecies.conf, server/flagship/config.json, tools/validation/economy-export-band-selftest.mjs
// red-proof: on the #4410 rows (iron typed at $40 a bar, coal $7, planks $6, no order bonus) this printed "FAIL
// iron_bars_steamer $1365/h is above the band", "FAIL coal_freighter $572/h is under the band", "FAIL planks_freighter
// $489/h is under the band", "FAIL export-band flag ..." and "FAIL ... is priced from a typed wholesale"; on the market-priced
// rows with their order bonuses it prints OK.
import { spawnSync } from "node:child_process";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { EXPORT_BAND } from "../lib/economy-export.mjs";

const root = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
const fails = [];
const run = spawnSync(process.execPath, [join(root, "tools", "cli.mjs"), "economy", "--json"], { cwd: root, encoding: "utf8", windowsHide: true, maxBuffer: 64 * 1024 * 1024 });
let rep = null;
try {
  rep = JSON.parse(run.stdout);
} catch {
  fails.push(`FAIL tools\\cli economy --json did not print a report (exit ${run.status}): ${String(run.stderr).slice(0, 400)}`);
}

if (rep) {
  const ex = rep.exportOrders;
  if (!ex || !Array.isArray(ex.orders) || ex.orders.length === 0) fails.push("FAIL the report has no export orders");
  for (const p of ex?.problems ?? []) fails.push(`FAIL ${p}`);
  for (const o of ex?.orders ?? []) {
    if (o.steady > EXPORT_BAND.max) fails.push(`FAIL ${o.id} $${o.steady}/h is above the band ($${EXPORT_BAND.max})`);
    if (o.steady < EXPORT_BAND.min) fails.push(`FAIL ${o.id} $${o.steady}/h is under the band ($${EXPORT_BAND.min})`);
    if (o.wholesaleFrom !== "market") fails.push(`FAIL ${o.id} is priced from a typed wholesale ($${o.wholesale}), not the market row`);
  }
  for (const f of rep.flags ?? []) if (f.kind === "export-band") fails.push(`FAIL export-band flag: ${f.text}`);
}

if (fails.length) {
  for (const f of fails) console.error(f);
  process.exit(1);
}
console.log("economy-export-band-selftest: OK");
