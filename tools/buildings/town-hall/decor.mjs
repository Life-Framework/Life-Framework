// The town hall's fit-out: lights, the lit facade lamp, wall dressing, stocked counters and the clutter of an office
// that is used. Only base-game props, all static (no physics, no scripts, nothing pickable).
//
//   node tools/buildings/town-hall/shell.mjs [--pass 1|2|3] [--check]    writes the building, the pruned vanilla set and this block
//   node tools/buildings/town-hall/decor.mjs [--pass 1|2|3]              prints the budget this block spends
//
// Passes (the PR shows each): BUILD is the pruned vanilla set alone (pass 0); POLISH 1 (pass 1) lights every room and
// lamps the facade, dresses the walls, stocks the counter and signs the rooms; POLISH 2 (pass 2) adds the lived-in
// clutter and the night mood; the A pass (pass 3) fits and lights the first floor. A row's `pass` says which pass adds it;
// `--pass N` writes every row up to N.
//
// Budget (docs/architecture/performance.md, a LARGE building): at most 500 entities, 10 lights, 2 shadow casters and
// no point light that casts one; static props only, no physics, replication or per-frame script on decoration. This
// building spends 9 lights, none casting a shadow: two corridor lights, the counter spot and the waiting-bench light
// (short range), the facade light, and on the first floor the landing, the hall's west end and the two offices (pass 3). Every LightEntity row sets CastShadow explicitly (unset
// means on). Pass 1 stood a vanilla lit lamp on the facade; its light leaves CastShadow unset and the prop carries
// replication, so pass 2 swaps it for a plain static lamp mesh and a light of our own. The ceiling lamp meshes glow
// through an emissive cover material, which costs no light.
//
// Coordinates are building space: the front is +z, the entrance x 0, z 7.7; floor y 0.2 (ground) and 4.0 (first
// floor). A wall item's yaw faces the room: 0 on a wall that faces +z, 180 on one that faces -z, 90 / -90 on walls
// that face +x / -x. Row IDs start with DC1F so a re-run replaces the block.
import { createHash } from "node:crypto";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { gameVfs, readText } from "../../lib/game-prefab.mjs";
import { gameDb } from "../pipeline/gamedb.mjs";
import { snapItems } from "../pipeline/surface.mjs";

const ROOT = join(dirname(fileURLToPath(import.meta.url)), "..", "..", "..");
export const ID_PREFIX = "DC1F";
const F = 0.2; // the ground floor
const F1 = 4.0; // the first floor

const P = {
  porchLamp: "Prefabs/Props/Furniture/LightWall_01.et",
  wallLampOn: "Prefabs/Structures/BuildingParts/Lights/LampIndustrial_01_on_exterior.et",
  clock: "Prefabs/Props/Furniture/ClockWall_01.et",
  noticeH3: "Prefabs/Props/Commercial/Posters/Notice_A4_Horizontal_V3.et",
  noticeH1: "Prefabs/Props/Commercial/Posters/Notice_A4_Horizontal_V1.et",
  noticeV3: "Prefabs/Props/Commercial/Posters/Notice_A4_Vertical_V3.et",
  insurance: "Prefabs/Props/Commercial/Posters/PosterSingle_A3_Insurance_V1.et",
  radioPoster: "Prefabs/Props/Commercial/Posters/PosterSingle_27x41_01_RadioEveron.et",
  paintingBig4: "Prefabs/Props/Decorations/Paintings/PaintingBig_01_V4.et",
  paintingBig9: "Prefabs/Props/Decorations/Paintings/PaintingBig_01_V9.et",
  paintingSmall3: "Prefabs/Props/Decorations/Paintings/PaintingSmall_01_V3.et",
  fileOpen: "Prefabs/Props/Civilian/FileOffice_01_Open.et",
  fileStack: "Prefabs/Props/Civilian/FileOffice_01_Stack.et",
  notebook: "Prefabs/Props/Civilian/Notebook_01_Open.et",
  pen: "Prefabs/Props/Civilian/Pen_01/Pen_01_base.et",
  phone: "Prefabs/Props/Furniture/PhoneTable_01_red.et",
  radio: "Prefabs/Props/Civilian/RadioSmall_01_red.et",
  frame: "Prefabs/Props/Civilian/TablePhotoFrame_01/TablePhotoFrame_01_V3.et",
  plant: "Prefabs/Props/Furniture/PlantRoom_02.et",
  coatRack: "Prefabs/Props/Furniture/CoatRack_04.et",
  bin: "Prefabs/Props/Garbage/Bins/BinInterior_01_green.et",
  floorLamp: "Prefabs/Props/Furniture/LampInterior_02/LampInterior_02_Orange.et",
  deskLamp: "Prefabs/Props/Furniture/LightTable_01.et",
  carpet: "Prefabs/Props/Furniture/Carpet_02.et",
  stool: "Prefabs/Props/Furniture/Stool_02.et",
  pot: "Prefabs/Props/Decorations/Flowerpots/FlowerpotStreet_01/FlowerpotStreet_01_rect_long_begonia_red.et",
  bench: "Prefabs/Props/Furniture/BenchOffice_01/BenchOffice_01_Base.et",
  box: "Prefabs/Props/Crates/CardboardBox/CardboardBox_06.et",
  tube: "Prefabs/Props/Furniture/LampFluorescent/LampFluorescent_02.et",
  typewriter: "Prefabs/Props/Civilian/Typewriter_01/Typewriter_01_Base.et",
};
// The emissive tube cover (the same material prune.mjs puts on the ground-floor tubes); a `lit` row gets it.
const LIT = ["LampFluorescent_02_cover", "{51611E400CB6FB8A}Assets/Props/Furniture/LampFluorescent_02/Data/LampFluorescent_02_cover_emissive.emat"];

