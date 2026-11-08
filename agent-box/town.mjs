// Town manifest: every M1 site a player must find, what look must offer there,
// and which sign explains it. Positions are NOT here. Each site names an entity
// (or a prefab, for unnamed placements such as spawn points) and resolves
// against the DebugWorld layers, so moving a site in a layer (M1-25) needs no
// manifest edit. `checkTown` is the offline half: it runs in selftest and in
// `tools\cli validate`. `report town` (report.mjs) is the play-mode half.

import { existsSync, readFileSync } from "node:fs";
import { join } from "node:path";
import { loadWorld } from "./layers.mjs";

// requireSigns: false until M1-09 (#94) places the signs. While false, a sign
// that is missing, farther than signRadiusM, or has a blank card is a warning
// ("sign: pending"). M1-09 sets it to true and the same checks fail the row.
//
// Site fields:
//   id       stable key, used in report rows
//   entity   layer entity name; must resolve to exactly one placed entity
//   prefab   instead of entity: prefab path; every placement counts, the site
//            sits at their centre (spawn points are unnamed)
//   actions  ScriptedUserAction classes look must offer on that entity. The
//            layer (or its addon prefab) must carry them too
//   sign     LF_Signs.conf id of the sign that explains the site, or null
//   step     player-journey step in docs/roadmap/m1-playtest-town.md section a
//   outskirts true for a site that stands off the street by design (farm,
//            quarry, the contraband barn out of town): the neighbour rule skips
//            it (docs/roadmap/m1-25-village.md)
export const TOWN = {
  requireSigns: true,
  signRadiusM: 8,
  neighbourM: 30,
  sites: [
    // The civilian town spawn on the square. The other spawn points (mine,
    // police) are elsewhere, and their centre is not a place a player stands.
    { id: "spawn", entity: "Spawn_Town", actions: [], sign: "town_map", step: 4 },
    // Arrival step 1 on the square; the town map board explains it. M1-18 swaps in the clerk prefab.
    { id: "clerk", prefab: "Prefabs/Characters/Quests/QuestGiver_Start.et", actions: ["LF_QuestAction"], sign: null, step: 5 },
    { id: "post_office", entity: "Roadmap_Job_Depot", actions: ["LF_JobSignUpAction", "LF_JobStartRunAction"], sign: "post_office", step: 6 },
    { id: "job_drop", entity: "Roadmap_Job_Drop", actions: ["LF_JobDropoffAction"], sign: null, step: 7 },
    { id: "traders", entity: "TraderChainCounter", actions: ["EL_OpenTraderAction"], sign: "traders", step: 7 },
    { id: "atm", entity: "ATM_Debug", actions: ["EL_ATMAction"], sign: "atm", step: 8 },
    { id: "general_store", entity: "Shop_GeneralStore_Debug", actions: ["EL_ShopAction"], sign: "general_store", step: 9 },
    { id: "black_market", entity: "Shop_BlackMarket_Debug", actions: ["EL_ShopAction"], sign: "black_market", step: 9 },
    { id: "vehicle_lot", entity: "Roadmap_Vehicle_Lot", actions: ["EL_VehicleBuyAction"], sign: "vehicle_lot", step: 10 },
    { id: "fuel_station", entity: "Roadmap_Fuel_Till", actions: ["EL_RobAction"], sign: "fuel_station", step: 11 },
    { id: "hospital_till", entity: "Roadmap_Hospital_Till", actions: ["EL_RobAction"], sign: null, step: 11 },
    { id: "bank", entity: "Roadmap_Bank_Vault", actions: ["LF_BankRobAction"], sign: "bank", step: 12 },
    { id: "hospital", entity: "Roadmap_Hospital_Cabinet", actions: ["LF_HospitalCabinetAction"], sign: "hospital", step: 13 },
    { id: "convoy_pickup", entity: "Roadmap_Convoy_Start", actions: ["LF_ConvoyStartAction"], sign: "convoy_pickup", step: 14, outskirts: true },
    { id: "convoy_drop", entity: "Roadmap_Convoy_Drop", actions: ["LF_ConvoyDeliverAction"], sign: "convoy_drop", step: 14 },
    // The duty desk (Clock in / Clock out) and the wanted board stand in front of the station (M1-11).
    { id: "police_station", entity: "Roadmap_Police_Desk", actions: ["EL_DutyAction"], sign: "police_station", step: 15 },
    { id: "wanted_board", entity: "Roadmap_Police_WantedBoard", actions: ["LF_WantedBoardReadAction"], sign: null, step: 17 },
    { id: "police_armory", entity: "Roadmap_Police_Armory", actions: ["LF_ArmoryDrawAction"], sign: null, step: 15 },
    { id: "escort_car", entity: "Roadmap_Escort_Car", actions: ["LF_JailPutInVehicleAction"], sign: null, step: 18 },
    // Arrest is a character action; the cell itself offers nothing to press.
    { id: "jail", entity: "Roadmap_Jail_Cell", actions: [], sign: "jail", step: 20 },
    { id: "org_registry", entity: "Roadmap_Org_Desk", actions: ["EL_OrganizationDeskAction"], sign: "org_registry", step: 21 },
    { id: "hideout", entity: "Roadmap_Hideout_Machine", actions: ["EL_ProcessAction"], sign: "hideout", step: 22 },
    { id: "garage", entity: "Roadmap_Garage", actions: ["EL_VehicleStoreAction", "EL_VehicleRetrieveAction", "LF_GarageLedgerAction"], sign: "garage", step: 23 },
    { id: "impound", entity: "Roadmap_Impound_Desk", actions: ["LF_ImpoundCollectAction", "LF_GarageLedgerAction"], sign: "impound", step: 23 },
    { id: "mining", entity: "Furnace", actions: ["EL_ProcessAction"], sign: "mining", step: 7, outskirts: true },
    { id: "farm", entity: "TomatoCrop_Debug", actions: ["EL_GatherAction"], sign: "farm", step: 7, outskirts: true },
  ],
};

