// tools/scout/analyse.mjs - what a dump says about a place: buildings with footprints, entrances and interiors,
// roads and junctions, the main street, free plots, elevation and slope. Pure functions over the parsed dump
// (tools/scout/dump.mjs) and the models' measured bounds (tools\cli measure); no game, no Workbench, so the selftest
// runs them on a fixture.
//
// Conventions (docs/placement-tool.md, tools/place/place.mjs rot): x east, z north, y up; yaw degrees, 0 faces +z,
// 90 faces +x. A prefab's local (lx, lz) turns to the world as [lx c + lz s, -lx s + lz c].
import { matchByPrefab } from "./catalog.mjs";
import { CIVIC_TYPES, COUNTED_TYPES, HOME_TYPES, classify } from "./types.mjs";

const D2R = Math.PI / 180;
const r1 = (n) => Math.round(n * 10) / 10;
const r2 = (n) => Math.round(n * 100) / 100;

export const rot = (yaw, lx, lz) => {
  const c = Math.cos(yaw * D2R);
  const s = Math.sin(yaw * D2R);
  return [lx * c + lz * s, -lx * s + lz * c];
};

const COMPASS = ["N", "NE", "E", "SE", "S", "SW", "W", "NW"];
// Pure: a direction in world x, z (z is north) -> N, NE, ... NW.
export const compass = (dx, dz) => COMPASS[Math.round((((Math.atan2(dx, dz) / D2R) % 360) + 360) % 360 / 45) % 8];
// Pure: a yaw -> the compass point the entity faces.
export const facing = (yaw) => compass(Math.sin(yaw * D2R), Math.cos(yaw * D2R));
const dist2 = (ax, az, bx, bz) => Math.hypot(ax - bx, az - bz);

// ------------------------------------------------------------------ footprints

// Pure: an entity's footprint. `bounds` is the model's AABB in prefab space from `tools\cli measure`
// ({ min: [x,y,z], max: [x,y,z] }, scale already applied), or null: then the world bounds the engine reported are
// used (axis aligned, so a turned building is over-reported; `source` says which).
// -> { source, w, d, h, area, cx, cz, corners: [[x,z] x4] }; w runs along the building's own x, d along its z.
export function footprint(e, bounds) {
  if (bounds) {
    const s = e.scale || 1;
    const x0 = bounds.min[0] * s;
    const x1 = bounds.max[0] * s;
    const z0 = bounds.min[2] * s;
    const z1 = bounds.max[2] * s;
    const corners = [[x0, z0], [x1, z0], [x1, z1], [x0, z1]].map(([lx, lz]) => {
      const [dx, dz] = rot(e.yaw, lx, lz);
      return [r2(e.x + dx), r2(e.z + dz)];
    });
    const [mx, mz] = rot(e.yaw, (x0 + x1) / 2, (z0 + z1) / 2);
    // The root model is the building unless parts hang off it as child entities: when the engine's own bounds
    // (children included) cover clearly more ground than the model does, the engine's are the truth.
    const xs = corners.map((c) => c[0]);
    const zs = corners.map((c) => c[1]);
    const modelBox = (Math.max(...xs) - Math.min(...xs)) * (Math.max(...zs) - Math.min(...zs));
    const engineBox = e.min ? (e.max[0] - e.min[0]) * (e.max[2] - e.min[2]) : 0;
    if (e.min && engineBox > modelBox * 1.25 + 1) return { ...worldBounds(e), note: `the engine's bounds are ${Math.round((engineBox / modelBox - 1) * 100)}% larger than the root model (parts hang off it)` };
    return { source: "measure", w: r1(x1 - x0), d: r1(z1 - z0), h: r1((bounds.max[1] - bounds.min[1]) * s), area: r1((x1 - x0) * (z1 - z0)), cx: r2(e.x + mx), cz: r2(e.z + mz), corners };
  }
  return worldBounds(e);
}

function worldBounds(e) {
  const [x0, z0] = [e.min[0], e.min[2]];
  const [x1, z1] = [e.max[0], e.max[2]];
  return { source: "world-bounds", w: r1(x1 - x0), d: r1(z1 - z0), h: r1(e.max[1] - e.min[1]), area: r1((x1 - x0) * (z1 - z0)), cx: r2((x0 + x1) / 2), cz: r2((z0 + z1) / 2), corners: [[x0, z0], [x1, z0], [x1, z1], [x0, z1]].map((c) => c.map(r2)) };
}

