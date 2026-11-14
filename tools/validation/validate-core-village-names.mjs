// tools/validation/validate-core-village-names.mjs
// inputs: addons/**, docs/architecture/**
//
// Core production Scripts must not name a village entity. The village (Street_*,
// Signs, the chain layers, Roadmap_* entities) moves to the LF_Everon1989 pack in
// M3 (#323); a core script that quotes one of its entity names ("Roadmap_Jail_Cell",
// "Roadmap_Vehicle_Lot") breaks the moment the pack is not loaded
// (docs/architecture/core-vs-pack.md). This check fails when a .c file under
// Scripts/Game, outside Tests/, quotes as a string literal the name of an entity
// that a DebugWorld village layer declares (top level, inside a $grp, or as a
// child; the same reader validate-annex-fixtures uses). Comments are ignored.
//
// What a script uses instead: an attribute on the placed entity (the layer sets
// it, a prefab default stays neutral), or a component the world places and the
// script finds through that component's static registry (one per world).
// tools/validation/core-village-names-baseline.json lists (file, name) pairs that
// are allowed for now. It is empty and must stay empty: the check never lets it
// grow, and --write-baseline refuses to add a pair.
// red-proof: tools/validation/core-village-names-selftest.mjs plants a village
// name in a core script (top-level layer entity and a $grp child) and asserts it
// fails, while a comment, a Tests/ file and an annex name pass.

import { existsSync, readFileSync, readdirSync, writeFileSync } from "node:fs";
import { dirname, join, relative } from "node:path";
import { fileURLToPath } from "node:url";
import { villageNames } from "./validate-annex-fixtures.mjs";

export const SCRIPTS = "addons/LifeFramework/Scripts/Game";
export const BASELINE = "tools/validation/core-village-names-baseline.json";

function* coreFiles(dir) {
  for (const e of readdirSync(dir, { withFileTypes: true })) {
    const full = join(dir, e.name);
    if (e.isDirectory()) {
      if (e.name === "Tests") continue;
      yield* coreFiles(full);
    } else if (e.name.endsWith(".c")) {
      yield full;
    }
  }
}

// { "<path under Scripts/Game>": ["Name", ...] } for every village name a core script quotes.
export function scanCore(root) {
  const village = villageNames(root);
  const out = {};
  const base = join(root, SCRIPTS);
  if (!existsSync(base) || !village.size) return out;
  for (const file of coreFiles(base)) {
    const text = readFileSync(file, "utf8").split("\n").map((l) => l.replace(/\/\/.*$/, "")).join("\n");
    const hits = new Set();
    for (const m of text.matchAll(/"([A-Za-z_]\w*)"/g)) if (village.has(m[1])) hits.add(m[1]);
    if (hits.size) out[relative(base, file).replaceAll("\\", "/")] = [...hits].sort();
  }
  return out;
}

export function loadBaseline(root) {
  const p = join(root, BASELINE);
  return existsSync(p) ? JSON.parse(readFileSync(p, "utf8")) : {};
}

// { errors, stale } comparing the tree with the baseline.
export function checkCoreVillageNames(root, baseline = loadBaseline(root)) {
  const now = scanCore(root);
  const errors = [];
  const stale = [];
  for (const [file, names] of Object.entries(now)) {
    const allowed = new Set(baseline[file] || []);
    for (const n of names) {
      if (!allowed.has(n)) {
        errors.push(`${SCRIPTS}/${file}: core script names village entity "${n}"; read an attribute on the placed entity or find a placed component instead (see validate-core-village-names.mjs), or the village cannot move to the pack (#323)`);
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
    const now = scanCore(root);
    const old = loadBaseline(root);
    const added = [];
    for (const [file, names] of Object.entries(now)) {
      for (const n of names) if (!(old[file] || []).includes(n)) added.push(`${file}: "${n}"`);
    }
    if (added.length) {
      console.log(`FAIL refusing to add ${added.length} pair(s) to the baseline:\n  ${added.join("\n  ")}`);
      process.exit(1);
    }
    writeFileSync(join(root, BASELINE), JSON.stringify(now, null, 1) + "\n");
    console.log(`validate-core-village-names: wrote baseline (${Object.keys(now).length} file(s))`);
    process.exit(0);
  }
  const { errors, stale } = checkCoreVillageNames(root);
  if (errors.length) {
    for (const e of errors) console.log(`FAIL ${e}`);
    console.log(`validate-core-village-names: ${errors.length} village reference(s) in core scripts`);
    process.exit(1);
  }
  const note = stale.length ? `; ${stale.length} baseline row(s) retired, run --write-baseline to drop them` : "";
  console.log(`validate-core-village-names: OK (${Object.keys(scanCore(root)).length} core file(s) still on baselined village names${note})`);
}
