// tools\cli fixtures: regenerates tools/buildings/pipeline/fixtures.json (shelf boards, goods footprints) from
// the models, so no shelf height is typed by hand (#1143). --check compares the committed file to a fresh
// measure and exits 1 when they differ.
import { readFileSync, writeFileSync } from "node:fs";
import { join } from "node:path";

export const help = "fixtures [--check]   regenerate the store fixtures' boards and goods' footprints from the models (tools/buildings/pipeline/fixtures.json)";

export async function main(args, { root } = {}) {
  const { measureAll, diffFixtures } = await import("../buildings/pipeline/fixtures.mjs");
  const file = join(root, "tools", "buildings", "pipeline", "fixtures.json");
  let fresh;
  try {
    fresh = await measureAll(root);
  } catch (e) {
    console.log(`FAIL  fixtures: ${e.message}`);
    return 1;
  }
  const have = JSON.parse(readFileSync(file, "utf8"));
  const diff = diffFixtures(have, fresh);
  if (args.includes("--check")) {
    for (const d of diff) console.log(`FAIL  ${d}`);
    console.log(`SUMMARY fixtures ${diff.length ? "fail" : "ok"} ${Object.keys(fresh.fixtures).length} fixture(s), ${Object.keys(fresh.items).length} item(s), ${diff.length} difference(s)`);
    return diff.length ? 1 : 0;
  }
  // Short number arrays stay on one line.
  const text = JSON.stringify(fresh, null, 1).replace(/\[\s+(-?[\d.]+(?:,\s+-?[\d.]+)*)\s+\]/g, (_, nums) => `[${nums.replace(/,\s+/g, ", ")}]`);
  writeFileSync(file, `${text}\n`);
  for (const [n, f] of Object.entries(fresh.fixtures)) console.log(`        ${n}: boards at y ${f.boards.map((b) => b.y).join(", ")}`);
  console.log(`SUMMARY fixtures ok ${Object.keys(fresh.fixtures).length} fixture(s), ${Object.keys(fresh.items).length} item(s) written, ${diff.length} changed`);
  return 0;
}
