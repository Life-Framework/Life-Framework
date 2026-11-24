// tools/town/shelf-stock.mjs - shelf stock for an island shop's vanilla shelving (#3516).
//
// An island slot is fitted into a vanilla building (b66 is ShopModern_E_01), and the shelves in it are the
// building's own furniture prefab: no store prefab of ours authored a slot on them, so nothing rested on
// them. This writes the pack prefab that does: a store entity at the building origin whose
// `LF_StoreComponent` lines stand one item directly on a board of the host's wooden shelf units (never a
// pile), and whose `LF_ShopComponent` prices them from the shop's stock id.
//
//   node tools/town/shelf-stock.mjs supermarket [--check]
//
// Everything that is a measurement comes from the game: the host's shelf units and their yaw from the vanilla
// furniture prefab, the board heights and goods footprints from `tools\cli measure` (the .xob colliders).
// Nothing is typed in. The points are local to the store entity, which the kit row puts at the building
// origin, so they are the furniture prefab's own coordinates and turn with the building when it is fitted.
// The store's spawn trace then settles each copy on the board by contact (LF_PlaceSnap), so the points only
// have to be near the board. `--check` fails when the committed prefab differs from a fresh run.
import { existsSync, readFileSync, writeFileSync } from "node:fs";
import { createHash } from "node:crypto";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { guidsInUse } from "../assets/pipeline/addon.mjs";
import { freshGuid, readGuid } from "../assets/pipeline/meta.mjs";
import { boardsOf, itemKey, itemOf } from "../buildings/pipeline/fixtures.mjs";
import { prefabMeta, slotsOf } from "../buildings/pipeline/store-slots.mjs";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");
const PACK = "addons/LF_Everon1989";

// One entry per shop whose host shelving carries stock. `fixture` is the vanilla shelf prefab the units are
// read from; `unitOrder` says which of its units (sorted by position, then yaw) carries which line.
export const SHOPS = {
  supermarket: {
    prefab: "Prefabs/Buildings/Supermarket/Supermarket_Shelves.et",
    storeKey: "everon_sp_supermarket",
    stockId: "supermarket",
    stockConf: `${PACK}/Configs/Shop/LF_ShopStock_Everon1989.conf`,
    hostFurniture: "Prefabs/Structures/Commercial/Shops/ShopModern_E_01/Furniture/ShopModern_E_01_furniture_01.et",
    hostBuilding: "Prefabs/Structures/Commercial/Shops/ShopModern_E_01/ShopModern_E_01_Base.et",
    fixture: "Prefabs/Props/Commercial/ShelfUniversal_01_wide.et",
    // The two back-to-back gondola runs through the middle of the hall: eight units, one line each.
    gondolaZ: -3.47,
    // Boards, lowest first, that carry goods (0.588, 1.081, 1.574): eye and hand height, no floor board, no top.
    boards: [1, 2, 3],
    perBoard: 8,
    // Left off every shelf: the cabbage (0.42 x 0.46 m) overlaps its neighbours and floats in the placement audit.
    skip: ["Prefabs/Items/Food/Cabbage.et"],
    // The sloped gondolas of the hall (`ShelfShop_01`, twelve units: the shelves a player faces from the door).
    // Each takes the next item in the stock's order (wrapping), so the neighbours differ; a unit's points join
    // that item's line. Board 0 (0.102) sits under the baskets, so it stays bare; 1, 2 and 3 carry goods.
    gondola: {
      fixture: "Prefabs/Props/Commercial/ShelfShop_01.et",
      boards: [1, 2, 3],
      perBoard: 7,
      maxFoot: 0.3,
    },
    // The boundary of the store: the host's own model bounds, leaving it with unpaid goods is shoplifting.
    restockSeconds: 120,
  },
};

const num = (n) => String(+Number(n).toFixed(3));
const hex16 = (seed) => createHash("sha1").update(seed).digest("hex").slice(0, 16).toUpperCase();

