// tools/validation/validate-annex-fixtures.mjs
// inputs: addons/**, docs/architecture/**
//
// A test must not depend on the DebugWorld village. The village (Street_*,
// Signs, the chain layers, Roadmap_* entities) moves to the pack in M3 (#323);
// a test that looks one up by name breaks "delete the pack and every test still
// passes" (docs/architecture/core-vs-pack.md). This check fails when a file in
// Scripts/Game/Tests/ names, as a string literal, an entity declared in a
// DebugWorld layer outside the annex layers, unless that (file, name) pair is in
// tools/validation/annex-fixtures-baseline.json. The baseline is today's debt;
// #323 batch 0 burns it down, and the check never lets it grow.
//
// What a test uses instead:
//   - an entity declared in an annex layer (ANNEX_LAYERS below), looked up with
//     GetGame().GetWorld().FindEntityByName("<annex name>"); or
//   - a prefab the test spawns itself with LF_Utils.SpawnEntityPrefab(prefab,
//     position) and deletes at the end (LF_TestCharacterFixture does this for
//     characters).
// To retire a baseline row, move the test onto one of those and run
//   node tools/validation/validate-annex-fixtures.mjs --write-baseline
// which rewrites the baseline from the current tree and refuses to add a pair.
// A baseline row whose pair no longer occurs is reported as stale (not a
// failure) so the next run of --write-baseline drops it.
// red-proof: tools/validation/annex-fixtures-selftest.mjs plants a new village
// reference and asserts it fails while a baselined one passes.

import { existsSync, readFileSync, readdirSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

// Annex layers per docs/architecture/core-pack-inventory.md. Every other layer
// under DebugWorld_Layers is the village (Quests is split; it counts as village
// until the clerk moves).
export const ANNEX_LAYERS = new Set([
  "Annex", "Crimes", "HousesKeys", "Jobs", "NPCs", "PRFeatures", "Police", "Tests", "WhitelistChat",
  "Factions", "Gamemode", "default", "Shops",
]);

export const LAYERS = "addons/LifeFramework/Worlds/DebugWorld/DebugWorld_Layers";
export const TESTS = "addons/LifeFramework/Scripts/Game/Tests";
export const BASELINE = "tools/validation/annex-fixtures-baseline.json";

// Declarations look like `GenericEntity Roadmap_Bank_Vault : "{GUID}prefab" {` at the top
// level; inside a `$grp` or a children block the type is dropped and the line is
// `Roadmap_Job_Depot {` (or `Name : "{GUID}prefab" {`, or `Type Name {`). A
// `components { ... }` block holds component overrides, not entities, so it is
// skipped by indent (an unnamed override reads `MeshObject {`).
export function declaredNames(text) {
  const names = [];
  let skipIndent = -1;
  for (const line of text.split("\n")) {
    const indent = line.length - line.trimStart().length;
    if (skipIndent >= 0) {
      if (indent > skipIndent || line.trim() === "") continue;
      skipIndent = -1;
    }
    if (/^\s*components\s*\{/.test(line)) {
      skipIndent = indent;
      continue;
    }
    const typed = line.match(/^\s*[A-Za-z_]\w*\s+([A-Za-z_]\w*)\s*(?::|\{)/);
    if (typed) {
      names.push(typed[1]);
      continue;
    }
    const nested = line.match(/^\s+([A-Z]\w*)\s*(?::|\{)/);
    if (nested) names.push(nested[1]);
  }
  return names;
}

// Map village entity name -> layer file that declares it. A name that an annex
// layer also declares is not village.
export function villageNames(root) {
  const dir = join(root, LAYERS);
  const village = new Map();
  const annex = new Set();
  if (!existsSync(dir)) return village;
  for (const f of readdirSync(dir)) {
    if (!f.endsWith(".layer")) continue;
    const layer = f.slice(0, -".layer".length);
    for (const n of declaredNames(readFileSync(join(dir, f), "utf8"))) {
      if (ANNEX_LAYERS.has(layer)) annex.add(n);
      else if (!village.has(n)) village.set(n, layer);
    }
  }
  for (const n of annex) village.delete(n);
  return village;
}

// { "<test file>": ["Name", ...] } for every village name a test file quotes.
export function scanTests(root) {
  const village = villageNames(root);
  const out = {};
  const dir = join(root, TESTS);
  if (!existsSync(dir) || !village.size) return out;
  for (const f of readdirSync(dir).sort()) {
    if (!f.endsWith(".c") || f.endsWith(".generated.c")) continue;
    const text = readFileSync(join(dir, f), "utf8").split("\n").map((l) => l.replace(/\/\/.*$/, "")).join("\n");
    const hits = new Set();
    for (const m of text.matchAll(/"([A-Za-z_]\w*)"/g)) if (village.has(m[1])) hits.add(m[1]);
    if (hits.size) out[f] = [...hits].sort();
  }
  return out;
}

export function loadBaseline(root) {
  const p = join(root, BASELINE);
  return existsSync(p) ? JSON.parse(readFileSync(p, "utf8")) : {};
}

// { errors, stale } comparing the tree with the baseline.
export function checkAnnexFixtures(root, baseline = loadBaseline(root)) {
  const now = scanTests(root);
  const errors = [];
  const stale = [];
  for (const [file, names] of Object.entries(now)) {
    const allowed = new Set(baseline[file] || []);
    for (const n of names) {
      if (!allowed.has(n)) {
        errors.push(`${TESTS}/${file}: names village entity "${n}"; look up an annex entity or spawn the prefab with LF_Utils.SpawnEntityPrefab (see validate-annex-fixtures.mjs), or this test breaks when the village moves to the pack (#323)`);
      }
    }
  }
  for (const [file, names] of Object.entries(baseline)) {
    const seen = new Set(now[file] || []);
    for (const n of names) if (!seen.has(n)) stale.push(`${file}: "${n}"`);
  }
  return { errors, stale };
}

if (process.argv[1] && fileURLToPath(import.meta.url) === process.argv[1]) {
  const root = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
  if (process.argv.includes("--write-baseline")) {
    const now = scanTests(root);
    const old = loadBaseline(root);
    const first = !existsSync(join(root, BASELINE));
    const added = [];
    for (const [file, names] of Object.entries(now)) {
      for (const n of names) if (!(old[file] || []).includes(n)) added.push(`${file}: "${n}"`);
    }
    if (!first && added.length) {
      console.log(`FAIL refusing to add ${added.length} pair(s) to the baseline:\n  ${added.join("\n  ")}`);
      process.exit(1);
    }
    writeFileSync(join(root, BASELINE), JSON.stringify(now, null, 1) + "\n");
    const files = Object.keys(now).length;
    const names = new Set(Object.values(now).flat()).size;
    console.log(`validate-annex-fixtures: wrote baseline (${files} test file(s), ${names} village name(s))`);
    process.exit(0);
  }
  const { errors, stale } = checkAnnexFixtures(root);
  if (errors.length) {
    for (const e of errors) console.log(`FAIL ${e}`);
    console.log(`validate-annex-fixtures: ${errors.length} new village reference(s) in tests`);
    process.exit(1);
  }
  const now = scanTests(root);
  const note = stale.length ? `; ${stale.length} baseline row(s) retired, run --write-baseline to drop them` : "";
  console.log(`validate-annex-fixtures: OK (${Object.keys(now).length} test file(s) still on ${new Set(Object.values(now).flat()).size} baselined village name(s)${note})`);
}
