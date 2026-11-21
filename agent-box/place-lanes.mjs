// agent-box/place-lanes.mjs - shop walk lanes, action reach and room crowding, judged from the boxes the placement audit measured
// (docs/architecture/placement-system.md "The audit", card #3883). Pure: no server, no files; `selftest()` runs inside
// `tools\cli audit selftest` (agent-box-selftest). place-audit.mjs `analyse` calls laneRows / actionProblems / childRows.
//
// What a room is: a building the audit measured (a layer entity or a base-map object whose box is a building) that has a ground-floor
// door leaf (the `door` lines of LF_PlacementAudit.DoorLines). Its floor is the lowest ground leaf's bottom; its props are the measured
// boxes whose centre is inside the building's footprint and whose bottom is within 0.5 m of that floor (a till on a counter, stock on a
// shelf, a sign on a wall is not an obstacle; the holder under it is).
//
// Checks (limits in LANE_LIMITS, metres):
//   lane     a 1.2 m wide lane (a body 0.6 m either side of a centre line) runs from each entrance to the till, and to a face of each
//            shelf, rack or display: some free cell of that lane map lies within 1.1 m of the thing, and the entrance reaches it
//   reach    every interactable (a prop with an action) is reachable from the entrance through a 0.9 m lane and has a standing cell within
//            1.5 m: nothing walls it in (the audit's `unreachable` only asks for a spot, never for a way there from the door)
//   crowded  prop footprints over the room's floor area over 0.30 (overlapping props count twice: that is the point)
//   action-off-mesh   an action on a mesh-less entity more than 0.5 m from any mesh: the F key never offers it (the clothing till, #3901)
//   action-crowds-till  another action within 0.5 m of a till: the interact key picks it instead (a radio behind the register, #3901)
//   blocks-door (prefab children)  furniture baked into a building prefab (a `child` line: same fields as a `door` line) inside a door
//            leaf's swing: the door test used to read layer entities only
// And `aimVerdict`: what the harness `aim` verb (LF_AimHarness, after `stand`) says the F key acts on, checked against the action the
// standing spot is for.

import { isKind, LIMITS, obbAabb, obbPenetration, doorZone, parseDoor, parseKv, rank } from "./place-audit.mjs";

export const LANE_LIMITS = {
  shopLane: 1.2, // the walk lane to a till and to each shelf face
  interactLane: 0.9, // the lane past an interactable
  shopReach: 1.1, // a till or shelf: a free lane cell within this of its footprint
  actionReach: 1.5, // an interactable: a standing cell within this of its footprint (the audit's `walk` limit)
  crowding: 0.3, // prop footprint area over floor area
  offMesh: 0.5, // an action context this far from every mesh is out of the player's reach
  tillClear: 0.5, // no other action this close to a till
  minHeight: 0.3, // lower than this is stepped over
  maxBottom: 0.5, // a prop whose bottom is higher above the floor than this rests on something else
  maxTop: 1.5, // a prop whose bottom is higher than this hangs above the lane
  cell: 0.1,
};

export const LANE_WEIGHT = { lane: 7, reach: 6, crowded: 5, "action-off-mesh": 6, "action-crowds-till": 6 };
export const LANE_LAYER = "(shop lanes)";

const r2 = (n) => Math.round(n * 100) / 100;
const label = (e) => e.name || (e.prefab ? e.prefab.split("/").pop().replace(/\.et$/, "") : "unnamed");
const dot2 = (a, b) => a[0] * b[0] + a[1] * b[1];

const TILL_RX = /Till|Cashier|Register/i;
const SHELF_RX = /Shelf|Shelves|Rack|Gondola|Display/i;

export const isTill = (e) => TILL_RX.test(`${e.name || ""} ${e.prefab || ""}`) || (e.actions ?? []).some((a) => TILL_RX.test(a));
export const isShelf = (e) => SHELF_RX.test(`${e.name || ""} ${(e.prefab || "").split("/").pop()}`) && !isTill(e);

