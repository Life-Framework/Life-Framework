// The config layers of a server, read offline: the core .conf, each pack overlay in load order, the
// owner's config.json. `pack check` and `pack explain` (#969) both read through here.
//
// The merge mirrors LF_ConfigOverlay.Apply (docs/config/override-model.md): add, replace, patch,
// remove and "no verb" by id; a refused verb is an event, not an exception; a second pack on an id
// an earlier pack changed is a conflict. Fields are named the way the owner file names them
// (`m_iPrice` is `price`, tools/config/schema.mjs jsonKey).
//
// SECTIONS is the one table of list sections. selftest tools/validation/pack-check-selftest.mjs
// fails when a LF_ConfigList subclass in the core has no row here.
import { existsSync, readdirSync, readFileSync, statSync } from "node:fs";
import { join } from "node:path";
import { parseConf } from "../economy/profit-loops.mjs";
import { jsonKey } from "../config/schema.mjs";
import { parseShopRuleKey, prefabKey, typeKey } from "../config/rules.mjs";

export const CORE = "LifeFramework";
export const VERBS = ["add", "replace", "patch", "remove"];

const shopKey = (p) => {
  if (p.m_Prefab) return prefabKey(p.m_Prefab);
  if (p.m_sFind) return p.m_sFind;
  if (typeof p.m_eItemType === "number" && p.m_eItemType !== 0) return typeKey(p.m_eItemType, typeof p.m_eItemMode === "number" ? p.m_eItemMode : 2);
  return "";
};

