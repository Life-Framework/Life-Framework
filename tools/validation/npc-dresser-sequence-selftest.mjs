// tools/validation/npc-dresser-sequence-selftest.mjs
// inputs: tools/validation/**, addons/LifeFramework/Scripts/WorkbenchGame/**
//
// Proves validate-npc-dresser-sequence passes the real plugin and fails the shape that gave NPCs a second head:
// slot writes with no edit sequence around them.
// red-proof: the "pre-fix" case below is the plugin before the fix (DressOne wrote slots directly); with the
// check reading the real plugin at the parent commit it printed FAIL "DressOne ... writes a variable with no
// edit sequence open" and exited 1 (2026-11-16).

import { readFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { checkSequence, PLUGIN } from "./validate-npc-dresser-sequence.mjs";

const root = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
const real = readFileSync(join(root, PLUGIN), "utf8");

const wrap = (inner) => `class P\n{\n\tprotected int DressOne(WorldEditorAPI api, IEntitySource src)\n\t{\n${inner}\n\t}\n\n\tprotected void Apply(WorldEditorAPI api, IEntitySource src)\n\t{\n\t\tDressOne(api, src);\n\t}\n}\n`;
const write = '\t\tapi.SetVariableValue(src, path, "Prefab", prefab);';
const cases = [
  ["real plugin is clean", real, 0],
  ["pre-fix: writes, no sequence", wrap(write), 1],
  ["Begin without End", wrap("\t\tapi.BeginEditSequence(src);\n" + write), 1],
  ["write before Begin", wrap(write + "\n\t\tapi.BeginEditSequence(src);\n\t\tapi.EndEditSequence(src);"), 1],
  ["sequence around the writes", wrap("\t\tapi.BeginEditSequence(src);\n" + write + "\n\t\tapi.EndEditSequence(src);"), 0],
];
let failed = 0;
for (const [name, text, want] of cases) {
  const got = checkSequence(text).length > 0 ? 1 : 0;
  if (got !== want) {
    console.log(`FAIL ${name}: expected ${want ? "a violation" : "clean"}`);
    failed++;
  }
}
if (failed) {
  console.log(`npc-dresser-sequence-selftest: ${failed} case(s) failed`);
  process.exit(1);
}
console.log(`npc-dresser-sequence-selftest: OK (${cases.length} cases)`);
