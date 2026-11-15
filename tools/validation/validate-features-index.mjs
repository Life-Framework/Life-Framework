// tools/validation/validate-features-index.mjs - docs/features.md is a generated index (AGENTS.md rules 8, 10).
// inputs: docs/features.md, docs/features/**, tools/lib/features-index.mjs
//
// Rows live in docs/features/<area>.md; the index lists those files and nothing else, so it
// changes only when an area file is added, removed or retitled. A hand edit (a row added to the
// index) fails here. Fix: tools\cli regen-features.
//
// red-proof: tools/validation/features-index-selftest.mjs adds a row to a fixture index and
// expects this check to fail.
import { existsSync, readFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { renderIndex } from "../lib/features-index.mjs";

export function checkIndex(root) {
  const file = join(root, "docs", "features.md");
  if (!existsSync(file)) return ["docs/features.md is missing; run tools\\cli regen-features"];
  if (readFileSync(file, "utf8") !== renderIndex(root)) {
    return ["docs/features.md differs from the generated index (rows belong in docs/features/<area>.md, never in the index); run tools\\cli regen-features"];
  }
  return [];
}

if (process.argv[1] && fileURLToPath(import.meta.url) === process.argv[1]) {
  const root = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
  const errors = checkIndex(root);
  if (errors.length) {
    for (const e of errors) console.log(`FAIL ${e}`);
    process.exit(1);
  }
  console.log("validate-features-index: OK");
}
