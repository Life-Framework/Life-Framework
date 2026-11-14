// Pins the three-boot plan of `tools\cli wt e2e <slug> --content-ids` (#965): the save boot is
// not a load boot, boot 2 has the rename alias only and ends by the host stop (so its capture is
// in the shutdown save), boot 3 adds the restore; the log filter keeps the stable-id lines only.
// red-proof: with boot 2's hostStop false this printed "boot 2 must end by the host stop" and exited 1.
import { selftestContentIds } from "../test/e2e-content-ids.mjs";

const failures = selftestContentIds();
if (failures.length) {
  console.log("e2e-content-ids-selftest: FAIL");
  for (const failure of failures) console.log(`  ${failure}`);
  process.exit(1);
}
console.log("e2e-content-ids-selftest: OK (save, rename + hold, restore)");
