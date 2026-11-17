// tools/scout/gazetteer.mjs - where a named place on vanilla Everon is, read from the game paks.
//
// The map's town, village and landmark labels are prefabs: Prefabs/World/Locations/Eden/**/*.et (Eden is the
// engine's name for Everon). Each carries its own `coords` and a `#AR-MapLocation_<Key>` display name, or
// inherits one from a Common/ prefab. The game's Language/localization.en_us.conf holds the text:
// parallel `Ids { ... }` and `Texts { ... }` lists. So "Saint Philippe" resolves to a coordinate with no
// Workbench and no world read. The core holds no list of these (LF_NamedPlaces reads the same entities).
import { existsSync, mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { buildStamp } from "../lib/install-api.mjs";
import { CACHE_DIR, gameVfs } from "../lib/game-prefab.mjs";

const LOCATIONS = /^Prefabs\/World\/Locations\/Eden\/.+\.et$/i;
const STRINGS = "Language/localization.en_us.conf";
const VERSION = 1;

// Pure: the quoted strings of one `Name { "a" "b" }` block of a runtime string table, escapes undone.
export function stringBlock(text, name) {
  const m = new RegExp(`^ ${name} ?\\{`, "m").exec(text);
  if (!m) return [];
  const out = [];
  const re = /"((?:[^"\\]|\\.)*)"|\}/g;
  re.lastIndex = m.index + m[0].length;
  for (let x; (x = re.exec(text)); ) {
    if (x[1] === undefined) break;
    out.push(x[1].replace(/\\(.)/g, (_, c) => (c === "n" ? "\n" : c === "t" ? "\t" : c)));
  }
  return out;
}

// Pure: letters only, lower case: the form a key's tail and its text are compared in.
const letters = (s) => String(s).toLowerCase().replace(/[^a-z]/g, "");

// Pure: { key: display text } for the keys that start with `prefix`. In the game's runtime string table the
// Texts list is NOT index-aligned with Ids (the same game build is 1525 texts ahead through the whole
// AR-MapLocation block, and the offset differs elsewhere), so the offset is found from the keys whose
// text is their own tail ("AR-MapLocation_Arleville" -> "Arleville"): the most common one wins, and every
// key of the block is read at it. A block with no such anchor gets no names (the caller falls back to the key).
export function blockNames(text, prefix = "AR-MapLocation_") {
  const ids = stringBlock(text, "Ids");
  const texts = stringBlock(text, "Texts");
  const at = new Map();
  texts.forEach((t, i) => {
    const k = letters(t);
    if (k && !at.has(k)) at.set(k, []);
    if (k) at.get(k).push(i);
  });
  const tally = new Map();
  ids.forEach((id, i) => {
    if (!id.startsWith(prefix)) return;
    for (const j of at.get(letters(id.slice(prefix.length))) ?? []) tally.set(j - i, (tally.get(j - i) ?? 0) + 1);
  });
  let offset = null;
  let best = 0;
  for (const [d, n] of tally) if (n > best) [offset, best] = [d, n];
  const out = {};
  if (offset === null) return out;
  ids.forEach((id, i) => {
    if (id.startsWith(prefix) && texts[i + offset] !== undefined) out[id] = texts[i + offset];
  });
  return out;
}

const header = (text) => /^[^\n]*?:\s*"\{[0-9A-Fa-f]{16}\}([^"]+\.et)"/.exec(text)?.[1] ?? null;

// Pure: what one location prefab says itself: { parent, key, coords: [x,y,z] | null }. A label key is the
// `Name` of its SCR_EditableCommentUIInfo or the `DisplayName` of its map descriptor.
export function parseLocationPrefab(text) {
  const key = /\bDisplayName\s+"#?([^"]+)"/.exec(text)?.[1] ?? /\bName\s+"#([^"]+)"/.exec(text)?.[1] ?? null;
  const c = /^\s*coords\s+(-?[\d.]+)\s+(-?[\d.]+)\s+(-?[\d.]+)/m.exec(text);
  return { parent: header(text), key, coords: c ? [Number(c[1]), Number(c[2]), Number(c[3])] : null };
}

// Pure: a place's kind from its prefab path (Urban, Natural, Aquatic; else "other").
export const kindOf = (path) => /\/(Urban|Natural|Aquatic)\//i.exec(path)?.[1].toLowerCase() ?? "other";