// The ground-plane rectangle of an oriented box: the two axes that lie flat, with their half extents. A box with fewer than two flat
// axes (a prop laid on its side) falls back to its world AABB.
export function footprint(B) {
  const flat = [0, 1, 2].filter((i) => Math.abs(B.a[i][1]) < 0.7);
  if (flat.length < 2) {
    const bb = obbAabb(B);
    return { c: [(bb.min[0] + bb.max[0]) / 2, (bb.min[2] + bb.max[2]) / 2], u: [1, 0], v: [0, 1], hu: (bb.max[0] - bb.min[0]) / 2, hv: (bb.max[2] - bb.min[2]) / 2 };
  }
  const norm = (a) => {
    const l = Math.hypot(a[0], a[2]) || 1;
    return [a[0] / l, a[2] / l];
  };
  const [i, j] = flat;
  return { c: [B.c[0], B.c[2]], u: norm(B.a[i]), v: norm(B.a[j]), hu: B.h[i], hv: B.h[j] };
}

export const footprintArea = (f) => 4 * f.hu * f.hv;

// Distance from a ground point to a rectangle (0 inside).
export function rectDistance(f, x, z) {
  const d = [x - f.c[0], z - f.c[1]];
  const dx = Math.max(Math.abs(dot2(d, f.u)) - f.hu, 0);
  const dz = Math.max(Math.abs(dot2(d, f.v)) - f.hv, 0);
  return Math.hypot(dx, dz);
}

// A flat-top (z-up) walkable grid over a rectangle: cell (i, j) is the point (x0 + (i + 0.5) cell, z0 + (j + 0.5) cell).
// blocked[k] marks cells whose centre is closer than `body` to an obstacle footprint (the lane's half width).
export function laneMap(room, obstacles, body, cell = LANE_LIMITS.cell) {
  const nx = Math.max(1, Math.ceil((room.max[0] - room.min[0]) / cell));
  const nz = Math.max(1, Math.ceil((room.max[1] - room.min[1]) / cell));
  const blocked = new Uint8Array(nx * nz);
  for (const f of obstacles) {
    const reach = body + Math.max(f.hu, f.hv) * Math.SQRT2;
    const i0 = Math.max(0, Math.floor((f.c[0] - reach - room.min[0]) / cell));
    const i1 = Math.min(nx - 1, Math.floor((f.c[0] + reach - room.min[0]) / cell));
    const j0 = Math.max(0, Math.floor((f.c[1] - reach - room.min[1]) / cell));
    const j1 = Math.min(nz - 1, Math.floor((f.c[1] + reach - room.min[1]) / cell));
    for (let j = j0; j <= j1; j++)
      for (let i = i0; i <= i1; i++) {
        if (blocked[j * nx + i]) continue;
        if (rectDistance(f, room.min[0] + (i + 0.5) * cell, room.min[1] + (j + 0.5) * cell) < body) blocked[j * nx + i] = 1;
      }
  }
  return { nx, nz, cell, room, blocked };
}

// The cells an 8-way walk reaches from (x, z) over free cells; null when the start cell itself is blocked or off the map.
export function flood(map, x, z) {
  const { nx, nz, cell, room, blocked } = map;
  const si = Math.floor((x - room.min[0]) / cell);
  const sj = Math.floor((z - room.min[1]) / cell);
  if (si < 0 || sj < 0 || si >= nx || sj >= nz || blocked[sj * nx + si]) return null;
  const seen = new Uint8Array(nx * nz);
  const queue = [sj * nx + si];
  seen[queue[0]] = 1;
  for (let q = 0; q < queue.length; q++) {
    const k = queue[q];
    const i = k % nx;
    const j = (k - i) / nx;
    for (let dj = -1; dj <= 1; dj++)
      for (let di = -1; di <= 1; di++) {
        const a = i + di;
        const b = j + dj;
        if ((!di && !dj) || a < 0 || b < 0 || a >= nx || b >= nz) continue;
        const n = b * nx + a;
        if (seen[n] || blocked[n]) continue;
        // No squeezing diagonally between two blocked cells.
        if (di && dj && blocked[j * nx + a] && blocked[b * nx + i]) continue;
        seen[n] = 1;
        queue.push(n);
      }
  }
  return seen;
}

// True when a reached cell lies within `within` metres of the footprint.
export function reaches(map, seen, f, within) {
  if (!seen) return false;
  const { nx, cell, room } = map;
  const r = within + Math.max(f.hu, f.hv) * Math.SQRT2;
  const i0 = Math.max(0, Math.floor((f.c[0] - r - room.min[0]) / cell));
  const i1 = Math.min(nx - 1, Math.floor((f.c[0] + r - room.min[0]) / cell));
  const j0 = Math.max(0, Math.floor((f.c[1] - r - room.min[1]) / cell));
  const j1 = Math.min(map.nz - 1, Math.floor((f.c[1] + r - room.min[1]) / cell));
  for (let j = j0; j <= j1; j++)
    for (let i = i0; i <= i1; i++) if (seen[j * nx + i] && rectDistance(f, room.min[0] + (i + 0.5) * cell, room.min[1] + (j + 0.5) * cell) <= within) return true;
  return false;
}