// Pure: is (x, z) inside a convex quad, `pad` metres grown on every side.
export function inQuad(corners, x, z, pad = 0) {
  let sign = 0;
  for (let k = 0; k < 4; k++) {
    const [ax, az] = corners[k];
    const [bx, bz] = corners[(k + 1) % 4];
    const len = Math.hypot(bx - ax, bz - az) || 1;
    const cross = ((bx - ax) * (z - az) - (bz - az) * (x - ax)) / len; // signed distance to the edge's line
    if (!sign) sign = Math.sign(cross) || 1;
    if (cross * sign < -pad) return false;
  }
  return true;
}

// Pure: distance from (x, z) to a convex quad's outline (0 inside).
export function quadDistance(corners, x, z) {
  if (inQuad(corners, x, z)) return 0;
  let best = Infinity;
  for (let k = 0; k < 4; k++) best = Math.min(best, segDistance([x, z], corners[k], corners[(k + 1) % 4]));
  return best;
}

export function segDistance(p, a, b) {
  const [dx, dz] = [b[0] - a[0], b[1] - a[1]];
  const l2 = dx * dx + dz * dz;
  const t = l2 ? Math.max(0, Math.min(1, ((p[0] - a[0]) * dx + (p[1] - a[1]) * dz) / l2)) : 0;
  return Math.hypot(p[0] - (a[0] + t * dx), p[1] - (a[1] + t * dz));
}

// ------------------------------------------------------------------ entrances and interiors

// Distance from (x, z) to the outline of a quad, inside or out.
export function outlineDistance(corners, x, z) {
  let best = Infinity;
  for (let k = 0; k < 4; k++) best = Math.min(best, segDistance([x, z], corners[k], corners[(k + 1) % 4]));
  return best;
}

// Pure: the doors of a building that are entrances: the side of the footprint each stands on and how far it is from the
// outline. A door entity below the building can be an inner door of the plan; only one within 1.3 m of the outline
// (a wall is thinner) leads in from outside.
export function entrances(e, fp) {
  return e.doors.filter((d) => outlineDistance(fp.corners, d.x, d.z) <= 1.3).map((d) => {
    const dx = d.x - fp.cx;
    const dz = d.z - fp.cz;
    const [lx, lz] = rot(-e.yaw, dx, dz); // in the building's own frame: +z is its front
    const side = Math.abs(lz) / (fp.d / 2 || 1) >= Math.abs(lx) / (fp.w / 2 || 1) ? (lz >= 0 ? "front" : "back") : lx >= 0 ? "right" : "left";
    return { prefab: d.prefab, x: r1(d.x), y: r1(d.y), z: r1(d.z), side, facing: compass(dx, dz), edge: r1(outlineDistance(fp.corners, d.x, d.z)) };
  });
}

// Types that are built to be walked into: with no door entity found the answer is "unknown", not "no".
const ENTERABLE = new Set(["church", "chapel", "government", "police", "fire", "pub", "shop", "hotel", "recreation", "house", "townhouse", "villa", "farmhouse", "industrial", "farm", "military", "castle"]);

// Pure: { value: yes | no | unknown, basis }. The dump reports door entities below the building (BaseDoorComponent),
// which is what makes a vanilla building enterable; a building with none is closed or needs a look.
export function interiorOf(type, doors, ruin) {
  if (doors > 0) return { value: "yes", basis: `${doors} door entit${doors === 1 ? "y" : "ies"} below the building` };
  if (ruin) return { value: "no", basis: "a ruin" };
  if (ENTERABLE.has(type)) return { value: "unknown", basis: "no door entity found, but this type is built to be entered: look at it (`look --interior`)" };
  return { value: "no", basis: "no door entity found" };
}

// ------------------------------------------------------------------ buildings

// The building record both sources fill (the catalog's and the dump's): where, how it faces, how far from the centre.
function buildingBase(e, c, centre) {
  return {
    prefab: e.prefab,
    path: e.prefab.replace(/^\{[0-9A-Fa-f]{16}\}/, ""),
    stem: c.stem,
    type: c.type,
    civic: c.civic,
    home: c.home,
    ruin: c.ruin,
    name: e.name ?? "",
    x: r1(e.x),
    y: r1(e.y ?? 0),
    z: r1(e.z),
    yaw: r1(e.yaw),
    facing: facing(e.yaw),
    scale: e.scale ?? 1,
    distance: r1(dist2(e.x, e.z, centre.x, centre.z)),
    bearing: compass(e.x - centre.x, e.z - centre.z),
  };
}

