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
// The ground height comes from the dossier (the nearest street prop or building floor): the audit judges the rest.
import { spawnSync } from "node:child_process";
import { existsSync, mkdirSync, readFileSync, readdirSync, writeFileSync } from "node:fs";
import { dirname, join, relative, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { CLAIMS_DIR, LAYER_DIR, claimNameOf, layerNameOf } from "./check.mjs";

export const help = "parked-cars <place> [--dry] [--no-audit]   the parked-cars outposts slot: one parked-car marker beside each drop building, on the kerb, clear of roads, doors and stops; writes the layer and the claim";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");
export const SLOT = "parked-cars";
export const MARKER_PREFAB = "{C48202E5BB6761C0}Prefabs/Systems/Vehicles/LF_ParkedCarMarker.et";
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

// Pure: the best spot beside one building, or null with the reasons seen. Candidates ring the footprint at 2 to 12 m out, every 1.25 m
// along the perimeter, each heading along the nearest road; the best has the car's centre closest to 5.6 m from the road centre line
// (a kerb: the car's side sits just off the carriageway) and the least distance from the building.
export function planCar(building, world, margin = 14) {
  const poly = building.footprint?.corners;
  if (!poly || poly.length < 3) return { spot: null, reasons: { "no footprint": 1 } };
  const reasons = {};
  let best = null;
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
          if (!best || score < best.score) best = { x: r3(x), z: r3(z), yaw, score, road: road.road, roadDistance: r3(road.d) };
        }
      }
    }
  }
  return { spot: best, reasons };
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

const entityText = (name, spot) => `GenericEntity ${name} : "${MARKER_PREFAB}" {
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

export function plan({ mapping, dossiers, others, slot = SLOT }) {
  const entry = mapping.slots?.[slot];
  if (!entry?.drops?.length) throw new Error(`slot ${slot} is not mapped (an entry with "drops": [{ place, building }])`);
  const own = Object.values(dossiers)[0];
  const world = worldOf(own, dossiers, others);
  const out = [];
  const problems = [];
  for (const drop of entry.drops) {
    const d = dossiers[drop.place];
    const b = d?.buildings?.find((x) => x.id === drop.building);
    if (!b) {
      problems.push(`${drop.place}/${drop.building}: not in its dossier`);
      continue;
    }
    // A drop's own building is not a reason to refuse, so its polygon stays in `world.buildings` (clearance 0.6 m).
    const { spot, reasons } = planCar(b, world, entry.margin ?? 14);
    if (!spot) {
      problems.push(`${drop.place}/${drop.building}: no spot (${Object.entries(reasons).map(([k, v]) => `${k} x${v}`).join(", ") || "none tried"})`);
      continue;
    }
    spot.y = r3(groundAt(spot.x, spot.z, d) + LIFT);
    world.placed.push([spot.x, spot.z]);
    out.push({ drop, building: b, spot, name: `Fit_${drop.place.replaceAll("-", "_")}_${slot.replaceAll("-", "_")}_car_${drop.building}` });
  }
  return { out, problems };
}

export async function main(args, { root = ROOT } = {}) {
  const flags = new Set(args.filter((a) => a.startsWith("--")));
  const pos = args.filter((a) => !a.startsWith("--"));
  if (pos.length !== 1 || !/^[a-z0-9][a-z0-9-]*$/.test(pos[0])) {
    console.log("usage: tools\\cli town parked-cars <place> [--dry] [--no-audit]");
    return 2;
  }
  const place = pos[0];
  const branch = (() => {
    const r = spawnSync("git", ["rev-parse", "--abbrev-ref", "HEAD"], { cwd: root, encoding: "utf8", windowsHide: true });
    return r.status === 0 ? String(r.stdout).trim() : null;
  })();
  if (!flags.has("--dry") && !new RegExp(`^ws/town-draft-${place}-${SLOT}$`).test(branch ?? "")) {
    console.log(`REFUSE  town parked-cars writes an island layer only from the worktree ws/town-draft-${place}-${SLOT} (this is ${branch ?? "unknown"}); or pass --dry`);
    return 1;
  }
  let planned;
  let mapping;
  try {
    const dir = join(root, "docs", "everon", "places", place);
    const dossier = readJson(join(dir, "dossier.json"), "dossier");
    mapping = readJson(join(dir, "mapping.json"), "mapping");
    const dossiers = { [place]: dossier };
    for (const dr of mapping.slots?.[SLOT]?.drops ?? []) {
      if (!dossiers[dr.place]) dossiers[dr.place] = readJson(join(root, "docs", "everon", "places", dr.place, "dossier.json"), `dossier of ${dr.place}`);
    }
    planned = plan({ mapping, dossiers, others: otherLayerCoords(root, layerNameOf(place, SLOT)) });
  } catch (e) {
    console.log(`REFUSE  ${e.message}`);
    return 1;
  }
  for (const p of planned.out) console.log(`car  ${p.drop.place}/${p.drop.building}: ${p.spot.x} ${p.spot.y} ${p.spot.z} yaw ${p.spot.yaw} (road ${p.spot.road} ${p.spot.roadDistance} m)`);
  for (const p of planned.problems) console.log(`REFUSE  ${p}`);
  if (planned.problems.length) {
    console.log(`SUMMARY town-parked-cars fail ${place}: ${planned.problems.length} of ${mapping.slots[SLOT].drops.length} drop(s) refused`);
    return 1;
  }
  const layerName = layerNameOf(place, SLOT);
  const claim = {
    schema: 1,
    place,
    slot: SLOT,
    drops: mapping.slots[SLOT].drops.map((d) => ({ place: d.place, building: d.building })),
    kit: SLOT,
    size: "small",
    layer: `${LAYER_DIR}/${layerName}`,
    kinds: [],
    rows: planned.out.length,
    date: new Date().toISOString().slice(0, 10),
  };
  const text = planned.out.map((p) => entityText(p.name, p.spot)).join("");
  if (flags.has("--dry")) {
    console.log(text);
    console.log(`SUMMARY town-parked-cars dry ${place}: ${planned.out.length} marker(s)`);
    return 0;
  }
  const layerFile = join(root, LAYER_DIR, layerName);
  const claimFile = join(root, CLAIMS_DIR, claimNameOf(place, SLOT));
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
