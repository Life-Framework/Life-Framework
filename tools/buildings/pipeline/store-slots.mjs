// tools/buildings/pipeline/store-slots.mjs - the `store` stage of tools\cli building (#1143).
//
// A building whose spec has a `store` block becomes one of our physical stores (Feature/Shop): a
// prefab inheriting the generated building, with LF_ShopComponent (prices), LF_StoreComponent (stock,
// shelf points, boundary) and a "Pay for goods" till context at every cash register the export placed.
// The furniture prefab sits at the building origin, so a fixture child's coords are store-local and
// each shelf point is the fixture's position plus a board point turned by its yaw. Board points come
// from fixtures.json (measured once from the vanilla models). The whole prefab is written from the
// spec on every run: rebuilding the building regenerates the slots with no hand edits.
//
// spec.store: {
//   "prefab": "Supermarket_Store",             file name in prefab_dir (default <name>_Store)
//   "key": "everon_supermarket",                LF_StoreComponent.m_sStoreKey, unique in the world
//   "boundary": { "center": [x, y, z], "half": [x, 0, z] },   leaving it unpaid is shoplifting
//   "restock_seconds": 120,
//   "till": "CashierShop_01",                   the vanilla prop that is a till
//   "lines": [ { "item": "{GUID}Prefabs/...et", "fixture": "ShelfShop_01", "pick": [0, 2],
//                "boards": [2], "per_board": 3, "max": 12, "restock": 4, "start": -1 } ]
// }
// `pick` indexes the fixtures of that prefab in the order the stage prints (by x, then z, then yaw).
import { existsSync, readFileSync, writeFileSync } from "node:fs";
import { createHash } from "node:crypto";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { addonPath, guidsInUse } from "../../assets/pipeline/addon.mjs";
import { freshGuid, readGuid } from "../../assets/pipeline/meta.mjs";

const HERE = dirname(fileURLToPath(import.meta.url));
export const FIXTURES = JSON.parse(readFileSync(join(HERE, "fixtures.json"), "utf8"));
// A shelf point is written this far above the board; the store's spawn trace starts a little higher.
const HINT = 0.03;
const TILL_CONTEXT_UP = 0.25;
const TILL_RADIUS = 1.5;
// Units of one line a price-list order may take (the store has no price-list action; this caps a direct request).
const ORDER_MAX = 5;

const num = (n) => String(+n.toFixed(3));
const hex16 = (seed) => createHash("sha1").update(seed).digest("hex").slice(0, 16).toUpperCase();

