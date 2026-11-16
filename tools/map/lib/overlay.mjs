// tools/map/lib/overlay.mjs - the data the map draws, read from the same confs the game reads.
// POIs and areas: addons/LifeFramework/Configs/Map/LF_MapPois.conf and LF_MapAreas.conf, then the pack
// overlay addons/LF_Everon1989/Configs/Map/ (same file names; a row with the same Id replaces the core row).
// Until M2 and M5 land those files do not exist and tools/map/render/fixtures/map-overlay.fixture.conf stands in.
// Places (town labels) and buildings: docs/everon/catalog-buildings.json, the dump of the real Everon world.
import { existsSync, readFileSync } from "node:fs";
import { join } from "node:path";
import { findBlocks, num, parseConf, str, vecXZ } from "./conf.mjs";

const CONF_DIRS = ["addons/LifeFramework/Configs/Map", "addons/LF_Everon1989/Configs/Map"];
const FIXTURE = "tools/map/render/fixtures/map-overlay.fixture.conf";

const poiOf = (b) => {
  const [x, z] = vecXZ(b.props.Pos ?? "0 0 0");
  return { id: str(b.props.Id, b.name), name: str(b.props.Name), kind: str(b.props.Kind, "PLACE").toUpperCase(), access: str(b.props.Access, "PUBLIC"), x, z, minZoom: num(b.props.MinZoom, 3), heardOf: num(b.props.HeardOf, 0) === 1 };
};
const areaOf = (b) => {
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

function readAll(root, name, cls, mk) {
  const rows = new Map();
  for (const d of CONF_DIRS) {
    const f = join(root, d, name);
    if (!existsSync(f)) continue;
    for (const b of findBlocks(parseConf(readFileSync(f, "utf8")), cls)) {
      const r = mk(b);
      rows.set(r.id, r);
    }
  }
  return rows;
}

export function loadOverlay(root) {
  const p = readAll(root, "LF_MapPois.conf", "LF_MapPoi", poiOf);
  const a = readAll(root, "LF_MapAreas.conf", "LF_MapArea", areaOf);
  let source = "game confs";
  if (!p.size && !a.size) {
    source = "fixture";
    const fx = parseConf(readFileSync(join(root, FIXTURE), "utf8"));
    for (const b of findBlocks(fx, "LF_MapPoi")) {
      const r = poiOf(b);
      p.set(r.id, r);
    }
    for (const b of findBlocks(fx, "LF_MapArea")) {
      const r = areaOf(b);
      a.set(r.id, r);
    }
  }
  return { pois: [...p.values()], areas: [...a.values()], source };
}

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
