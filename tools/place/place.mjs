#!/usr/bin/env node
// tools\cli place: put a world prop where it belongs without typing coordinates (docs/placement-tool.md).
//
//   place on <surface> <prefab> --layer L.layer [--name N] [--at center|north|south-east|u,v] [--near x,z] [--yaw deg] [--facing north|deg]
//   place against <target> <prefab> --layer L.layer (--near x,z | --face north [--along 0.5]) [--hang 1.4] [--front +z|-z|+x|-x]
//   place ground <x,z> <prefab> --layer L.layer [--yaw deg | --facing north|deg | --face-to x,z]
//   place arrange on <surface> <prefab>... --layer L.layer [--count n] [--spacing 0.05] [--at north]
//   place arrange row <x1,z1> <x2,z2> <prefab>... --layer L.layer [--yaw deg]
//   place bounds <prefab|entity>      size, base, scale, rigid body
//   place probe <surface>             trace a surface's real top into tools/place/surface-cache.json
//   place verify <entity>...          the placement audit, on those entities only
//
// `--move <Entity>[,<Entity>...]` in place of <prefab> re-places entities a layer already has.
// `--in <Prefabs/X.et>` works on a prefab's own children (surface `root`), offline only.
// Common: --dry (print, write nothing), --no-verify (skip the audit boot), --keep (leave a failed
// placement in the file), --force (write despite an offline finding), --scale s, --front <axis>.
//
// How it knows: mesh bounds come from the .xob header (offline, tools/actions/place-action-contexts.mjs);
// a surface's real top, a wall's real face and the ground height come from probe rays traced on the
// dedicated test server (LF_PlacementAudit.RayLine, about 11 s); every write is then measured by the
// placement audit (agent-box/place-audit.mjs) and rolled back when the audit flags it.

import { existsSync, mkdirSync, readFileSync, readdirSync, writeFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { boundsOf, braceBlock, matchBlock, parentOf, readResource, resolveChain } from "../actions/place-action-contexts.mjs";
import { meshOf, ownMesh, parseTree, prefabOf } from "../actions/prop-on-surface.mjs";
import { LIMITS, analyse, auditRecords, doorZone, insideFraction, isKind, measureTown, obbAabb, obbPenetration, parseDoor, parseKv, parseRoads, parseRow, polylineDistance, readJson } from "../../agent-box/place-audit.mjs";

export const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "../..");
const ADDON = join(ROOT, "addons", "LifeFramework");
const LAYERS_DIR = join(ADDON, "Worlds", "DebugWorld", "DebugWorld_Layers");
export const CACHE_FILE = join(ROOT, "tools", "place", "surface-cache.json");
const TMP = join(ROOT, "tmp", "place");

// Tolerances (metres). FLAT: how far heights under one footprint may differ and still be one surface.
// REST: how far the audit may measure a placed prop's bottom from its support. INSET: kept clear of an edge.
export const TOL = { flat: 0.03, rest: 0.05, inset: 0.03, wallGap: 0.02, wallFlat: 0.05, groundFlat: 0.25, overlap: LIMITS.overlap };

// A placement the tool refuses, with the reason an agent can act on.
export class PlaceError extends Error {}

// ------------------------------------------------------------------------------ maths

const D2R = Math.PI / 180;
const r3 = (n) => Math.round(n * 1000) / 1000;
const r2 = (n) => Math.round(n * 100) / 100;

// Enfusion yaw (angleY, degrees): 0 faces +z (north), 90 faces +x (east). Local (lx, lz) -> world offset.
export function rot(yaw, lx, lz) {
  const c = Math.cos(yaw * D2R);
  const s = Math.sin(yaw * D2R);
  return [lx * c + lz * s, -lx * s + lz * c];
}

// World offset -> local (the inverse of rot).
export function unrot(yaw, dx, dz) {
  const c = Math.cos(yaw * D2R);
  const s = Math.sin(yaw * D2R);
  return [dx * c - dz * s, dx * s + dz * c];
}

export function normYaw(a) {
  let v = ((a % 360) + 360) % 360;
  if (v > 180) v -= 360;
  return r3(v) === -180 ? 180 : r3(v);
}

export const headingOf = (dx, dz) => Math.atan2(dx, dz) / D2R;

const COMPASS = { north: 0, n: 0, east: 90, e: 90, south: 180, s: 180, west: 270, w: 270, "north-east": 45, ne: 45, "south-east": 135, se: 135, "south-west": 225, sw: 225, "north-west": 315, nw: 315 };
const FRONT = { "+z": 0, "+x": 90, "-z": 180, "-x": 270 };

// "north", "sw" or a number of degrees -> a world heading.
export function parseHeading(text) {
  const key = String(text).toLowerCase();
  if (key in COMPASS) return COMPASS[key];
  const n = Number(text);
  if (!Number.isFinite(n)) throw new PlaceError(`'${text}' is not a heading (north, south-east, ... or degrees: 0 = +z, 90 = +x)`);
  return n;
}

// The entity yaw that points the prop's front (a local axis) at a world heading.
export function yawForFacing(heading, front = "+z") {
  if (!(front in FRONT)) throw new PlaceError(`--front must be one of ${Object.keys(FRONT).join(", ")}`);
  return normYaw(heading - FRONT[front]);
}

// Footprint corners (x, z) relative to the pivot, for mesh bounds at a scale and yaw.
export function footprintCorners(bounds, scale, yaw) {
  const out = [];
  for (const lx of [bounds.min[0], bounds.max[0]]) for (const lz of [bounds.min[2], bounds.max[2]]) out.push(rot(yaw, lx * scale, lz * scale));
  return out;
}

// Axis-aligned rectangle of those corners: { min: [x, z], max: [x, z] }.
export function footprintRect(bounds, scale, yaw) {
  const pts = footprintCorners(bounds, scale, yaw);
  return { min: [Math.min(...pts.map((p) => p[0])), Math.min(...pts.map((p) => p[1]))], max: [Math.max(...pts.map((p) => p[0])), Math.max(...pts.map((p) => p[1]))] };
}

// The oriented box the audit uses, from a pose and mesh bounds.
export function boxOf({ x, y, z, yaw = 0, scale = 1, bounds }) {
  const mid = bounds.min.map((v, i) => ((v + bounds.max[i]) / 2) * scale);
  const h = bounds.min.map((v, i) => ((bounds.max[i] - v) / 2) * scale);
  const [dx, dz] = rot(yaw, mid[0], mid[2]);
  const [ax, az] = rot(yaw, 1, 0);
  const [bx, bz] = rot(yaw, 0, 1);
  return { c: [x + dx, y + mid[1], z + dz], h, a: [[ax, 0, az], [0, 1, 0], [bx, 0, bz]] };
}

const boxVolume = (b) => 8 * b.h[0] * b.h[1] * b.h[2];

// ------------------------------------------------------------------- prefabs and layers

const baseOf = (path) => String(path || "").split("/").pop().replace(/\.et$/, "");

