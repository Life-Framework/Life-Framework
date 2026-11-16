// tools/validation/validate-npc-dresser-sequence.mjs
// inputs: addons/LifeFramework/Scripts/WorkbenchGame/LFTools/LF_NpcDresserPlugin.c, tools/validation/**
//
// The NPC Dresser writes several loadout slots per NPC. Each WorldEditorAPI SetVariableValue or
// ClearVariableValue re-initialises the entity unless an edit sequence is open, and a character's identity
// head spawns again on every re-init without the old one going: the NPC grew a second head after a dress.
// So every such write must sit in a method that opens api.BeginEditSequence(src) before and closes
// api.EndEditSequence(src) after it, or in a method only ever called from inside one.
// red-proof: tools/validation/npc-dresser-sequence-selftest.mjs feeds this check the pre-fix plugin shape
// (writes with no sequence) and asserts it fails.

import { existsSync, readFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

export const PLUGIN = "addons/LifeFramework/Scripts/WorkbenchGame/LFTools/LF_NpcDresserPlugin.c";
const WRITE = /\bapi\.(SetVariableValue|ClearVariableValue)\s*\(/;

// Methods of the plugin: { name, start, end (1-based inclusive lines), body }. A method starts at a line
// `protected|override ... Name(` at one tab and ends at the closing `\t}`.
export function methods(text) {
  const lines = text.split("\n");
  const out = [];
  let cur = null;
  lines.forEach((line, i) => {
    const m = line.match(/^\t(?:protected |override |static )*[\w<>, ]+?\s+(\w+)\s*\(/);
    if (!cur && m && !line.trim().endsWith(";")) cur = { name: m[1], start: i + 1, lines: [] };
    if (cur) cur.lines.push(line);
    if (cur && line === "\t}") {
      out.push({ ...cur, end: i + 1, body: cur.lines.join("\n") });
      cur = null;
    }
  });
  return out;
}

export function checkSequence(text) {
  const errors = [];
  const ms = methods(text);
  for (const m of ms) {
    if (!WRITE.test(m.body)) continue;
    const begins = (m.body.match(/api\.BeginEditSequence\s*\(/g) || []).length;
    const ends = (m.body.match(/api\.EndEditSequence\s*\(/g) || []).length;
    if (begins || ends) {
      const first = m.body.search(WRITE);
      const b = m.body.search(/api\.BeginEditSequence\s*\(/);
      const e = m.body.lastIndexOf("api.EndEditSequence");
      if (begins !== 1 || ends !== 1 || b < 0 || b > first || e < first) errors.push(`${m.name} (line ${m.start}) opens or closes its edit sequence around the writes wrongly (${begins} Begin, ${ends} End)`);
      continue;
    }
    // No sequence here: every caller must hold one around the call.
    const call = new RegExp(String.raw`\b${m.name}\s*\(`);
    const held = (c) => {
      const at = c.body.search(call);
      return c.body.search(/api\.BeginEditSequence\s*\(/) >= 0 && c.body.search(/api\.BeginEditSequence\s*\(/) < at && c.body.lastIndexOf("api.EndEditSequence") > at;
    };
    const callers = ms.filter((o) => o !== m && call.test(o.body));
    if (!callers.length || !callers.every(held)) {
      errors.push(`${m.name} (line ${m.start}) writes a variable with no edit sequence open: wrap the NPC's writes in api.BeginEditSequence(src) / api.EndEditSequence(src)`);
    }
  }
  return errors;
}

if (process.argv[1] && fileURLToPath(import.meta.url) === process.argv[1]) {
  const root = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
  const file = join(root, PLUGIN);
  if (!existsSync(file)) {
    console.log("validate-npc-dresser-sequence: OK (no dresser plugin)");
    process.exit(0);
  }
  const errors = checkSequence(readFileSync(file, "utf8"));
  if (errors.length) {
    for (const e of errors) console.log(`FAIL ${PLUGIN}: ${e}`);
    console.log(`validate-npc-dresser-sequence: ${errors.length} violation(s)`);
    process.exit(1);
  }
  console.log("validate-npc-dresser-sequence: OK (every slot write is inside one edit sequence per NPC)");
}