// list: "section.list" as the owner file spells it. id(): the id of a parsed .conf record.
// ownerKey(): an owner-file key in the form id() returns, or "" when it cannot be an id.
export const SECTIONS = {
  "catalogue.items": { section: "catalogue", cls: "LF_ItemCatalogueConfig", core: "Configs/Catalogue/LF_ItemCatalogue.conf", field: "m_aItems", key: "items", id: (p) => p.m_sId ?? "", ownerKey: (k) => k },
  "catalogue.offers": { section: "catalogue", cls: "LF_ItemCatalogueConfig", core: "Configs/Catalogue/LF_ItemCatalogue.conf", field: "m_aOffers", key: "offers", id: (p) => (p.m_sOutlet && p.m_sItem ? `${p.m_sOutlet}/${p.m_sItem}` : ""), ownerKey: (k) => k },
  "crime.crimes": { section: "crime", cls: "LF_CrimeCatalogConfig", core: "Configs/Crime/LF_CrimeCatalog.conf", field: "m_aCrimes", key: "crimes", id: (p) => p.m_sCrimeId ?? "", ownerKey: (k) => k },
  "vehicles.vehicles": { section: "vehicles", cls: "LF_VehiclePricesConfig", core: "Configs/Vehicles/LF_VehiclePrices.conf", field: "m_aVehicles", key: "vehicles", id: (p) => guidOf(p.m_Prefab), ownerKey: (k) => /^{?[0-9A-Fa-f]{16}}?$/.test(k) ? k.replace(/[{}]/g, "").toUpperCase() : "" },
  "sites.rules": { section: "sites", cls: "LF_SitesConfig", core: "Configs/Sites/LF_Sites.conf", field: "m_aRules", key: "rules", id: (p) => p.m_sHideoutId ?? "", ownerKey: (k) => k },
  "shops.priceRules": { section: "shops", cls: "LF_ShopPricesConfig", core: "Configs/Shop/ShopPrices.conf", field: "m_aPriceRules", key: "priceRules", id: shopKey, ownerKey: (k) => parseShopRuleKey(k)?.norm ?? "" },
};
// ---- The remaining core list sections (#2804) ----
// row(): idField names a record, so it is not listed as a field a layer sets (idFields below). A list with
// no owner section is read from pack overlays only: its ownerKey says no key can be an id.
const noOwner = () => "";
const same = (k) => k;
const byField = (f) => (p) => p[f] ?? "";
const ZONE_RULES = ["NORMAL", "SAFE", "LAWLESS"];
const zoneRule = (p) => (typeof p.m_eRule === "number" ? ZONE_RULES[p.m_eRule] ?? "" : p.m_eRule === undefined ? "" : String(p.m_eRule));
const row = (section, key, cls, core, field, idField, id, ownerKey = noOwner) => ({ section, cls, core, field, key, idFields: [idField], id, ownerKey });
Object.assign(SECTIONS, {
  "prefabs.slots": row("prefabs", "slots", "LF_PrefabSlotsConfig", "Configs/PrefabSlots/LF_PrefabSlots.conf", "m_aSlots", "m_sSlot", byField("m_sSlot")),
  "sounds.slots": row("sounds", "slots", "LF_SoundSlotsConfig", "Configs/SoundSlots/LF_SoundSlots.conf", "m_aSlots", "m_sSlot", byField("m_sSlot")),
  "safeTypes.types": row("safeTypes", "types", "LF_SafeTypeConfig", "Configs/Crime/LF_SafeTypes.conf", "m_aTypes", "m_sId", byField("m_sId")),
  "heist.sites": row("heist", "sites", "LF_HeistConfig", "Configs/Crime/LF_Heist.conf", "m_aSites", "m_sId", byField("m_sId")),
  "dialogue.index": row("dialogue", "index", "LF_DialogueIndexConfig", "Configs/Dialogue/LF_DialogueIndex.conf", "m_aEntries", "m_sId", byField("m_sId")),
  "events.events": row("events", "events", "LF_EventsConfig", "Configs/Events/LF_Events.conf", "m_aEvents", "m_sEventId", byField("m_sEventId")),
  "jobs.tracks": row("jobs", "tracks", "LF_JobsConfig", "Configs/Jobs/LF_Jobs.conf", "m_aTracks", "m_sJob", byField("m_sJob"), same),
  "jobs.xpAwards": row("jobs", "xpAwards", "LF_JobsConfig", "Configs/Jobs/LF_Jobs.conf", "m_aXpAwards", "m_sAction", byField("m_sAction"), same),
  "jobs.licenceGates": row("jobs", "licenceGates", "LF_JobsConfig", "Configs/Jobs/LF_Jobs.conf", "m_aLicenceGates", "m_sLicence", byField("m_sLicence"), same),
  "jobs.runPays": row("jobs", "runPays", "LF_JobsConfig", "Configs/Jobs/LF_Jobs.conf", "m_aRunPays", "m_sJob", byField("m_sJob"), same),
  "mapAreas.areas": row("mapAreas", "areas", "LF_MapAreaConfig", "Configs/Map/LF_MapAreas.conf", "m_aAreas", "m_sId", byField("m_sId")),
  "mapPois.kinds": row("mapPois", "kinds", "LF_MapPoiConfig", "Configs/Map/LF_MapPois.conf", "m_aKinds", "m_sKind", byField("m_sKind")),
  "zones.ruleSets": row("zones", "ruleSets", "LF_ZoneConfig", "Configs/Moderation/LF_Zones.conf", "m_aRuleSets", "m_eRule", zoneRule, (k) => (ZONE_RULES.includes(k) ? k : "")),
  "organizations.rankDefaults": row("organizations", "rankDefaults", "LF_OrgConfig", "Configs/Organizations/LF_Org.conf", "m_aRankDefaults", "m_sRank", byField("m_sRank"), (k) => (["founder", "officer", "member"].includes(k) ? k : "")),
  "police.ranks": row("police", "ranks", "LF_PoliceConfig", "Configs/Police/LF_Police.conf", "m_aRanks", "m_sId", byField("m_sId"), same),
  "welcomeGift.gifts": row("welcomeGift", "gifts", "LF_WelcomeGiftConfig", "Configs/Quests/LF_WelcomeGift.conf", "m_aGifts", "m_sQuestId", byField("m_sQuestId")),
  "questGivers.givers": row("questGivers", "givers", "LF_QuestGiverConfig", "Configs/Quests/LF_QuestGiverTable.conf", "m_aGivers", "m_sGiverId", byField("m_sGiverId")),
  // The core ships no course (LF_RaceCourseBook starts empty), so the core layer is empty: a pack adds.
  "racing.courses": row("racing", "courses", "LF_RaceCoursesConfig", null, "m_aCourses", "m_sId", byField("m_sId"), same),
  "residents.residents": row("residents", "residents", "LF_ResidentsConfig", "Configs/Residents/LF_Residents.conf", "m_aResidents", "m_sId", byField("m_sId")),
  "handbook.pages": row("handbook", "pages", "LF_HandbookConfig", "Configs/Rules/LF_Handbook.conf", "m_aPages", "m_sId", byField("m_sId")),
  "signs.signs": row("signs", "signs", "LF_SignConfig", "Configs/Signs/LF_Signs.conf", "m_aSigns", "m_sId", byField("m_sId")),
  "signstyles.styles": row("signstyles", "styles", "LF_SignStyleConfig", "Configs/Signs/LF_SignStyles.conf", "m_aStyles", "m_sId", byField("m_sId")),
  "signstyles.sizes": row("signstyles", "sizes", "LF_SignStyleConfig", "Configs/Signs/LF_SignStyles.conf", "m_aSizes", "m_sId", byField("m_sId")),
  "drinkImpairment.drinks": row("drinkImpairment", "drinks", "LF_DrinkImpairmentConfig", "Configs/Survival/LF_DrinkImpairment.conf", "m_aDrinks", "m_Prefab", (p) => guidOf(p.m_Prefab), noOwner),
  "systems.catalog": row("systems", "catalog", "LF_SystemCatalog", "Configs/Systems/LF_SystemCatalog.conf", "m_aSystems", "m_sId", byField("m_sId")),
  "terms.terms": row("terms", "terms", "LF_TermsConfig", "Configs/Terms/LF_Terms.conf", "m_aTerms", "m_sId", byField("m_sId")),
  "qualifications.courses": row("qualifications", "courses", "LF_QualificationConfig", "Configs/Qualifications/LF_Qualifications.conf", "m_aCourses", "m_sId", byField("m_sId")),
  "property.plots": row("property", "plots", "LF_PropertyConfig", "Configs/Property/LF_Property.conf", "m_aPlots", "m_sPlotId", byField("m_sPlotId")),
  "property.houses": row("property", "houses", "LF_PropertyConfig", "Configs/Property/LF_Property.conf", "m_aHouses", "m_sHouseId", byField("m_sHouseId")),
  "loot.tables": row("loot", "tables", "LF_LootConfig", "Configs/Loot/LF_Loot.conf", "m_aTables", "m_sKind", byField("m_sKind"), noOwner),
  "market.goods": row("market", "goods", "LF_MarketConfig", "Configs/Market/LF_Market.conf", "m_aGoods", "m_sGood", (p) => guidOf(p.m_sGood), noOwner),
  // Keyed by buyer plus good (#3267); no owner section. The core ships no rows, so the core layer is empty: a pack adds.
  "buyerDemand.rows": { ...row("buyerDemand", "rows", "LF_BuyerDemandConfig", "Configs/Market/LF_BuyerDemand.conf", "m_aRows", "m_sBuyer", (p) => `${p.m_sBuyer ?? ""}|${guidOf(p.m_sGood)}`, noOwner), idFields: ["m_sBuyer", "m_sGood"] },
  // The core file lists no rung (LF_MedicRanks falls back to the built-in three in script), so the core layer is empty: a pack adds.
  "farming.crops": row("farming", "crops", "LF_CropCatalogue", "Configs/Farming/LF_Crops.conf", "m_aCrops", "m_sCropId", byField("m_sCropId"), noOwner),
  "medic.ranks": row("medic", "ranks", "LF_MedicConfig", null, "m_aRanks", "m_sId", byField("m_sId"), noOwner),
  "reputation.factions": row("reputation", "factions", "LF_ReputationConfig", "Configs/Reputation/LF_Reputation.conf", "m_aFactions", "m_sId", byField("m_sId")),
  "reputation.awards": row("reputation", "awards", "LF_ReputationConfig", "Configs/Reputation/LF_Reputation.conf", "m_aAwards", "m_sAction", byField("m_sAction")),
  "reputation.gates": row("reputation", "gates", "LF_ReputationConfig", "Configs/Reputation/LF_Reputation.conf", "m_aGates", "m_sGate", byField("m_sGate")),
  "reputation.pays": row("reputation", "pays", "LF_ReputationConfig", "Configs/Reputation/LF_Reputation.conf", "m_aPays", "m_sFaction", byField("m_sFaction")),
  "reputationContracts.contracts": row("reputationContracts", "contracts", "LF_ReputationContractsConfig", "Configs/Reputation/LF_ReputationContracts.conf", "m_aContracts", "m_sId", byField("m_sId")),
  "townBoard.cards": row("townBoard", "cards", "LF_TownBoardConfig", "Configs/TownBoard/LF_TownBoard.conf", "m_aCards", "m_sId", byField("m_sId")),
  "ui-theme.themes": row("ui-theme", "themes", "LF_UIThemeConfig", "Configs/UI/LF_UITheme.conf", "m_aThemes", "m_sThemeId", byField("m_sThemeId")),
  "parkedcars.cars": row("parkedcars", "cars", "LF_ParkedCarConfig", "Configs/Vehicles/LF_ParkedCars.conf", "m_aCars", "m_sPrefab", (p) => guidOf(p.m_sPrefab), noOwner),
  "licenceSeizure.rules": row("licenceSeizure", "rules", "LF_LicenceSeizureRules", "Configs/Crime/LF_LicenceSeizure.conf", "m_aRules", "m_sCrimeId", byField("m_sCrimeId")),
  "vehicle-colours.models": row("vehicle-colours", "models", "LF_VehicleColoursConfig", "Configs/Vehicles/LF_VehicleColours.conf", "m_aModels", "m_Prefab", (p) => guidOf(p.m_Prefab), noOwner),
});

