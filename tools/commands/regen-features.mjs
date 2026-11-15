// tools\cli regen-features: rewrite the docs/features.md index from docs/features/*.md.
import { writeFileSync } from "node:fs";
import { join } from "node:path";
import { renderIndex } from "../lib/features-index.mjs";

export const help = "regen-features   rewrite the generated docs/features.md index from docs/features/<area>.md";

export async function main(args, { root } = {}) {
  const r = root || process.cwd();
  writeFileSync(join(r, "docs", "features.md"), renderIndex(r));
  console.log("regen-features: docs/features.md written");
  return 0;
}
