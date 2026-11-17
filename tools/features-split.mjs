// One-off (kept for the record): split the old docs/features.md by "## " headings into
// docs/features/<slug>.md, then write the generated index. Run once: node tools/features-split.mjs
import { mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { renderIndex, splitSections, areaPath } from "./lib/features-index.mjs";

const root = join(dirname(fileURLToPath(import.meta.url)), "..");
const sections = splitSections(readFileSync(join(root, "docs", "features.md"), "utf8"));
mkdirSync(join(root, "docs", "features"), { recursive: true });
const seen = new Set();
for (const s of sections) {
  if (seen.has(s.slug)) throw new Error(`duplicate slug ${s.slug}`);
  seen.add(s.slug);
  writeFileSync(join(root, areaPath(s.slug)), s.body);
}
writeFileSync(join(root, "docs", "features.md"), renderIndex(root));
console.log(`split into ${seen.size} files: ${[...seen].join(", ")}`);