// Lists the offline merge cannot model, each with the reason. selftest pack-check-selftest accepts a
// core list that is here instead of in SECTIONS; the runtime still merges it.
export const UNMODELLED = {
  "dialogue.nodes": "a conversation is one core .conf per NPC and a pack overlay is matched to it by its m_sId, so there is no single core file to merge into; validate-dialogue-patch.mjs checks each pack patch against its core dialogue with the runtime's rules",
  "dialogue.lines": "same as dialogue.nodes: the lines live in the per-NPC dialogue files, checked by validate-dialogue-patch.mjs",
  "licences.rules": "no core .conf: the licence catalog is built in script (LF_LicencesConfig.LoadFrom), and a rule's id must be a catalog name, which only the running game knows",
};

export const sectionNames = () => [...new Set(Object.values(SECTIONS).map((s) => s.section))];
export const sectionDef = (name) => Object.values(SECTIONS).find((s) => s.section === name);
export const classes = () => new Set(Object.values(SECTIONS).map((s) => s.cls));

export function walk(dir, out = []) {
  if (!existsSync(dir)) return out;
  for (const n of readdirSync(dir)) {
    if (n === "log" || n === ".git") continue;
    const p = join(dir, n);
    if (statSync(p).isDirectory()) walk(p, out);
    else out.push(p);
  }
  return out;
}

