// tools/validation/validate-pack-unsourced-goods.mjs
// inputs: addons/LF_Everon1989/Configs/**, addons/LF_Everon1989/Prefabs/**, addons/LF_Everon1989/UI/**,
//         addons/LF_Everon1989/Worlds/MainWorld/**/*.layer, addons/LifeFramework/Prefabs/Resources/Mining/*.et.meta,
//         addons/LifeFramework/Configs/Resources/ResourceChains.conf
//
// #5069, #5074, #5078, then the "Copper Bar 38" market board line seen in PR #5163's review: the island has no copper
// or gold rock, but the pack still priced, bought and smelted what those rocks give. The permit check
// (validate-pack-permit-sources) only looks at miner licences; this one looks at every other place a player meets
// an item: a market curve (the price board), a buyer-demand row, a trader's list and prices, a shop stock, a loot
// row, an export order, an unlock row that names a machine, and a prefab the island places that carries a recipe.
//
// The rule, with no list of metals to keep in sync: every mining family in the core (Prefabs/Resources/Mining/
// <Family>Ore.et with its Nugget, Bar or Lump) must have its ore prefab placed in the island layers (MainWorld) for
// the pack to name any of its items. A family with no placed ore is "unsourced": its ore, nugget, bar and lump may
// not appear in a pack config or prefab, nor in a core prefab the island places, and no pack file may name a core
// machine whose recipe takes or makes one. A pack row that REMOVES a core row (m_sOp "remove") is how the pack
// hides the core's generic system, so it is allowed. The core keeps the generic system; the pack stops offering it.
// The check reads which prefab a layer names, never a position.
// red-proof: putting the copper bar back as an `add` row in LF_BuyerDemand_Everon1989.conf (or in the wholesale
// buyer's TraderItem list, or the Smelting 15 `refinery-copper` unlock row) fails with "CopperBar ... is named by ...
// but the island places no Prefabs/Resources/Mining/CopperOre.et" (observed 2026-11-27 on the unfixed pack: the
// market curve, the hardware row, the wholesale ore and bar rows, the dump's copper nugget and the refinery row all
// fail; tools/validation/pack-unsourced-goods-selftest.mjs keeps a red proof for each rule).

import { existsSync, readFileSync, readdirSync } from "node:fs";
import { dirname, join, relative } from "node:path";
import { fileURLToPath } from "node:url";

const ROOT = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
const PACK = join(ROOT, "addons", "LF_Everon1989");
const CORE = join(ROOT, "addons", "LifeFramework");
const MINING = "Prefabs/Resources/Mining";
const KINDS = ["Ore", "Nugget", "Bar", "Lump"];

function walk(dir, keep, out = []) {
  if (!existsSync(dir)) return out;
  for (const e of readdirSync(dir, { withFileTypes: true })) {
    const p = join(dir, e.name);
    if (e.isDirectory()) walk(p, keep, out);
    else if (keep(e.name)) out.push(p);
  }
  return out;
}

