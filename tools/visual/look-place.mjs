// tools/visual/look-place.mjs - `tools\cli look place:<place>/<slot>`: a fitted town slot on the pack's island (#2347).
//
// A town draft's slot sits on the real Everon map, which DebugWorld does not have: shot there, the camera found no
// ground (y -253.8) and every picture was sky. This resolves the slot to a look area on ISLAND_WORLD (the pack's
// MainWorld) from the place's own files, so no coordinate is typed:
//   docs/everon/places/<place>/mapping.json   slot -> building (or plot)
//   docs/everon/places/<place>/dossier.json   that building's footprint, yaw, floor heights and entrances
// The view: the building's front is its local +z, so the wide stands on compass heading `yaw` (0 = +z), at eye level
// first; a known entrance beats the yaw. The ground height comes from the island terrain in the plugin (`-`).
// `--interior auto` stands inside on the ground floor (the footprint centre, the dossier's floor height).
// Pure but for the two JSON reads; tools/visual/look.mjs selftest runs it on the fixture place.

import { existsSync, readFileSync } from "node:fs";
import { join } from "node:path";

export const PLACE_RX = /^place:([a-z0-9][a-z0-9-]*)\/([a-z0-9][a-z0-9-]*)(?:@([a-z0-9]+))?$/;
const PLACE_SLUG = /^[a-z0-9][a-z0-9-]*$/;
const r1 = (v) => Math.round(v * 10) / 10;
const norm = (az) => ((Math.round(az) % 360) + 360) % 360;

// The dossier and mapping of a place: { dossier, mapping } or a throw that names the missing file.
export function readPlace(root, place, { dossierFile, mappingFile } = {}) {
  const dir = join(root, "docs", "everon", "places", place);
  const dFile = dossierFile ?? join(dir, "dossier.json");
  const mFile = mappingFile ?? join(dir, "mapping.json");
  if (!existsSync(dFile)) throw new Error(`place ${place} has no dossier (${dFile}); run tools\\cli scout first`);
  if (!existsSync(mFile)) throw new Error(`place ${place} has no mapping (${mFile}); run tools\\cli town map ${place}`);
  const mapping = JSON.parse(readFileSync(mFile, "utf8"));
  const others = {};
  for (const e of Object.values(mapping?.slots ?? {})) {
    // An outposts slot (`drops`) stands on buildings of several places' surveys: load each one named.
    for (const slug of [e?.dossier, ...(e?.drops ?? []).map((dr) => dr?.place)]) {
      const f = typeof slug === "string" && PLACE_SLUG.test(slug) ? join(root, "docs", "everon", "places", slug, "dossier.json") : null;
      if (f && !others[slug] && existsSync(f)) others[slug] = JSON.parse(readFileSync(f, "utf8"));
    }
  }
  return { dossier: JSON.parse(readFileSync(dFile, "utf8")), mapping, others };
}

