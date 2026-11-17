// tools/scout/catalog.mjs - the Everon building catalog (docs/everon/catalog-buildings.json, built from an in-engine dump of
// the whole island) as scout's source of buildings. The catalog already says, for every building instance on the island,
// which prefab it is, its type, footprint, floors, whether it has an interior and how many doors, so a dossier reads it
// instead of measuring and classifying again. The catalog's building type names are the dossier's (docs/everon/README.md).
//
//   catalog-buildings.json: { meta, types, prefabs: [{ res, path, type, footprintM: [x, z], areaM2, heightM, basementM,
//     floors, doorChildren, enterable, ... }], places, instances: [[prefabIndex, x, z, yawDeg, 0], ...] }
//   <place>.json (docs/everon/saint-philippe.json): { buildings: [{ type, prefab, res, x, z, yawDeg, propsInside, topProps }] },
//     the same buildings with what stands inside them; optional enrichment, matched by prefab and position.
import { existsSync, readFileSync } from "node:fs";
import { join } from "node:path";

export const CATALOG_FILE = join("docs", "everon", "catalog-buildings.json");

// The catalog (parsed) or null when it is not in the checkout. `file` overrides the path (tests, `--catalog`).
export function loadCatalog(root, file) {
  const f = file ? file : join(root, CATALOG_FILE);
  if (!existsSync(f)) return null;
  const c = JSON.parse(readFileSync(f, "utf8"));
  return c.prefabs && c.instances ? c : null;
}

// Pure: the catalog's buildings within `radius` of (x, z): { res, path, stem, type, x, z, yaw, footprint: [w, d], area, height, basement, floors, doorChildren, enterable, measured }.
export function catalogBuildings(cat, x, z, radius) {
  const out = [];
  for (const [pi, bx, bz, yaw] of cat.instances) {
    if (Math.hypot(bx - x, bz - z) > radius) continue;
    const p = cat.prefabs[pi];
    if (!p) continue;
    out.push({
      res: p.res,
      path: p.path,
      stem: p.path.split("/").pop().replace(/\.et$/i, ""),
      type: p.type,
      x: bx,
      z: bz,
      yaw,
      footprint: p.footprintM,
      area: p.areaM2,
      height: p.heightM,
      basement: p.basementM ?? 0,
      floors: p.floors,
      floorLevelsY: p.floorLevelsY ?? [],
      doorChildren: p.doorChildren ?? 0,
      enterable: !!p.enterable,
      measured: p.measured !== false,
    });
  }
  return out;
}

// Pure: place file buildings keyed for matching: the entry within `tol` metres of (x, z) with the same prefab path, or null.
export function matchByPrefab(list, path, x, z, tol = 1.5) {
  let best = null;
  for (const b of list) {
    const p = (b.path ?? String(b.res ?? "").replace(/^\{[0-9A-Fa-f]{16}\}/, "")).toLowerCase();
    if (p !== path.toLowerCase()) continue;
    const d = Math.hypot(b.x - x, b.z - z);
    if (d <= tol && (!best || d < best.d)) best = { d, b };
  }
  return best?.b ?? null;
}

// The `<slug>.json` place file beside the catalog, or null.
export function loadPlaceFile(root, slug, dir = join("docs", "everon")) {
  const f = join(root, dir, `${slug}.json`);
  if (!existsSync(f)) return null;
  try {
    const j = JSON.parse(readFileSync(f, "utf8"));
    return Array.isArray(j.buildings) ? j : null;
  } catch {
    return null;
  }
}