// Where a person steps in: the door leaf's centre pushed 0.7 m along its thin axis toward the room's middle.
export function entrancePoint(door, room) {
  const f = footprint(door.box);
  const thin = f.hu <= f.hv ? f.u : f.v;
  const mid = [(room.min[0] + room.max[0]) / 2, (room.min[1] + room.max[1]) / 2];
  const sign = dot2([mid[0] - f.c[0], mid[1] - f.c[1]], thin) >= 0 ? 1 : -1;
  return [f.c[0] + thin[0] * sign * 0.7, f.c[1] + thin[1] * sign * 0.7];
}

const isBuildingEntry = ({ e, m }) => !m.missing && !m.point && (isKind(e, "building") || 8 * m.box.h[0] * m.box.h[1] * m.box.h[2] > 40) && m.bmin && m.bmax;
const ownDoor = (entry, d) => (entry.m.found === d.root || label(entry.e) === d.root) && Math.hypot(entry.e.x - d.rp[0], entry.e.z - d.rp[2]) < 1.5;

// A base-map building is no layer row, so the audit has no box for it: LF_PlacementAudit.DoorLines writes one
// `root name=<root> rp=x,y,z rmin=x,y,z rmax=x,y,z` line per building with its world bounds.
// { "<root>@<x>,<z>": { min: [x, z], max: [x, z] } }
export function rootBounds(outText) {
  const out = {};
  for (const l of String(outText ?? "").split(/\r?\n/)) {
    if (!l.startsWith("root ")) continue;
    const kv = parseKv(l.slice(5));
    if (!kv.rmin || !kv.rmax || !kv.name || !kv.rp) continue;
    const n = (s) => s.split(",").map(Number);
    const [rmin, rmax, rp] = [n(kv.rmin), n(kv.rmax), n(kv.rp)];
    out[`${kv.name}@${r2(rp[0])},${r2(rp[2])}`] = { min: [rmin[0], rmin[2]], max: [rmax[0], rmax[2]] };
  }
  return out;
}

// The rooms: [{ name, buildingIdx, floorY, min, max, entrances: [door], at: [x, y, z] }] for each building with a ground-floor leaf: a
// measured layer building, else the door's root with its world bounds.
export function roomsOf(list, doors, bounds = {}) {
  const rooms = [];
  const claimed = new Set();
  const ground = (d) => d.box.c[1] - d.box.h[1] - d.rp[1] < 1.2; // the leaf stands within 1.2 m of its building origin: not a balcony door
  for (const entry of list.filter(isBuildingEntry)) {
    const mine = doors.filter((d) => ownDoor(entry, d) && ground(d));
    if (!mine.length) continue;
    for (const d of mine) claimed.add(d);
    rooms.push({ name: label(entry.e), buildingIdx: entry.idx, floorY: Math.min(...mine.map((d) => d.box.c[1] - d.box.h[1])), min: [entry.m.bmin[0], entry.m.bmin[2]], max: [entry.m.bmax[0], entry.m.bmax[2]], entrances: mine, at: [entry.e.x, entry.e.y, entry.e.z] });
  }
  const byRoot = new Map();
  for (const d of doors) if (!claimed.has(d) && ground(d)) byRoot.set(`${d.root}@${r2(d.rp[0])},${r2(d.rp[2])}`, [...(byRoot.get(`${d.root}@${r2(d.rp[0])},${r2(d.rp[2])}`) ?? []), d]);
  for (const [key, mine] of byRoot) {
    const b = bounds[key];
    if (!b) continue;
    rooms.push({ name: mine[0].root, buildingIdx: null, floorY: Math.min(...mine.map((d) => d.box.c[1] - d.box.h[1])), min: b.min, max: b.max, entrances: mine, at: [mine[0].rp[0], mine[0].rp[1], mine[0].rp[2]] });
  }
  return rooms;
}

