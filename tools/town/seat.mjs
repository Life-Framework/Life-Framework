// tools/town/seat.mjs - seat a fitted kit's rows on the island host building, offline (no game, no Workbench).
//
// `town fit` turns a kit's building-local offsets into world rows. A kit is lifted from the playtest copy of a building,
// and the island's vanilla host can differ from it (another floor height, other wall positions), so a row placed by
// offset alone floats, sinks or clips the shell (island audits on #2369, #2378, #2394). This module reads the HOST's
// model colliders, the same .xob data `tools\cli measure --surfaces` and the building surface snap read
// (tools/buildings/pipeline/surface.mjs), and corrects each row:
//
//   floor   a row standing on the floor drops (or rises) onto the host's top under its footprint centre;
//   wall    a wall-mounted row (`on: "wall"` / `mount: "wall"` / a `wall` tag on the kit row, or a hung kind:
//           electricity box, sign, poster; see surface.mjs HUNG) moves along its facing onto the nearest host wall plane;
//           a floor row whose verb is `against wall` also backs onto a wall within NEAR_WALL, keeping its floor;
//   stack   a row `on <row>` moves with the row it stands on;
//   terrain a row in open ground (kit row `"mount": "terrain"`, no building under it): it rests on the ground the server
//           probed under its footprint (tools/town/terrain.mjs, `town fit --terrain`), the highest of the centre and the four
//           corners, so a prop on a slope never sinks at its high side. Without a probed height it is left and reported;
//   ground  a row the kit stands on the ground under it (`"mount": "ground"`, a fence panel, a street bin): its y is the
//           height of its pivot over the terrain (the bottom of its bounds on the ground), no probe needed for that;
//           `town fit` writes it in the row's own frame (tools/lib/height-frame.mjs), which for a world-frame prefab
//           needs the probed terrain height;
//   clip   a row whose bounds cut a host wall is pushed out along that wall's normal by up to NUDGE; a row that still
//           cuts the shell is reported as a clip (town fit refuses it unless --allow-clip).
//
// Everything is computed in the host's own frame (host-local x, y, z, +z the prefab's front), with the same yaw rule as
// `kit apply` (tools/town/kit.mjs toWorld). The reader is injectable: the selftest passes fixture hosts.
//   reader.info(path)  -> { bounds: { min, max }, scale } | null        a row prefab's visual bounds (surface.mjs gameReader)
//   reader.shell(path) -> { mesh, surfaces: [{ y, x:[a,b], z:[a,b], area }], walls: [{ n:[nx,ny,nz], p:[[x,y,z] x3] }] } | null
import { BELOW_PIVOT, isHung, footOf, scaleOf, gameReader as surfaceReader, prefabChildren as prefabChildrenOf } from "../buildings/pipeline/surface.mjs";
import { toWorld, toLocal } from "./kit.mjs";

export const TOL = 0.02; // a row within 2 cm of its support is left alone
export const FLOOR_BAND = 0.75; // a floor row seats on a host top this far above or below its kit height, no further
export const NEAR_WALL = 0.6; // a floor row `against wall` backs onto a wall this close along its facing
export const HUNG_REACH = 6; // a wall-mounted row looks this far along its facing for its wall
export const NUDGE = 0.3; // a clipping row is pushed out of a wall by at most this much
export const CLIP_TOL = 0.03; // bounds may touch a wall by this much (paint, skirting) before it counts as a clip
export const WALL_GAP = 0.005; // a wall row stands this far off the plane
export const FLAT_TOL = 0.02; // `flatWalls`: the wall plane under a wall row's width and height may differ by this much
export const SLIDE_STEP = 0.1; // `flatWalls`: a wall row on an uneven wall slides along it in steps of this
export const SLIDE_MAX = 3; // ... and at most this far each way (a kit row's `slide` overrides it, metres)

const r3 = (n) => Math.round(n * 1000) / 1000;
const dot = (a, b) => a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
const sub = (a, b) => [a[0] - b[0], a[1] - b[1], a[2] - b[2]];
const cross = (a, b) => [a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]];
const norm = (d) => {
  let x = ((d % 360) + 360) % 360;
  if (x > 180) x -= 360;
  return x;
};
const pathOf = (ref) => String(ref ?? "").replace(/^\{[0-9A-Fa-f]{16}\}/, "");

// ------------------------------------------------------------------ the host's walls, from the model colliders

