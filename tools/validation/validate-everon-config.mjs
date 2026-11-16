// tools/validation/validate-everon-config.mjs
//
// The flagship pack's catalogue and police overlays read against what they must sell and field (#1760).
// The mission header LF_Everon1989.conf lists the overlays in precedence order (lowest first). This check:
//   1. finds every overlay the header names, in the pack or the core, with a .meta carrying the same GUID;
//   2. requires the four chapter overlays (weapons, clothing, kit, police ranks) in the header, each with the
//      root class the loader reads (LF_ItemCatalogueConfig, LF_PoliceConfig);
//   3. merges the core catalogue and police files with those overlays the way the game layers them (by id; an
//      op of remove, or m_bRemoved 1, hides a record; a field a layer does not write keeps the layer below);
//   4. runs every fixture in tools/validation/fixtures/everon/*.json against the merged result.
// Fixture kinds (`outlets`, `ranks`) are in fixtures/everon/README.md.
//
//   node tools/validation/validate-everon-config.mjs                     check the repo
//   node tools/validation/validate-everon-config.mjs --fixtures <dir>    use another fixture directory
//
// red-proof: everon-config-selftest.mjs, and by hand: --fixtures tools/validation/fixtures/everon/red
// (missing-offer.json expects pistol_m1911 at gun_counter, which no overlay offers) prints
// "FAIL missing-offer.json: gun_counter does not offer pistol_m1911 ..." and exits 1; the live fixtures
// and the empty stubs pass.
import { existsSync, readdirSync, readFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const here = dirname(fileURLToPath(import.meta.url));
export const ROOT = join(here, "..", "..");
const PACK_DIR = "addons/LF_Everon1989";
const CORE_DIR = "addons/LifeFramework";
export const HEADER = `${PACK_DIR}/Missions/LF_Everon1989.conf`;
export const CORE_CATALOGUE = `${CORE_DIR}/Configs/Catalogue/LF_ItemCatalogue.conf`;
export const CORE_POLICE = `${CORE_DIR}/Configs/Police/LF_Police.conf`;
export const REQUIRED = [
  ["Configs/Catalogue/LF_Catalogue_Weapons_Everon1989.conf", "LF_ItemCatalogueConfig"],
  ["Configs/Catalogue/LF_Catalogue_Clothing_Everon1989.conf", "LF_ItemCatalogueConfig"],
  ["Configs/Catalogue/LF_Catalogue_Kit_Everon1989.conf", "LF_ItemCatalogueConfig"],
  ["Configs/Police/LF_Police_Everon1989.conf", "LF_PoliceConfig"],
];

function unquote(v) {
  const m = /^"(.*)"$/.exec(v.trim());
  return m ? m[1] : v.trim();
}

/** Parse an Enfusion text resource into a tree. A node is { cls, guid, props, lists, strs, line, kids }. */
export function parseConf(text) {
  const root = { cls: "", guid: "", props: {}, lists: {}, strs: [], line: 0, kids: [] };
  const stack = [root];
  const lines = text.split("\n");
  for (let i = 0; i < lines.length; i++) {
    const line = lines[i].trim();
    if (!line) continue;
    const top = stack[stack.length - 1];
    if (line === "}") { if (stack.length > 1) stack.pop(); continue; }
    let m = /^(\w+)(?:\s+"\{([0-9A-F]{16})\}")?\s*\{$/.exec(line);
    if (m) {
      const node = { cls: m[1], guid: m[2] || "", props: {}, lists: {}, strs: [], line: i + 1, kids: [] };
      top.kids.push(node);
      if (/^m_/.test(m[1])) top.lists[m[1]] = node;
      stack.push(node);
      continue;
    }
    m = /^(m_\w+)\s+(.*)$/.exec(line);
    if (m) { top.props[m[1]] = unquote(m[2]); continue; }
    m = /^"(.*)"$/.exec(line);
    if (m) top.strs.push(m[1]);
  }
  return root.kids[0] || null;
}

/** Records of a list node (m_aItems, m_aOffers, m_aRanks) as plain objects with the props written. */
function records(node, listName) {
  const list = node?.lists[listName];
  return list ? list.kids.filter((k) => !/^m_/.test(k.cls)).map((k) => ({ ...k.props, __kit: k.lists.m_aKit?.strs, __line: k.line })) : [];
}

function mergeInto(map, order, key, rec, file) {
  const op = rec.m_sOp || "";
  const at = `${file}:${rec.__line}`;
  if (op === "remove") { map.delete(key); return; }
  const prev = map.get(key);
  if (!prev) order.push(key);
  if (!prev || op === "replace") map.set(key, { ...rec, __at: at });
  else map.set(key, { ...prev, ...rec, __kit: rec.__kit ?? prev.__kit, __at: at });
}

/** The merged catalogue and police ranks of a set of layers [{ file, root }], core first. */
export function mergeLayers(layers) {
  const items = new Map(), offers = new Map(), ranks = new Map();
  const itemOrder = [], offerOrder = [], rankOrder = [];
  for (const { file, root } of layers) {
    if (!root) continue;
    if (root.cls === "LF_ItemCatalogueConfig") {
      for (const r of records(root, "m_aItems")) mergeInto(items, itemOrder, r.m_sId, r, file);
      for (const r of records(root, "m_aOffers")) mergeInto(offers, offerOrder, `${r.m_sOutlet}/${r.m_sItem}`, r, file);
    } else if (root.cls === "LF_PoliceConfig") {
      for (const r of records(root, "m_aRanks")) mergeInto(ranks, rankOrder, r.m_sId, r, file);
    }
  }
  const live = (map, order) => order.filter((k) => map.has(k) && map.get(k).m_bRemoved !== "1").map((k) => map.get(k));
  return { items: live(items, itemOrder), offers: live(offers, offerOrder), ranks: live(ranks, rankOrder) };
}

function metaGuid(abs) {
  const meta = abs + ".meta";
  if (!existsSync(meta)) return null;
  return /Name "\{([0-9A-F]{16})\}/.exec(readFileSync(meta, "utf8"))?.[1] ?? null;
}

/** Findings for the header, its overlays and the fixtures. root is the repo root; fixtureDir holds *.json. */
export function checkEveron(root, fixtureDir) {
  const fails = [];
  const read = (rel) => (existsSync(join(root, rel)) ? readFileSync(join(root, rel), "utf8") : null);
  const headerText = read(HEADER);
  if (headerText === null) return { fails: [`${HEADER}: missing`], merged: null };
  const header = parseConf(headerText);
  const refs = (header?.lists.m_aLF_ConfigOverlays?.strs ?? []).map((s) => /^\{([0-9A-F]{16})\}(.+)$/.exec(s)).filter(Boolean);
  const layers = [
    { file: CORE_CATALOGUE, root: parseConf(read(CORE_CATALOGUE) ?? "") },
    { file: CORE_POLICE, root: parseConf(read(CORE_POLICE) ?? "") },
  ];
  const listed = new Set();
  for (const [, guid, path] of refs) {
    listed.add(path);
    const rel = [`${PACK_DIR}/${path}`, `${CORE_DIR}/${path}`].find((p) => existsSync(join(root, p)));
    if (!rel) { fails.push(`${HEADER}: overlay ${path} is in neither the pack nor the core`); continue; }
    const g = metaGuid(join(root, rel));
    if (g === null) fails.push(`${rel}: has no .meta`);
    else if (g !== guid) fails.push(`${HEADER}: overlay ${path} says GUID ${guid}, its .meta says ${g}`);
    layers.push({ file: rel, root: parseConf(read(rel)) });
  }
  for (const [path, cls] of REQUIRED) {
    if (!listed.has(path)) { fails.push(`${HEADER}: m_aLF_ConfigOverlays does not list ${path}`); continue; }
    const layer = layers.find((l) => l.file === `${PACK_DIR}/${path}`);
    if (!layer?.root || layer.root.cls !== cls) fails.push(`${PACK_DIR}/${path}: root class is ${layer?.root?.cls ?? "(none)"}, the loader reads ${cls}`);
  }
  const merged = mergeLayers(layers);
  if (existsSync(fixtureDir)) {
    for (const name of readdirSync(fixtureDir).filter((f) => f.endsWith(".json")).sort()) {
      let fx;
      try { fx = JSON.parse(readFileSync(join(fixtureDir, name), "utf8")); }
      catch (e) { fails.push(`${name}: not JSON (${e.message})`); continue; }
      fails.push(...checkFixture(name, fx, merged));
    }
  }
  return { fails, merged };
}

const GATES = [["licence", "m_sLicence", ""], ["job", "m_sJob", ""], ["minRank", "m_iMinRank", "0"], ["orgRight", "m_sOrgRight", ""]];
const RANK_FIELDS = [["id", "m_sId"], ["titleKey", "m_sTitleKey"], ["fineCap", "m_iFineCap"], ["payPerSecond", "m_fPayPerSecond"], ["dutySecondsForNext", "m_iDutySecondsForNext"]];

/** Findings of one fixture against the merged config. */
export function checkFixture(name, fx, merged) {
  const fails = [];
  if (fx.kind === "outlets") {
    for (const want of fx.offers ?? []) {
      const key = `${fx.outlet}/${want.item}`;
      const offer = merged.offers.find((o) => o.m_sOutlet === fx.outlet && o.m_sItem === want.item);
      if (want.present === false) {
        if (offer) fails.push(`${name}: ${fx.outlet} still offers ${want.item} (${offer.__at}); the fixture says it must not`);
        continue;
      }
      if (!offer) { fails.push(`${name}: ${fx.outlet} does not offer ${want.item}; the fixture expects it`); continue; }
      for (const [field, prop, dflt] of GATES) {
        if (want[field] === undefined) continue;
        const got = offer[prop] ?? dflt;
        if (String(got) !== String(want[field])) fails.push(`${name}: ${key} gate ${field} is "${got}", the fixture expects "${want[field]}" (${offer.__at})`);
      }
      if (want.price !== undefined) {
        const item = merged.items.find((i) => i.m_sId === want.item);
        const got = item ? (item.m_iPrice ?? "100") : null;
        if (String(got) !== String(want.price)) fails.push(`${name}: item ${want.item} price is ${got}, the fixture expects ${want.price}`);
      }
    }
  } else if (fx.kind === "ranks") {
    const n = merged.ranks.length;
    if (fx.count !== undefined && n !== fx.count) fails.push(`${name}: the police ladder has ${n} ranks, the fixture expects ${fx.count}`);
    if (fx.minCount !== undefined && n < fx.minCount) fails.push(`${name}: the police ladder has ${n} ranks, the fixture expects at least ${fx.minCount}`);
    for (const want of fx.ranks ?? []) {
      const rank = want.id !== undefined ? merged.ranks.find((r) => r.m_sId === want.id) : merged.ranks[want.index];
      const label = want.id ?? `#${want.index}`;
      if (!rank) { fails.push(`${name}: no police rank ${label}`); continue; }
      for (const [field, prop] of RANK_FIELDS) {
        if (want[field] !== undefined && String(rank[prop]) !== String(want[field])) fails.push(`${name}: rank ${label} ${field} is ${rank[prop]}, the fixture expects ${want[field]} (${rank.__at})`);
      }
      for (const prefab of want.kit ?? []) {
        if (!(rank.__kit ?? []).some((k) => k.includes(prefab))) fails.push(`${name}: rank ${label} kit lacks ${prefab} (${rank.__at})`);
      }
    }
  } else fails.push(`${name}: unknown fixture kind "${fx.kind}" (outlets or ranks)`);
  return fails;
}

if (process.argv[1] && fileURLToPath(import.meta.url) === process.argv[1]) {
  const i = process.argv.indexOf("--fixtures");
  const dir = i > 0 ? process.argv[i + 1] : join(here, "fixtures", "everon");
  const { fails, merged } = checkEveron(ROOT, dir);
  for (const f of fails) console.error(`FAIL ${f}`);
  if (fails.length) { console.error(`validate-everon-config: ${fails.length} finding(s)`); process.exit(1); }
  console.log(`validate-everon-config: ok (${merged.items.length} items, ${merged.offers.length} offers, ${merged.ranks.length} ranks after the overlays)`);
}
