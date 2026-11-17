// tools/map/lib/icons.mjs - the map icon set's manifest, shared by the generator
// (tools/map/icons/build-icons.mjs), validate-ui-kit and the selftest (card M3, #2231).
//
// Two imagesets, one set of quad names (docs/architecture/everon-map.md "Icons: the 1989 style"):
//   core  addons/LifeFramework/UI/Imagesets/LF_MapIcons.imageset              neutral placeholders
//   pack  addons/LF_Everon1989/UI/Imagesets/LF_MapIcons_Everon1989.imageset  the 1989 pictograms in ink
// A POI row's icon is its `m_sIcon` quad, else `poi_<Kind>` in the pack imageset, else the
// category fallback `cat_<category>` (both imagesets carry every category).
import { existsSync, readFileSync } from "node:fs";
import { join } from "node:path";

export const CELL = 64;
export const ATLAS = 512;

export const CATEGORIES = ["civic", "shop", "work", "law", "health", "travel", "story", "underworld"];

// Quads both imagesets carry: personal pencil marks, shared pins, map stamps.
export const PENCIL = ["pencil_cross", "pencil_circle", "pencil_arrow", "pencil_note", "pencil_question"];
export const PINS = ["pin_police", "pin_org", "pin_taxi", "pin_waypoint"];
export const STAMPS = ["stamp_town", "stamp_nopolice", "stamp_restricted", "stamp_you"];
export const SHARED = [...PENCIL, ...PINS, ...STAMPS];

// Pictograms the pack draws per place kind (the LF_Places kinds of the POI conf, plus the period
// shapes the map spec names). A kind the conf names that is not here fails the validator.
export const PACK_KINDS = [
  "Bank", "PoliceStation", "PoliceArmoury", "HospitalSite", "Workplace", "Fence", "Fuel", "Payphone",
  "Quarry", "Fish", "Story", "Taxi", "Harbour", "Farm", "Shop", "TownHall", "Church", "Pub", "Fire", "Depot",
];

// Tileable hatch textures (PolygonDrawCommand.m_pTexture): not imageset quads.
export const CORE_HATCH = ["Diagonal", "Cross", "Dots"];
export const PACK_HATCH = ["Lawless", "Gated"];

export const FILES = {
  core: {
    addon: "addons/LifeFramework",
    imageset: "addons/LifeFramework/UI/Imagesets/LF_MapIcons.imageset",
    atlas: "addons/LifeFramework/UI/Imagesets/LF_MapIcons.edds",
    hatch: CORE_HATCH.map((n) => `addons/LifeFramework/UI/Textures/Map/LF_MapHatch_${n}.edds`),
  },
  pack: {
    addon: "addons/LF_Everon1989",
    imageset: "addons/LF_Everon1989/UI/Imagesets/LF_MapIcons_Everon1989.imageset",
    atlas: "addons/LF_Everon1989/UI/Imagesets/LF_MapIcons_Everon1989.edds",
    hatch: PACK_HATCH.map((n) => `addons/LF_Everon1989/UI/Imagesets/LF_MapIcons_Everon1989_Hatch${n}.edds`),
  },
};

export const catQuad = (c) => `cat_${c}`;
export const poiQuad = (k) => `poi_${k}`;

export function coreQuadNames() {
  return [...CATEGORIES.map(catQuad), ...SHARED];
}
export function packQuadNames() {
  return [...CATEGORIES.map(catQuad), ...PACK_KINDS.map(poiQuad), ...SHARED];
}

// Quad name -> { x, y, w, h } on the 512 atlas, row-major in manifest order.
export function layout(names) {
  const per = ATLAS / CELL;
  if (names.length > per * per) throw new Error(`${names.length} quads do not fit a ${per}x${per} atlas`);
  const out = new Map();
  names.forEach((n, i) => out.set(n, { x: (i % per) * CELL, y: Math.floor(i / per) * CELL, w: CELL, h: CELL }));
  return out;
}

// ---- parsing (used by the validator) -------------------------------------------------------

export function parseImageset(text) {
  const ref = /RefSize\s+(\d+)\s+(\d+)/.exec(text);
  const textures = [...text.matchAll(/path\s+"\{([0-9A-F]{16})\}([^"]+)"/g)].map((m) => ({ guid: m[1], path: m[2] }));
  const quads = new Map();
  for (const m of text.matchAll(/ImageSetDefClass\s+\S+\s*\{([^}]*)\}/g)) {
    const name = /Name\s+"([^"]+)"/.exec(m[1]);
    const pos = /Pos\s+(\d+)\s+(\d+)/.exec(m[1]);
    const size = /Size\s+(\d+)\s+(\d+)/.exec(m[1]);
    if (name && pos && size) quads.set(name[1], { x: +pos[1], y: +pos[2], w: +size[1], h: +size[2] });
  }
  return { refSize: ref ? [+ref[1], +ref[2]] : null, textures, quads };
}

