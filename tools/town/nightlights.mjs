// tools/town/nightlights.mjs - `node tools/town/nightlights.mjs <place> [--check]`: the island night-light layer of a place (#3534).
//
// Reads  docs/everon/places/<place>/purpose.json (one row per enterable building) and the place's dossiers (<place>, <place>-districts,
//        <place>-dock): footprint corners, floors, ground height, roads and the vanilla street lamps already standing.
// Writes addons/LF_Everon1989/Worlds/MainWorld/MainWorld_Layers/Lights_<place>.layer:
//   window glows  one or two LF_WindowGlow.et (#3390) on the street-facing wall of every building somebody uses: a fitted business or civic
//                 building (status exists/flight), a resident's home (purpose c), the open church (card D1). Never a for-sale house, a
//                 locked shop, a shed or a garage. The light is a point light with no shadow; hours come on at 18 and go off 22 to 02, so
//                 the town does not go dark all at once.
//   lamp heads    LF_LampHeadNight.et on top of a vanilla `_off` street lamp (a dead post), and vanilla LampStreet_E_01_4m.et (it switches
//                 itself on at dusk: the 22:00 look of #3534) in the biggest gaps of the vanilla lamps beside the square and main street.
// A generated file: rerun the script, never hand-edit. `--check` writes nothing and exits 1 when the file on disk differs.
// Core names no coordinates: this is pack data (core-vs-pack.md "Map independence").
import { existsSync, mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";
import { measureTown } from "../../agent-box/place-audit.mjs";
import { frameOf, gameVfsSync } from "../lib/height-frame.mjs";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");
export const LAYER_DIR = "addons/LF_Everon1989/Worlds/MainWorld/MainWorld_Layers";
export const GLOW = "{E24B79E78F3A5685}Prefabs/Props/Lights/LF_WindowGlow.et";
export const GLOW_COMPONENT = "{56B3030CD27BB27B}"; // LF_NightLightComponent in LF_WindowGlow.et: overridden by GUID
export const HEAD = "{E8943F7063E1262D}Prefabs/Props/Lights/LF_LampHeadNight.et";
export const HEAD_COMPONENT = "{9EA0FADFE29DA81E}"; // LF_NightLightComponent in LF_LampHeadNight.et
export const LAMP = "{E021E2FFF02ADF0C}Prefabs/Structures/Infrastructure/Lamps/LampStreet_E_01/LampStreet_E_01_4m.et";
const SURVEYS = { sp: "", di: "-districts", dk: "-dock" };

// Lamps are looked for around the arrival square and along the main street.
export const SQUARE = [4437, 10670];
export const LAMP_REACH = 95; // metres from the square centre
export const LAMP_COUNT = 10; // new self-lit lamps; the dead posts come on top
export const LAMP_GAP = 12; // a new lamp stands this far from every lamp already there, and from each other
export const MAX_REJECTS = 4; // probe rounds that may reject lamps
export const REJECT_RADIUS = 3; // a candidate this close to a spot the probe rejected is skipped
export const LAMP_OFFSET = 5.2; // metres from the road centre line to the post
export const HEAD_HEIGHT = 4.05; // the lamp head on a 4 m post; a taller post (E_03) is left alone
export const WALL_OFFSET = 0.35; // a glow stands this far outside the wall plane (the light has no shadow: it lights both sides)
export const FLOOR_Y = [2.1, 5.3]; // window height above the ground, ground floor and first floor

// Fronts the night sweep found black (docs/playtest/visual-sweep-night-2026-11-25.md, card C): a player at 22:00 cannot find the
// door or the stop. Each gets one vanilla self-lit street lamp beside its sign, on the pavement side of the player's approach.
//   sign    the fascia, plate or stop the lamp is for [x, z]        from  where a player stands to read it [x, z] (the sweep camera)
//   ground  the ground height at `from` (the sweep's `probe ground`): the probe ray's start, the seat settles the real height
export const FRONTS = [
  { id: "bank", sign: [4372.58, 10649.7], from: [4376.2, 10643.7], ground: 11.9 },
  { id: "pharmacy", sign: [4437.82, 10847.02], from: [4443.1, 10851.6], ground: 9.1 },
  { id: "furniture", sign: [4505.21, 10654.13], from: [4504.4, 10663.1], ground: 15.8 },
  { id: "garage", sign: [4700.3, 10756.41], from: [4706.9, 10758.7], ground: 21 },
  { id: "carlot", sign: [4396.5, 10624.6], from: [4401.5, 10619.7], ground: 15.7, gap: 5, road: 3 },
  { id: "busstop_b32", sign: [4550.26, 10752.13], from: [4554.3, 10752.1], ground: 17.3 },
  // Night re-check 2026-11-25: the cafe door (its fascia is `cafe-fascia` in the cafe kit, on the west wall) and the gang registry cabin
  // (its plate is on the south wall, the door on the east end: the lamp stands off the south side, clear of the door's swing).
  { id: "cafe", sign: [4447.15, 10701.36], from: [4439.5, 10701.4], ground: 12.5 },
  { id: "registry", sign: [4503.49, 10621.95], from: [4503.5, 10615.5], ground: 16 },
];
export const FRONT_OUT = 3.5; // the lamp stands this far out from the sign toward the player ...
export const FRONT_SIDE = 3; // ... and this far to one side of the sign's axis (never in front of the door)
export const FRONT_SEARCH = 4; // candidate spots within this of that point, every metre
export const FRONT_LAMP_GAP = 7; // from every other lamp: a front lamp lights a dark front, it does not double a lit one (a front may set `gap`)
export const FRONT_ROAD = 3.6; // metres from a road centre line: off the carriageway (a front may set `road`)
export const FRONT_GROW = 1.2; // a building footprint grown this much is no place for a post

const SKIP_STEM =/^(Shed|Chapel|NewspaperBooth|PortableCabin|FuelStation_E_01_building_small)/;
const r2 = (n) => Math.round(n * 100) / 100;

// A tiny deterministic hash in [0, 1) so the layer is the same on every run.
export function unit(text, salt = 0) {
  let h = 2166136261 ^ salt;
  for (const c of String(text)) h = Math.imul(h ^ c.charCodeAt(0), 16777619);
  return ((h >>> 0) % 10000) / 10000;
}

// Is somebody using this building at night? Not a for-sale house (purpose a), a locked planned shop, a shed or a garage (e).
export function occupied(row) {
  if (row.partOf) return false;
  if (row.status === "exists" || row.status === "flight") return true;
  return row.card === "D1" || row.purpose === "c";
}

export function loadPlace(root, place) {
  const purposeFile = join(root, "docs/everon/places", place, "purpose.json");
  if (!existsSync(purposeFile)) throw new Error(`no docs/everon/places/${place}/purpose.json`);
  const purpose = JSON.parse(readFileSync(purposeFile, "utf8"));
  const buildings = new Map();
  const roads = [];
  const lamps = [];
  const props = [];
  for (const [prefix, suffix] of Object.entries(SURVEYS)) {
    const f = join(root, "docs/everon/places", `${place}${suffix}`, "dossier.json");
    if (!existsSync(f)) continue;
    const d = JSON.parse(readFileSync(f, "utf8"));
    for (const b of d.buildings ?? []) buildings.set(`${prefix}:${b.id}`, b);
    if (prefix === "sp") for (const r of d.roads ?? []) roads.push(r.points);
    for (const p of d.props ?? []) {
      props.push(p);
      if (p.kind === "street-lamp") lamps.push(p);
    }
  }
  return { purpose, buildings, roads, lamps, props };
}

const dist = (a, b) => Math.hypot(a[0] - b[0], a[1] - b[1]);

// Nearest point of a set of polylines to p: { at: [x, z], d }.
export function nearestRoad(roads, p) {
  let best = { at: null, d: Infinity };
  for (const line of roads) {
    for (let i = 0; i + 1 < line.length; i++) {
      const a = line[i];
      const b = line[i + 1];
      const dx = b[0] - a[0];
      const dz = b[1] - a[1];
      const len2 = dx * dx + dz * dz;
      const t = len2 ? Math.max(0, Math.min(1, ((p[0] - a[0]) * dx + (p[1] - a[1]) * dz) / len2)) : 0;
      const at = [a[0] + t * dx, a[1] + t * dz];
      const d = dist(at, p);
      if (d < best.d) best = { at, d };
    }
  }
  return best;
}

// The wall of a footprint that looks at the nearest road: { mid, tan, normal, len }.
export function streetWall(corners, centre, roadAt) {
  const toRoad = [roadAt[0] - centre[0], roadAt[1] - centre[1]];
  const tl = Math.hypot(toRoad[0], toRoad[1]) || 1;
  let best = null;
  for (let i = 0; i < 4; i++) {
    const a = corners[i];
    const b = corners[(i + 1) % 4];
    const len = dist(a, b);
    if (!len) continue;
    const tan = [(b[0] - a[0]) / len, (b[1] - a[1]) / len];
    let normal = [tan[1], -tan[0]];
    const mid = [(a[0] + b[0]) / 2, (a[1] + b[1]) / 2];
    if ((mid[0] - centre[0]) * normal[0] + (mid[1] - centre[1]) * normal[1] < 0) normal = [-normal[0], -normal[1]];
    const score = (normal[0] * toRoad[0] + normal[1] * toRoad[1]) / tl;
    if (!best || score > best.score) best = { a, mid, tan, normal, len, score };
  }
  return best;
}

// How many glows a building gets: a big town house or a pub two or three, one for a plain or low one.
export function glowCount(b) {
  const w = b.footprint?.w ?? 0;
  const d = b.footprint?.d ?? 0;
  if ((b.floors ?? 1) >= 3 && Math.min(w, d) >= 16) return 3;
  if ((b.floors ?? 1) >= 2 && Math.min(w, d) >= 10) return 2;
  return 1;
}

// The hour a window goes dark: a home late, a shop at 23, so the square is never switched off in one blink.
export function hourOff(row, id) {
  if (row.purpose === "c") return [23, 0, 1, 2][Math.floor(unit(id, 3) * 4)];
  if (row.status === "flight" && /hostel/i.test(row.use ?? "")) return 2;
  if (/pub|caf/i.test(row.use ?? "")) return 1;
  return 23;
}

export function buildGlows({ purpose, buildings, roads }) {
  const glows = [];
  const errors = [];
  for (const r of purpose.buildings ?? []) {
    if (!occupied(r)) continue;
    const b = buildings.get(r.id);
    if (!b) {
      errors.push(`${r.id}: no building in the dossier`);
      continue;
    }
    if (SKIP_STEM.test(b.stem ?? "")) continue;
    const fp = b.footprint ?? {};
    if (!Array.isArray(fp.corners) || fp.corners.length !== 4 || ![b.y, fp.cx, fp.cz].every(Number.isFinite)) {
      errors.push(`${r.id}: no footprint corners or ground height`);
      continue;
    }
    const centre = [fp.cx, fp.cz];
    const road = nearestRoad(roads, centre);
    if (!road.at) {
      errors.push(`${r.id}: no road to face`);
      continue;
    }
    const wall = streetWall(fp.corners, centre, road.at);
    if (!wall) {
      errors.push(`${r.id}: no wall`);
      continue;
    }
    const n = glowCount(b);
    const off = hourOff(r, r.id);
    for (let i = 0; i < n; i++) {
      // Spread along the wall, the second row a floor up; a little jitter so two houses do not match.
      const t = (n === 1 ? 0.3 : 0.2 + (0.6 * i) / (n - 1)) + (unit(r.id, i) - 0.5) * 0.12;
      const along = (t - 0.5) * wall.len;
      const x = wall.mid[0] + wall.tan[0] * along + wall.normal[0] * WALL_OFFSET;
      const z = wall.mid[1] + wall.tan[1] * along + wall.normal[1] * WALL_OFFSET;
      const y = b.y + FLOOR_Y[i % 2 === 0 ? 0 : 1];
      glows.push({ name: `Glow_${r.id.replace(":", "_")}_${i}`, id: r.id, x: r2(x), y: r2(y), z: r2(z), off, normal: wall.normal, floor: FLOOR_Y[i % 2 === 0 ? 0 : 1] });
    }
  }
  return { glows, errors };
}

// Heights near a point from the props and buildings the dossier measured: a ground estimate for a lamp in a gap.
export function groundNear(p, props) {
  const near = props
    .filter((q) => Number.isFinite(q.y))
    .map((q) => ({ d: dist([q.x, q.z], p), y: q.y }))
    .filter((q) => q.d <= 14)
    .sort((a, b) => a.d - b.d)
    .slice(0, 3);
  // Two or three measured props close by that agree within 90 cm, else the ground here is a guess and no lamp stands on it.
  if (near.length < 2 || Math.max(...near.map((q) => q.y)) - Math.min(...near.map((q) => q.y)) > 0.9) return null;
  let sw = 0;
  let sy = 0;
  for (const q of near) {
    const w = 1 / Math.max(q.d, 1);
    sw += w;
    sy += w * q.y;
  }
  return sy / sw;
}

const inPoly = (p, poly) => {
  let inside = false;
  for (let i = 0, j = poly.length - 1; i < poly.length; j = i++) {
    const [xi, zi] = poly[i];
    const [xj, zj] = poly[j];
    if (zi > p[1] !== zj > p[1] && p[0] < ((xj - xi) * (p[1] - zi)) / (zj - zi) + xi) inside = !inside;
  }
  return inside;
};

// A footprint polygon grown by `m` metres from its centre.
const grow = (corners, c, m) =>
  corners.map((q) => {
    const d = dist(q, c) || 1;
    return [q[0] + ((q[0] - c[0]) / d) * m, q[1] + ((q[1] - c[1]) / d) * m];
  });

export function buildLamps({ buildings, roads, lamps, props }, rejected = []) {
  const errors = [];
  const out = [];
  // Existing lights a new post must keep away from.
  const taken = lamps.map((l) => [l.x, l.z]);
  const polys = [...buildings.values()].filter((b) => b.footprint?.corners?.length === 4).map((b) => grow(b.footprint.corners, [b.footprint.cx, b.footprint.cz], 2.5));
  // Candidates: both sides of every road piece near the square, every 3 m.
  const cands = [];
  for (const line of roads) {
    for (let i = 0; i + 1 < line.length; i++) {
      const a = line[i];
      const b = line[i + 1];
      const len = dist(a, b);
      if (len < 1) continue;
      const tan = [(b[0] - a[0]) / len, (b[1] - a[1]) / len];
      for (let s = 0; s <= len; s += 3) {
        const c = [a[0] + tan[0] * s, a[1] + tan[1] * s];
        if (dist(c, SQUARE) > LAMP_REACH) continue;
        for (const side of [1, -1]) {
          const p = [c[0] - tan[1] * side * LAMP_OFFSET, c[1] + tan[0] * side * LAMP_OFFSET];
          if (dist(p, SQUARE) > LAMP_REACH) continue;
          if (nearestRoad(roads, p).d < LAMP_OFFSET - 1.2) continue; // another carriageway
          if (polys.some((q) => inPoly(p, q))) continue;
          if (props.some((q) => dist([q.x, q.z], p) < 2.2)) continue;
          if (rejected.some((q) => dist(q, p) < REJECT_RADIUS)) continue; // the island probe found this spot blocked or sloped (`--probe`)
          const y = groundNear(p, props);
          if (y === null) continue;
          cands.push({ p, face: c, y });
        }
      }
    }
  }
  // Greedy: always the candidate farthest from every light so far, until the count or no gap is left.
  for (let k = 0; k < LAMP_COUNT; k++) {
    let best = null;
    for (const c of cands) {
      const d = Math.min(...taken.map((t) => dist(t, c.p)), 999);
      if (d >= LAMP_GAP && (!best || d > best.d)) best = { ...c, d };
    }
    if (!best) break;
    taken.push(best.p);
    const y = best.y;
    const yaw = (Math.atan2(best.face[0] - best.p[0], best.face[1] - best.p[1]) * 180) / Math.PI;
    out.push({ name: `Lamp_${out.length + 1}`, x: r2(best.p[0]), y: r2(y), z: r2(best.p[1]), yaw: r2(yaw) });
  }
  // The dark fronts, after the gap lamps so those keep their places.
  const fpolys = [...buildings.values()].filter((b) => b.footprint?.corners?.length === 4).map((b) => grow(b.footprint.corners, [b.footprint.cx, b.footprint.cz], FRONT_GROW));
  for (const f of FRONTS) {
    const d = dist(f.from, f.sign) || 1;
    const u = [(f.from[0] - f.sign[0]) / d, (f.from[1] - f.sign[1]) / d];
    let best = null;
    for (const side of [1, -1]) {
      const want = [f.sign[0] + u[0] * FRONT_OUT - u[1] * side * FRONT_SIDE, f.sign[1] + u[1] * FRONT_OUT + u[0] * side * FRONT_SIDE];
      for (let dx = -FRONT_SEARCH; dx <= FRONT_SEARCH; dx++) {
        for (let dz = -FRONT_SEARCH; dz <= FRONT_SEARCH; dz++) {
          const p = [want[0] + dx, want[1] + dz];
          const score = dist(p, want);
          if (score > FRONT_SEARCH || (best && score >= best.score)) continue;
          const along = (p[0] - f.sign[0]) * u[0] + (p[1] - f.sign[1]) * u[1]; // out from the sign
          const lat = Math.abs(-(p[0] - f.sign[0]) * u[1] + (p[1] - f.sign[1]) * u[0]); // off the sign's axis
          if (along < 1.5 || lat < 1.8) continue;
          if (taken.some((t) => dist(t, p) < (f.gap ?? FRONT_LAMP_GAP))) continue;
          if (fpolys.some((q) => inPoly(p, q))) continue;
          if (props.some((q) => dist([q.x, q.z], p) < 1.8)) continue;
          if (nearestRoad(roads, p).d < (f.road ?? FRONT_ROAD)) continue;
          if (rejected.some((q) => dist(q, p) < REJECT_RADIUS)) continue;
          best = { p, score };
        }
      }
    }
    if (!best) {
      errors.push(`${f.id}: no clear spot for a front lamp near ${f.sign.join(", ")}`);
      continue;
    }
    taken.push(best.p);
    const yaw = (Math.atan2(f.sign[0] - best.p[0], f.sign[1] - best.p[1]) * 180) / Math.PI;
    out.push({ name: `Lamp_front_${f.id}`, x: r2(best.p[0]), y: f.ground, z: r2(best.p[1]), yaw: r2(yaw), front: true });
  }
  return { lamps: out, errors };
}

// A dead 4 m post near the town gets a lamp head on top (E_03 posts stand 11 m: left alone).
export function buildHeads({ props }) {
  return props
    .filter((p) => p.kind === "street-lamp" && /^LampStreet_E_01_4m_off$/.test(p.stem))
    .map((p, i) => ({ name: `LampHead_${i + 1}`, x: r2(p.x), y: r2(p.y + HEAD_HEIGHT), z: r2(p.z) }));
}

// ------------------------------------------------------------------ seats (placement-system.md: the world answers how high)
//
// The survey's one height per building put 31 of 38 glows more than 1.5 m from any wall and 8 of 10 new lamps off the street
// (audit placement --world MainWorld, 2026-11-19). `--probe` asks the island: one ray per glow from 2.5 m out through the
// footprint wall (the real wall it hits, and the terrain under that point), one ray down per new lamp (the pavement under it).
// The answers go in tools/town/probes/lights-<place>.json, which every later run (and --check) reads, so the layer stays
// reproducible without a server. A glow whose ray meets no wall is not written: a light is attached to something or absent.
export const SEATS_DIR = "tools/town/probes";
export const seatsPath = (root, place) => join(root, SEATS_DIR, `lights-${place}.json`);
const GLOW_OUT = 1.2; // the wall ray starts this far outside the footprint wall (2.5 m started behind a street prop: glow b12_0 was seated on a post in the road, #3888)
const GLOW_IN = 1.5; // and ends this far inside it
const LAMP_UP = 3;
const LAMP_DOWN = 4;
export const RING_RADIUS = 1; // a new lamp's ground is probed on a ring this wide round its post
export const RING_POINTS = 8;
export const LEVEL_MAX = 0.45; // the ground under the post and its ring may differ by this much
// A rock or a cliff within the ring blocks a lamp: lamp 10 stood beside a granite rock the centre ray missed and read 36 cm over it
// (#3938). Kerbs, walls and bushes beside a street post are normal and do not block.
export const BLOCK_ROOT = /(Cliff|Rock|Boulder)/i;

// The probe rays (LF_PlacementAudit.RayLine input lines) for the unseated glows and lamps.
export function probeRays({ glows, lamps }) {
  const rays = [];
  for (const g of glows) {
    const [nx, nz] = g.normal;
    const out = GLOW_OUT - WALL_OFFSET;
    const inn = GLOW_IN + WALL_OFFSET;
    rays.push(`ray ${g.name} ${r2(g.x + nx * out)} ${g.y} ${r2(g.z + nz * out)} ${r2(g.x - nx * inn)} ${g.y} ${r2(g.z - nz * inn)}`);
  }
  for (const l of lamps) rays.push(`ray ${l.name} ${l.x} ${r2(l.y + LAMP_UP)} ${l.z} ${l.x} ${r2(l.y - LAMP_DOWN)} ${l.z}`);
  return rays;
}

// The ring rays round each new lamp: `<lamp>_r<i>`, same reach as the centre ray.
export function ringRays(lamps) {
  const rays = [];
  for (const l of lamps) {
    for (let i = 0; i < RING_POINTS; i++) {
      const a = (2 * Math.PI * i) / RING_POINTS;
      const x = r2(l.x + Math.sin(a) * RING_RADIUS);
      const z = r2(l.z + Math.cos(a) * RING_RADIUS);
      rays.push(`ray ${l.name}_r${i} ${x} ${r2(l.y + LAMP_UP)} ${z} ${x} ${r2(l.y - LAMP_DOWN)} ${z}`);
    }
  }
  return rays;
}

// Which lamps stand on clear, level ground: the centre ray's seat and the ring's hits (from the same out.txt). A ring hit on a
// rock or a cliff, or a height spread over LEVEL_MAX, or a post that would stand on top of something (a wall, a stall) rather than
// the ground, blocks the lamp. Returns { name: reason } for the blocked ones. `only`: the lamps this run moved, the only ones also judged for standing on top of something.
export function blockedLamps(outText, lamps, only = null) {
  const byId = new Map();
  for (const line of outText.split(/\r?\n/)) {
    if (!line.startsWith("ray ")) continue;
    const rest = line.slice(4);
    const sp = rest.indexOf(" ");
    byId.set(rest.slice(0, sp), kvOf(rest.slice(sp + 1)));
  }
  const blocked = {};
  for (const l of lamps) {
    const heights = [];
    const mid = byId.get(l.name);
    if (only?.has(l.name) && mid && Number(mid.d) < Number(mid.len) - 0.01 && mid.hit !== "none" && !/^(Eden|RoadEntity|Asphalt|Pavement)/i.test(String(mid.root)) && Number(mid.p.split(",")[1]) - Number(mid.sy) > 0.3) blocked[l.name] = `stands on ${mid.root}`;
    const names = [l.name, ...Array.from({ length: RING_POINTS }, (_, i) => `${l.name}_r${i}`)];
    for (const n of names) {
      const r = byId.get(n);
      if (!r) continue;
      const hit = Number(r.d) < Number(r.len) - 0.01 && r.hit !== "none";
      if (hit && BLOCK_ROOT.test(String(r.root))) blocked[l.name] = `${r.root} within ${RING_RADIUS} m`;
      if (Number.isFinite(Number(r.sy))) heights.push(Number(r.sy)); // the terrain under the ray, whatever stands on it
    }
    if (!blocked[l.name] && heights.length && Math.max(...heights) - Math.min(...heights) > LEVEL_MAX) blocked[l.name] = `ground varies ${r2(Math.max(...heights) - Math.min(...heights))} m`;
  }
  return blocked;
}

const kvOf = (rest) => Object.fromEntries(rest.trim().split(/\s+/).map((t) => [t.slice(0, t.indexOf("=")), t.slice(t.indexOf("=") + 1)]).filter(([k]) => k));

// The seats from a probe run's out.txt: { glows: { name: { x, y, z, on } | null }, lamps: { name: { y, on } } }.
export function seatsFromRays(outText, { glows, lamps }) {
  const byId = new Map();
  for (const line of outText.split(/\r?\n/)) {
    if (!line.startsWith("ray ")) continue;
    const rest = line.slice(4);
    const sp = rest.indexOf(" ");
    byId.set(rest.slice(0, sp), kvOf(rest.slice(sp + 1)));
  }
  const seats = { glows: {}, lamps: {} };
  for (const g of glows) {
    const r = byId.get(g.name);
    if (!r) continue;
    const hit = Number(r.d) < Number(r.len) - 0.01 && r.hit !== "none";
    if (!hit) {
      seats.glows[g.name] = null;
      continue;
    }
    const [px, , pz] = r.p.split(",").map(Number);
    seats.glows[g.name] = { x: r2(px + g.normal[0] * WALL_OFFSET), y: r2(Number(r.sy) + g.floor), z: r2(pz + g.normal[1] * WALL_OFFSET), on: r.root };
  }
  for (const l of lamps) {
    const r = byId.get(l.name);
    if (!r) continue;
    const py = Number(r.p.split(",")[1]);
    const hit = Number(r.d) < Number(r.len) - 0.01 && r.hit !== "none";
    seats.lamps[l.name] = { y: r2(hit ? Math.max(py, Number(r.sy)) : Number(r.sy)), ty: r2(Number(r.sy)), on: hit ? r.root : "terrain" };
  }
  return seats;
}

// The glows and lamps moved onto their seats; a glow seated on nothing is dropped. Without seats, as generated.
export function applySeats(built, seats) {
  if (!seats) return built;
  const glows = [];
  for (const g of built.glows) {
    const s = seats.glows?.[g.name];
    if (s === null) continue;
    glows.push(s ? { ...g, x: s.x, y: s.y, z: s.z } : g);
  }
  const lamps = built.lamps.map((l) => (seats.lamps?.[l.name] ? { ...l, y: seats.lamps[l.name].y, ...(Number.isFinite(seats.lamps[l.name].ty) ? { ty: seats.lamps[l.name].ty } : {}) } : l));
  return { ...built, glows, lamps };
}

export function buildAll(loaded, seats = null) {
  const g = buildGlows(loaded);
  const l = buildLamps(loaded, seats?.rejected ?? []);
  return applySeats({ glows: g.glows, lamps: l.lamps, heads: buildHeads(loaded), errors: [...g.errors, ...l.errors] }, seats);
}

// Every row in the frame its prefab reads y in (tools/lib/height-frame.mjs): the seats are world heights, and a prefab whose
// flags carry RELATIVE_Y (the vanilla street lamp: Lamps_Base 0x80403) reads y as the height over the terrain, so it is written
// as its seat minus the probed ground under it (`ty`). Without that ground height the row cannot be written right: an error.
// The ten new lamps stood 9 to 16 m in the air when their world seat went in as written (#3938).
export function frameRows(built, frameOfRef = (ref) => frameOf(ROOT, gameVfsSync(), ref)) {
  const errors = [];
  const conv = (list, ref) => {
    if (frameOfRef(ref) !== "terrain") return list;
    return list.map((l) => {
      if (Number.isFinite(l.ty)) return { ...l, y: r2(l.y - l.ty) };
      errors.push(`${l.name}: ${ref.split("/").pop()} reads y over the terrain and its seat keeps no ground height (rerun with --probe)`);
      return l;
    });
  };
  return { ...built, glows: conv(built.glows, GLOW), heads: conv(built.heads, HEAD), lamps: conv(built.lamps, LAMP), errors: [...(built.errors ?? []), ...errors] };
}

export function renderLayer(place, { glows, lamps, heads }) {
  const out = [];
  const slug = place.replace(/-/g, "_");
  for (const g of glows) {
    out.push(
      `GenericEntity ${g.name.replace(/^Glow_/, `Glow_${slug}_`)} : "${GLOW}" {`,
      " components {",
      `  LF_NightLightComponent "${GLOW_COMPONENT}" {`,
      `   m_fHourOff ${g.off}`,
      "  }",
      " }",
      ` coords ${g.x} ${g.y} ${g.z}`,
      "}",
    );
  }
  for (const h of heads) {
    out.push(`GenericEntity ${h.name.replace(/^LampHead_/, `LampHead_${slug}_`)} : "${HEAD}" {`, ` coords ${h.x} ${h.y} ${h.z}`, "}");
  }
  for (const l of lamps) {
    out.push(`GenericEntity ${l.name.replace(/^Lamp_/, `Lamp_${slug}_`)} : "${LAMP}" {`, ` coords ${l.x} ${l.y} ${l.z}`, ` angleY ${l.yaw}`, "}");
  }
  return out.join("\n") + "\n";
}

export const layerPath = (root, place) => join(root, LAYER_DIR, `Lights_${place}.layer`);

export function main(args, { root = ROOT } = {}) {
  const check = args.includes("--check");
  const place = args.find((a) => !a.startsWith("--"));
  if (!place || !/^[a-z0-9][a-z0-9-]*$/.test(place)) {
    console.log("usage: node tools/town/nightlights.mjs <place> [--check] [--probe [--reseat Glow_a,Glow_b]]");
    return 1;
  }
  let loaded;
  try {
    loaded = loadPlace(root, place);
  } catch (e) {
    console.log(`nightlights: ${e.message}`);
    return 1;
  }
  const seatsFile = seatsPath(root, place);
  if (args.includes("--probe")) {
    // A re-probe changes only what moved: every committed glow, and every lamp that stands where the committed seats put it, keeps its
    // seat (the world has changed since: re-seating them all would shift lights nobody asked to move). A lamp with a
    // rock or cliff within a metre or a steep slope, or a moved one on top of a wall or stall, is rejected and the next best gap is
    // probed instead (at most MAX_REJECTS rounds).
    const outDir = join(root, "tmp", "nightlights-probe");
    const old = existsSync(seatsFile) ? JSON.parse(readFileSync(seatsFile, "utf8")) : null;
    const rejected = [...(old?.rejected ?? [])];
    const before = buildAll(loaded, { rejected: [...rejected] }).lamps;
    let raw;
    let seats;
    for (let round = 0; ; round++) {
      raw = buildAll(loaded, { rejected });
      const fresh = new Set(raw.lamps.filter((l) => !old?.lamps?.[l.name] || !before.some((q) => q.name === l.name && q.x === l.x && q.z === l.z)).map((l) => l.name));
      const run = measureTown([], { outDir, extra: [...probeRays(raw), ...ringRays(raw.lamps)], island: true });
      if (run.outText == null) {
        console.log("SUMMARY nightlights fail: the island probe wrote no out.txt (lines above)");
        return 1;
      }
      seats = seatsFromRays(run.outText, raw);
      const blocked = blockedLamps(run.outText, raw.lamps, fresh);
      for (const [name, why] of Object.entries(blocked)) console.log(`nightlights: ${name} rejected (${why})`);
      if (!Object.keys(blocked).length || round >= MAX_REJECTS) {
        if (Object.keys(blocked).length) console.log("nightlights: still blocked after the last round: " + Object.keys(blocked).join(" "));
        for (const l of raw.lamps) if (!fresh.has(l.name) && old?.lamps?.[l.name]) seats.lamps[l.name] = old.lamps[l.name];
        break;
      }
      for (const name of Object.keys(blocked)) {
        const l = raw.lamps.find((q) => q.name === name);
        rejected.push([l.x, l.z]);
      }
    }
    seats.rejected = rejected;
    // `--reseat a,b`: the named glows take this probe's seat instead of the committed one (a world that moved under them).
    const reseat = new Set(String(args[args.indexOf("--reseat") + 1] ?? "").split(",").filter((n) => n && !n.startsWith("--")));
    if (old?.glows) seats.glows = { ...old.glows, ...Object.fromEntries(Object.entries(seats.glows).filter(([n]) => reseat.has(n))) };
    mkdirSync(dirname(seatsFile), { recursive: true });
    writeFileSync(seatsFile, JSON.stringify({ place, at: new Date().toISOString().slice(0, 10), ...seats }, null, 1) + "\n");
    const dropped = Object.values(seats.glows).filter((s) => s === null).length;
    console.log(`nightlights: probed ${Object.keys(seats.glows).length} glows (${dropped} meet no wall: not written) and ${Object.keys(seats.lamps).length} lamps into ${SEATS_DIR}/lights-${place}.json`);
  }
  const built = frameRows(buildAll(loaded, existsSync(seatsFile) ? JSON.parse(readFileSync(seatsFile, "utf8")) : null));
  for (const e of built.errors) console.log(`ERROR ${e}`);
  const text = renderLayer(place, built);
  const file = layerPath(root, place);
  const summary = `glows=${built.glows.length} lamps=${built.lamps.length} heads=${built.heads.length}`;
  if (check) {
    const same = existsSync(file) && readFileSync(file, "utf8") === text;
    console.log(`SUMMARY nightlights ${same && !built.errors.length ? "ok" : "fail"} place=${place} ${summary}${same ? "" : ` (layer on disk differs: rerun node tools/town/nightlights.mjs ${place})`}`);
    return same && !built.errors.length ? 0 : 1;
  }
  writeFileSync(file, text);
  console.log(`SUMMARY nightlights ${built.errors.length ? "fail" : "ok"} place=${place} ${summary} wrote=${LAYER_DIR}/Lights_${place}.layer`);
  return built.errors.length ? 1 : 0;
}

if (process.argv[1] && pathToFileURL(resolve(process.argv[1])).href === import.meta.url) process.exit(main(process.argv.slice(2)));