// The instances of one prefab in a vanilla furniture prefab dump: { at, yaw }. Entities are either single
// (`GenericEntity : "{GUID}path" { ... coords ... }`) or a `$grp` of blocks that share the prefab.
export function instancesOf(text, prefabPath) {
  const lines = text.split(/\r?\n/);
  const out = [];
  for (let i = 0; i < lines.length; i++) {
    const m = /^\s*(\$grp )?\w+ : "\{[0-9A-F]{16}\}([^"]+)"\s*\{\s*$/.exec(lines[i]);
    if (!m || m[2] !== prefabPath) continue;
    let depth = 0;
    const body = [];
    for (let j = i; j < lines.length; j++) {
      for (const ch of lines[j]) {
        if (ch === "{") depth++;
        else if (ch === "}") depth--;
      }
      body.push(lines[j]);
      if (depth === 0) break;
    }
    const chunks = m[1] ? body.join("\n").split(/\n\s*\{\n/).slice(1) : [body.join("\n")];
    for (const c of chunks) {
      const at = /coords (\S+) (\S+) (\S+)/.exec(c);
      if (!at) continue;
      const angles = /angles (\S+) (\S+) (\S+)/.exec(c);
      const angleY = /angleY (\S+)/.exec(c);
      out.push({ at: [Number(at[1]), Number(at[2]), Number(at[3])], yaw: angles ? Number(angles[2]) : angleY ? Number(angleY[1]) : 0 });
    }
  }
  return out;
}

// The sorted order `store-slots.fixturesOf` uses: by x, then z, then yaw (rounded to 10 cm).
const r1 = (v) => Math.round(v * 10) / 10;
export const sortUnits = (units) => [...units].sort((a, b) => r1(a.at[0]) - r1(b.at[0]) || r1(a.at[2]) - r1(b.at[2]) || a.yaw - b.yaw);

// The item prefabs of a stock id, in the config's order: "{GUID}path" strings.
export function stockItems(confText, stockId) {
  const start = confText.indexOf(`m_sId "${stockId}"`);
  if (start < 0) return [];
  const rest = confText.slice(start);
  const next = rest.indexOf("LF_ShopStock ", 10);
  const block = next < 0 ? rest : rest.slice(0, next);
  return [...block.matchAll(/m_Prefab "(\{[0-9A-F]{16}\}[^"]+\.et)"/g)].map((m) => m[1]);
}