// What a prefab resolves to through its parents: entity class, mesh, mesh bounds, root scale, and whether
// its rigid body is simulated (a pickup with `Active Active` rolls and tips; a `Static 1` prop stays).
const infoCache = new Map();
export function prefabInfo(root, path) {
  const key = `${root}|${path}`;
  if (infoCache.has(key)) return infoCache.get(key);
  const info = { path, cls: "GenericEntity", mesh: null, bounds: null, scale: 1, dynamic: false, found: false };
  try {
    let text = readResource(root, path);
    info.found = Boolean(text);
    if (text) {
      info.cls = (text.match(/^\s*(\w+)/) || [])[1] || "GenericEntity";
      info.mesh = meshOf(root, { header: `X : "{0000000000000000}${path}"`, props: {}, kids: [] }, null);
      let scale = null;
      let body = null;
      const seen = new Set();
      for (let p = path; text && !seen.has(p); ) {
        seen.add(p);
        if (scale == null) scale = (text.match(/^ scale\s+(-?\d+(?:\.\d+)?)/m) || [])[1] ?? null;
        if (body == null) {
          const rb = matchBlock(text, /RigidBody\s*"\{[0-9A-Fa-f]{16}\}"\s*\{/);
          if (rb && /\b(?:Active|Static)\b/.test(rb.body)) body = rb.body;
        }
        p = parentOf(text);
        text = p ? readResource(root, p) : null;
      }
      if (scale != null) info.scale = Number(scale);
      info.dynamic = Boolean(body) && /\bActive\s+Active\b/.test(body) && !/\bStatic\s+1\b/.test(body);
    } else {
      info.mesh = meshOf(root, { header: `X : "{0000000000000000}${path}"`, props: {}, kids: [] }, null);
    }
    info.bounds = info.mesh ? boundsOf(root, info.mesh) : null;
  } catch (e) {
    info.error = e.message;
  }
  infoCache.set(key, info);
  return info;
}

function walkFiles(dir, exts, out = []) {
  for (const entry of readdirSync(dir, { withFileTypes: true })) {
    const path = join(dir, entry.name);
    if (entry.isDirectory()) walkFiles(path, exts, out);
    else if (exts.some((x) => entry.name.endsWith(x))) out.push(path);
  }
  return out;
}

// "{GUID}Prefabs/X.et" from a full reference, a path, or a file name: a GUID is never invented, it is
// read from the prefab's .meta or from a reference the addon already holds.
let refIndex = null;
export function resolveRef(root, arg) {
  const full = String(arg).match(/^\{([0-9A-Fa-f]{16})\}(.+\.et)$/);
  if (full) return { guid: full[1].toUpperCase(), path: full[2] };
  const want = String(arg).replace(/\\/g, "/");
  const addon = join(root, "addons", "LifeFramework");
  if (want.includes("/") && existsSync(join(addon, `${want}.meta`))) {
    const m = readFileSync(join(addon, `${want}.meta`), "utf8").match(/Name\s+"\{([0-9A-Fa-f]{16})\}/);
    if (m) return { guid: m[1].toUpperCase(), path: want };
  }
  if (!refIndex) {
    refIndex = new Map();
    for (const file of walkFiles(addon, [".layer", ".et", ".conf", ".c", ".meta"])) {
      for (const m of readFileSync(file, "utf8").matchAll(/\{([0-9A-Fa-f]{16})\}([A-Za-z0-9_\-./ ]+\.et)/g)) {
        if (!refIndex.has(m[2])) refIndex.set(m[2], m[1].toUpperCase());
      }
    }
  }
  const name = want.endsWith(".et") ? want : `${want}.et`;
  const hits = [...refIndex.keys()].filter((p) => p === name || p.endsWith(`/${name}`));
  if (hits.length === 1) return { guid: refIndex.get(hits[0]), path: hits[0] };
  if (hits.length > 1) throw new PlaceError(`'${arg}' matches ${hits.length} prefabs (${hits.slice(0, 4).join(", ")}); give the path`);
  throw new PlaceError(`no GUID known for '${arg}': the addon does not reference it yet. Pass the full "{GUID}Prefabs/....et" (asset_search gives the path; the GUID is in Workbench's resource name or any base-game prefab that uses it)`);
}

function headerInfo(header, inGroup) {
  const h = header.replace(/^\$grp\s+/, "").trim();
  const ref = h.match(/:\s*"\{([0-9A-Fa-f]{16})\}([^"]+)"\s*$/);
  const words = h.replace(/:\s*".*$/, "").trim().split(/\s+/).filter(Boolean);
  const out = { cls: "", name: "", prefab: ref ? ref[2] : "" };
  if (words.length >= 2) [out.cls, out.name] = words;
  else if (words.length === 1) {
    if (ref || !inGroup) out.cls = words[0];
    else out.name = words[0];
  }
  return out;
}

// Every entity a text places (a .layer, or a prefab's own children with `prefabRoot`), with world pose,
// mesh bounds and scale. A child's coords are relative to its parent entity.
export function modelOf(root, text, file, { prefabRoot = null } = {}) {
  const ents = [];
  const add = (block, par, group, head) => {
    const c = block.props.coords.split(/\s+/).map(Number);
    const yawLocal = block.props.angleY ? Number(block.props.angleY) : 0;
    const [dx, dz] = par ? rot(par.yaw, c[0], c[2]) : [c[0], c[2]];
    const prefab = head.prefab || group?.prefab || "";
    const info = prefab ? prefabInfo(root, prefab) : null;
    const mesh = ownMesh(block) || info?.mesh || null;
    const ent = {
      file,
      name: head.name,
      cls: head.cls || group?.cls || info?.cls || "GenericEntity",
      prefab,
      x: (par ? par.x : 0) + dx,
      y: (par ? par.y : 0) + c[1],
      z: (par ? par.z : 0) + dz,
      yaw: (par ? par.yaw : 0) + yawLocal,
      scale: block.props.scale ? Number(block.props.scale) : info?.scale ?? 1,
      tilted: Boolean(Number(block.props.angleX || 0) || Number(block.props.angleZ || 0)),
      mesh,
      bounds: mesh ? safeBounds(root, mesh) : null,
      dynamic: Boolean(info?.dynamic),
      parent: par,
      coordsText: block.props.coords.trim(),
    };
    ents.push(ent);
    return ent;
  };
  const visit = (block, par, group) => {
    for (const k of block.kids) {
      if (k.header === "components") continue;
      if (k.header.startsWith("$grp")) {
        const g = headerInfo(k.header, false);
        visit(k, par, { cls: g.cls, prefab: g.prefab });
        continue;
      }
      if (!k.props.coords) {
        visit(k, par, group);
        continue;
      }
      visit(k, add(k, par, group, headerInfo(k.header, Boolean(group))), null);
    }
  };
  const tree = parseTree(text);
  if (prefabRoot) {
    for (const k of tree.kids) {
      const head = headerInfo(k.header, false);
      const info = prefabInfo(root, prefabRoot);
      const mesh = ownMesh(k) || info.mesh;
      const ent = { file, name: "root", cls: head.cls, prefab: prefabRoot, x: 0, y: 0, z: 0, yaw: 0, scale: 1, tilted: false, mesh, bounds: mesh ? safeBounds(root, mesh) : null, dynamic: false, parent: null, isPrefabRoot: true };
      ents.push(ent);
      visit(k, ent, null);
    }
  } else visit(tree, null, null);
  return ents;
}

function safeBounds(root, mesh) {
  try {
    return boundsOf(root, mesh);
  } catch {
    return null;
  }
}

export function loadModel(root = ROOT) {
  const dir = join(root, "addons", "LifeFramework", "Worlds", "DebugWorld", "DebugWorld_Layers");
  const ents = [];
  for (const file of readdirSync(dir)) {
    if (file.endsWith(".layer")) ents.push(...modelOf(root, readFileSync(join(dir, file), "utf8"), file));
  }
  return ents;
}

const labelOf = (e) => e.name || baseOf(e.prefab) || e.cls;
// What LF_PlacementAudit.Label can call this entity in a ray or measurement line.
const labelsOf = (e) => new Set([e.name, baseOf(e.prefab), e.mesh ? e.mesh.split("/").pop().replace(/\.xob$/, "") : ""].filter(Boolean));

// An entity by name, or an unnamed one by its prefab file name: "Villa_E_2I01_Police", or the one of
// several nearest a point: "LampStreet_E_01_6m@196,255.4".
export function findEntity(ents, spec) {
  const [key, at] = String(spec).split("@");
  const named = ents.filter((e) => e.name === key);
  if (named.length === 1) return named[0];
  if (named.length > 1) throw new PlaceError(`${named.length} entities are named ${key} (${named.map((e) => e.file).join(", ")})`);
  const byPrefab = ents.filter((e) => !e.name && baseOf(e.prefab) === key);
  if (!byPrefab.length) throw new PlaceError(`no entity named ${key}, and no unnamed entity of a prefab ${key}.et, in the DebugWorld layers`);
  if (byPrefab.length === 1 && !at) return byPrefab[0];
  if (!at) throw new PlaceError(`${byPrefab.length} unnamed entities use ${key}.et; say which with ${key}@x,z (the nearest one is taken)`);
  const p = at.split(",").map(Number);
  if (p.length !== 2 || p.some((v) => !Number.isFinite(v))) throw new PlaceError(`'${spec}': the point after @ must be x,z`);
  const best = byPrefab.reduce((a, b) => (Math.hypot(b.x - p[0], b.z - p[1]) < Math.hypot(a.x - p[0], a.z - p[1]) ? b : a));
  if (Math.hypot(best.x - p[0], best.z - p[1]) > 3) throw new PlaceError(`no ${key} within 3 m of ${at} (the nearest is at ${r2(best.x)}, ${r2(best.z)})`);
  return best;
}

// --------------------------------------------------------------------- layer text edits

const num = (v) => String(r3(v) === 0 ? 0 : r3(v));

// The text span of one entity: by name, or (unnamed) by its prefab reference and the coords line it has now.
function entityBlockOf(text, target) {
  const named = typeof target === "string";
  const what = named ? target : `${baseOf(target.prefab)} at ${target.coordsText}`;
  const spans = [];
  let offset = 0;
  for (const line of text.split("\n")) {
    const start = offset;
    offset += line.length + 1;
    const head = line.trim();
    if (!head.endsWith("{")) continue;
    const words = head.replace(/^\$grp\s+/, "").replace(/:\s*".*$/, "").replace(/\{$/, "").trim().split(/\s+/);
    const ref = (head.match(/:\s*"\{[0-9A-Fa-f]{16}\}([^"]+)"\s*\{$/) || [])[1] ?? "";
    // A named entity's header ends in its name (`Class Name : "ref" {`, or `Name {` inside a $grp).
    if (named ? words[words.length - 1] !== target || (words.length === 1 && ref) : ref !== target.prefab || words.length !== 1) continue;
    const open = start + line.lastIndexOf("{");
    const block = braceBlock(text, open);
    if (!block) throw new PlaceError(`unbalanced braces after ${what}`);
    if (!named && !text.slice(open, block.end).split("\n").some((l) => l.trim() === `coords ${target.coordsText}`)) continue;
    spans.push({ start, open, end: block.end });
  }
  if (spans.length !== 1) throw new PlaceError(`${spans.length} entity blocks match ${what}; expected one${named ? "" : " (name the entity in the layer to address it)"}`);
  return spans[0];
}

// Rewrite one entity's `coords` and `angleY` lines (in its parent's frame); every other byte stays.
export function setTransform(text, target, { coords, yaw }) {
  const name = typeof target === "string" ? target : baseOf(target.prefab);
  const span = entityBlockOf(text, target);
  const body = text.slice(span.open, span.end);
  const lines = body.split("\n");
  let depth = 0;
  let coordsAt = -1;
  let yawAt = -1;
  for (let i = 0; i < lines.length; i++) {
    const line = lines[i];
    if (depth === 1 && /^\s*coords\s/.test(line)) coordsAt = i;
    if (depth === 1 && /^\s*angleY\s/.test(line)) yawAt = i;
    depth += (line.match(/\{/g) || []).length - (line.match(/\}/g) || []).length;
  }
  if (coordsAt < 0) throw new PlaceError(`${name} has no coords line`);
  const indent = lines[coordsAt].match(/^\s*/)[0];
  lines[coordsAt] = `${indent}coords ${coords.map(num).join(" ")}`;
  const yawText = num(normYaw(yaw));
  if (yawAt >= 0) lines[yawAt] = `${indent}angleY ${yawText}`;
  else if (yawText !== "0") lines.splice(coordsAt + 1, 0, `${indent}angleY ${yawText}`);
  return text.slice(0, span.open) + lines.join("\n") + text.slice(span.end);
}

// Append one top-level entity in the form Workbench writes: class, name, prefab reference, coords, angleY, scale.
export function appendEntity(text, { cls, name, guid, path, coords, yaw, scale }) {
  const head = `${cls}${name ? ` ${name}` : ""} : "{${guid}}${path}" {`;
  const lines = [head, ` coords ${coords.map(num).join(" ")}`];
  if (num(normYaw(yaw)) !== "0") lines.push(` angleY ${num(normYaw(yaw))}`);
  if (scale != null) lines.push(` scale ${num(scale)}`);
  lines.push("}");
  return `${text}${text.endsWith("\n") || text === "" ? "" : "\n"}${lines.join("\n")}\n`;
}

// --------------------------------------------------------------------- surface tops

// Probe grid over a surface mesh's plan, in unscaled local coordinates.
export function surfaceGrid(bounds) {
  const sx = bounds.max[0] - bounds.min[0];
  const sz = bounds.max[2] - bounds.min[2];
  const nx = Math.max(3, Math.min(16, Math.round(sx / 0.08)));
  const nz = Math.max(3, Math.min(16, Math.round(sz / 0.08)));
  const cells = [];
  for (let j = 0; j < nz; j++) for (let i = 0; i < nx; i++) cells.push({ i, j, lx: bounds.min[0] + ((i + 0.5) * sx) / nx, lz: bounds.min[2] + ((j + 0.5) * sz) / nz });
  return { nx, nz, cells };
}

// Where a surface really is, from its probed cells (a number: the surface's own top there, local metres;
// "o": something stands there; null: no surface, the ray went past) or, with no probe, from mesh bounds.
// The working top is the most common height; the usable rectangle is the extent of the cells at it, so a
// workbench's raised back or a counter's lip is outside it.
export function surfaceTop(bounds, cache) {
  const full = { y: bounds.max[1], rect: { minX: bounds.min[0], maxX: bounds.max[0], minZ: bounds.min[2], maxZ: bounds.max[2] }, source: "mesh bounds", cells: [] };
  if (!cache?.h) return full;
  const { nx, nz, cells } = surfaceGrid(bounds);
  if (cache.nx !== nx || cache.nz !== nz) return full;
  const heights = cache.h.filter((v) => typeof v === "number");
  if (!heights.length) return { ...full, source: "mesh bounds (the probe found no collision on its top)" };
  const votes = new Map();
  for (const v of heights) votes.set(Math.round(v * 100), (votes.get(Math.round(v * 100)) ?? 0) + 1);
  const best = [...votes.entries()].sort((a, b) => b[1] - a[1] || b[0] - a[0])[0][0] / 100;
  const level = heights.filter((v) => Math.abs(v - best) <= TOL.flat);
  const y = level.reduce((a, b) => a + b, 0) / level.length;
  const cw = (bounds.max[0] - bounds.min[0]) / nx / 2;
  const cd = (bounds.max[2] - bounds.min[2]) / nz / 2;
  const rect = { minX: Infinity, maxX: -Infinity, minZ: Infinity, maxZ: -Infinity };
  const out = [];
  cells.forEach((c, k) => {
    const v = cache.h[k];
    out.push({ ...c, h: v });
    if (typeof v === "number" && Math.abs(v - y) <= TOL.flat) {
      rect.minX = Math.min(rect.minX, c.lx - cw);
      rect.maxX = Math.max(rect.maxX, c.lx + cw);
      rect.minZ = Math.min(rect.minZ, c.lz - cd);
      rect.maxZ = Math.max(rect.maxZ, c.lz + cd);
    }
  });
  // A cell with a prop on it is surface too when it lies between working cells or at the mesh edge; the
  // probe cannot see under it, so the rectangle grows over occupied cells that touch it.
  let grew = true;
  while (grew) {
    grew = false;
    for (const c of out) {
      if (c.h !== "o") continue;
      const touches = c.lx + cw >= rect.minX - 1e-6 && c.lx - cw <= rect.maxX + 1e-6 && c.lz + cd >= rect.minZ - 1e-6 && c.lz - cd <= rect.maxZ + 1e-6;
      const inside = c.lx - cw >= rect.minX - 1e-6 && c.lx + cw <= rect.maxX + 1e-6 && c.lz - cd >= rect.minZ - 1e-6 && c.lz + cd <= rect.maxZ + 1e-6;
      if (touches && !inside) {
        rect.minX = Math.min(rect.minX, c.lx - cw);
        rect.maxX = Math.max(rect.maxX, c.lx + cw);
        rect.minZ = Math.min(rect.minZ, c.lz - cd);
        rect.maxZ = Math.max(rect.maxZ, c.lz + cd);
        grew = true;
      }
    }
  }
  return { y, rect, source: "probe", cells: out };
}

// Turn probe rays over a surface's grid into the cache entry.
export function cacheFromRays(surface, hits) {
  const { nx, nz, cells } = surfaceGrid(surface.bounds);
  const names = labelsOf(surface);
  const height = (surface.bounds.max[1] - surface.bounds.min[1]) * surface.scale;
  const h = cells.map((c, k) => {
    const ray = hits[k];
    if (!ray || ray.d >= ray.len - 0.001) return null;
    const local = (ray.p[1] - surface.y) / surface.scale;
    if (names.has(ray.hit) || names.has(ray.root)) return r3(local);
    return ray.p[1] > surface.y + surface.bounds.min[1] * surface.scale + height * 0.3 ? "o" : null;
  });
  return { nx, nz, h };
}

// ------------------------------------------------------------------------- primitives

// Rest a prop on a surface: its mesh base on the surface's real top, its whole footprint inside the edges.
//   surface { x, y, z, yaw, scale, bounds }   prop { bounds, scale }
//   opts.at: "center" | compass word | [u, v] in -1..1 of the surface's own x and z; opts.near: [x, z] world;
//   opts.center: [lx, lz] local metres (exact, for arrange); opts.yaw: relative to the surface.
export function restOn(surface, prop, opts = {}) {
  const top = opts.top ?? surfaceTop(surface.bounds, null);
  const s = surface.scale ?? 1;
  const relYaw = opts.yaw ?? 0;
  const inset = opts.inset ?? TOL.inset;
  const fp = footprintRect(prop.bounds, prop.scale, relYaw);
  const size = [fp.max[0] - fp.min[0], fp.max[1] - fp.min[1]];
  const rect = { minX: top.rect.minX * s + inset, maxX: top.rect.maxX * s - inset, minZ: top.rect.minZ * s + inset, maxZ: top.rect.maxZ * s - inset };
  const room = [rect.maxX - rect.minX, rect.maxZ - rect.minZ];
  if (!opts.allowOverhang && (size[0] > room[0] + 1e-6 || size[1] > room[1] + 1e-6)) {
    throw new PlaceError(`does not fit: the footprint is ${r2(size[0])} x ${r2(size[1])} m and the top has ${r2(room[0])} x ${r2(room[1])} m (${top.source}). Turn it (--yaw 90), scale it, or pick a larger surface`);
  }
  const lo = [rect.minX + size[0] / 2, rect.minZ + size[1] / 2];
  const hi = [rect.maxX - size[0] / 2, rect.maxZ - size[1] / 2];
  const clamp = (v, i) => (lo[i] > hi[i] ? (lo[i] + hi[i]) / 2 : Math.max(lo[i], Math.min(hi[i], v)));
  let centre;
  if (opts.center) {
    centre = opts.center;
    if (!opts.allowOverhang && (centre[0] < lo[0] - 1e-6 || centre[0] > hi[0] + 1e-6 || centre[1] < lo[1] - 1e-6 || centre[1] > hi[1] + 1e-6)) throw new PlaceError("that spot puts the footprint over the edge");
  } else if (opts.near) {
    const [lx, lz] = unrot(surface.yaw, opts.near[0] - surface.x, opts.near[1] - surface.z);
    centre = [clamp(lx, 0), clamp(lz, 1)];
  } else {
    let uv = [0, 0];
    const at = opts.at ?? "center";
    if (Array.isArray(at)) uv = at;
    else if (at !== "center" && at !== "centre") {
      const head = parseHeading(at);
      const [lx, lz] = unrot(surface.yaw, Math.sin(head * D2R), Math.cos(head * D2R));
      uv = [Math.abs(lx) > 0.38 ? Math.sign(lx) : 0, Math.abs(lz) > 0.38 ? Math.sign(lz) : 0];
    }
    centre = [clamp((lo[0] + hi[0]) / 2 + (uv[0] * (hi[0] - lo[0])) / 2, 0), clamp((lo[1] + hi[1]) / 2 + (uv[1] * (hi[1] - lo[1])) / 2, 1)];
  }
  // Heights the probe saw under the footprint must be one level.
  const under = top.cells.filter((c) => typeof c.h === "number" && c.lx * s >= centre[0] - size[0] / 2 && c.lx * s <= centre[0] + size[0] / 2 && c.lz * s >= centre[1] - size[1] / 2 && c.lz * s <= centre[1] + size[1] / 2);
  const off = under.filter((c) => Math.abs(c.h - top.y) > TOL.flat);
  if (off.length && !opts.allowOverhang) throw new PlaceError(`the top is not level under that footprint (${r2(Math.min(...under.map((c) => c.h)))} to ${r2(Math.max(...under.map((c) => c.h)))} m): a raised back, a lip or a hole. Pick another --at`);
  const pivot = [centre[0] - (fp.min[0] + fp.max[0]) / 2, centre[1] - (fp.min[1] + fp.max[1]) / 2];
  const [dx, dz] = rot(surface.yaw, pivot[0], pivot[1]);
  return {
    x: surface.x + dx,
    y: surface.y + top.y * s - prop.bounds.min[1] * prop.scale,
    z: surface.z + dz,
    yaw: normYaw(surface.yaw + relYaw),
    local: { centre, size, rect },
    notes: [`top ${r3(surface.y + top.y * s)} m from ${top.source}; footprint ${r2(size[0])} x ${r2(size[1])} m in ${r2(room[0])} x ${r2(room[1])} m`],
  };
}

// The outward normal of the side of `target` that faces a world point or a compass heading: one of its
// own four horizontal axes, so a turned building still gets a square face.
export function faceNormal(target, { near, face }) {
  const box = boxOf(target);
  let local;
  if (face != null) {
    const head = parseHeading(face);
    local = unrot(target.yaw, Math.sin(head * D2R), Math.cos(head * D2R));
  } else {
    const [lx, lz] = unrot(target.yaw, near[0] - box.c[0], near[1] - box.c[2]);
    local = [lx / Math.max(box.h[0], 0.01), lz / Math.max(box.h[2], 0.01)];
  }
  const nl = Math.abs(local[0]) >= Math.abs(local[1]) ? [Math.sign(local[0]) || 1, 0] : [0, Math.sign(local[1]) || 1];
  const n = rot(target.yaw, nl[0], nl[1]);
  return { n, t: [n[1], -n[0]], half: nl[0] ? [box.h[0], box.h[2]] : [box.h[2], box.h[0]], centre: [box.c[0], box.c[2]] };
}

// Stand or hang a prop flush against a face, from probe rays.
//   rays: [{ lat, h, d }]   horizontal rays from the start line toward the face: lateral offset (m), absolute
//                           height, distance to the first surface (Infinity when clear)
//   floors: [{ dist, y }]   floor heights along the centre line, by distance from the start toward the face
// Returns the pivot as (along: metres from the start toward the face, lat: metres sideways, y).
export function againstWall({ rays, floors, prop, yaw, n, hang = null, gap = TOL.wallGap, allowUneven = false }) {
  const t = [n[1], -n[0]];
  const corners = footprintCorners(prop.bounds, prop.scale, yaw);
  const pn = corners.map((c) => c[0] * n[0] + c[1] * n[1]);
  const pt = corners.map((c) => c[0] * t[0] + c[1] * t[1]);
  const back = Math.min(...pn);
  const depth = Math.max(...pn) - back;
  const latMid = (Math.min(...pt) + Math.max(...pt)) / 2;
  const height = (prop.bounds.max[1] - prop.bounds.min[1]) * prop.scale;
  const floorAt = (dist) => {
    const ok = floors.filter((f) => Number.isFinite(f.y));
    if (!ok.length) throw new PlaceError("no floor found in front of that face");
    return ok.reduce((a, b) => (Math.abs(b.dist - dist) < Math.abs(a.dist - dist) ? b : a));
  };
  if (!rays.some((r) => Number.isFinite(r.d))) throw new PlaceError("no surface within reach on that side: the rays hit nothing. Start nearer (--near) or pick another face");
  // The face is the farthest surface the rays reach. A hit nearer than the prop's own front is something
  // standing in front of where the prop goes; a hit inside the prop's depth is a part of the face that
  // sticks out (a plinth, a post, a frame), and a ray that reaches nothing is an opening.
  const split = (set) => {
    const far = Math.max(...set.filter((r) => Number.isFinite(r.d)).map((r) => r.d));
    const front = far - gap - depth - 0.05;
    return { far, inFront: set.filter((r) => r.d < front), behind: set.filter((r) => r.d >= front) };
  };
  const mid = (Math.min(...rays.map((r) => r.h)) + Math.max(...rays.map((r) => r.h))) / 2;
  let d = split(rays.filter((r) => r.h >= mid && Number.isFinite(r.d)).length ? rays.filter((r) => r.h >= mid) : rays).far;
  let floor = null;
  let parts = null;
  for (let pass = 0; pass < 3; pass++) {
    floor = floorAt(d - gap - depth / 2);
    const bottom = floor.y + (hang ?? 0);
    const level = rays.filter((r) => r.h >= bottom + 0.05 && r.h <= bottom + height - 0.05);
    if (!level.length) throw new PlaceError(`the floor there is at ${r2(floor.y)} m, outside the probed heights; pass --floor ${r2(floor.y)}`);
    if (!level.some((r) => Number.isFinite(r.d))) throw new PlaceError("nothing behind the prop at that height: the rays went through (an opening, or the face ends). Move along the face");
    parts = split(level);
    d = parts.far;
  }
  const notes = [`face ${r3(d)} m from the start, floor ${r3(floor.y)} m`];
  const describe = (set) => {
    const by = new Map();
    for (const r of set) {
      const k = Number.isFinite(r.d) ? r.label || "a surface" : "nothing";
      by.set(k, [Math.min(by.get(k)?.[0] ?? Infinity, r.d), Math.max(by.get(k)?.[1] ?? -Infinity, r.d)]);
    }
    return [...by.entries()].map(([k, v]) => (k === "nothing" ? "open in places" : `${k} at ${r2(v[0])}${v[1] - v[0] > 0.005 ? ` to ${r2(v[1])}` : ""} m`)).join(", ");
  };
  if (parts.inFront.length > (parts.inFront.length + parts.behind.length) * 0.4) throw new PlaceError(`most rays stop before the face (${describe(parts.inFront)}): something stands between the start and the face. Start nearer the face (--near)`);
  if (parts.inFront.length) notes.push(`in front of it: ${describe(parts.inFront)} from the start`);
  const near = Math.min(...parts.behind.map((r) => r.d));
  const open = parts.behind.some((r) => !Number.isFinite(r.d));
  if (d - near > TOL.wallFlat || open) {
    const what = describe(parts.behind);
    if (!allowUneven) throw new PlaceError(`the face is not flat behind the prop (${what}): a window, a door, a plinth or a post. Move along the face (--near / --along), raise it (--hang), or pass --allow-uneven to stand off the nearest part`);
    notes.push(`uneven face (${what}): standing off the nearest part`);
    d = near;
  }
  if (hang == null) {
    const span = floors.filter((f) => Number.isFinite(f.y) && f.dist >= d - gap - depth - 0.05 && f.dist <= d - gap + 0.05);
    const ys = span.map((f) => f.y);
    if (ys.length && Math.max(...ys) - Math.min(...ys) > TOL.wallFlat && !allowUneven) throw new PlaceError(`the floor under the prop is not level (${r2(Math.min(...ys))} to ${r2(Math.max(...ys))} m): a step, a kerb or something standing there`);
  }
  return { along: d - gap + back, lat: -latMid, y: floor.y + (hang ?? 0) - prop.bounds.min[1] * prop.scale, depth, notes };
}

// Stand a prop on the ground from height samples under its centre (first) and footprint corners.
export function onGround({ samples, prop }) {
  const ys = samples.map((s) => s.y).filter(Number.isFinite);
  if (!ys.length) throw new PlaceError("no ground found there");
  const spread = Math.max(...ys) - Math.min(...ys);
  if (spread > TOL.groundFlat) throw new PlaceError(`the ground under the footprint is not level (${r2(Math.min(...ys))} to ${r2(Math.max(...ys))} m): a slope, a kerb or something standing there`);
  return { y: samples[0].y - prop.bounds.min[1] * prop.scale, notes: [`ground ${r3(samples[0].y)} m (${r2(spread)} m across the footprint)`] };
}

// Lay props side by side along a surface's longer usable side, centred, `spacing` apart, none over an edge.
// Returns one restOn result per prop. `row` (-1..1) shifts the row across the other side.
export function arrangeOn(surface, props, opts = {}) {
  const top = opts.top ?? surfaceTop(surface.bounds, null);
  const s = surface.scale ?? 1;
  const inset = opts.inset ?? TOL.inset;
  const spacing = opts.spacing ?? 0.05;
  const room = [(top.rect.maxX - top.rect.minX) * s - inset * 2, (top.rect.maxZ - top.rect.minZ) * s - inset * 2];
  const axis = room[0] >= room[1] ? 0 : 1;
  const sizes = props.map((p) => {
    let yaw = opts.yaw ?? 0;
    let fp = footprintRect(p.bounds, p.scale, yaw);
    let size = [fp.max[0] - fp.min[0], fp.max[1] - fp.min[1]];
    // Too deep for the top but not when turned: turn it.
    if (opts.yaw == null && size[1 - axis] > room[1 - axis] && size[axis] <= room[1 - axis]) {
      yaw = 90;
      fp = footprintRect(p.bounds, p.scale, yaw);
      size = [fp.max[0] - fp.min[0], fp.max[1] - fp.min[1]];
    }
    return { yaw, size };
  });
  const total = sizes.reduce((a, b) => a + b.size[axis], 0) + spacing * (props.length - 1);
  const deepest = Math.max(...sizes.map((x) => x.size[1 - axis]));
  if (total > room[axis] + 1e-6 || deepest > room[1 - axis] + 1e-6) {
    throw new PlaceError(`they do not fit: ${props.length} side by side need ${r2(total)} x ${r2(deepest)} m and the top has ${r2(room[axis])} x ${r2(room[1 - axis])} m (${top.source}). Place fewer, scale them, or use a larger surface`);
  }
  const mid = [((top.rect.minX + top.rect.maxX) / 2) * s, ((top.rect.minZ + top.rect.maxZ) / 2) * s];
  let cursor = mid[axis] - total / 2;
  return props.map((p, i) => {
    const centre = [0, 0];
    centre[axis] = cursor + sizes[i].size[axis] / 2;
    centre[1 - axis] = mid[1 - axis] + ((opts.row ?? 0) * (room[1 - axis] - sizes[i].size[1 - axis])) / 2;
    cursor += sizes[i].size[axis] + spacing;
    return restOn(surface, p, { top, inset, yaw: sizes[i].yaw, center: centre });
  });
}

// Evenly spaced centres from a to b for footprints of the given widths along the row; throws when neighbours would touch.
export function rowCentres(a, b, widths, spacing = 0.05) {
  const len = Math.hypot(b[0] - a[0], b[1] - a[1]);
  const n = widths.length;
  if (n === 1) return [[(a[0] + b[0]) / 2, (a[1] + b[1]) / 2]];
  const step = len / (n - 1);
  for (let i = 0; i + 1 < n; i++) {
    const need = (widths[i] + widths[i + 1]) / 2 + spacing;
    if (step < need - 1e-6) throw new PlaceError(`the row is too short: ${n} props need ${r2(need)} m between centres and get ${r2(step)} m. Lengthen the row or place fewer`);
  }
  return widths.map((_, i) => [a[0] + ((b[0] - a[0]) * i) / (n - 1), a[1] + ((b[1] - a[1]) * i) / (n - 1)]);
}

// --------------------------------------------------------------------- offline checks

const related = (a, b) => {
  for (let p = a; p; p = p.parent) if (p === b) return true;
  for (let p = b; p; p = p.parent) if (p === a) return true;
  return false;
};

const isUnder = (e, top) => {
  for (let p = e; p; p = p.parent) if (p === top) return true;
  return false;
};

// What is wrong with a candidate before anything is written: prop overlaps (mesh bounds as oriented boxes),
// carriageways, door swings and walk-ups, scale, and a simulated body that will not stay put.
//   cand { name, prefab, x, y, z, yaw, scale, bounds, dynamic, self?, support?, hung? }
export function offlineProblems(cand, { ents = [], roads = [], doors = [], allowRoad = false } = {}) {
  const out = [];
  const kind = { name: cand.name || "", prefab: cand.prefab || "" };
  const box = boxOf(cand);
  if (cand.scale < LIMITS.scaleMin || cand.scale > LIMITS.scaleMax) out.push(`scale ${r2(cand.scale)} is outside ${LIMITS.scaleMin} to ${LIMITS.scaleMax}: use a prefab of the right size`);
  if (cand.dynamic && cand.support) {
    const size = [box.h[0] * 2, box.h[1] * 2, box.h[2] * 2];
    if (size[1] > Math.min(size[0], size[2]) * 2) out.push(`${labelOf(cand)} is a simulated rigid body ${r2(size[1])} m tall on a ${r2(Math.min(size[0], size[2]))} m base: it tips over on a raised surface when a client simulates it. Lay it on the ground (place ground)`);
  }
  if (!isKind(kind, "vegetation") && !isKind(kind, "building") && boxVolume(box) <= 40) {
    for (const e of ents) {
      if (!e.bounds || e === cand.self || e === cand.support || (cand.self && related(e, cand.self))) continue;
      if (Math.abs(e.x - cand.x) > 12 || Math.abs(e.z - cand.z) > 12) continue;
      if (isKind(e, "vegetation") || isKind(e, "building") || isKind(e, "surface") || e.tilted) continue;
      if (isKind(kind, "chain") && isKind(e, "chain")) continue;
      const other = boxOf(e);
      if (boxVolume(other) > 40 || boxVolume(other) < 1e-4) continue;
      const depth = obbPenetration(box, other);
      if (depth <= TOL.overlap) continue;
      const [small, big, bigEnt] = boxVolume(box) <= boxVolume(other) ? [box, other, e] : [other, box, kind];
      if (isKind(bigEnt, "holder") && boxVolume(small) < boxVolume(big) * 0.5 && insideFraction(small, big) > 0.6) continue;
      if (obbAabb(small).min[1] >= obbAabb(big).max[1] - TOL.overlap * 1.5) continue;
      out.push(`overlaps ${labelOf(e)} [${e.file}] by ${r2(depth)} m`);
    }
  }
  if (!allowRoad && !isKind(kind, "vehicle") && !isKind(kind, "roadFurniture") && !isKind(kind, "building") && box.h[1] >= 0.05) {
    const pts = [[box.c[0], box.c[2]], ...footprintCorners(cand.bounds, cand.scale, cand.yaw).map((p) => [cand.x + p[0], cand.z + p[1]])];
    for (const r of roads) {
      if (r.width >= LIMITS.roadMaxWidth || /Market|Square|Apron|Plaza|Yard/i.test(r.name)) continue;
      const d = Math.min(...pts.map((p) => polylineDistance(p[0], p[1], r.pts)));
      if (d < r.width / 2 - LIMITS.roadMargin) {
        out.push(`stands on the carriageway of ${r.name} (${r2(d)} m from its centre line, ${r.width} m wide). Move it to the verge, ${r2(r.width / 2 - d)} m or more further out`);
        break;
      }
    }
  }
  if (!isKind(kind, "chain") && boxVolume(box) <= 40 && box.h[1] >= 0.25 && !cand.hung) {
    for (const d of doors) {
      if (Math.abs(d.box.c[0] - box.c[0]) > 15 || Math.abs(d.box.c[2] - box.c[2]) > 15) continue;
      const swing = doorZone(d.box);
      if (!swing) continue;
      if (obbPenetration(swing, box) > LIMITS.door) {
        out.push(`stands in the swing of a door of ${d.root} (door at ${r2(d.box.c[0])}, ${r2(d.box.c[2])})`);
        break;
      }
      const ground = d.box.c[1] - d.box.h[1] < 1.2;
      const home = ents.find((e) => e.bounds && labelsOf(e).has(d.root) && Math.hypot(e.x - d.rp[0], e.z - d.rp[2]) < 1.5);
      const hb = home ? obbAabb(boxOf(home)) : null;
      const indoors = hb && box.c[0] > hb.min[0] && box.c[0] < hb.max[0] && box.c[2] > hb.min[2] && box.c[2] < hb.max[2];
      if (ground && !indoors && obbPenetration(doorZone(d.box, LIMITS.doorWalk), box) > LIMITS.door) {
        out.push(`stands in the walk-up to a door of ${d.root} (within ${LIMITS.doorWalk} m in front of the door at ${r2(d.box.c[0])}, ${r2(d.box.c[2])})`);
        break;
      }
    }
  }
  return out;
}

// ---------------------------------------------------------------------------- server

function loadRoads(root) {
  try {
    return parseRoads(readFileSync(join(root, "addons", "LifeFramework", "Worlds", "DebugWorld", "DebugWorld_Layers", "Village_Roads.layer"), "utf8"));
  } catch {
    return [];
  }
}

function loadDoors() {
  return readJson(join(TMP, "doors.json"), { doors: [] }).doors ?? [];
}

export function parseRays(outText) {
  const rays = new Map();
  const doors = [];
  for (const raw of outText.split(/\r?\n/)) {
    const line = raw.trim();
    if (line.startsWith("door ")) doors.push(parseDoor(line.slice(5)));
    if (!line.startsWith("ray ")) continue;
    const sp = line.indexOf(" ", 4);
    const kv = parseKv(line.slice(sp + 1));
    rays.set(line.slice(4, sp), { d: Number(kv.d), len: Number(kv.len), hit: kv.hit, root: kv.root, p: String(kv.p).split(",").map(Number), sy: Number(kv.sy) });
  }
  return { rays, doors };
}

// One boot of the test server: the whole town measured (so the audit's pair checks see everything) plus the probe rays.
function serverRun(rayLines = []) {
  mkdirSync(TMP, { recursive: true });
  const records = auditRecords({ annex: true });
  const run = measureTown(records, { outDir: TMP, extra: rayLines });
  if (run.outText == null) throw new PlaceError("the test server wrote no measurements (lines above). Run it from a worktree; `tools\\cli test --no-build --only LF_Test_PlacementAuditTown` shows why");
  const { rays, doors } = parseRays(run.outText);
  if (doors.length) writeFileSync(join(TMP, "doors.json"), JSON.stringify({ at: new Date().toISOString(), doors }));
  const missingRays = rayLines.length - rays.size;
  if (missingRays > 0) throw new PlaceError(`the test server answered ${rays.size} of ${rayLines.length} probe rays: its scripts are older than this tool (run from a worktree at this commit)`);
  return { records, outText: run.outText, rays, doors };
}

// `exclude`: the name of the entity being re-placed, which the ray passes through.
const rayLine = (id, from, to, exclude = "") => `ray ${id} ${from.map(r3).join(" ")} ${to.map(r3).join(" ")}${exclude ? ` ${exclude}` : ""}`;

// The placement audit on the entities just written. Fails on any problem the audit ranks for one of them or
// that names one of them, on a bottom more than TOL.rest from its support, and on a yaw the engine read differently.
export function judge(targets, records, outText, { allow = [], areas = [], roads = [] } = {}) {
  const res = analyse(records, outText, { allow, areas, roads });
  const rows = new Map();
  for (const raw of outText.split(/\r?\n/)) {
    const sp = raw.indexOf(" ");
    if (sp > 0 && /^\d+$/.test(raw.slice(0, sp))) rows.set(Number(raw.slice(0, sp)), parseRow(raw.slice(sp + 1)));
  }
  const out = [];
  for (const t of targets) {
    const problems = [];
    const idx = records.findIndex((e) => (t.name ? e.name === t.name : !e.name && e.prefab === t.prefab) && Math.hypot(e.x - t.x, e.z - t.z) < 0.02);
    const m = rows.get(idx);
    const label = t.name || baseOf(t.prefab);
    if (idx < 0 || !m) problems.push("the audit did not list it (is its layer skipped?)");
    else if (m.missing) problems.push("not in the world at that position: the prefab reference did not load (wrong GUID or path?)");
    else {
      for (const r of res.ranked) {
        const mine = r.name === label && Math.hypot(r.x - t.x, r.z - t.z) < 0.02;
        for (const p of r.problems) {
          if (mine) problems.push(p.text);
          else if (new RegExp(`\\b${label}\\b`).test(p.text)) problems.push(`${r.name}: ${p.text}`);
        }
      }
      if (!t.hung && !m.point && Math.abs(m.gap) > TOL.rest && !problems.some((p) => /floating|sunk/.test(p))) problems.push(`its base is ${r2(m.gap)} m ${m.gap > 0 ? "above" : "below"} ${m.on}`);
      if (t.hung && !m.backed) problems.push("nothing is behind it within 0.3 m: it hangs in the air");
      if (!m.point && !m.character) {
        const want = rot(t.yaw, 0, 1);
        const got = m.box.a[2];
        if (Math.hypot(got[0] - want[0], got[2] - want[1]) > 0.05) problems.push(`the engine reads its facing as ${r2(headingOf(got[0], got[2]))} deg, not ${t.yaw}`);
      }
    }
    out.push({ target: t, problems, measure: m });
  }
  return out;
}

// --------------------------------------------------------------------------------- cli

const VALUED = new Set(["layer", "name", "at", "near", "yaw", "facing", "face-to", "face", "along", "hang", "front", "gap", "floor", "reach", "count", "spacing", "move", "in", "scale", "row", "y-hint", "inset"]);

function parseArgs(argv) {
  const out = { _: [], flags: {} };
  for (let i = 0; i < argv.length; i++) {
    const a = argv[i];
    if (a.startsWith("--")) {
      const k = a.slice(2);
      out.flags[k] = VALUED.has(k) ? argv[++i] : true;
    } else out._.push(a);
  }
  return out;
}

const pair = (text, what) => {
  const p = String(text ?? "").split(",").map(Number);
  if (p.length !== 2 || p.some((v) => !Number.isFinite(v))) throw new PlaceError(`${what} must be x,z (two numbers, comma, no space)`);
  return p;
};

// The working set of one command: where the entities live (the layers, or one prefab with --in).
function openWorld(flags) {
  if (flags.in) {
    const rel = String(flags.in).replace(/\\/g, "/").replace(/^addons\/LifeFramework\//, "");
    const abs = join(ADDON, ...rel.split("/"));
    if (!existsSync(abs)) throw new PlaceError(`no prefab ${rel} in the addon`);
    return { prefabFile: abs, rel, ents: modelOf(ROOT, readFileSync(abs, "utf8"), rel, { prefabRoot: rel }), world: loadModel(ROOT) };
  }
  const ents = loadModel(ROOT);
  return { ents, world: ents };
}

// The props a command places: new ones from prefab references, or existing entities with --move.
function itemsOf(ctx, prefabArgs, flags) {
  const items = [];
  if (flags.move) {
    for (const name of String(flags.move).split(",")) {
      const ent = findEntity(ctx.ents, name);
      if (!ent.bounds) throw new PlaceError(`${name} has no mesh bounds (${ent.prefab || "no prefab"}): nothing to rest on a surface`);
      if (ent.tilted) throw new PlaceError(`${name} is tilted (angleX/angleZ): the tool places upright props only`);
      items.push({ mode: "move", ent, name: ent.name, prefab: ent.prefab, bounds: ent.bounds, scale: flags.scale ? Number(flags.scale) : ent.scale, dynamic: ent.dynamic });
    }
    return items;
  }
  if (!prefabArgs.length) throw new PlaceError("give a prefab to place, or --move <Entity>");
  if (ctx.prefabFile) throw new PlaceError("--in re-places a prefab's existing children: use --move <Child>");
  if (!flags.layer) throw new PlaceError("--layer <File.layer> says which layer gets the new entity");
  if (!existsSync(join(LAYERS_DIR, String(flags.layer)))) throw new PlaceError(`no layer ${flags.layer} in DebugWorld_Layers`);
  const count = Number(flags.count ?? 1);
  const list = prefabArgs.length === 1 && count > 1 ? Array(count).fill(prefabArgs[0]) : prefabArgs;
  const names = flags.name ? String(flags.name).split(",") : [];
  list.forEach((arg, i) => {
    const ref = resolveRef(ROOT, arg);
    const info = prefabInfo(ROOT, ref.path);
    if (!info.bounds) throw new PlaceError(`${ref.path} has no mesh the tool can measure (${info.error || "no MeshObject in its chain"})`);
    const name = names.length === 1 && list.length > 1 ? `${names[0]}_${i + 1}` : names[i] ?? "";
    if (name && ctx.world.some((e) => e.name === name)) throw new PlaceError(`an entity named ${name} exists already; --move ${name} re-places it`);
    items.push({ mode: "new", ref, info, name, prefab: ref.path, layer: String(flags.layer), bounds: info.bounds, scale: flags.scale ? Number(flags.scale) : info.scale, setScale: flags.scale ? Number(flags.scale) : null, dynamic: info.dynamic });
  });
  return items;
}

function loadCache() {
  return readJson(CACHE_FILE, {});
}

// The surface's real top: from the cache, else by probing a world entity with that mesh (one server boot).
function topOf(ctx, surface, flags, { force = false } = {}) {
  if (!surface.bounds) throw new PlaceError(`${labelOf(surface)} has no mesh bounds: it is not a surface`);
  const cache = loadCache();
  if (cache[surface.mesh] && !force) return surfaceTop(surface.bounds, cache[surface.mesh]);
  const host = surface.isPrefabRoot || surface.tilted ? ctx.world.find((e) => e.mesh === surface.mesh && !e.tilted && (e.name || e.prefab)) : surface;
  if (flags["no-verify"] && !force) return surfaceTop(surface.bounds, null);
  if (!host) {
    console.log(`  no world entity uses ${surface.mesh}: its top comes from mesh bounds`);
    return surfaceTop(surface.bounds, null);
  }
  const { cells } = surfaceGrid(host.bounds);
  const topY = host.y + host.bounds.max[1] * host.scale + 0.4;
  const lowY = host.y + host.bounds.min[1] * host.scale - 0.1;
  const lines = cells.map((c, k) => {
    const [dx, dz] = rot(host.yaw, c.lx * host.scale, c.lz * host.scale);
    return rayLine(`s${k}`, [host.x + dx, topY, host.z + dz], [host.x + dx, lowY, host.z + dz]);
  });
  console.log(`  probing the top of ${labelOf(host)} (${cells.length} rays, one test-server boot)`);
  const run = serverRun(lines);
  const entry = cacheFromRays(host, cells.map((_, k) => run.rays.get(`s${k}`)));
  const next = loadCache();
  next[surface.mesh] = { ...entry, from: labelOf(host), at: new Date().toISOString().slice(0, 10) };
  const sorted = Object.fromEntries(Object.keys(next).sort().map((k) => [k, next[k]]));
  // One line per mesh: two branches that each probed a surface merge as two added lines.
  writeFileSync(CACHE_FILE, `{\n${Object.entries(sorted).map(([k, v]) => `${JSON.stringify(k)}: ${JSON.stringify(v)}`).join(",\n")}\n}\n`);
  return surfaceTop(surface.bounds, entry);
}

// The offline checks on every plan of one command. Prints each plan (unless `quiet`) and its problems; true when any has one.
function review(ctx, plans, flags, { quiet = false } = {}) {
  const roads = loadRoads(ROOT);
  const doors = ctx.doors ?? loadDoors();
  const staged = [];
  // Entities this command moves leave their old spots: they are not obstacles for each other.
  const moving = plans.map((p) => p.item.ent).filter(Boolean);
  let blocked = false;
  for (const p of plans) {
    const it = p.item;
    const cand = { name: it.name, prefab: it.prefab, x: p.x, y: p.y, z: p.z, yaw: p.yaw, scale: it.scale, bounds: it.bounds, dynamic: it.dynamic, self: it.ent ?? null, support: p.support ?? null, hung: Boolean(p.hung) };
    const others = (ctx.prefabFile ? ctx.ents : [...ctx.world, ...staged]).filter((e) => !moving.some((m) => m !== it.ent && isUnder(e, m)));
    const found = offlineProblems(cand, { ents: others, roads: ctx.prefabFile ? [] : roads, doors: ctx.prefabFile ? [] : doors, allowRoad: Boolean(flags["allow-road"]) });
    const label = it.name || baseOf(it.prefab);
    if (!quiet || found.length) console.log(`${it.mode === "move" ? "move" : "new "} ${label} -> coords ${num(p.x)} ${num(p.y)} ${num(p.z)} angleY ${num(p.yaw)}${it.scale !== 1 ? ` scale ${num(it.scale)}` : ""}`);
    if (!quiet) {
      for (const n of p.notes ?? []) console.log(`     ${n}`);
      if (it.dynamic) console.log("     simulated rigid body: clients may move it; confirm in a shot");
    }
    for (const f of found) console.log(`     PROBLEM ${f}`);
    if (found.length) blocked = true;
    staged.push({ ...cand, file: it.layer ?? it.ent?.file, self: undefined, parent: null, tilted: false, mesh: null });
  }
  return blocked;
}

// The offline checks, the write, the audit, and the rollback: shared by every placing command.
function commit(ctx, plans, flags) {
  const blocked = review(ctx, plans, flags);
  if (blocked && !flags.force) {
    console.log("place: not written (problems above). Fix the request, or --force to write anyway.");
    return 1;
  }
  if (flags.dry) {
    console.log("place: dry run, nothing written.");
    return 0;
  }
  // Write, keeping each file's original text for the rollback.
  const originals = new Map();
  const edit = (file, fn) => {
    if (!originals.has(file)) originals.set(file, readFileSync(file, "utf8"));
    writeFileSync(file, fn(readFileSync(file, "utf8")));
  };
  for (const p of plans) {
    const it = p.item;
    if (it.mode === "move") {
      const par = it.ent.parent;
      let coords = [p.x, p.y, p.z];
      let yaw = p.yaw;
      if (par) {
        const [lx, lz] = unrot(par.yaw, p.x - par.x, p.z - par.z);
        coords = [lx, p.y - par.y, lz];
        yaw = p.yaw - par.yaw;
      }
      edit(ctx.prefabFile ?? join(LAYERS_DIR, it.ent.file), (t) => setTransform(t, it.ent.name || it.ent, { coords, yaw }));
    } else {
      edit(join(LAYERS_DIR, it.layer), (t) => appendEntity(t, { cls: it.info.cls, name: it.name, guid: it.ref.guid, path: it.ref.path, coords: [p.x, p.y, p.z], yaw: p.yaw, scale: it.setScale }));
    }
  }
  const files = [...originals.keys()].map((f) => f.slice(ROOT.length + 1).replace(/\\/g, "/"));
  if (flags["no-verify"] || ctx.prefabFile) {
    console.log(`place: wrote ${files.join(", ")} (not audited${ctx.prefabFile ? ": a prefab's children are checked offline; validate-prop-on-surface covers them" : "; run tools\\cli place verify <names> before shipping"}).`);
    return 0;
  }
  const targets = plans.map((p) => ({ name: p.item.name, prefab: p.item.prefab, x: r3(p.x), y: r3(p.y), z: r3(p.z), yaw: p.yaw, hung: Boolean(p.hung) }));
  let verdict;
  try {
    verdict = runJudge(targets);
  } catch (e) {
    for (const [file, text] of originals) writeFileSync(file, text);
    throw e;
  }
  const bad = verdict.filter((v) => v.problems.length);
  for (const v of verdict) {
    const m = v.measure;
    const label = v.target.name || baseOf(v.target.prefab);
    if (!v.problems.length) console.log(`audit ${label}: clean (on ${m.on}, gap ${r3(m.gap)} m${m.backed ? ", backed" : ""})`);
    else for (const p of v.problems) console.log(`audit ${label}: ${p}`);
  }
  if (bad.length && !flags.keep) {
    for (const [file, text] of originals) writeFileSync(file, text);
    console.log(`place: the audit flagged ${bad.length} placement(s); ${files.join(", ")} restored. --keep leaves a flagged placement in the file.`);
    return 1;
  }
  console.log(`place: ${bad.length ? "kept (flagged)" : "placed and audited"} in ${files.join(", ")}.`);
  return bad.length ? 1 : 0;
}

function runJudge(targets) {
  const run = serverRun([]);
  const allow = readJson(join(ROOT, "agent-box", "place-audit-allow.json"), { allow: [] }).allow ?? [];
  const areas = readJson(join(ROOT, "agent-box", "areas.json"), { areas: [] }).areas ?? [];
  return judge(targets, run.records, run.outText, { allow, areas, roads: loadRoads(ROOT) });
}

function relYawOf(flags, surface) {
  if (flags.facing != null) return normYaw(yawForFacing(parseHeading(flags.facing), flags.front ?? "+z") - surface.yaw);
  return flags.yaw != null ? Number(flags.yaw) : 0;
}

function cmdOn(args, flags) {
  const [surfaceName, ...prefabs] = args;
  if (!surfaceName) throw new PlaceError("usage: place on <surface> <prefab> --layer L.layer | place on <surface> --move <Entity>");
  const ctx = openWorld(flags);
  const surface = findEntity(ctx.ents, surfaceName);
  const items = itemsOf(ctx, prefabs, flags);
  if (items.length !== 1) throw new PlaceError("place on takes one prop; use place arrange on for several");
  const top = topOf(ctx, surface, flags);
  const at = flags.at && /^-?[\d.]+,-?[\d.]+$/.test(flags.at) ? pair(flags.at, "--at") : flags.at;
  const res = restOn(surface, items[0], { top, at, near: flags.near ? pair(flags.near, "--near") : null, yaw: relYawOf(flags, surface), inset: flags.inset ? Number(flags.inset) : undefined, allowOverhang: Boolean(flags["allow-overhang"]) });
  return commit(ctx, [{ ...res, item: items[0], support: surface }], flags);
}

function cmdArrange(args, flags) {
  const [mode, ...rest] = args;
  const ctx = openWorld(flags);
  if (mode === "on") {
    const [surfaceName, ...prefabs] = rest;
    if (!surfaceName) throw new PlaceError("usage: place arrange on <surface> <prefab>... --layer L.layer");
    const surface = findEntity(ctx.ents, surfaceName);
    const items = itemsOf(ctx, prefabs, flags);
    const top = topOf(ctx, surface, flags);
    let row = 0;
    if (flags.at && flags.at !== "center") {
      const head = parseHeading(flags.at);
      const [lx, lz] = unrot(surface.yaw, Math.sin(head * D2R), Math.cos(head * D2R));
      const longX = (top.rect.maxX - top.rect.minX) >= (top.rect.maxZ - top.rect.minZ);
      row = Math.sign(Math.round(longX ? lz : lx));
    }
    const placed = arrangeOn(surface, items, { top, spacing: flags.spacing ? Number(flags.spacing) : undefined, yaw: flags.yaw != null ? Number(flags.yaw) : undefined, row });
    return commit(ctx, placed.map((res, i) => ({ ...res, item: items[i], support: surface })), flags);
  }
  if (mode === "row") {
    const [aText, bText, ...prefabs] = rest;
    const a = pair(aText, "the row start");
    const b = pair(bText, "the row end");
    const items = itemsOf(ctx, prefabs, flags);
    const yaw = groundYaw(flags, null);
    const along = [b[0] - a[0], b[1] - a[1]].map((v) => v / (Math.hypot(b[0] - a[0], b[1] - a[1]) || 1));
    const widths = items.map((it) => {
      const pts = footprintCorners(it.bounds, it.scale, yaw).map((p) => p[0] * along[0] + p[1] * along[1]);
      return Math.max(...pts) - Math.min(...pts);
    });
    const centres = rowCentres(a, b, widths, flags.spacing ? Number(flags.spacing) : undefined);
    return groundPlans(ctx, items.map((item, i) => ({ item, at: centres[i], yaw })), flags);
  }
  throw new PlaceError("usage: place arrange on <surface> <prefab>... | place arrange row <x1,z1> <x2,z2> <prefab>...");
}

function groundYaw(flags, at) {
  if (flags["face-to"] != null && at) {
    const to = pair(flags["face-to"], "--face-to");
    return yawForFacing(headingOf(to[0] - at[0], to[1] - at[1]), flags.front ?? "+z");
  }
  if (flags.facing != null) return yawForFacing(parseHeading(flags.facing), flags.front ?? "+z");
  return flags.yaw != null ? normYaw(Number(flags.yaw)) : 0;
}

// Ground placements: one probe boot for every footprint's heights, then the shared commit.
function groundPlans(ctx, wants, flags) {
  if (ctx.prefabFile) throw new PlaceError("--in works on a prefab's children: use place on root");
  const hintOf = (at) => {
    if (flags["y-hint"] != null) return Number(flags["y-hint"]);
    const near = ctx.world.filter((e) => !e.parent && Math.hypot(e.x - at[0], e.z - at[1]) < 25).sort((p, q) => Math.hypot(p.x - at[0], p.z - at[1]) - Math.hypot(q.x - at[0], q.z - at[1]))[0];
    return near ? near.y : 0;
  };
  const lines = [];
  const early = [];
  wants.forEach((w, i) => {
    // The centre of the footprint first (a mesh is not always centred on its pivot), then its corners pulled 10% in.
    const fp = footprintRect(w.item.bounds, w.item.scale, w.yaw);
    const mid = [(fp.min[0] + fp.max[0]) / 2, (fp.min[1] + fp.max[1]) / 2];
    // --at is where the footprint centre goes; the pivot follows from the mesh.
    w.pivot = [w.at[0] - mid[0], w.at[1] - mid[1]];
    const pts = [w.at, ...footprintCorners(w.item.bounds, w.item.scale, w.yaw).map((p) => [w.pivot[0] + mid[0] + (p[0] - mid[0]) * 0.9, w.pivot[1] + mid[1] + (p[1] - mid[1]) * 0.9])];
    const y0 = hintOf(w.at);
    early.push({ x: w.pivot[0], y: y0 - w.item.bounds.min[1] * w.item.scale, z: w.pivot[1], yaw: w.yaw, item: w.item });
    pts.forEach((p, k) => lines.push(rayLine(`g${i}_${k}`, [p[0], y0 + 3, p[1]], [p[0], y0 - 8, p[1]], w.item.mode === "move" ? w.item.name : "")));
  });
  // Roads, doors and neighbours do not depend on the exact height: refuse those before booting the server.
  if (review(ctx, early, flags, { quiet: true }) && !flags.force) {
    console.log("place: not written (problems above). Pick another spot, or --force to write anyway.");
    return 1;
  }
  console.log(`  probing the ground (${lines.length} rays, one test-server boot)`);
  const run = serverRun(lines);
  ctx.doors = run.doors;
  const plans = wants.map((w, i) => {
    const samples = [0, 1, 2, 3, 4].map((k) => {
      const ray = run.rays.get(`g${i}_${k}`);
      const hitY = ray.d < ray.len - 0.001 ? ray.p[1] : -Infinity;
      return { y: Math.max(hitY, ray.sy), hit: ray.d < ray.len - 0.001 && hitY >= ray.sy - 0.02 ? ray.root : "terrain" };
    });
    const res = onGround({ samples, prop: w.item });
    const under = samples[0].hit;
    const notes = [...res.notes];
    if (under !== "terrain" && under !== "none") notes.push(`standing on ${under}`);
    return { x: w.pivot[0], y: res.y, z: w.pivot[1], yaw: w.yaw, notes, item: w.item };
  });
  return commit(ctx, plans, flags);
}

function cmdGround(args, flags) {
  const [atText, ...prefabs] = args;
  const at = pair(atText, "the spot");
  const ctx = openWorld(flags);
  const items = itemsOf(ctx, prefabs, flags);
  if (items.length !== 1) throw new PlaceError("place ground takes one prop; use place arrange row for several");
  return groundPlans(ctx, [{ item: items[0], at, yaw: groundYaw(flags, at) }], flags);
}

function cmdAgainst(args, flags) {
  const [targetName, ...prefabs] = args;
  if (!targetName || (!flags.near && !flags.face)) throw new PlaceError("usage: place against <target> <prefab> --layer L.layer (--near x,z | --face north [--along 0.5]) [--hang h]");
  const ctx = openWorld(flags);
  if (ctx.prefabFile) throw new PlaceError("--in works on a prefab's children: use place on root");
  const target = findEntity(ctx.ents, targetName);
  if (!target.bounds) throw new PlaceError(`${targetName} has no mesh bounds`);
  const items = itemsOf(ctx, prefabs, flags);
  if (items.length !== 1) throw new PlaceError("place against takes one prop");
  const item = items[0];
  const near = flags.near ? pair(flags.near, "--near") : null;
  const face = faceNormal(target, { near, face: flags.face ?? null });
  const { n, t } = face;
  const yaw = yawForFacing(headingOf(n[0], n[1]), flags.front ?? "+z");
  const reach = flags.reach ? Number(flags.reach) : 6;
  let start = near;
  if (!start) {
    const along = flags.along != null ? Number(flags.along) : 0.5;
    const out = face.half[0] + 1;
    const side = -face.half[1] + along * 2 * face.half[1];
    start = [face.centre[0] + n[0] * out + t[0] * side, face.centre[1] + n[1] * out + t[1] * side];
  }
  const hang = flags.hang != null ? Number(flags.hang) : null;
  const corners = footprintCorners(item.bounds, item.scale, yaw).map((c) => c[0] * t[0] + c[1] * t[1]);
  const width = Math.max(...corners) - Math.min(...corners);
  const height = (item.bounds.max[1] - item.bounds.min[1]) * item.scale;
  const y0 = flags.floor != null ? Number(flags.floor) : target.y;
  const lats = [];
  const nLat = Math.max(3, Math.ceil(width / 0.3) + 1);
  for (let i = 0; i < nLat; i++) lats.push(-width / 2 + 0.03 + ((width - 0.06) * i) / (nLat - 1));
  const hs = [];
  for (let h = y0 - 0.5; h <= y0 + (hang ?? 0) + height + 1.0; h += 0.15) hs.push(h);
  const lines = [];
  const meta = [];
  const exclude = item.mode === "move" ? item.name : "";
  lats.forEach((lat, i) => hs.forEach((h, j) => {
    const from = [start[0] + t[0] * lat, h, start[1] + t[1] * lat];
    lines.push(rayLine(`w${i}_${j}`, from, [from[0] - n[0] * reach, h, from[2] - n[1] * reach], exclude));
    meta.push({ id: `w${i}_${j}`, lat, h });
  }));
  const steps = Math.floor(reach / 0.2);
  for (let k = 0; k <= steps; k++) {
    const p = [start[0] - n[0] * k * 0.2, start[1] - n[1] * k * 0.2];
    lines.push(rayLine(`f${k}`, [p[0], y0 + 1.3, p[1]], [p[0], y0 - 2, p[1]], exclude));
  }
  console.log(`  probing the ${r2(headingOf(n[0], n[1]))} deg face of ${targetName} from ${r2(start[0])}, ${r2(start[1])} (${lines.length} rays, one test-server boot)`);
  const run = serverRun(lines);
  ctx.doors = run.doors;
  const seen = (ray) => ray.d < ray.len - 0.001;
  const nameOfHit = (ray) => (ray.root && ray.root !== "GenericEntity" && ray.root !== "none" ? ray.root : ray.hit);
  const rays = meta.map((m) => ({ lat: m.lat, h: m.h, d: seen(run.rays.get(m.id)) ? run.rays.get(m.id).d : Infinity, label: nameOfHit(run.rays.get(m.id)) }));
  const floors = [];
  for (let k = 0; k <= steps; k++) {
    const ray = run.rays.get(`f${k}`);
    floors.push({ dist: k * 0.2, y: Math.max(seen(ray) ? ray.p[1] : -Infinity, ray.sy) });
  }
  const res = againstWall({ rays, floors, prop: item, yaw, n, hang, gap: flags.gap != null ? Number(flags.gap) : undefined, allowUneven: Boolean(flags["allow-uneven"]) });
  const plan = {
    x: start[0] - n[0] * res.along + t[0] * res.lat,
    y: res.y,
    z: start[1] - n[1] * res.along + t[1] * res.lat,
    yaw,
    notes: [...res.notes, `facing ${r2(headingOf(n[0], n[1]))} deg (front taken as ${flags.front ?? "+z"}; confirm the front in a shot)`],
    item,
    hung: hang != null,
  };
  return commit(ctx, [plan], flags);
}

function cmdBounds(args) {
  const [arg] = args;
  if (!arg) throw new PlaceError("usage: place bounds <prefab|entity>");
  let subject;
  const ents = loadModel(ROOT);
  const ent = ents.filter((e) => e.name === arg);
  if (ent.length === 1) subject = { label: `${arg} [${ent[0].file}] at ${num(ent[0].x)} ${num(ent[0].y)} ${num(ent[0].z)} yaw ${num(ent[0].yaw)}`, prefab: ent[0].prefab, mesh: ent[0].mesh, bounds: ent[0].bounds, scale: ent[0].scale, dynamic: ent[0].dynamic };
  else {
    const ref = resolveRef(ROOT, arg);
    const info = prefabInfo(ROOT, ref.path);
    subject = { label: `{${ref.guid}}${ref.path} (${info.cls})`, prefab: ref.path, mesh: info.mesh, bounds: info.bounds, scale: info.scale, dynamic: info.dynamic };
  }
  console.log(subject.label);
  if (!subject.bounds) {
    console.log("  no mesh bounds");
    return 1;
  }
  const b = subject.bounds;
  const s = subject.scale;
  console.log(`  mesh ${subject.mesh}`);
  console.log(`  size ${r3((b.max[0] - b.min[0]) * s)} x ${r3((b.max[1] - b.min[1]) * s)} x ${r3((b.max[2] - b.min[2]) * s)} m (x, y, z) at scale ${s}`);
  console.log(`  bounds min ${b.min.map((v) => r3(v * s)).join(" ")} max ${b.max.map((v) => r3(v * s)).join(" ")} (pivot at 0 0 0; base ${r3(b.min[1] * s)} m from the pivot)`);
  console.log(`  rigid body: ${subject.dynamic ? "simulated (clients can move it; it can tip or roll)" : "static or none"}`);
  const cached = loadCache()[subject.mesh];
  if (cached) {
    const top = surfaceTop(b, cached);
    console.log(`  as a surface: top ${r3(top.y * s)} m, usable ${r2((top.rect.maxX - top.rect.minX) * s)} x ${r2((top.rect.maxZ - top.rect.minZ) * s)} m (${top.source}, ${cached.at})`);
  }
  return 0;
}

function cmdProbe(args, flags) {
  const [name] = args;
  if (!name) throw new PlaceError("usage: place probe <surface entity>");
  const ctx = openWorld(flags);
  const surface = findEntity(ctx.ents, name);
  const top = topOf(ctx, surface, flags, { force: true });
  const s = surface.scale;
  console.log(`${name}: top ${r3(top.y * s)} m above its pivot (mesh bounds top ${r3(surface.bounds.max[1] * s)} m), usable ${r2((top.rect.maxX - top.rect.minX) * s)} x ${r2((top.rect.maxZ - top.rect.minZ) * s)} m of ${r2((surface.bounds.max[0] - surface.bounds.min[0]) * s)} x ${r2((surface.bounds.max[2] - surface.bounds.min[2]) * s)} m (${top.source}); cached for ${surface.mesh}`);
  return 0;
}

function cmdVerify(args) {
  if (!args.length) throw new PlaceError("usage: place verify <entity>...");
  const ents = loadModel(ROOT);
  const targets = args.map((name) => {
    const e = findEntity(ents, name);
    return { name, prefab: e.prefab, x: r3(e.x), y: r3(e.y), z: r3(e.z), yaw: e.yaw, hung: false, tilted: e.tilted };
  });
  const verdict = runJudge(targets);
  let bad = 0;
  for (const v of verdict) {
    // A hung thing is judged by the audit's own mounted rule, not by the tool's rest tolerance.
    const problems = v.problems.filter((p) => !(v.measure?.backed && /^its base is/.test(p)) && !(v.target.tilted && /facing/.test(p)));
    if (!problems.length) console.log(`audit ${v.target.name}: clean (on ${v.measure.on}, gap ${r3(v.measure.gap)} m${v.measure.backed ? ", backed" : ""})`);
    else {
      bad++;
      for (const p of problems) console.log(`audit ${v.target.name}: ${p}`);
    }
  }
  return bad ? 1 : 0;
}

const USAGE = `usage: tools\\cli place <command>   (docs/placement-tool.md)
  on <surface> <prefab> --layer L.layer [--name N] [--at center|north|south-east|u,v] [--near x,z] [--yaw deg | --facing north|deg]
  against <target> <prefab> --layer L.layer (--near x,z | --face north [--along 0..1]) [--hang m] [--front +z|-z|+x|-x] [--allow-uneven]
  ground <x,z> <prefab> --layer L.layer [--yaw deg | --facing north|deg | --face-to x,z] [--allow-road]
  arrange on <surface> <prefab>... --layer L.layer [--count n] [--spacing m] [--at north]
  arrange row <x1,z1> <x2,z2> <prefab>... --layer L.layer [--yaw deg]
  bounds <prefab|entity> | probe <surface> | verify <entity>...
  --move <Entity>[,...] re-places existing entities; --in <Prefabs/X.et> works on a prefab's children (surface: root)
  --dry  --no-verify  --keep  --force  --scale s`;

export async function main(argv) {
  const [cmd, ...rest] = argv;
  const { _: args, flags } = parseArgs(rest);
  try {
    if (cmd === "on") return cmdOn(args, flags);
    if (cmd === "against") return cmdAgainst(args, flags);
    if (cmd === "ground") return cmdGround(args, flags);
    if (cmd === "arrange") return cmdArrange(args, flags);
    if (cmd === "bounds") return cmdBounds(args, flags);
    if (cmd === "probe") return cmdProbe(args, flags);
    if (cmd === "verify") return cmdVerify(args, flags);
  } catch (e) {
    if (!(e instanceof PlaceError)) throw e;
    console.log(`place: ${e.message}`);
    return 1;
  }
  console.log(USAGE);
  return 1;
}

if (process.argv[1] && resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
  main(process.argv.slice(2)).then((c) => process.exit(c ?? 0));
}
