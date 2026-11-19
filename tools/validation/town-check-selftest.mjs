// tools/validation/town-check-selftest.mjs
// inputs: tools/town/**, tools/perf/building-count.mjs
//
// Runs tools/town/check.selftest.mjs (card #2937) so `tools\cli validate` covers it: `town check` fails a fitted slot whose kind has a
// marker and no working component (a shop counter with no LF_ShopComponent, a jail with no cell).
// It also holds the optional lumber and farm slots (#3174, #3190, #3272): the Saint-Philippe mapping maps a woodlot, a farm and a merchant-timber
// slot and the dock mapping a wholesale slot, nobody has claimed them, and `town check` lists each as "not fitted yet" (an unclaimed mapped slot),
// never as an unknown slot the template lacks. #3315 adds the dock's lockers, smokehouse, warehouse, quay-fishing, fish-counter and chandler slots and
// the lead's approval of that mapping (`status: approved`).
// #3314 adds the M6-M8 outpost slots (bins, loot, buyer-spots: outposts; tow-yard; memorial) to the Saint-Philippe mapping; #3346 adds the wave-13 slots
// (safe-zone, atms, payphones, street-life, parked-cars, fuel-run, fuel-drops, pond, orchard, night-lights, square-evening, cashvan, org) and the dock's fishing-quay.
// #3350 fits the safe-zone slot, so it leaves the unfitted list; #3532 fits parked-cars, so it leaves too; #3377 fits fuel-drops; #3398 fits cashvan; #3416 fits buyer-spots.
// red-proof: see tools/town/check.selftest.mjs; before the woodlot slot existed this printed "FAIL  saint-philippe: woodlot is not among the unfitted slots"
// before the farm slot existed "FAIL  saint-philippe: farm is not among the unfitted slots", and before #3272 "FAIL  saint-philippe-dock: wholesale is not among the unfitted slots", before #3315 "FAIL  saint-philippe-dock: lockers is not among the unfitted slots" and "FAIL  saint-philippe-dock: the mapping status is not approved" (2026-11-18).
// before #3532 left the list, the next line read the parked-cars slot as unclaimed (2026-11-19).
// before #3314 "FAIL  saint-philippe: bins is not among the unfitted slots" and one such line for each of loot, buyer-spots, tow-yard and memorial (2026-11-18).
import { spawnSync } from "node:child_process";
import { readFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");
const r = spawnSync(process.execPath, [join(ROOT, "tools", "town", "check.selftest.mjs")], { cwd: ROOT, encoding: "utf8" });
process.stdout.write(r.stdout ?? "");
process.stderr.write(r.stderr ?? "");

let failed = r.status ?? 1;
const check = (cond, msg) => {
  if (!cond) {
    console.log(`FAIL  ${msg}`);
    failed++;
  }
};
{
  const { loadPlace, checkPlace } = await import(pathToFileURL(join(ROOT, "tools", "town", "check.mjs")).href);
  // a mapped slot the template lacks is an error from checkMapping; an unclaimed one is only listed as not fitted yet
  for (const [place, slots] of [["saint-philippe", ["woodlot", "farm", "bins", "loot", "tow-yard", "memorial", "atms", "payphones", "street-life", "fuel-run", "pond", "orchard", "night-lights", "square-evening", "org"]], ["saint-philippe-dock", ["lockers", "smokehouse", "quay-fishing", "fish-counter", "chandler", "fishing-quay"]]]) {
    const result = checkPlace(loadPlace(ROOT, place), {});
    for (const slot of slots) {
      check(result.unfitted.includes(slot), `${place}: ${slot} is not among the unfitted slots`);
      check(result.notes.some((n) => n.includes("not fitted yet") && n.split(/[\s,:()]+/).includes(slot)), `${place}: town check does not name the ${slot} slot as not fitted yet`);
      check(![...result.errors, ...result.warnings].some((m) => m.includes(`slot ${slot} `) || m.includes(`slot ${slot}:`)), `${place}: ${slot} is reported as an error or warning`);
    }
  }
}
// the org slot's building carries the purpose row 'org registry' (#3346), so `town doors --planned` keeps seeing every building with a purpose
{
  const rows = JSON.parse(readFileSync(join(ROOT, "docs", "everon", "places", "saint-philippe", "purpose.json"), "utf8")).buildings;
  const org = JSON.parse(readFileSync(join(ROOT, "docs", "everon", "places", "saint-philippe", "mapping.json"), "utf8")).slots.org;
  const row = rows.find((r) => org && r.id === `sp:${org.building}`);
  check(!!row && row.purpose === "b" && /org registry/.test(row.use ?? ""), "saint-philippe: the org slot's building has no purpose row 'org registry'");
}
// the lead approves the dock mapping on merge (#3315): a `proposed` file is a draft nobody has accepted
check(JSON.parse(readFileSync(join(ROOT, "docs", "everon", "places", "saint-philippe-dock", "mapping.json"), "utf8")).status === "approved", "saint-philippe-dock: the mapping status is not approved");
process.exit(failed ? 1 : 0);