const SIGN_FIELDS = ["m_sTitle", "m_sWhat", "m_sHow", "m_sNeeds", "m_sRules", "m_sNext"];

function round(value) {
  return Math.round(value * 1000) / 1000;
}

// Shape errors in a manifest. An empty list means every site can be resolved.
export function manifestErrors(manifest) {
  const errors = [];
  if (!manifest || !Array.isArray(manifest.sites) || !manifest.sites.length) return ["manifest has no sites"];
  if (typeof manifest.requireSigns !== "boolean") errors.push("requireSigns must be true or false");
  if (!(manifest.signRadiusM > 0)) errors.push("signRadiusM must be a positive number");
  if (!(manifest.neighbourM > 0)) errors.push("neighbourM must be a positive number");
  const ids = new Set();
  for (const site of manifest.sites) {
    const label = site?.id || "(no id)";
    if (!site?.id) errors.push("a site has no id");
    else if (ids.has(site.id)) errors.push(`duplicate site id ${site.id}`);
    ids.add(site?.id);
    if (!site?.entity === !site?.prefab) errors.push(`${label}: name exactly one of entity or prefab`);
    if (!Array.isArray(site?.actions)) errors.push(`${label}: actions must be a list`);
    if (site?.sign !== null && (typeof site?.sign !== "string" || !site.sign)) errors.push(`${label}: sign is an LF_Signs.conf id or null`);
    if (!Number.isInteger(site?.step)) errors.push(`${label}: step must be a journey step number`);
    if ("x" in (site || {}) || "z" in (site || {}) || "coords" in (site || {})) {
      errors.push(`${label}: positions come from the layers, not the manifest`);
    }
  }
  return errors;
}

function samePrefab(a, b) {
  return a.replaceAll("\\", "/").toLowerCase() === b.replaceAll("\\", "/").toLowerCase();
}

// One site against the placed entities. Returns the position the report uses.
export function resolveSite(site, world) {
  const errors = [];
  if (site.entity) {
    const hits = world.filter((entity) => entity.name === site.entity);
    if (!hits.length) return { ok: false, errors: [`${site.entity} is not in any DebugWorld layer`] };
    if (hits.length > 1) {
      const where = hits.map((hit) => hit.layer).join(", ");
      return { ok: false, errors: [`${site.entity} is placed ${hits.length} times (${where})`] };
    }
    const hit = hits[0];
    for (const action of site.actions) {
      if (!hit.actions.includes(action)) errors.push(`${site.entity} in ${hit.layer} does not carry ${action}`);
    }
    return { ok: !errors.length, errors, name: hit.name, layer: hit.layer, x: hit.x, y: hit.y, z: hit.z, count: 1 };
  }
  const hits = world.filter((entity) => entity.prefab && samePrefab(entity.prefab, site.prefab));
  if (!hits.length) return { ok: false, errors: [`no placement of ${site.prefab}`] };
  for (const action of site.actions) {
    if (!hits.every((hit) => hit.actions.includes(action))) errors.push(`a ${site.prefab} placement does not carry ${action}`);
  }
  const at = (key) => round(hits.reduce((sum, hit) => sum + hit[key], 0) / hits.length);
  const layers = [...new Set(hits.map((hit) => hit.layer))].join(", ");
  return { ok: !errors.length, errors, name: "", layer: layers, x: at("x"), y: at("y"), z: at("z"), count: hits.length };
}