// The near-vertical faces of one collider, in prefab space: [{ n: unit outward normal, p: [a, b, c] }].
// A trimesh gives its triangles (Enfusion's winding: the cross product points out, as surfacesOf reads it);
// a box gives its two triangles per vertical side; other kinds are skipped (a building shell is a trimesh).
export function wallsOfCollider(rec, toPrefab, boxCorners, scale = 1) {
  const sc = (v) => v.map((n) => n * scale);
  const out = [];
  const push = (a, b, c) => {
    const n = cross(sub(b, a), sub(c, a));
    const l = Math.hypot(...n);
    if (!l || Math.abs(n[1]) / l > 0.7) return; // floors, ceilings and steep roofs are not walls
    out.push({ n: n.map((k) => k / l), p: [a, b, c] });
  };
  if (rec.kind === "trimesh") {
    const pts = rec.verts.map((v) => sc(toPrefab(rec, v)));
    for (const t of rec.tris) push(pts[t[0]], pts[t[1]], pts[t[2]]);
  } else if (rec.kind === "box") {
    const c = boxCorners(rec).map(sc); // index = sx*4 + sy*2 + sz over [-1, 1]
    const quads = [[0, 1, 3, 2], [4, 6, 7, 5], [0, 4, 5, 1], [2, 3, 7, 6], [0, 2, 6, 4], [1, 5, 7, 3]];
    for (const [a, b, cc, d] of quads) {
      push(c[a], c[b], c[cc]);
      push(c[a], c[cc], c[d]);
    }
  }
  return out;
}

// The props a host prefab carries itself (its vanilla furniture sets, expanded one level into their pieces), in host-local
// space: [{ path, x, y, z, yaw, scale }]. A layer cannot remove a terrain building's children, so a kit row that lands on
// one is flagged (the clothes shop-house keeps ShopHouse_E_2I01t_furniture_clothesstore_01 on the island, #2369).
// Socketed children (windows, doors, signs) are left out: they sit in the walls the shell test already covers.
function hostKids(root, vfs, gp, path, rows) {
  const out = [];
  const parentOf = (t) => /^[^\n]*?:\s*"\{[0-9A-Fa-f]{16}\}([^"]+\.et)"/.exec(t)?.[1] ?? null;
  const visit = (p, base, depth) => {
    const seen = new Set();
    for (let q = p; q && !seen.has(q) && seen.size < 6; ) {
      seen.add(q);
      const t = gp.readText(root, vfs, q);
      if (!t) break;
      for (const e of prefabChildrenOf(t)) {
        if (e.socket || !e.path || /\/Doors?\//i.test(e.path)) continue;
        const [dx, dz] = toWorld(e.x, e.z, base.yaw);
        const at = { path: e.path, x: base.x + dx, y: base.y + e.y, z: base.z + dz, yaw: base.yaw + e.yaw, scale: e.scale };
        const b = rows.info(e.path)?.bounds;
        const set = !b || Math.max(b.max[0] - b.min[0], b.max[2] - b.min[2]) > 4; // a furniture set: its pieces, not its hull
        if (!set) out.push(at);
        else if (depth < 2) visit(e.path, at, depth + 1);
      }
      q = parentOf(t);
    }
  };
  try {
    visit(path, { x: 0, y: 0, z: 0, yaw: 0 }, 0);
    // A building prefab saved from a world instance (House_Town_E_2I02_yellow) keeps that instance's own `coords` and
    // `angles` on its root, and its furniture children carry them too: take them out so a kid is in the building's own
    // frame, as the kit rows and the shell are (the barber's chairs sat on the sofa and nothing flagged it, #3236).
    const rootText = gp.readText(root, vfs, path) ?? "";
    const rc = /^ coords\s+(\S+)\s+(\S+)\s+(\S+)/m.exec(rootText);
    const ra = /^ angles\s+(\S+)\s+(\S+)\s+(\S+)/m.exec(rootText);
    if (rc && Number(rc[1]) !== 0) {
      const yaw = ra ? Number(ra[2]) : 0;
      for (const k of out) {
        const [lx, lz] = toLocal(k.x - Number(rc[1]), k.z - Number(rc[3]), yaw);
        k.x = lx;
        k.z = lz;
        k.yaw = k.yaw - yaw;
      }
    }
  } catch {
    // an unreadable child list is no furniture to flag
  }
  return out;
}

