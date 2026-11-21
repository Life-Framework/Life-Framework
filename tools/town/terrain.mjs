// tools/town/terrain.mjs - the ground heights a fitted slot's open-ground rows rest on (kit rows with `"mount": "terrain"`,
// tools/town/seat.mjs).
//
// A site in the open (the farm compound, the hilltop mast) has no floor to seat its props on, and the dossier holds no
// height grid. `town fit <place> <slot> --terrain` asks the island's test server for the terrain height under every point
// a terrain row touches (one vertical probe ray each, LF_PlacementAudit.RayLine: `sy`, the height of the terrain at the
// ray's x and z) and keeps the answers in tools/town/terrain/<place>-<slot>.json. A fit without the flag reads that file,
// so it stays offline, deterministic and checkable (`--check`). The file is keyed by the point, so a kit change that moves
// a row asks for a new probe: a point with no answer is reported, never guessed.
//
//   readPins(root, place, slot)      -> { points: { "x,z": y } } | null
//   terrainFromPins(pins)            -> { at(x, z) } | null              the height at a probed point, else null
//   probeLines(points, aroundY)      -> string[]                         `ray <id> ...` lines for the server's input
//   pinsFromRays(points, rays)       -> { points, missing }              rays: parseRays() of the server's out.txt
//   probeTerrain(points, opts)       -> { points, missing }              one server boot (pool, never pinned)
//   writePins(root, place, slot, points, date)
import { existsSync, mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";

export const TERRAIN_DIR = "tools/town/terrain";
export const pinFileOf = (place, slot) => `${TERRAIN_DIR}/${place}-${slot}.json`;
const r3 = (n) => Math.round(n * 1000) / 1000;
export const pinKey = (x, z) => `${r3(x)},${r3(z)}`;
const REACH = 60; // metres a probe ray runs above and below the slot's anchor height

export function readPins(root, place, slot) {
  const f = join(root, pinFileOf(place, slot));
  if (!existsSync(f)) return null;
  try {
    const j = JSON.parse(readFileSync(f, "utf8"));
    return j && typeof j.points === "object" && j.points ? j : null;
  } catch {
    return null;
  }
}

export function terrainFromPins(pins) {
  if (!pins?.points) return null;
  return {
    at(x, z) {
      const y = pins.points[pinKey(x, z)];
      return Number.isFinite(y) ? y : null;
    },
  };
}

const uniq = (points) => [...new Map(points.map((p) => [pinKey(p[0], p[1]), [r3(p[0]), r3(p[1])]])).values()];

// One vertical ray per distinct point: from aroundY + REACH down to aroundY - REACH. The answer's `sy` is the terrain.
export function probeLines(points, aroundY) {
  return uniq(points).map((p, i) => `ray t${i} ${p[0]} ${r3(aroundY + REACH)} ${p[1]} ${p[0]} ${r3(aroundY - REACH)} ${p[1]}`);
}

// A paved surface laid on the terrain (asphalt, a pavement slab, cobbles) is the ground a prop stands on: the terrain's own height is
// 10 cm under it (the residents' benches stood 10.9 cm into AsphaltPavement_01_Narrow_4m_A, #3888).
export const SURFACE_HIT = /^(Asphalt|Pavement|Road|Cobble|Kerb|Curb|Sidewalk)/i;

export function pinsFromRays(points, rays) {
  const out = {};
  const missing = [];
  uniq(points).forEach((p, i) => {
    const ray = rays.get(`t${i}`);
    const sy = ray?.sy;
    const hitY = ray?.p?.[1];
    const onSurface = SURFACE_HIT.test(String(ray?.hit ?? "")) && Number.isFinite(hitY) && Number.isFinite(sy) && hitY > sy;
    if (Number.isFinite(sy)) out[pinKey(p[0], p[1])] = r3(onSurface ? hitY : sy);
    else missing.push(pinKey(p[0], p[1]));
  });
  return { points: out, missing };
}

// One boot of the island's test server (the pool picks the machine; nothing is pinned). The slot's own layer must be on
// disk: the audit lists the island's records from it and boots MainWorld around them.
export async function probeTerrain(points, { layerName, aroundY, log = console.log, measure = null, parse = null }) {
  const audit = await import("../../agent-box/place-audit.mjs");
  const parseRays = parse ?? (await import("../place/place.mjs")).parseRays;
  const run = (measure ?? audit.measureTown)(audit.auditRecords({ layers: [layerName], island: true }), { island: true, extra: probeLines(points, aroundY), preferLocal: true, log });
  if (run.outText == null) throw new Error("the island's test server wrote no measurements (lines above)");
  return pinsFromRays(points, parseRays(run.outText).rays);
}

export function writePins(root, place, slot, points, date) {
  const f = join(root, pinFileOf(place, slot));
  mkdirSync(dirname(f), { recursive: true });
  const sorted = Object.fromEntries(Object.entries(points).sort(([a], [b]) => a.localeCompare(b)));
  writeFileSync(f, `${JSON.stringify({ schema: 1, place, slot, probed: date, points: sorted }, null, 2)}\n`);
  return f;
}
