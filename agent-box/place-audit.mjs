#!/usr/bin/env node
// tools\cli audit placement: a ranked placement audit of every entity the DebugWorld layers place
// (docs/placement-audit.md). No game client and no session: it writes the entity list to the test
// profile, runs LF_Test_PlacementAuditTown alone on the dedicated test server (which loads the same
// DebugWorld the playtest mission does), and ranks what LF_PlacementAudit measured there.
//
//   audit placement [--top 40] [--area <name>] [--layer A.layer,B.layer] [--annex] [--from <out.txt>] [--out dir] [--strict]
//   audit selftest
//
// red-proof: 2026-11-14, the fixture layer's twin crate moved 1 m away printed "FAILED duplicate not named"; restored, OK.
// red-proof: 2026-11-14, LIMITS.sharedOverlap set to 0.08 printed "place-audit selftest: FAILED cups on one counter"; restored, OK.
// In-game (LF_PlacementAudit.c): floating / sunk (a trace down from the bounds base), corners hanging
// off a surface edge, clips through walls and other surfaces, tilt, scale, reach, oriented boxes, doors.
// Here: overlaps between oriented boxes, props in a door's swing, props on a carriageway, absurd scale,
// the largest side against tools/place/expected-sizes.json, what is exempt (a hung sign, a sack laid down, a shelf's contents), the allowlist, and the ranking.
import { spawnSync } from "node:child_process";
import { existsSync, mkdirSync, readFileSync, readdirSync, rmSync, writeFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { loadWorld } from "./layers.mjs";
import { BUDGET, cellBudget, worldFindings } from "./place-budget.mjs";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..");
const LAYERS_DIR = join(ROOT, "addons", "LifeFramework", "Worlds", "DebugWorld", "DebugWorld_Layers");
const ALLOW_FILE = join(ROOT, "agent-box", "place-audit-allow.json");
const AREAS_FILE = join(ROOT, "agent-box", "areas.json");
const SIZES_FILE = join(ROOT, "tools", "place", "expected-sizes.json");
// The test server's $profile: (test-e2e.ps1 recreates it on every boot and copies LF_PLACE_AUDIT_IN in).
export const PROFILE_DIR = join(ROOT, "server", "profile", "test", "profile", "place-audit");
export const TOWN = { x: 220, z: 260, r: 320 }; // door query circle: the whole town and its outskirts

// Thresholds (metres, degrees). The physical ones match LF_PlacementAudit (GAP, TILT, SCALE_MIN/MAX).
export const LIMITS = { gap: 0.15, tilt: 5, overlap: 0.08, sharedOverlap: 0.02, door: 0.1, doorClear: 1.0, doorWalk: 3, walk: 1.5, signWalk: 3, scaleMin: 0.5, scaleMax: 2, oddLow: 0.75, oddHigh: 1.6, roadMargin: 0.3, roadMaxWidth: 15 };

// Layers that hold no props: world settings, managers, factions, the roads themselves (checked as paths).
const SKIP_LAYERS = new Set(["default.layer", "Gamemode.layer", "Factions.layer", "Village_Roads.layer", "Roadmap.layer"]);

// Kinds, from the name and prefab path.
export const RX = {
  vegetation: /Vegetation\/|\/Tree|\bTree\b|Bush|Grass|Shrub/i,
  vehicle: /Prefabs\/Vehicles\/|_Car\b|_Van\b|Van$|Truck|UAZ|S105|S1203|Ural|LF_Vehicle/i,
  building: /Structures\/(Houses|Industrial|Commercial|Cultural|Military|Agriculture|Services|Civilian|Infrastructure\/(?!Lamps))|Building|House|Barn|Garage_|Shed|Office|Church|Station_|Hangar|Wall|Fence|Gate|Ramp/i,
  mounted: /Sign|Board|Poster|Notice|Plaque|(^|[^a-z])Map|Clock|WallLamp|Lamp_?Wall|Picture|Painting|Flag|Banner|Payphone_?Wall|Mirror/i,
  laidDown: /Bag|Sack|Log|Plank|Tire|Tyre|Wheel|Body|Corpse|Lying|Fallen|Rubble|Debris|Pipe|Lumber|Board_|Ladder|Stretcher|Tarp/i,
  holder: /Shelf|Shelves|Rack|Stand|Display|Pallet|Table|Desk|Counter|Workbench|Trailer|Cart|Locker|Cabinet|Crate|Box|Basket|Bucket|Barrel|TradeContainer|Container/i,
  roadFurniture: /Cone|Barrier|Bollard|Speed|Manhole|Drain|Puddle|Decal|Crossing|Marking|Spawn|Trigger|Zone|Area|Point/i,
  // Ground plates and road surfaces: things stand on them, and their edges sit in the ground by design.
  surface: /(^|[^a-z])(Pave|Slab|Concrete|Asphalt|Decal|Tarmac)|RoadEntity|RoadGenerator|SplineShape/i,
  // Rock outcrops overlap each other, wall and fence runs overlap at their joints: that is how they are built.
  chain: /Cliff|Rock|Stone|Boulder|Wall|Fence|Hedge/i,
};
const kindText = (e) => `${e.name || ""} ${e.prefab || ""} ${e.found || ""}`;
export const isKind = (e, k) => RX[k].test(kindText(e));

// --------------------------------------------------------------------- pure geometry

const dot = (a, b) => a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
const sub = (a, b) => [a[0] - b[0], a[1] - b[1], a[2] - b[2]];
const cross = (a, b) => [a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]];
const vlen = (a) => Math.hypot(a[0], a[1], a[2]);
const r2 = (n) => Math.round(n * 100) / 100;

// Oriented boxes { c, h: [hx, hy, hz], a: [ax0, ax1, ax2] } (unit axes). Penetration depth: the smallest
// overlap over the 15 separating axes, or a negative number (the gap) when they are apart.
export function obbPenetration(A, B) {
  const axes = [...A.a, ...B.a];
  for (const p of A.a) for (const q of B.a) {
    const c = cross(p, q);
    const l = vlen(c);
    if (l > 1e-4) axes.push([c[0] / l, c[1] / l, c[2] / l]);
  }
  const d = sub(B.c, A.c);
  let best = Infinity;
  for (const L of axes) {
    const ra = A.h[0] * Math.abs(dot(A.a[0], L)) + A.h[1] * Math.abs(dot(A.a[1], L)) + A.h[2] * Math.abs(dot(A.a[2], L));
    const rb = B.h[0] * Math.abs(dot(B.a[0], L)) + B.h[1] * Math.abs(dot(B.a[1], L)) + B.h[2] * Math.abs(dot(B.a[2], L));
    const o = ra + rb - Math.abs(dot(d, L));
    if (o < best) best = o;
    if (best < 0) return best;
  }
  return best;
}

