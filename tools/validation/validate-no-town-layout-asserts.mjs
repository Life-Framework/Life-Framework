// tools/validation/validate-no-town-layout-asserts.mjs
// inputs: addons/**, agent-box/**, tools/validation/**, tools/test/**
//
// Tests belong to the grid, not the town. The town (the DebugWorld village layers and the pack
// world Everon1989Town) is placed by hand, and where a thing stands there is mostly arbitrary
// (maintainer, 2026-11-15: "it should probably not be written in the first place"). A test, harness
// proof or validator that reads it breaks every time he moves a crate.
//
// This check fails when a NEW checked file:
//   name   quotes a town entity's name as a fixture: a string literal in a test, proof, validator
//          or the town manifest, or a bare word in a harness flow (`layer Quest_Clerk ...`);
//   coord  writes a position that stands on the town: a "x y z" string, a Vector(x, y, z), or an
//          `x: .., z: ..` pair within NEAR_M of a placed town entity and outside the annex.
// Checked files: Scripts/Game/Tests/*.c of the core and the pack, agent-box/proofs/*.mjs,
// agent-box/flows/*.txt, agent-box/town.mjs (the manifest), agent-box/harness.mjs (its selftest)
// and tools/validation/validate-*.mjs.
// Selftests are not scanned: they build their own throwaway trees.
//
// What to write instead (docs/architecture/three-worlds.md, core-vs-pack.md "Map independence"):
//   - a fixture in the debug grid (`tools\cli grid place`) or a prefab the test spawns itself;
//   - for a contract that must hold wherever the maintainer puts things: find the place by
//     component, `LF_Places.Find(kind, id)`, a sign id, or the actions an entity carries.
// A line that names a town entity or layer only to read the layer's structure (parse, GUIDs,
// zone membership), never where it stands, may carry `town-layout-ok: <reason>` in a comment.
//
// tools/validation/town-layout-baseline.json is today's debt as { "<file>": ["name:X", "coord:x,z"] }.
// It only shrinks: `--write-baseline` rewrites it from the tree and refuses to add a row. A row
// that no longer occurs is reported as retired (not a failure) so the next --write-baseline drops it.
// A new row in a file this branch did not change came from the town moving (a crate now stands
// near an old literal), not from a new test, so it is a warning: a hand edit of the town never
// fails this check on any branch.
// red-proof: tools/validation/no-town-layout-asserts-selftest.mjs.

