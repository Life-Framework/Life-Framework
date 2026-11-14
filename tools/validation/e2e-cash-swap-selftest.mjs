// Pins the two-boot plan of `tools\cli wt e2e <slug> --cash-swap` (#1130): the save boot is not a
// load boot and ends by the host stop, boot 2 loads that save; the log filter keeps the slot lines.
// red-proof: with boot 1 flagged LF_E2E_LOAD_BOOT this printed "the save boot must not be a load boot" and exited 1.
import { selftestCashSwap } from "../test/e2e-cash-swap.mjs";

const failures = selftestCashSwap();
if (failures.length) {
  console.log("e2e-cash-swap-selftest: FAIL");
  for (const failure of failures) console.log(`  ${failure}`);
  process.exit(1);
}
console.log("e2e-cash-swap-selftest: OK (save, swap + reload)");
