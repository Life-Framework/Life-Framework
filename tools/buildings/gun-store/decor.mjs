// The gunsmith's dressing: lamps and lights, wall decor, stock on the back shelves, the workbench corner,
// counter clutter, floor clutter and the pavement outside (the "town-gunstore-polish" pass).
//
//   node tools/buildings/gun-store/decor.mjs          rewrite the decor block in the furniture prefab
//   node tools/buildings/gun-store/decor.mjs --check  exit 1 when the prefab is not what this script writes
//   node tools/buildings/gun-store/decor.mjs --report print every prop whose height the snap changed
// Heights: a prop's y is a hint; surface.mjs snaps it onto the top under it (`on:` names the support), so a counter-top
// prop never hovers over its counter. `toolsi building check gun-store` proves it.
//
// Everything here is a base-game prop standing on its own: no inventory item, nothing pickable, no store
// slot. The sold goods stay in GunStore_Store.et (spec.json, `tools\cli building run gun-store`).
// Decor entities carry IDs that start with DC1F, so a re-run removes the old block and writes the new one.
//
// Coordinates are building space: x along the facade, z from the back wall (-3.0) to the street (+7.7),
// floor at y 0.199, and the ceiling about 3.9 above it. The shop room is x -1.54..5.75, z -3.0..7.25.
// A wall item's yaw faces the room: a poster (front +z) takes yaw 0 on the back wall, 90 on the x -1.54
// wall and -90 on the x 5.75 wall; a trophy board (it sticks out toward -z) takes 180, -90 and 90.
import { createHash } from "node:crypto";
import { readFileSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { gameVfs, readText } from "../../lib/game-prefab.mjs";
import { gameDb } from "../pipeline/gamedb.mjs";
import { prefabChildren, snapItems } from "../pipeline/surface.mjs";

const ROOT = join(dirname(fileURLToPath(import.meta.url)), "..", "..", "..");
const FURNITURE = join(ROOT, "addons", "LF_Everon1989", "Prefabs", "Buildings", "GunStore", "Furniture", "GunStore_furniture_01.et");
const STORE = join(ROOT, "addons", "LF_Everon1989", "Prefabs", "Buildings", "GunStore", "GunStore_Store.et"); // the gun counter and the till stand in the store prefab
const COUNTER = "B9C79E66EFB96EA3"; // the LF_GunCounter entity in GunStore_Store.et
const F = 0.199; // the shop floor
const ID_PREFIX = "DC1F";

const P = {
  lamp: "Prefabs/Props/Furniture/LampFluorescent/LampFluorescent_02.et",
  wallLampOn: "Prefabs/Structures/BuildingParts/Lights/LampIndustrial_01_on_exterior.et",
  skull1: "Prefabs/Props/Forest/DeerSkull_01/DeerSkull_01_board.et",
  skull2: "Prefabs/Props/Forest/DeerSkull_01/DeerSkull_02_board.et",
  skull3: "Prefabs/Props/Forest/DeerSkull_01/DeerSkull_03_board.et",
  posterA1: "Prefabs/Props/Commercial/Posters/PosterSingle_E_A1_rallyeveron.et",
  radioPoster: "Prefabs/Props/Commercial/Posters/PosterSingle_27x41_01_RadioEveron.et",
  forest1: "Prefabs/Props/Commercial/Posters/PosterSingle_A3_Forest_V1.et",
  forest2: "Prefabs/Props/Commercial/Posters/PosterSingle_A3_Forest_V2.et",
  clock: "Prefabs/Props/Civilian/ClockUniversal_01.et",
  toolbox: "Prefabs/Props/Industrial/ToolBox_01/ToolBox_01_red.et",
  can: "Prefabs/Props/Civilian/PaintCan_01.et",
  box1: "Prefabs/Props/Crates/CardboardBox/CardboardBox_01.et",
  box2: "Prefabs/Props/Crates/CardboardBox/CardboardBox_02.et",
  box5: "Prefabs/Props/Crates/CardboardBox/CardboardBox_05.et",
  box6: "Prefabs/Props/Crates/CardboardBox/CardboardBox_06.et",
  bin: "Prefabs/Props/Garbage/Bins/BinInterior_01_green.et",
  mat: "Prefabs/Props/Furniture/Carpet_01.et",
  notebook: "Prefabs/Props/Civilian/Notebook_01_Open.et",
  radio: "Prefabs/Props/Civilian/RadioSmall_01_black.et",
  phone: "Prefabs/Props/Furniture/PhoneTable_01_grey.et",
  frame: "Prefabs/Props/Civilian/TablePhotoFrame_01/TablePhotoFrame_01_V2.et",
  crate: "Prefabs/Props/Crates/Crate_01_orange.et",
  bucket: "Prefabs/Props/Civilian/Bucket_01.et",
  broom: "Prefabs/Props/Civilian/Broom_01.et",
  bench3: "Prefabs/Props/Furniture/BenchWooden_03.et",
  binStreet: "Prefabs/Props/Garbage/Bins/BinStreet_01.et",
  pot: "Prefabs/Props/Decorations/Flowerpots/FlowerpotStreet_01/FlowerpotStreet_01_octa_begonia_pink.et",
  // the second pass: the middle of the shop, the right wall, the repair corner
  cabinet: "Prefabs/Props/Furniture/CupboardOld_01/CupboardOld_01_medium.et",
  table: "Prefabs/Props/Furniture/Table_01.et",
  suitcase: "Prefabs/Props/Civilian/Suitcase_04_open.et",
  coatTree: "Prefabs/Props/Furniture/CoatRack_01.et",
  floorLamp: "Prefabs/Props/Furniture/LampInterior_02/LampInterior_02_Orange.et",
  bench: "Prefabs/Props/Industrial/Workbench_02.et",
  vice: "Prefabs/Props/Industrial/Workbench_Vice_01.et",
  deskLamp: "Prefabs/Props/Furniture/LightTable_01.et",
  stool: "Prefabs/Props/Furniture/Chair_02/Chair_02_red.et",
  notice: "Prefabs/Props/Commercial/Posters/Notice_A4_Vertical_V3.et",
};
// The base game's lit diffuser for the fluorescent lamps' "cover" slot (the lamp meshes ship dark).
const LAMP_COVER_LIT = "Assets/Props/Furniture/LampFluorescent_02/Data/LampFluorescent_02_cover_emissive.emat";
// The two lamps the building pipeline flattened into the furniture prefab: their covers glow too.
const VANILLA_LAMPS = ["52640E95D114237B", "E99B992F704FA5F1"];

const items = []; // { path, x, y, z, yaw, scale }
const lights = []; // { x, y, z, angles, color, lv, radius, angle }
// y is a hint: every prop is snapped onto the top under it (tools/buildings/pipeline/surface.mjs). `o.on` names its support
// (an `id` given to another row, or an entity ID of the prefab or the store), `o.id` names this row as a support.
const put = (path, x, y, z, yaw = 0, scale = 1, lit = false, o = {}) => items.push({ path, x, y, z, yaw, scale, lit, ...o });
const spot = (x, y, z, o = {}) => lights.push({ type: "Spot", x, y, z, angles: "-90 0 0", color: "1 0.86 0.64 1", lv: 5.5, radius: 7, angle: 140, ...o });
// A point light has no cone: it is what lights the ceiling and the upper walls, which the downward spots never reach.
const glow = (x, y, z, o = {}) => lights.push({ type: "Point", x, y, z, color: "1 0.88 0.68 1", lv: 2.2, radius: 5.5, ...o });
const lamp = (x, z) => put(P.lamp, x, 3.918, z, 0, 1, true);

// ---- Lights: ceiling lamps over the shop floor (the vanilla set's two lamps at x 3 are dead meshes) ----
spot(2.99, 3.7, 4.668); // over the window end of the shop (lamp mesh already in the furniture)
spot(2.962, 3.7, 0.922); // over the middle (lamp mesh already in the furniture)
lamp(0.4, 3.3);
spot(0.4, 3.7, 3.3);
lamp(3.0, -0.5); // the lamp over the till and the clerk
spot(3.0, 3.7, -0.5, { color: "1 0.82 0.55 1", lv: 6.5, radius: 5, angle: 110 });
lamp(3.0, 6.3); // the window display: a spot on the rack the street sees
spot(3.0, 3.7, 6.3, { color: "1 0.93 0.8 1", lv: 7.5, radius: 4.5, angle: 90 });
// The black ceiling at night (#2012 second pass): every spot above points down and the ceiling is 0.2 m over it,
// so nothing reached it and the covers were dark meshes. A soft point light hangs under three of the fittings
// (it lights the ceiling above it and the walls) and the covers of all five lamps carry the lit material.
glow(2.99, 3.0, 4.668);
glow(2.962, 3.0, 0.922);
glow(0.4, 3.0, 3.3);

// ---- Walls ----
// x -1.54 wall (faces +x): a trophy beside the door to the entry hall. Behind it, for z below -0.4, is the stair
// room: nothing of ours goes there (an earlier workbench corner did, and was removed).
put(P.skull2, -1.53, 2.45, 0.15, -90);
// back wall z -3.0 (faces +z): trophies above the stock shelves
put(P.skull1, 1.8, 3.1, -2.99, 180);
put(P.skull3, 3.0, 3.25, -2.99, 180);
// x 5.75 wall (faces -x): a trophy over the rifle cabinet, posters over the gear shelf and in the back corner
put(P.skull1, 5.74, 2.3, 0.6, 90);
put(P.posterA1, 5.72, 2.6, 3.35, -90);
put(P.forest1, 5.72, 2.7, 2.65, -90);
put(P.forest2, 5.72, 2.7, 4.05, -90);
put(P.clock, 5.72, 3.2, 5.3, -90, 1.15);
put(P.radioPoster, 5.72, 3.1, 5.9, -90);
put(P.posterA1, 5.72, 1.95, -1.3, -90);
put(P.forest2, 5.72, 2.0, -2.0, -90);

// ---- The counter top: ledger, phone, radio, a photograph ----
const onCounter = { on: COUNTER };
put(P.notebook, 3.3, 1.238, -0.1, 15, 1, false, onCounter);
put(P.phone, 3.5, 1.238, -0.75, 200, 1, false, onCounter);
put(P.radio, 3.1, 1.238, -0.9, 180, 1, false, onCounter);
put(P.frame, 3.62, 1.238, -0.3, 195, 1, false, onCounter);

// ---- The floor ----
put(P.box1, 5.4, F, -1.8, 90);
put(P.box2, 5.4, F + 0.41, -1.8, 80);
put(P.box5, 5.4, F, -2.45, 90);
put(P.bin, 4.1, F, -0.65);
put(P.mat, -0.75, F, 1.5, 90);
put(P.bucket, 4.2, F, -2.5);
put(P.broom, 4.6, F + 0.01, -2.85, 0);
// window end: crates and a box beside the rack the street sees
put(P.crate, 0.45, F, 6.85);
put(P.crate, 0.45, F + 0.223, 6.85, 20);
put(P.box6, 0.45, F + 0.446, 6.85, 100, 0.9);
put(P.toolbox, 1.45, F, 6.85, 10);

// ---- Second pass: the middle of the shop, the right wall, the repair corner ----
// The shop is the room x -1.54..5.75, z -3..7.25 (the stair room is behind the x -1.54 wall, z below -0.4). The lane
// from the front door (near x 0) to the till (x 3, z -0.5) stays open: the table stands right of it.
// Right wall (x -1.54, faces +x): between the notice board (z 2.6) and the hat shelf (z 4.25) a tall glass-fronted
// cupboard stands as the gun cabinet, a deer mount over it. The pivot is the cabinet's right end; at yaw 90 it runs
// +z from the pivot and its depth reaches into the room.
put(P.cabinet, -1.53, F, 3.0, 90, 0.85);
put(P.skull1, -1.53, 2.6, 3.62, -90);
// The middle: a display table of cleaning kits and cases, a coat tree and a case beside it.
put(P.table, 3.35, F, 3.1, 0, 1, false, { id: "table" });
const TT = F + 0.92; // the table top (a hint; the props are snapped onto the real top)
const onTable = { on: "table" };
put(P.suitcase, 2.95, TT, 3.1, 20, 1, false, onTable);
put(P.toolbox, 3.75, TT, 2.95, -10, 1, false, onTable);
put(P.can, 3.3, TT, 3.35, 0, 0.8, false, onTable);
put(P.can, 3.5, TT, 3.42, 40, 0.8, false, onTable);
put(P.can, 3.4, TT, 3.2, 80, 0.8, false, onTable);
put(P.notebook, 3.95, TT, 3.4, 80, 1, false, onTable);
put(P.coatTree, 4.65, F, 4.25);
// The repair corner, inside the shop room: the shop room's back-left is NOT the shop (the stair room is behind a
// partition at x 0.9, z -3..-0.4; the first pass and this one's first try put a bench there). The partition's shop
// face (x 0.9, faces +x) is bare, so the workbench stands along it, left of the till counter (x 1.9), with a vice, a
// toolbox, solvent tins and a desk lamp, a stool, notices and a deer mount over it.
put(P.bench, 1.28, F, -1.75, 90, 0.85, false, { id: "bench" });
const BT = F + 0.85; // the bench top (a hint; the props are snapped onto the real top)
const onBench = { on: "bench" };
put(P.vice, 1.5, BT, -2.5, 90, 1, false, onBench);
put(P.toolbox, 1.2, BT, -1.5, 90, 1, false, onBench);
put(P.can, 1.2, BT, -2.05, 0, 0.8, false, onBench);
put(P.can, 1.32, BT, -2.12, 50, 0.8, false, onBench);
put(P.notebook, 1.35, BT, -1.0, 100, 1, false, onBench);
put(P.deskLamp, 1.1, BT, -2.6, 70, 1, false, onBench);
put(P.stool, 2.0, F, -1.75, -90);
put(P.notice, 0.93, 1.85, -1.35, 90);
put(P.notice, 0.93, 1.85, -1.8, 90);
put(P.skull2, 0.93, 2.55, -2.2, 90);
// A floor lamp on the right wall between the hall door and the notice board.
put(P.floorLamp, -1.2, F, 2.0, 90);
glow(1.1, BT + 0.4, -2.55, { color: "1 0.84 0.6 1", lv: 2.6, radius: 3.5 });
glow(-1.2, 1.7, 2.0, { color: "1 0.8 0.52 1", lv: 2.6, radius: 4.2 });

// ---- The back shelves: stock boxes, tool cases and solvent tins ----
// unit: origin and yaw of the vanilla shelf; boards are y above the shelf origin, x the usable width.
const BOARDS = [0.112, 0.588, 1.081, 1.574, 2.067];
const shelf = (unit, patternFor) => {
  const t = (unit.yaw * Math.PI) / 180;
  BOARDS.forEach((y, b) => {
    for (const [path, off, itemYaw, scale = 1] of patternFor(b, unit.width)) {
      const lx = unit.x0 + off;
      const lz = 0.213;
      const x = unit.ox + lx * Math.cos(t) + lz * Math.sin(t);
      const z = unit.oz - lx * Math.sin(t) + lz * Math.cos(t);
      put(path, +x.toFixed(3), +(F + y).toFixed(3), +z.toFixed(3), unit.yaw + itemYaw, scale);
    }
  });
};
const wide = (b) => {
  const set = [
    [[P.box1, 0.33, 90, 0.9], [P.box5, 0.97, 90, 0.9]],
    [[P.toolbox, 0.3, 90, 1], [P.can, 0.75, 0, 0.8], [P.can, 0.98, 30, 0.8], [P.can, 1.2, 70, 0.8]],
    [[P.box2, 0.33, 90, 0.9], [P.can, 0.8, 0, 0.8], [P.can, 1.03, 50, 0.8], [P.can, 1.24, 10, 0.8]],
    [[P.toolbox, 0.3, 90, 1], [P.box6, 0.95, 90, 0.9]],
    [[P.box5, 0.33, 90, 0.9], [P.box1, 0.97, 90, 0.9]],
  ];
  return set[b];
};
const narrow = (b) => {
  const set = [
    [[P.box6, 0.33, 90, 0.9], [P.can, 0.75, 0, 0.8]],
    [[P.can, 0.15, 0, 0.8], [P.can, 0.4, 40, 0.8], [P.toolbox, 0.72, 90, 0.85]],
    [[P.box1, 0.33, 90, 0.9], [P.can, 0.72, 0, 0.8]],
    [[P.toolbox, 0.3, 90, 0.9], [P.can, 0.65, 20, 0.8], [P.can, 0.8, 70, 0.8]],
    [[P.box2, 0.33, 90, 0.9], [P.box5, 0.76, 100, 0.55]],
  ];
  return set[b];
};
shelf({ ox: 2.297, oz: -2.955, yaw: 0, x0: -1.38, width: 1.32 }, wide); // behind the clerk, wide unit
shelf({ ox: 3.296, oz: -3.003, yaw: 0, x0: -0.96, width: 0.91 }, narrow); // behind the clerk, narrow unit
shelf({ ox: 5.745, oz: 6.057, yaw: -90, x0: -1.38, width: 1.32 }, wide); // on the x 5.75 wall by the window

// ---- Outside: the pavement in front of the shop (the front wall is at z 7.75) ----
put(P.wallLampOn, -0.9, 3.4, 7.73, 0); // a lit lamp between the window and the arch
put(P.wallLampOn, 4.9, 3.4, 7.73, 0); // and one right of the window
put(P.bench3, 1.7, 0, 8.3, 180);
put(P.crate, 0.3, 0, 8.35, 8);
put(P.crate, 0.3, 0.223, 8.35, -12);
put(P.binStreet, -0.45, 0.461, 8.2, 180);
put(P.pot, 4.95, 0, 8.55);

// ---------------------------------------------------------------------------------------------------
const hex16 = (s) => ID_PREFIX + createHash("sha1").update(s).digest("hex").slice(0, 12).toUpperCase();
const num = (n) => String(+(+n).toFixed(3));

async function build(fixed) {
  const snap = await snapItems(items, { fixed });
  console.log(`decor: ${snap.summary}${snap.unmeasured.length ? ` (not measurable, left as typed: ${snap.unmeasured.join(", ")})` : ""}`);
  if (process.argv.includes("--report")) for (const c of snap.changes) console.log(`  ${c.path.split("/").pop().replace(".et", "")} y ${c.from} -> ${c.to} (on ${c.on})`);
  const vfs = await gameVfs();
  const db = gameDb();
  if (!db) throw new Error("the game's resource database is not readable here (ENFUSION_GAME_PATH)");
  const memo = new Map();
  const info = (path) => {
    if (memo.has(path)) return memo.get(path);
    const guid = db.guidOf(path);
    if (!guid) throw new Error(`no GUID for ${path} in the game's resource database`);
    const text = readText(ROOT, vfs, path);
    const cls = (text && /^(\w+)/.exec(text.trimStart()))?.[1];
    if (!cls) throw new Error(`cannot read the root class of ${path}`);
    const v = { ref: `{${guid}}${path}`, cls };
    memo.set(path, v);
    return v;
  };
  const litGuid = db.guidOf(LAMP_COVER_LIT);
  if (!litGuid) throw new Error(`no GUID for ${LAMP_COVER_LIT} in the game's resource database`);
  const litRef = `{${litGuid}}${LAMP_COVER_LIT}`;
  const rows = [];
  items.forEach((it, i) => {
    const { ref, cls } = info(it.path);
    const lines = [`  ${cls} : "${ref}" {`, `   ID "${hex16(`gun-store|decor|${i}|${it.path}`)}"`];
    if (it.lit) lines.push(...litCover(litRef, hex16(`gun-store|decor|lit|${i}`)));
    if (it.scale !== 1) lines.push(`   scale ${num(it.scale)}`);
    lines.push(`   coords ${num(it.x)} ${num(it.y)} ${num(it.z)}`);
    if (it.yaw) lines.push(`   angleY ${num(it.yaw)}`);
    lines.push("  }");
    rows.push(lines.join("\n"));
  });
  lights.forEach((l, i) => {
    const r = ["  LightEntity {", `   ID "${hex16(`gun-store|decor|light|${i}`)}"`, `   coords ${num(l.x)} ${num(l.y)} ${num(l.z)}`];
    if (l.type === "Spot") r.push(`   angles ${l.angles}`);
    r.push(`   Color ${l.color}`, `   LV ${l.lv}`, "   ClipEV -10", `   Type ${l.type}`, `   Radius ${l.radius}`, "   NearPlane 0.3");
    if (l.type === "Spot") r.push(`   SpotAngle ${l.angle}`);
    r.push("   LensFlareType Disabled", "  }");
    rows.push(r.join("\n"));
  });
  return { block: rows.join("\n"), litRef };
}

// The lamp meshes' "cover" slot, assigned the base game's lit diffuser (the component id is the lamp prefabs' MeshObject).
function litCover(litRef, id) {
  return [
    "   components {",
    `    MeshObject "{5872F0EB7FA40561}" {`,
    "     Materials {",
    `      MaterialAssignClass "{${id.slice(0, 16)}}" {`,
    `       SourceMaterial "LampFluorescent_01_cover"`,
    `       AssignedMaterial "${litRef}"`,
    "      }",
    "     }",
    "    }",
    "   }",
  ];
}

// The two lamps the building pipeline put in the furniture prefab get the same lit cover (once: a lamp that has a
// components block already is left as it is).
export function glowVanillaLamps(text, litRef) {
  const lines = text.split("\n");
  const out = [];
  for (let i = 0; i < lines.length; i++) {
    out.push(lines[i]);
    const id = /^ {3}ID "([0-9A-F]{16})"$/.exec(lines[i])?.[1];
    if (id && VANILLA_LAMPS.includes(id) && lines[i + 1] !== "   components {") out.push(...litCover(litRef, hex16(`gun-store|vanilla-lamp|${id}`)));
  }
  return out.join("\n");
}

// Remove every child entity whose ID starts with the decor prefix, then append the new block.
export function rewrite(text, block) {
  const lines = text.split("\n");
  const out = [];
  for (let i = 0; i < lines.length; i++) {
    const open = /^ {2}\w+(?: : "[^"]+")? \{$/.exec(lines[i]);
    if (open && lines[i + 1]?.startsWith(`   ID "${ID_PREFIX}`)) {
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
const fixed = [...prefabChildren(rewrite(text, "")), ...prefabChildren(readFileSync(STORE, "utf8"))];
const built = await build(fixed);
const next = glowVanillaLamps(rewrite(text, built.block), built.litRef);
if (process.argv.includes("--check")) {
  const same = next === text;
  console.log(same ? "decor: the furniture prefab is current" : "decor: the furniture prefab differs; run node tools/buildings/gun-store/decor.mjs");
  process.exit(same ? 0 : 1);
}
writeFileSync(FURNITURE, next);
console.log(`decor: ${items.length} prop(s), ${lights.length} light(s) written to ${FURNITURE.replace(ROOT, "").replace(/\\/g, "/")}`);