// World AABB of an oriented box.
export function obbAabb(B) {
  const ext = [0, 1, 2].map((i) => B.h[0] * Math.abs(B.a[0][i]) + B.h[1] * Math.abs(B.a[1][i]) + B.h[2] * Math.abs(B.a[2][i]));
  return { min: B.c.map((v, i) => v - ext[i]), max: B.c.map((v, i) => v + ext[i]) };
}

// Fraction of box S's AABB volume inside box H's AABB (a shelf's contents sit inside the shelf's box).
export function insideFraction(S, H) {
  const s = obbAabb(S);
  const h = obbAabb(H);
  let inter = 1;
  let vol = 1;
  for (let i = 0; i < 3; i++) {
    inter *= Math.max(0, Math.min(s.max[i], h.max[i]) - Math.max(s.min[i], h.min[i]));
    vol *= Math.max(1e-6, s.max[i] - s.min[i]);
  }
  return inter / vol;
}

// A door's clearance: the leaf's box with its thin horizontal side pushed out doorClear metres both ways.
export function doorZone(D, clear = LIMITS.doorClear) {
  const horiz = [0, 1, 2].filter((i) => Math.abs(D.a[i][1]) < 0.7);
  const up = [0, 1, 2].find((i) => !horiz.includes(i)) ?? 1;
  if (horiz.length < 2) return null;
  const thin = D.h[horiz[0]] <= D.h[horiz[1]] ? horiz[0] : horiz[1];
  const h = [...D.h];
  h[thin] = Math.max(h[thin], clear);
  h[up] = Math.min(h[up], 1.1);
  return { c: D.c, h, a: D.a };
}

// Distance in the ground plane from (x, z) to a polyline.
export function polylineDistance(x, z, pts) {
  let best = Infinity;
  for (let i = 0; i + 1 < pts.length; i++) {
    const [ax, az] = pts[i];
    const [bx, bz] = pts[i + 1];
    const dx = bx - ax;
    const dz = bz - az;
    const l2 = dx * dx + dz * dz || 1e-9;
    const t = Math.max(0, Math.min(1, ((x - ax) * dx + (z - az) * dz) / l2));
    best = Math.min(best, Math.hypot(x - (ax + t * dx), z - (az + t * dz)));
  }
  return best;
}