export const rel = (base, p) => p.slice(base.length + 1).split(/[\\/]/).join("/");
export const guidOf = (res) => /^\{([0-9A-Fa-f]{16})\}/.exec(String(res ?? ""))?.[1].toUpperCase() ?? "";
export const pathOfRes = (res) => String(res ?? "").replace(/^\{[0-9A-Fa-f]+\}/, "");

/** GUID -> { addon, path } for every .meta under addons/. */
export function metaIndex(addonsDir) {
  const idx = new Map();
  if (!existsSync(addonsDir)) return idx;
  for (const addon of readdirSync(addonsDir)) {
    const dir = join(addonsDir, addon);
    if (!statSync(dir).isDirectory()) continue;
    for (const f of walk(dir)) {
      if (!f.endsWith(".meta")) continue;
      const m = /Name\s+"\{([0-9A-Fa-f]{16})\}([^"]*)"/.exec(readFileSync(f, "utf8"));
      if (m) idx.set(m[1].toUpperCase(), { addon, path: m[2], file: rel(dir, f).replace(/\.meta$/, "") });
    }
  }
  return idx;
}

/** Fields of a parsed record (or section root): { name: value }, `op` and the id fields apart. */
export function fieldsOf(props, skip = []) {
  const out = {};
  for (const [k, v] of Object.entries(props)) {
    if (k === "m_sOp" || skip.includes(k)) continue;
    out[jsonKey(k)] = v;
  }
  return out;
}

/** Every string leaf of a parsed node, for GUID and string-key checks. */
export function strings(node, out = []) {
  if (typeof node === "string") out.push(node);
  else if (node && typeof node === "object") {
    for (const v of Object.values(node.props ?? {})) strings(v, out);
    for (const v of node.list ?? []) strings(v, out);
  }
  return out;
}

export const show = (v) => (v !== null && typeof v === "object" ? `{${Object.keys(v.props ?? {}).length ? Object.entries(v.props).map(([k, x]) => `${jsonKey(k)}: ${show(x)}`).join(", ") : (v.list ?? []).map(show).join(", ")}}` : JSON.stringify(v));

// ---- Loading the layers ----------------------------------------------------------------------

