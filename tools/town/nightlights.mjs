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
import { existsSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";

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
export const LAMP_OFFSET = 5.2; // metres from the road centre line to the post
export const HEAD_HEIGHT = 4.05; // the lamp head on a 4 m post; a taller post (E_03) is left alone
export const WALL_OFFSET = 0.35; // a glow stands this far outside the wall plane (the light has no shadow: it lights both sides)
export const FLOOR_Y = [2.1, 5.3]; // window height above the ground, ground floor and first floor

const SKIP_STEM = /^(Shed|Chapel|NewspaperBooth|PortableCabin|FuelStation_E_01_building_small)/;
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
      glows.push({ name: `Glow_${r.id.replace(":", "_")}_${i}`, id: r.id, x: r2(x), y: r2(y), z: r2(z), off });
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

export function buildLamps({ buildings, roads, lamps, props }) {
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
  return { lamps: out, errors };
}

// A dead 4 m post near the town gets a lamp head on top (E_03 posts stand 11 m: left alone).
export function buildHeads({ props }) {
  return props
    .filter((p) => p.kind === "street-lamp" && /^LampStreet_E_01_4m_off$/.test(p.stem))
    .map((p, i) => ({ name: `LampHead_${i + 1}`, x: r2(p.x), y: r2(p.y + HEAD_HEIGHT), z: r2(p.z) }));
}

export function buildAll(loaded) {
  const g = buildGlows(loaded);
  const l = buildLamps(loaded);
  return { glows: g.glows, lamps: l.lamps, heads: buildHeads(loaded), errors: [...g.errors, ...l.errors] };
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
    console.log("usage: node tools/town/nightlights.mjs <place> [--check]");
    return 1;
  }
  let loaded;
  try {
    loaded = loadPlace(root, place);
  } catch (e) {
    console.log(`nightlights: ${e.message}`);
    return 1;
  }
  const built = buildAll(loaded);
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