import { existsSync, readFileSync, readdirSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { ANNEX_LAYERS, classWords, declaredNames } from "./validate-annex-fixtures.mjs";
import { changedFiles } from "./validate-world-zones.mjs";

export const BASELINE = "tools/validation/town-layout-baseline.json";
export const TOWN_DIRS = [
  "addons/LifeFramework/Worlds/DebugWorld/DebugWorld_Layers",
  "addons/LF_Everon1989/Worlds/Everon1989Town/Everon1989Town_Layers",
];
// The annex rectangle (three-worlds.md "What counts as grid") and how near a literal must be to a
// town entity to count as standing on the town.
export const ANNEX = { x0: 18, x1: 114, z0: 18, z1: 114 };
export const NEAR_M = 6;
const PRAGMA = /town-layout-ok:\s*\S/;

function isGridLayer(layer) {
  return ANNEX_LAYERS.has(layer) || layer.startsWith("Grid_");
}

function layerTexts(root) {
  const out = [];
  for (const rel of TOWN_DIRS) {
    const dir = join(root, rel);
    if (!existsSync(dir)) continue;
    for (const f of readdirSync(dir).sort()) {
      if (f.endsWith(".layer")) out.push({ layer: f.slice(0, -".layer".length), text: readFileSync(join(dir, f), "utf8") });
    }
  }
  return out;
}

// { names: Map(name -> layer), points: [{ x, z, name }] } for the town. A name an annex or grid
// layer also declares is not town. Points are every top-level `coords` of a town layer.
export function townModel(root) {
  const texts = layerTexts(root);
  const classes = new Set();
  for (const { text } of texts) for (const c of classWords(text)) classes.add(c);
  const names = new Map();
  const grid = new Set();
  const points = [];
  for (const { layer, text } of texts) {
    const declared = declaredNames(text, classes);
    if (isGridLayer(layer)) {
      for (const n of declared) grid.add(n);
      continue;
    }
    for (const n of declared) if (!names.has(n)) names.set(n, layer);
    // Only top-level and first-nesting coords are world positions; deeper ones are offsets.
    for (const m of text.matchAll(/^ {0,2}coords\s+(-?[\d.]+)\s+(-?[\d.]+)\s+(-?[\d.]+)/gm)) {
      points.push({ x: Number(m[1]), z: Number(m[3]), layer });
    }
  }
  for (const n of grid) names.delete(n);
  return { names, points };
}

function inAnnex(x, z) {
  return x >= ANNEX.x0 && x <= ANNEX.x1 && z >= ANNEX.z0 && z <= ANNEX.z1;
}

export function onTown(x, z, points) {
  if (!Number.isFinite(x) || !Number.isFinite(z) || inAnnex(x, z)) return false;
  if (Math.abs(x) < 20 && Math.abs(z) < 20) return false; // offsets and small vectors, not positions
  return points.some((p) => Math.hypot(p.x - x, p.z - z) <= NEAR_M);
}

// The files this check reads, repo-relative.
export function checkedFiles(root) {
  const files = [];
  const add = (rel, filter) => {
    const dir = join(root, rel);
    if (!existsSync(dir)) return;
    for (const f of readdirSync(dir).sort()) if (filter(f)) files.push(`${rel}/${f}`);
  };
  add("addons/LifeFramework/Scripts/Game/Tests", (f) => f.endsWith(".c") && !f.endsWith(".generated.c"));
  add("addons/LF_Everon1989/Scripts/Game/Tests", (f) => f.endsWith(".c"));
  add("agent-box/proofs", (f) => f.endsWith(".mjs"));
  add("agent-box/flows", (f) => f.endsWith(".txt"));
  for (const f of ["agent-box/town.mjs", "agent-box/harness.mjs"]) if (existsSync(join(root, f))) files.push(f);
  add("tools/validation", (f) => /^validate-.*\.mjs$/.test(f) && f !== "validate-no-town-layout-asserts.mjs");
  return files;
}

function stripComment(line, rel) {
  if (rel.endsWith(".txt")) return line.replace(/^\s*#.*$/, "");
  return line.replace(/(^|[^:"'`])\/\/.*$/, "$1");
}

// Sorted ["name:X", "coord:x,z"] rows for one file's text.
export function scanText(rel, text, model) {
  const hits = new Set();
  for (const raw of text.split("\n")) {
    if (PRAGMA.test(raw)) continue;
    const line = stripComment(raw, rel);
    if (!line.trim()) continue;
    const words = rel.endsWith(".txt")
      ? line.matchAll(/\b([A-Za-z_]\w*)\b/g)
      : line.matchAll(/["'`]([A-Za-z_]\w*)["'`]/g);
    for (const m of words) if (model.names.has(m[1])) hits.add(`name:${m[1]}`);
    const coord = (x, z) => {
      if (onTown(Number(x), Number(z), model.points)) hits.add(`coord:${Math.round(Number(x))},${Math.round(Number(z))}`);
    };
    for (const m of line.matchAll(/"(-?\d+(?:\.\d+)?)\s+(-?\d+(?:\.\d+)?)\s+(-?\d+(?:\.\d+)?)"/g)) coord(m[1], m[3]);
    for (const m of line.matchAll(/Vector\(\s*(-?\d+(?:\.\d+)?)\s*,\s*(-?\d+(?:\.\d+)?)\s*,\s*(-?\d+(?:\.\d+)?)\s*\)/g)) coord(m[1], m[3]);
    for (const m of line.matchAll(/\bx:\s*(-?\d+(?:\.\d+)?)[^}\n]*?\bz:\s*(-?\d+(?:\.\d+)?)/g)) coord(m[1], m[2]);
  }
  return [...hits].sort();
}

// { "<file>": [rows] } over the tree.
export function scanTree(root, model = townModel(root)) {
  const out = {};
  for (const rel of checkedFiles(root)) {
    const rows = scanText(rel, readFileSync(join(root, rel), "utf8"), model);
    if (rows.length) out[rel] = rows;
  }
  return out;
}

export function loadBaseline(root) {
  const p = join(root, BASELINE);
  return existsSync(p) ? JSON.parse(readFileSync(p, "utf8")) : {};
}

// `changed`: the repo paths this branch changed, or null when git cannot tell (then every new row fails).
export function check(root, baseline = loadBaseline(root), now = scanTree(root), changed = null) {
  const errors = [];
  const warnings = [];
  const retired = [];
  for (const [file, rows] of Object.entries(now)) {
    const allowed = new Set(baseline[file] || []);
    const sink = changed && !changed.includes(file) ? warnings : errors;
    for (const row of rows) {
      if (allowed.has(row)) continue;
      const [kind, what] = [row.slice(0, row.indexOf(":")), row.slice(row.indexOf(":") + 1)];
      sink.push(
        kind === "name"
          ? `${file}: names town entity "${what}" as a fixture; use a grid fixture, LF_Places, a sign id or the actions it carries`
          : `${file}: position ${what} stands on the town; spawn on grid ground or find the place by component`,
      );
    }
  }
  for (const [file, rows] of Object.entries(baseline)) {
    const seen = new Set(now[file] || []);
    for (const row of rows) if (!seen.has(row)) retired.push(`${file}: ${row}`);
  }
  return { errors, warnings, retired };
}

if (process.argv[1] && fileURLToPath(import.meta.url) === process.argv[1]) {
  const root = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
  const now = scanTree(root);
  const count = (o) => Object.values(o).reduce((n, rows) => n + rows.length, 0);
  if (process.argv.includes("--write-baseline")) {
    const old = loadBaseline(root);
    const first = !existsSync(join(root, BASELINE));
    const added = [];
    for (const [file, rows] of Object.entries(now)) for (const r of rows) if (!(old[file] || []).includes(r)) added.push(`${file}: ${r}`);
    if (!first && added.length) {
      console.log(`FAIL refusing to add ${added.length} row(s) to the baseline:\n  ${added.join("\n  ")}`);
      process.exit(1);
    }
    writeFileSync(join(root, BASELINE), JSON.stringify(now, null, 1) + "\n");
    console.log(`validate-no-town-layout-asserts: wrote baseline (${Object.keys(now).length} file(s), ${count(now)} row(s))`);
    process.exit(0);
  }
  if (process.argv.includes("--list")) {
    for (const [file, rows] of Object.entries(now)) console.log(`${file}\n  ${rows.join("\n  ")}`);
    process.exit(0);
  }
  const { errors, warnings, retired } = check(root, loadBaseline(root), now, changedFiles(root));
  for (const w of warnings) console.log(`WARN ${w} (the file is unchanged on this branch: the town moved)`);
  if (errors.length) {
    for (const e of errors) console.log(`FAIL ${e}`);
    console.log(`validate-no-town-layout-asserts: ${errors.length} new town layout reference(s) (docs/architecture/world-zones.md "Tests do not read the town")`);
    process.exit(1);
  }
  const note = retired.length ? `; ${retired.length} baseline row(s) retired, run --write-baseline to drop them` : "";
  console.log(`validate-no-town-layout-asserts: OK (${Object.keys(now).length} file(s) on ${count(now)} baselined row(s)${note})`);
}