// Localization table: key -> en_us text.
export function loadStrings(root) {
  const file = join(root, "addons", "LifeFramework", "Language", "everonlife_localization.st");
  const strings = new Map();
  if (!existsSync(file)) return strings;
  let id = "";
  for (const raw of readFileSync(file, "utf8").split(/\r?\n/)) {
    const line = raw.trim();
    const key = line.match(/^Id\s+"((?:[^"\\]|\\.)*)"$/);
    if (key) {
      id = key[1];
      continue;
    }
    const text = line.match(/^Target_en_us\s+"((?:[^"\\]|\\.)*)"$/);
    if (text && id) strings.set(id, text[1]);
  }
  return strings;
}

// LF_Signs.conf: sign id -> raw fields (a value may be a #key).
export function parseSignConfig(text) {
  const signs = new Map();
  let current = null;
  for (const raw of text.split(/\r?\n/)) {
    const line = raw.trim();
    if (/^LF_SignDef\b.*\{$/.test(line)) {
      current = {};
      continue;
    }
    if (!current) continue;
    const field = line.match(/^(m_s[A-Za-z]+)\s+"((?:[^"\\]|\\.)*)"$/);
    if (field) {
      current[field[1]] = field[2];
      continue;
    }
    if (line === "}") {
      if (current.m_sId) signs.set(current.m_sId, current);
      current = null;
    }
  }
  return signs;
}

export function loadSignCatalog(root) {
  const file = join(root, "addons", "LifeFramework", "Configs", "Signs", "LF_Signs.conf");
  if (!existsSync(file)) return new Map();
  return parseSignConfig(readFileSync(file, "utf8"));
}

// What the Read card shows, resolved the way LF_SignCatalog.Resolve does. A
// card is blank when it has no title or no section text.
export function signCard(id, catalog, strings) {
  const def = catalog.get(id);
  if (!def) return { found: false, blank: true, title: "", sections: 0 };
  const resolved = {};
  for (const field of SIGN_FIELDS) {
    const value = (def[field] || "").trim();
    resolved[field] = value.length > 1 && value.startsWith("#") ? (strings.get(value.slice(1)) || "").trim() : value;
  }
  const sections = ["m_sWhat", "m_sHow", "m_sNeeds", "m_sRules", "m_sNext"].filter((field) => resolved[field]).length;
  return { found: true, blank: !resolved.m_sTitle || sections === 0, title: resolved.m_sTitle, sections };
}

// The nearest placed sign with this id to a point, or null.
export function nearestSign(signId, point, world) {
  let best = null;
  for (const entity of world) {
    if (entity.signId !== signId) continue;
    const distanceM = round(Math.hypot(entity.x - point.x, entity.z - point.z));
    if (!best || distanceM < best.distanceM) best = { name: entity.name, layer: entity.layer, x: entity.x, z: entity.z, distanceM };
  }
  return best;
}

// Sign problems for one resolved site. `card` may be the live Read card
// verdict ("blank" or "ok") from play mode; otherwise the config decides.
export function signIssues(site, point, world, catalog, strings, radiusM, live = {}) {
  if (!site.sign) return { issues: [], sign: null };
  const sign = nearestSign(site.sign, point, world);
  const issues = [];
  if (!sign) issues.push(`no ${site.sign} sign placed`);
  else if (sign.distanceM > radiusM) issues.push(`${site.sign} sign is ${sign.distanceM} m away (max ${radiusM})`);
  const card = signCard(site.sign, catalog, strings);
  if (!card.found) issues.push(`${site.sign} is not in LF_Signs.conf`);
  else if (card.blank) issues.push(`${site.sign} Read card is blank`);
  if (live.read === false && sign) issues.push(`look does not offer Read on ${sign.name || "the sign"}`);
  if (live.card === "blank" && card.found && !card.blank) issues.push(`${site.sign} Read card shows blank in play`);
  return { issues, sign, card };
}

export function nearestNeighbour(index, points) {
  let best = Infinity;
  for (let i = 0; i < points.length; i++) {
    if (i === index) continue;
    best = Math.min(best, Math.hypot(points[i].x - points[index].x, points[i].z - points[index].z));
  }
  return Number.isFinite(best) ? round(best) : 0;
}

