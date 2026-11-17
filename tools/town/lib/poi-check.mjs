// tools/town/lib/poi-check.mjs - the town pipeline's POI hook (docs/architecture/everon-map.md "Where the data comes
// from", card M17 / #2245).
//
// A map POI is a live LF_Places entry whose kind has a row in the POI table (core `Configs/Map/LF_MapPois.conf` plus
// every pack overlay `LF_MapPois*.conf`). A town template slot therefore names the one *front* kind its kit puts on
// the map: `"poi": "Bank"`, or `"poi": "none"` when the slot puts nothing there (the arrival spawn, a vault). A slot that
// names nothing, or names a kind with no row, would never reach the map and nobody would notice, so the check fails it.
//
//   loadPoiKinds(root)         -> { kinds: Set<string>, files: string[] }   the POI table's kinds, read from the confs
//   parsePoiKinds(text)        -> string[]                                  `m_sKind "X"` of every row of one conf
//   checkSlotPois(template, kinds) -> string[]                              the problems, [] when every slot is sound
//
// Pure apart from loadPoiKinds' file reads. `town map --check` calls checkSlotPois; `town check` (T4, #2206) calls it
// the same way.
import { existsSync, readdirSync, readFileSync } from "node:fs";
import { join } from "node:path";

export const NONE = "none";
const KIND_RX = /^[A-Za-z][A-Za-z0-9]*$/;
const CORE_CONF = join("addons", "LifeFramework", "Configs", "Map", "LF_MapPois.conf");

// Pure: the `m_sKind` values of a POI conf's rows, in file order.
export function parsePoiKinds(text) {
  const out = [];
  for (const m of String(text).matchAll(/^\s*m_sKind\s+"([^"]*)"/gm)) if (m[1]) out.push(m[1]);
  return out;
}

// The POI table of a checkout: the core conf and every pack overlay (addons/*/Configs/Map/LF_MapPois*.conf).
export function loadPoiKinds(root) {
  const core = join(root, CORE_CONF);
  if (!existsSync(core)) throw new Error(`the core POI conf ${CORE_CONF} does not exist under ${root}`);
  const files = [core];
  const addons = join(root, "addons");
  for (const a of readdirSync(addons, { withFileTypes: true })) {
    if (!a.isDirectory()) continue;
    const dir = join(addons, a.name, "Configs", "Map");
    if (!existsSync(dir)) continue;
    for (const f of readdirSync(dir).sort()) {
      const file = join(dir, f);
      if (/^LF_MapPois.*\.conf$/.test(f) && !files.includes(file)) files.push(file);
    }
  }
  const kinds = new Set();
  for (const f of files) for (const k of parsePoiKinds(readFileSync(f, "utf8"))) kinds.add(k);
  return { kinds, files };
}

// Pure: the problems of a template's POI naming, [] when each slot names a kind with a row (that it registers) or "none".
export function checkSlotPois(template, kinds) {
  const out = [];
  const known = kinds instanceof Set ? kinds : new Set(kinds ?? []);
  for (const s of Array.isArray(template?.slots) ? template.slots : []) {
    const at = `slot ${s?.id ?? "?"}`;
    const p = s?.poi;
    if (typeof p !== "string" || !p) {
      out.push(`${at}: names no POI kind; set "poi" to the slot's front kind (a row of LF_MapPois.conf) or to "${NONE}"`);
      continue;
    }
    if (p === NONE) continue;
    if (!KIND_RX.test(p)) {
      out.push(`${at}: poi '${p}' is not a kind name; use a row of LF_MapPois.conf or "${NONE}"`);
      continue;
    }
    if (!known.has(p)) {
      out.push(`${at}: front kind '${p}' has no POI row in LF_MapPois.conf or a pack overlay; add the row or set "poi": "${NONE}"`);
      continue;
    }
    if (!Array.isArray(s.places) || !s.places.includes(p)) {
      out.push(`${at}: front kind '${p}' is not one of the slot's places (${Array.isArray(s.places) ? s.places.join(", ") : "none"}), so fitting the slot would never register it`);
    }
  }
  return out;
}