// Pure: a catalog building (tools/scout/catalog.mjs) as a dossier building. `match` is the dump's entity for the same
// building (same prefab, within 1.5 m) or null; it adds the door positions and, when its bounds agree with the
// catalog's extents, the footprint's true centre (the catalog gives extents, not the model's offset from its origin).
// `placeRow` is the building's row of a place file (docs/everon/<place>.json) or null: what stands inside it.
export function catalogBuilding(c, match, centre, placeRow = null, groundY = null) {
  const [w, d] = c.footprint ?? [0, 0];
  const e = { prefab: c.res, x: c.x, y: match?.y ?? groundY ?? 0, z: c.z, yaw: c.yaw, scale: match?.scale ?? 1, name: match?.name ?? "", doors: match?.doors ?? [] };
  const kind = { stem: c.stem, type: c.type, civic: CIVIC_TYPES.has(c.type), home: HOME_TYPES.has(c.type), ruin: c.type === "ruin" };
  const fp = footprint({ x: c.x, z: c.z, yaw: c.yaw, scale: e.scale }, { min: [-w / 2, 0, -d / 2], max: [w / 2, c.height ?? 0, d / 2] });
  fp.source = "catalog";
  if (match && match.min) {
    const xs = fp.corners.map((p) => p[0]);
    const zs = fp.corners.map((p) => p[1]);
    const obb = (Math.max(...xs) - Math.min(...xs)) * (Math.max(...zs) - Math.min(...zs));
    const eng = (match.max[0] - match.min[0]) * (match.max[2] - match.min[2]);
    const dx = (match.min[0] + match.max[0]) / 2 - (Math.max(...xs) + Math.min(...xs)) / 2;
    const dz = (match.min[2] + match.max[2]) / 2 - (Math.max(...zs) + Math.min(...zs)) / 2;
    if (obb > 0 && eng <= obb * 1.25 + 1 && eng >= obb * 0.8 - 1 && Math.hypot(dx, dz) > 0.3 && Math.hypot(dx, dz) <= 3) {
      fp.corners = fp.corners.map(([x, z]) => [r2(x + dx), r2(z + dz)]);
      fp.cx = r2(fp.cx + dx);
      fp.cz = r2(fp.cz + dz);
      fp.note = "centred on the engine's bounds (the catalog gives extents only)";
    }
  }
  const ent = entrances(e, fp);
  const doors = match ? match.doors.length : c.doorChildren;
  let interior;
  if (c.enterable) interior = { value: "yes", basis: `catalog: ${c.doorChildren} door entit${c.doorChildren === 1 ? "y" : "ies"} and a walkable floor${c.floors ? ` (${c.floors} floor${c.floors === 1 ? "" : "s"})` : ""}` };
  else if (doors > 0) interior = { value: "unknown", basis: "door entities but no walkable floor level in the model: look at it (`look --interior`)" };
  else interior = { value: "no", basis: c.type === "ruin" ? "a ruin" : "catalog: no door entity and no walkable floor" };
  const b = {
    ...buildingBase(e, kind, centre),
    footprint: fp,
    floors: c.floors,
    firstFloor: (c.floorLevelsY ?? []).find((y) => y >= -0.5) ?? null,
    basement: c.basement,
    height: c.height,
    interior,
    doors,
    entrances: ent,
    entrancesKnown: !!match,
    matched: !!match,
  };
  if (placeRow) {
    b.furnitureInside = placeRow.propsInside ?? 0;
    if (placeRow.topProps) b.topProps = placeRow.topProps;
  }
  return b;
}

// Pure: the buildings, props and counts of a dump. `measured` maps a prefab path to its model bounds (or null).
// `catalog` ({ buildings: catalogBuildings(...), placeFile }) makes the catalog the source of the buildings (their types,
// footprints, floors, interiors), the dump adding door positions; without it every building comes from the dump's own
// entities, typed by prefab path and measured with `measured`.
export function sortEntities(dump, centre, measured = {}, catalog = null) {
  const buildings = [];
  const props = [];
  const counts = { walls: 0, vegetation: dump.trees.length, roadParts: 0, other: 0, infrastructure: 0 };
  const propCounts = {};
  const used = new Set();
  const nearby = dump.entities.filter((e) => classify(e.prefab).group === "building");
  for (const e of dump.entities) {
    const c = classify(e.prefab);
    const d = dist2(e.x, e.z, centre.x, centre.z);
    if (c.group === "building") {
      if (catalog) continue;
      const fp = footprint(e, measured[e.prefab.replace(/^\{[0-9A-Fa-f]{16}\}/, "")] ?? null);
      buildings.push({ ...buildingBase(e, c, centre), footprint: fp, interior: interiorOf(c.type, e.doors.length, c.ruin), doors: e.doors.length, entrances: entrances(e, fp) });
    } else if (c.group === "prop") {
      props.push({ prefab: e.prefab, stem: c.stem, kind: c.type, x: r1(e.x), y: r1(e.y), z: r1(e.z), yaw: r1(e.yaw), distance: r1(d), size: [r1(e.max[0] - e.min[0]), r1(e.max[1] - e.min[1]), r1(e.max[2] - e.min[2])], min: e.min, max: e.max });
      propCounts[c.type] = (propCounts[c.type] ?? 0) + 1;
    } else if (c.group === "wall") counts.walls++;
    else if (c.group === "road-part") counts.roadParts++;
    else if (c.group === "vegetation") counts.vegetation++;
    else if (c.group === "infrastructure") counts.infrastructure++;
    else counts.other++;
  }
  if (catalog) {
    for (const c of catalog.buildings) {
      if (COUNTED_TYPES.has(c.type)) {
        counts.infrastructure++;
        continue;
      }
      const m = matchByPrefab(nearby.map((e) => ({ ...e, path: e.prefab.replace(/^\{[0-9A-Fa-f]{16}\}/, "") })), c.path, c.x, c.z);
      if (m) used.add(m);
      const row = catalog.placeFile ? matchByPrefab(catalog.placeFile.buildings, c.path, c.x, c.z) : null;
      buildings.push(catalogBuilding(c, m, centre, row, dump.grid ? heightAt(dump.grid, c.x, c.z) : null));
    }
  }
  buildings.sort((a, b) => a.distance - b.distance);
  buildings.forEach((b, i) => (b.id = `b${String(i + 1).padStart(2, "0")}`));
  props.sort((a, b) => a.distance - b.distance);
  return { buildings, props, counts, propCounts };
}

