// tools/map/lib/places.mjs - the live places of a world, read from its `.layer` files.
//
// The POI table (Configs/Map/LF_MapPois.conf) is per place KIND and holds no coordinates (AGENTS.md rule 9, map
// independence). A place is an entity that registers in LF_Places under a kind and an id; the layers are where
// each one stands. This module finds them the way the game does and returns { kind, id, x, z, state, live }.
//
// Which components register a place. LF_PlaceComponent is the generic one (kind, id, order and live state are
// attributes). A few feature components register a fixed kind of their own; list one here when its kind has a row
// in the POI table, or it never reaches the map. Each mirrors an `LF_Places.Register(<kind>, <id>, owner)` call:
//   LF_WorkplaceComponent       Jobs/LF_WorkplaceComponent.c        kind Workplace,    id m_sPlaceId or the name (detail: its job)
//   LF_HospitalTillComponent    Crime/LF_HospitalSite.c             kind HospitalSite, id m_sTargetId or the name
//   LF_HospitalCabinetComponent Crime/LF_HospitalSite.c             kind HospitalSite, id m_sTargetId or the name
// Live state (Core/LF_PlaceLiveState.c): a place with state >= 0 registers only while the mission header's
// m_aLF_LiveStates lists it (an empty or missing list: every state is live). Dedicated components are always live.
import { existsSync, readFileSync } from "node:fs";
import { basename, isAbsolute, join, resolve as resolvePath } from "node:path";
import { layerFiles, readLayers } from "./layers.mjs";

export const PLACE_SOURCES = {
  LF_PlaceComponent: { kindAttr: "m_sKind", idAttr: "m_sId", orderAttr: "m_iOrder", stateAttr: "m_iLiveState" },
  LF_WorkplaceComponent: { kind: "Workplace", idAttr: "m_sPlaceId", detailAttr: "m_sJob" },
  LF_HospitalTillComponent: { kind: "HospitalSite", idAttr: "m_sTargetId" },
  LF_HospitalCabinetComponent: { kind: "HospitalSite", idAttr: "m_sTargetId" },
};

// --world names. main: the pack's island. town: what the playtest mission loads around the village (the
// DebugWorld layers and the pack's Everon1989Town layers). debug: DebugWorld alone. grid: the fenced GridWorld.
export const WORLDS = {
  main: ["addons/LF_Everon1989/Worlds/MainWorld/MainWorld_Layers"],
  town: ["addons/LifeFramework/Worlds/DebugWorld/DebugWorld_Layers", "addons/LF_Everon1989/Worlds/Everon1989Town/Everon1989Town_Layers"],
  debug: ["addons/LifeFramework/Worlds/DebugWorld/DebugWorld_Layers"],
  grid: ["addons/LifeFramework/Worlds/GridWorld/GridWorld_Layers"],
};
export const DEFAULT_WORLD = "main";
const HEADER = "addons/LF_Everon1989/Missions/LF_Everon1989.conf";

/** The layer files `--world` names: a world name, a layers folder, a world folder, or one .layer file. */
export function worldFiles(root, world = DEFAULT_WORLD) {
  if (WORLDS[world]) return WORLDS[world].flatMap((d) => layerFiles(join(root, d)));
  const p = isAbsolute(world) ? world : resolvePath(root, world);
  if (existsSync(p) && p.endsWith(".layer")) return [p];
  for (const d of [p, join(p, `${basename(p)}_Layers`)]) {
    const f = layerFiles(d);
    if (f.length) return f;
  }
  throw new Error(`unknown --world ${world} (${Object.keys(WORLDS).join(", ")}, a layers folder or a .layer file)`);
}

/** The pack header's m_aLF_LiveStates as numbers; [] (every state live) when the header lists none. */
export function headerLiveStates(root) {
  const f = join(root, HEADER);
  if (!existsSync(f)) return [];
  const m = readFileSync(f, "utf8").match(/m_aLF_LiveStates\s*\{([^}]*)\}/);
  return m ? m[1].split(/[\s,]+/).filter(Boolean).map(Number).filter(Number.isFinite) : [];
}

/** Pure: LF_PlaceLiveState.IsLive. */
export const isLive = (state, liveStates) => state < 0 || !liveStates || liveStates.length === 0 || liveStates.includes(state);

/**
 * Places of a world. opts: { world, files (override the file list), liveStates (override the header's) }.
 * Returns { world, files, places: [{ kind, id, x, z, state, live, source, entity }], dupes }.
 */
export function loadPlaces(root, opts = {}) {
  const world = opts.world || DEFAULT_WORLD;
  const files = opts.files || worldFiles(root, world);
  const liveStates = opts.liveStates || headerLiveStates(root);
  const places = [];
  const seen = new Set();
  let dupes = 0;
  for (const e of readLayers(root, files)) {
    for (const [cls, src] of Object.entries(PLACE_SOURCES)) {
      const p = e.comps[cls];
      if (!p) continue;
      const kind = src.kind || p[src.kindAttr] || "";
      let id = p[src.idAttr] || e.name;
      const order = src.orderAttr ? Number(p[src.orderAttr] ?? -1) : -1;
      if (order >= 0) id = `${id}#${order}`;
      if (!kind || !id) continue; // the game logs an error and registers nothing
      const key = `${kind}|${id}`;
      if (seen.has(key)) { dupes++; continue; }
      seen.add(key);
      const state = src.stateAttr ? Number(p[src.stateAttr] ?? -1) : -1;
      places.push({ kind, id, x: e.xz[0], z: e.xz[1], state, live: isLive(state, liveStates), detail: src.detailAttr ? p[src.detailAttr] || "" : "", source: cls, entity: e.name, file: basename(e.file) });
    }
  }
  return { world, files, places, dupes, liveStates };
}