// A shop is often up a step or two from its door (the general store's counter stands 1.1 m over the doorstep): the floor is where
// its own furniture stands, the lower quarter of the bottoms of the props and baked-in children in the footprint, not the leaf's foot.
export function refineFloor(room, list, kids = []) {
  const bottoms = [];
  const take = (m, id) => {
    if (m.missing || m.point || m.character || id === room.buildingIdx || 2 * m.box.h[1] < LANE_LIMITS.minHeight) return;
    if (!inRoom(room, m.box.c[0], m.box.c[2])) return;
    const b = m.box.c[1] - m.box.h[1];
    if (b >= room.floorY - 0.3 && b <= room.floorY + 1.6) bottoms.push(b);
  };
  for (const { idx, m, e } of list) if (!String(idx).startsWith("shelf:") && !["building", "vegetation", "surface", "chain", "vehicle", "mounted"].some((k) => isKind(e, k))) take(m, idx);
  for (const k of kids) if (ownKid(room, k)) take({ box: k.box }, null);
  if (bottoms.length < 3) return room.floorY;
  return bottoms.sort((a, b) => a - b)[Math.floor(bottoms.length * 0.25)];
}

const inRoom = (room, x, z) => x > room.min[0] && x < room.max[0] && z > room.min[1] && z < room.max[1];

// What stands in the way of a person in `room`: furniture on the floor (not a building, a plant, a wall run, a vehicle, a hung thing, a
// stepped-over mat, or something resting on another prop).
export function obstaclesOf(room, list, kids = []) {
  const out = [];
  const keep = (e, m, id) => {
    if (m.missing || m.point || m.character || id === room.buildingIdx) return;
    if (["building", "vegetation", "surface", "chain", "vehicle", "roadFurniture", "mounted"].some((k) => isKind(e, k))) return;
    const bottom = m.box.c[1] - m.box.h[1] - room.floorY;
    if (2 * m.box.h[1] < LANE_LIMITS.minHeight || bottom > LANE_LIMITS.maxBottom || bottom > LANE_LIMITS.maxTop) return;
    if (8 * m.box.h[0] * m.box.h[1] * m.box.h[2] > 40 || !inRoom(room, m.box.c[0], m.box.c[2])) return;
    const f = footprint(m.box);
    if (footprintArea(f) < 0.04) return;
    out.push({ e, f, id });
  };
  for (const { idx, e, m } of list) if (!String(idx).startsWith("shelf:")) keep(e, m, idx);
  for (const k of kids) {
    if (!ownKid(room, k)) continue;
    const e = { name: k.name, prefab: k.name };
    keep(e, { box: k.box, bmin: [0, 0, 0], bmax: [0, 0, 0] }, `kid:${k.name}`);
  }
  return out;
}

const ownKid = (room, k) => room.name === k.root;

const targetsOf = (room, list) => {
  const t = [];
  for (const { idx, e, m } of list) {
    if (m.missing || m.point || m.character || String(idx).startsWith("shelf:") || idx === room.buildingIdx) continue;
    if (!inRoom(room, m.box.c[0], m.box.c[2])) continue;
    const bottom = m.box.c[1] - m.box.h[1] - room.floorY;
    if (bottom > 2.2 || bottom < -0.6) continue;
    const kind = isTill(e) ? "till" : isShelf(e) ? "shelf" : e.actions?.length ? "action" : null;
    if (kind) t.push({ e, kind, f: footprint(m.box) });
  }
  return t;
};

