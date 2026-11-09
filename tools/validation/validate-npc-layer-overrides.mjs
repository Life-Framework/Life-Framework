// tools/validation/validate-npc-layer-overrides.mjs
//
// Npc_Base.et replicates with "Rpl State Override" Runtime (#150): the server
// streams each placed NPC to clients, and a client builds it from the prefab,
// not from the layer. A component a layer adds to an NPC therefore exists only
// on the server, and the client is kicked on join with
// `RplCreationError: Number of items mismatch` (observed 2026-11-08 with
// LF_CharacterSurvivalComponent on NPC_TestDebug). Layer overrides of an
// existing component are dropped on clients just as silently. A placed NPC may
// therefore override only CharacterIdentityComponent; anything else belongs in
// a prefab (PoliceDutyNPC.et, QuestGiver_Start.et).
// red-proof: with NPC_TestDebug's LF_CharacterSurvivalComponent and
// ActionsManagerComponent overrides restored in NPCs.layer this printed
// "FAIL ...NPCs.layer:4 NPC_TestDebug overrides LF_CharacterSurvivalComponent, ActionsManagerComponent"
// and exited 1.

import { readdirSync, readFileSync, statSync } from "node:fs";
import { dirname, join, relative } from "node:path";
import { fileURLToPath } from "node:url";

const root = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
const worlds = join(root, "addons", "LifeFramework", "Worlds");
const ALLOWED = new Set(["CharacterIdentityComponent"]);

function layers(dir) {
  const out = [];
  for (const name of readdirSync(dir)) {
    const path = join(dir, name);
    if (statSync(path).isDirectory()) out.push(...layers(path));
    else if (name.endsWith(".layer")) out.push(path);
  }
  return out;
}

const indentOf = (line) => line.length - line.trimStart().length;
const braces = (line) => (line.match(/{/g) || []).length - (line.match(/}/g) || []).length;

// Returns the class names directly inside the entity's components block.
export function npcOverrides(text) {
  const lines = text.split(/\r?\n/);
  const found = [];
  for (let i = 0; i < lines.length; i++) {
    // Only prefab instances replicate; a bare LF_NpcBaseEntity has no RplComponent.
    const head = lines[i].match(/^\s*LF_NpcBaseEntity\b([^:{]*):\s*"\{[0-9A-F]{16}\}/);
    if (!head) continue;
    const name = head[1].trim() || "(unnamed)";
    let depth = braces(lines[i]);
    const comps = [];
    for (let j = i + 1; j < lines.length && depth > 0; j++) {
      const line = lines[j];
      if (depth === 1 && /^\s*components\s*{/.test(line)) {
        const inner = indentOf(line) + 1;
        let d = braces(line);
        for (let k = j + 1; k < lines.length && d > 0; k++) {
          const l = lines[k];
          if (d === 1 && indentOf(l) === inner && /^\s*\w+\s+"\{[0-9A-F]{16}\}"/.test(l)) comps.push(l.trim().split(/\s+/)[0]);
          d += braces(l);
        }
      }
      depth += braces(line);
    }
    const bad = comps.filter((c) => !ALLOWED.has(c));
    if (bad.length) found.push({ line: i + 1, name, bad });
  }
  return found;
}

const errors = [];
let checked = 0;
for (const path of layers(worlds)) {
  checked++;
  for (const hit of npcOverrides(readFileSync(path, "utf8"))) {
    errors.push(`${relative(root, path)}:${hit.line} ${hit.name} overrides ${hit.bad.join(", ")}`);
  }
}

if (errors.length) {
  console.log("validate-npc-layer-overrides: FAIL");
  for (const e of errors) console.log(`  FAIL ${e}`);
  console.log("  A placed NPC may override only CharacterIdentityComponent; move the rest into a prefab.");
  process.exit(1);
}
console.log(`validate-npc-layer-overrides: OK (${checked} layer(s))`);
