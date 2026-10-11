// Proves suite-valid on fixture names. Does not boot the game.
import { selftestSuiteStamp } from "../suite-stamp.mjs";

const failures = selftestSuiteStamp();
if (failures.length) {
  console.log("suite-valid-selftest: FAIL");
  for (const failure of failures) console.log(`  ${failure}`);
  process.exit(1);
}
console.log("suite-valid-selftest: OK (safe paths exit 0; gameplay script, persistence serializer, prefab, and world layer exit 1)");