// ------------------------------------------------------------------ roads

const inRadius = (p, centre, radius) => dist2(p[0], p[2] ?? p[1], centre.x, centre.z) <= radius;
const xz = (p) => [p[0], p[2]];

// Pure: roads as { id, name, cls, points: [[x,z]], length } clipped to `radius` of the centre (a road leaving the
// circle keeps its points inside it plus one beyond, so the line still leaves).
export function roadsIn(roads, centre, radius) {
  const out = [];
  roads.forEach((r, k) => {
    const pts = r.points;
    const keep = [];
    pts.forEach((p, i) => {
      if (inRadius(p, centre, radius) || (pts[i - 1] && inRadius(pts[i - 1], centre, radius)) || (pts[i + 1] && inRadius(pts[i + 1], centre, radius))) keep.push(xz(p));
    });
    if (keep.length < 2) return;
    let length = 0;
    for (let i = 1; i < keep.length; i++) length += dist2(keep[i - 1][0], keep[i - 1][1], keep[i][0], keep[i][1]);
    const id = `r${String(k + 1).padStart(2, "0")}`;
    const runs = chordRuns(keep);
    out.push({ id, name: r.name || `${runs} ${r.cls === "RoadEntity" ? "road" : r.cls} ${id}`, cls: r.cls, runs, points: keep.map((p) => p.map(r1)), length: r1(length) });
  });
  return out;
}

// The compass line a polyline runs along end to end: N-S, NE-SW, E-W or NW-SE.
export function chordRuns(points) {
  const a = points[0];
  const b = points[points.length - 1];
  const deg = ((Math.atan2(b[0] - a[0], b[1] - a[1]) / D2R) % 180 + 180) % 180;
  return deg < 22.5 || deg >= 157.5 ? "N-S" : deg < 67.5 ? "NE-SW" : deg < 112.5 ? "E-W" : "NW-SE";
}

function segIntersect(a, b, c, d) {
  const r = [b[0] - a[0], b[1] - a[1]];
  const s = [d[0] - c[0], d[1] - c[1]];
  const den = r[0] * s[1] - r[1] * s[0];
  if (Math.abs(den) < 1e-9) return null;
  const t = ((c[0] - a[0]) * s[1] - (c[1] - a[1]) * s[0]) / den;
  const u = ((c[0] - a[0]) * r[1] - (c[1] - a[1]) * r[0]) / den;
  if (t < 0 || t > 1 || u < 0 || u > 1) return null;
  return [a[0] + t * r[0], a[1] + t * r[1]];
}

