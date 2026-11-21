// tools/town/outposts.mjs - the pure helpers of an `outposts` slot (the parcel drop points, #2959).
//
// An outposts slot stands in buildings of one or more surveys, one small prop per building (a mailbox on the entrance wall). The parcel
// drops list OTHER places' buildings; a village-wide slot (ATMs, payphones, bins, #3346) lists its own place's buildings too (`place` = the slug
// of the mapping's own place), read from the same dossier file.
// Its mapping entry lists them: `drops`: [{ place, building }]. It rides on the same idea as an entry's `dossier` (a harbour
// master's cabin in the dock survey, #3058: `others` holds the other places' dossiers, a building is keyed "<place>/<id>"):
// each drop is read as `{ building, dossier: place }`, so one footprint lookup (check.mjs `slotGround`) serves both.
//
// This module imports nothing from check.mjs or map.mjs, so both can call it without a cycle.

import { existsSync, readFileSync } from "node:fs";
import { join } from "node:path";

const SLUG_RX = /^[a-z0-9][a-z0-9-]*$/;

// A drop as a mapping entry the single-building tools already read.
export const dropAsEntry = (dr) => ({ building: dr?.building, dossier: dr?.place });
// An entry's or claim's drops as sorted "<place>/<building>" keys.
export const dropKeys = (drops) => (Array.isArray(drops) ? drops.map((d) => `${d?.place}/${d?.building}`).sort() : []);

// The dossiers the drops of a mapping stand in, by place slug (a missing file is left out; checkOutposts reports it).
export function loadDropDossiers(root, mapping) {
  const out = {};
  for (const e of Object.values(mapping?.slots ?? {})) {
    for (const dr of Array.isArray(e?.drops) ? e.drops : []) {
      if (typeof dr?.place !== "string" || !SLUG_RX.test(dr.place) || out[dr.place]) continue;
      const f = join(root, "docs", "everon", "places", dr.place, "dossier.json");
      if (existsSync(f)) out[dr.place] = JSON.parse(readFileSync(f, "utf8"));
    }
  }
  return out;
}

// Reads a mapping's drop dossiers once and keeps them on the mapping object (not enumerable: the file text never carries them),
// so every tool that read the mapping (check, map --check, fit) finds them without another parameter. Returns the mapping.
export function attachDropDossiers(root, mapping) {
  if (mapping && typeof mapping === "object" && !("dropDossiers" in mapping)) {
    Object.defineProperty(mapping, "dropDossiers", { value: loadDropDossiers(root, mapping), enumerable: false });
  }
  return mapping;
}

// Pure: the problems of an `outposts` mapping entry, [] when it is sound. `dossiers` ({ <place>: dossier json }) is optional;
// the buildings are looked up when it is given.
export function checkOutposts(slotId, e, dossiers, ownPlace) {
  const out = [];
  const at = `slot ${slotId}`;
  if (!Array.isArray(e.drops) || !e.drops.length) return [`${at}: an outposts slot needs a non-empty "drops" list of { place, building }`];
  const seen = new Set();
  for (const [i, dr] of e.drops.entries()) {
    const name = `${at}: drops[${i}]`;
    if (typeof dr?.place !== "string" || typeof dr?.building !== "string") {
      out.push(`${name} needs a place and a building`);
      continue;
    }
    const key = `${dr.place}/${dr.building}`;
    if (seen.has(key)) out.push(`${name}: ${key} is listed twice`);
    seen.add(key);
    if (dossiers) {
      const dossier = dossiers[dr.place];
      if (!dossier) out.push(`${name}: no dossier for place ${dr.place}`);
      else {
        const b = (dossier.buildings ?? []).find((x) => x.id === dr.building);
        if (!b) out.push(`${name}: building ${dr.building} is not in the dossier of ${dr.place}`);
        else if (b.ruin) out.push(`${name}: ${key} is a ruin`);
      }
    }
  }
  return out;
}