// Pure: the lines of the store from the units, the measured boards and the measured goods.
// Returns { lines: [{ item, slots, max, restock }], skipped: [{ item, why }], problems }.
// `gondola` (optional) is a second fixture type: { units, boards, boardIdx, perBoard }. Its units, sorted, take the
// items that fit its boards in turn (wrapping), and the points join that item's line, so one item stands on
// the wall units and on several gondolas, one copy per point.
export function planShelves({ units, items, itemRefs, boards, boardIdx, perBoard, gondolaZ, gondola = null }) {
  const problems = [];
  const skipped = [];
  // The units that stand in the gondola runs, in sorted order, one line each (front sides face the aisles).
  const wallRun = sortUnits(units.filter((u) => Math.abs(u.at[2] - gondolaZ) < 0.1));
  const lines = [];
  let k = 0;
  for (const ref of itemRefs) {
    const item = items[itemKey(ref)];
    if (!item) {
      problems.push(`${itemKey(ref)}: not measured`);
      continue;
    }
    const unit = wallRun[k];
    if (!unit) {
      skipped.push({ item: ref, why: "no shelf unit left for it" });
      continue;
    }
    const slots = [];
    for (const b of boardIdx) {
      if (!boards[b]) {
        problems.push(`board ${b}: the shelf has ${boards.length} boards`);
        continue;
      }
      slots.push(...slotsOf(unit, boards[b], item, { perBoard, rows: 1 }));
    }
    if (!slots.length) {
      skipped.push({ item: ref, why: `${itemKey(ref)} does not fit the shelf boards` });
      continue;
    }
    k++;
    lines.push({ item: ref, slots, max: slots.length, restock: 1 });
  }
  let gondolaUnits = 0;
  if (gondola) {
    const fits = (ref, unit) => {
      const item = items[itemKey(ref)];
      if (!item) return [];
      // Goods bigger than this stay off the gondolas: the cabbage (0.42 x 0.46 m) overlapped its neighbours.
      if (gondola.maxFoot && Math.max(item.size[0], item.size[2]) > gondola.maxFoot) return [];
      const pts = [];
      for (const b of gondola.boardIdx) {
        if (!gondola.boards[b]) {
          problems.push(`gondola board ${b}: the shelf has ${gondola.boards.length} boards`);
          continue;
        }
        pts.push(...slotsOf(unit, gondola.boards[b], item, { perBoard: gondola.perBoard, rows: 1 }));
      }
      return pts;
    };
    const sorted = sortUnits(gondola.units);
    const stocked = itemRefs.filter((r) => fits(r, sorted[0]).length);
    sorted.forEach((unit, j) => {
      const ref = stocked[j % Math.max(stocked.length, 1)];
      if (!ref) return;
      const pts = fits(ref, unit);
      if (!pts.length) return;
      let line = lines.find((l) => l.item === ref);
      if (!line) {
        line = { item: ref, slots: [], max: 0, restock: 1 };
        lines.push(line);
      }
      line.slots.push(...pts);
      gondolaUnits++;
    });
  }
  for (const l of lines) {
    l.max = l.slots.length;
    l.restock = Math.max(1, Math.round(l.slots.length / 6));
  }
  return { lines, skipped, problems, units: k, gondolaUnits };
}

// The prefab text. Component GUIDs derive from the store key, so a rerun writes the same file.
export function shelvesPrefab(shop, plan, boundary) {
  const g = (role) => `{${hex16(`${shop.storeKey}|shelves|${role}`)}}`;
  const vec = (v) => v.map(num).join(" ");
  const lines = plan.lines.map((l, i) => [
    `    LF_StoreLine "${g(`line${i}`)}" {`,
    `     m_ItemPrefab "${l.item}"`,
    `     m_iMaxStock ${l.max}`,
    `     m_iRestockAmount ${l.restock}`,
    `     m_sSlots "${l.slots.map(vec).join(" | ")}"`,
    "    }",
  ]);
  return [
    "GenericEntity {",
    ` ID "${hex16(`${shop.storeKey}|shelves|entity`)}"`,
    " components {",
    `  LF_ShopComponent "${g("shop")}" {`,
    `   m_sStockId "${shop.stockId}"`,
    "  }",
    `  LF_StoreComponent "${g("store")}" {`,
    `   m_sStoreKey "${shop.storeKey}"`,
    "   m_aLines {",
    ...lines.flat(),
    "   }",
    `   m_vBoundaryCenter ${vec(boundary.center)}`,
    `   m_vBoundaryHalf ${vec(boundary.half)}`,
    `   m_iRestockSeconds ${shop.restockSeconds}`,
    "  }",
    `  RplComponent "${g("rpl")}" {`,
    "   Enabled 1",
    "  }",
    " }",
    " coords 0 0 0",
    "}",
    "",
  ].join("\n");
}

