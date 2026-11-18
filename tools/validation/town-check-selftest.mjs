// tools/validation/town-check-selftest.mjs
// inputs: tools/town/**, tools/perf/building-count.mjs
//
// Runs tools/town/check.selftest.mjs (card #2937) so `tools\cli validate` covers it: `town check` fails a fitted slot whose kind has a
// marker and no working component (a shop counter with no LF_ShopComponent, a jail with no cell).
// It also holds the optional lumber and farm slots (#3174, #3190): the approved Saint-Philippe mapping maps a woodlot and a farm slot nobody has
// claimed, and `town check` lists each as "not fitted yet" (an unclaimed mapped slot), never as an unknown slot the template lacks.
// red-proof: see tools/town/check.selftest.mjs; before the woodlot slot existed this printed "FAIL  saint-philippe: woodlot is not among the unfitted slots"
// and before the farm slot existed "FAIL  saint-philippe: farm is not among the unfitted slots" (2026-11-18).
import { spawnSync } from "node:child_process";
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
  const result = checkPlace(loadPlace(ROOT, "saint-philippe"), {});
  // a mapped slot the template lacks is an error from checkMapping; an unclaimed one is only listed as not fitted yet
  for (const slot of ["woodlot", "farm"]) {
    check(result.unfitted.includes(slot), `saint-philippe: ${slot} is not among the unfitted slots`);
    check(result.notes.some((n) => n.includes("not fitted yet") && n.split(/[\s,:()]+/).includes(slot)), `saint-philippe: town check does not name the ${slot} slot as not fitted yet`);
    check(![...result.errors, ...result.warnings].some((m) => m.includes(`slot ${slot} `) || m.includes(`slot ${slot}:`)), `saint-philippe: ${slot} is reported as an error or warning`);
  }
}
process.exit(failed ? 1 : 0);
