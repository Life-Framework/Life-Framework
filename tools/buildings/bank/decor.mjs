// The bank's dressing: counters, the slip desk, queue rope, clock, notices, the manager's office and the lights,
// written into the furniture prefab after shell.mjs has copied the vanilla props.
//
//   node tools/buildings/bank/decor.mjs          rewrite the decor block in Furniture/Bank_furniture_01.et
//   node tools/buildings/bank/decor.mjs --check  exit 1 when the prefab is not what this script writes
//
// Positions live in layout.mjs. Entity IDs start with BA4C, so a re-run removes its own old block.
import { readFileSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { decor } from "./layout.mjs";
import { renderBlock, swapBlock } from "./decor-lib.mjs";

const ROOT = join(dirname(fileURLToPath(import.meta.url)), "..", "..", "..");
const FURNITURE = join(ROOT, "addons", "LF_Everon1989", "Prefabs", "Buildings", "Bank", "Furniture", "Bank_furniture_01.et");
const PREFIX = "BA4C";

const text = readFileSync(FURNITURE, "utf8");
const next = swapBlock(text, PREFIX, await renderBlock(ROOT, PREFIX, decor));
if (process.argv.includes("--check")) {
  const same = next === text;
  console.log(same ? "decor: the furniture prefab is current" : "decor: the furniture prefab differs; run node tools/buildings/bank/decor.mjs");
  process.exit(same ? 0 : 1);
}
writeFileSync(FURNITURE, next);
console.log(`decor: ${decor.items.length} prop(s), ${decor.lights.length} light(s) written to Furniture/Bank_furniture_01.et`);
