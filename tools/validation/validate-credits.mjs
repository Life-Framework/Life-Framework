// validate-credits: the in-game credits list matches docs/credits.md (tools/credits/gen.mjs).
// red-proof: observed 2026-11-13: with a row added to docs/credits.md and no regeneration it printed
// "LF_CreditsRows.generated.c is out of date with docs/credits.md"; regenerated, OK.
import { problems } from "../credits/gen.mjs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const bad = problems(join(dirname(fileURLToPath(import.meta.url)), "..", ".."));
if (bad.length) {
  console.log("validate-credits: FAIL");
  for (const b of bad) console.log(`  ${b}`);
  process.exit(1);
}
console.log("validate-credits: OK (the in-game credits match docs/credits.md)");