// Pure: where two roads meet. A crossing of two segments, or an end of one road within `tol` of the other, is a
// junction; candidates within `merge` metres are one. -> [{ x, z, roads: [ids], arms }].
export function junctions(roads, { tol = 3, merge = 8 } = {}) {
  const cand = [];
  for (let i = 0; i < roads.length; i++) {
    for (let j = i + 1; j < roads.length; j++) {
      const A = roads[i].points;
      const B = roads[j].points;
      for (let a = 1; a < A.length; a++) {
        for (let b = 1; b < B.length; b++) {
          const p = segIntersect(A[a - 1], A[a], B[b - 1], B[b]);
          if (p) cand.push({ p, ids: [roads[i].id, roads[j].id] });
        }
      }
      for (const [P, Q, ids] of [[A, B, [roads[i].id, roads[j].id]], [B, A, [roads[j].id, roads[i].id]]]) {
        for (const end of [P[0], P[P.length - 1]]) {
          for (let b = 1; b < Q.length; b++) if (segDistance(end, Q[b - 1], Q[b]) <= tol) cand.push({ p: end, ids });
        }
      }
    }
  }
  const out = [];
  for (const c of cand) {
    const hit = out.find((o) => dist2(o.x, o.z, c.p[0], c.p[1]) <= merge);
    if (hit) {
      for (const id of c.ids) if (!hit.roads.includes(id)) hit.roads.push(id);
      continue;
    }
    out.push({ x: c.p[0], z: c.p[1], roads: [...c.ids] });
  }
  // Arms: a road that runs through the junction has two, one that ends there has one.
  for (const o of out) {
    o.arms = 0;
    for (const id of o.roads) {
      const pts = roads.find((r) => r.id === id).points;
      const first = dist2(pts[0][0], pts[0][1], o.x, o.z) <= merge;
      const last = dist2(pts[pts.length - 1][0], pts[pts.length - 1][1], o.x, o.z) <= merge;
      o.arms += first || last ? 1 : 2;
    }
    o.x = r1(o.x);
    o.z = r1(o.z);
    o.kind = o.arms >= 4 ? "crossroads" : o.arms === 3 ? "t-junction" : "bend-or-join";
  }
  return out.sort((a, b) => b.arms - a.arms);
}

// Pure: distance from (x, z) to a road polyline.
export function roadDistance(road, x, z) {
  let best = Infinity;
  for (let i = 1; i < road.points.length; i++) best = Math.min(best, segDistance([x, z], road.points[i - 1], road.points[i]));
  return best;
}

// Pure: the street the place's life runs along: the road with the most buildings within 25 m, longer roads
// breaking a tie. -> { road, buildings, at: [x, z] (the point of it nearest the buildings' middle), bearing } or null.
export function mainStreet(roads, buildings, centre) {
  let best = null;
  for (const r of roads) {
    const near = buildings.filter((b) => !b.ruin && roadDistance(r, b.x, b.z) <= 25);
    const score = near.length * 100 + r.length;
    if (!best || score > best.score) best = { r, near, score };
  }
  if (!best || !best.near.length) return best ? { road: best.r, buildings: 0, at: nearestOn(best.r, centre.x, centre.z), bearing: bearingOf(best.r, centre) } : null;
  const mx = best.near.reduce((s, b) => s + b.x, 0) / best.near.length;
  const mz = best.near.reduce((s, b) => s + b.z, 0) / best.near.length;
  const at = nearestOn(best.r, mx, mz);
  return { road: best.r, buildings: best.near.length, at, bearing: bearingOf(best.r, { x: at[0], z: at[1] }) };
}

function nearestOn(road, x, z) {
  let best = { d: Infinity, p: road.points[0] };
  for (let i = 1; i < road.points.length; i++) {
    const a = road.points[i - 1];
    const b = road.points[i];
    const [dx, dz] = [b[0] - a[0], b[1] - a[1]];
    const l2 = dx * dx + dz * dz;
    const t = l2 ? Math.max(0, Math.min(1, ((x - a[0]) * dx + (z - a[1]) * dz) / l2)) : 0;
    const p = [a[0] + t * dx, a[1] + t * dz];
    const d = dist2(p[0], p[1], x, z);
    if (d < best.d) best = { d, p };
  }
  return best.p.map(r1);
}

// The compass line a road runs along near a point ("N-S", "E-W", "NE-SW", "NW-SE").
function bearingOf(road, near) {
  let best = { d: Infinity, i: 1 };
  for (let i = 1; i < road.points.length; i++) {
    const d = segDistance([near.x, near.z], road.points[i - 1], road.points[i]);
    if (d < best.d) best = { d, i };
  }
  const a = road.points[best.i - 1];
  const b = road.points[best.i];
  const deg = ((Math.atan2(b[0] - a[0], b[1] - a[1]) / D2R) % 180 + 180) % 180;
  return deg < 22.5 || deg >= 157.5 ? "N-S" : deg < 67.5 ? "NE-SW" : deg < 112.5 ? "E-W" : "NW-SE";
}

// ------------------------------------------------------------------ terrain

// Pure: the ground height at (x, z) from the grid, bilinear; null outside it.
export function heightAt(grid, x, z) {
  const fx = (x - grid.x0) / grid.step;
  const fz = (z - grid.z0) / grid.step;
  const i = Math.floor(fx);
  const j = Math.floor(fz);
  if (i < 0 || j < 0 || i >= grid.nx - 1 || j >= grid.nz - 1) return null;
  const [tx, tz] = [fx - i, fz - j];
  const h = (a, b) => grid.h[b][a];
  return h(i, j) * (1 - tx) * (1 - tz) + h(i + 1, j) * tx * (1 - tz) + h(i, j + 1) * (1 - tx) * tz + h(i + 1, j + 1) * tx * tz;
}