// LF_MapPoiKind rows of a POI conf (core conf or a pack overlay): { kind, category, icon }.
export function confRows(text) {
  const rows = [];
  const re = /LF_MapPoiKind\s+(?:"[^"]*"\s*)?\{/g;
  let m;
  while ((m = re.exec(text))) {
    let depth = 1;
    let i = re.lastIndex;
    while (i < text.length && depth > 0) {
      if (text[i] === "{") depth++;
      else if (text[i] === "}") depth--;
      i++;
    }
    const body = text.slice(re.lastIndex, i - 1);
    const get = (k) => (new RegExp(`${k}\\s+"([^"]*)"`).exec(body) ?? [])[1] ?? "";
    rows.push({ kind: get("m_sKind"), category: get("m_sCategory"), icon: get("m_sIcon") });
    re.lastIndex = i;
  }
  return rows.filter((r) => r.kind);
}

function metaGuid(file) {
  const m = /Name\s+"\{([0-9A-F]{16})\}/.exec(readFileSync(file, "utf8"));
  return m ? m[1] : null;
}

// root: repo root. confs: [{ path, text }] of the POI confs (core plus pack overlays; none yet is fine).
// Returns the failure lines.
export function iconProblems(root, confs = []) {
  const out = [];
  const sets = {};
  for (const side of ["core", "pack"]) {
    const f = FILES[side];
    const file = join(root, f.imageset);
    // Not built yet: the textures need a Workbench pool machine (docs/features/map.md "Icons"), and an
    // imageset is committed only with its .edds. Nothing to check until then; once it exists, all of it is.
    if (!existsSync(file)) continue;
    const set = parseImageset(readFileSync(file, "utf8"));
    sets[side] = set;
    const want = side === "core" ? coreQuadNames() : packQuadNames();
    for (const n of want) if (!set.quads.has(n)) out.push(`${f.imageset}: quad "${n}" is missing`);
    for (const [n, q] of set.quads) {
      if (set.refSize && (q.x + q.w > set.refSize[0] || q.y + q.h > set.refSize[1])) out.push(`${f.imageset}: quad "${n}" is outside the ${set.refSize.join("x")} texture`);
    }
    if (!set.textures.length) out.push(`${f.imageset}: no texture`);
    for (const t of set.textures) {
      const rel = `${f.addon}/${t.path}`;
      if (!existsSync(join(root, rel))) out.push(`${f.imageset}: texture ${rel} is missing (rebuild it through ui-image on a pool machine)`);
      else if (!existsSync(join(root, `${rel}.meta`)) || metaGuid(join(root, `${rel}.meta`)) !== t.guid) out.push(`${f.imageset}: texture GUID {${t.guid}} does not match ${rel}.meta`);
    }
    for (const h of f.hatch) if (!existsSync(join(root, h)) || !existsSync(join(root, `${h}.meta`))) out.push(`${h}: hatch texture (and its .meta) is missing`);
  }
  // Every row the POI confs name resolves to a quad that exists.
  const core = sets.core?.quads ?? new Map();
  const pack = sets.pack?.quads ?? new Map();
  const cats = new Map();
  for (const c of confs) for (const r of confRows(c.text)) if (r.category) cats.set(r.kind, r.category);
  for (const c of confs) {
    for (const r of confRows(c.text)) {
      const cat = r.category || cats.get(r.kind) || "";
      if (cat && !CATEGORIES.includes(cat)) out.push(`${c.path}: kind ${r.kind} has category "${cat}"; the icon set has ${CATEGORIES.join(", ")}`);
      if (r.icon && !pack.has(r.icon) && !core.has(r.icon)) out.push(`${c.path}: kind ${r.kind} names icon "${r.icon}", which is in neither LF_MapIcons imageset`);
      if (!r.icon && !pack.has(poiQuad(r.kind)) && !(cat && pack.has(catQuad(cat)))) out.push(`${c.path}: kind ${r.kind} has no pictogram (${poiQuad(r.kind)}) and no category fallback in the pack imageset`);
    }
  }
  return out;
}