// One finding row per room that has a problem: { name, layer, x, y, z, actions, problems, allowed }.
export function laneRows(list, doors, kids = [], { allow = [], bounds = {} } = {}) {
  const rows = [];
  for (const room of roomsOf(list, doors, bounds)) {
    room.floorY = refineFloor(room, list, kids);
    const obst = obstaclesOf(room, list, kids);
    const targets = targetsOf(room, list);
    const problems = [];
    const area = (room.max[0] - room.min[0]) * (room.max[1] - room.min[1]);
    const used = obst.reduce((s, o) => s + footprintArea(o.f), 0);
    const ratio = area > 0 ? used / area : 0;
    if (ratio > LANE_LIMITS.crowding) problems.push({ kind: "crowded", amount: ratio, text: `props cover ${Math.round(ratio * 100)}% of the floor (${r2(used)} of ${r2(area)} m2; limit ${Math.round(LANE_LIMITS.crowding * 100)}%): ${obst.length} props in ${room.name}` });
    const wide = laneMap(room, obst.map((o) => o.f), LANE_LIMITS.shopLane / 2);
    const narrow = laneMap(room, obst.map((o) => o.f), LANE_LIMITS.interactLane / 2);
    const fromDoor = (map) => room.entrances.map((d) => ({ d, seen: flood(map, ...entrancePoint(d, room)) }));
    const wideSeen = fromDoor(wide);
    const narrowSeen = fromDoor(narrow);
    if (targets.length && wideSeen.every((s) => !s.seen)) problems.push({ kind: "lane", amount: 1, text: `no 1.2 m lane starts at ${room.entrances.length > 1 ? "any entrance" : "the entrance"} of ${room.name}: something stands in the doorway` });
    for (const t of targets) {
      const name = label(t.e);
      if (t.kind === "till" || t.kind === "shelf") {
        if (wideSeen.every((s) => !s.seen)) continue;
        if (!wideSeen.some((s) => reaches(wide, s.seen, t.f, LANE_LIMITS.shopReach))) problems.push({ kind: "lane", amount: 1, text: `no 1.2 m lane from the door to ${t.kind === "till" ? "the till" : "the shelf"} ${name}` });
      }
      if (t.kind === "till" || t.kind === "action") {
        if (narrowSeen.every((s) => !s.seen)) continue;
        if (!narrowSeen.some((s) => reaches(narrow, s.seen, t.f, LANE_LIMITS.actionReach))) problems.push({ kind: "reach", amount: 1, text: `${name} cannot be reached from the door: no 0.9 m lane to a spot within ${LANE_LIMITS.actionReach} m of it` });
      }
    }
    const kept = problems.filter((p) => !allow.some((a) => (!a.kind || a.kind === p.kind) && new RegExp(a.match).test(`${room.name} ${p.text}`)));
    if (kept.length) rows.push({ name: `Room_${room.name}`, layer: LANE_LAYER, x: r2(room.at[0]), y: r2(room.at[1]), z: r2(room.at[2]), actions: [], problems: kept, allowed: [] });
  }
  return rows;
}

// Furniture baked into a building prefab (`child` lines) inside a door leaf's swing, the way doorProblems judges a layer entity:
// rows { name, layer, x, y, z, problems }. A leaf is never judged against itself, a low child (a mat) is stepped over.
export function childRows(kids, doors) {
  const rows = [];
  const zones = doors.map((d) => ({ d, swing: doorZone(d.box), ground: d.box.c[1] - d.box.h[1] < 1.2 })).filter((z) => z.swing && z.ground);
  for (const k of kids) {
    if (k.box.h[1] < 0.25 || /Door/i.test(k.name)) continue;
    for (const { d, swing } of zones) {
      if (k.root !== d.root) continue;
      const depth = obbPenetration(swing, k.box);
      if (depth > LIMITS.door) {
        rows.push({ name: k.name, layer: `(prefab child of ${k.root})`, x: r2(k.box.c[0]), y: r2(k.box.c[1]), z: r2(k.box.c[2]), actions: [], problems: [{ kind: "blocks-door", amount: 1, text: `baked into ${k.root}: in the swing of its door ${d.name} (${r2(depth)} m in)` }], allowed: [] });
        break;
      }
    }
  }
  return rows;
}

