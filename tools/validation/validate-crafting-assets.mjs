// tools/validation/validate-crafting-assets.mjs
// inputs: addons/**
//
// The crafting foundation README names the ported stations and materials and lists
// their prefab paths. Every listed path must exist with a .meta next to it, and the
// README must still name each station. This used to be an in-engine LOGIC test that
// read repo files through a cwd-dependent $profile path; a repo-file check belongs here.
// The engine-level check (each prefab loads by {GUID}path) stays in
// LF_Test_CraftingAssets (crafting/assets, WORLD tier).

import { existsSync, readFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const root = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
const addon = join(root, "addons", "LifeFramework");
const readmePath = join(addon, "Prefabs", "Crafting", "README.md");

const names = ["lathe", "sewing machine", "boiler", "fireplace", "vice", "charcoal", "sulfur", "gunpowder", "linen"];

const errors = [];
if (!existsSync(readmePath)) {
  console.log(`ERROR crafting README missing: ${readmePath}`);
  console.log("validate-crafting-assets: FAILED (1 error(s))");
  process.exit(1);
}

const readme = readFileSync(readmePath, "utf8");
for (const name of names) {
  if (!readme.includes(name)) errors.push(`crafting README no longer names "${name}"`);
}

const paths = [];
for (const raw of readme.split(/\r?\n/)) {
  const line = raw.trim();
  const start = line.indexOf("Prefabs/Crafting/");
  if (start < 0) continue;
  const rest = line.slice(start);
  const et = rest.indexOf(".et");
  if (et < 0) continue;
  paths.push(rest.slice(0, et + 3));
}

if (paths.length === 0) errors.push("crafting README lists no prefab paths");

for (const rel of paths) {
  const file = join(addon, ...rel.split("/"));
  if (!existsSync(file)) errors.push(`crafting README lists a missing prefab: ${rel}`);
  else if (!existsSync(`${file}.meta`)) errors.push(`crafting prefab has no .meta: ${rel}`);
}

for (const error of errors) console.log(`ERROR ${error}`);
if (errors.length) {
  console.log(`validate-crafting-assets: FAILED (${errors.length} error(s), ${paths.length} prefab path(s) checked)`);
  process.exit(1);
}
console.log(`validate-crafting-assets: OK (${paths.length} prefab path(s), ${names.length} name(s) checked)`);
