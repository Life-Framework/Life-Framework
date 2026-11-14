// The item catalogue's checks (#988): every catalogue prefab exists, no DLC-owned prefab is in the
// catalogue (so no outlet can sell one), no item sells back for more than its price (#686 profit
// loops), and every outlet offer names an item some catalogue file lists.
//
// A catalogue file is any addon .conf whose root is LF_ItemCatalogueConfig: the core file and every
// pack overlay. A prefab "exists" when it is an addon resource whose .meta carries the same GUID, or a
// base game prefab listed in the game's data resource database (read from ENFUSION_GAME_PATH when the
// game is installed; CI has none and skips that half, with a note). DLC-owned prefabs are the paths in
// dlc-owned-prefabs.txt, made from the installed game with --refresh-dlc (paths the Rally DLC's
// resource database lists and the base game's does not).
//
//   node tools/validation/validate-item-catalogue.mjs                check
//   node tools/validation/validate-item-catalogue.mjs --refresh-dlc  rewrite dlc-owned-prefabs.txt
//
// red-proof: item-catalogue-selftest.mjs (a fixture with a DLC helmet, a sell price above the price,
// a missing addon prefab and an offer for no item must fail each check). Observed 2026-11-14 on the real
// file: the raincoat's prefab set to Suit_Rally_01_red.et and its sell price to 90 printed "FAIL ...
// LF_ItemCatalogue.conf:12 item raincoat_green: ... is DLC-owned" and "sells back for 90, above its
// price 60 (a profit loop, #686)" and exited 1.
import { execFileSync } from "node:child_process";
import { existsSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const here = dirname(fileURLToPath(import.meta.url));
export const ROOT = join(here, "..", "..");
export const DLC_LIST = join(here, "dlc-owned-prefabs.txt");
export const CORE_FILE = "addons/LifeFramework/Configs/Catalogue/LF_ItemCatalogue.conf";
const GAME = process.env.ENFUSION_GAME_PATH || "C:/Program Files (x86)/Steam/steamapps/common/Arma Reforger";

/** The records of one catalogue .conf: items with id, prefab, price, sell price (null when the
 *  file does not write it), and offers with outlet and item. Null when the root is another class. */
export function parseCatalogue(text) {
  if (!/^\s*LF_ItemCatalogueConfig\s*\{/.test(text)) return null;
  const items = [];
  const offers = [];
  const lines = text.split("\n");
  let cur = null;
  let depth = 0;
  let recDepth = -1;
  lines.forEach((line, i) => {
    const head = /^\s*(LF_CatalogueItem|LF_OutletOffer)\b.*\{\s*$/.exec(line);
    if (head && !cur) {
      cur = head[1] === "LF_CatalogueItem"
        ? { kind: "item", line: i + 1, id: "", prefab: "", price: null, sell: null }
        : { kind: "offer", line: i + 1, outlet: "", item: "" };
      recDepth = depth;
    }
    if (cur) {
      const f = /^\s*(m_\w+)\s+(.*?)\s*$/.exec(line);
      if (f) {
        const v = f[2].replace(/^"(.*)"$/, "$1");
        if (f[1] === "m_sId") cur.id = v;
        else if (f[1] === "m_Prefab") cur.prefab = v;
        else if (f[1] === "m_iPrice") cur.price = Number(v);
        else if (f[1] === "m_iSellPrice") cur.sell = Number(v);
        else if (f[1] === "m_sOutlet") cur.outlet = v;
        else if (f[1] === "m_sItem") cur.item = v;
      }
    }
    for (const c of line) {
      if (c === "{") depth++;
      else if (c === "}") depth--;
    }
    if (cur && depth === recDepth) {
      (cur.kind === "item" ? items : offers).push(cur);
      cur = null;
    }
  });
  return { items, offers };
}

/** The path of a `{GUID}path` reference, or null when it is not one. */
export function splitRef(ref) {
  const m = /^\{([0-9A-F]{16})\}(.+\.et)$/.exec(ref || "");
  return m ? { guid: m[1], path: m[2] } : null;
}

/** Paths of .et resources a resource database names (it stores them as plain strings). */
export function rdbPaths(file) {
  if (!existsSync(file)) return null;
  const text = readFileSync(file).toString("latin1");
  return new Set(text.match(/[A-Za-z0-9_/]+\.et/g) || []);
}

/**
 * Every finding for a set of catalogue files.
 * @param files [{ file, text }]
 * @param ctx { dlc: Set<path>, addonMeta(path) -> guid|null|undefined (undefined: not an addon path),
 *              basePaths: Set<path>|null (null: base game not installed) }
 */
export function checkCatalogues(files, ctx) {
  const fails = [];
  const notes = [];
  const parsed = files.map((f) => ({ ...f, cat: parseCatalogue(f.text) })).filter((f) => f.cat);
  const ids = new Set(parsed.flatMap((f) => f.cat.items.map((it) => it.id)));
  let unchecked = 0;
  for (const { file, cat } of parsed) {
    for (const it of cat.items) {
      const at = `${file}:${it.line} item ${it.id || "(no id)"}`;
      if (!it.id) fails.push(`${at}: has no m_sId`);
      if (it.prefab) {
        const ref = splitRef(it.prefab);
        if (!ref) fails.push(`${at}: prefab ${it.prefab} is not a {GUID}path to an .et`);
        else {
          if (ctx.dlc.has(ref.path)) fails.push(`${at}: ${ref.path} is DLC-owned (dlc-owned-prefabs.txt); a player without the DLC cannot use it, so no outlet may sell it`);
          const guid = ctx.addonMeta(ref.path);
          if (guid === null) fails.push(`${at}: ${ref.path} is an addon path with no .meta`);
          else if (guid !== undefined && guid !== ref.guid) fails.push(`${at}: ${ref.path} has GUID ${guid} in its .meta, the catalogue says ${ref.guid}`);
          else if (guid === undefined) {
            if (!ctx.basePaths) unchecked++;
            else if (!ctx.basePaths.has(ref.path)) fails.push(`${at}: ${ref.path} is neither an addon resource nor a base game prefab`);
          }
        }
      }
    }
    for (const of of cat.offers) {
      const at = `${file}:${of.line} offer ${of.outlet}/${of.item}`;
      if (!of.outlet || !of.item) fails.push(`${at}: needs m_sOutlet and m_sItem`);
      else if (!ids.has(of.item)) fails.push(`${at}: no catalogue file lists item ${of.item}`);
    }
  }
  // Prices merge by id the way the game layers them: the core file first, then the overlays (a new
  // id starts from the attribute defaults, 100 and 50; a known id changes only what a file writes).
  const order = [...parsed].sort((a, b) => (a.file === CORE_FILE ? -1 : b.file === CORE_FILE ? 1 : a.file.localeCompare(b.file)));
  const merged = new Map();
  for (const { file, cat } of order) {
    for (const it of cat.items) {
      if (!it.id) continue;
      const m = merged.get(it.id) ?? { price: 100, sell: 50 };
      if (it.price !== null) m.price = it.price;
      if (it.sell !== null) m.sell = it.sell;
      m.at = `${file}:${it.line}`;
      merged.set(it.id, m);
    }
  }
  for (const [id, m] of merged) {
    if (m.sell > m.price) fails.push(`${m.at} item ${id}: sells back for ${m.sell}, above its price ${m.price} after the layers merge (a profit loop, #686)`);
  }
  if (unchecked) notes.push(`${unchecked} base game prefab(s) not checked: no game install at ENFUSION_GAME_PATH`);
  return { fails, notes, files: parsed.length };
}

function gitFiles(root) {
  return execFileSync("git", ["ls-files", "-z", "--cached", "--others", "--exclude-standard", "addons"], { cwd: root, encoding: "utf8", maxBuffer: 1 << 28 })
    .split("\0").filter((f) => f && existsSync(join(root, f)));
}

/** addonMeta for the repo: the .meta GUID of an addon path, null for an addon path with no meta,
 *  undefined when no addon has the path. */
export function repoMeta(root, files) {
  const metas = new Map();
  for (const f of files) {
    const m = /^addons\/[^/]+\/(.+\.et)\.meta$/.exec(f);
    if (!m) continue;
    const g = /Name\s+"\{([0-9A-F]{16})\}/.exec(readFileSync(join(root, f), "utf8"));
    metas.set(m[1], g ? g[1] : null);
  }
  const ets = new Set(files.map((f) => /^addons\/[^/]+\/(.+\.et)$/.exec(f)?.[1]).filter(Boolean));
  return (path) => (metas.has(path) ? metas.get(path) : ets.has(path) ? null : undefined);
}

export function loadDlc(file = DLC_LIST) {
  if (!existsSync(file)) return new Set();
  return new Set(readFileSync(file, "utf8").split("\n").map((l) => l.trim()).filter((l) => l && !l.startsWith("#")));
}

function refreshDlc() {
  const rally = rdbPaths(join(GAME, "addons", "rally", "resourceDatabase.rdb"));
  const data = rdbPaths(join(GAME, "addons", "data", "resourceDatabase.rdb"));
  if (!rally || !data) {
    console.log(`validate-item-catalogue: no game install at ${GAME} (set ENFUSION_GAME_PATH)`);
    process.exit(1);
  }
  const only = [...rally].filter((p) => !data.has(p)).sort();
  const head = "# DLC-owned prefabs: .et paths the Rally DLC's resource database lists and the base game's does not.\n# Regenerate: node tools/validation/validate-item-catalogue.mjs --refresh-dlc\n";
  writeFileSync(DLC_LIST, head + only.join("\n") + "\n");
  console.log(`validate-item-catalogue: wrote ${only.length} DLC-owned prefab path(s) to dlc-owned-prefabs.txt`);
}

function main() {
  if (process.argv.includes("--refresh-dlc")) return refreshDlc();
  const files = gitFiles(ROOT);
  const confs = files.filter((f) => f.endsWith(".conf")).map((f) => ({ file: f, text: readFileSync(join(ROOT, f), "utf8") }));
  const res = checkCatalogues(confs, {
    dlc: loadDlc(),
    addonMeta: repoMeta(ROOT, files),
    basePaths: rdbPaths(join(GAME, "addons", "data", "resourceDatabase.rdb")),
  });
  for (const n of res.notes) console.log(`NOTE ${n}`);
  if (res.fails.length) {
    for (const f of res.fails) console.log(`FAIL ${f}`);
    console.log(`validate-item-catalogue: ${res.fails.length} finding(s) in ${res.files} catalogue file(s)`);
    process.exit(1);
  }
  console.log(`validate-item-catalogue: OK (${res.files} catalogue file(s))`);
}

if (process.argv[1] && fileURLToPath(import.meta.url) === process.argv[1]) main();