// Static action rules over the layer entities (idx -> problems): an action on a mesh-less entity too far from any mesh, another
// action too close to a till.
export function actionProblems(list) {
  const out = new Map();
  const add = (idx, p) => {
    if (!out.has(idx)) out.set(idx, []);
    out.get(idx).push(p);
  };
  const live = list.filter(({ m, idx }) => !m.missing && !String(idx).startsWith("shelf:"));
  const meshes = live.filter(({ m, e }) => !m.point && !m.character && !isKind(e, "building") && 8 * m.box.h[0] * m.box.h[1] * m.box.h[2] <= 40).map(({ m }) => obbAabb(m.box));
  const gap3 = (bb, p) => Math.hypot(...[0, 1, 2].map((i) => Math.max(bb.min[i] - p[i], 0, p[i] - bb.max[i])));
  const at = ({ e, m }) => (m.point || !m.box ? [e.x, e.y, e.z] : m.box.c);
  for (const entry of live) {
    const { idx, e, m } = entry;
    if (!e.actions?.length || !m.point) continue;
    const p = [e.x, e.y, e.z];
    let best = Infinity;
    for (const bb of meshes) best = Math.min(best, gap3(bb, p));
    if (best > LANE_LIMITS.offMesh) add(idx, { kind: "action-off-mesh", amount: Math.min(best, 2), text: `its action stands ${best === Infinity ? "far" : `${r2(best)} m`} from any mesh (limit ${LANE_LIMITS.offMesh} m): the interact key never offers it` });
  }
  const acting = live.filter(({ e }) => e.actions?.length);
  for (const till of acting.filter(({ e }) => isTill(e))) {
    const box = till.m.point ? { min: [till.e.x, till.e.y, till.e.z], max: [till.e.x, till.e.y, till.e.z] } : obbAabb(till.m.box);
    for (const other of acting) {
      if (other.idx === till.idx || isTill(other.e)) continue;
      const g = gap3(box, other.m.point || !other.m.box ? at(other) : obbAabb(other.m.box).min.map((v, i) => (v + obbAabb(other.m.box).max[i]) / 2));
      if (g <= LANE_LIMITS.tillClear) add(other.idx, { kind: "action-crowds-till", amount: 1, text: `its action is ${r2(g)} m from the till ${label(till.e)} (limit ${LANE_LIMITS.tillClear} m): the interact key picks it over the till` });
    }
  }
  return out;
}

// out.txt `child` lines (furniture baked into a building prefab), same fields as a `door` line: name root rp c h a0 a1 a2.
export function parseChildren(outText) {
  return String(outText ?? "").split(/\r?\n/).filter((l) => l.startsWith("child ")).map((l) => parseDoor(l.slice(6)));
}

// The harness `aim` reply after `stand <entity>` (LF_AimHarness: `aim selected=<Class>@<entity> context=<name>@<entity> offered=<Class>@<entity>:<1|0>,...`
// or `aim none`): does the F key act on `action` of `entity`? { selected, offered, none, stolen } where stolen names the action the key
// would take instead.
export function aimVerdict(detail, action, entity) {
  const text = String(detail ?? "");
  const esc = (s) => s.replace(/[.*+?^${}()|[\]\\]/g, "\\$&");
  const want = `${esc(action)}@${esc(entity)}`;
  const selected = new RegExp(`selected=${want}(?=\\s|$)`).test(text);
  const offered = new RegExp(`offered=[^ ]*${want}:1`).test(text);
  const took = /selected=(\S+)/.exec(text)?.[1] ?? null;
  return { selected, offered, none: /^aim none/.test(text), stolen: selected || !took ? null : took };
}

// Ranked rows for place-audit: the lane rows, the prefab-child rows, each scored.
export function laneFindings(list, doors, outText, opts = {}) {
  const kids = parseChildren(outText);
  return rank([...laneRows(list, doors, kids, { ...opts, bounds: rootBounds(outText) }), ...childRows(kids, doors)]);
}

// ---------------------------------------------------------------------------- selftest

