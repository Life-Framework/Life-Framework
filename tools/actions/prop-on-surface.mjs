// tools/actions/prop-on-surface.mjs
//
// Footprint check for props that rest on a counter, desk, table or bench: the
// prop's footprint (its .xob bounds, rotated by its yaw, in world space) must lie
// on the surface's top face. Reads the DebugWorld layers and every addon prefab
// that places children, with the same offline .xob header bounds the action
// anchor tools use. Entry point: tools/validation/validate-prop-on-surface.mjs.
//
// A prop counts as "placed on" a surface when its centre is within MARGIN of the
// surface footprint and its bottom is within BAND of the surface top. It is
// flagged when any corner of its footprint is more than OVERHANG_MAX past an edge.
// A backsplash (a workbench's raised back) does not matter: only the plan view is
// tested. Docs: docs/agent-area-polish.md ("placement").

import { readdirSync, readFileSync } from "node:fs";
import { join } from "node:path";
import { ROOT, addonOf, boundsOf, resolveChain } from "./place-action-contexts.mjs";

export const OVERHANG_MAX = 0.1; // metres a prop may hang past the edge (round bottles, a tilted lid)
export const MARGIN = 0.3; // centre may sit this far outside the footprint and still count as "on" it
export const BAND = 0.5; // bottom within this of the surface top
export const MAX_PROP = 1.5; // anything larger than this in plan is furniture, not a prop

const SURFACE_MESH = /Counter|Desk|Table|Workbench|MarketStand/i;
const NOT_SURFACE = /Chair|Phone|Clock|Umbrella|Poster|Cashier|Post(?:Office)?Box/i;
const NOT_PROP = /Wall|Roof|Sign|Tree|Door|Window|Fence|Villa|Building/i;

// ---- parsing ----------------------------------------------------------------

// Layer and prefab text is a tree of `Header {` ... `}` blocks with `key value` lines.
export function parseTree(text) {
  const lines = text.split("\n");
  let i = 0;
  function block(header) {
    const b = { header, props: {}, kids: [] };
    while (i < lines.length) {
      const l = lines[i++].trim();
      if (l === "}") return b;
      if (l === "") continue;
      if (l.endsWith("{")) {
        b.kids.push(block(l.slice(0, -1).trim()));
        continue;
      }
      const m = l.match(/^(\w+) (.*)$/);
      if (m) b.props[m[1]] = m[2];
    }
    return b;
  }
  return block("");
}

const prefabOf = (header) => (header.match(/"\{[0-9A-Fa-f]{16}\}([^"]+\.et)"/) || [])[1] || null;

function ownMesh(b) {
  for (const k of b.kids) {
    if (k.header !== "components") continue;
    for (const c of k.kids) {
      if (c.header.startsWith("MeshObject") && c.props.Object) {
        const m = c.props.Object.match(/"\{[0-9A-Fa-f]{16}\}([^"]+)"/);
        if (m) return m[1];
      }
    }
  }
  return null;
}

// The base game's TableRecreation prefab library path does not exist in the pak
// (PrefabLibrary/DefaultLibrary/...); it is the same mesh everywhere it is used.
const KNOWN_MESH = [[/TableRecreation_01/, "Assets/Props/Furniture/TableRecreation_01/TableRecreation_01.xob"]];

function meshOf(root, b, inheritedPrefab) {
  const own = ownMesh(b);
  if (own) return own;
  const prefab = prefabOf(b.header) || inheritedPrefab;
  if (!prefab) return null;
  for (const [re, mesh] of KNOWN_MESH) if (re.test(prefab)) return mesh;
  try {
    return resolveChain(root, `X : "{0000000000000000}${prefab}" {\n}`).mesh;
  } catch {
    return null;
  }
}

function nameOf(b) {
  const m = b.header.match(/^(?:\w+\s+)?(\w+)/);
  return m && m[1] !== "GenericEntity" && !m[1].startsWith("$") ? m[1] : "(unnamed)";
}

// Flatten a parsed tree into entities with world position and yaw. `par` is the
// parent entity. `$grp` blocks only carry a prefab for their children.
export function flatten(root, tree, file, { prefabRoot = null } = {}) {
  const out = [];
  function visit(b, par, grpPrefab) {
    for (const k of b.kids) {
      if (k.header === "components") continue;
      if (k.header.startsWith("$grp")) {
        visit(k, par, prefabOf(k.header) || grpPrefab);
        continue;
      }
      if (!k.props.coords) {
        visit(k, par, grpPrefab);
        continue;
      }
      const c = k.props.coords.split(" ").map(Number);
      const yaw = k.props.angleY ? Number(k.props.angleY) : 0;
      const scale = k.props.scale ? Number(k.props.scale) : 1;
      const r = par ? (par.yaw * Math.PI) / 180 : 0;
      const x = par ? par.x + c[0] * Math.cos(r) + c[2] * Math.sin(r) : c[0];
      const z = par ? par.z - c[0] * Math.sin(r) + c[2] * Math.cos(r) : c[2];
      const y = par ? par.y + c[1] : c[1];
      const mesh = meshOf(root, k, grpPrefab);
      const ent = { file, name: nameOf(k), mesh, bounds: mesh ? boundsOf(root, mesh) : null, x, y, z, yaw: (par ? par.yaw : 0) + yaw, scale };
      out.push(ent);
      visit(k, ent, null);
    }
  }
  if (prefabRoot) {
    // An .et file: the single top-level entity is the root, at the origin.
    for (const k of tree.kids) {
      const mesh = meshOf(root, k, null) || prefabRoot.mesh;
      const ent = { file, name: prefabRoot.name, mesh, bounds: mesh ? boundsOf(root, mesh) : null, x: 0, y: 0, z: 0, yaw: 0, scale: 1 };
      out.push(ent);
      visit(k, ent, null);
    }
  } else {
    visit(tree, null, null);
  }
  return out;
}

