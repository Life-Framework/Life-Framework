// The draper's dressing: the lighting that lifts the ceiling, the shop window display, wall coat boards with
// hanging garments, the fitting corner, the shoe cabinet, the hat rack, the sale table and the pavement rail
// (the "town-clothing-store-polish-2" pass; it also carries the first pass's hand-written decor).
//
//   node tools/buildings/clothing-store/decor.mjs          rewrite the decor block in the furniture prefab
//   node tools/buildings/clothing-store/decor.mjs --check  exit 1 when the prefab is not what this script writes
//
// Everything here is a base-game prop or a mesh-only garment (LF_MannequinGarment, no inventory item, nothing
// pickable, no store slot). The sold goods stay in ClothingStore_Store.et (spec.json,
// `tools\cli building run clothing-store`). Decor entities carry IDs that start with DC1F, so a re-run removes the
// old block and writes the new one. FIRST_PASS_IDS are the entities the first polish pass wrote by hand; they are
// taken out once and written again from here.
//
// Coordinates are building space: x along the facade, z from the back wall (-3.0) to the street (+7.75), floor at
// y 0.199, ceiling about 4.1 above the world origin. The shop room is x -1.54..5.75, z -3.0..7.25. A wall item's
// yaw faces the room: front +z takes yaw 0 on the back wall, 90 on the x -1.54 wall and -90 on the x 5.75 wall.
import { createHash } from "node:crypto";
import { readFileSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { gameVfs, readText } from "../../lib/game-prefab.mjs";
import { gameDb } from "../pipeline/gamedb.mjs";

const ROOT = join(dirname(fileURLToPath(import.meta.url)), "..", "..", "..");
const FURNITURE = join(ROOT, "addons", "LF_Everon1989", "Prefabs", "Buildings", "ClothingStore", "Furniture", "ShopHouse_E_2I01t_furniture_clothesstore_lf_01.et");
const F = 0.199; // the shop floor
const ID_PREFIX = "DC1F";
const GARMENT = "{8733B60DA6679BF0}Prefabs/Props/Commercial/LF_MannequinGarment.et"; // core: a worn model as a static mesh

// The first pass wrote these by hand, after the coat tree on the pavement (647DDDDFB68BBE27).
const FIRST_PASS_IDS = new Set(
  `2B4D01C04FCE36DF A244D4DCCFAF3DA2 9D9C2A8901335D7A 3086FBF886A5ED69 885CD02BE411F3D9 02A586812023C0F3 8369BD565BAECA04
   C018FCDE11F92D1C 3D435C3FAD704D51 B21BC3828A875E72 57690E2BF9AA1698 5B2884E2AE5D37EC AAF7D44F136FF47C CAFEE0E5B8FEF2A3
   05F272AF39CE1C8A 7EB932E7D49120C9 24171C082A81C130 E51E440BD316C7ED 76B945B659552E8E 2F62AF994CA871A2 5FCF786D617F87A3
   0DC3CFC2430BD458`.split(/\s+/),
);

const P = {
  chandelier: "Prefabs/Props/Furniture/LightCeiling_01_on.et",
  carpet: "Prefabs/Props/Furniture/Carpet_02.et",
  plant: "Prefabs/Props/Furniture/PlantRoom_02.et",
  mirrorFull: "Prefabs/Props/Furniture/MirrorBathroom_02/MirrorBathroom_02.et",
  paintingBig4: "Prefabs/Props/Decorations/Paintings/PaintingBig_01_V4.et",
  paintingBig9: "Prefabs/Props/Decorations/Paintings/PaintingBig_01_V9.et",
  paintingSmall3: "Prefabs/Props/Decorations/Paintings/PaintingSmall_01_V3.et",
  radioPoster: "Prefabs/Props/Commercial/Posters/PosterSingle_27x41_01_RadioEveron.et",
  clock: "Prefabs/Props/Furniture/ClockWall_01.et",
  noticeH3: "Prefabs/Props/Commercial/Posters/Notice_A4_Horizontal_V3.et",
  noticeH1: "Prefabs/Props/Commercial/Posters/Notice_A4_Horizontal_V1.et",
  insurance: "Prefabs/Props/Commercial/Posters/PosterSingle_A3_Insurance_V1.et",
  notebook: "Prefabs/Props/Civilian/Notebook_01_Open.et",
  pen: "Prefabs/Props/Civilian/Pen_01/Pen_01_base.et",
  phone: "Prefabs/Props/Furniture/PhoneTable_01_red.et",
  radio: "Prefabs/Props/Civilian/RadioSmall_01_red.et",
  frame: "Prefabs/Props/Civilian/TablePhotoFrame_01/TablePhotoFrame_01_V3.et",
  sewing: "Prefabs/Props/Civilian/SewingMachine_01.et",
  box06: "Prefabs/Props/Crates/CardboardBox/CardboardBox_06.et",
  box03: "Prefabs/Props/Crates/CardboardBox/CardboardBox_03.et",
  box01: "Prefabs/Props/Crates/CardboardBox/CardboardBox_01.et",
  box05: "Prefabs/Props/Crates/CardboardBox/CardboardBox_05.et",
  box07: "Prefabs/Props/Crates/CardboardBox/CardboardBox_07.et",
  bin: "Prefabs/Props/Garbage/Bins/BinInterior_01_green.et",
  pot: "Prefabs/Props/Decorations/Flowerpots/FlowerpotStreet_01/FlowerpotStreet_01_rect_long_begonia_red.et",
  crate: "Prefabs/Props/Crates/Crate_01_orange.et",
  counter: "Prefabs/Props/Commercial/CounterShop_01.et",
  coatTree: "Prefabs/Props/Furniture/CoatRack_04.et",
  coatBoard: "Prefabs/Props/Furniture/CoatRack_03.et",
  hatBoard: "Prefabs/Props/Furniture/CoatRack_02.et",
  rail: "Prefabs/Props/Furniture/CoatRack_05/CoatRack_05.et",
  shoeCabinet: "Prefabs/Props/Furniture/ShoeCabinet_01.et",
  stool: "Prefabs/Props/Furniture/Stool_02.et",
  basket: "Prefabs/Props/Civilian/BasketWicker_01.et",
};

const U = "Assets/Characters/Uniforms";
const H = "Assets/Characters/HeadGear";
// Garments: { xob, mat: [SourceMaterial, emat path] }. No mat keeps the model's own material.
const G = {
  denimJacket: { xob: `${U}/Jacket_Denim_01/Jacket_Denim_01.xob` },
  raincoatYellow: { xob: `${U}/Jacket_Raincoat_01/Jacket_Raincoat_01.xob` },
  raincoatGreen: { xob: `${U}/Jacket_Raincoat_01/Jacket_Raincoat_01.xob`, mat: ["Jacket_Raincoat_01", `${U}/Jacket_Raincoat_01/Data/Jacket_Raincoat_01_Green.emat`] },
  suitJacketBrown: { xob: `${U}/Jacket_Suit_01/Jacket_Suit_01.xob`, mat: ["Jacket_Suit_01", `${U}/Jacket_Suit_01/Data/Jacket_Suit_01_Brown.emat`] },
  suitJacketGrey: { xob: `${U}/Jacket_Suit_01/Jacket_Suit_01.xob`, mat: ["Jacket_Suit_01", `${U}/Jacket_Suit_01/Data/Jacket_Suit_01_Grey.emat`] },
  turtleBlue: { xob: `${U}/Shirt_Turtleneck_01/Shirt_Turtleneck_01.xob`, mat: ["Shirt_Turtleneck_01", `${U}/Shirt_Turtleneck_01/Data/Shirt_Turtleneck_01_Blue.emat`] },
  cottonShirt: { xob: `${U}/Shirt_CottonShirt_01/Shirt_CottonShirt_01.xob`, mat: ["Shirt_CottonShirt_01", `${U}/Shirt_CottonShirt_01/Data/Shirt_CottonShirt_01_V4.emat`] },
  capBlue: { xob: `${H}/Hat_FlatCap_01/Hat_FlatCap_01.xob`, mat: ["Hat_FlatCap_01", `${H}/Hat_FlatCap_01/Data/Hat_FlatCap_01_Blue.emat`] },
  capGrey: { xob: `${H}/Hat_FlatCap_01/Hat_FlatCap_01.xob`, mat: ["Hat_FlatCap_01", `${H}/Hat_FlatCap_01/Data/Hat_FlatCap_01_LightGray.emat`] },
  capDark: { xob: `${H}/Hat_FlatCap_01/Hat_FlatCap_01.xob`, mat: ["Hat_FlatCap_01", `${H}/Hat_FlatCap_01/Data/Hat_FlatCap_01_DarkGray.emat`] },
  knitRed: { xob: `${H}/Hat_Knitted_01/Hat_Knitted_01.xob`, mat: ["Hat_Knitted_01", `${H}/Hat_Knitted_01/Data/Hat_Knitted_01_red.emat`] },
  knitBlue: { xob: `${H}/Hat_Knitted_01/Hat_Knitted_01.xob`, mat: ["Hat_Knitted_01", `${H}/Hat_Knitted_01/Data/Hat_Knitted_01_blue.emat`] },
  knitBrown: { xob: `${H}/Hat_Knitted_01/Hat_Knitted_01.xob`, mat: ["Hat_Knitted_01", `${H}/Hat_Knitted_01/Data/Hat_Knitted_01_brown.emat`] },
  boots: { xob: "Assets/Characters/Footwear/CombatBoots_Soviet_01/CombatBoots_Soviet_01.xob" },
  // folded (the models the store shelves show; none of these is a sold line)
  foldRaincoatGreen: { xob: `${U}/Jacket_Raincoat_01/Jacket_Raincoat_01_item.xob`, mat: ["Jacket_Raincoat_01_Item", `${U}/Jacket_Raincoat_01/Data/Jacket_Raincoat_01_Item_Green.emat`], h: 0.11 },
  foldFishermanGreen: { xob: `${U}/Pants_Fisherman_01/Pants_Fisherman_01_item.xob`, mat: ["Pants_Fisherman_01_Item", `${U}/Pants_Fisherman_01/Data/Pants_Fisherman_01_Item_Green.emat`], h: 0.09 },
  foldSuitJacketBrown: { xob: `${U}/Jacket_Suit_01/Jacket_Suit_01_item.xob`, mat: ["Jacket_Suit_01_Item", `${U}/Jacket_Suit_01/Data/Jacket_Suit_01_Item_Brown.emat`], h: 0.08 },
  foldSuitPantsBrown: { xob: `${U}/Pants_Suit_01/Pants_Suit_01_item.xob`, mat: ["Pants_Suit_01_item", `${U}/Pants_Suit_01/Data/Pants_Suit_01_Item_Brown.emat`], h: 0.08 },
  foldTrousersLight: { xob: `${U}/Pants_Trousers_01/Pants_Trousers_01_item.xob`, mat: ["Pants_Trousers_01_Item", `${U}/Pants_Trousers_01/Data/Pants_Trousers_01_item_LightBrown.emat`], h: 0.095 },
  foldTurtleBlue: { xob: `${U}/Shirt_Turtleneck_01/Shirt_Turtleneck_01_item.xob`, mat: ["Shirt_Turtleneck_01_Item", `${U}/Shirt_Turtleneck_01/Data/Shirt_Turtleneck_01_Item_Blue.emat`], h: 0.07 },
  foldDenim: { xob: `${U}/Jacket_Denim_01/Jacket_Denim_01_item.xob`, h: 0.067 },
  foldCottonV5: { xob: `${U}/Shirt_CottonShirt_01/Shirt_CottonShirt_01_item.xob`, mat: ["Shirt_Retopo_01_Item", `${U}/Shirt_CottonShirt_01/Data/Shirt_CottonShirt_01_Item_V5.emat`], h: 0.11 },
};

const items = []; // { path, x, y, z, yaw, scale, angles? }
const garments = []; // { key, x, y, z, yaw, scale }
const lights = []; // { x, y, z, color, lv, radius }
const put = (path, x, y, z, yaw = 0, scale = 1, angles = null) => items.push({ path, x, y, z, yaw, scale, angles });
const wear = (key, x, y, z, yaw = 0, scale = 1) => garments.push({ key, x, y, z, yaw, scale });
const glow = (x, y, z, o = {}) => lights.push({ x, y, z, color: "1 0.88 0.68 1", lv: 6.5, radius: 8, ...o });
// a folded pile of n garments on a surface at (x, top, z); each copy turned a little
const pile = (key, n, x, top, z, yaw = 0) => {
  for (let k = 0; k < n; k++) wear(key, x, +(top + k * G[key].h).toFixed(3), z, yaw + (k % 2 ? 4 : -3));
};

// ---- Lighting: the chandeliers' own lights are mostly under them, so the ceiling above read black at night. ----
// Four soft warm point lights under the ceiling (no cone, no shadow) lift the plaster and the wall tops; the three
// chandeliers (each an ambient light and an up light of its own) and the two window spots keep the rest.
glow(0.3, 3.3, 3.4);
glow(4.6, 3.3, 3.0);
glow(3.3, 3.3, -1.7, { lv: 6, radius: 6.5 });
glow(1.0, 3.3, 6.0, { lv: 6, radius: 5.5 });

put(P.chandelier, 2.7, 4, 5.6);
put(P.chandelier, 1.5, 3.99, -0.3);
put(P.chandelier, 2.7, 3.99, 1.8);

// ---- The window display (street side, z 6.4..7.15) ----
put(P.carpet, 2.75, F, 6.8, 90);
// a third mannequin in the middle, the denim one left and the raincoat one right come from the spec fixtures
// riser at the left end: two crates, shoe boxes and a price card
put(P.crate, 0.98, F, 6.95, 4);
put(P.crate, 0.98, F + 0.223, 6.95, -6);
put(P.box01, 0.88, F + 0.446, 6.93, 90, 0.5);
put(P.box07, 1.08, F + 0.446, 6.96, 84, 0.5);
put(P.box05, 0.88, F + 0.446 + 0.213, 6.93, 96, 0.5);
put(P.noticeH1, 0.98, F + 0.12, 7.16, 0, 0.7);
// riser at the right end: a crate with folded knitwear and a basket of caps; a hat stand beyond
put(P.crate, 4.35, F, 6.35, -8);
pile("foldTurtleBlue", 3, 4.35, F + 0.223, 6.35, 12);
put(P.basket, 4.72, F, 6.95, 0, 0.55);
wear("knitRed", 4.72, F + 0.2, 6.95, 20);
wear("knitBlue", 4.78, F + 0.18, 6.9, -30);
put(P.coatTree, 0.45, F, 6.0);
wear("capGrey", 0.45, F + 0.05, 6.0, 0);
wear("capBlue", 0.6, F - 0.2, 6.05, 40);
wear("knitBrown", 0.32, F - 0.2, 5.95, -40);

// ---- Walls ----
// x 5.75 wall (faces -x): coat boards with a coat each, the long mirror by the window, paintings over the shelves
put(P.coatBoard, 5.745, F, -0.55, -90);
wear("raincoatYellow", 5.53, F + 0.1, -0.55, -90);
put(P.coatBoard, 5.745, F, 0.3, -90);
wear("suitJacketGrey", 5.53, F + 0.1, 0.3, -90);
put(P.paintingBig9, 5.765, 2.45, 4.6, -90);
put(P.paintingSmall3, 5.765, 2.4, 2, -90);
put(P.mirrorFull, 5.745, F + 1.3, 6.35, -90, 1, "0 -90 90"); // full length: the 2.1 m wide mirror stood on its side
// x -1.54 wall (faces +x): coat boards by the hall door, a painting over the shelf, the poster by the window
put(P.coatBoard, -1.535, F, 1.9, 90);
wear("denimJacket", -1.32, F + 0.1, 1.9, 90);
put(P.coatBoard, -1.535, F, 2.8, 90);
wear("raincoatGreen", -1.32, F + 0.1, 2.8, 90);
put(P.paintingBig4, -1.535, 2.35, 4.4, 90);
put(P.radioPoster, -1.535, 1.6, 6.4, 90);
// back wall z -3.0 (faces +z): clock, notices, and the hat rack
put(P.clock, 2.3, 2.6, -3.015);
put(P.noticeH3, 0.9, 1.7, -2.995);
put(P.insurance, 4.85, 1.7, -2.995);
put(P.hatBoard, 3.95, F, -2.995, 0);
wear("capDark", 3.7, F + 0.0, -2.88, 0);
wear("knitBrown", 3.97, F - 0.05, -2.88, 10);
wear("capBlue", 4.2, F + 0.0, -2.88, -8);

// ---- The fitting corner and the shoe corner ----
put(P.stool, 5.05, F, 6.2, 90);
put(P.shoeCabinet, -1.53, F, 5.95, 90); // low shoe cabinet, west wall near the window: boots on top
wear("boots", -1.3, F + 0.561, 6.25, 90);
wear("boots", -1.3, F + 0.561, 6.65, 80);
put(P.plant, 5.3, F, 7);

// ---- The till counter: ledger, pen, phone, radio, a photograph; the clerk's side ----
put(P.notebook, 1.75, 1.238, -0.25, 180);
put(P.pen, 1.98, 1.245, -0.2, 25);
put(P.phone, 2.1, 1.238, -0.65, 200);
put(P.radio, 1, 1.238, -0.75, 0);
put(P.frame, 2.25, 1.238, -0.05, 195);
put(P.sewing, 0, F, -2.5, 0);
put(P.box06, -0.95, F, -2.5, 0);
put(P.box03, -0.95, 0.624, -2.5, 10);
put(P.bin, 2.6, F, -1.15, 0);

// ---- The display counters: a framed fashion plate in a corner of each ----
put(P.frame, 2.86, 1.238, 2.2, 200);
put(P.frame, 2.87, 1.238, 4.45, 160);

// ---- The sale table: a spare counter with unsold goods folded on it ----
put(P.counter, 4.05, F, 3.4, 90);
pile("foldRaincoatGreen", 2, 4.05, F + 1.039, 2.75, 90);
pile("foldFishermanGreen", 3, 4.05, F + 1.039, 3.3, 90);
pile("foldSuitJacketBrown", 4, 4.05, F + 1.039, 3.85, 90);
pile("foldSuitPantsBrown", 3, 3.75, F + 1.039, 4.0, 90);
put(P.noticeH1, 3.56, F + 1.039 + 0.12, 3.4, 90, 0.8, "0 90 0");

// ---- The pavement: the coat tree (already in the furniture prefab) with a raincoat out for the street ----
wear("raincoatYellow", 5.2, 0.28, 8.7, 200);
put(P.pot, 2.7, 0, 8.45);

// ---------------------------------------------------------------------------------------------------
const hex16 = (s) => ID_PREFIX + createHash("sha1").update(s).digest("hex").slice(0, 12).toUpperCase();
const num = (n) => String(+(+n).toFixed(3));

async function build() {
  const vfs = await gameVfs();
  const db = gameDb();
  if (!db) throw new Error("the game's resource database is not readable here (ENFUSION_GAME_PATH)");
  const guid = (path) => {
    const g = db.guidOf(path);
    if (!g) throw new Error(`no GUID for ${path} in the game's resource database`);
    return `{${g}}${path}`;
  };
  const memo = new Map();
  const info = (path) => {
    if (memo.has(path)) return memo.get(path);
    const text = readText(ROOT, vfs, path);
    const cls = (text && /^(\w+)/.exec(text.trimStart()))?.[1];
    if (!cls) throw new Error(`cannot read the root class of ${path}`);
    const v = { ref: guid(path), cls };
    memo.set(path, v);
    return v;
  };
  const rows = [];
  const place = (lines, o) => {
    if (o.scale !== 1) lines.push(`   scale ${num(o.scale)}`);
    lines.push(`   coords ${num(o.x)} ${num(o.y)} ${num(o.z)}`);
    if (o.angles) lines.push(`   angles ${o.angles}`);
    else if (o.yaw) lines.push(`   angleY ${num(o.yaw)}`);
    lines.push("  }");
    return lines.join("\n");
  };
  items.forEach((it, i) => {
    const { ref, cls } = info(it.path);
    rows.push(place([`  ${cls} : "${ref}" {`, `   ID "${hex16(`clothing-store|decor|${i}|${it.path}`)}"`], it));
  });
  garments.forEach((g, i) => {
    const def = G[g.key];
    if (!def) throw new Error(`unknown garment ${g.key}`);
    const lines = [`  GenericEntity : "${GARMENT}" {`, `   ID "${hex16(`clothing-store|garment|${i}|${g.key}`)}"`, "   components {", '    MeshObject "{9F2610C81BC2FC77}" {', `     Object "${guid(def.xob)}"`];
    if (def.mat) {
      lines.push("     Materials {", `      MaterialAssignClass "{${hex16(`clothing-store|garment-mat|${i}|${g.key}`).slice(4).padStart(16, "0")}}" {`, `       SourceMaterial "${def.mat[0]}"`, `       AssignedMaterial "${guid(def.mat[1])}"`, "      }", "     }");
    }
    lines.push("    }", "   }");
    rows.push(place(lines, { ...g, scale: g.scale }));
  });
  lights.forEach((l, i) => {
    rows.push(
      [
        "  LightEntity {",
        `   ID "${hex16(`clothing-store|decor|light|${i}`)}"`,
        `   coords ${num(l.x)} ${num(l.y)} ${num(l.z)}`,
        `   Color ${l.color}`,
        `   LV ${l.lv}`,
        "   ClipEV -10",
        `   Radius ${l.radius}`,
        "   NearPlane 0.3",
        "   LensFlareType Disabled",
        "  }",
      ].join("\n"),
    );
  });
  return rows.join("\n");
}

// Remove every child entity whose ID starts with the decor prefix (or is a first-pass hand-written one), then
// append the new block.
export function rewrite(text, block) {
  const lines = text.split("\n");
  const out = [];
  for (let i = 0; i < lines.length; i++) {
    const open = /^ {2}\w+(?: : "[^"]+")? \{$/.exec(lines[i]);
    const id = open && /^ {3}ID "([0-9A-F]+)"$/.exec(lines[i + 1] ?? "")?.[1];
    if (id && (id.startsWith(ID_PREFIX) || FIRST_PASS_IDS.has(id))) {
      while (i < lines.length && lines[i] !== "  }") i++; // skip to the closing line of this entity
      continue;
    }
    out.push(lines[i]);
  }
  const body = out.join("\n");
  const at = body.lastIndexOf("\n }\n}");
  if (at < 0) throw new Error("the furniture prefab has no child list");
  return `${body.slice(0, at)}\n${block}${body.slice(at)}`;
}

const text = readFileSync(FURNITURE, "utf8");
const next = rewrite(text, await build());
if (process.argv.includes("--check")) {
  const same = next === text;
  console.log(same ? "decor: the furniture prefab is current" : "decor: the furniture prefab differs; run node tools/buildings/clothing-store/decor.mjs");
  process.exit(same ? 0 : 1);
}
writeFileSync(FURNITURE, next);
console.log(`decor: ${items.length} prop(s), ${garments.length} garment(s), ${lights.length} light(s) written to ${FURNITURE.replace(ROOT, "").replace(/\\/g, "/")}`);
