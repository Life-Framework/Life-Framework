// validate-mannequin-outfits: every piece a dressed mannequin wears is a priced catalogue item its outlet offers.
// inputs: addons/**/*.conf, addons/**/Props/Clothing/*.et
//
// The revert audit (docs/testing/revert-audit-2026-11-27.md): #5096 dropped the pack's `jacket_raincoat_green` clothing
// offer but left it on the raincoat mannequin's outfit list, so `everon/mannequins` (pack leg, a game boot GitHub's
// validate never runs) failed on main and the main guard reverted the PR. This is the same rule as that test
// (LF_MannequinOutfit.Piece and HasOutfit) read from the files: a prefab carrying an LF_MannequinOutfitComponent names
// an outlet (LF_ShopComponent m_sOutletId) and the item ids it wears; each id must be a catalogue item with a price
// above 0, not removed, that the outlet offers (an offer that is not removed and not a licence row), and the outfit
// needs two sellable pieces.
//
// The catalogue merges the way the game layers it: the core file, then every overlay in path order; a later layer
// changes only the fields it writes (a new id starts at the default price 100), and `m_bRemoved` on a later layer wins.
//
// red-proof: mannequin-outfits-selftest.mjs; on the tree before #5098, `jacket_raincoat_green` back on the raincoat
// mannequin with the pack offer dropped fails with "LF_Mannequin_Raincoat.et wears jacket_raincoat_green, which
// clothing_rack does not sell".

import { existsSync, readdirSync, readFileSync } from "node:fs";
import { dirname, join, relative } from "node:path";
import { fileURLToPath } from "node:url";
import { CORE_FILE, parseCatalogue } from "./validate-item-catalogue.mjs";

const ROOT = join(dirname(fileURLToPath(import.meta.url)), "..", "..");

// The merged catalogue of a set of files: { items: Map<id, {price, removed}>, offers: Map<"outlet|item", {removed, licence}> }.
export function mergeCatalogue(files) {
  const parsed = files.map((f) => ({ ...f, cat: parseCatalogue(f.text) })).filter((f) => f.cat);
  parsed.sort((a, b) => (a.file === CORE_FILE ? -1 : b.file === CORE_FILE ? 1 : a.file.localeCompare(b.file)));
  const items = new Map();
  const offers = new Map();
  for (const { cat } of parsed) {
    for (const it of cat.items) {
      if (!it.id) continue;
      const m = items.get(it.id) ?? { price: 100, removed: false };
      if (it.price !== null) m.price = it.price;
      if (it.removed !== null) m.removed = it.removed;
      items.set(it.id, m);
    }
    for (const of of cat.offers) {
      const key = `${of.outlet}|${of.item}`;
      const m = offers.get(key) ?? { removed: false, licence: false };
      if (of.removed !== null) m.removed = of.removed;
      if (of.grants) m.licence = true;
      offers.set(key, m);
    }
  }
  return { items, offers };
}

// The outfit a mannequin prefab's text declares, or null when it has no LF_MannequinOutfitComponent.
export function parseMannequin(text) {
  if (!/\bLF_MannequinOutfitComponent\b/.test(text)) return null;
  const shop = /\bLF_ShopComponent\b[^{]*\{[\s\S]*?\bm_sOutletId\s+"([^"]*)"/.exec(text);
  const ids = /\bm_aItemIds\s*\{([^}]*)\}/.exec(text);
  return { outlet: shop ? shop[1] : "", ids: ids ? [...ids[1].matchAll(/"([^"]*)"/g)].map((m) => m[1]) : [] };
}

export function checkMannequins(catalogueFiles, prefabs) {
  const cat = mergeCatalogue(catalogueFiles);
  const fails = [];
  for (const { file, text } of prefabs) {
    const m = parseMannequin(text);
    if (!m || (!m.ids.length && !m.outlet)) continue; // the base display prefab carries neither
    const name = file.split("/").pop();
    if (!m.outlet) { fails.push(`${file}: ${name} has an outfit list but no LF_ShopComponent m_sOutletId`); continue; }
    let sellable = 0;
    for (const id of m.ids) {
      const item = cat.items.get(id);
      const offer = cat.offers.get(`${m.outlet}|${id}`);
      if (!item || item.removed || item.price <= 0) fails.push(`${file}: ${name} wears ${id}, which is not a live priced catalogue item`);
      else if (!offer || offer.removed || offer.licence) fails.push(`${file}: ${name} wears ${id}, which ${m.outlet} does not sell`);
      else sellable++;
    }
    if (m.ids.length && sellable < 2) fails.push(`${file}: ${name} sells ${sellable} of its ${m.ids.length} pieces at ${m.outlet}; a mannequin needs two to offer the outfit`);
  }
  return fails;
}

function walk(dir, ok, out = []) {
  if (!existsSync(dir)) return out;
  for (const e of readdirSync(dir, { withFileTypes: true })) {
    const p = join(dir, e.name);
    if (e.isDirectory()) walk(p, ok, out);
    else if (ok(e.name)) out.push(p);
  }
  return out;
}

export function run(root = ROOT) {
  const rel = (p) => relative(root, p).replace(/\\/g, "/");
  const confs = walk(join(root, "addons"), (n) => n.endsWith(".conf")).map((p) => ({ file: rel(p), text: readFileSync(p, "utf8") }));
  const prefabs = walk(join(root, "addons"), (n) => /^LF_Mannequin\w*\.et$/.test(n)).map((p) => ({ file: rel(p), text: readFileSync(p, "utf8") }));
  return { fails: checkMannequins(confs, prefabs), prefabs: prefabs.length };
}

if (process.argv[1] && fileURLToPath(import.meta.url) === process.argv[1]) {
  const { fails, prefabs } = run();
  for (const f of fails) console.log(`FAIL  ${f}`);
  if (fails.length) process.exit(1);
  console.log(`validate-mannequin-outfits: ok (${prefabs} mannequin prefab(s))`);
}