// The default reader: row bounds from surface.mjs (prefabInfo), host shells from the .xob in addons/ or the game's paks.
export function shellReader(root) {
  const rows = surfaceReader(root);
  const shells = new Map();
  let libs = null;
  return {
    info: (path) => rows.info(pathOf(path)),
    async shell(ref) {
      const path = pathOf(ref);
      if (shells.has(path)) return shells.get(path);
      let out = null;
      try {
        if (!libs) {
          const gp = await import("../lib/game-prefab.mjs");
          libs = { gp, xob: await import("../lib/xob.mjs"), vfs: await gp.gameVfs().catch(() => null) };
        }
        const { gp, xob, vfs } = libs;
        const { mesh, scale } = gp.meshOf(root, vfs, path);
        const buf = mesh ? gp.readBytes(root, vfs, mesh) : null;
        if (buf) {
          const m = xob.measureXob(buf, { scale });
          const walls = xob.colliders(buf).list.flatMap((rec) => wallsOfCollider(rec, xob.toPrefab, xob.boxCorners, scale));
          out = { mesh, surfaces: m.surfaces, walls, kids: hostKids(root, vfs, gp, path, rows) };
        }
      } catch {
        out = null;
      }
      shells.set(path, out);
      return out;
    },
  };
}

// ------------------------------------------------------------------ geometry helpers (host-local)

// A row's box in host-local space: centre, half sizes, its own axes (x, y, z unit vectors).
function boxOf(row, info, shrink = 0) {
  const k = scaleOf(row, info);
  const b = info.bounds;
  const yawR = (row.yaw * Math.PI) / 180;
  const [ox, oz] = toWorld((b.min[0] + b.max[0]) / 2 * k, (b.min[2] + b.max[2]) / 2 * k, row.yaw);
  const ax = [Math.cos(yawR), 0, -Math.sin(yawR)];
  const az = [Math.sin(yawR), 0, Math.cos(yawR)];
  const h = [0, 1, 2].map((i) => Math.max(0, ((b.max[i] - b.min[i]) / 2) * k - shrink));
  return { c: [row.x + ox, row.y + ((b.min[1] + b.max[1]) / 2) * k, row.z + oz], h, axes: [ax, [0, 1, 0], az] };
}

// Triangle against an oriented box: the separating axis test (box face normals, the triangle normal, nine edge crosses).
export function triCutsBox(tri, box) {
  const v = tri.map((p) => {
    const d = sub(p, box.c);
    return box.axes.map((a) => dot(d, a));
  });
  const h = box.h;
  const e = [sub(v[1], v[0]), sub(v[2], v[1]), sub(v[0], v[2])];
  const sep = (axis) => {
    const l = Math.hypot(...axis);
    if (l < 1e-9) return false;
    const p = v.map((q) => dot(q, axis));
    const r = h[0] * Math.abs(axis[0]) + h[1] * Math.abs(axis[1]) + h[2] * Math.abs(axis[2]);
    return Math.min(...p) > r || Math.max(...p) < -r;
  };
  for (let i = 0; i < 3; i++) {
    const a = [0, 0, 0];
    a[i] = 1;
    if (sep(a)) return false;
  }
  if (sep(cross(e[0], e[1]))) return false;
  for (let i = 0; i < 3; i++) {
    for (let j = 0; j < 3; j++) {
      const a = [0, 0, 0];
      a[i] = 1;
      if (sep(cross(a, e[j]))) return false;
    }
  }
  return true;
}

const corners = (box) => {
  const out = [];
  for (const sx of [-1, 1]) for (const sy of [-1, 1]) for (const sz of [-1, 1]) out.push([0, 1, 2].map((k) => box.c[k] + sx * box.h[0] * box.axes[0][k] + sy * box.h[1] * box.axes[1][k] + sz * box.h[2] * box.axes[2][k]));
  return out;
};

// The walls a box cuts (bounding-box prefilter, then the exact test).
function cutWalls(walls, box) {
  const reach = Math.hypot(...box.h);
  const hits = [];
  for (const w of walls) {
    let near = true;
    for (let k = 0; k < 3 && near; k++) {
      const lo = Math.min(w.p[0][k], w.p[1][k], w.p[2][k]);
      const hi = Math.max(w.p[0][k], w.p[1][k], w.p[2][k]);
      if (lo > box.c[k] + reach || hi < box.c[k] - reach) near = false;
    }
    if (near && triCutsBox(w.p, box)) hits.push(w);
  }
  return hits;
}