const items = []; // { pass, path, x, y, z, yaw, scale, lit }
const lights = []; // { pass, x, y, z, angles, color, lv, radius, angle, type, shadow }
// y is a hint: decorBlock snaps every prop onto the top under it (tools/buildings/pipeline/surface.mjs); `o.on` names a support.
const put = (pass, path, x, y, z, yaw = 0, scale = 1, until = 9, o = {}) => items.push({ pass, path, x, y, z, yaw, scale, until, ...o });
const spot = (pass, x, y, z, o = {}) => lights.push({ pass, type: "Spot", x, y, z, angles: "-90 0 0", color: "1 0.86 0.64 1", lv: 5.5, radius: 6.5, angle: 130, shadow: false, ...o });
const glow = (pass, x, y, z, o = {}) => lights.push({ pass, type: "Point", x, y, z, color: "1 0.88 0.68 1", lv: 2.4, radius: 5, shadow: false, ...o });

// ---- POLISH 1: lights. Ceiling at 3.9 on the ground floor, 7.7 on the first. ----
glow(1, 0.2, 3.4, 5.8, { lv: 3.4, radius: 6.5, until: 1 }); // the entrance corridor by the arrival marker and the board
glow(2, -0.5, 3.4, 5.6, { lv: 4, radius: 7 }); // pass 2: moved to the board wall, which the night pictures showed dark
glow(1, 0.4, 3.4, 1.4, { lv: 3, radius: 6.5, until: 2 }); // the middle of the corridor
glow(3, 0.2, 3.4, 3.0, { lv: 3.2, radius: 6.5 }); // the A pass: moved up the corridor, which the night pictures showed dark between the board and the glass doors
spot(1, 8.0, 3.5, 2.7, { lv: 5, radius: 5, angle: 120 }); // the clerk's side of the counter
glow(1, 7.4, 3.4, 6.0, { lv: 2.2, radius: 4.5 }); // the waiting benches
glow(1, 0, 7.3, 1.0, { lv: 2.2, radius: 5.5 }); // the first-floor landing
put(1, P.wallLampOn, 2.6, 3.0, 7.73, 0, 1, 1); // pass 1: the vanilla lit lamp over the porch; its light leaves CastShadow unset and the prop carries replication, so pass 2 replaces it
put(2, P.porchLamp, 2.6, 3.0, 7.76, 0); // the lamp mesh (a plain static prop), lit by the light below
glow(2, 2.6, 3.0, 8.5, { lv: 3.6, radius: 7.5 }); // the facade light: a point light, CastShadow 0

// ---- POLISH 1: the counter and the clerk's side (z 1.5 is the wall behind the clerk) ----
put(1, P.clock, 8.4, 2.7, 1.52, 0);
put(1, P.noticeH3, 6.3, 1.75, 1.52, 0, 0.9);
put(1, P.insurance, 10.1, 1.9, 1.52, 0, 0.9);
put(1, P.fileOpen, 6.3, 1.106, 3.9, 180); // the register open on the counter
put(1, P.notebook, 7.9, 1.106, 3.85, 180);
put(1, P.pen, 8.15, 1.11, 3.9, 20);
put(1, P.phone, 5.55, 1.106, 3.7, 200);

