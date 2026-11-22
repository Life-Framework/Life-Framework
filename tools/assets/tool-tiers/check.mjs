// Acceptance check for #4188: nine tier prefabs (3 tools x worn, standard, fine), each inheriting
// its tool's prefab, each with its own material, no two finishes alike, every resource with a
// meta and every GUID unique. Run `node tools/assets/tool-tiers/check.mjs`; exit 1 on a failure.
// red-proof: before build.mjs has run the nine prefabs are missing and this exits 1.
import { existsSync, readFileSync } from "node:fs";
import { join } from "node:path";
import { OUT_DIR, TIERS, TOOLS, name } from "./spec.mjs";

const root = process.argv[2] || process.cwd();
const fail = [];
const guids = new Map();
const finishes = new Map();

function noteGuid(file, text) {
  const m = /Name "\{([0-9A-F]{16})\}/.exec(text);
  if (!m) return fail.push(`${file}: no resource GUID in meta`);
  if (guids.has(m[1])) fail.push(`${file}: GUID ${m[1]} repeats ${guids.get(m[1])}`);
  guids.set(m[1], file);
}

for (const tool of TOOLS) {
  for (const tier of TIERS) {
    const base = join(root, OUT_DIR, name(tool, tier));
    for (const ext of [".et", ".et.meta", ".emat", ".emat.meta"]) {
      if (!existsSync(base + ext)) fail.push(`${name(tool, tier)}${ext}: missing`);
    }
    if (!existsSync(base + ".et") || !existsSync(base + ".emat")) continue;
    const et = readFileSync(base + ".et", "utf8");
    const emat = readFileSync(base + ".emat", "utf8");
    if (!et.includes(`: "${tool.parent}"`)) fail.push(`${name(tool, tier)}.et: does not inherit ${tool.parent}`);
    if (!et.includes(`SourceMaterial "${tool.source}"`)) fail.push(`${name(tool, tier)}.et: no override of material "${tool.source}"`);
    if (!emat.includes(tool.bcr)) fail.push(`${name(tool, tier)}.emat: lost the tool's own colour map`);
    const guid = /AssignedMaterial "\{([0-9A-F]{16})\}/.exec(et);
    const meta = readFileSync(base + ".emat.meta", "utf8");
    noteGuid(name(tool, tier) + ".emat.meta", meta);
    noteGuid(name(tool, tier) + ".et.meta", readFileSync(base + ".et.meta", "utf8"));
    if (!guid || !meta.includes(`{${guid[1]}}`)) fail.push(`${name(tool, tier)}.et: assigns a material GUID that is not its own emat`);
    const key = `${tool.key}:${emat}`;
    if (finishes.has(key)) fail.push(`${name(tool, tier)}.emat: same finish as ${finishes.get(key)}`);
    finishes.set(key, name(tool, tier));
  }
}

if (fail.length) {
  for (const f of fail) console.log(`FAIL  ${f}`);
  console.log(`SUMMARY tool-tiers check fail ${fail.length} problem(s)`);
  process.exit(1);
}
console.log(`SUMMARY tool-tiers check ok ${TOOLS.length * TIERS.length} prefabs`);