import { existsSync, readFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const HERE = dirname(fileURLToPath(import.meta.url));

// agent-box/fixtures/place-lanes/ holds two rooms, as the audit would measure them (records.json, out.txt): `Shop_Crammed` (shelves
// across the shop with the till behind them, three overlapping locker banks, a chair in their fronts, a wardrobe through the east wall,
// a radio on the till) and `Shop_Clear` (two shelves on the back wall, a counter with a till, space to walk).
// red-proof: 2026-11-21, with laneRows returning [] the selftest printed "FAILED crammed room: no lane problem" and exited 1.
export function selftest(fail = (m) => {
  console.log(`place-lanes selftest: FAILED ${m}`);
  process.exitCode = 1;
}) {
  const dir = join(HERE, "fixtures", "place-lanes");
  if (!existsSync(join(dir, "records.json"))) return fail("agent-box/fixtures/place-lanes/records.json missing");
  const records = JSON.parse(readFileSync(join(dir, "records.json"), "utf8"));
  const outText = readFileSync(join(dir, "out.txt"), "utf8");
  const list = [];
  const doors = [];
  for (const line of outText.split(/\r?\n/)) {
    if (line.startsWith("door ")) doors.push(parseDoor(line.slice(5)));
  }
  for (const line of outText.split(/\r?\n/)) {
    const sp = line.indexOf(" ");
    const idx = Number(line.slice(0, sp));
    if (!line || !Number.isInteger(idx) || !records[idx]) continue;
    const m = rowOf(line.slice(sp + 1));
    list.push({ idx, e: records[idx], m });
  }
  const kids = parseChildren(outText);
  const rows = laneRows(list, doors, kids, { bounds: rootBounds(outText) });
  const room = (n) => rows.find((r) => r.name === `Room_${n}`);
  const kinds = (n) => (room(n)?.problems ?? []).map((p) => p.kind);
  const text = (n) => (room(n)?.problems ?? []).map((p) => p.text).join("; ");
  if (!kinds("Shop_Crammed").includes("lane")) fail(`crammed room: no lane problem: ${text("Shop_Crammed")}`);
  if (!kinds("Shop_Crammed").includes("crowded")) fail(`crammed room: not crowded: ${text("Shop_Crammed")}`);
  if (!kinds("Shop_Crammed").includes("reach")) fail(`crammed room: the till is reachable: ${text("Shop_Crammed")}`);
  if (!/till/.test(text("Shop_Crammed"))) fail(`crammed room: the till is not named: ${text("Shop_Crammed")}`);
  if (!kinds("Shop_Base").includes("lane")) fail(`base-map room (bounds from its door line): no lane problem: ${text("Shop_Base")}`);
  if (room("Shop_Clear")) fail(`clear room flagged: ${text("Shop_Clear")}`);
  const act = actionProblems(list);
  const byName = (n) => (act.get(list.find((x) => label(x.e) === n)?.idx) ?? []).map((p) => p.kind);
  if (!byName("Radio_Crammed").includes("action-crowds-till")) fail(`radio behind the till: ${byName("Radio_Crammed")}`);
  if (!byName("Pay_Crammed").includes("action-off-mesh")) fail(`a till action on a mesh-less entity 1.2 m from the register: ${byName("Pay_Crammed")}`);
  if (byName("Till_Clear").length || byName("Pay_Clear").length) fail(`the clear till's action flagged: ${byName("Till_Clear")} ${byName("Pay_Clear")}`);
  if (kids.length !== 2) fail(`child lines parsed: ${kids.length}`);
  const kr = childRows(kids, doors);
  if (kr.length !== 1 || kr[0].name !== "Furniture_Cabinet" || !kr[0].problems.some((p) => p.kind === "blocks-door")) fail(`prefab child in the door swing: ${JSON.stringify(kr.map((r) => r.name))}`);
  const noKids = laneRows(list, doors, [], { bounds: rootBounds(outText) });
  if (noKids.some((r) => r.name === "Room_Shop_Base" && !r.problems.length)) fail("rows carry problems");
  // aim after stand
  const aim = "aim selected=LF_StoreTillAction@Till_Clear context=x (Till_Clear) offered=LF_StoreTillAction@Till_Clear:1,LF_RadioAction@Radio_1:1";
  if (!aimVerdict(aim, "LF_StoreTillAction", "Till_Clear").selected) fail("aim: the till's action is the selected one");
  const stolen = aimVerdict("aim selected=LF_RadioAction@Radio_1 context=x (Radio_1) offered=LF_RadioAction@Radio_1:1,LF_StoreTillAction@Till_Clear:1", "LF_StoreTillAction", "Till_Clear");
  if (stolen.selected || !stolen.offered || stolen.stolen !== "LF_RadioAction@Radio_1") fail(`aim: the radio takes the key: ${JSON.stringify(stolen)}`);
  if (!aimVerdict("aim none", "LF_StoreTillAction", "Till_Clear").none) fail("aim none");
  return process.exitCode ?? 0;
}

// The audit's out.txt row format (place-audit parseRow) for the fixture; no import cycle at load time.
function rowOf(rest) {
  const kv = {};
  for (const part of rest.split(" ")) {
    const i = part.indexOf("=");
    if (i > 0) kv[part.slice(0, i)] = part.slice(i + 1);
  }
  if (kv.missing) return { missing: true };
  const tri = (s) => String(s ?? "").split(",").map(Number);
  return { gap: Number(kv.gap), point: kv.point === "1", character: kv.char === "1", found: kv.found, backed: kv.backed === "1", bmin: tri(kv.bmin), bmax: tri(kv.bmax), box: { c: tri(kv.c), h: tri(kv.h), a: [tri(kv.a0), tri(kv.a1), tri(kv.a2)] } };
}