const isWater = (grid, x, z) => {
  const i = Math.round((x - grid.x0) / grid.step);
  const j = Math.round((z - grid.z0) / grid.step);
  return !!grid.water?.[j]?.[i];
};

// Slope in degrees of the cell at (i, j) from central differences.
function slopeAt(grid, i, j) {
  const g = (a, b) => grid.h[Math.max(0, Math.min(grid.nz - 1, b))][Math.max(0, Math.min(grid.nx - 1, a))];
  const dx = (g(i + 1, j) - g(i - 1, j)) / (2 * grid.step);
  const dz = (g(i, j + 1) - g(i, j - 1)) / (2 * grid.step);
  return Math.atan(Math.hypot(dx, dz)) / D2R;
}

// Pure: elevation and slope of the area within `radius` of the centre. -> { min, max, mean, range, flat, steep, tilt, water, notes }.
export function terrain(grid, centre, radius) {
  if (!grid || !grid.h.length) return { notes: ["no ground grid in the dump"] };
  const hs = [];
  let flat = 0;
  let mid = 0;
  let steep = 0;
  let wet = 0;
  let n = 0;
  let steepest = { deg: 0, x: 0, z: 0 };
  // Least-squares plane over the cells: the overall fall of the ground.
  let sx = 0, sz = 0, sy = 0, sxx = 0, szz = 0, sxy = 0, szy = 0;
  let nearestWater = null;
  for (let j = 0; j < grid.nz; j++) {
    for (let i = 0; i < grid.nx; i++) {
      const x = grid.x0 + i * grid.step;
      const z = grid.z0 + j * grid.step;
      const d = dist2(x, z, centre.x, centre.z);
      if (d > radius) continue;
      n++;
      if (grid.water?.[j]?.[i]) {
        wet++;
        if (!nearestWater || d < nearestWater.d) nearestWater = { d, x, z };
        continue;
      }
      const y = grid.h[j][i];
      hs.push(y);
      const s = slopeAt(grid, i, j);
      if (s < 3) flat++;
      else if (s < 8) mid++;
      if (s >= 15) steep++;
      if (s > steepest.deg) steepest = { deg: s, x, z };
      const [dx, dz] = [x - centre.x, z - centre.z];
      sx += dx; sz += dz; sy += y; sxx += dx * dx; szz += dz * dz; sxy += dx * y; szy += dz * y;
    }
  }
  if (!hs.length) return { notes: ["every cell of the grid is under the sea"] };
  const m = hs.length;
  const mean = hs.reduce((a, b) => a + b, 0) / m;
  // Solve y = a + b dx + c dz (normal equations, small and well-conditioned about the centre).
  const det = (A) => A[0][0] * (A[1][1] * A[2][2] - A[1][2] * A[2][1]) - A[0][1] * (A[1][0] * A[2][2] - A[1][2] * A[2][0]) + A[0][2] * (A[1][0] * A[2][1] - A[1][1] * A[2][0]);
  const M = [[m, sx, sz], [sx, sxx, 0], [sz, 0, szz]];
  const solveCol = (col, rhs) => {
    const A = M.map((r) => r.slice());
    for (let r = 0; r < 3; r++) A[r][col] = rhs[r];
    return det(A) / det(M);
  };
  const rhs = [sy, sxy, szy];
  const b = det(M) ? solveCol(1, rhs) : 0;
  const c = det(M) ? solveCol(2, rhs) : 0;
  const tiltDeg = Math.atan(Math.hypot(b, c)) / D2R;
  const t = {
    min: r1(Math.min(...hs)),
    max: r1(Math.max(...hs)),
    mean: r1(mean),
    range: r1(Math.max(...hs) - Math.min(...hs)),
    flatShare: r2(flat / m),
    gentleShare: r2(mid / m),
    steepShare: r2(steep / m),
    steepest: { deg: r1(steepest.deg), x: r1(steepest.x), z: r1(steepest.z) },
    tilt: { deg: r1(tiltDeg), fallsToward: tiltDeg >= 0.5 ? compass(-b, -c) : null },
    waterShare: r2(wet / n),
    nearestWater: nearestWater ? { distance: r1(nearestWater.d), direction: compass(nearestWater.x - centre.x, nearestWater.z - centre.z) } : null,
    notes: [],
  };
  t.notes.push(`ground ${t.min} to ${t.max} m (${t.range} m of relief) within ${radius} m; mean ${t.mean} m`);
  t.notes.push(`${Math.round(t.flatShare * 100)}% of the ground is under 3 degrees, ${Math.round(t.gentleShare * 100)}% 3 to 8, ${Math.round(t.steepShare * 100)}% over 15`);
  if (t.tilt.fallsToward) t.notes.push(`overall the ground falls ${t.tilt.deg} degrees toward the ${t.tilt.fallsToward}`);
  else t.notes.push("the ground is level overall");
  if (t.steepest.deg >= 15) t.notes.push(`steepest ground ${t.steepest.deg} degrees at ${t.steepest.x}, ${t.steepest.z}`);
  if (t.nearestWater) t.notes.push(`the sea is ${t.nearestWater.distance} m away to the ${t.nearestWater.direction}${t.waterShare > 0 ? ` (${Math.round(t.waterShare * 100)}% of the circle is water)` : ""}`);
  return t;
}