// Ray from `o` along `dir` (horizontal) against the walls that face back along it: the nearest hit within `max`, at height
// `y` (the row's centre height). Returns { t, point, wall } or null.
function rayWall(walls, o, dir, max) {
  let best = null;
  for (const w of walls) {
    if (dot(w.n, dir) > -0.85) continue; // the wall must face the ray (its outward side towards the row)
    const denom = dot(w.n, dir);
    const t = dot(sub(w.p[0], o), w.n) / denom;
    if (t < -0.001 || t > max || (best && t >= best.t)) continue;
    const q = [o[0] + dir[0] * t, o[1] + dir[1] * t, o[2] + dir[2] * t];
    // inside the triangle (barycentric, a small slack so a seam between two triangles still hits)
    const [a, b, c] = w.p;
    const v0 = sub(c, a), v1 = sub(b, a), v2 = sub(q, a);
    const d00 = dot(v0, v0), d01 = dot(v0, v1), d02 = dot(v0, v2), d11 = dot(v1, v1), d12 = dot(v1, v2);
    const inv = d00 * d11 - d01 * d01;
    if (Math.abs(inv) < 1e-12) continue;
    const u = (d11 * d02 - d01 * d12) / inv;
    const vv = (d00 * d12 - d01 * d02) / inv;
    if (u < -1e-3 || vv < -1e-3 || u + vv > 1 + 1e-3) continue;
    best = { t, point: q, wall: w };
  }
  return best;
}

// How far two upright boxes (yaw only) overlap: the smallest overlap over the four plan axes, 0 when apart or when their
// heights do not meet.
export function boxOverlap(a, b) {
  if (a.c[1] + a.h[1] <= b.c[1] - b.h[1] || b.c[1] + b.h[1] <= a.c[1] - a.h[1]) return 0;
  let least = Infinity;
  for (const ax of [a.axes[0], a.axes[2], b.axes[0], b.axes[2]]) {
    const proj = (x) => x.h[0] * Math.abs(dot(x.axes[0], ax)) + x.h[2] * Math.abs(dot(x.axes[2], ax));
    const gap = Math.abs(dot(sub(a.c, b.c), ax));
    const o = proj(a) + proj(b) - gap;
    if (o <= 0) return 0;
    least = Math.min(least, o);
  }
  return least;
}
export const FURNITURE_OVERLAP = 0.05; // a kit row this deep in one of the host's own props is flagged

// The host tops under host-local (x, z): [y], any order.
function topsUnder(surfaces, x, z, edge = 0.03) {
  return surfaces.filter((s) => x >= s.x[0] - edge && x <= s.x[1] + edge && z >= s.z[0] - edge && z <= s.z[1] + edge).map((s) => s.y);
}

// ------------------------------------------------------------------ classifying a kit row

// The world (x, z) points a terrain row's footprint touches: the centre first, then the four corners of its bounds turned
// by its yaw. A row with no bounds (a marker) has just its own point. Pure; keys the probe's answers (terrain.mjs pinKey).
export function footprintPoints(coords, yaw, info, scale = null) {
  const b = info?.bounds;
  if (!b || !Number.isFinite(b.min?.[0])) return [[r3(coords[0]), r3(coords[2])]];
  // A prop whose mesh runs below its pivot (a pole, the site flag: -0.8 m) stands by the pivot, as the audit judges it
  // (surface.mjs footOf): the ground under the pivot is the one that matters, not the cloth's corners.
  if (b.min[1] < -BELOW_PIVOT) return [[r3(coords[0]), r3(coords[2])]];
  const k = scaleOf({ scale }, info);
  const at = (x, z) => {
    const [dx, dz] = toWorld(x * k, z * k, yaw);
    return [r3(coords[0] + dx), r3(coords[2] + dz)];
  };
  const [cx, cz] = [(b.min[0] + b.max[0]) / 2, (b.min[2] + b.max[2]) / 2];
  return [at(cx, cz), at(b.min[0], b.min[2]), at(b.max[0], b.min[2]), at(b.min[0], b.max[2]), at(b.max[0], b.max[2])];
}

// How a kit row is held: "wall", "floor" (with `against` when its verb is `against wall`), "stack" (on another row), "free",
// "terrain" (open ground, probed) or "ground" (on the ground, its y the height over the terrain; "relative" is the old name).
// The layer class token says nothing about the frame: that is the prefab's RELATIVE_Y flag (tools/lib/height-frame.mjs).
export function mountOf(src) {
  const tags = [].concat(src.tags ?? [], src.kind ?? []).map(String);
  const verb = String(src.verb ?? "");
  if (src.free || src.mount === "free") return { mount: "free" };
  if (src.mount === "ground" || src.mount === "relative") return { mount: "ground" };
  if (src.mount === "terrain") return { mount: "terrain" };
  if (src.on === "wall" || src.mount === "wall" || tags.includes("wall") || tags.includes("wall-mounted") || isHung(src.prefab) || src.role === "sign") return { mount: "wall" };
  if (verb.startsWith("on ") && verb !== "on floor") return { mount: "stack", on: verb.slice(3) };
  return { mount: "floor", against: verb === "against wall" };
}