// Roads from Village_Roads.layer: each top-level spline's points and its road width.
export function parseRoads(text) {
  const roads = [];
  let cur = null;
  let depth = 0;
  let inPoints = false;
  let pointsDone = false;
  for (const raw of text.split(/\r?\n/)) {
    const line = raw.trim();
    const top = /^SplineShapeEntity\s+(\S+)\s*\{$/.exec(line);
    if (top && depth === 0) {
      cur = { name: top[1], origin: [0, 0, 0], pts: [], width: 0 };
      roads.push(cur);
      inPoints = false;
      pointsDone = false;
    }
    if (cur && depth === 1) {
      const c = /^coords\s+(\S+)\s+(\S+)\s+(\S+)$/.exec(line);
      if (c) cur.origin = [Number(c[1]), Number(c[2]), Number(c[3])];
      if (line === "Points {" && !pointsDone) inPoints = true;
    }
    if (cur && inPoints) {
      const p = /^Position\s+(\S+)\s+(\S+)\s+(\S+)$/.exec(line);
      if (p) cur.pts.push([cur.origin[0] + Number(p[1]), cur.origin[2] + Number(p[3])]);
    }
    const w = /^Width\s+(\S+)$/.exec(line);
    if (cur && w && !cur.width) cur.width = Number(w[1]);
    depth += (line.match(/\{/g) || []).length - (line.match(/\}/g) || []).length;
    if (inPoints && depth === 1) {
      inPoints = false;
      pointsDone = true;
    }
    if (depth === 0) cur = cur && line === "}" ? null : cur;
  }
  return roads.filter((r) => r.pts.length >= 2 && r.width > 0);
}

// ---------------------------------------------------------------------- measurements

const triple = (s) => String(s ?? "").split(",").map(Number);

export function parseKv(text) {
  const out = {};
  for (const part of String(text ?? "").split(" ")) {
    const i = part.indexOf("=");
    if (i > 0) out[part.slice(0, i)] = part.slice(i + 1);
  }
  return out;
}

// One out.txt row -> numbers. "<index> missing=1" -> { missing: true }.
export function parseRow(rest) {
  const kv = parseKv(rest);
  if (kv.missing) return { missing: true };
  return {
    gap: Number(kv.gap),
    pgap: Number(kv.pgap),
    tilt: Number(kv.tilt),
    walk: Number(kv.walk),
    scale: Number(kv.scale),
    on: kv.on,
    over: Number(kv.over),
    drop: Number(kv.drop),
    backed: kv.backed === "1",
    point: kv.point === "1",
    character: kv.char === "1",
    clip: kv.clip && kv.clip !== "none" ? kv.clip.split(";") : [],
    flags: kv.flags && kv.flags !== "none" ? kv.flags.split(",") : [],
    found: kv.found,
    bmin: triple(kv.bmin),
    bmax: triple(kv.bmax),
    box: { c: triple(kv.c), h: triple(kv.h), a: [triple(kv.a0), triple(kv.a1), triple(kv.a2)] },
  };
}

export function parseDoor(rest) {
  const kv = parseKv(rest);
  return { name: kv.name, root: kv.root, rp: triple(kv.rp), box: { c: triple(kv.c), h: triple(kv.h), a: [triple(kv.a0), triple(kv.a1), triple(kv.a2)] } };
}

const volume = (m) => 8 * m.box.h[0] * m.box.h[1] * m.box.h[2];
const label = (e) => e.name || (e.prefab ? e.prefab.split("/").pop().replace(/\.et$/, "") : "unnamed");

// ----------------------------------------------------------------------------- checks

// Severity points per problem kind; the rank sums them and weights interactables and named areas.
export const WEIGHT = { floating: 10, sunk: 7, overhang: 6, clips: 6, overlaps: 5, "blocks-door": 7, "door-path": 4, "in-road": 5, scaled: 6, "odd-scale": 2, size: 6, tilted: 2, unreachable: 6 };

// Expected sizes (tools/place/expected-sizes.json): the first entry whose regex matches the prefab file name
// (or, for a bare mesh entity, the mesh file name) gives the sane range for the largest side, in metres, in
// world space (the measured box has scale applied). An entity no entry matches is not checked.
let sizesCache = null;
export function loadSizes(file = SIZES_FILE) {
  if (file === SIZES_FILE && sizesCache) return sizesCache;
  let list = [];
  try {
    list = (JSON.parse(readFileSync(file, "utf8")).sizes ?? []).map((s) => ({ ...s, rx: new RegExp(s.match, "i") }));
  } catch (e) {
    console.log(`audit placement: no size table (${e.message})`);
  }
  if (file === SIZES_FILE) sizesCache = list;
  return list;
}

export function sizeEntry(sizes, e) {
  const text = (e.prefab || e.mesh || "").split("/").pop().replace(/.(et|xob)$/, "");
  return sizes.find((s) => s.rx.test(text)) ?? null;
}

// One problem when the largest side is outside the entry's range; null when covered and sane or not covered.
export function sizeProblem(e, m, sizes) {
  const s = sizeEntry(sizes, e);
  if (!s) return { covered: false, problem: null };
  const side = 2 * Math.max(...m.box.h);
  if (side >= s.min && side <= s.max) return { covered: true, problem: null };
  const amount = side > s.max ? side / s.max - 1 : s.min / side - 1;
  return { covered: true, problem: { kind: "size", amount, text: `size ${r2(side)} m outside ${s.min}-${s.max} m for ${s.id} (${s.why})` } };
}

// Problems for one measured entity, before pairs (overlaps, doors, roads) are added.
export function singleProblems(e, m) {
  const out = [];
  if (m.missing || m.point) return out;
  const veg = isKind(e, "vegetation");
  // Hung on something: a sign, board or poster with a wall (or another entity) at its back, or any thin thing
  // more than 0.5 m up against one. A pole sign or a board on legs has nothing behind it and must stand.
  const mounted = m.backed && (isKind(e, "mounted") || (m.gap > 0.5 && Math.min(m.box.h[0], m.box.h[2]) < 0.15));
  const building = isKind(e, "building") || volume(m) > 40;
  const vehicle = isKind(e, "vehicle");
  if (!mounted && m.gap > LIMITS.gap) out.push({ kind: "floating", amount: m.gap, text: `floating ${r2(m.gap)} m above ${m.on}` });
  const sunk = -Math.max(m.gap, m.pgap); // both the bounds bottom and the pivot under the floor (LF_PlacementAudit.Flags)
  if (!mounted && !vehicle && sunk > (building ? 0.5 : LIMITS.gap)) out.push({ kind: "sunk", amount: sunk, text: `sunk ${r2(sunk)} m into ${m.on}` });
  if (!mounted && m.over > 0 && m.gap <= LIMITS.gap && m.on !== "terrain") out.push({ kind: "overhang", amount: m.over / 4, text: `${m.over}/4 corners hang off the edge of ${m.on} (drop ${r2(m.drop)} m)` });
  if (m.tilt > LIMITS.tilt && !isKind(e, "laidDown") && !veg) out.push({ kind: "tilted", amount: m.tilt / 45, text: `tilted ${Math.round(m.tilt)} deg` });
  if (m.scale < LIMITS.scaleMin || m.scale > LIMITS.scaleMax) out.push({ kind: "scaled", amount: 1, text: `scale ${r2(m.scale)}` });
  else if (!/^Sign_/.test(e.name || "") && (m.scale < LIMITS.oddLow || m.scale > LIMITS.oddHigh)) out.push({ kind: "odd-scale", amount: 1, text: `scale ${r2(m.scale)}` });
  // A building's walls meet roads, fences and its neighbours by design; a prop in a building is reported from the prop.
  const clips = building || isKind(e, "surface") ? [] : m.clip.filter((c) => !(mounted && RX.building.test(c)) && !RX.surface.test(c) && c !== "unnamed" && !(vehicle && c === "GenericEntity") && !(isKind(e, "chain") && RX.chain.test(c)));
  if (clips.length && !veg) out.push({ kind: "clips", amount: clips.length, text: `clips ${clips.join(", ")}` });
  if (e.actions?.length && m.walk >= 0) {
    const lim = /^Sign_/.test(e.name || "") ? LIMITS.signWalk : LIMITS.walk;
    if (m.walk > lim) out.push({ kind: "unreachable", amount: 1, text: m.walk >= 99 ? "unreachable (no standing spot within 3 m of its edge)" : `reach ${r2(m.walk)} m from its edge to standing room` });
  }
  return out;
}

// Both rest (within the float tolerance) on the same named prop, not the ground.
function sharedSurface(a, b) {
  return !!a.on && a.on === b.on && a.on !== "terrain" && a.on !== "none" && Math.abs(a.gap) <= LIMITS.gap && Math.abs(b.gap) <= LIMITS.gap;
}

// Pairs: oriented boxes that interpenetrate beyond LIMITS.overlap. Skips vegetation (canopies have no
// collision), buildings (the ray clip check sees their walls), a layer parent and its child, and an item
// that sits inside an open holder (a shelf, a stand, a table group).
export function overlapProblems(list) {
  const cand = list.filter(({ e, m }) => !m.missing && !m.point && !isKind(e, "vegetation") && !isKind(e, "building") && !isKind(e, "surface") && volume(m) <= 40 && volume(m) > 1e-4);
  const out = new Map();
  for (let i = 0; i < cand.length; i++) {
    for (let j = i + 1; j < cand.length; j++) {
      const A = cand[i];
      const B = cand[j];
      if (Math.abs(A.m.box.c[0] - B.m.box.c[0]) > 12 || Math.abs(A.m.box.c[2] - B.m.box.c[2]) > 12) continue;
      if (A.e.parent && (A.e.parent === B.e.name || A.e.parent === B.e.parent)) continue;
      if (B.e.parent && B.e.parent === A.e.name) continue;
      if (isKind(A.e, "chain") && isKind(B.e, "chain")) continue;
      const depth = obbPenetration(A.m.box, B.m.box);
      // Two props standing on the same table or counter are neighbours, not one inside the other: any
      // push-through counts (a till and a bottle on one counter overlapped by 0.05 m and passed the 0.08 m limit).
      const sameSurface = sharedSurface(A.m, B.m);
      if (depth <= (sameSurface ? LIMITS.sharedOverlap : LIMITS.overlap)) continue;
      const [small, big] = volume(A.m) <= volume(B.m) ? [A, B] : [B, A];
      // Contents: something much smaller sitting inside a shelf, a stand, a crate (apples in the press crate).
      if (!sameSurface && isKind(big.e, "holder") && volume(small.m) < volume(big.m) * 0.5 && insideFraction(small.m.box, big.m.box) > 0.6) continue;
      // Resting on it: the surface found under the small one is the big one (a jerrycan on a workbench whose
      // back panel rises above the top), or the small one's bottom is at the big one's top.
      if (small.m.gap <= LIMITS.gap && small.m.gap >= -LIMITS.gap && (small.m.on === big.m.found || small.m.on === label(big.e))) continue;
      // Resting on top: the small one's bottom is at the big one's top.
      const sb = obbAabb(small.m.box);
      const bb = obbAabb(big.m.box);
      if (sb.min[1] >= bb.max[1] - LIMITS.overlap * 1.5) continue;
      for (const [x, y] of [[A, B], [B, A]]) {
        const key = x.idx;
        if (!out.has(key)) out.set(key, []);
        out.get(key).push({ kind: "overlaps", amount: Math.min(depth, 1), text: `overlaps ${label(y.e)} by ${r2(depth)} m` });
      }
    }
  }
  return out;
}

// Doors: anything (but the door's own building, and a hung sign) inside a door leaf's clearance zone.
export function doorProblems(list, doors) {
  const out = new Map();
  // Ground-level leaves only for the walk-up (a first-floor balcony door has no path to it).
  const zones = doors.map((d) => ({ d, swing: doorZone(d.box), walk: doorZone(d.box, LIMITS.doorWalk), ground: d.box.c[1] - d.box.h[1] < 1.2 })).filter((x) => x.swing);
  // The building each door belongs to (its layer entity), so furniture inside it is not on the street path.
  const buildingOf = (d) => list.find(({ e, m }) => !m.missing && (m.found === d.root || label(e) === d.root) && Math.hypot(e.x - d.rp[0], e.z - d.rp[2]) < 1.5);
  const inside = (b, p) => b && p[0] > b.m.bmin[0] && p[0] < b.m.bmax[0] && p[2] > b.m.bmin[2] && p[2] < b.m.bmax[2];
  for (const { idx, e, m } of list) {
    // Walls and fences frame a door; a sign hung above it is above the zone.
    if (m.missing || m.point || isKind(e, "chain")) continue;
    // A whole building next door is a wall, not a prop in the way; a low thing (a kerb, a mat) is stepped over.
    if (volume(m) > 40 || m.box.h[1] < 0.25) continue;
    let hit = null;
    for (const { d, swing, walk, ground } of zones) {
      if (Math.abs(swing.c[0] - m.box.c[0]) > 15 || Math.abs(swing.c[2] - m.box.c[2]) > 15) continue;
      const own = (m.found === d.root || label(e) === d.root) && Math.hypot(e.x - d.rp[0], e.z - d.rp[2]) < 1.5;
      if (own) continue;
      const depth = obbPenetration(swing, m.box);
      if (depth > LIMITS.door) {
        hit = { kind: "blocks-door", amount: 1, text: `in the swing of a door of ${d.root} (${r2(depth)} m in)` };
        break;
      }
      if (!hit && ground && obbPenetration(walk, m.box) > LIMITS.door && !inside(buildingOf(d), m.box.c))
        hit = { kind: "door-path", amount: 0.5, text: `stands in the walk-up to a door of ${d.root} (within ${LIMITS.doorWalk} m in front of it)` };
    }
    if (hit) out.set(idx, [hit]);
  }
  return out;
}

// Carriageways (roads narrower than LIMITS.roadMaxWidth; the square is a plaza): a prop whose centre
// stands on the driving surface. Vehicles, markings and road furniture are exempt.
export function roadProblems(list, roads) {
  const out = new Map();
  // The market street, the square and the fuel apron are walked, not driven: stalls and people stand there.
  const narrow = roads.filter((r) => r.width < LIMITS.roadMaxWidth && !/Market|Square|Apron|Plaza|Yard/i.test(r.name));
  for (const { idx, e, m } of list) {
    if (m.missing || m.point || isKind(e, "vehicle") || isKind(e, "roadFurniture") || isKind(e, "building")) continue;
    if (m.box.h[1] < 0.05) continue;
    for (const r of narrow) {
      const d = polylineDistance(m.box.c[0], m.box.c[2], r.pts);
      if (d < r.width / 2 - LIMITS.roadMargin) {
        if (!out.has(idx)) out.set(idx, []);
        out.get(idx).push({ kind: "in-road", amount: 1, text: `stands on ${r.name} (${r2(d)} m from its centre line, road ${r.width} m wide)` });
        break;
      }
    }
  }
  return out;
}

// Allowlist: [{ "match": "<regex on name or prefab>", "kind": "<problem kind>", "why": "..." }].
export function allowed(allow, e, p) {
  return allow.some((a) => (!a.kind || a.kind === p.kind) && new RegExp(a.match).test(`${e.name || ""} ${e.prefab || ""}`) && (!a.layer || a.layer === e.layer));
}

export function areaOf(areas, e) {
  let best = null;
  for (const a of areas) {
    const d = Math.hypot(e.x - a.x, e.z - a.z);
    if (d <= a.radius && (!best || d < best.d)) best = { name: a.name, d };
  }
  return best ? best.name : "";
}

// Rank: the sum of weight x (1 + amount) per problem, x1.5 for an interactable, x1.3 inside a named area.
export function rank(rows) {
  for (const r of rows) {
    let s = 0;
    for (const p of r.problems) s += (WEIGHT[p.kind] ?? 1) * (1 + Math.min(p.amount ?? 0, 2));
    if (r.actions?.length) s *= 1.5;
    if (r.area) s *= 1.3;
    r.score = Math.round(s * 10) / 10;
  }
  return rows.filter((r) => r.problems.length).sort((a, b) => b.score - a.score || a.layer.localeCompare(b.layer));
}

export function analyse(records, outText, { allow = [], areas = [], roads = [], sizes = loadSizes() } = {}) {
  const list = [];
  const doors = [];
  for (const raw of outText.split(/\r?\n/)) {
    const line = raw.trim();
    if (!line) continue;
    if (line.startsWith("door ")) {
      doors.push(parseDoor(line.slice(5)));
      continue;
    }
    const sp = line.indexOf(" ");
    if (line.startsWith("shelf ")) {
      // A display copy a store spawned at load (no layer entry): measured like a placed prop.
      const sp2 = line.indexOf(" ", 6);
      const tag = line.slice(6, sp2);
      const m = parseRow(line.slice(sp2 + 1));
      if (m.missing) continue;
      list.push({ idx: `shelf:${tag}`, e: { name: `Stock_${tag.replace(/[/@]/g, "_")}`, prefab: tag.split("/")[1].split("@")[0], layer: "(store stock)", x: m.box.c[0], y: m.box.c[1], z: m.box.c[2], actions: [], found: m.found }, m });
      continue;
    }
    const idx = Number(line.slice(0, sp));
    const e = records[idx];
    if (!e) continue;
    const m = parseRow(line.slice(sp + 1));
    if (m.found) e.found = m.found;
    list.push({ idx, e, m });
  }
  const pairs = [overlapProblems(list), doorProblems(list, doors), roadProblems(list, roads)];
  const rows = [];
  const missing = [];
  let covered = 0;
  for (const { idx, e, m } of list) {
    if (m.missing) {
      missing.push(e);
      continue;
    }
    let problems = singleProblems(e, m);
    if (!m.point) {
      const sz = sizeProblem(e, m, sizes);
      if (sz.covered) covered++;
      if (sz.problem) problems.push(sz.problem);
    }
    for (const p of pairs) problems.push(...(p.get(idx) ?? []));
    const suppressed = problems.filter((p) => allowed(allow, e, p));
    problems = problems.filter((p) => !allowed(allow, e, p));
    rows.push({ name: label(e), layer: e.layer, x: e.x, y: e.y, z: e.z, actions: e.actions, area: areaOf(areas, e), problems, allowed: suppressed.map((p) => p.kind) });
  }
  return { ranked: rank(rows), measured: list.length, sizeCovered: covered, missing, doors: doors.length };
}

// ----------------------------------------------------------------------------- inputs

// The debug annex (south-west of the town: the old per-feature test stations) is not part of the playtest town.
export const inAnnex = (e) => e.x < 110 && e.z < 120;

export function auditRecords({ layers, area, areas, annex } = {}) {
  let recs = loadWorld(ROOT).filter((e) => !SKIP_LAYERS.has(e.layer) && (annex || !inAnnex(e)));
  if (layers) recs = recs.filter((e) => layers.includes(e.layer));
  if (area) {
    const a = areas.find((x) => x.name === area);
    if (!a) throw new Error(`unknown area ${area}`);
    recs = recs.filter((e) => Math.hypot(e.x - a.x, e.z - a.z) <= a.radius);
  }
  return recs;
}

const tokenOf = (s) => (s && /^[A-Za-z0-9_]+$/.test(s) ? s : "-");

export function inputText(records, extra = []) {
  const lines = [`town ${TOWN.x} ${TOWN.z} ${TOWN.r}`, ...extra];
  records.forEach((e, i) => {
    const base = e.prefab ? e.prefab.split("/").pop().replace(/\.et$/, "") : "";
    lines.push(`${i} ${tokenOf(e.name)} ${tokenOf(base)} ${e.x} ${e.y} ${e.z} ${e.actions?.length ? 1 : 0}`);
  });
  return lines.join("\n") + "\n";
}

// One boot of the dedicated test server: LF_Test_PlacementAuditTown measures `records` and traces the
// `extra` input lines (the placement tool's probe rays). Returns the raw out.txt (null when the server
// wrote none) and whether the test failed. --local: the list goes in and the result comes out through
// this checkout's test profile.
export function measureTown(records, { outDir = join(ROOT, "tmp", "place-audit"), extra = [], log = console.log } = {}) {
  mkdirSync(outDir, { recursive: true });
  const inFile = join(outDir, "in.txt");
  const outFile = join(PROFILE_DIR, "out.txt");
  rmSync(outFile, { force: true });
  writeFileSync(inFile, inputText(records, extra));
  writeFileSync(join(outDir, "records.json"), JSON.stringify(records));
  const r = spawnSync(process.execPath, [join(ROOT, "tools", "cli.mjs"), "test", "--no-build", "--only", "LF_Test_PlacementAuditTown", "--local", "--verbose"], { cwd: ROOT, encoding: "utf8", windowsHide: true, timeout: 600000, env: { ...process.env, LF_PLACE_AUDIT_IN: inFile } });
  const text = `${r.stdout ?? ""}${r.stderr ?? ""}`;
  const keep = text.split(/\r?\n/).filter((l) => /SUMMARY|^pool:|FAILED|TIMEOUT|error/i.test(l));
  for (const l of keep.slice(-4)) log(`  ${l.trim()}`);
  if (!existsSync(outFile)) {
    log(`audit placement: the test server wrote no ${outFile} (exit ${r.status}); see the lines above`);
    return { outText: null, failed: true };
  }
  const outText = readFileSync(outFile, "utf8");
  writeFileSync(join(outDir, "out.txt"), outText);
  return { outText, failed: r.status !== 0 };
}

export function readJson(file, fallback) {
  try {
    return JSON.parse(readFileSync(file, "utf8"));
  } catch {
    return fallback;
  }
}

function parseArgs(argv) {
  const out = { _: [], flags: {} };
  const valued = new Set(["top", "area", "layer", "from", "out"]);
  for (let i = 0; i < argv.length; i++) {
    const a = argv[i];
    if (a.startsWith("--")) {
      const k = a.slice(2);
      out.flags[k] = valued.has(k) ? argv[++i] : true;
    } else out._.push(a);
  }
  return out;
}

// --------------------------------------------------------------------------- commands

async function placement(argv) {
  const { flags } = parseArgs(argv);
  const areas = readJson(AREAS_FILE, { areas: [] }).areas;
  const allow = readJson(ALLOW_FILE, { allow: [] }).allow ?? [];
  const roads = parseRoads(readFileSync(join(LAYERS_DIR, "Village_Roads.layer"), "utf8"));
  let records = auditRecords({ layers: flags.layer ? String(flags.layer).split(",") : null, area: flags.area, areas, annex: !!flags.annex });
  const outDir = resolve(flags.out ?? join(ROOT, "tmp", "place-audit"));
  mkdirSync(outDir, { recursive: true });
  const t0 = Date.now();
  let outText;
  let testFailed = false;
  if (flags.from) {
    outText = readFileSync(resolve(flags.from), "utf8");
    // The rows are indexed by the list that run measured, which a later layer edit can shift.
    const saved = join(dirname(resolve(flags.from)), "records.json");
    if (existsSync(saved)) records = JSON.parse(readFileSync(saved, "utf8"));
    else console.log("audit placement: no records.json beside " + flags.from + "; matching rows to the current layers");
  } else {
    const run = measureTown(records, { outDir });
    if (run.outText == null) return 1;
    outText = run.outText;
    testFailed = run.failed;
    if (testFailed) console.log("audit placement: LF_Test_PlacementAuditTown failed (lines above); ranking what it measured");
  }
  const res = analyse(records, outText, { allow, areas, roads });
  // Static world checks on the layer entries: duplicates and the per-cell entity budget.
  const world = worldFindings(records, { allow });
  const cells = cellBudget(records);
  const top = Number(flags.top ?? 40);
  const fmt = (r, i) => `${String(i + 1).padStart(3)}. ${String(r.score).padStart(5)}  ${r.name} [${r.layer}${r.area ? `, ${r.area}` : ""}] @${r2(r.x)},${r2(r.y)},${r2(r.z)}: ${r.problems.map((p) => p.text).join("; ")}`;
  const byKind = {};
  for (const r of res.ranked) for (const p of r.problems) byKind[p.kind] = (byKind[p.kind] ?? 0) + 1;
  const lines = [
    `placement audit: ${res.ranked.length} of ${res.measured} entities flagged (${res.missing.length} not found in the world, ${res.doors} door leaves checked, sizes: ${res.sizeCovered} covered by expected-sizes.json), ${((Date.now() - t0) / 1000).toFixed(0)} s`,
    `by kind: ${Object.entries(byKind).sort((a, b) => b[1] - a[1]).map(([k, n]) => `${k} ${n}`).join(", ") || "none"}`,
    `world: ${cells.length} cells of ${BUDGET.cell} m, busiest ${cells[0] ? `${cells[0].cell} with ${cells[0].count} of ${BUDGET.limit}` : "none"}; ${world.length} world finding(s)`,
    ...world.map((w) => `  ${w.text}`),
    ...res.ranked.map(fmt),
  ];
  if (res.missing.length) lines.push("", "not found at their layer position:", ...res.missing.map((e) => `  ${label(e)} [${e.layer}] @${e.x},${e.y},${e.z}`));
  writeFileSync(join(outDir, "report.txt"), lines.join("\n") + "\n");
  writeFileSync(join(outDir, "report.json"), JSON.stringify({ at: new Date().toISOString(), limits: LIMITS, budget: BUDGET, world, cells, ...res, missing: res.missing.map((e) => ({ name: label(e), layer: e.layer, x: e.x, y: e.y, z: e.z })) }, null, 2));
  console.log(lines.slice(0, 3 + world.length + top).join("\n"));
  if (res.ranked.length > top) console.log(`  ... ${res.ranked.length - top} more`);
  console.log(`report: ${join(outDir, "report.txt")} (json beside it)`);
  return testFailed || (flags.strict && (res.ranked.length || world.length)) ? 1 : 0;
}

// The fixture layer (agent-box/fixtures/place-budget/Fixture.layer): a crate and its twin 1 cm apart, a barrel
// 1.4 m up in the air and five cups in a row. With a cell limit of 5 the cups' cell is over-full; each finding
// must name its prop or cell. The floating barrel goes through the measured-row path (analyse), the rest is static.
function worldSelftest(fail) {
  const recs = loadWorld(ROOT, join(ROOT, "agent-box", "fixtures", "place-budget"));
  if (recs.length !== 8) fail(`fixture layer entities: ${recs.length}`);
  const found = worldFindings(recs, { limit: 5 });
  const dup = found.find((f) => f.kind === "duplicate");
  if (!dup || !dup.text.includes("FixtureCrate_Twin") || !dup.text.includes("FixtureCrate_A")) fail(`duplicate not named: ${JSON.stringify(found)}`);
  if (found.filter((f) => f.kind === "duplicate").length !== 1) fail("the cups 0.5 m apart are not duplicates");
  const over = found.find((f) => f.kind === "budget");
  if (!over || !over.text.includes("cell 0,0") || !over.text.includes("8 entities") || !over.text.includes("Cup_01 x5")) fail(`over-full cell not named: ${JSON.stringify(found)}`);
  if (worldFindings(recs, { limit: 50 }).some((f) => f.kind === "budget")) fail("a cell under the limit is flagged");
  if (worldFindings(recs, { limit: 5, allow: [{ match: "^FixtureCrate_Twin", kind: "duplicate", why: "t" }, { match: "cell:0,0", kind: "budget", why: "t" }] }).length) fail("the allowlist suppresses world findings");
  const row = "bmin=19.7,1.1,9.7 bmax=20.3,1.7,10.3 gap=1.4 tilt=0 walk=-1 pgap=1.4 clip=none scale=1 on=terrain over=0 drop=0 backed=0 c=20,1.4,10 h=0.3,0.3,0.3 a0=1,0,0 a1=0,1,0 a2=0,0,1 flags=none found=Barrel_01";
  const floater = recs.findIndex((e) => e.name === "FixtureFloater");
  const res = analyse(recs, `${floater} ${row}`, {});
  if (!res.ranked.some((r) => r.name === "FixtureFloater" && r.problems.some((p) => p.kind === "floating"))) fail("the fixture's floating barrel is flagged floating");
  // The measured bounds list covers every vanilla prefab the layers use, or lists it as unmeasured.
  const list = readJson(join(ROOT, "tools", "place", "prefab-bounds.json"), null);
  if (!list?.prefabs) fail("tools/place/prefab-bounds.json missing; run tools\cli place measure");
  else {
    const addon = join(ROOT, "addons", "LifeFramework");
    const unlisted = new Set();
    for (const f of readdirSync(LAYERS_DIR).filter((n) => n.endsWith(".layer"))) {
      for (const m of readFileSync(join(LAYERS_DIR, f), "utf8").matchAll(/"{[0-9A-Fa-f]{16}}([^"]+.et)"/g)) {
        if (!existsSync(join(addon, m[1])) && !list.prefabs[m[1]] && !(list.unmeasured ?? []).includes(m[1])) unlisted.add(m[1]);
      }
    }
    if (unlisted.size) fail(`vanilla prefabs missing from prefab-bounds.json (run tools\cli place measure): ${[...unlisted].slice(0, 3)}`);
  }
}