async function build(root, name) {
  const shop = SHOPS[name];
  if (!shop) throw new Error(`no shop '${name}' (${Object.keys(SHOPS).join(", ")})`);
  const { gameVfs, readText } = await import("../lib/game-prefab.mjs");
  const { measureOne } = await import("../commands/measure.mjs");
  const vfs = await gameVfs();
  const furniture = readText(root, vfs, shop.hostFurniture);
  if (!furniture) throw new Error(`${shop.hostFurniture}: not found in the game`);
  const units = instancesOf(furniture, shop.fixture);
  if (!units.length) throw new Error(`${shop.hostFurniture} places no ${shop.fixture}`);

  const fixture = await measureOne(root, shop.fixture, { minSize: 0.05 });
  const boards = boardsOf(fixture.surfaces);

  const itemRefs = stockItems(readFileSync(join(root, shop.stockConf), "utf8"), shop.stockId).filter((r) => !(shop.skip ?? []).includes(itemKey(r)));
  if (!itemRefs.length) throw new Error(`${shop.stockConf}: no items under stock '${shop.stockId}'`);
  const items = {};
  for (const ref of itemRefs) {
    const m = await measureOne(root, ref, { minSize: 0.05 });
    items[itemKey(ref)] = itemOf(m.bounds);
  }

  let gondola = null;
  if (shop.gondola) {
    const gf = await measureOne(root, shop.gondola.fixture, { minSize: 0.05 });
    const gUnits = instancesOf(furniture, shop.gondola.fixture);
    if (!gUnits.length) throw new Error(`${shop.hostFurniture} places no ${shop.gondola.fixture}`);
    gondola = { units: gUnits, boards: boardsOf(gf.surfaces), boardIdx: shop.gondola.boards, perBoard: shop.gondola.perBoard, maxFoot: shop.gondola.maxFoot };
  }
  const plan = planShelves({ units, items, itemRefs, boards, boardIdx: shop.boards, perBoard: shop.perBoard, gondolaZ: shop.gondolaZ, gondola });

  const host = await measureOne(root, shop.hostBuilding, { minSize: 0.05 });
  const b = host.bounds;
  const boundary = {
    center: [(b.max[0] + b.min[0]) / 2, 0, (b.max[2] + b.min[2]) / 2],
    half: [(b.max[0] - b.min[0]) / 2, 0, (b.max[2] - b.min[2]) / 2],
  };
  return { shop, plan, boundary, boards, units, text: shelvesPrefab(shop, plan, boundary) };
}

export async function main(args, { root = ROOT } = {}) {
  const name = args.find((a) => !a.startsWith("--"));
  const check = args.includes("--check");
  let r;
  try {
    r = await build(root, name);
  } catch (e) {
    console.log(`FAIL  shelf-stock: ${e.message}`);
    return 1;
  }
  for (const p of r.plan.problems) console.log(`FAIL  ${p}`);
  if (r.plan.problems.length) return 1;
  const file = join(root, PACK, r.shop.prefab);
  const rel = r.shop.prefab;
  if (check) {
    const same = existsSync(file) && readFileSync(file, "utf8") === r.text;
    console.log(`SUMMARY shelf-stock ${same ? "ok" : "fail"} ${name}: ${same ? "matches a fresh run" : `${rel} differs from a fresh run`}`);
    return same ? 0 : 1;
  }
  writeFileSync(file, r.text);
  if (!existsSync(`${file}.meta`)) writeFileSync(`${file}.meta`, prefabMeta(freshGuid(guidsInUse(root)), rel));
  for (const l of r.plan.lines) console.log(`        ${itemKey(l.item).replace(/^.*\//, "")}: ${l.slots.length} shelf point(s), max ${l.max}, restock ${l.restock}`);
  for (const s of r.plan.skipped) console.log(`        skipped ${itemKey(s.item).replace(/^.*\//, "")}: ${s.why}`);
  console.log(`SUMMARY shelf-stock ok ${name}: {${readGuid(`${file}.meta`)}}${rel}, ${r.plan.lines.length} line(s), ${r.plan.lines.reduce((n, l) => n + l.slots.length, 0)} shelf point(s) on ${r.plan.units} of ${r.units.length} wall unit(s) and ${r.plan.gondolaUnits} gondola(s)`);
  return 0;
}

if (process.argv[1] && resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
  process.exit(await main(process.argv.slice(2)));
}