// ---- POLISH 1: the waiting side (benches at z 6.8; the east wall at x 10.85) ----
put(1, P.paintingBig9, 10.8, 2.1, 5.7, -90);
put(1, P.noticeV3, 10.8, 1.5, 4.7, -90);
put(1, P.plant, 10.1, F, 7.1);
put(1, P.radioPoster, 5.1, 1.7, 7.62, 180, 0.9);

// ---- POLISH 1: the entrance corridor ----
put(1, P.plant, -1.4, F, 6.9);
put(1, P.paintingBig4, 2.6, 2.1, 7.2, -90);

// ---- POLISH 2: a used office (stories on the surfaces) ----
put(2, P.coatRack, -1.3, F, 5.7, 90);
put(2, P.bin, 4.4, F, 7.0);
put(2, P.carpet, 0.2, F, 4.6, 0, 0.9);
put(2, P.stool, 5.6, F, 3.2, 40);
put(2, P.frame, 9.4, 1.106, 3.95, 200); // the clerk desks behind the counter are cut (prune.mjs), so the props stand on the counter
put(2, P.radio, 9.95, 1.106, 3.95, 190);
put(2, P.fileStack, 5.0, 1.106, 3.7, 15);
put(2, P.deskLamp, 6.9, 1.106, 3.95, 160);
put(2, P.box, 10.2, F, 2.4, 20, 0.6);
put(2, P.box, 10.2, F + 0.21, 2.4, 70, 0.6);
put(2, P.noticeH1, 10.8, 1.5, 6.3, -90, 0.9);
put(2, P.paintingSmall3, 3.0, 1.9, 7.2, -90, 0.9);
put(2, P.floorLamp, 4.6, F, 6.0);
put(2, P.pot, -2.4, 0, 8.8);
put(2, P.pot, 2.4, 0, 8.8);
put(2, P.bench, -3.0, 0, 9.0, 0);

// ---- A PASS, part 1 (pass 3, #2309): the first floor fitted and lit. prune.mjs drops the vanilla first-floor
// ceiling tubes (LampFluorescent_02), so the floor read as one dark room under the landing light. Each office gets a
// ceiling tube with the emissive cover (`lit`) and one short fill light; the desks (Desk_01 pivots at a corner, top
// 4.903) get the paper of a working registry. Desk centres: the east block (6.38, 4.43), (4.67, 5.17), (6.37, 5.25),
// (3.0, 4.31); the west desk (-6.09, 5.8). The front wall's inner face is z 7.3 (the vanilla clock), yaw 180.
const TUBE = 7.297; // the vanilla tube height on the first floor (the lamp's pivot is its top)
const T = 4.903; // the measured Desk_01 top (3.997 + 0.906); the snap keeps each prop on it
put(3, P.tube, -7.6, TUBE, 4.6, 90, 1, 9, { lit: true }); // the west office
put(3, P.tube, 4.6, TUBE, 4.8, 0, 1, 9, { lit: true }); // the east office, over the desk block
put(3, P.tube, 7.4, TUBE, 4.8, 0, 1, 9, { lit: true });
put(3, P.tube, -4.5, TUBE, -1.0, 0, 1, 9, { lit: true }); // the hall (z -3 to 1.1, windows on the courtyard), dark past the landing at night
put(3, P.tube, 4.5, TUBE, -1.0, 0, 1, 9, { lit: true });
glow(3, -7.4, 7.0, 4.8, { lv: 2.4, radius: 5 }); // the west office
glow(3, 5.6, 7.0, 4.8, { lv: 2.6, radius: 6 }); // the east office
glow(3, -4.5, 7.0, -1.0, { lv: 2.2, radius: 6 }); // the west end of the hall; the landing light covers the east

