// tools/lib/xob.mjs - read an Enfusion .xob model without Workbench: its visual bounds and its colliders,
// and from the colliders the horizontal surfaces a trace lands on (shelf boards, counter tops).
//
// Layout, worked out from the game's own files (2026-11-14, ShelfShop_01 against the boards #1143 traced):
//   "FORM" u32be size "XOB9", then IFF chunks (id, u32be size): HEAD, COLL, VOLM, LODS (LZ4 meshes).
//   HEAD: +4 min xyz, +16 max xyz (floats), +56 u32 string blob length, +60 the strings (\0-separated):
//         names and material paths a collider record points at by index.
//   COLL: records back to back. Every record starts with
//         u8 type, u8 0xFF, u16 n, 3x3 floats (the local axes, one row per axis), 3 floats position,
//         u32 0, u16 name string, u16 game material string, u32 0      (64 bytes), then by type:
//     1    sphere    radius                                          (68)
//     2    capsule   radius, half height (along the local y axis)    (72)
//     3    box       half extents x y z                              (76)
//     7    cylinder  radius, half height (along the local y axis)    (72)
//     4    convex    u16 verts, faces, edges, half-edges; verts xyz; half-edges 3 x u16; faces 2 x u16
//     0x85 trimesh   u16 verts, tris; verts xyz; tris 3 x u16; then a BVH
//     0x86 trimesh   u16 verts, tris, u32 k, k x u32 material ranges; verts; tris; then a BVH
//   The trimesh BVH is not decoded: the next record is found by its header (type, 0xFF, an
//   orthonormal axis matrix, the two zero words), so an unknown tail never shifts what follows.
// Colliders are what a trace (and a player's foot, and LF_StoreComponent.SnapToSurface) hits, so a
// surface read from them is the height goods stand at, not the painted mesh.

export const KIND = { 1: "sphere", 2: "capsule", 3: "box", 4: "convex", 7: "cylinder", 0x85: "trimesh", 0x86: "trimesh" };
const HEADER = 64;
// A face counts as horizontal when its normal is within about 5 degrees of straight up.
export const FLAT_COS = 0.996;
// Points within this many metres of one height are one level.
export const LEVEL_EPS = 0.003;

export function chunks(buf) {
  if (!buf || buf.length < 12 || buf.toString("ascii", 0, 4) !== "FORM" || buf.toString("ascii", 8, 12) !== "XOB9") return null;
  const out = {};
  let o = 12;
  while (o + 8 <= buf.length) {
    const id = buf.toString("ascii", o, o + 4);
    const size = buf.readUInt32BE(o + 4);
    if (o + 8 + size > buf.length) break;
    if (!out[id]) out[id] = { off: o + 8, size };
    o += 8 + size;
  }
  return out;
}

export function headBounds(buf, ch = chunks(buf)) {
  const h = ch?.HEAD;
  if (!h || h.size < 28) return null;
  const f = (k) => buf.readFloatLE(h.off + 4 + k * 4);
  const min = [f(0), f(1), f(2)];
  const max = [f(3), f(4), f(5)];
  if ([...min, ...max].some((v) => !Number.isFinite(v))) return null;
  return { min, max };
}

// The HEAD string table, or [] when its length word does not fit (then records carry no names).
export function headStrings(buf, ch = chunks(buf)) {
  const h = ch?.HEAD;
  if (!h || h.size < 64) return [];
  const len = buf.readUInt32LE(h.off + 56);
  if (len <= 0 || h.off + 60 + len > h.off + h.size) return [];
  return buf.toString("latin1", h.off + 60, h.off + 60 + len).split("\0");
}

const known = (t) => t in KIND;

// A record header: a known type, 0xFF, three unit rows that are mutually perpendicular, the zero words.
export function isRecordAt(buf, p, end) {
  if (p + HEADER > end || !known(buf[p]) || buf[p + 1] !== 0xff) return false;
  if (buf.readUInt32LE(p + 52) !== 0 || buf.readUInt32LE(p + 60) !== 0) return false;
  const r = [0, 1, 2].map((i) => [0, 1, 2].map((k) => buf.readFloatLE(p + 4 + (i * 3 + k) * 4)));
  const dot = (a, b) => a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
  for (let i = 0; i < 3; i++) if (!(Math.abs(dot(r[i], r[i]) - 1) < 0.01)) return false;
  return Math.abs(dot(r[0], r[1])) < 0.01 && Math.abs(dot(r[0], r[2])) < 0.01 && Math.abs(dot(r[1], r[2])) < 0.01;
}

