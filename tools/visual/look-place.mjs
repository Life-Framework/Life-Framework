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

export const PLACE_RX = /^place:([a-z0-9][a-z0-9-]*)\/([a-z0-9][a-z0-9-]*)$/;
const r1 = (v) => Math.round(v * 10) / 10;
const norm = (az) => ((Math.round(az) % 360) + 360) % 360;

// The dossier and mapping of a place: { dossier, mapping } or a throw that names the missing file.
export function readPlace(root, place, { dossierFile, mappingFile } = {}) {
  const dir = join(root, "docs", "everon", "places", place);
  const dFile = dossierFile ?? join(dir, "dossier.json");
  const mFile = mappingFile ?? join(dir, "mapping.json");
  if (!existsSync(dFile)) throw new Error(`place ${place} has no dossier (${dFile}); run tools\\cli scout first`);
  if (!existsSync(mFile)) throw new Error(`place ${place} has no mapping (${mFile}); run tools\\cli town map ${place}`);
  return { dossier: JSON.parse(readFileSync(dFile, "utf8")), mapping: JSON.parse(readFileSync(mFile, "utf8")) };
}

// Pure: { dossier, mapping } and a slot -> the look area { name, x, y, z, radius, front, world: "island", room, wideCands, slot }.
export function placeArea({ dossier, mapping }, place, slot, radius) {
  const entry = mapping?.slots?.[slot];
  if (!entry || entry.none) {
    const known = Object.entries(mapping?.slots ?? {}).filter(([, e]) => e && !e.none).map(([id]) => id);
    throw new Error(`place ${place} maps no slot "${slot}" (mapped: ${known.join(", ") || "none"})`);
  }
  const name = `place-${place}-${slot}`;
  if (entry.plot) {
    const p = (dossier.freePlots?.plots ?? []).find((q) => q.id === entry.plot);
    if (!p) throw new Error(`slot ${slot}: plot ${entry.plot} is not in the dossier of ${place}`);
    const R = radius ?? Math.max(15, r1(Math.hypot(p.width ?? 20, p.depth ?? 20) / 2 + 6));
    return { name, x: p.x, y: 0, z: p.z, radius: R, front: 180, world: "island", slot: { place, id: slot, plot: p.id } };
  }
  const b = (dossier.buildings ?? []).find((q) => q.id === entry.building);
  if (!b) throw new Error(`slot ${slot}: building ${entry.building} is not in the dossier of ${place}`);
  const fp = b.footprint ?? {};
  const cx = fp.cx ?? b.x;
  const cz = fp.cz ?? b.z;
  const w = fp.w ?? 12;
  const d = fp.d ?? 12;
  // Far enough that the wide holds the whole front, close enough that the building fills it.
  const R = radius ?? Math.max(14, r1(Math.hypot(w, d) / 2 + 8));
  // The front: the first known entrance seen from the centre, else the building's local +z (its yaw).
  const door = (b.entrances ?? []).find((e) => Number.isFinite(e?.x) && Number.isFinite(e?.z));
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
  return { name, x: cx, y: 0, z: cz, radius: R, front, world: "island", wideCands, room, slot: { place, id: slot, building: b.id, stem: b.stem, yaw: b.yaw, entrance: door ? { x: door.x, z: door.z } : null } };
}

// I/O wrapper: place:<place>/<slot> -> the look area.
export function resolvePlace(arg, root, radius, files = {}) {
  const m = PLACE_RX.exec(String(arg));
  if (!m) throw new Error(`"${arg}" is not place:<place>/<slot> (e.g. place:saint-philippe/arrival)`);
  return placeArea(readPlace(root, m[1], files), m[1], m[2], radius);
}