// The east office desk block: a registry that is half way through the morning.
put(3, P.typewriter, 6.3, T, 4.45, 180);
put(3, P.fileStack, 6.95, T, 4.3, 170);
put(3, P.fileOpen, 4.6, T, 5.2, 5);
put(3, P.pen, 4.95, T, 5.05, 60);
put(3, P.phone, 3.95, T, 5.3, 20);
put(3, P.deskLamp, 6.9, T, 5.4, -10);
put(3, P.notebook, 6.1, T, 5.25, -15);
put(3, P.frame, 2.5, T, 4.2, 160);
put(3, P.fileStack, 3.4, T, 4.35, 190);
// The west desk: the licence officer's paperwork.
put(3, P.deskLamp, -6.05, T, 6.4, 90);
put(3, P.fileOpen, -6.1, T, 5.6, 95);
put(3, P.radio, -6.15, T, 5.3, 80);
// Floors and the front wall.
put(3, P.plant, 9.9, F1, 6.8);
put(3, P.bin, 3.3, F1, 6.6);
put(3, P.paintingBig9, 8.6, 6.0, 7.3, 180, 0.9);
put(3, P.noticeH3, -8.6, 5.6, 7.3, 180, 0.9);

// ---------------------------------------------------------------------------------------------------
const hex16 = (s) => ID_PREFIX + createHash("sha1").update(s).digest("hex").slice(0, 12).toUpperCase();
const num = (n) => String(+(+n).toFixed(3));

export function budget(pass) {
  const li = lights.filter((l) => l.pass <= pass && (l.until ?? 9) >= pass);
  const it = items.filter((i) => i.pass <= pass && (i.until ?? 9) >= pass);
  const lampProps = it.filter((i) => /LampIndustrial_01_on_exterior/.test(i.path)).length; // a vanilla lamp carries a light of its own
  return { props: it.length, lightRows: li.length, lights: li.length + lampProps, shadows: li.filter((l) => l.shadow).length };
}

// The decor block (child entities of the furniture prefab) for every row up to `pass`.
export async function decorBlock(pass, fixed = []) {
  const snap = await snapItems(items.filter((i) => i.pass <= pass && (i.until ?? 9) >= pass), { fixed });
  console.log(`decor: ${snap.summary}`);
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
  const rows = [];
  items.filter((i) => i.pass <= pass && (i.until ?? 9) >= pass).forEach((it, i) => {
    const { ref, cls } = info(it.path);
    const lines = [`  ${cls} : "${ref}" {`, `   ID "${hex16(`town-hall|decor|${i}|${it.path}`)}"`];
    if (it.scale !== 1) lines.push(`   scale ${num(it.scale)}`);
    lines.push(`   coords ${num(it.x)} ${num(it.y)} ${num(it.z)}`);
    if (it.yaw) lines.push(`   angleY ${num(it.yaw)}`);
    if (it.lit) {
      const id = hex16(`town-hall|decor|lit|${i}`);
      lines.push("   components {", '    MeshObject "{5872F0EB7FA40561}" {', "     Materials {", `      MaterialAssignClass "{${id}}" {`, `       SourceMaterial "${LIT[0]}"`, `       AssignedMaterial "${LIT[1]}"`, "      }", "     }", "    }", "   }");
    }
    lines.push("  }");
    rows.push(lines.join("\n"));
  });
  lights.filter((l) => l.pass <= pass && (l.until ?? 9) >= pass).forEach((l, i) => {
    const r = ["  LightEntity {", `   ID "${hex16(`town-hall|decor|light|${i}`)}"`, `   coords ${num(l.x)} ${num(l.y)} ${num(l.z)}`];
    if (l.type === "Spot") r.push(`   angles ${l.angles}`);
    r.push(`   Color ${l.color}`, `   LV ${l.lv}`, "   ClipEV -10", `   Type ${l.type}`, `   Radius ${l.radius}`, "   NearPlane 0.3");
    if (l.type === "Spot") r.push(`   SpotAngle ${l.angle}`);
    r.push(`   CastShadow ${l.shadow ? 1 : 0}`, "   LensFlareType Disabled", "  }");
    rows.push(r.join("\n"));
  });
  return rows.join("\n");
}

if (process.argv[1] && process.argv[1].endsWith("decor.mjs")) {
  const pass = process.argv.includes("--pass") ? Number(process.argv[process.argv.indexOf("--pass") + 1]) : 3;
  const b = budget(pass);
  console.log(`decor pass ${pass}: ${b.props} prop(s) + ${b.lightRows} light row(s), ${b.lights} dynamic light(s) (${b.shadows} shadow caster(s))`);
}
