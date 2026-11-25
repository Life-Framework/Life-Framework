// Stocks the empty shelves of the gun store, the hardware store and the general store (visual sweep 2026-11-25,
// card 2): the `<shop>-host-*` dressing rows of `tools/town/kits/<kit>.json` and the mesh-only pack prefabs they stand.
// It is the pharmacy's method (tools/town/gen-pharmacy-shelves.mjs) for three more shops: one single item per spot,
// directly on the wood, packed along each board, never a pile. Every prefab is a bare `MeshObject` (no pickup, no
// store line, nothing sellable): static display only. Guns stay on the shop's own wall rack and display case.
//
//   node tools/town/gen-shop-shelves.mjs [<shop>] [--check]     (a shop is a file in tools/town/shop-shelves/)
//   node tools/cli.mjs town fit saint-philippe <slot> --no-look      (then writes the layer)
//
// The shelf poses are the vanilla furniture prefab's own units (`ShelfUniversal_01_wide`) read with
// `tools/town/shelf-stock.mjs instancesOf`; a spot that a piece of the furniture prefab's own stock already
// occupies is left out. Board heights are the measured tops of the shelf models (`tools\cli measure --surfaces`)
// plus the shell floor, the same numbers the pharmacy proved with `look contact`. Every width below is the measured
// bound of the mesh times its scale. Deterministic: the same bytes twice; `--check` exits 1 when files differ.
// The work runs only when this file is the command: CI imports every tools module.
import { createHash } from "node:crypto";
import { existsSync, readFileSync, readdirSync, writeFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { instancesOf } from "./shelf-stock.mjs";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");
const PACK = join(ROOT, "addons/LF_Everon1989");
const DIR = "Prefabs/Props/Commercial";

// Board tops above the building origin: the measured shelf tops (0.588, 1.081, 1.574) plus the interior floor 0.196 and a
// 5 mm rest; the island audit measured the first pass 3 cm high, so no extra lift.
const BOARD_Y = { wide: { 1: 0.789, 2: 1.282, 3: 1.775 } };
const FORWARD = 0.03;
const GAP = 0.05;

// A shop is one file in tools/town/shop-shelves/: its `goods` and its `shop` (kit, row prefix, furniture prefab, stock cycle).
// Its units come from the vanilla furniture prefab, ground floor only; its stock cycle is a list of goods keys.
export const SHOP_NAMES = readdirSync(join(ROOT, "tools/town/shop-shelves")).filter((f) => f.endsWith(".mjs")).map((f) => f.slice(0, -4));
let G = {};

const r3 = (n) => Math.round(n * 1000) / 1000;
const widthOf = (k) => G[k].w * G[k].scale;
const hex = (seed) => createHash("sha1").update(seed).digest("hex").slice(0, 16).toUpperCase();

// The ground-floor shelf units of a furniture prefab as { id, at, yaw, len, kind }: `at` is the middle of a board (x, z) in the
// building frame. A wide unit's origin sits at the end of its length.
export function unitsOf(text) {
  const out = [];
  const rad = (y) => (y * Math.PI) / 180;
  const frame = (o, a, f, yaw) => {
    const along = [Math.cos(rad(yaw)), -Math.sin(rad(yaw))];
    const front = [Math.sin(rad(yaw)), Math.cos(rad(yaw))];
    return [o.at[0] + along[0] * a + front[0] * f, o.at[2] + along[1] * a + front[1] * f];
  };
  const kinds = [
    ["wide", "Prefabs/Props/Commercial/ShelfUniversal_01_wide.et", -0.721, 1.22],
  ];
  for (const [kind, prefab, shift, len] of kinds) {
    const list = instancesOf(text, prefab).filter((u) => u.at[1] < 1).sort((a, b) => a.at[0] - b.at[0] || a.at[2] - b.at[2]);
    list.forEach((u, i) => {
      const [x, z] = frame(u, shift, 0.213, u.yaw);
      out.push({ id: `${kind[0]}${i + 1}`, at: [x, z], yaw: u.yaw, len, kind });
    });
  }
  return out;
}

// The pieces of the furniture prefab's own stock: { at: [x, y, z], r } to keep our goods off them.
export function obstaclesOf(text) {
  const names = [...new Set([...text.matchAll(/"\{[0-9A-F]{16}\}([^"]+\.et)"/g)].map((m) => m[1]))];
  const out = [];
  for (const n of names) {
    if (/Shelf|Lamp|Notice|Poster|Light|Wardrobe|Cupboard|Cabinet|Radiator|Painting|Carpet|Chair|Table/.test(n)) continue;
    for (const i of instancesOf(text, n)) if (i.at[1] > 0.4 && i.at[1] < 2.2) out.push({ at: i.at, r: 0.12 });
  }
  return out;
}

// One board's rows: goods packed along `len`, the leftover split at both ends; spots over an obstacle are dropped.
export function boardRows(shop, unit, board, start, obstacles) {
  const kinds = [];
  let used = 0;
  for (let i = 0; i < shop.cycle.length * 2; i++) {
    const k = shop.cycle[(start + i) % shop.cycle.length];
    if (shop.only?.[k] && !shop.only[k].includes(board)) continue;
    const w = widthOf(k);
    if (used + (kinds.length ? GAP : 0) + w > unit.len) {
      const fit = shop.cycle
        .filter((c) => !(shop.only?.[c] && !shop.only[c].includes(board)) && used + GAP + widthOf(c) <= unit.len)
        .sort((a, b) => widthOf(b) - widthOf(a))[0];
      if (fit) { kinds.push(fit); used += GAP + widthOf(fit); }
      break;
    }
    used += (kinds.length ? GAP : 0) + w;
    kinds.push(k);
  }
  const rad = (unit.yaw * Math.PI) / 180;
  const along = [Math.cos(rad), -Math.sin(rad)];
  const front = [Math.sin(rad), Math.cos(rad)];
  const y0 = (unit.boardY ?? BOARD_Y[unit.kind])[board];
  let t = -used / 2;
  const rows = [];
  kinds.forEach((k, i) => {
    const w = widthOf(k);
    const mid = t + w / 2;
    t += w + GAP;
    const x = unit.at[0] + along[0] * mid + front[0] * FORWARD;
    const z = unit.at[1] + along[1] * mid + front[1] * FORWARD;
    const y = y0 + G[k].lift * G[k].scale;
    if (obstacles.some((o) => Math.hypot(o.at[0] - x, o.at[2] - z) < o.r + w / 2 && Math.abs(o.at[1] - y) < 0.3)) return;
    const row = {
      id: `${shop.prefix}-${unit.id}-b${board}-${i + 1}`,
      name: `${shop.prefix.replace(/(^|-)(\w)/g, (_, a, c) => `${a ? "_" : ""}${c.toUpperCase()}`)}_${unit.id}_b${board}_${i + 1}`,
      prefab: `{${GUIDS[k]}}${DIR}/${G[k].file}`,
      role: "dressing",
      verb: "on floor",
      offset: [r3(x), r3(y), r3(z)],
      yaw: unit.yaw + (G[k].yawOff ?? 0),
      free: true,
      wall: "free",
      floor: "f0",
    };
    if (G[k].scale !== 1) row.scale = G[k].scale;
    rows.push(row);
  });
  return rows;
}

let GUIDS = {};

const metaText = (guid, path) => `MetaFileClass {
 Name "{${guid}}${path}"
 Configurations {
  EntityTemplateResourceClass PC {
  }
  EntityTemplateResourceClass XBOX_ONE : PC {
  }
  EntityTemplateResourceClass XBOX_SERIES : PC {
  }
  EntityTemplateResourceClass PS4 : PC {
  }
  EntityTemplateResourceClass PS5 : PC {
  }
  EntityTemplateResourceClass HEADLESS : PC {
  }
 }
}
`;

// A prefab's resource GUID: the one in its .meta, else one derived from the file name, so two shop cards that stock the same
// goods write the same bytes and merge.
function guidOf(file) {
  const meta = join(PACK, DIR, file + ".meta");
  const g = existsSync(meta) ? /Name "\{([0-9A-F]{16})\}/.exec(readFileSync(meta, "utf8"))?.[1] : null;
  return g ?? hex(`${file}#guid`);
}

// The planned files for one shop: { path, text }[] and the row count.
export async function plan(name) {
  if (!SHOP_NAMES.includes(name)) throw new Error(`no shop '${name}' (${SHOP_NAMES.join(", ")})`);
  const mod = await import(`./shop-shelves/${name}.mjs`);
  const shop = mod.shop;
  G = mod.goods;
  const files = [];
  const used = new Set();
  // Units and the furniture's own stock come from the vanilla furniture prefab, or a shop lists `units` (and `obstacles`) itself.
  const text = shop.furniture ? readFileSync(join(PACK, shop.furniture), "utf8") : "";
  const units = [...(shop.units ?? unitsOf(text)), ...(shop.extraUnits ?? [])];
  const obstacles = shop.obstacles ?? obstaclesOf(text);
  const rows = [];
  let n = 0;
  for (const k of Object.keys(G)) GUIDS[k] = guidOf(G[k].file);
  for (const unit of units) {
    for (const b of [1, 2, 3]) {
      const made = boardRows(shop, unit, b, (n++ * 5) % shop.cycle.length, obstacles);
      if (!(shop.dropUnits ?? []).includes(unit.id)) rows.push(...made.filter((r) => !(shop.dropRows ?? []).includes(r.id)));
    }
  }
  for (const r of rows) used.add(Object.keys(G).find((k) => r.prefab.endsWith(G[k].file)));
  for (const k of used) {
    const p = G[k];
    files.push({
      path: join(PACK, DIR, p.file),
      text: `GenericEntity {\n ID "${hex(`${p.file}#e`)}"\n components {\n  MeshObject "{${hex(`${p.file}#m`)}}" {\n   Object "${p.xob}"\n  }\n }\n coords 0 0 0\n}\n`,
    });
    files.push({ path: join(PACK, DIR, p.file + ".meta"), text: metaText(GUIDS[k], `${DIR}/${p.file}`) });
  }
  const kitPath = join(ROOT, shop.kit);
  const kit = JSON.parse(readFileSync(kitPath, "utf8"));
  kit.rows = [...kit.rows.filter((r) => !r.id.startsWith(`${shop.prefix}-`)), ...rows];
  files.push({ path: kitPath, text: JSON.stringify(kit, null, 2) + "\n" });
  return { files, rows: rows.length, units: units.length };
}

async function main() {
  const check = process.argv.includes("--check");
  const names = process.argv.slice(2).filter((a) => !a.startsWith("--"));
  for (const name of names.length ? names : SHOP_NAMES) {
    const { files, rows, units } = await plan(name);
    let differ = 0;
    for (const f of files) {
      const same = existsSync(f.path) && readFileSync(f.path, "utf8") === f.text;
      if (!same) differ++;
      if (!check) writeFileSync(f.path, f.text);
    }
    console.log(`${check ? "check" : "wrote"} ${name}: ${rows} shelf rows on ${units} unit(s), ${files.length} files, ${differ} differ`);
    if (check && differ) process.exitCode = 1;
  }
}

if (process.argv[1] && resolve(process.argv[1]) === fileURLToPath(import.meta.url)) await main();
