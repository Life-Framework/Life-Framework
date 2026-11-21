// tools/town/parked-cars.mjs - `tools\cli town parked-cars <place> [--dry] [--no-audit]`: fit the `parked-cars` outposts slot (#3532):
// one LF_ParkedCarMarker (#3365) beside each drop building, on the kerb or a driveway, a car's length from the nearest road.
//
// Reads  docs/everon/places/<place>/mapping.json (slot `parked-cars`: `drops: [{ place, building }]`), each drop place's dossier.json
//        (buildings, entrances, roads, props) and the MainWorld layers already written (what stands there is kept clear).
// Writes addons/LF_Everon1989/Worlds/MainWorld/MainWorld_Layers/Place_<place>_parked-cars.layer, tools/town/claims/<place>-parked-cars.json.
//
// No coordinate is typed: each marker is searched on rings around the host footprint. A spot passes when the car's box (4.6 x 2.1 m,
// the marker's heading along the road) clears every building by 0.6 m, every entrance by 3.5 m, every road's carriageway, every
// dossier prop and every entity of another Saint-Philippe layer by 2.5 m (the taxi rank by 9 m), and every other marker by 6 m.
// The ground height comes from the dossier (the nearest street prop or building floor) unless --probe is given. --probe (#3879) asks the
// island itself: the best few dossier spots per building are checked with rays on a booted MainWorld (the surface under the centre and
// corners, the box outline at car height for walls, fences and gates, the door leaves' swing), and the first clean one is written with the
// height the island answered. The audit then judges the written layer.
import { spawnSync } from "node:child_process";
import { existsSync, mkdirSync, readFileSync, readdirSync, writeFileSync } from "node:fs";
import { dirname, join, relative, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { LIMITS, doorZone, measureTown, obbPenetration, parseDoor } from "../../agent-box/place-audit.mjs";
import { CLAIMS_DIR, LAYER_DIR, claimNameOf, layerNameOf } from "./check.mjs";

export const help = "parked-cars <place> [--slot <id>] [--probe] [--dry] [--no-audit]   a parked-cars outposts slot (default parked-cars; police-cars #3875): one marker (a drop's \"count\", default 1) beside each drop building, on the kerb, clear of roads, doors and stops; --probe sites each one on the island by ray; writes the layer and the claim";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");
export const SLOT = "parked-cars";
export const MARKER_PREFAB = "{C48202E5BB6761C0}Prefabs/Systems/Vehicles/LF_ParkedCarMarker.et";
// A slot whose mapping entry says "marker": "police" puts out the locked police-car marker (#3875) instead of the civilian one.
export const MARKERS = { police: "{5D2B8E41A7C3906F}Prefabs/Systems/Vehicles/LF_PoliceCarMarker.et" };
export const CAR = { half: [2.3, 1.05] }; // half length and half width of the keep-clear box around the marker, metres
export const ROAD_HALF = 3.6; // a carriageway's half width, metres
export const CLEAR = { building: 0.6, door: 3.5, road: 0.4, prop: 2.5, layer: 2.5, taxi: 9, marker: 6 };
const LIFT = 0.15; // the marker sits this far above the dossier's ground; the car settles onto the real one
const r3 = (n) => Math.round(n * 1000) / 1000;
const readJson = (f, what) => {
  if (!existsSync(f)) throw new Error(`no ${what}: ${f} does not exist`);
  return JSON.parse(readFileSync(f, "utf8"));
};

// Pure: distance from a point to a segment, and to a polyline (list of [x, z]).
export function distToSegment(px, pz, [ax, az], [bx, bz]) {
  const dx = bx - ax;
  const dz = bz - az;
  const len2 = dx * dx + dz * dz;
  const t = len2 ? Math.max(0, Math.min(1, ((px - ax) * dx + (pz - az) * dz) / len2)) : 0;
  return Math.hypot(px - (ax + t * dx), pz - (az + t * dz));
}
export const distToLine = (px, pz, pts) => Math.min(...pts.slice(1).map((p, i) => distToSegment(px, pz, pts[i], p)));

// Pure: signed-free distance from a point to a polygon (0 inside).
export function distToPoly(px, pz, poly) {
  let inside = false;
  for (let i = 0, j = poly.length - 1; i < poly.length; j = i++) {
    const [xi, zi] = poly[i];
    const [xj, zj] = poly[j];
    if (zi > pz !== zj > pz && px < ((xj - xi) * (pz - zi)) / (zj - zi) + xi) inside = !inside;
  }
  if (inside) return 0;
  return Math.min(...poly.map((p, i) => distToSegment(px, pz, poly[(i + poly.length - 1) % poly.length], p)));
}

// Pure: the heading (degrees, the engine's angleY: 0 faces +z, 90 faces +x) of the road nearest to a point, and how far it is.
export function nearestRoad(px, pz, roads) {
  let best = { d: Infinity, yaw: 0 };
  for (const r of roads) {
    const pts = r.points ?? [];
    for (let i = 1; i < pts.length; i++) {
      const d = distToSegment(px, pz, pts[i - 1], pts[i]);
      if (d < best.d) best = { d, yaw: (Math.atan2(pts[i][0] - pts[i - 1][0], pts[i][1] - pts[i - 1][1]) * 180) / Math.PI, road: r.id };
    }
  }
  return best;
}

// Pure: the car's keep-clear box corners at (x, z) heading yaw degrees.
export function carBox(x, z, yaw) {
  const a = (yaw * Math.PI) / 180;
  const f = [Math.sin(a), Math.cos(a)]; // forward
  const s = [Math.cos(a), -Math.sin(a)]; // right
  const [hl, hw] = CAR.half;
  return [[1, 1], [1, -1], [-1, -1], [-1, 1]].map(([i, j]) => [x + f[0] * hl * i + s[0] * hw * j, z + f[1] * hl * i + s[1] * hw * j]);
}

// Pure: why a spot is refused, or null. `world`: { buildings: [{ id, poly }], doors: [[x, z]], roads, props: [[x, z]], layers: [[x, z]], taxi: [[x, z]], placed: [[x, z]] }.
export function refuse(x, z, yaw, world) {
  const box = carBox(x, z, yaw);
  const samples = [[x, z], ...box, ...box.map((p, i) => [(p[0] + box[(i + 1) % 4][0]) / 2, (p[1] + box[(i + 1) % 4][1]) / 2])];
  for (const b of world.buildings) {
    if (samples.some(([sx, sz]) => distToPoly(sx, sz, b.poly) < CLEAR.building)) return `touches building ${b.id}`;
  }
  for (const r of world.roads) {
    const pts = r.points ?? [];
    if (pts.length > 1 && samples.some(([sx, sz]) => distToLine(sx, sz, pts) < ROAD_HALF + CLEAR.road)) return `on the carriageway of ${r.id}`;
  }
  for (const [dx, dz] of world.doors) if (samples.some(([sx, sz]) => Math.hypot(sx - dx, sz - dz) < CLEAR.door)) return "blocks a door";
  for (const [px, pz] of world.props) if (samples.some(([sx, sz]) => Math.hypot(sx - px, sz - pz) < CLEAR.prop)) return "overlaps a prop";
  for (const [px, pz] of world.layers) if (samples.some(([sx, sz]) => Math.hypot(sx - px, sz - pz) < CLEAR.layer)) return "overlaps another layer's entity";
  for (const [px, pz] of world.taxi) if (Math.hypot(x - px, z - pz) < CLEAR.taxi) return "near the taxi rank";
  for (const [px, pz] of world.placed) if (Math.hypot(x - px, z - pz) < CLEAR.marker) return "next to another marker";
  return null;
}

// Pure: the spots beside one building, best first, or an empty list with the reasons seen. Candidates ring the footprint at 2 to 12 m
// out, every 1.25 m along the perimeter, each heading along the nearest road; the best has the car's centre closest to 5.6 m from the
// road centre line (a kerb: the car's side sits just off the carriageway) and the least distance from the building. `limit` keeps the
// best few, at least SPREAD metres apart, for a probe to choose among.
export const SPREAD = 1.5;
export function candidates(building, world, margin = 14, limit = 1) {
  const poly = building.footprint?.corners;
  if (!poly || poly.length < 3) return { list: [], reasons: { "no footprint": 1 } };
  const reasons = {};
  const all = [];
  for (let i = 0; i < poly.length; i++) {
    const [ax, az] = poly[i];
    const [bx, bz] = poly[(i + 1) % poly.length];
    const len = Math.hypot(bx - ax, bz - az);
    const steps = Math.max(1, Math.floor(len / 1.25));
    for (let s = 0; s <= steps; s++) {
      const px = ax + ((bx - ax) * s) / steps;
      const pz = az + ((bz - az) * s) / steps;
      for (let out = 2; out <= 12; out += 1) {
        for (const side of [1, -1]) {
          // The outward normal of this edge, checked against the polygon so a concave corner never points in.
          const nx = ((bz - az) / len) * side;
          const nz = (-(bx - ax) / len) * side;
          const x = px + nx * out;
          const z = pz + nz * out;
          if (distToPoly(x, z, poly) > margin - 0.5 || distToPoly(x, z, poly) < 1) continue;
          const road = nearestRoad(x, z, world.roads);
          if (!Number.isFinite(road.d) || road.d > 14) {
            reasons["no road near"] = (reasons["no road near"] ?? 0) + 1;
            continue;
          }
          const yaw = r3(Math.round(road.yaw));
          const why = refuse(x, z, yaw, world);
          if (why) {
            reasons[why] = (reasons[why] ?? 0) + 1;
            continue;
          }
          const score = Math.abs(road.d - 5.6) * 2 + distToPoly(x, z, poly) * 0.3;
          all.push({ x: r3(x), z: r3(z), yaw, score, road: road.road, roadDistance: r3(road.d) });
        }
      }
    }
  }
  all.sort((a, b) => a.score - b.score);
  const list = [];
  for (const c of all) {
    if (list.length >= limit) break;
    if (list.every((k) => Math.hypot(k.x - c.x, k.z - c.z) >= SPREAD)) list.push({ ...c, idx: list.length });
  }
  return { list, reasons };
}

// Pure: the best spot beside one building, or null with the reasons seen.
export function planCar(building, world, margin = 14) {
  const { list, reasons } = candidates(building, world, margin, 1);
  return { spot: list[0] ?? null, reasons };
}

// Pure: the ground height at (x, z) from the dossier: the nearest street prop within 18 m, else the nearest building's floor.
export function groundAt(x, z, dossier) {
  let best = null;
  for (const p of dossier.props ?? []) {
    const d = Math.hypot(p.x - x, p.z - z);
    if (typeof p.y === "number" && d < 18 && (!best || d < best.d)) best = { d, y: p.y };
  }
  if (best) return best.y;
  for (const b of dossier.buildings ?? []) {
    const d = Math.hypot(b.x - x, b.z - z);
    if (typeof b.y === "number" && (!best || d < best.d)) best = { d, y: b.y };
  }
  return best ? best.y : 0;
}

const entityText = (name, spot, marker = MARKER_PREFAB) => `GenericEntity ${name} : "${marker}" {
 coords ${spot.x} ${spot.y} ${spot.z}
 angleY ${spot.yaw}
}
`;

// The coordinates of every top-level entity of the other MainWorld layers (this slot's own layer excluded).
function otherLayerCoords(root, ownLayer) {
  const dir = join(root, LAYER_DIR);
  const out = { all: [], taxi: [] };
  if (!existsSync(dir)) return out;
  for (const f of readdirSync(dir)) {
    if (!f.endsWith(".layer") || f === ownLayer || f === "default.layer" || f === "Gamemode.layer") continue;
    const text = readFileSync(join(dir, f), "utf8");
    for (const m of text.matchAll(/^\s*coords\s+(-?[\d.]+)\s+(-?[\d.]+)\s+(-?[\d.]+)/gm)) {
      const p = [Number(m[1]), Number(m[3])];
      out.all.push(p);
      if (/taxi/i.test(f)) out.taxi.push(p);
    }
  }
  return out;
}

export function worldOf(dossier, drops, others) {
  const dossiers = { [dossier.place.slug]: dossier, ...drops };
  const buildings = [];
  const doors = [];
  const props = [];
  const roads = [];
  for (const [slug, d] of Object.entries(dossiers)) {
    for (const b of d.buildings ?? []) {
      if (b.footprint?.corners?.length >= 3) buildings.push({ id: `${slug}/${b.id}`, poly: b.footprint.corners });
      for (const e of b.entrances ?? []) doors.push([e.x, e.z]);
    }
    for (const p of d.props ?? []) props.push([p.x, p.z]);
    for (const r of d.roads ?? []) roads.push({ ...r, id: `${slug}/${r.id}` });
  }
  return { buildings, doors, props, roads, layers: others.all, taxi: others.taxi, placed: [] };
}

// `accept(drop, cand)` (--probe) answers { ok, y, why } for one candidate; without it the dossier's ground height is used. `tries`
// candidates per drop are offered to it. `all` holds every drop's shortlist (the probe's input).
export function plan({ mapping, dossiers, others, slot = SLOT, accept = null, tries = 1 }) {
  const entry = mapping.slots?.[slot];
  if (!entry?.drops?.length) throw new Error(`slot ${slot} is not mapped (an entry with "drops": [{ place, building }])`);
  const own = Object.values(dossiers)[0];
  const world = worldOf(own, dossiers, others);
  const out = [];
  const all = [];
  const problems = [];
  for (const drop of entry.drops) {
    const d = dossiers[drop.place];
    const b = d?.buildings?.find((x) => x.id === drop.building);
    if (!b) {
      problems.push(`${drop.place}/${drop.building}: not in its dossier`);
      continue;
    }
    // A drop with "count": N puts N markers beside its building; the first claims the best spot, each next the best one a marker's
    // clearance away from those placed before it.
    const count = Math.max(1, Math.floor(drop.count ?? 1));
    let anchor = null; // the first marker of the drop: the next ones stand in a row with it, not on another street
    for (let n = 0; n < count; n++) {
      // A drop's own building is not a reason to refuse, so its polygon stays in `world.buildings` (clearance 0.6 m).
      // With a probe the shortlist must not depend on what was placed before it (its keys are the candidate indices), so the other markers
      // are checked below instead.
      const { list, reasons } = candidates(b, accept ? { ...world, placed: [] } : world, entry.margin ?? 14, accept ? tries : n > 0 ? 24 : 1);
      if (anchor) list.sort((p, q) => Math.hypot(p.x - anchor.x, p.z - anchor.z) - Math.hypot(q.x - anchor.x, q.z - anchor.z));
      let spot = null;
      for (const c of list) {
        if (world.placed.some(([px, pz]) => Math.hypot(c.x - px, c.z - pz) < CLEAR.marker)) {
          reasons["next to another marker"] = (reasons["next to another marker"] ?? 0) + 1;
          continue;
        }
        if (!accept) {
          spot = { ...c, y: r3(groundAt(c.x, c.z, d) + LIFT) };
          break;
        }
        const v = accept(drop, c);
        if (v?.ok) {
          spot = { ...c, y: v.y };
          break;
        }
        const why = `probe: ${v?.why ?? "no answer"}`;
        reasons[why] = (reasons[why] ?? 0) + 1;
      }
      if (n === 0) all.push({ drop, cands: list });
      if (!spot) {
        problems.push(`${drop.place}/${drop.building}: no spot (${Object.entries(reasons).map(([k, v]) => `${k} x${v}`).join(", ") || "none tried"})`);
        continue;
      }
      world.placed.push([spot.x, spot.z]);
      anchor ??= spot;
      const tail = count > 1 ? `_${n + 1}` : "";
      out.push({ drop, building: b, spot, name: `Fit_${drop.place.replaceAll("-", "_")}_${slot.replaceAll("-", "_")}_car_${drop.building}${tail}` });
    }
  }
  return { out, all, problems };
}

// ------------------------------------------------------------------ the island probe (#3879)
//
// The dossier knows buildings, entrances and surveyed props, not the base map's walls, fences, gates and kerbs: b15 stood against a
// wooden wall and a gate's swing, b17 on a net fence. Two boots of MainWorld answer: (1) one ray down from each candidate's centre and
// corners (the surface under the car), (2) rays along the box outline, its diagonals and its long axis at three car heights, pushed out
// BODY_OUT metres, plus the door leaves of the circle, judged with the audit's own door zone.
export const BODY_OUT = 0.5; // the outline rays run this far outside the car's keep-clear box
export const HEIGHTS = [0.45, 0.75, 1.1]; // ray heights over the surface, metres: bumper, a guard rail (#3875: a CrashBarrier slipped between 0.45 and 1.1) and the window line
export const FLAT = 0.8; // the surface under the box may differ by this much (a car tilts to the slope, the audit allows 12 degrees)
const SKY = 2.5; // a down ray starts this far over the dossier height and ends DEEP under it
const DEEP = 3;
const SHELF = 0.6; // an entity this far over the terrain under the centre is a structure, not the street

export const keyOf = (drop, c) => `${drop.place}/${drop.building}#${c.idx}`;
const idOf = (drop, c) => keyOf(drop, c).replace(/[/#]/g, "_");
function corners(c, grow = 0) {
  const a = (c.yaw * Math.PI) / 180;
  const f = [Math.sin(a), Math.cos(a)];
  const s = [Math.cos(a), -Math.sin(a)];
  const hl = CAR.half[0] + grow;
  const hw = CAR.half[1] + grow;
  return [[1, 1], [1, -1], [-1, -1], [-1, 1]].map(([i, j]) => [c.x + f[0] * hl * i + s[0] * hw * j, c.z + f[1] * hl * i + s[1] * hw * j]);
}

// The down rays of the first round: the centre and the four corners of every item { id, c, y0 }.
export function groundRays(items) {
  const rays = [];
  for (const { id, c, y0 } of items) {
    [[c.x, c.z], ...corners(c)].forEach(([x, z], i) => rays.push(`ray ${id}_g${i} ${r3(x)} ${r3(y0 + SKY)} ${r3(z)} ${r3(x)} ${r3(y0 - DEEP)} ${r3(z)}`));
  }
  return rays;
}

// The level rays of the second round, over the surface `g` the first round measured for each item { id, c, g }: 7 per height.
export function bodyRays(items) {
  const rays = [];
  for (const { id, c, g } of items) {
    const box = corners(c, BODY_OUT);
    const a = (c.yaw * Math.PI) / 180;
    const reach = CAR.half[0] + BODY_OUT;
    const lines = box.map((p, i) => [p, box[(i + 1) % 4]]);
    lines.push([box[0], box[2]], [box[1], box[3]], [[c.x + Math.sin(a) * reach, c.z + Math.cos(a) * reach], [c.x - Math.sin(a) * reach, c.z - Math.cos(a) * reach]]);
    HEIGHTS.forEach((h, k) => lines.forEach(([p, q], i) => rays.push(`ray ${id}_b${k}${i} ${r3(p[0])} ${r3(g + h)} ${r3(p[1])} ${r3(q[0])} ${r3(g + h)} ${r3(q[1])}`)));
  }
  return rays;
}

// out.txt "ray" lines -> Map id -> { hit, root, d, len, p, sy }.
export function parseRays(outText) {
  const byId = new Map();
  for (const line of String(outText ?? "").split(/\r?\n/)) {
    if (!line.startsWith("ray ")) continue;
    const rest = line.slice(4);
    const sp = rest.indexOf(" ");
    const kv = Object.fromEntries(rest.slice(sp + 1).trim().split(/\s+/).map((tok) => [tok.slice(0, tok.indexOf("=")), tok.slice(tok.indexOf("=") + 1)]));
    byId.set(rest.slice(0, sp), { hit: kv.hit, root: kv.root, d: Number(kv.d), len: Number(kv.len), p: String(kv.p ?? "0,0,0").split(",").map(Number), sy: Number(kv.sy) });
  }
  return byId;
}
// An entity was struck (a terrain hit has no entity: a slope under a level ray is not a wall).
const struck = (r) => r && r.hit !== "none" && r.d < r.len - 0.01;

// Pure: the surface under a candidate from its five down rays: { g } (under the centre) or { why }.
export function surfaceOf(id, rays) {
  const ys = [];
  let centre = 0;
  for (let i = 0; i < 5; i++) {
    const r = rays.get(`${id}_g${i}`);
    if (!r) return { why: "the island gave no ray answer" };
    const y = struck(r) ? Math.max(r.p[1], r.sy) : r.sy;
    if (i === 0) {
      if (struck(r) && r.p[1] > r.sy + SHELF) return { why: `the surface under the centre is ${r.root}, not the street` };
      centre = y;
    }
    ys.push(y);
  }
  const spread = Math.max(...ys) - Math.min(...ys);
  if (spread > FLAT) return { why: `the surface under the box varies ${r3(spread)} m` };
  return { g: r3(centre) };
}

// Pure: why the level rays and the door leaves refuse a candidate (null: clean). `doors`: parseDoor rows.
export function bodyWhy(id, c, g, rays, doors) {
  for (let k = 0; k < HEIGHTS.length; k++) {
    for (let i = 0; i < 7; i++) {
      const r = rays.get(`${id}_b${k}${i}`);
      if (struck(r)) return `${r.root} at car height (${r3(r.d)} m along an outline ray)`;
    }
  }
  const a = (c.yaw * Math.PI) / 180;
  const car = { c: [c.x, g + 0.8, c.z], h: [CAR.half[1], 0.8, CAR.half[0]], a: [[Math.cos(a), 0, -Math.sin(a)], [0, 1, 0], [Math.sin(a), 0, Math.cos(a)]] };
  for (const d of doors) {
    if (Math.abs(d.box.c[0] - c.x) > 15 || Math.abs(d.box.c[2] - c.z) > 15) continue;
    const swing = doorZone(d.box);
    if (swing && obbPenetration(swing, car) > LIMITS.door) return `in the swing of a door of ${d.root}`;
    const walk = doorZone(d.box, LIMITS.doorWalk);
    if (walk && d.box.c[1] - d.box.h[1] < 1.2 && obbPenetration(walk, car) > LIMITS.door) return `in the walk-up of a door of ${d.root}`;
  }
  return null;
}

// Two island boots: the answers per candidate key { ok, y, why }. `cands`: [{ drop, c, y0 }].
export function probe(cands, { root = ROOT, log = console.log } = {}) {
  const outDir = join(root, "tmp", "parked-cars-probe");
  const items = cands.map(({ drop, c, y0 }) => ({ id: idOf(drop, c), k: keyOf(drop, c), c, y0 }));
  const xs = items.map((i) => i.c.x);
  const zs = items.map((i) => i.c.z);
  // measureTown sizes the door circle from its entity rows: two rows at the extremes of the candidates give the circle (and are "missing").
  const records = [[Math.min(...xs), Math.min(...zs)], [Math.max(...xs), Math.max(...zs)]].map(([x, z], i) => ({ name: `probe_${i}`, prefab: "", x, y: 0, z }));
  const boot = (extra, dir) => measureTown(records, { outDir: join(outDir, dir), extra, island: true, log });
  const r1 = boot(groundRays(items), "ground");
  if (r1.outText == null) throw new Error("the island probe wrote no out.txt (round 1)");
  const ground = parseRays(r1.outText);
  const answers = {};
  const seated = [];
  for (const it of items) {
    const s = surfaceOf(it.id, ground);
    if (s.why) answers[it.k] = { ok: false, why: s.why };
    else seated.push({ ...it, g: s.g });
  }
  if (!seated.length) return answers;
  const r2 = boot(bodyRays(seated), "body");
  if (r2.outText == null) throw new Error("the island probe wrote no out.txt (round 2)");
  const rays = parseRays(r2.outText);
  const doors = r2.outText.split(/\r?\n/).filter((l) => l.startsWith("door ")).map((l) => parseDoor(l.slice(5)));
  for (const it of seated) {
    const why = bodyWhy(it.id, it.c, it.g, rays, doors);
    answers[it.k] = why ? { ok: false, why } : { ok: true, y: r3(it.g + LIFT) };
  }
  return answers;
}

export async function main(args, { root = ROOT } = {}) {
  const at = args.indexOf("--slot");
  const slot = at >= 0 ? args[at + 1] : SLOT;
  const rest = args.filter((a, i) => !(at >= 0 && (i === at || i === at + 1)));
  const flags = new Set(rest.filter((a) => a.startsWith("--")));
  const pos = rest.filter((a) => !a.startsWith("--"));
  const NAME = /^[a-z0-9][a-z0-9-]*$/;
  if (pos.length !== 1 || !NAME.test(pos[0]) || !NAME.test(slot ?? "")) {
    console.log("usage: tools\\cli town parked-cars <place> [--slot <id>] [--probe] [--dry] [--no-audit]");
    return 2;
  }
  const place = pos[0];
  const branch = (() => {
    const r = spawnSync("git", ["rev-parse", "--abbrev-ref", "HEAD"], { cwd: root, encoding: "utf8", windowsHide: true });
    return r.status === 0 ? String(r.stdout).trim() : null;
  })();
  if (!flags.has("--dry") && !new RegExp(`^ws/town-draft-${place}-${slot}(-[a-z0-9-]+)?$`).test(branch ?? "")) {
    console.log(`REFUSE  town parked-cars writes an island layer only from the worktree ws/town-draft-${place}-${slot} (a '-<tail>' follow-up is fine; this is ${branch ?? "unknown"}); or pass --dry`);
    return 1;
  }
  let planned;
  let mapping;
  try {
    const dir = join(root, "docs", "everon", "places", place);
    const dossier = readJson(join(dir, "dossier.json"), "dossier");
    mapping = readJson(join(dir, "mapping.json"), "mapping");
    const dossiers = { [place]: dossier };
    for (const dr of mapping.slots?.[slot]?.drops ?? []) {
      if (!dossiers[dr.place]) dossiers[dr.place] = readJson(join(root, "docs", "everon", "places", dr.place, "dossier.json"), `dossier of ${dr.place}`);
    }
    const others = otherLayerCoords(root, layerNameOf(place, slot));
    planned = plan({ mapping, dossiers, others, slot });
    if (flags.has("--probe")) {
      // 16 shortlist spots per marker a drop asks for (#3875: three police cars ran out of clean spots at 16 for the drop).
      const tries = 16 * Math.max(1, ...mapping.slots[slot].drops.map((d) => Math.floor(d.count ?? 1)));
      const short = plan({ mapping, dossiers, others, slot, tries, accept: () => ({ ok: false, why: "shortlist" }) });
      const cands = [];
      for (const o of short.all) for (const c of o.cands) cands.push({ drop: o.drop, c, y0: groundAt(c.x, c.z, dossiers[o.drop.place]) });
      console.log(`probe: ${cands.length} candidate spot(s) for ${short.all.length} drop(s), two island boots`);
      const answers = probe(cands, { root });
      planned = plan({ mapping, dossiers, others, slot, tries, accept: (drop, c) => answers[keyOf(drop, c)] });
    }
  } catch (e) {
    console.log(`REFUSE  ${e.message}`);
    return 1;
  }
  for (const p of planned.out) console.log(`car  ${p.drop.place}/${p.drop.building}: ${p.spot.x} ${p.spot.y} ${p.spot.z} yaw ${p.spot.yaw} (road ${p.spot.road} ${p.spot.roadDistance} m)`);
  for (const p of planned.problems) console.log(`REFUSE  ${p}`);
  if (planned.problems.length) {
    console.log(`SUMMARY town-parked-cars fail ${place}: ${planned.problems.length} of ${mapping.slots[slot].drops.length} drop(s) refused`);
    return 1;
  }
  const layerName = layerNameOf(place, slot);
  const claim = {
    schema: 1,
    place,
    slot,
    drops: mapping.slots[slot].drops.map((d) => ({ place: d.place, building: d.building })),
    kit: SLOT,
    size: "small",
    layer: `${LAYER_DIR}/${layerName}`,
    kinds: [],
    rows: planned.out.length,
    date: new Date().toISOString().slice(0, 10),
  };
  const text = planned.out.map((p) => entityText(p.name, p.spot, MARKERS[mapping.slots[slot].marker] ?? MARKER_PREFAB)).join("");
  if (flags.has("--dry")) {
    console.log(text);
    console.log(`SUMMARY town-parked-cars dry ${place}: ${planned.out.length} marker(s)`);
    return 0;
  }
  const layerFile = join(root, LAYER_DIR, layerName);
  const claimFile = join(root, CLAIMS_DIR, claimNameOf(place, slot));
  mkdirSync(dirname(layerFile), { recursive: true });
  mkdirSync(dirname(claimFile), { recursive: true });
  writeFileSync(layerFile, text);
  writeFileSync(claimFile, `${JSON.stringify(claim, null, 2)}\n`);
  let ok = true;
  const steps = [];
  if (!flags.has("--no-audit")) {
    const cli = join(root, "tools", process.platform === "win32" ? "cli.cmd" : "cli");
    const r = spawnSync(cli, ["audit", "placement", "--strict", "--world", "MainWorld", "--layer", layerName, "--hard-layer", layerName], { cwd: root, encoding: "utf8", shell: process.platform === "win32", timeout: 600000 });
    ok = r.status === 0;
    steps.push(`audit ${ok ? "ok" : "fail"}`);
    if (!ok) console.log(String(r.stdout).split("\n").slice(-30).join("\n"));
  }
  console.log(`SUMMARY town-parked-cars ${ok ? "ok" : "fail"} ${place}: ${planned.out.length} marker(s), ${relative(root, layerFile).replaceAll("\\", "/")}${steps.length ? `; ${steps.join(", ")}` : "; audit skipped"}`);
  return ok ? 0 : 1;
}