function selftest() {
  const fail = (m) => {
    console.log(`place-audit selftest: FAILED ${m}`);
    process.exitCode = 1;
  };
  const I = [[1, 0, 0], [0, 1, 0], [0, 0, 1]];
  const box = (c, h, a = I) => ({ c, h, a });
  if (!(obbPenetration(box([0, 0, 0], [1, 1, 1]), box([1.5, 0, 0], [1, 1, 1])) > 0.4)) fail("overlapping boxes");
  if (!(obbPenetration(box([0, 0, 0], [1, 1, 1]), box([2.5, 0, 0], [1, 1, 1])) < 0)) fail("separate boxes");
  const s = Math.SQRT1_2;
  const rot = [[s, 0, -s], [0, 1, 0], [s, 0, s]];
  if (!(obbPenetration(box([0, 0, 0], [1, 1, 1]), box([2.3, 0, 0], [1, 1, 1], rot)) > 0)) fail("rotated box reaching in");
  if (!(obbPenetration(box([0, 0, 0], [1, 1, 1]), box([2.5, 0, 0], [1, 1, 1], rot)) < 0)) fail("rotated box apart");
  const z = doorZone(box([0, 1, 0], [0.5, 1, 0.03]));
  if (!z || z.h[2] !== LIMITS.doorClear || z.h[0] !== 0.5) fail("door zone pushes the thin side out");
  if (Math.abs(polylineDistance(5, 3, [[0, 0], [10, 0]]) - 3) > 1e-9) fail("polyline distance");
  const roads = parseRoads("SplineShapeEntity R {\n coords 10 0 20\n Points {\n  ShapePoint \"{A}\" {\n   Position 0 0 0\n  }\n  ShapePoint \"{B}\" {\n   Position 10 0 0\n  }\n }\n {\n  RoadGeneratorEntity : \"{1}x.et\" {\n   {\n    RoadEntity {\n     Width 6\n    }\n   }\n  }\n }\n}\n");
  if (roads.length !== 1 || roads[0].width !== 6 || roads[0].pts[1][0] !== 20 || roads[0].pts[1][1] !== 20) fail(`roads ${JSON.stringify(roads)}`);

  // A fixture town: a floating crate, a grounded one, a crate pushed half off a table, a crate inside
  // another, a barrel in a door's swing, a bench on the road, a sign that hangs (backed) and is exempt.
  const row = (o) => {
    const c = o.c ?? [0, 0.3, 0];
    const h = o.h ?? [0.3, 0.3, 0.3];
    return `bmin=${c[0] - h[0]},${c[1] - h[1]},${c[2] - h[2]} bmax=${c[0] + h[0]},${c[1] + h[1]},${c[2] + h[2]} gap=${o.gap ?? 0} tilt=${o.tilt ?? 0} walk=${o.walk ?? -1} pgap=${o.pgap ?? 0} clip=${o.clip ?? "none"} scale=${o.scale ?? 1} on=${o.on ?? "terrain"} over=${o.over ?? 0} drop=0 backed=${o.backed ?? 0} c=${c.join(",")} h=${h.join(",")} a0=1,0,0 a1=0,1,0 a2=0,0,1 flags=none found=${o.found ?? "x"}`;
  };
  const recs = [
    { name: "FloatCrate", layer: "T.layer", x: 0, y: 1, z: 0, prefab: "Prefabs/Props/Crates/BoxWooden_01.et" },
    { name: "GroundCrate", layer: "T.layer", x: 5, y: 0, z: 0, prefab: "Prefabs/Props/Crates/BoxWooden_01.et" },
    { name: "EdgeCrate", layer: "T.layer", x: 10, y: 0.9, z: 0, prefab: "Prefabs/Props/Crates/BoxWooden_01.et" },
    { name: "CrateA", layer: "T.layer", x: 20, y: 0, z: 0, prefab: "Prefabs/Props/Crates/BoxWooden_01.et" },
    { name: "CrateB", layer: "T.layer", x: 20.2, y: 0, z: 0, prefab: "Prefabs/Props/Crates/BoxWooden_01.et" },
    { name: "DoorBarrel", layer: "T.layer", x: 30, y: 0, z: 0.6, prefab: "Prefabs/Props/Barrel_01.et" },
    { name: "RoadBench", layer: "T.layer", x: 15, y: 0, z: 20, prefab: "Prefabs/Props/BenchStreet_01.et" },
    { name: "Sign_Hung", layer: "T.layer", x: 40, y: 2, z: 0, prefab: "Prefabs/Props/Sign_01.et" },
    { name: "BigCrate", layer: "T.layer", x: 50, y: 0, z: 0, prefab: "Prefabs/Props/Crates/BoxWooden_01.et" },
    { name: "PathBoard", layer: "T.layer", x: 30, y: 0, z: 2.5, prefab: "Prefabs/Comms/LF_DispatchBoard.et" },
    { name: "BigBag", layer: "T.layer", x: 60, y: 0, z: 0, prefab: "Prefabs/Items/Roleplay/LF_MoneyBag.et" },
    { name: "GoodBag", layer: "T.layer", x: 62, y: 0, z: 0, prefab: "Prefabs/Items/Roleplay/LF_MoneyBag.et" },
    { name: "Mystery", layer: "T.layer", x: 64, y: 0, z: 0, prefab: "Prefabs/Props/Mystery_01.et" },
    { name: "CupA", layer: "T.layer", x: 70, y: 1, z: 0, prefab: "Prefabs/Props/Cup_01.et" },
    { name: "CupB", layer: "T.layer", x: 70.26, y: 1, z: 0, prefab: "Prefabs/Props/Cup_01.et" },
    { name: "CupC", layer: "T.layer", x: 70.6, y: 1, z: 5, prefab: "Prefabs/Props/Cup_01.et" },
    { name: "CupD", layer: "T.layer", x: 70.86, y: 1, z: 5, prefab: "Prefabs/Props/Cup_01.et" },
  ];
  const out = [
    `0 ${row({ c: [0, 1.3, 0], gap: 1, pgap: 1 })}`,
    `1 ${row({ c: [5, 0.3, 0] })}`,
    `2 ${row({ c: [10, 1.2, 0], on: "TableRecreation_01", over: 2 })}`,
    `3 ${row({ c: [20, 0.3, 0] })}`,
    `4 ${row({ c: [20.2, 0.3, 0] })}`,
    `5 ${row({ c: [30, 0.4, 0.6], h: [0.3, 0.4, 0.3] })}`,
    `6 ${row({ c: [15, 0.4, 20], h: [0.8, 0.4, 0.3] })}`,
    `7 ${row({ c: [40, 2.3, 0], gap: 2, pgap: 2, backed: 1 })}`,
    `8 ${row({ c: [50, 0.9, 0], h: [0.9, 0.9, 0.9], scale: 3 })}`,
    `9 ${row({ c: [30, 0.6, 2.5], h: [0.9, 0.6, 0.02] })}`,
    `10 ${row({ c: [60, 0.7, 0], h: [0.4, 0.7, 0.4] })}`,
    `11 ${row({ c: [62, 0.275, 0], h: [0.15, 0.275, 0.1] })}`,
    `12 ${row({ c: [64, 5, 0], h: [5, 5, 5] })}`,
    // Two cups on one counter, 0.26 m apart with 0.15 m half widths: 0.04 m deep, under the 0.08 m limit.
    `13 ${row({ c: [70, 1.15, 0], h: [0.15, 0.15, 0.15], on: "Counter_01" })}`,
    `14 ${row({ c: [70.26, 1.15, 0], h: [0.15, 0.15, 0.15], on: "Counter_01" })}`,
    // The same overlap on two different surfaces is a coincidence of two props, kept at the 0.08 m limit.
    `15 ${row({ c: [70.6, 1.15, 5], h: [0.15, 0.15, 0.15], on: "Counter_01" })}`,
    `16 ${row({ c: [70.86, 1.15, 5], h: [0.15, 0.15, 0.15], on: "Shelf_01" })}`,
    "door name=Door_01 root=House_01 rp=30,0,-5 c=30,1,0 h=0.5,1,0.03 a0=1,0,0 a1=0,1,0 a2=0,0,1",
  ].join("\n");
  const res = analyse(recs, out, { roads: [{ name: "Lane", pts: [[0, 20], [30, 20]], width: 4 }] });
  const kinds = (n) => (res.ranked.find((r) => r.name === n)?.problems ?? []).map((p) => p.kind);
  if (!kinds("FloatCrate").includes("floating")) fail(`floating crate: ${kinds("FloatCrate")}`);
  if (kinds("GroundCrate").length) fail(`grounded crate flagged: ${kinds("GroundCrate")}`);
  if (!kinds("EdgeCrate").includes("overhang") || kinds("EdgeCrate").includes("floating")) fail(`edge crate: ${kinds("EdgeCrate")}`);
  if (!kinds("CrateA").includes("overlaps") || !kinds("CrateB").includes("overlaps")) fail("crates inside each other");
  if (!kinds("CupA").includes("overlaps") || !kinds("CupB").includes("overlaps")) fail(`cups on one counter: ${kinds("CupA")}`);
  if (kinds("CupC").includes("overlaps") || kinds("CupD").includes("overlaps")) fail("cups on two surfaces held to the 0.08 m limit");
  const stock = analyse(recs, `${out}
shelf shop/Water@0 ${row({ c: [70.1, 1.15, 0], h: [0.15, 0.15, 0.15], on: "Counter_01", found: "WaterBottle" })}`, {});
  if (!stock.ranked.some((r) => r.layer === "(store stock)" && r.problems.some((p) => p.kind === "overlaps"))) fail("a store stock row is measured and paired with the placed props");
  if (!kinds("DoorBarrel").includes("blocks-door")) fail(`door barrel: ${kinds("DoorBarrel")}`);
  if (!kinds("RoadBench").includes("in-road")) fail(`road bench: ${kinds("RoadBench")}`);
  if (kinds("Sign_Hung").length) fail(`hung sign flagged: ${kinds("Sign_Hung")}`);
  if (!kinds("BigCrate").includes("scaled")) fail(`scaled crate: ${kinds("BigCrate")}`);
  if (!kinds("PathBoard").includes("door-path")) fail(`board in front of the door: ${kinds("PathBoard")}`);
  if (!kinds("BigBag").includes("size")) fail(`1.4 m money bag: ${kinds("BigBag")}`);
  if (kinds("GoodBag").length) fail(`0.55 m money bag flagged: ${kinds("GoodBag")}`);
  if (kinds("Mystery").length) fail("a prefab outside the size table is flagged");
  if (kinds("GroundCrate").includes("size") || kinds("BigCrate").includes("size") === false) fail(`crate sizes: ${kinds("BigCrate")}`);
  if (res.sizeCovered !== 11) fail(`size coverage ${res.sizeCovered}`);
  if (res.ranked[0]?.name !== "FloatCrate") fail(`rank order: ${res.ranked.map((r) => r.name)}`);
  const suppressed = analyse(recs, out, { allow: [{ match: "^FloatCrate", kind: "floating", why: "test" }] });
  if (suppressed.ranked.some((r) => r.name === "FloatCrate")) fail("the allowlist suppresses a problem");
  const text = inputText(recs.slice(0, 2));
  if (!text.startsWith("town ") || !text.includes("\n0 FloatCrate BoxWooden_01 0 1 0 0")) fail(`input text: ${text}`);
  // "Roadmap_" holds "map": a backed prop named that way was read as a hung map and never checked for floating or
  // sinking (the fuel counter stood 0.35 m into the kiosk floor and through its wall, #936).
  const kioskCounter = { name: "Roadmap_Fuel_Counter", prefab: "Prefabs/Props/Commercial/CounterShop_01.et" };
  const sunkM = { gap: -0.346, pgap: -0.345, over: 0, tilt: 0, scale: 1, walk: -1, backed: true, clip: [], box: { h: [0.75, 0.52, 0.56] } };
  if (!singleProblems(kioskCounter, sunkM).some((p) => p.kind === "sunk")) fail("a backed prop named Roadmap_* sunk 0.35 m into a floor is flagged");
  if (singleProblems({ name: "Roadmap_Sign_Map", prefab: "Prefabs/Props/Signs/Map_01.et" }, sunkM).length !== 0) fail("a real map on a wall is still hung");
  worldSelftest(fail);
  if (!process.exitCode) console.log("place-audit selftest: OK");
  return process.exitCode ?? 0;
}

export async function main(argv) {
  const [sub, ...rest] = argv;
  if (sub === "placement") return placement(rest);
  if (sub === "selftest") return selftest();
  console.log("usage: tools\\cli audit placement [--top 40] [--area <name>] [--layer A.layer,B.layer] [--from <out.txt>] [--out dir] [--strict]\n       tools\\cli audit selftest");
  return 1;
}

if (process.argv[1] && resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
  main(process.argv.slice(2)).then((c) => process.exit(c ?? 0));
}
