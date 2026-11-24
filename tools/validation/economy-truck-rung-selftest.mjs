// Proves the truck is the climb (#4826, WK-W36 of the 0.3.0 plan): the shipped `tools\cli economy --json` report puts
// both truck loops (stone by truck, hardwood by truck) on the high-tier civilian rung, at least $600/h steady, above
// every duty rank with its full bonus cap, and prints an export-order line of its own at $650/h or more.
// inputs: tools/commands/economy.mjs, tools/lib/economy-duty.mjs, tools/lib/economy-perks.mjs, tools/lib/economy-wheat.mjs, tools/economy/profit-loops.mjs, addons/LF_Everon1989/Configs/Police/LF_Police_Everon1989.conf, addons/LF_Everon1989/Configs/Medic/LF_Medic_Everon1989.conf, addons/LifeFramework/Scripts/Game/Feature/Police/LF_DutyBonus.c, server/flagship/config.json, addons/LifeFramework/Configs/Lumber/LF_TreeSpecies.conf, addons/LifeFramework/Configs/Resources/ResourceChains.conf, addons/LifeFramework/Scripts/Game/Feature/Jobs/LF_ExportOrderConfig.c, tools/lib/economy.mjs, tools/lib/economy-export.mjs, tools/lib/economy-mining.mjs, tools/lib/economy-lumber.mjs, tools/economy/mining-assumptions.json, addons/LF_Everon1989/Configs/Market/LF_Market_Everon1989.conf, addons/LF_Everon1989/Configs/Market/LF_BuyerDemand_Everon1989.conf, addons/LF_Everon1989/Configs/Jobs/LF_ExportOrders_Everon1989.conf, tools/validation/economy-truck-rung-selftest.mjs
// red-proof: on the 0.2.1 rows (cement premium $10, board premium $12 with a 24 purse refilling 8 an hour) and before
// economy-export.mjs this printed "FAIL stone by truck $460/h", "FAIL hardwood by truck $365/h", "FAIL the export-order
// line is missing" and "FAIL police lieutenant most $520/h is above ..."; with the lifted rows it prints OK.
import { spawnSync } from "node:child_process";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const root = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
const TRUCK_MIN = 600, EXPORT_MIN = 650;
const fails = [];
const run = spawnSync(process.execPath, [join(root, "tools", "cli.mjs"), "economy", "--json"], { cwd: root, encoding: "utf8", windowsHide: true, maxBuffer: 64 * 1024 * 1024 });
let rep = null;
try {
  rep = JSON.parse(run.stdout);
} catch {
  fails.push(`FAIL tools\\cli economy --json did not print a report (exit ${run.status}): ${String(run.stderr).slice(0, 400)}`);
}

if (rep) {
  const truck = [
    { label: "stone by truck", loop: (rep.mining?.loops ?? []).find((l) => l.id === "stone-truck") },
    { label: "hardwood by truck", loop: (rep.lumber?.loops ?? []).find((l) => l.id === "truck-hardwood") },
  ];
  for (const t of truck) {
    if (!t.loop) fails.push(`FAIL ${t.label} is missing from the report`);
    else if (!(t.loop.steady >= TRUCK_MIN)) fails.push(`FAIL ${t.label} $${t.loop.steady}/h steady is under the $${TRUCK_MIN}/h high-tier rung`);
  }
  const lowestTruck = Math.min(...truck.map((t) => t.loop?.steady ?? 0));
  for (const r of rep.dutyLadder ?? []) {
    if (r.most > lowestTruck) fails.push(`FAIL ${r.job} ${r.id} most $${r.most}/h is above a truck loop ($${lowestTruck}/h)`);
  }
  if (!(rep.dutyLadder ?? []).length) fails.push("FAIL the report has no duty ranks to compare");
  const ex = rep.exportOrders;
  if (!ex || !Array.isArray(ex.orders) || ex.orders.length === 0) fails.push("FAIL the export-order line is missing from the report");
  else if (!(ex.steady >= EXPORT_MIN)) fails.push(`FAIL export orders $${ex.steady}/h steady is under $${EXPORT_MIN}/h`);
}

if (fails.length) {
  for (const f of fails) console.error(f);
  process.exit(1);
}
console.log("economy-truck-rung-selftest: OK");
