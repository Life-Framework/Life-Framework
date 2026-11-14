// tools/validation/validate-road-ids.mjs
//
// A road spline's RoadEntity takes its material from the road generator prefab only when its `ID`
// is the ID of that prefab's own RoadEntity child. Swap the generator (dirt for cobblestone) and
// keep the old child ID, and the road is a new child with no material: Workbench and the game draw
// it as a red, blue and white checkerboard (#924, the market square on 2026-11-14).
//
// The base-game prefabs are not readable in CI, so the check is self-consistency: every RoadEntity
// under the same generator prefab, across the DebugWorld layers, carries the same ID, and that ID
// is never the one another generator's roads use. Known IDs (read with game_read):
//   RG_Road_Cobblestone_01        56F07CB2CD0F496C
//   RG_Road_Dirt_01               523EAA80CAA6C6B5
//   RG_Road_Asphalt_E_01_Narrow   556C6554AF23997E
// red-proof: 2026-11-14, Village_Market with the cobblestone generator and the dirt child ID printed
// "FAIL Village_Roads.layer:353 RoadEntity ID 523EAA80CAA6C6B5 under RG_Road_Cobblestone_01.et; its other roads use 56F07CB2CD0F496C"
// and exited 1.

import { readFileSync, readdirSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";

const root = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
const LAYERS = join(root, "addons", "LifeFramework", "Worlds", "DebugWorld", "DebugWorld_Layers");
export const KNOWN = {
  "RG_Road_Cobblestone_01.et": "56F07CB2CD0F496C",
  "RG_Road_Dirt_01.et": "523EAA80CAA6C6B5",
  "RG_Road_Asphalt_E_01_Narrow.et": "556C6554AF23997E",
};

// [{ file, line, generator, id }] for every RoadEntity under a RoadGeneratorEntity.
export function roadIds(file, text) {
  const out = [];
  let gen = null;
  text.split(/\r?\n/).forEach((l, i) => {
    const g = l.match(/RoadGeneratorEntity : "\{[0-9A-F]{16}\}[^"]*\/([^"/]+\.et)"/);
    if (g) gen = g[1];
    const id = l.match(/^\s*ID "([0-9A-F]{16})"/);
    if (id && gen) { out.push({ file, line: i + 1, generator: gen, id: id[1] }); gen = null; }
  });
  return out;
}

export function problems(rows) {
  const out = [];
  const byGen = new Map();
  for (const r of rows) (byGen.get(r.generator) || byGen.set(r.generator, []).get(r.generator)).push(r);
  const majority = new Map();
  for (const [gen, list] of byGen) {
    if (KNOWN[gen]) { majority.set(gen, KNOWN[gen]); continue; }
    const counts = new Map();
    for (const r of list) counts.set(r.id, (counts.get(r.id) || 0) + 1);
    majority.set(gen, [...counts].sort((a, b) => b[1] - a[1])[0][0]);
  }
  for (const r of rows) {
    const want = majority.get(r.generator);
    if (r.id !== want) out.push(`${r.file}:${r.line} RoadEntity ID ${r.id} under ${r.generator}; its other roads use ${want}`);
  }
  return out;
}

function main() {
  const rows = [];
  for (const f of readdirSync(LAYERS).filter((n) => n.endsWith(".layer"))) rows.push(...roadIds(f, readFileSync(join(LAYERS, f), "utf8")));
  const bad = problems(rows);
  for (const p of bad) console.log(`FAIL ${p}`);
  if (bad.length) {
    console.log("validate-road-ids: a road's RoadEntity ID must be its generator prefab's child ID, or it renders as a checkerboard");
    return 1;
  }
  console.log(`validate-road-ids: OK (${rows.length} roads)`);
  return 0;
}

if (process.argv[1] && import.meta.url === pathToFileURL(process.argv[1]).href) process.exit(main());
