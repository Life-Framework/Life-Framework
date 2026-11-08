// Pins `tools\cli host`: the playtest launch runs LF_PlaytestTown.conf with no
// test define and no -config, passes -loadSessionSave only when a save exists,
// and backs the save folder up into a timestamped copy.
// red-proof: with hostArgs pushing -loadSessionSave unconditionally this
// printed "a boot with no save passes -loadSessionSave" and exited 1.
import { selftestHost } from "../host/host.mjs";

const failures = selftestHost();
if (failures.length) {
  console.log("host-selftest: FAIL");
  for (const failure of failures) console.log(`  ${failure}`);
  process.exit(1);
}
console.log("host-selftest: OK (playtest launch flags, save load, backup)");