// Every collider: { type, kind, name, material, axes: [x, y, z rows], pos, ...shape }. `skipped` counts
// records whose size had to be found by scanning (a trimesh BVH), `truncated` a COLL that ended mid-record.
export function colliders(buf, ch = chunks(buf)) {
  const c = ch?.COLL;
  const out = { list: [], skipped: 0, truncated: false };
  if (!c) return out;
  const strings = headStrings(buf, ch);
  const end = c.off + c.size;
  const f = (o) => buf.readFloatLE(o);
  const u16 = (o) => buf.readUInt16LE(o);
  const vec = (o) => [f(o), f(o + 4), f(o + 8)];
  let p = c.off;
  while (p < end) {
    if (!isRecordAt(buf, p, end)) {
      out.truncated = true;
      break;
    }
    const type = buf[p];
    const axes = [vec(p + 4), vec(p + 16), vec(p + 28)];
    const rec = { type, kind: KIND[type], axes, pos: vec(p + 40), name: strings[u16(p + 56)] ?? null, material: strings[u16(p + 58)] ?? null };
    let next = null;
    const q = p + HEADER;
    try {
      if (type === 1) {
        rec.radius = f(q);
        next = q + 4;
      } else if (type === 2 || type === 7) {
        rec.radius = f(q);
        rec.half = f(q + 4);
        next = q + 8;
      } else if (type === 3) {
        rec.half3 = vec(q);
        next = q + 12;
      } else if (type === 4) {
        const nv = u16(q), nf = u16(q + 2), nh = u16(q + 6);
        rec.verts = Array.from({ length: nv }, (_, i) => vec(q + 8 + i * 12));
        next = q + 8 + nv * 12 + nh * 6 + nf * 4;
      } else {
        const nv = u16(q), nt = u16(q + 2);
        const vs = type === 0x86 ? q + 8 + 4 * buf.readUInt32LE(q + 4) : q + 4;
        rec.verts = Array.from({ length: nv }, (_, i) => vec(vs + i * 12));
        const ts = vs + nv * 12;
        rec.tris = Array.from({ length: nt }, (_, i) => [u16(ts + i * 6), u16(ts + i * 6 + 2), u16(ts + i * 6 + 4)]);
        if (rec.tris.some((t) => t.some((ix) => ix >= nv))) throw new Error("triangle index out of range");
        next = ts + nt * 6; // the BVH follows; found below
      }
    } catch {
      out.truncated = true;
      break;
    }
    out.list.push(rec);
    if (next === end) break;
    if (next < end && isRecordAt(buf, next, end) && type !== 0x85 && type !== 0x86) {
      p = next;
      continue;
    }
    // Scan for the next header (a trimesh's BVH, or a size this reader has wrong).
    let s = next;
    while (s < end && !isRecordAt(buf, s, end)) s++;
    if (type !== 0x85 && type !== 0x86) out.skipped++;
    p = s;
  }
  return out;
}

// Local point -> prefab space: x*axisX + y*axisY + z*axisZ + pos (Enfusion's vector[4] rows).
export function toPrefab(rec, v) {
  const [a, b, c] = rec.axes;
  return [0, 1, 2].map((k) => v[0] * a[k] + v[1] * b[k] + v[2] * c[k] + rec.pos[k]);
}

export function boxCorners(rec) {
  const [hx, hy, hz] = rec.half3;
  const out = [];
  for (const sx of [-1, 1]) for (const sy of [-1, 1]) for (const sz of [-1, 1]) out.push(toPrefab(rec, [sx * hx, sy * hy, sz * hz]));
  return out;
}

// Where a collider's points are, in prefab space (for bounds).
export function points(rec) {
  if (rec.kind === "box") return boxCorners(rec);
  if (rec.verts) return rec.verts.map((v) => toPrefab(rec, v));
  const r = rec.radius ?? 0;
  const h = (rec.half ?? 0) + (rec.kind === "capsule" ? r : 0);
  const out = [];
  for (const sx of [-1, 1]) for (const sy of [-1, 1]) for (const sz of [-1, 1]) out.push(toPrefab(rec, [sx * r, sy * (rec.kind === "sphere" ? r : h), sz * r]));
  return out;
}

export function boundsOf(pts) {
  if (!pts.length) return null;
  const min = [Infinity, Infinity, Infinity];
  const max = [-Infinity, -Infinity, -Infinity];
  for (const p of pts) for (let k = 0; k < 3; k++) {
    if (p[k] < min[k]) min[k] = p[k];
    if (p[k] > max[k]) max[k] = p[k];
  }
  return { min, max };
}

const flatTop = (pts) => {
  const y = Math.max(...pts.map((p) => p[1]));
  const top = pts.filter((p) => p[1] > y - LEVEL_EPS);
  if (top.length < 3) return null;
  const b = boundsOf(top);
  return { y, x: [b.min[0], b.max[0]], z: [b.min[2], b.max[2]] };
};