// The location prefab with its inherited key and coordinates: { path, key, coords } or null when none.
function resolveLocation(read, path) {
  let key = null;
  let coords = null;
  const seen = new Set();
  for (let p = path; p && !seen.has(p.toLowerCase()); ) {
    seen.add(p.toLowerCase());
    const t = read(p);
    if (!t) break;
    const one = parseLocationPrefab(t);
    key ??= one.key;
    coords ??= one.coords;
    p = one.parent;
  }
  return key && coords ? { path, key, coords } : null;
}

// Pure: "Saint Philippe" / "St. Phillipe" / "st philipe" -> "stphilipe": saints abbreviated, punctuation, spaces and
// doubled letters dropped, so the map's own spelling ("StPhillipe") and the way an agent writes it meet.
export function normalise(s) {
  return String(s)
    .toLowerCase()
    .replace(/^#?ar-maplocation_/, "")
    .replace(/[^a-z0-9]+/g, " ")
    .replace(/\bsaint\b/g, "st")
    .replace(/\bsainte\b/g, "ste")
    .replace(/\s+/g, "")
    .replace(/(.)\1+/g, "$1");
}

// Pure: the entries of the gazetteer for a list of { path, key, coords } and the string table.
export function buildEntries(locations, strings) {
  return locations
    .map((l) => ({
      key: l.key,
      name: strings[l.key] ?? strings[l.key.replace(/^#/, "")] ?? l.key.replace(/^AR-MapLocation_/, ""),
      kind: kindOf(l.path),
      x: l.coords[0],
      y: l.coords[1],
      z: l.coords[2],
      prefab: l.path,
    }))
    .sort((a, b) => a.name.localeCompare(b.name) || a.prefab.localeCompare(b.prefab));
}

// Pure: the entries a query names, best first: an exact normalised match, then a prefix, then a substring;
// within a tier Urban before Natural before Aquatic.
export function matchPlace(entries, query) {
  const q = normalise(query);
  if (!q) return [];
  const tier = (e) => {
    const n = normalise(e.name);
    const k = normalise(e.key);
    if (n === q || k === q) return 0;
    if (n.startsWith(q) || k.startsWith(q)) return 1;
    // A short query ("saint" folds to "st") would hit every name with an st in it: only a prefix counts.
    if (q.length >= 4 && (n.includes(q) || k.includes(q))) return 2;
    return 9;
  };
  const rank = { urban: 0, natural: 1, aquatic: 2, other: 3 };
  return entries
    .map((e) => ({ e, t: tier(e) }))
    .filter((r) => r.t < 9)
    .sort((a, b) => a.t - b.t || rank[a.e.kind] - rank[b.e.kind] || a.e.name.localeCompare(b.e.name))
    .map((r) => r.e);
}

// Pure: "x,z" or "x z" or "x,z,r" -> { x, z, radius|null } or null.
export function parseCoords(s) {
  const m = /^\s*(-?\d+(?:\.\d+)?)[,\s]+(-?\d+(?:\.\d+)?)(?:[,\s]+(\d+(?:\.\d+)?))?\s*$/.exec(String(s));
  return m ? { x: Number(m[1]), z: Number(m[2]), radius: m[3] === undefined ? null : Number(m[3]) } : null;
}

// Pure: the named places within `radius` metres of (x, z), nearest first (the label entities, not the buildings).
export function placesNear(entries, x, z, radius) {
  return entries
    .map((e) => ({ ...e, dist: Math.hypot(e.x - x, e.z - z) }))
    .filter((e) => e.dist <= radius)
    .sort((a, b) => a.dist - b.dist);
}

const cacheFile = () => join(CACHE_DIR(), `everon-gazetteer-${buildStamp().buildid}.json`);

// Every named place on vanilla Everon: [{ key, name, kind, x, y, z, prefab }], cached per game build.
export async function gazetteer({ fresh = false } = {}) {
  const file = cacheFile();
  if (!fresh && existsSync(file)) {
    try {
      const c = JSON.parse(readFileSync(file, "utf8"));
      if (c.v === VERSION) return c.entries;
    } catch {}
  }
  const vfs = await gameVfs();
  const read = (p) => (vfs.exists(p) ? vfs.readTextFile(p) : null);
  const strings = blockNames(read(STRINGS) ?? "");
  const locations = [];
  for (const p of vfs.allFilePaths()) if (LOCATIONS.test(p)) {
    const l = resolveLocation(read, p);
    if (l) locations.push(l);
  }
  const entries = buildEntries(locations, strings);
  try {
    mkdirSync(CACHE_DIR(), { recursive: true });
    writeFileSync(file, JSON.stringify({ v: VERSION, entries }));
  } catch {}
  return entries;
}
