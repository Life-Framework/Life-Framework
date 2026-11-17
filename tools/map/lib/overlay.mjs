// tools/map/lib/overlay.mjs - the data the map draws, read from the same confs the game reads.
// POIs: a place kind row (addons/LifeFramework/Configs/Map/LF_MapPois.conf, then each pack overlay
// addons/LF_Everon1989/Configs/Map/LF_MapPois*.conf: a row with the same kind replaces the core row) joined to the
// live places of a world, whose positions come from its `.layer` files (tools/map/lib/places.mjs). The table
// holds no coordinates (AGENTS.md rule 9). A place whose kind has no row never shows; a dark place (live state not
// listed by the header) stays off the map. Areas: LF_MapAreas.conf, same two folders, same-Id replaces (M5).
// tools/map/render/fixtures/map-overlay.fixture.conf is the stand-in only when asked for (`--fixture`) and in selftests.
// Places (town labels) and buildings: docs/everon/catalog-buildings.json, the dump of the real Everon world.
import { existsSync, readFileSync, readdirSync } from "node:fs";
import { join } from "node:path";
import { findBlocks, num, parseConf, str, vecListXZ, vecXZ } from "./conf.mjs";
import { STYLES } from "../rules/areas.mjs";
import { DEFAULT_WORLD, loadPlaces } from "./places.mjs";

const CONF_DIRS = ["addons/LifeFramework/Configs/Map", "addons/LF_Everon1989/Configs/Map"];
const STRING_TABLES = ["addons/LifeFramework/Language/everonlife_localization.st", "addons/LF_Everon1989/Language/lf_everon1989_localization.st"];
const FIXTURE = "tools/map/render/fixtures/map-overlay.fixture.conf";

const fixturePoiOf = (b) => {
  const [x, z] = vecXZ(b.props.Pos ?? "0 0 0");
  return { id: str(b.props.Id, b.name), name: str(b.props.Name), kind: str(b.props.Kind, "PLACE").toUpperCase(), access: str(b.props.Access, "PUBLIC"), x, z, minZoom: num(b.props.MinZoom, 3), heardOf: num(b.props.HeardOf, 0) === 1 };
};
const fixtureAreaOf = (b) => {
  const a = { id: str(b.props.Id, b.name), name: str(b.props.Name), style: str(b.props.Style, "NORMAL").toUpperCase(), shape: str(b.props.Shape, "CIRCLE").toUpperCase() };
  if (a.shape === "POLYGON") {
    const t = (b.props.Points || []).map(Number);
    a.points = [];
    for (let i = 0; i + 1 < t.length; i += 2) a.points.push([t[i], t[i + 1]]);
  } else {
    [a.x, a.z] = vecXZ(b.props.Centre ?? "0 0 0");
    a.radius = num(b.props.Radius, 50);
  }
  return a;
};

const confFiles = (root, dir, prefix) => {
  const d = join(root, dir);
  return existsSync(d) ? readdirSync(d).filter((n) => n.startsWith(prefix) && n.endsWith(".conf")).sort().map((n) => join(d, n)) : [];
};

// One row of LF_MapAreas.conf (class LF_MapAreaRow, members as the game reads them) as the renderer draws it. Points
// are world vectors, unquoted (a flat run of numbers); the name is the row's label, else the style's own label ("Safe zone"), else empty (a
// town line carries none: the vanilla town name is already on the map).
const rowAreaOf = (b, text) => {
  const style = str(b.props.m_eStyle, "CUSTOM").toUpperCase();
  const label = str(b.props.m_sLabel) || STYLES[style]?.labelKey || "";
  const a = { id: str(b.props.m_sId), name: label.startsWith("#") ? text(label) : label, style, shape: str(b.props.m_eShape, "POLYGON").toUpperCase() };
  if (a.shape === "CIRCLE") {
    [a.x, a.z] = vecXZ(b.props.m_vCentre ?? "0 0 0");
    a.radius = num(b.props.m_fRadius, 30);
  } else {
    a.points = vecListXZ(b.props.m_aPoints);
  }
  return a;
};

/** The area rows: { id -> area }, core LF_MapAreas.conf then each pack overlay LF_MapAreas*.conf, same Id replaces. */
export function loadAreas(root, text = () => "") {
  const rows = new Map();
  for (const d of CONF_DIRS) {
    for (const f of confFiles(root, d, "LF_MapAreas")) {
      for (const b of findBlocks(parseConf(readFileSync(f, "utf8")), "LF_MapAreaRow")) {
        const r = rowAreaOf(b, text);
        if (!r.id) continue;
        if (str(b.props.m_sOp) === "remove") rows.delete(r.id);
        else rows.set(r.id, r);
      }
    }
  }
  return rows;
}

/** The POI kind rows: { kind -> { kind, category, icon, label, minZoom, visibility, role } }, pack rows over core rows. */
export function loadKinds(root) {
  const rows = new Map();
  for (const d of CONF_DIRS) {
    for (const f of confFiles(root, d, "LF_MapPois")) {
      for (const b of findBlocks(parseConf(readFileSync(f, "utf8")), "LF_MapPoiKind")) {
        const kind = str(b.props.m_sKind);
        if (!kind) continue;
        rows.set(kind, { kind, category: str(b.props.m_sCategory, "civic"), icon: str(b.props.m_sIcon), label: str(b.props.m_sLabel), minZoom: num(b.props.m_fMinZoom, 0), visibility: str(b.props.m_eVisibility, "PUBLIC").toUpperCase(), role: str(b.props.m_sRole) });
      }
    }
  }
  return rows;
}