// ---- geometry ---------------------------------------------------------------

export function footprint(e) {
  const r = (e.yaw * Math.PI) / 180;
  const cs = Math.cos(r);
  const sn = Math.sin(r);
  const pts = [];
  for (const lx of [e.bounds.min[0], e.bounds.max[0]]) {
    for (const lz of [e.bounds.min[2], e.bounds.max[2]]) {
      pts.push([e.x + lx * e.scale * cs + lz * e.scale * sn, e.z - lx * e.scale * sn + lz * e.scale * cs]);
    }
  }
  return pts;
}

export function isSurface(e) {
  if (!e.bounds || !e.mesh) return false;
  const { min, max } = e.bounds;
  if (!SURFACE_MESH.test(e.mesh) || NOT_SURFACE.test(e.mesh)) return false;
  if (max[0] - min[0] < 0.6 || max[2] - min[2] < 0.3) return false;
  return max[1] * e.scale > 0.6 && max[1] * e.scale < 1.6;
}

// Returns how far (m) the prop's footprint corners stick out past the surface's
// edges, measured in the surface's own axes. 0 means fully on.
export function overhang(surface, prop) {
  const r = (-surface.yaw * Math.PI) / 180;
  const cs = Math.cos(r);
  const sn = Math.sin(r);
  const loc = footprint(prop).map(([x, z]) => {
    const dx = x - surface.x;
    const dz = z - surface.z;
    return [dx * cs + dz * sn, -dx * sn + dz * cs];
  });
  const s = surface.scale;
  const minX = Math.min(...loc.map((p) => p[0]));
  const maxX = Math.max(...loc.map((p) => p[0]));
  const minZ = Math.min(...loc.map((p) => p[1]));
  const maxZ = Math.max(...loc.map((p) => p[1]));
  const b = surface.bounds;
  return {
    over: Math.max(b.min[0] * s - minX, maxX - b.max[0] * s, b.min[2] * s - minZ, maxZ - b.max[2] * s, 0),
    // how far the centre is outside the footprint (0 inside); used for the "placed on" test
    centreOut: Math.max(
      b.min[0] * s - (minX + maxX) / 2,
      (minX + maxX) / 2 - b.max[0] * s,
      b.min[2] * s - (minZ + maxZ) / 2,
      (minZ + maxZ) / 2 - b.max[2] * s,
      0,
    ),
  };
}

// Check one flat entity list. Returns findings { file, prop, surface, over, drop }.
export function checkEntities(ents) {
  const surfaces = ents.filter(isSurface);
  const findings = [];
  for (const p of ents) {
    if (!p.bounds || !p.mesh || isSurface(p) || NOT_PROP.test(p.mesh)) continue;
    const size = [p.bounds.max[0] - p.bounds.min[0], p.bounds.max[2] - p.bounds.min[2]].map((v) => v * p.scale);
    if (Math.max(...size) > MAX_PROP) continue;
    for (const s of surfaces) {
      if (s === p) continue;
      const top = s.y + s.bounds.max[1] * s.scale;
      const bottom = p.y + p.bounds.min[1] * p.scale;
      if (Math.abs(bottom - top) > BAND) continue;
      const o = overhang(s, p);
      if (o.centreOut > MARGIN) continue;
      if (o.over > OVERHANG_MAX) findings.push({ file: p.file, prop: p.name, mesh: p.mesh.split("/").pop(), surface: s.name, surfaceMesh: s.mesh.split("/").pop(), over: o.over, drop: bottom - top });
    }
  }
  return findings;
}

// ---- repo scan ----------------------------------------------------------------

function walkDir(dir, ext, out = []) {
  for (const entry of readdirSync(dir, { withFileTypes: true })) {
    const path = join(dir, entry.name);
    if (entry.isDirectory()) walkDir(path, ext, out);
    else if (entry.name.endsWith(ext)) out.push(path);
  }
  return out;
}

export function scanRepo(root = ROOT) {
  const addon = addonOf(root);
  const findings = [];
  let entities = 0;
  const layers = walkDir(join(addon, "Worlds"), ".layer");
  // Surfaces and props live in different layers (a counter in Shops, its till in the same,
  // but a prop may be in a later layer): check the union of all layers together.
  const all = [];
  for (const f of layers) all.push(...flatten(root, parseTree(readFileSync(f, "utf8")), f.slice(addon.length + 1).replace(/\\/g, "/")));
  entities += all.length;
  findings.push(...checkEntities(all));
  for (const f of walkDir(join(addon, "Prefabs"), ".et")) {
    const text = readFileSync(f, "utf8");
    if (!/^\s*\{\s*$/m.test(text) && !/\n\s+(?:GenericEntity|GameEntity|SCR_\w+)\s+\w+\s*(?::|\{)/.test(text)) continue; // no child entities
    const rel = f.slice(addon.length + 1).replace(/\\/g, "/");
    let mesh = null;
    try {
      mesh = resolveChain(root, text).mesh;
    } catch {
      mesh = null;
    }
    const tree = parseTree(text);
    const ents = flatten(root, tree, rel, { prefabRoot: { name: rel.split("/").pop(), mesh } });
    entities += ents.length;
    findings.push(...checkEntities(ents));
  }
  return { findings, entities };
}