/** Core conf root node of a section, or null. */
export function loadCore(addonsDir, def) {
  if (!def.core) return null; // a section whose core ships no records (racing.courses)
  const p = join(addonsDir, CORE, def.core);
  return existsSync(p) ? parseConf(readFileSync(p, "utf8")) : null;
}

/**
 * The overlays a pack loads: [{ file, root }] in the mission header's order. `problems` collects
 * what cannot be loaded: no header, an overlay GUID that resolves to nothing, an unparsable file.
 */
export function loadPackOverlays(addonsDir, pack, meta) {
  const dir = join(addonsDir, pack);
  const out = { overlays: [], problems: [], unlisted: [], header: null, manifestRef: null };
  const headers = walk(join(dir, "Missions")).filter((f) => f.endsWith(".conf"));
  const header = headers.map((f) => ({ f, root: parseConf(readFileSync(f, "utf8")) })).find((h) => h.root?.cls === "LF_MissionHeader");
  if (!header) {
    out.problems.push({ file: "Missions/", id: "", msg: "no mission header (.conf with root LF_MissionHeader)" });
    return out;
  }
  out.header = rel(dir, header.f);
  out.manifestRef = header.root.props.m_LF_PackManifest ?? null;
  const listed = new Set();
  for (const ref of header.root.props.m_aLF_ConfigOverlays?.list ?? []) {
    const g = guidOf(ref);
    const hit = g ? meta.get(g) : null;
    if (!hit || hit.addon !== pack) {
      out.problems.push({ file: out.header, id: String(ref), msg: hit ? `overlay ${ref} is a file of ${hit.addon}, not of this pack` : `overlay ${ref} resolves to no resource (GUID not in any .meta)` });
      continue;
    }
    if (hit.path !== pathOfRes(ref)) out.problems.push({ file: out.header, id: String(ref), msg: `overlay GUID {${g}} is ${hit.path}, the header says ${pathOfRes(ref)}` });
    const full = join(dir, hit.file);
    if (!existsSync(full)) { out.problems.push({ file: out.header, id: String(ref), msg: `overlay ${hit.file} does not exist` }); continue; }
    listed.add(hit.file);
    const root = parseConf(readFileSync(full, "utf8"));
    if (!root) { out.problems.push({ file: hit.file, id: "", msg: "overlay is empty or not a .conf object" }); continue; }
    out.overlays.push({ file: hit.file, root });
    // LF_ConfigOverlay.LoadFolderOverlays: every .conf under `<overlay without .conf>/` is one more
    // overlay right after its parent, in path order. A file the header also lists keeps its header place.
    const folder = full.replace(/\.conf$/, "");
    for (const f of existsSync(folder) ? walk(folder).filter((p) => p.endsWith(".conf")).sort() : []) {
      const r = rel(dir, f);
      if (listed.has(r) || out.overlays.some((o) => o.file === r)) continue;
      const sub = parseConf(readFileSync(f, "utf8"));
      if (!sub) { out.problems.push({ file: r, id: "", msg: "folder overlay is empty or not a .conf object" }); continue; }
      listed.add(r);
      out.overlays.push({ file: r, root: sub });
    }
  }
  for (const f of walk(join(dir, "Configs")).filter((p) => p.endsWith(".conf"))) {
    const r = rel(dir, f);
    const root = parseConf(readFileSync(f, "utf8"));
    // Configs/Tests/ holds fixtures a test loads itself (like the core's), never a header overlay.
    if (root && classes().has(root.cls) && !listed.has(r) && !r.startsWith("Configs/Tests/")) out.unlisted.push(r);
  }
  return out;
}

/** The owner's config.json as { json } or { error }. */
export function loadOwner(path) {
  if (!path) return { json: null };
  if (!existsSync(path)) return { error: `cannot open ${path}` };
  try { return { json: JSON.parse(readFileSync(path, "utf8")) }; } catch (e) { return { error: `${path} is not valid JSON (${e.message})` }; }
}

// ---- The merge -------------------------------------------------------------------------------

/**
 * Merges one list section. `layers` are { kind: "core"|"pack"|"owner", label, file, records }
 * with records [{ id, op, fields }] in order (core first). Returns
 * { entries: Map(id -> { id, fields: Map(name -> { value, layer, was: [{value, layer}] }), present, trail }), events, conflicts }.
 * Event: { layer, id, op, result: added|replaced|patched|removed|refused, why }.
 */
