// Pins the shutdown-save check of `tools\cli wt e2e` (#830): a pass 1 log with
// `session ended without a shutdown save`, with no host stop request, or with the
// feature statics reset before the SHUTDOWN save fails; a host-stop close that
// writes the SHUTDOWN save and then resets the statics passes.
// red-proof: with checkShutdownSave ignoring the reset order this printed
// "statics reset before the shutdown save passes" and exited 1.
import { selftestE2eShutdown } from "../test/e2e-shutdown.mjs";

const failures = selftestE2eShutdown();
if (failures.length) {
  console.log("e2e-shutdown-selftest: FAIL");
  for (const failure of failures) console.log(`  ${failure}`);
  process.exit(1);
}
console.log("e2e-shutdown-selftest: OK (shutdown save written, host stop, statics reset after the save)");
