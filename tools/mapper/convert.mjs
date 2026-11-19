// tools/mapper/convert.mjs: a Reforger Mapper project (mapgen.json, version 0) to a placement plan for our layers.
//
// Pure functions, no I/O: tools/commands/mapper.mjs reads the file, resolves prefabs and runs the placement tool;
// tools/validation/mapper-selftest.mjs runs these on tools/test/fixtures/mapper/. The format is read from Mapper's
// published docs (docs/ARCHITECTURE.md "Pivot format", src/project.ts) and described in
// docs/architecture/reforger-mapper-import.md. Nothing here is Mapper's code (MIT, notice in
// tools/test/fixtures/mapper/LICENSE-Mapper.txt); the shape is reimplemented from the documented fields.
//
//   parseProject   JSON text -> { entities, shapes, notes, error }: what a project holds that we can place
//   fit            the layout's extent and the cell span it needs
//   translate      entities moved so the layout's centre lands on a point, yaw kept
//   entityName     the layer entity name of the n-th imported prop

import { CELL } from "../grid/lattice.mjs";
import { MARGIN, spanFor } from "../grid/autosize.mjs";

export const PROJECT_VERSION = 0;
const GUID_PATH = /^(?:\{([0-9A-Fa-f]{16})\})?([^{].*\.et)$/;
const r3 = (n) => Math.round(n * 1000) / 1000;

// `{GUID}path.et` or a bare `path.et` (Mapper allows both; its plugin resolves a bare path in Workbench).
export function splitPrefab(text) {
  const m = GUID_PATH.exec(String(text ?? "").trim().replace(/\\/g, "/"));
  return m ? { guid: m[1] ? m[1].toUpperCase() : null, path: m[2] } : null;
}

// The Enfusion yaw, degrees, as the plugin writes it (`angles "0 <yaw> 0"`) and as `place ground --yaw` takes it.
const normYaw = (a) => {
  const y = ((Number(a) % 360) + 360) % 360;
  return r3(y > 180 ? y - 360 : y);
};

// Reads the text of a project file. `error` is set when nothing can be placed from it (not JSON, wrong shape, wrong
// version, no entities); `notes` are the things left out on purpose (shapes, heights, bad rows), one line each.
export function parseProject(text) {
  const out = { entities: [], shapes: 0, notes: [], error: null };
  let doc;
  try {
    doc = JSON.parse(text);
  } catch (e) {
    out.error = `not JSON (${e.message})`;
    return out;
  }
  if (!doc || typeof doc !== "object" || Array.isArray(doc)) {
    out.error = "the project is not an object";
    return out;
  }
  if (doc.version !== PROJECT_VERSION) {
    out.error = `project version is ${JSON.stringify(doc.version)}, this reader knows version ${PROJECT_VERSION} (Mapper src/project.ts)`;
    return out;
  }
  const shapes = Array.isArray(doc.shapes) ? doc.shapes : [];
  out.shapes = shapes.length;
  if (shapes.length) {
    const kinds = new Map();
    for (const s of shapes) {
      const k = String(s?.generator ?? "").replace(/^\{[0-9A-Fa-f]{16}\}/, "").split("/").slice(-2, -1)[0] || String(s?.kind ?? "shape");
      kinds.set(k, (kinds.get(k) ?? 0) + 1);
    }
    out.notes.push(`${shapes.length} shape(s) not imported (terrain/generator shapes: ${[...kinds].map(([k, n]) => `${k} x${n}`).join(", ")}); a generator is Workbench's to run, not a layer prop`);
  }
  if (Array.isArray(doc.remove) && doc.remove.length) out.notes.push(`${doc.remove.length / 2} "remove" origin(s) ignored (they delete vanilla objects in a Mapper world; we never delete)`);
  const rows = Array.isArray(doc.entities) ? doc.entities : [];
  if (!rows.length) {
    out.error = `no entities to place${shapes.length ? " (the project holds shapes only)" : ""}`;
    return out;
  }
  const seen = new Set();
  let heights = 0;
  rows.forEach((e, i) => {
    const label = `entities[${i}]${e && typeof e.id === "string" ? ` ${e.id}` : ""}`;
    if (!e || typeof e !== "object") return out.notes.push(`${label}: not an object, dropped`);
    const p = splitPrefab(e.prefab);
    if (!p) return out.notes.push(`${label}: prefab ${JSON.stringify(e.prefab)} is not a "{GUID}path.et" or a path ending in .et, dropped`);
    if (!Number.isFinite(e.x) || !Number.isFinite(e.z)) return out.notes.push(`${label}: x/z are not numbers, dropped`);
    if (e.yaw != null && !Number.isFinite(e.yaw)) return out.notes.push(`${label}: yaw is not a number, dropped`);
    const id = typeof e.id === "string" && e.id ? e.id : `e${i}`;
    if (seen.has(id)) out.notes.push(`${label}: id repeats (kept, names come from the order)`);
    seen.add(id);
    if (e.ycm) heights++;
    out.entities.push({ id, prefab: p, x: Number(e.x), z: Number(e.z), yaw: normYaw(e.yaw ?? 0), role: typeof e.role === "string" ? e.role : null });
  });
  if (heights) out.notes.push(`${heights} entit${heights === 1 ? "y has" : "ies have"} an absolute height (ycm); ignored, props stand on the ground the placement tool probes`);
  if (!out.entities.length) out.error = "every entity row was dropped (see notes)";
  return out;
}

// The footprint of the layout (entity positions, not meshes) and the cells it needs with the grid's walk-around margin.
export function fit(entities, margin = MARGIN) {
  const xs = entities.map((e) => e.x);
  const zs = entities.map((e) => e.z);
  const min = [Math.min(...xs), Math.min(...zs)];
  const max = [Math.max(...xs), Math.max(...zs)];
  const w = max[0] - min[0];
  const d = max[1] - min[1];
  return { min, max, centre: [(min[0] + max[0]) / 2, (min[1] + max[1]) / 2], w: r3(w), d: r3(d), span: [spanFor(w, margin), spanFor(d, margin)] };
}

// Entities with `x`/`z` moved so the centre of their bounding box is `to`. Yaw and order are kept.
export function translate(entities, to) {
  const f = fit(entities);
  return entities.map((e) => ({ ...e, x: r3(e.x - f.centre[0] + to[0]), z: r3(e.z - f.centre[1] + to[1]) }));
}

// The rectangle of a claim as the lattice gives it ({x0,z0,x1,z1}): its middle.
export const centreOf = (rect) => [(rect.x0 + rect.x1) / 2, (rect.z0 + rect.z1) / 2];

// `Grid_<slug>_M<n>`: the entity prefix of the cell and a 1-based number; the Mapper id is not used (it can hold any text).
export const entityName = (prefix, n) => `${prefix}_M${n}`;

export const CELL_SIZE = CELL.size;
