// Pins `tools\cli host` and `tools\cli playtest`: the playtest launch runs
// inputs: tools/**, agent-box/**, .githooks/**
// LF_PlaytestTown.conf with no test define and no -config, passes
// -loadSessionSave only when a save exists, and backs the save folder up into a
// timestamped copy. The player client is a real player (no harness flags, no
// -scrDefine, its own profile).
// red-proof: with hostArgs pushing -loadSessionSave unconditionally this
// printed "a boot with no save passes -loadSessionSave" and exited 1.
// red-proof: with playerClientArgs adding -noBackend this printed
// "player client passes -noBackend" and exited 1.
import { selftestHost } from "../host/host.mjs";
import { selftestPlaytest } from "../host/playtest.mjs";

const failures = [...selftestHost(), ...selftestPlaytest()];
if (failures.length) {
  console.log("host-selftest: FAIL");
  for (const failure of failures) console.log(`  ${failure}`);
  process.exit(1);
}
console.log("host-selftest: OK (playtest launch flags, save load, backup, player client)");