// The child entities of a furniture (or building) prefab: { name, ref, at, yaw }.
export function readChildren(text) {
  const out = [];
  const re = /^ {2}\w+ : "(\{[0-9A-F]{16}\}[^"]*\/([^"/]+)\.et)" \{\n((?: {3}[^\n]*\n)*?) {2}\}/gm;
  for (const m of text.matchAll(re)) {
    const c = /^ {3}coords (\S+) (\S+) (\S+)$/m.exec(m[3]);
    if (!c) continue;
    const yaw = /^ {3}angleY (\S+)$/m.exec(m[3]);
    out.push({ name: m[2], ref: m[1], at: [Number(c[1]), Number(c[2]), Number(c[3])], yaw: yaw ? Number(yaw[1]) : 0 });
  }
  return out;
}

// The fixtures of one prefab in the order `pick` counts them.
export function fixturesOf(children, name) {
  const r1 = (v) => Math.round(v * 10) / 10;
  return children.filter((c) => c.name === name).sort((a, b) => r1(a.at[0]) - r1(b.at[0]) || r1(a.at[2]) - r1(b.at[2]) || a.yaw - b.yaw);
}

// A point in the fixture's own space, in building space. Enfusion yaw: +90 turns +Z onto +X.
export function toBuilding(f, [lx, ly, lz]) {
  const r = (f.yaw * Math.PI) / 180;
  const c = Math.cos(r);
  const s = Math.sin(r);
  return [f.at[0] + lx * c + lz * s, f.at[1] + ly, f.at[2] - lx * s + lz * c].map((v) => +v.toFixed(3));
}

export function slotsOf(f, data, boards, perBoard) {
  const out = [];
  const [x0, x1] = data.x;
  for (const b of boards) {
    const board = data.boards[b];
    for (let i = 0; i < perBoard; i++) out.push(toBuilding(f, [x0 + ((x1 - x0) * (i + 0.5)) / perBoard, board.y + HINT, board.z]));
  }
  return out;
}

// What the store prefab holds, from the spec and the exported furniture. problems empty = usable.
export function storePlan(store, furnitureText, fixtures = FIXTURES) {
  const problems = [];
  const children = readChildren(furnitureText);
  const lines = [];
  for (const [i, l] of (store.lines || []).entries()) {
    const at = `store.lines[${i}]`;
    if (!/^\{[0-9A-F]{16}\}Prefabs\/[^"]+\.et$/.test(l.item || "")) problems.push(`${at}: item must be "{GUID}Prefabs/...et", not ${l.item}`);
    const data = fixtures[l.fixture];
    if (!data) {
      problems.push(`${at}: no board measurements for ${l.fixture} in tools/buildings/pipeline/fixtures.json`);
      continue;
    }
    const list = fixturesOf(children, l.fixture);
    const slots = [];
    for (const p of l.pick || []) {
      if (!list[p]) problems.push(`${at}: pick ${p}, but the export placed ${list.length} ${l.fixture}`);
      else for (const b of l.boards || []) if (!data.boards[b]) problems.push(`${at}: ${l.fixture} has boards 0..${data.boards.length - 1}, not ${b}`);
    }
    if (problems.length) continue;
    for (const p of l.pick || []) slots.push(...slotsOf(list[p], data, l.boards || [], l.per_board || 1));
    if (!slots.length) problems.push(`${at}: no shelf points (pick, boards and per_board)`);
    lines.push({ item: l.item, max: l.max ?? slots.length, restock: l.restock ?? 2, start: l.start ?? -1, slots });
  }
  const tills = children.filter((c) => c.name === (store.till || "CashierShop_01")).map((c) => [c.at[0], c.at[1] + TILL_CONTEXT_UP, c.at[2]].map((v) => +v.toFixed(3)));
  if (!tills.length) problems.push(`the export placed no ${store.till || "CashierShop_01"} to pay at`);
  if (!store.key) problems.push("store.key is empty: the saved stock is matched by it");
  alarmProblems(store.alarm, problems);
  return { lines, tills, problems, children };
}

// The store prefab text. Component GUIDs are derived from the slug, so a rerun writes the same file.
export function storePrefab(slug, store, plan, buildingRef) {
  const g = (role) => `{${hex16(`${slug}|store|${role}`)}}`;
  const vec = (v) => v.map(num).join(" ");
  const lines = plan.lines.map((l, i) => [
    `    LF_StoreLine "${g(`line${i}`)}" {`,
    `     m_ItemPrefab "${l.item}"`,
    `     m_iMaxStock ${l.max}`,
    ...(l.start !== -1 ? [`     m_iStartStock ${l.start}`] : []),
    `     m_iRestockAmount ${l.restock}`,
    `     m_sSlots "${l.slots.map(vec).join(" | ")}"`,
    "    }",
  ]);
  // The shop's allowlist is the store's own lines. An empty list sells (and buys back) every priced
  // item in the game, and LF_CharacterShopComponent's buy and sell RPCs only check the distance.
  const allow = plan.lines.map((l, i) => [
    `    LF_ShopItem "${g(`shopitem${i}`)}" {`,
    `     m_ItemPrefab "${l.item}"`,
    '     m_sDisplayName ""',
    `     m_iMaxQuantity ${ORDER_MAX}`,
    "    }",
  ]);
  const contexts = plan.tills.map((t, i) => [
    `    UserActionContext "${g(`ctx${i}`)}" {`,
    `     ContextName "till${i + 1}"`,
    `     Position PointInfo "${g(`ctxpos${i}`)}" {`,
    `      Offset ${vec(t)}`,
    "     }",
    `     Radius ${TILL_RADIUS}`,
    "    }",
  ]);
  const actions = plan.tills.map((t, i) => [
    `    LF_StoreTillAction "${g(`till${i}`)}" {`,
    "     ParentContextList {",
    `      "till${i + 1}"`,
    "     }",
    `     UIInfo UIInfo "${g(`tillui${i}`)}" {`,
    '      Name "#LF-Store_Pay"',
    "     }",
    "    }",
  ]);
  const b = store.boundary || {};
  return [
    `SCR_DestructibleBuildingEntity : "${buildingRef}" {`,
    ` ID "${hex16(`${slug}|store|entity`)}"`,
    " components {",
    `  LF_ShopComponent "${g("shop")}" {`,
    "   m_aShopItems {",
    ...allow.flat(),
    "   }",
    "  }",
    `  LF_StoreComponent "${g("store")}" {`,
    `   m_sStoreKey "${store.key}"`,
    "   m_aLines {",
    ...lines.flat(),
    "   }",
    `   m_vBoundaryCenter ${vec(b.center || [0, 0, 0])}`,
    `   m_vBoundaryHalf ${vec(b.half || [6, 0, 4])}`,
    `   m_iRestockSeconds ${store.restock_seconds ?? 120}`,
    "  }",
    `  ActionsManagerComponent "${g("actions")}" {`,
    "   ActionContexts {",
    ...contexts.flat(),
    "   }",
    "   additionalActions {",
    ...actions.flat(),
    "   }",
    "  }",
    " }",
    ...alarmChild(slug, store.alarm, g),
    "}",
    "",
  ].join("\n");
}

// store.alarm: { "prefab": "{GUID}Prefabs/...et", "at": [x, y, z], "yaw": 180, "offset": [x, y, z], "reach": 10 }.
// A child entity (a bell box on the shop front) carrying LF_AlarmMountComponent: a walk-out's site
// alarm (LF_Dispatch at the store's origin) sounds on it instead of on the ground at the site.
// `reach` must cover the box's distance from the store's origin (capped at 12 by the mount).
function alarmChild(slug, alarm, g) {
  if (!alarm) return [];
  const vec = (v) => v.map(num).join(" ");
  return [
    " {",
    `  GenericEntity : "${alarm.prefab}" {`,
    `   ID "${hex16(`${slug}|store|alarmbox`)}"`,
    "   components {",
    `    LF_AlarmMountComponent "${g("alarmmount")}" {`,
    `     m_vOffset ${vec(alarm.offset || [0, 0, 0])}`,
    `     m_fReach ${num(alarm.reach ?? 10)}`,
    "    }",
    "   }",
    `   coords ${vec(alarm.at)}`,
    ...(alarm.yaw ? [`   angleY ${num(alarm.yaw)}`] : []),
    "  }",
    " }",
  ];
}

export function alarmProblems(alarm, problems) {
  if (!alarm) return;
  if (!/^\{[0-9A-F]{16}\}Prefabs\/[^"]+\.et$/.test(alarm.prefab || "")) problems.push(`store.alarm: prefab must be "{GUID}Prefabs/...et", not ${alarm.prefab}`);
  if (!Array.isArray(alarm.at) || alarm.at.length !== 3) problems.push("store.alarm: at must be [x, y, z]");
  const reach = alarm.reach ?? 10;
  const far = Array.isArray(alarm.at) ? Math.hypot(...alarm.at) : 0;
  if (reach > 12 || far > reach) problems.push(`store.alarm: reach ${reach} must cover the box's ${far.toFixed(1)} m from the store origin and stay within the mount's 12 m`);
}

const META_CONFIGS =["PC {", "XBOX_ONE : PC {", "XBOX_SERIES : PC {", "PS4 : PC {", "PS5 : PC {", "HEADLESS : PC {"];
export const prefabMeta = (guid, path) => `MetaFileClass {\n Name "{${guid}}${path}"\n Configurations {\n${META_CONFIGS.map((c) => `  EntityTemplateResourceClass ${c}\n  }`).join("\n")}\n }\n}\n`;

// The stage: writes <prefab_dir>/<store.prefab>.et (and its .meta, GUID kept) into the building's addon.
export async function stageStore(ctx) {
  const { root, spec, slug } = ctx;
  const store = spec.store;
  if (!store) {
    console.log(`store: ${slug} has no store block; nothing to do`);
    return { skipped: true };
  }
  const addon = addonPath(root, spec);
  const furniture = join(addon, spec.prefab_dir, "Furniture", `${spec.name}_furniture_01.et`);
  const building = `${spec.prefab_dir}/${spec.name}.et`;
  if (!existsSync(furniture)) throw new Error(`no ${furniture}; run the import stage`);
  const buildingGuid = readGuid(join(addon, `${building}.meta`));
  if (!buildingGuid) throw new Error(`no GUID for ${building}`);
  const plan = storePlan(store, readFileSync(furniture, "utf8"));
  for (const name of new Set((store.lines || []).map((l) => l.fixture))) {
    console.log(`        ${name} (pick by index):`);
    fixturesOf(plan.children, name).forEach((f, i) => console.log(`          ${String(i).padStart(2)}  ${f.at.map(num).join(" ")}  yaw ${f.yaw}, front at ${toBuilding(f, [0, 0, 0.5]).map(num).join(" ")}`));
  }
  for (const p of plan.problems) console.log(`FAIL  ${p}`);
  if (plan.problems.length) throw new Error(`${plan.problems.length} store check(s) failed`);
  const rel = `${spec.prefab_dir}/${store.prefab || `${spec.name}_Store`}.et`;
  const abs = join(addon, rel);
  const guid = readGuid(`${abs}.meta`) || freshGuid(guidsInUse(root));
  writeFileSync(abs, storePrefab(slug, store, plan, `{${buildingGuid}}${building}`));
  if (!existsSync(`${abs}.meta`)) writeFileSync(`${abs}.meta`, prefabMeta(guid, rel));
  for (const l of plan.lines) console.log(`        ${l.item.replace(/^.*\//, "")}: ${l.slots.length} shelf point(s), max ${l.max}, restock ${l.restock}`);
  console.log(`store: {${guid}}${rel}; key ${store.key}, ${plan.lines.length} line(s), ${plan.tills.length} till(s)`);
  return { prefab: `{${guid}}${rel}`, lines: plan.lines.length, slots: plan.lines.reduce((n, l) => n + l.slots.length, 0), tills: plan.tills.length };
}