// The x, z of the kit rows a slot's layer holds (Place_<place>_<slot>.layer, entities named Fit_*, the _Place marker left out: it
// stands on the host building, not where the kit's things are), or [] when there is no layer yet.
export function layerRows(root, place, slot) {
  const f = join(root, "addons", "LF_Everon1989", "Worlds", "MainWorld", "MainWorld_Layers", `Place_${place}_${slot}.layer`);
  if (!existsSync(f)) return [];
  const out = [];
  for (const m of readFileSync(f, "utf8").matchAll(/^\w+ (Fit_\w+)(?: : "[^"]+")? \{[\s\S]*?\n coords (\S+) (\S+) (\S+)/gm)) {
    if (!/_Place(_\w+)?$/.test(m[1])) out.push([Number(m[2]), Number(m[4])]);
  }
  return out.filter((r) => r.every(Number.isFinite));
}

// Pure: the look frame when the slot's rows lie outside the host's own frame (a pond 62 m from the barn it anchors on: the
// bench and the fishing spot were never in any look, #3666). The middle of the rows' box, far enough to hold the farthest one;
// null when every row is inside the frame already (the host's own view stays as it was).
export function frameRows(rows, host, R, { margin = 8, max = 90 } = {}) {
  if (!rows?.length) return null;
  if (rows.every(([x, z]) => Math.hypot(x - host.x, z - host.z) <= R)) return null;
  const xs = rows.map((r) => r[0]);
  const zs = rows.map((r) => r[1]);
  const x = r1((Math.min(...xs) + Math.max(...xs)) / 2);
  const z = r1((Math.min(...zs) + Math.max(...zs)) / 2);
  return { x, z, radius: Math.min(max, r1(Math.max(...rows.map(([rx, rz]) => Math.hypot(rx - x, rz - z))) + margin)) };
}

// Pure: { dossier, mapping } and a slot -> the look area { name, x, y, z, radius, front, world: "island", room, wideCands, slot }.
// `rows` (layerRows) widens the frame to the kit's things when they lie outside the host's own.
export function placeArea({ dossier, mapping, others }, place, slot, radius, pick = null, rows = []) {
  const entry = mapping?.slots?.[slot];
  if (!entry || entry.none) {
    const known = Object.entries(mapping?.slots ?? {}).filter(([, e]) => e && !e.none).map(([id]) => id);
    throw new Error(`place ${place} maps no slot "${slot}" (mapped: ${known.join(", ") || "none"})`);
  }
  let name = `place-${place}-${slot}`;
  // An outposts slot is several drops: `place:<place>/<slot>@<building>` looks at that drop, the first one without a suffix.
  let hostPlace = entry.dossier;
  let hostBuilding = entry.building;
  if (Array.isArray(entry.drops)) {
    const dr = pick ? entry.drops.find((q) => q.building === pick) : entry.drops[0];
    if (!dr) throw new Error(`slot ${slot}: no drop on building ${pick} (drops: ${entry.drops.map((q) => q.building).join(", ")})`);
    hostPlace = dr.place === place ? undefined : dr.place;
    hostBuilding = dr.building;
    name = `${name}-${dr.building}`;
  }
  if (entry.plot) {
    const p = (dossier.freePlots?.plots ?? []).find((q) => q.id === entry.plot);
    if (!p) throw new Error(`slot ${slot}: plot ${entry.plot} is not in the dossier of ${place}`);
    const R = radius ?? Math.max(15, r1(Math.hypot(p.width ?? 20, p.depth ?? 20) / 2 + 6));
    const fr = radius ? null : frameRows(rows, p, R);
    return { name, x: fr?.x ?? p.x, y: 0, z: fr?.z ?? p.z, radius: fr?.radius ?? R, front: 180, world: "island", slot: { place, id: slot, plot: p.id } };
  }
  // An entry with `dossier` stands in another place's survey (the harbour master's cabin at the dock): read that one.
  const from = hostPlace ? (others?.[hostPlace] ?? { buildings: [] }) : dossier;
  const b = (from.buildings ?? []).find((q) => q.id === hostBuilding);
  if (!b) throw new Error(`slot ${slot}: building ${hostBuilding} is not in the dossier of ${hostPlace ?? place}`);
  const fp = b.footprint ?? {};
  const cx = fp.cx ?? b.x;
  const cz = fp.cz ?? b.z;
  const w = fp.w ?? 12;
  const d = fp.d ?? 12;
  // Far enough that the wide holds the whole front, close enough that the building fills it.
  const R = radius ?? Math.max(14, r1(Math.hypot(w, d) / 2 + 8));
  // The front: the first known entrance seen from the centre, else the building's local +z (its yaw).
  // A door on the building's own front side (the dossier's `side: "front"`) beats the first one listed: the harbour
  // warehouse lists a side door first and its wide stood in the grass beside it (#3577).
  const doors = (b.entrances ?? []).filter((e) => Number.isFinite(e?.x) && Number.isFinite(e?.z));
  const door = doors.find((e) => e.side === "front") ?? doors[0];
  const front = door && Math.hypot(door.x - cx, door.z - cz) > 0.5 ? norm((Math.atan2(door.x - cx, door.z - cz) * 180) / Math.PI) : norm(b.yaw ?? 180);
  // Eye level on the street in front, straight on, then a step back and up: tried before look's own fan.
  const reach = Math.max(w, d) / 2;
  const wideCands = [
    { az: front, dist: r1(reach + 6), up: 1.7 },
    { az: front, dist: r1(reach + 10), up: 3 },
    { az: norm(front - 20), dist: r1(reach + 9), up: 2.2 },
    { az: norm(front + 20), dist: r1(reach + 9), up: 2.2 },
  ];
  // The ground floor's room, for --interior auto: the footprint centre at the dossier's floor height.
  const floorY = r1((b.y ?? 0) + (Number(b.firstFloor) || 0));
  const room = { x: cx, y: floorY, z: cz, radius: Math.min(8, Math.max(4, r1(Math.min(w, d) / 2 - 1.5))) };
  const fr = radius ? null : frameRows(rows, { x: cx, z: cz }, R);
  return { name, x: fr?.x ?? cx, y: 0, z: fr?.z ?? cz, radius: fr?.radius ?? R, front, world: "island", wideCands: fr ? wideCands.map((c) => ({ ...c, dist: r1(fr.radius * 0.8) })) : wideCands, room, slot: { place, id: slot, building: b.id, stem: b.stem, yaw: b.yaw, entrance: door ? { x: door.x, z: door.z } : null } };
}

// I/O wrapper: place:<place>/<slot> -> the look area.
export function resolvePlace(arg, root, radius, files = {}) {
  const m = PLACE_RX.exec(String(arg));
  if (!m) throw new Error(`"${arg}" is not place:<place>/<slot> (e.g. place:saint-philippe/arrival)`);
  return placeArea(readPlace(root, m[1], files), m[1], m[2], radius, m[3] ?? null, layerRows(root, m[1], m[2]));
}