// Offline check: every site resolves to placed entities with the expected
// actions, sits within neighbourM of another site, and has its sign (or a
// pending warning while requireSigns is false).
export function checkTown(root, { manifest = TOWN, world, catalog, strings } = {}) {
  const errors = [];
  const warnings = [];
  const shape = manifestErrors(manifest);
  if (shape.length) return { ok: false, errors: shape, warnings, rows: [] };
  world = world || loadWorld(root);
  catalog = catalog || loadSignCatalog(root);
  strings = strings || loadStrings(root);
  const rows = manifest.sites.map((site) => ({ site, place: resolveSite(site, world) }));
  const placed = rows.filter((row) => row.place.errors.length === 0 || row.place.x != null);
  for (const row of rows) {
    for (const error of row.place.errors) errors.push(`${row.site.id}: ${error}`);
  }
  for (const row of placed) {
    const index = placed.indexOf(row);
    row.neighbourM = nearestNeighbour(index, placed.map((item) => item.place));
    if (row.neighbourM > manifest.neighbourM && !row.site.outskirts) {
      errors.push(`${row.site.id}: ${row.neighbourM} m from the nearest other site (max ${manifest.neighbourM})`);
    }
    const signed = signIssues(row.site, row.place, world, catalog, strings, manifest.signRadiusM);
    row.sign = signed.sign;
    row.signIssues = signed.issues;
    for (const issue of signed.issues) {
      if (manifest.requireSigns) errors.push(`${row.site.id}: ${issue}`);
      else warnings.push(`${row.site.id}: sign: pending (${issue})`);
    }
  }
  return { ok: errors.length === 0, errors, warnings, rows };
}

export function formatTownCheck(result) {
  const lines = [];
  for (const row of result.rows) {
    const place = row.place;
    const where = place.x == null ? "unresolved" : `${place.layer} at ${place.x} ${place.z}`;
    const sign = row.site.sign ? (row.signIssues?.length ? "sign pending" : `sign ${row.sign.name || "(unnamed)"} ${row.sign.distanceM} m`) : "no sign";
    lines.push(`  ${row.site.id}  ${row.site.entity || row.site.prefab}  ${where}  ${sign}`);
  }
  for (const warning of result.warnings) lines.push(`warn ${warning}`);
  for (const error of result.errors) lines.push(`FAIL ${error}`);
  return lines.join("\n");
}

// Which expected actions look did not offer on this entity.
export function missingActions(expected, entityName, look) {
  const text = (look ?? "").trim();
  const offered = new Set();
  if (text && text !== "none") {
    for (const part of text.split(",")) {
      const hit = part.trim();
      const at = hit.lastIndexOf("@");
      if (at < 0) continue;
      if (entityName && hit.slice(at + 1) !== entityName) continue;
      offered.add(hit.slice(0, at));
    }
  }
  return expected.filter((action) => !offered.has(action));
}

// One play-mode row. `row` carries the live look, mesh, menu, and the offline
// resolve and sign results; `requireSigns` decides whether sign issues fail.
export function judgeTownRow(row, { requireSigns = TOWN.requireSigns, neighbourM = TOWN.neighbourM } = {}) {
  const failures = [...(row.resolveErrors || [])];
  const warnings = [];
  if (row.entity && row.actions?.length) {
    if (!(row.look ?? "").trim() || row.look === "none") failures.push("look offers no interaction");
    else {
      const missing = missingActions(row.actions, row.entity, row.look);
      if (missing.length) failures.push(`look lacks ${missing.join(", ")}`);
    }
  }
  if (row.menu != null) {
    const menu = String(row.menu).trim();
    const widgets = Boolean(menu) && menu !== "none" && menu !== "blank";
    const shot = row.menuMesh == null || row.menuMesh === "ok" || row.menuMesh === "painted-light";
    if (!widgets || !shot) failures.push("menu is blank");
  }
  if (row.mesh === "white" || row.mesh === "missing") failures.push("site is white or missing");
  if (Number(row.neighbourM) > neighbourM && !row.outskirts) failures.push(`more than ${neighbourM} m from its nearest neighbour`);
  for (const issue of row.signIssues || []) {
    if (requireSigns) failures.push(`sign: ${issue}`);
    else warnings.push(`sign: pending (${issue})`);
  }
  return { ok: failures.length === 0, failures, warnings };
}