// { family -> { items: [{ name, guid }] } } from the core mining prefab metas: Copper -> CopperOre, CopperNugget, CopperBar.
export function miningFamilies(metaFiles) {
  const families = new Map();
  for (const { file, text } of metaFiles) {
    const m = file.replace(/\\/g, "/").match(/\/([A-Z][a-z]+)(Ore|Nugget|Bar|Lump)\.et\.meta$/);
    if (!m) continue;
    const guid = text.match(/Name "\{([0-9A-F]{16})\}/);
    if (!families.has(m[1])) families.set(m[1], { ore: false, items: [] });
    const f = families.get(m[1]);
    if (m[2] === "Ore") f.ore = true;
    f.items.push({ name: `${m[1]}${m[2]}`, kind: m[2], guid: guid ? guid[1] : "" });
  }
  // A family without an Ore prefab has no rock to place, and one without a Nugget is a base prefab (MinableOre),
  // not a metal; the rule is about the rocks the core ships with something to smelt.
  return new Map([...families].filter(([, f]) => f.ore && f.items.some((i) => i.kind === "Nugget")));
}

// The families whose ore prefab is not named anywhere in the island layers.
export function unsourcedFamilies(families, islandText) {
  const out = new Map();
  for (const [name, f] of families) {
    if (!islandText.includes(`${MINING}/${name}Ore.et`)) out.set(name, f);
  }
  return out;
}

// Machine ids in the core resource chains whose recipe takes or makes an unsourced item.
export function unsourcedMachines(chainsText, unsourced) {
  const out = new Map();
  for (const block of chainsText.split(/LF_ResourceMachineDef "\{/).slice(1)) {
    const id = block.match(/m_sMachineId "([^"]+)"/);
    if (!id) continue;
    const hit = firstItem(block, unsourced);
    if (hit) out.set(id[1], hit);
  }
  return out;
}

function firstItem(text, unsourced) {
  for (const [, f] of unsourced) {
    for (const item of f.items) {
      if (text.includes(`${MINING}/${item.name}.et`) || (item.guid && text.includes(`{${item.guid}}`))) return item.name;
    }
  }
  return "";
}

// The index of every place `needle` occurs in `text` as the good of a row that is not a core-row removal.
function liveMentions(text, needle) {
  const hits = [];
  let at = text.indexOf(needle);
  while (at >= 0) {
    // The row's own block: from its `Name "{GUID}" {` header line down to the match.
    const headers = [...text.slice(0, at).matchAll(/^[ \t]*\w+ "\{[0-9A-F]{16}\}" \{[ \t]*$/gm)];
    const blockStart = headers.length > 0 ? headers[headers.length - 1].index : 0;
    const preamble = text.slice(blockStart, at);
    const ops = [...preamble.matchAll(/m_sOp "([a-z]+)"/g)];
    const removal = ops.length > 0 && ops[ops.length - 1][1] === "remove";
    if (!removal) hits.push(at);
    at = text.indexOf(needle, at + needle.length);
  }
  return hits;
}

// files: [{ rel, text }] pack configs and prefabs; placed: [{ rel, text }] core prefabs the island places.
export function check({ files, placed, unsourced, machines }) {
  const errors = [];
  const reasonFor = (item) => `the island places no ${MINING}/${item.name.replace(/(Nugget|Bar|Lump)$/, "Ore")}.et`;
  const scan = (list, how) => {
    for (const { rel, text } of list) {
      for (const [, f] of unsourced) {
        for (const item of f.items) {
          const needles = [`${MINING}/${item.name}.et`];
          if (item.guid) needles.push(`{${item.guid}}`);
          const live = needles.some((n) => liveMentions(text, n).length > 0);
          if (live) errors.push(`${item.name} is named by ${how} ${rel}, but ${reasonFor(item)}: drop the row, or place the rock`);
        }
      }
      for (const [id, item] of machines) {
        if (new RegExp(`"${id}"`).test(text)) errors.push(`machine ${id} (it works ${item}) is named by ${how} ${rel}, but the island places no rock for ${item}: drop the row, or place the rock`);
      }
    }
  };
  scan(files, "the pack file");
  scan(placed, "the island-placed core prefab");
  return errors;
}

// Core prefab paths a layer names ("{GUID}Prefabs/Processor/Furnace.et") that exist in the core, not the pack.
export function placedCorePrefabs(islandText, coreHas) {
  const out = new Set();
  for (const m of islandText.matchAll(/"\{[0-9A-F]{16}\}(Prefabs\/[^"]+\.et)"/g)) {
    if (coreHas(m[1])) out.add(m[1]);
  }
  return [...out];
}

export function run() {
  const islandText = walk(join(PACK, "Worlds", "MainWorld"), (n) => n.endsWith(".layer"))
    .map((f) => readFileSync(f, "utf8"))
    .join("\n");
  const metas = walk(join(CORE, MINING), (n) => n.endsWith(".et.meta")).map((file) => ({ file, text: readFileSync(file, "utf8") }));
  const unsourced = unsourcedFamilies(miningFamilies(metas), islandText);
  if (unsourced.size === 0) return { errors: [], unsourced };

  const chains = readFileSync(join(CORE, "Configs", "Resources", "ResourceChains.conf"), "utf8");
  const machines = unsourcedMachines(chains, unsourced);

  // Not the worlds (judged by what they place), scripts and tests, text tables, or art assets.
  const files = [];
  for (const dir of ["Configs", "Prefabs", "UI", "Missions"]) {
    for (const f of walk(join(PACK, dir), (n) => /\.(conf|et|layout)$/.test(n))) {
      files.push({ rel: relative(ROOT, f).replace(/\\/g, "/"), text: readFileSync(f, "utf8") });
    }
  }

  const placed = placedCorePrefabs(islandText, (p) => existsSync(join(CORE, p)) && !existsSync(join(PACK, p))).map((p) => ({
    rel: `addons/LifeFramework/${p}`,
    text: readFileSync(join(CORE, p), "utf8"),
  }));

  return { errors: check({ files, placed, unsourced, machines }), unsourced };
}

if (import.meta.url === `file://${process.argv[1].replace(/\\/g, "/")}` || process.argv[1]?.endsWith("validate-pack-unsourced-goods.mjs")) {
  const { errors, unsourced } = run();
  if (errors.length) {
    for (const e of errors) console.error("FAIL " + e);
    console.error(`validate-pack-unsourced-goods: ${errors.length} problem(s)`);
    process.exit(1);
  }
  console.log(`validate-pack-unsourced-goods: OK (no pack board, counter, recipe or offer names the items of an unplaced rock: ${[...unsourced.keys()].join(", ") || "none unplaced"})`);
}