// ------------------------------------------------------------------ free plots

// Pure: the largest open rectangles of a place, for something to be built on.
//   blocked: building outlines (2.5 m margin), other entities bigger than 1.5 m (their bounds), roads (4.5 m half
//   width, the vanilla two-lane asphalt is about 6 m wide plus verge), trees (2 m), sea, and ground steeper than 12 degrees.
//   The biggest rectangle is taken, blocked with a 2 m margin, and the search repeats; `count` plots at most, each at
//   least `minSide` metres on its short side.
export function freePlots({ grid, centre, radius, buildings, props, roads, trees, cell = 2, minSide = 10, count = 8, roadHalf = 4.5, maxSlope = 12 }) {
  if (!grid || !grid.h.length) return { plots: [], note: "no ground grid in the dump, so no plots were found" };
  const n = Math.floor((2 * radius) / cell);
  const x0 = centre.x - radius;
  const z0 = centre.z - radius;
  const blocked = Array.from({ length: n }, () => new Uint8Array(n));
  const at = (i, j) => [x0 + (i + 0.5) * cell, z0 + (j + 0.5) * cell];
  const block = (fn) => {
    for (let j = 0; j < n; j++) for (let i = 0; i < n; i++) if (!blocked[j][i] && fn(...at(i, j))) blocked[j][i] = 1;
  };
  for (let j = 0; j < n; j++) {
    for (let i = 0; i < n; i++) {
      const [x, z] = at(i, j);
      const h = heightAt(grid, x, z);
      const gi = Math.round((x - grid.x0) / grid.step);
      const gj = Math.round((z - grid.z0) / grid.step);
      if (dist2(x, z, centre.x, centre.z) > radius || h === null || isWater(grid, x, z) || (gi >= 0 && gj >= 0 && gi < grid.nx && gj < grid.nz && slopeAt(grid, gi, gj) > maxSlope)) blocked[j][i] = 1;
    }
  }
  for (const b of buildings) block((x, z) => inQuad(b.footprint.corners, x, z, 2.5));
  for (const p of props) if (Math.max(p.size[0], p.size[2]) >= 1.5) block((x, z) => x >= p.min[0] - 0.5 && x <= p.max[0] + 0.5 && z >= p.min[2] - 0.5 && z <= p.max[2] + 0.5);
  for (const r of roads) block((x, z) => roadDistance(r, x, z) <= roadHalf);
  for (const [tx, tz] of trees) {
    const ci = Math.floor((tx - x0) / cell);
    const cj = Math.floor((tz - z0) / cell);
    for (let dj = -1; dj <= 1; dj++) for (let di = -1; di <= 1; di++) if (blocked[cj + dj]?.[ci + di] === 0) blocked[cj + dj][ci + di] = 1;
  }
  const plots = [];
  const need = Math.ceil(minSide / cell);
  for (let k = 0; k < count; k++) {
    const best = largestRect(blocked, n);
    if (!best || Math.min(best.w, best.h) < need) break;
    const w = best.w * cell;
    const d = best.h * cell;
    const px = x0 + (best.i + best.w / 2) * cell;
    const pz = z0 + (best.j + best.h / 2) * cell;
    const hs = [];
    for (let j = best.j; j < best.j + best.h; j++) for (let i = best.i; i < best.i + best.w; i++) hs.push(heightAt(grid, ...at(i, j)) ?? 0);
    const road = nearestRoad(roads, px, pz);
    plots.push({
      id: `p${k + 1}`,
      x: r1(px),
      z: r1(pz),
      width: r1(w),
      depth: r1(d),
      area: Math.round(w * d),
      ground: { min: r1(Math.min(...hs)), max: r1(Math.max(...hs)) },
      nearestRoad: road ? { name: road.name, distance: r1(road.distance) } : null,
      nearestBuilding: nearestBuilding(buildings, px, pz),
      distance: r1(dist2(px, pz, centre.x, centre.z)),
    });
    for (let j = Math.max(0, best.j - 1); j < Math.min(n, best.j + best.h + 1); j++) for (let i = Math.max(0, best.i - 1); i < Math.min(n, best.i + best.w + 1); i++) blocked[j][i] = 1;
  }
  return { plots, note: `plots are open rectangles at least ${minSide} m on the short side, kept ${2.5} m off buildings, ${roadHalf} m off a road's centre line, 2 m off a tree, on ground under ${maxSlope} degrees; axis aligned, not the best use of the land` };
}