// English text of a localization key (`#Key` or `Key`); the pack table over the core table. "" when unknown.
export function loadStrings(root) {
  const t = new Map();
  for (const f of STRING_TABLES) {
    const p = join(root, f);
    if (!existsSync(p)) continue;
    const text = readFileSync(p, "utf8");
    for (const m of text.matchAll(/Id "([^"]+)"\s+Target_en_us "([^"]*)"/g)) t.set(m[1], m[2]);
  }
  return (key) => t.get(String(key).replace(/^#/, "")) || "";
}

// The colour family and glyph of a POI: by who sees it (ACCESS_COLOR in render.mjs reads `access`).
const accessOf = (row) => {
  if (row.visibility === "PUBLIC") return "PUBLIC";
  if (row.visibility === "DISCOVER") return "DISCOVER";
  if (row.visibility === "HIDDEN") return "UNDERWORLD";
  return row.role === "police" ? "POLICE" : "CIVIC";
};

/** Pure: joins live places to kind rows. A place with no row is skipped; returns { pois, unjoined (kind -> count) }. */
export function joinPois(kinds, places, text = () => "") {
  const pois = [];
  const unjoined = {};
  for (const pl of places) {
    const row = kinds.get(pl.kind);
    if (!row) { unjoined[pl.kind] = (unjoined[pl.kind] || 0) + 1; continue; }
    pois.push({ id: pl.id, name: (text(row.label) || row.kind) + (pl.detail ? ` (${pl.detail})` : ""), kind: row.kind, category: row.category, visibility: row.visibility, role: row.role, access: accessOf(row), x: pl.x, z: pl.z, minZoom: row.minZoom, live: pl.live, state: pl.state, heardOf: false, source: pl.source, file: pl.file });
  }
  return { pois, unjoined };
}

/**
 * opts: { world (main|town|debug|grid|path), fixture (draw the stand-in rows instead), files, liveStates }.
 * Returns { pois, areas, source, info } where source is "places:<world>", "fixture" or "none" (no place has a POI
 * kind: nothing is drawn, and the caller says so), and info has the world, the counts and the unjoined kinds.
 */
export function loadOverlay(root, opts = {}) {
  const a = loadAreas(root, loadStrings(root));
  const info = { world: opts.world || DEFAULT_WORLD, places: 0, pois: 0, dark: 0, unjoined: {}, files: 0, kinds: 0, dupes: 0 };
  if (opts.fixture) {
    const p = new Map();
    const fx = parseConf(readFileSync(join(root, FIXTURE), "utf8"));
    for (const b of findBlocks(fx, "LF_MapPoi")) { const r = fixturePoiOf(b); p.set(r.id, r); }
    for (const b of findBlocks(fx, "LF_MapArea")) { const r = fixtureAreaOf(b); a.set(r.id, r); }
    return { pois: [...p.values()], areas: [...a.values()], source: "fixture", info };
  }
  const kinds = loadKinds(root);
  const found = loadPlaces(root, { world: opts.world, files: opts.files, liveStates: opts.liveStates });
  const { pois, unjoined } = joinPois(kinds, found.places, loadStrings(root));
  Object.assign(info, { world: found.world, places: found.places.length, pois: pois.length, dark: pois.filter((p) => !p.live).length, unjoined, files: found.files.length, kinds: kinds.size, dupes: found.dupes, liveStates: found.liveStates, extent: extentOf(pois.length ? pois : found.places) });
  return { pois, areas: [...a.values()], source: pois.length ? `places:${found.world}` : "none", info };
}

const extentOf = (items) => (items.length ? { x0: Math.min(...items.map((i) => i.x)), z0: Math.min(...items.map((i) => i.z)), x1: Math.max(...items.map((i) => i.x)), z1: Math.max(...items.map((i) => i.z)) } : null);

let catalogCache = null;
export function loadCatalog(root) {
  if (catalogCache?.root === root) return catalogCache.data;
  const j = JSON.parse(readFileSync(join(root, "docs/everon/catalog-buildings.json"), "utf8"));
  const buildings = j.instances.map(([pi, x, z, yaw]) => ({ x, z, yaw, type: j.prefabs[pi]?.type || "house", fp: j.prefabs[pi]?.footprintM || [6, 6] }));
  const data = { places: j.places, natural: j.naturalPlaces, buildings };
  catalogCache = { root, data };
  return data;
}

// What a player sees of one POI: "full", "faded" (heard of), "pencil" (a private mark) or "hidden".
// The visibility rule module (M2) overrides this when it exists; this is the D-M3..D-M6 reading of the design.
export function personaVisibility(poi, persona) {
  const acc = poi.access;
  if (acc === "PUBLIC" || acc === "CIVIC") return "full";
  const [who, org] = persona.split(":");
  if (acc === "DISCOVER") {
    if (who === "new") return poi.heardOf ? "faded" : "hidden";
    return who === "org" ? "faded" : "full";
  }
  if (acc === "POLICE") return who === "police" ? "full" : "hidden";
  if (acc === "UNDERWORLD") return who === "known" ? "pencil" : "hidden";
  if (acc.startsWith("ORG:")) return who === "org" && acc.slice(4) === org ? "full" : "hidden";
  return "hidden";
}
