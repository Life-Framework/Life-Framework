// Pins the shutdown-save check of `tools\cli wt e2e` (#830): a pass 1 log with
// inputs: tools/**, agent-box/**, .githooks/**
// `session ended without a shutdown save`, with no host stop request, or with the
// feature statics reset before the SHUTDOWN save fails; a host-stop close that
// writes the SHUTDOWN save and then resets the statics passes.
// #3630: a leg that holds LF_Test_CargoBedPersist fails when its pass 2 log has no `e2e restart: e2e-cargo-bed-truck`
// line (missingRestartProofs; red observed 2026-11-19 with the truck seed disabled: `tools\cli e2e` printed
// `FAILED in pass 2 (restart proof): cargo bed truck keeps its logs (#3266) never reached its check`, exit 1).
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
