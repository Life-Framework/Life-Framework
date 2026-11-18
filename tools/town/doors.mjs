// tools/town/doors.mjs - `tools\cli town doors <place>`: every enterable building has a purpose and something in the world that carries it (#3202).
//
//   town doors <place> [--planned] [--json] [--root <dir>]
//
// Reads  docs/everon/places/<place>/purpose.json  (the allocation: one row per enterable building, keyed `sp:b01` / `di:b04` / `dk:b02`),
//        the place's dossiers (<place>, <place>-districts, <place>-dock: docs/everon/places/<dir>/dossier.json),
//        addons/LF_Everon1989/Worlds/MainWorld/MainWorld_Layers/*.layer.
// An enterable building is a dossier building with a door entity (`doors` > 0) and an interior that is not "no". The centre survey and the
// district survey overlap; a building the later survey sees at the same spot (its centre inside the earlier footprint, or within 6 m of the
// earlier centre) is the same building and counts once, under the earlier survey's prefix.
// A building carries its purpose when a world marker stands inside its footprint (corners) plus 3 m (the slack LF_FittedBuildings.BOUNDS_SLACK
// uses). Carriers: a fitted place marker (LF_FittedBuildingComponent), a house marker (LF_HouseManagerComponent, LF_LockComponent or a
// LF_PlotComponent sign: house for sale, resident home) or a lock marker (LF_BuildingLockComponent). One marker serves the building it is
// nearest to, never two.
// A row with `partOf: "<id>"` (a side shed or workshop of a fitted slot) is carried by that building's marker.
// Errors (exit 1): an enterable building with no purpose row; a purpose row with no carrier; a row whose building is not in the dossiers.
// --planned turns a missing carrier into a warning for a row whose status is planned or in flight (a card has not built it yet); a row marked
// `exists` still needs its carrier. A building with no purpose row is an error either way.
import { existsSync, readdirSync, readFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";

export const help = "doors <place> [--planned] [--json]   every enterable building has a purpose row and a carrier (fitted, house or lock marker) in the MainWorld layers; exit 1 otherwise";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");
export const LAYER_DIR = "addons/LF_Everon1989/Worlds/MainWorld/MainWorld_Layers";
export const PLACES_DIR = "docs/everon/places";
export const SLACK = 3; // metres around a dossier footprint (LF_FittedBuildings.BOUNDS_SLACK)
export const SAME_SPOT = 6; // metres between the centres of two surveys' records of one building
const SLUG_RX = /^[a-z0-9][a-z0-9-]*$/;

// Dossier folder suffix -> id prefix. The centre survey is the place folder itself and comes first.
export const SURVEYS = [
  { suffix: "", prefix: "sp" },
  { suffix: "-districts", prefix: "di" },
  { suffix: "-dock", prefix: "dk" },
];

// Component class -> carrier kind.
export const CARRIERS = {
  LF_FittedBuildingComponent: "fitted",
  LF_HouseManagerComponent: "house",
  LF_LockComponent: "house",
  LF_PlotComponent: "house",
  LF_BuildingLockComponent: "lock",
};

// ------------------------------------------------------------------ geometry
// Distance in metres from (x, z) to a polygon's area: 0 inside, else to the nearest edge.
export function distToPolygon(corners, x, z) {
  let inside = false;
  for (let i = 0, j = corners.length - 1; i < corners.length; j = i++) {
    const [xi, zi] = corners[i];
    const [xj, zj] = corners[j];
    if (zi > z !== zj > z && x < ((xj - xi) * (z - zi)) / (zj - zi) + xi) inside = !inside;
  }
  if (inside) return 0;
  let best = Infinity;
  for (let i = 0; i < corners.length; i++) {
    const [x1, z1] = corners[i];
    const [x2, z2] = corners[(i + 1) % corners.length];
    const dx = x2 - x1;
    const dz = z2 - z1;
    const len2 = dx * dx + dz * dz;
    const t = len2 === 0 ? 0 : Math.max(0, Math.min(1, ((x - x1) * dx + (z - z1) * dz) / len2));
    best = Math.min(best, Math.hypot(x - (x1 + t * dx), z - (z1 + t * dz)));
  }
  return best;
}

// ------------------------------------------------------------------ layers
// Top-level entities of a .layer: { name, x, z, comps: [class], line }. A component counts wherever it sits in the entity's braces.
export function layerMarkers(text, file = "") {
  const out = [];
  let depth = 0;
  let cur = null;
  const lines = String(text).split("\n");
  for (let n = 0; n < lines.length; n++) {
    const t = lines[n].trim();
    if (depth === 0 && /\{\s*$/.test(t)) {
      const m = /^[A-Za-z_][A-Za-z0-9_]*\s+([A-Za-z_][A-Za-z0-9_]*)\b/.exec(t);
      cur = { file, line: n + 1, name: m ? m[1] : t, x: null, z: null, comps: [] };
    } else if (cur) {
      if (depth === 1) {
        const c = /^coords\s+(-?[\d.]+)\s+(-?[\d.]+)\s+(-?[\d.]+)/.exec(t);
        if (c) {
          cur.x = Number(c[1]);
          cur.z = Number(c[3]);
        }
      }
      const k = /^([A-Za-z_][A-Za-z0-9_]*)\s+"\{[0-9A-Fa-f]+\}"\s*\{/.exec(t);
      if (k && CARRIERS[k[1]]) cur.comps.push(k[1]);
    }
    depth += (t.match(/\{/g) || []).length - (t.match(/\}/g) || []).length;
    if (depth <= 0 && cur) {
      if (cur.x !== null && cur.comps.length) out.push(cur);
      cur = null;
      depth = 0;
    }
  }
  return out;
}

// ------------------------------------------------------------------ buildings
const enterable = (b) => b && b.doors > 0 && b.interior?.value !== "no" && Array.isArray(b.footprint?.corners) && b.footprint.corners.length >= 3;

// surveys: [{ prefix, buildings: [dossier building] }] in priority order. -> [{ id, survey, building, stem, type, x, z, corners, doors }]
export function enterableBuildings(surveys) {
  const out = [];
  for (const s of surveys) {
    const mine = [];
    for (const b of s.buildings ?? []) {
      if (!enterable(b)) continue;
      const same = out.some((o) => distToPolygon(o.corners, b.x, b.z) === 0 || Math.hypot(o.x - b.x, o.z - b.z) < SAME_SPOT);
      if (same) continue;
      mine.push({ id: `${s.prefix}:${b.id}`, survey: s.prefix, building: b.id, stem: b.stem ?? "", type: b.type ?? "", x: b.x, z: b.z, corners: b.footprint.corners, doors: b.doors });
    }
    out.push(...mine);
  }
  return out;
}

// Each marker serves the one building it is nearest to within SLACK. -> Map<buildingId, [marker + kind]>
export function assignMarkers(buildings, markers) {
  const by = new Map();
  for (const m of markers) {
    let best = null;
    for (const b of buildings) {
      const d = distToPolygon(b.corners, m.x, m.z);
      if (d > SLACK) continue;
      const centre = Math.hypot(b.x - m.x, b.z - m.z);
      if (!best || d < best.d || (d === best.d && centre < best.centre)) best = { b, d, centre };
    }
    if (!best) continue;
    if (!by.has(best.b.id)) by.set(best.b.id, []);
    for (const comp of m.comps) by.get(best.b.id).push({ kind: CARRIERS[comp], comp, name: m.name, file: m.file, line: m.line });
  }
  return by;
}

// purpose: the parsed purpose.json; surveys: see enterableBuildings; markers: layerMarkers rows. -> { rows, errors, warnings }
export function checkDoors({ purpose, surveys, markers, planned = false }) {
  const buildings = enterableBuildings(surveys);
  const carriers = assignMarkers(buildings, markers);
  const rowOf = new Map();
  const errors = [];
  const warnings = [];
  for (const r of purpose?.buildings ?? []) {
    if (rowOf.has(r.id)) errors.push(`purpose.json: building ${r.id} appears twice`);
    rowOf.set(r.id, r);
  }
  const rows = [];
  for (const b of buildings) {
    const p = rowOf.get(b.id) ?? null;
    let found = carriers.get(b.id) ?? [];
    // A side shed or workshop of a fitted slot (`partOf` names the parent) is carried by the parent's marker.
    if (!found.length && p?.partOf) found = (carriers.get(p.partOf) ?? []).map((c) => ({ ...c, via: p.partOf }));
    const row = { id: b.id, stem: b.stem, type: b.type, purpose: p?.purpose ?? null, use: p?.use ?? null, status: p?.status ?? null, card: p?.card ?? null, carriers: found, problem: null };
    if (!p) {
      row.problem = "error";
      errors.push(`${b.id} (${b.stem}) has no purpose row in purpose.json`);
    } else if (!found.length) {
      const inFlight = p.status !== "exists";
      if (planned && inFlight) {
        row.problem = "warn";
        warnings.push(`${b.id} (${p.use}) has no carrier yet (${p.status}${p.card ? `, card ${p.card}` : ""})`);
      } else {
        row.problem = "error";
        errors.push(`${b.id} (${p.use}) has no carrier: no fitted, house or lock marker stands inside its footprint + ${SLACK} m`);
      }
    }
    rows.push(row);
  }
  for (const id of rowOf.keys()) {
    if (!buildings.some((b) => b.id === id)) errors.push(`purpose.json: ${id} is not an enterable building of the dossiers`);
  }
  return { rows, errors, warnings };
}

// ------------------------------------------------------------------ files
const readJson = (p) => JSON.parse(readFileSync(p, "utf8"));

export function loadPlace(root, place) {
  const purposePath = join(root, PLACES_DIR, place, "purpose.json");
  if (!existsSync(purposePath)) throw new Error(`no ${PLACES_DIR}/${place}/purpose.json`);
  const surveys = [];
  for (const s of SURVEYS) {
    const f = join(root, PLACES_DIR, `${place}${s.suffix}`, "dossier.json");
    if (existsSync(f)) surveys.push({ prefix: s.prefix, buildings: readJson(f).buildings ?? [] });
  }
  const layerDir = join(root, LAYER_DIR);
  const markers = [];
  if (existsSync(layerDir)) {
    for (const f of readdirSync(layerDir).filter((n) => n.endsWith(".layer")).sort()) {
      markers.push(...layerMarkers(readFileSync(join(layerDir, f), "utf8"), f));
    }
  }
  return { purpose: readJson(purposePath), surveys, markers };
}

const tag = (r) => (r.problem === "error" ? "ERROR" : r.problem === "warn" ? "WARN " : "ok   ");

export function main(args, { root = ROOT } = {}) {
  const flags = new Set(args.filter((a) => a.startsWith("--") && a !== "--root"));
  const rootAt = args.indexOf("--root");
  if (rootAt >= 0) root = resolve(args[rootAt + 1] ?? "");
  const place = args.find((a, i) => !a.startsWith("--") && args[i - 1] !== "--root");
  if (!place || !SLUG_RX.test(place)) {
    console.log("usage: tools\\cli town doors <place> [--planned] [--json]");
    return 1;
  }
  let loaded;
  try {
    loaded = loadPlace(root, place);
  } catch (e) {
    console.log(`town doors: ${e.message}`);
    return 1;
  }
  const res = checkDoors({ ...loaded, planned: flags.has("--planned") });
  const ok = res.errors.length === 0;
  if (flags.has("--json")) {
    console.log(JSON.stringify({ place, ok, buildings: res.rows.length, errors: res.errors, warnings: res.warnings, rows: res.rows }, null, 1));
    return ok ? 0 : 1;
  }
  for (const r of res.rows) {
    const carrier = r.carriers.length ? [...new Set(r.carriers.map((c) => `${c.kind}:${c.name}${c.via ? ` (via ${c.via})` : ""}`))].join(", ") : "NO CARRIER";
    console.log(`${tag(r)} ${r.id.padEnd(7)} ${String(r.purpose ?? "?").padEnd(1)} ${String(r.stem).padEnd(32)} ${r.use ?? "NO PURPOSE ROW"}  <- ${carrier}`);
  }
  for (const m of res.errors.filter((e) => !res.rows.some((r) => r.problem === "error" && e.startsWith(r.id + " ")))) console.log(`ERROR ${m}`);
  console.log(`SUMMARY town-doors ${ok ? "ok" : "fail"} place=${place} buildings=${res.rows.length} errors=${res.errors.length} warnings=${res.warnings.length}${flags.has("--planned") ? " planned" : ""}`);
  return ok ? 0 : 1;
}

if (process.argv[1] && pathToFileURL(resolve(process.argv[1])).href === import.meta.url) process.exit(main(process.argv.slice(2)));