export function mergeList(layers) {
  const entries = new Map();
  const events = [];
  const conflicts = [];
  const lastPack = new Map(); // id -> label of the last pack layer that changed it
  const setField = (e, name, value, layer) => {
    const cur = e.fields.get(name);
    // A patch that repeats the id field (the vehicle's prefab) or a value a lower layer has changes nothing: keep the lower source.
    if (cur && JSON.stringify(cur.value) === JSON.stringify(value)) return;
    e.fields.set(name, { value, layer, was: cur ? [...cur.was, { value: cur.value, layer: cur.layer }] : [] });
  };
  for (const layer of layers) {
    for (const r of layer.records) {
      const ev = (result, why = "") => events.push({ layer, id: r.id, op: r.op || "(none)", result, why });
      if (r.op && !VERBS.includes(r.op)) { ev("refused", `unknown verb "${r.op}" (${VERBS.join(", ")})`); continue; }
      if (!r.id) { ev("refused", "the record has no usable id"); continue; }
      const cur = entries.get(r.id);
      const exists = !!cur?.present;
      // Two core records may share an id (shop rules): the last one wins the walk.
      const verb = layer.kind === "core" ? (exists ? "replace" : "add") : r.op || (exists ? "patch" : "add");
      if (verb === "add" && exists) { ev("refused", "add of an id a lower layer already has"); continue; }
      if (verb !== "add" && !exists) { ev("refused", `${verb} on an id no lower layer has`); continue; }
      if (layer.kind === "pack") {
        const prev = lastPack.get(r.id);
        if (prev && prev !== layer.label) conflicts.push({ id: r.id, first: prev, second: layer.label });
        lastPack.set(r.id, layer.label);
      }
      if (verb === "remove") {
        cur.present = false;
        cur.trail.push({ layer, op: "remove" });
        ev("removed");
        continue;
      }
      let e = cur;
      if (!e) { e = { id: r.id, fields: new Map(), present: true, trail: [] }; entries.set(r.id, e); }
      if (verb === "add") { e.fields = new Map(); e.present = true; }
      if (verb === "replace") e.fields = new Map();
      for (const [name, value] of Object.entries(r.fields)) setField(e, name, value, layer);
      e.trail.push({ layer, op: verb });
      ev(verb === "add" ? "added" : verb === "replace" ? "replaced" : "patched");
    }
  }
  return { entries, events, conflicts };
}

export const layerName = (l) => (l.kind === "core" ? "core" : l.kind === "owner" ? `owner ${l.file}` : `pack ${l.label}`);

/** Records of a list section for one layer, from a parsed .conf root. */
export function confRecords(def, root) {
  const list = root?.props[def.field]?.list ?? [];
  return list.map((n) => ({ id: def.id(n.props), op: String(n.props.m_sOp ?? ""), fields: fieldsOf(n.props, idFields(def)), node: n }));
}
// The fields that make up an id are not "set by a layer"; they are the record's name.
export const idFields = (def) => def.idFields ?? (def.key === "items" ? ["m_sId"] : def.key === "offers" ? ["m_sOutlet", "m_sItem"] : def.key === "crimes" ? ["m_sCrimeId"] : []);

/** Records of a list section from the owner's JSON section object. */
export function ownerRecords(def, json) {
  const obj = json?.[def.section]?.[def.key];
  if (!obj || typeof obj !== "object" || Array.isArray(obj)) return [];
  return Object.entries(obj).map(([k, v]) => {
    const rec = v && typeof v === "object" ? v : {};
    const { op, ...fields } = rec;
    return { id: def.ownerKey(k), op: typeof op === "string" ? op : "", fields, rawKey: k };
  });
}

/** All layers of one list section for `packs` (pack dir names, in load order). */
export function sectionLayers(addonsDir, def, packLayers, owner) {
  const layers = [];
  const core = loadCore(addonsDir, def);
  layers.push({ kind: "core", label: CORE, file: def.core, records: core ? confRecords(def, core) : [] });
  for (const p of packLayers) {
    for (const o of p.overlays) {
      if (o.root.cls !== def.cls) continue;
      layers.push({ kind: "pack", label: p.pack, file: o.file, records: confRecords(def, o.root) });
    }
  }
  if (owner?.json) layers.push({ kind: "owner", label: "owner", file: owner.file ?? "config.json", records: ownerRecords(def, owner.json) });
  return layers;
}