// The upward horizontal faces of one collider: [{ y, x: [min, max], z: [min, max], area }].
// A box or cylinder has one when an axis is vertical; a convex hull at most its top cap; a trimesh
// every upward triangle, joined into pieces that share a vertex at one height.
export function surfacesOf(rec) {
  const len = (v) => Math.hypot(v[0], v[1], v[2]);
  if (rec.kind === "box") {
    const up = rec.axes.some((a) => Math.abs(a[1]) / (len(a) || 1) > FLAT_COS);
    if (!up) return [];
    const s = flatTop(boxCorners(rec));
    return s ? [{ ...s, area: (s.x[1] - s.x[0]) * (s.z[1] - s.z[0]) }] : [];
  }
  if (rec.kind === "cylinder") {
    const a = rec.axes[1];
    if (!(Math.abs(a[1]) / (len(a) || 1) > FLAT_COS)) return [];
    const top = toPrefab(rec, [0, Math.sign(a[1]) * rec.half, 0]);
    return [{ y: top[1], x: [top[0] - rec.radius, top[0] + rec.radius], z: [top[2] - rec.radius, top[2] + rec.radius], area: Math.PI * rec.radius ** 2 }];
  }
  if (rec.kind === "convex") {
    const s = flatTop(rec.verts.map((v) => toPrefab(rec, v)));
    return s ? [{ ...s, area: (s.x[1] - s.x[0]) * (s.z[1] - s.z[0]) }] : [];
  }
  if (rec.kind === "trimesh") {
    const pts = rec.verts.map((v) => toPrefab(rec, v));
    const flat = [];
    for (const t of rec.tris) {
      const [a, b, c] = t.map((i) => pts[i]);
      const u = [b[0] - a[0], b[1] - a[1], b[2] - a[2]];
      const v = [c[0] - a[0], c[1] - a[1], c[2] - a[2]];
      const n = [u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0]];
      const l = len(n);
      if (!l || Math.abs(n[1]) / l <= FLAT_COS) continue;
      // Clockwise seen from above faces up (Enfusion winds its collision triangles that way).
      if (n[1] < 0) continue;
      flat.push({ t, y: (a[1] + b[1] + c[1]) / 3, area: l / 2 });
    }
    // Join triangles that share a vertex and sit at one height.
    const parent = flat.map((_, i) => i);
    const find = (i) => (parent[i] === i ? i : (parent[i] = find(parent[i])));
    const byVert = new Map();
    flat.forEach((f, i) => {
      for (const ix of f.t) {
        const key = `${ix}|${Math.round(f.y / LEVEL_EPS)}`;
        if (byVert.has(key)) parent[find(i)] = find(byVert.get(key));
        else byVert.set(key, i);
      }
    });
    const groups = new Map();
    flat.forEach((f, i) => {
      const g = find(i);
      if (!groups.has(g)) groups.set(g, []);
      groups.get(g).push(f);
    });
    return [...groups.values()].map((fs) => {
      const b = boundsOf(fs.flatMap((f) => f.t.map((i) => pts[i])));
      return { y: b.max[1], x: [b.min[0], b.max[0]], z: [b.min[2], b.max[2]], area: fs.reduce((s, f) => s + f.area, 0) };
    });
  }
  return [];
}

// Everything `tools\cli measure` prints for one model, scaled by the prefab's root scale.
// minSize: a surface narrower than this in x or z is left out (an upright's 2 mm top), unless 0.
export function measureXob(buf, { scale = 1, minSize = 0.05 } = {}) {
  const ch = chunks(buf);
  if (!ch) throw new Error("not an XOB9 model");
  const sc = (v) => v.map((n) => n * scale);
  const vb = headBounds(buf, ch);
  const col = colliders(buf, ch);
  const cb = boundsOf(col.list.flatMap(points));
  const kinds = {};
  for (const r of col.list) kinds[r.kind] = (kinds[r.kind] ?? 0) + 1;
  let surfaces = [];
  col.list.forEach((r, i) => {
    for (const s of surfacesOf(r)) surfaces.push({ ...s, collider: i, kind: r.kind, name: r.name, material: r.material });
  });
  surfaces = surfaces
    .map((s) => ({ ...s, y: s.y * scale, x: sc(s.x), z: sc(s.z), area: s.area * scale * scale }))
    .filter((s) => !minSize || (s.x[1] - s.x[0] >= minSize && s.z[1] - s.z[0] >= minSize))
    .sort((a, b) => b.y - a.y);
  return {
    bounds: vb && { min: sc(vb.min), max: sc(vb.max) },
    colliderBounds: cb && { min: sc(cb.min), max: sc(cb.max) },
    colliders: col.list.length,
    kinds,
    skipped: col.skipped,
    truncated: col.truncated,
    surfaces,
  };
}