// Largest all-free axis-aligned rectangle of a boolean grid (blocked = 1): the histogram method. -> { i, j, w, h } or null.
export function largestRect(blocked, n) {
  const hist = new Int32Array(n);
  let best = null;
  for (let j = 0; j < n; j++) {
    for (let i = 0; i < n; i++) hist[i] = blocked[j][i] ? 0 : hist[i] + 1;
    const stack = [];
    for (let i = 0; i <= n; i++) {
      const h = i === n ? 0 : hist[i];
      let start = i;
      while (stack.length && stack[stack.length - 1].h >= h) {
        const top = stack.pop();
        const area = top.h * (i - top.start);
        if (top.h && (!best || area > best.w * best.h)) best = { i: top.start, j: j - top.h + 1, w: i - top.start, h: top.h };
        start = top.start;
      }
      stack.push({ h, start });
    }
  }
  return best;
}

function nearestRoad(roads, x, z) {
  let best = null;
  for (const r of roads) {
    const d = roadDistance(r, x, z);
    if (!best || d < best.distance) best = { name: r.name, distance: d };
  }
  return best;
}

function nearestBuilding(buildings, x, z) {
  let best = null;
  for (const b of buildings) {
    const d = quadDistance(b.footprint.corners, x, z);
    if (!best || d < best.distance) best = { id: b.id, type: b.type, stem: b.stem, distance: r1(d) };
  }
  return best;
}

// ------------------------------------------------------------------ the whole dossier

// Pure: the analysis of one dump -> the sections of dossier.json (no pictures, no place header).
export function analyse(dump, centre, radius, measured = {}, opts = {}) {
  const { buildings, props, counts, propCounts } = sortEntities(dump, centre, measured, opts.catalog ?? null);
  const roads = roadsIn(dump.roads, centre, radius);
  const jn = junctions(roads);
  const street = mainStreet(roads, buildings, centre);
  const ground = terrain(dump.grid, centre, radius);
  const plots = freePlots({ grid: dump.grid, centre, radius, buildings, props, roads, trees: dump.trees, ...(opts.plots ?? {}) });
  const byType = {};
  for (const b of buildings) byType[b.type] = (byType[b.type] ?? 0) + 1;
  const notes = [];
  if (!roads.length) {
    const vars = Object.entries(dump.roadVars).map(([c, v]) => `${c}: ${v.join(", ")}`).join("; ");
    notes.push(`no road geometry could be read from the engine${vars ? ` (road entity variables: ${vars})` : ""}: roads, junctions, the main street and free plots (which keep off roads) are incomplete`);
  }
  if (opts.catalog) {
    const loose = buildings.filter((b) => !b.matched).length;
    notes.push(`buildings (types, footprints, floors, interiors) are read from the Everon catalog, docs/everon/catalog-buildings.json; ${buildings.length - loose} of ${buildings.length} matched an entity of the engine dump, which adds the door positions${loose ? `, and the other ${loose} have no entrances listed` : ""}`);
    if (counts.infrastructure) notes.push(`${counts.infrastructure} quay, pier and waterworks pieces are counted, not listed`);
  }
  if (buildings.some((b) => b.footprint.source === "world-bounds")) notes.push("some footprints are the engine's world bounds (axis aligned, so a turned building reads larger than it is): their `footprint.source` is `world-bounds`, not `measure`");
  return {
    summary: {
      buildings: buildings.length,
      byType,
      civic: buildings.filter((b) => b.civic && !b.ruin).length,
      homes: buildings.filter((b) => b.home && !b.ruin).length,
      ruins: buildings.filter((b) => b.ruin).length,
      enterable: buildings.filter((b) => b.interior.value === "yes").length,
      entrances: buildings.reduce((s, b) => s + b.entrances.length, 0),
      doors: buildings.reduce((s, b) => s + b.doors, 0),
      props: props.length,
      trees: dump.trees.length,
      roads: roads.length,
      junctions: jn.length,
      freePlots: plots.plots.length,
      walls: counts.walls,
      otherEntities: counts.other,
      portPieces: counts.infrastructure,
    },
    buildings,
    roads,
    junctions: jn,
    mainStreet: street && { road: street.road.name, roadId: street.road.id, buildingsAlong: street.buildings, at: street.at, runs: street.bearing },
    props: props.map(({ min, max, ...p }) => p),
    propCounts,
    freePlots: plots,
    terrain: ground,
    notes,
  };
}