const socketOf = (src) => {
  for (const c of src.config ?? []) if (c.component === "Hierarchy" && c.set?.PivotID) return c.set.PivotID.replace(/"/g, "");
  return null;
};

// ------------------------------------------------------------------ seating

// Seat world rows (applyKit output, coords mutated in place) in `host` ({ x, y, z, yaw, prefab|path }).
//   kitRows: the kit's own rows (for verb, role, tags, config); reader: shellReader() or a fixture.
// Returns { ok, host: { mesh } | null, rows: [{ id, mount, from, to, moved, dy, wall, clip, notes }], clips, unseated }.
// flatWalls (`town stamp`, #3931): a wall row must stand flush on a wall that is flat under its whole plate. Where the wall under
// its centre steps (a door recess, a lintel, a neighbour's gable: the b59 fascia clipped at z <= 6.35 and floated at z >= 6.45),
// the row slides along the wall to the nearest run that is flat and cuts nothing; none within the reach leaves it seated at its
// centre and says so.
export async function seatRows(rows, kitRows, host, { reader, allowClip = false, terrain = null, flatWalls = false } = {}) {
  const shell = await reader.shell(host.prefab ?? host.path);
  if (!shell) return { ok: false, host: null, rows: [], clips: [], unseated: [], note: `the host ${pathOf(host.prefab ?? host.path)} has no readable model; rows were not seated` };
  const src = new Map(kitRows.map((r) => [r.id, r]));
  const toL = (c, yaw) => {
    const [lx, lz] = toLocal(c[0] - host.x, c[2] - host.z, host.yaw ?? 0);
    return { x: lx, y: c[1] - host.y, z: lz, yaw: norm(yaw - (host.yaw ?? 0)) };
  };
  const toW = (l) => {
    const [wx, wz] = toWorld(l.x, l.z, host.yaw ?? 0);
    return [r3(host.x + wx), r3(host.y + l.y), r3(host.z + wz)];
  };
  const out = [];
  const byId = new Map();
  const order = [...rows].sort((a, b) => (mountOf(src.get(a.id) ?? {}).mount === "stack") - (mountOf(src.get(b.id) ?? {}).mount === "stack"));
  for (const row of order) {
    const s = src.get(row.id) ?? {};
    const m = mountOf({ ...s, prefab: s.prefab ?? row.prefab });
    const l = { ...toL(row.coords, row.yaw), scale: s.scale ?? null };
    const from = [...row.coords];
    const res = { id: row.id, mount: m.mount, from, to: from, moved: 0, dy: 0, wall: null, clip: null, notes: [] };
    const sock = socketOf(s);
    const info = reader.info(row.prefab);
    const bounded = info?.bounds && Number.isFinite(info.bounds.min?.[0]);
    if (sock && flatWalls) res.notes.push(`hangs on socket ${sock}: kept as the kit puts it (a stamp never drops a row)`);
    else if (sock) {
      // Its offset counts from a socket of the source building (a Hierarchy PivotID), not from the building's origin, and a
      // layer cannot hang an entity on a terrain building's socket: placed by offset it lands inside the shell (the bank
      // fascia floated 5 m, #2378). Left out; the kit needs the row as a building-space wall row.
      res.skip = `hangs on socket ${sock} of the kit's source building: its offset is socket-relative, so it is left out of the layer (give the kit row a building-space offset)`;
      res.notes.push(res.skip);
    } else if (m.mount === "free") res.notes.push("free: left as the kit puts it");
    else if (m.mount === "ground") {
      // Its y becomes the height of its pivot over the terrain under it: the bottom of its bounds on the ground (0 for a
      // prop whose pivot is its base, the fence panel). `town fit` turns it into the prefab's frame when it writes the row.
      const f = bounded ? footOf(l, info) : null;
      l.y = (f ? -f.bottom : 0) - host.y;
      res.overGround = true;
      res.notes.push("on the ground: y is its height over the terrain under it");
      // A kit row's `lift` (metres) raises the pivot over the terrain under it: a fence panel on a slope stands with its pivot
      // (the low end) as far up as the ground rises to its middle, or the high half of the panel is under the grass (#3968).
      if (Number.isFinite(s.lift) && s.lift !== 0) {
        l.y += s.lift;
        res.notes.push(`lifted ${s.lift} m over the terrain at its pivot`);
      }
    } else if (m.mount === "terrain") {
      const world = toW(l);
      const pts = footprintPoints(world, row.yaw, bounded ? info : null, l.scale);
      res.samples = pts;
      const heights = terrain ? pts.map((p) => terrain.at(p[0], p[1])) : [];
      if (!terrain || heights.some((h) => !Number.isFinite(h))) res.notes.push("no probed terrain height under it: kept at the kit height (run town fit with --terrain)");
      else {
        const f = bounded ? footOf(l, info) : { bottom: 0 };
        l.y = Math.max(...heights) - f.bottom - host.y;
        // The same kit `lift` as the floor branch (metres, negative sinks it): a desk whose legs sit 1.5 cm over the probed ground (#3685).
        if (Number.isFinite(s.lift) && s.lift !== 0) {
          l.y += s.lift;
          res.notes.push(`lifted ${s.lift} m from the probed ground`);
        }
        res.notes.push(`on the terrain: ${r3(Math.min(...heights))} to ${r3(Math.max(...heights))} m under its footprint`);
      }
    } else if (m.mount === "stack") {
      const base = byId.get(m.on);
      if (base) {
        l.x += base.d[0];
        l.y += base.d[1];
        l.z += base.d[2];
        res.notes.push(`moves with ${m.on}`);
      } else res.notes.push(`stands on ${m.on}, which is not a seated row; left as the kit puts it`);
    } else if (!bounded) {
      res.notes.push("no bounds for this prefab (a furniture set or an unmeasured prop): kept at the kit height");
    } else {
      const k = scaleOf(l, info);
      const b = info.bounds;
      const fwd = [Math.sin((l.yaw * Math.PI) / 180), 0, Math.cos((l.yaw * Math.PI) / 180)];
      const cy = l.y + ((b.min[1] + b.max[1]) / 2) * k;
      // A wall row looks behind it (its back, -z, to the wall) first, then a short way in front; a floor row against a
      // wall looks both ways a short distance. The row's face touches the plane.
      const wallSnap = (behind, ahead) => {
        const [ox, oz] = toWorld(((b.min[0] + b.max[0]) / 2) * k, 0, l.yaw);
        const o = [l.x + ox, cy, l.z + oz];
        const back = rayWall(shell.walls, [o[0] + fwd[0] * 0.05, o[1], o[2] + fwd[2] * 0.05], [-fwd[0], 0, -fwd[2]], behind + 0.05);
        const front = ahead > 0 ? rayWall(shell.walls, [o[0] - fwd[0] * 0.05, o[1], o[2] - fwd[2] * 0.05], fwd, ahead + 0.05) : null;
        const pick = back && (!front || back.t <= front.t + 0.2) ? { ...back, side: "back" } : front ? { ...front, side: "front" } : null;
        if (!pick) return null;
        // the face along the facing that should touch the wall, from the pivot
        const face = pick.side === "back" ? b.min[2] * k - WALL_GAP : b.max[2] * k + WALL_GAP;
        const along = dot([pick.point[0] - (l.x + ox), 0, pick.point[2] - (l.z + oz)], fwd);
        const shift = along - face;
        return { shift, side: pick.side };
      };
      if (m.mount === "wall") {
        const w = wallSnap(HUNG_REACH, NEAR_WALL);
        // A facade that steps back under a long plate leaves one end in the wall (pharmacy fascia, 6.6 cm, #3685): the kit row
        // says how far to stand off the plane, `standoff` in metres, so the whole plate clears it.
        const standoff = (side) => (Number.isFinite(s.standoff) ? (side === "back" ? -s.standoff : s.standoff) : 0);
        const seatOn = (pick) => {
          const off = standoff(pick.side);
          l.x += fwd[0] * (pick.shift - off);
          l.z += fwd[2] * (pick.shift - off);
          return off;
        };
        if (w) {
          const off = seatOn(w);
          res.wall = { side: w.side, shift: r3(w.shift - off) };
          if (off) res.notes.push(`stands ${Math.abs(off)} m off the wall`);
          if (flatWalls) {
            const yawR = (l.yaw * Math.PI) / 180;
            const ax = [Math.cos(yawR), 0, -Math.sin(yawR)];
            const [ox, oz] = toWorld(((b.min[0] + b.max[0]) / 2) * k, 0, l.yaw);
            const hx = Math.max(0, ((b.max[0] - b.min[0]) / 2) * k - 0.02);
            const hy = ((b.max[1] - b.min[1]) / 2) * k * 0.8;
            // the wall plane at nine points over the plate (3 across, 3 up): how far along the facing each lies; a miss is a hole
            const spread = (side) => {
              const dir = side === "back" ? [-fwd[0], 0, -fwd[2]] : fwd;
              const sign = side === "back" ? 1 : -1;
              const alongs = [];
              for (const dx of [-hx, 0, hx]) {
                for (const dy of [-hy, 0, hy]) {
                  const base = [l.x + ox + ax[0] * dx, cy + dy, l.z + oz + ax[2] * dx];
                  const hit = rayWall(shell.walls, [base[0] + fwd[0] * 0.05 * sign, base[1], base[2] + fwd[2] * 0.05 * sign], dir, NEAR_WALL + 0.05);
                  if (!hit) return Infinity;
                  alongs.push(dot([hit.point[0] - base[0], 0, hit.point[2] - base[2]], fwd));
                }
              }
              return Math.max(...alongs) - Math.min(...alongs);
            };
            const flat = (side) => spread(side) <= FLAT_TOL && !cutWalls(shell.walls, boxOf(l, info, CLIP_TOL)).length;
            if (!flat(w.side)) {
              const reach = Number.isFinite(s.slide) ? s.slide : SLIDE_MAX;
              const home = { x: l.x, z: l.z };
              let slid = null;
              for (let i = 1; i <= Math.round(reach / SLIDE_STEP) && slid === null; i++) {
                for (const sgn of [1, -1]) {
                  const d = r3(sgn * i * SLIDE_STEP);
                  l.x = home.x + ax[0] * d;
                  l.z = home.z + ax[2] * d;
                  const w2 = wallSnap(HUNG_REACH, NEAR_WALL);
                  if (w2) {
                    seatOn(w2);
                    if (flat(w2.side)) {
                      slid = { d, side: w2.side, shift: r3(w2.shift - standoff(w2.side)) };
                      break;
                    }
                  }
                  l.x = home.x;
                  l.z = home.z;
                }
              }
              if (slid) {
                res.wall = { side: slid.side, shift: slid.shift, slide: slid.d };
                res.notes.push(`slid ${slid.d} m along the wall to a flat run (the wall under its centre steps)`);
              } else {
                l.x = home.x;
                l.z = home.z;
                res.notes.push(`no flat wall run within ${reach} m along the wall: left seated at the wall under its centre`);
              }
            }
          }
        } else res.notes.push(`no host wall within ${HUNG_REACH} m behind or ${NEAR_WALL} m in front of it`);
      } else {
        const f = footOf(l, info);
        const bottom = l.y + f.bottom;
        const [lcx, lcz] = [f.cx, f.cz];
        const tops = topsUnder(shell.surfaces, lcx, lcz).filter((y) => Math.abs(y - bottom) <= FLOOR_BAND);
        if (tops.length) {
          const top = tops.sort((a, b2) => Math.abs(a - bottom) - Math.abs(b2 - bottom))[0];
          if (Math.abs(top - bottom) > TOL) l.y += top - bottom;
        } else res.notes.push(`no host floor within ${FLOOR_BAND} m of its kit height under it`);
        // A host whose walkable floor lies below its model's top surface (the House_Town_E_2I02 hall tiles: 4.5 cm, measured by
        // `look contact`, #3236): the kit row says how far, as `lift` in metres (negative sinks it), applied after the snap.
        if (Number.isFinite(s.lift) && s.lift !== 0 && tops.length) {
          l.y += s.lift;
          res.notes.push(`lifted ${s.lift} m from the model's top surface`);
        }
        if (m.against) {
          const w = wallSnap(NEAR_WALL, NEAR_WALL);
          if (w && Math.abs(w.shift) > TOL) {
            l.x += fwd[0] * w.shift;
            l.z += fwd[2] * w.shift;
            res.wall = { side: w.side, shift: r3(w.shift) };
          }
        }
      }
      // Clip: push out of any wall it cuts, NUDGE at most, then judge again.
      if (m.mount !== "wall") {
        let cut = cutWalls(shell.walls, boxOf(l, info, CLIP_TOL));
        let pushed = 0;
        for (let i = 0; i < 4 && cut.length; i++) {
          const box = boxOf(l, info, CLIP_TOL);
          const w = cut[0];
          const n = [w.n[0], 0, w.n[2]];
          const nl = Math.hypot(...n) || 1;
          const nn = n.map((v) => v / nl);
          const d = dot(w.p[0], nn);
          const side = dot(box.c, nn) - d >= 0 ? 1 : -1;
          const depth = Math.max(...corners(box).map((c) => side * (d - dot(c, nn)))) + 0.002;
          if (pushed + depth > NUDGE) break;
          l.x += nn[0] * side * depth;
          l.z += nn[2] * side * depth;
          pushed += depth;
          cut = cutWalls(shell.walls, boxOf(l, info, CLIP_TOL));
        }
        if (pushed > 0 && !cut.length) res.notes.push(`pushed ${r3(pushed)} m out of a host wall`);
        if (cut.length) {
          const c = cutWalls(shell.walls, boxOf(l, info, CLIP_TOL));
          res.clip = { walls: c.length, text: `cuts ${c.length} host wall face(s) after a ${NUDGE} m nudge` };
        }
        // The host's own props (its vanilla furniture): flagged, not moved; the fix is the kit or the building's override.
        const mine = boxOf(l, info);
        const hits = [];
        for (const kid of shell.kids ?? []) {
          const ki = reader.info(kid.path);
          if (!ki?.bounds) continue;
          const o = boxOverlap(mine, boxOf(kid, ki));
          if (o > FURNITURE_OVERLAP) hits.push(`${kid.path.split("/").pop().replace(/\.et$/, "")} (${r3(o)} m)`);
        }
        if (hits.length) res.furniture = `overlaps the host's own furniture, which the island building keeps: ${[...new Set(hits)].slice(0, 4).join(", ")}${hits.length > 4 ? ", ..." : ""}`;
      }
    }
    const to = toW(l);
    res.to = to;
    res.d = [to[0] - from[0], to[1] - from[1], to[2] - from[2]].map(r3);
    res.dy = res.d[1];
    res.moved = r3(Math.hypot(...res.d));
    // a stacked row follows its support in host-local terms
    const dl = toL(to, row.yaw);
    const fl = toL(from, row.yaw);
    byId.set(row.id, { d: [dl.x - fl.x, dl.y - fl.y, dl.z - fl.z] });
    row.coords = to;
    out.push(res);
  }
  const clips = out.filter((r) => r.clip);
  const keep = rows.map((r) => out.find((o) => o.id === r.id));
  // every (x, z) a terrain row stands on: what `town fit --terrain` asks the server for
  const terrainPoints = out.flatMap((r) => r.samples ?? []);
  return { ok: allowClip || !clips.length, host: { mesh: shell.mesh }, rows: keep, terrainPoints, clips, skipped: out.filter((r) => r.skip), furniture: out.filter((r) => r.furniture), unseated: out.filter((r) => r.notes.some((n) => /^no /.test(n))) };
}

// Does the host show the same model as the building the kit was lifted from? Then the kit's offsets are in the host's own
// frame and the kit sits at the host's origin and yaw, never turned to an entrance.
export async function sameShell(kit, hostRef, reader) {
  const kitHost = kit.building?.vanilla ?? kit.building?.via;
  if (!kitHost || !hostRef) return false;
  const [a, b] = [await reader.shell(kitHost), await reader.shell(hostRef)];
  return Boolean(a?.mesh && b?.mesh && a.mesh.toLowerCase() === b.mesh.toLowerCase());
}

// One line per row for `town fit --check`.
export function seatReport(seat, { against = null } = {}) {
  const lines = [];
  for (const r of seat.rows) {
    const was = against?.get(r.id) ?? r.from;
    const d = r.to.map((v, i) => r3(v - was[i]));
    const moved = r3(Math.hypot(...d));
    const tags = [r.mount, r.wall ? `wall ${r.wall.side} ${r.wall.shift} m${r.wall.slide ? ` slid ${r.wall.slide} m` : ""}` : null, r.clip ? "CLIP" : null, r.furniture ? "FURN" : null].filter(Boolean).join(", ");
    lines.push(`${r.skip ? "SKIP " : r.clip ? "CLIP " : moved > TOL ? "fix  " : "ok   "} ${r.id.padEnd(44)} ${moved > TOL ? `moves ${moved} m (dx ${d[0]} dy ${d[1]} dz ${d[2]})` : "stays"}  [${tags}]${r.notes.length ? `  ${r.notes.join("; ")}` : ""}${r.clip ? `  ${r.clip.text}` : ""}${r.furniture ? `  ${r.furniture}` : ""}`);
  }
  return lines;
}
