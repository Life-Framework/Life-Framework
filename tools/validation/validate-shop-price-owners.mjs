// One owner per price (gun-counter-overlay, 2026-11-27). Pack overlays of the same kind merge by key in
// mission-header order, so when two pack files price the same thing the later one wins in silence and the
// price a player pays depends on load order (the 0.3.0 log audit, PR #5109: the gun store file sold the
// vz. 58 at 1100 while the weapons file, later in the list, made it 1800 and took it off the counter).
// This check fails when, across the non-core files of all addons:
//   - a shop price rule key (`prefab:<GUID>` for an `m_Prefab` rule, else its `m_sFind`) is written in two
//     `LF_ShopPricesConfig` files;
//   - a catalogue item id gets an `m_iPrice` or `m_iSellPrice` in two `LF_ItemCatalogueConfig` files;
//   - a catalogue offer (outlet/item) is written in two `LF_ItemCatalogueConfig` files (one adds it, the
//     other removes or rewrites it).
// The core files (`addons/LifeFramework/Configs/Shop/ShopPrices.conf`, `.../Catalogue/LF_ItemCatalogue.conf`)
// are the base layer a pack is meant to override, so a pack row over a core row is fine. Test fixtures under
// `Configs/Tests/` overlay on purpose and are skipped. Fix a finding by keeping the row in the file whose
// shop sells the thing and deleting it from the other.
//
//   node tools/validation/validate-shop-price-owners.mjs
//
// red-proof: shop-price-owners-selftest.mjs (a prefab rule in two pack files, an item priced in two pack
// files and an offer added in one and removed in another must each fail; a pack over core must pass).
// Observed 2026-11-27 on main before this change: 5 FAIL lines for the shop rules (flat cap, ushanka, two
// knitted hats in ClothingStore and GunStore; the Soviet flashlight in ClothingStore and GeneralStore),
// 2 for the vz. 58 and its magazine prices, 3 for the gun counter offers, exit 1.
// inputs: addons/**/*.conf, tools/validation/validate-item-catalogue.mjs
import { execFileSync } from "node:child_process";
import { readFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { parseCatalogue } from "./validate-item-catalogue.mjs";

const here = dirname(fileURLToPath(import.meta.url));
const ROOT = join(here, "..", "..");
export const CORE_FILES = new Set([
  "addons/LifeFramework/Configs/Shop/ShopPrices.conf",
  "addons/LifeFramework/Configs/Catalogue/LF_ItemCatalogue.conf",
]);

/** The rule keys of one shop price .conf, as LF_ShopPriceResolver.RuleKey makes them, each with its line.
 *  Null when the root is another class. */
export function parseShopRules(text) {
  if (!/^\s*LF_ShopPricesConfig\s*\{/.test(text)) return null;
  const rules = [];
  let cur = null;
  text.split("\n").forEach((line, i) => {
    if (/^\s*LF_ShopPriceRule\b.*\{\s*$/.test(line)) {
      cur = { line: i + 1, prefab: "", find: "" };
      rules.push(cur);
      return;
    }
    if (!cur) return;
    const prefab = /^\s*m_Prefab\s+"\{([0-9A-Fa-f]{16})\}/.exec(line);
    if (prefab) cur.prefab = prefab[1].toUpperCase();
    const find = /^\s*m_sFind\s+"(.*)"\s*$/.exec(line);
    if (find) cur.find = find[1];
  });
  return rules
    .map((r) => ({ line: r.line, key: r.prefab ? `prefab:${r.prefab}` : r.find }))
    .filter((r) => r.key);
}

/** Every key written by two or more non-core files. \param files [{file, text}] \return {fails, files} */
export function checkOwners(files) {
  const owners = { rule: new Map(), price: new Map(), offer: new Map() };
  const claim = (kind, key, where) => {
    const list = owners[kind].get(key) || [];
    if (!list.some((w) => w.file === where.file)) list.push(where);
    owners[kind].set(key, list);
  };
  let counted = 0;
  for (const { file, text } of files) {
    if (CORE_FILES.has(file) || file.includes("/Configs/Tests/")) continue;
    const rules = parseShopRules(text);
    if (rules) {
      counted++;
      for (const r of rules) claim("rule", r.key, { file, line: r.line });
      continue;
    }
    const cat = parseCatalogue(text);
    if (!cat) continue;
    counted++;
    for (const it of cat.items) if (it.id && (it.price !== null || it.sell !== null)) claim("price", it.id, { file, line: it.line });
    for (const of of cat.offers) if (of.outlet && of.item) claim("offer", `${of.outlet}/${of.item}`, { file, line: of.line });
  }
  const label = { rule: "shop price rule", price: "catalogue price of item", offer: "catalogue offer" };
  const fails = [];
  for (const kind of Object.keys(owners)) {
    for (const [key, list] of owners[kind]) {
      if (list.length < 2) continue;
      const at = list.map((w) => `${w.file}:${w.line}`).join(" and ");
      fails.push(`${label[kind]} ${key} is written in ${list.length} files (${at}); the later one in load order wins in silence. Keep one owner`);
    }
  }
  return { fails, files: counted };
}

function main() {
  const files = execFileSync("git", ["ls-files", "addons"], { cwd: ROOT, encoding: "utf8" })
    .split("\n")
    .filter((f) => f.endsWith(".conf"))
    .map((f) => ({ file: f, text: readFileSync(join(ROOT, f), "utf8") }));
  const res = checkOwners(files);
  if (res.fails.length) {
    for (const f of res.fails) console.log(`FAIL ${f}`);
    console.log(`validate-shop-price-owners: ${res.fails.length} finding(s) in ${res.files} price file(s)`);
    process.exit(1);
  }
  console.log(`validate-shop-price-owners: OK (${res.files} price file(s), one owner per price)`);
}

if (process.argv[1] && fileURLToPath(import.meta.url) === process.argv[1]) main();
