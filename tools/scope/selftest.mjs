// Proves tools\cli scope against a temp directory. Does not boot the game.
import { selftestScope } from "./scaffold.mjs";

const failures = selftestScope();
if (failures.length) {
  console.log("scope: FAIL");
  for (const failure of failures) console.log(`  ${failure}`);
  process.exit(1);
}
console.log("scope: OK");
