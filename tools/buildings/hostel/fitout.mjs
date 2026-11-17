// The 1989 hostel fit-out, pass 1 BUILD: the systems fitted into the hostel shell (House_Town_E_2I02, Hostel.et) in its
// pack grid claim. Writes the HostelKit_* block of Grid_hostel.layer and gives the same bytes twice.
//
//   node tools/buildings/hostel/fitout.mjs            rewrite the HostelKit_* block
//   node tools/buildings/hostel/fitout.mjs --check    exit 1 when the layer differs
//   node tools/buildings/hostel/fitout.mjs --report   entity, light and shadow counts (exit 1 over the budget)
//
// Systems (existing only, M5 #1958/#1965): the first-night beds are `LF_FirstSpawnBed` (a brand-new character wakes on the
// first free one and steps off at its `m_vStandOffset`, local space). The reception desk, the dorm bunks, the luggage
// lockers and the washrooms are plain base-game furniture; the washrooms come with the shell. The attic (floor 2) stays shut.
//
// Every row is in BUILDING space: the hostel's own origin, front toward +z, ground floor y 0.26, first floor y 4.06
// (the furniture set's floors). The building's placement (coords and yaw) is read from Grid_hostel_Main, so the kit holds
// no world coordinates; `town fit` maps the same rows onto the real building later.
//
// Budget MEDIUM (spec.json): at most 300 entities, 6 dynamic lights, 1 shadow caster. Every light sets CastShadow; none
// casts in pass 1 (one short-range shadowless spot per room). Decor is static base-game props with LODs, no scripts.
import { readFileSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { gameVfs, readText } from "../../lib/game-prefab.mjs";
import { gameDb } from "../pipeline/gamedb.mjs";
import { prefabChildren, snapItems, specSupports } from "../pipeline/surface.mjs";

const ROOT = join(dirname(fileURLToPath(import.meta.url)), "..", "..", "..");
const SPEC = JSON.parse(readFileSync(join(ROOT, "tools", "buildings", "hostel", "spec.json"), "utf8"));
const GRID_LAYER = join(ROOT, SPEC.grid.layer);
const FURNITURE_FILE = join(ROOT, "addons", SPEC.addon, SPEC.prefab_dir, "Furniture", "Hostel_furniture_01.et");
const PREFIX = "HostelKit_";
const HOST = "Grid_hostel_Main";
const BUDGET = SPEC.budget;
const F0 = 0.26;
const F1 = 4.06;
const args = process.argv.slice(2);

const P = {
  bunk: "Prefabs/Props/Furniture/BunkBed_02/BunkBed_02.et",
  locker: "Prefabs/Props/Furniture/CabinetMetal_01_grey_V1.et",
  suitcase: "Prefabs/Props/Civilian/Suitcase_01.et",
  counter: "Prefabs/Props/Furniture/Desk_01_long.et",
  phone: "Prefabs/Props/Furniture/PhoneTable_01_grey.et",
  register: "Prefabs/Props/Civilian/Notebook_01_Open.et",
  ashtray: "Prefabs/Props/Civilian/Ashtray_01_Full_yellow.et",
  chair: "Prefabs/Props/Furniture/Chair_02/Chair_02_green.et",
  notice: "Prefabs/Props/Commercial/Posters/Notice_A4_Vertical_V3.et",
};
const FIRST_BED = "{15468371DAC524FC}Prefabs/Medic/LF_FirstSpawnBed.et";
const FIRST_BED_COMPONENT = "{FCB27F8C7C2E211B}";

// ---- systems: the first-night beds (stand offset in the bed's local space, clear of the bunks and the sofa) ----
const beds = [
  { name: "FirstBed_1", x: -2.4, y: F1, z: 2.6, yaw: 0, stand: "-0.85 0 0" }, // dorm-west, beside the bunks
  { name: "FirstBed_2", x: 1.45, y: F1, z: 2.9, yaw: 0, stand: "0.9 0 -1" }, // dorm-east, under the window side
];

// ---- decor (heights are hints, snapItems stands each on the top under it) ----
const props = [];
const put = (path, x, y, z, yaw = 0, o = {}) => props.push({ path, x, y, z, yaw, scale: 1, ...o });
// dorm-west: two bunks beside the first-night bed
put(P.bunk, -6.2, F1, 2.3, 0, { id: "bunkW1" });
put(P.bunk, -5.0, F1, 2.3, 0, { id: "bunkW2" });
put(P.suitcase, -5.6, F1, 4.1, 90);
// reception (ground floor front): a counter facing the front door with the guest register and the phone
put(P.counter, 3.2, F0, 1.6, 0, { id: "counter" });
put(P.register, 2.3, F0 + 0.95, 1.95, 180, { on: "counter" });
put(P.phone, 1.7, F0 + 0.95, 1.95, 160, { on: "counter" });
put(P.ashtray, 2.8, F0 + 0.95, 1.95, 0, { on: "counter" });
// luggage hall: a row of lockers on the back wall, two cases waiting
for (const [i, x] of [-7.5, -6.7, -5.9, -5.1].entries()) put(P.locker, x, F0, -5.3, 0, { id: `locker${i + 1}` });
put(P.suitcase, -3.6, F0, -4.9, 10);
put(P.suitcase, -7.2, F0, 3.0, 80);

// ---- lights: one per room, shadowless, short range ----
const lights = [];
const spot = (x, y, z, o = {}) => lights.push({ type: "Spot", x, y, z, angles: "-90 0 0", color: "1 0.86 0.64 1", lv: 5, radius: 5.5, angle: 130, shadow: false, ...o });
spot(4.6, 3.4, 2.8); // reception
spot(6.4, 3.4, -3.4, { radius: 4.5 }); // kitchen
spot(1.8, 3.2, -3.8, { color: "0.95 0.92 0.85 1", radius: 4 }); // washroom
spot(-4.6, 3.4, 0.0, { color: "0.95 0.9 0.8 1", lv: 4.5, radius: 6 }); // luggage hall
spot(-4.6, F1 + 2.8, 2.0); // dorm-west
spot(3.0, F1 + 2.8, 2.8); // dorm-east

// ---- building space to world space ----
const num = (n) => String(+(+n).toFixed(3));
function hostPlacement(text) {
  const m = new RegExp(`^\\w+ ${HOST} : "[^"]+" \\{\\n((?: [^\\n]*\\n)*?)\\}`, "m").exec(text);
  if (!m) throw new Error(`no entity ${HOST} in ${SPEC.grid.layer}`);
  const c = /^ coords (\S+) (\S+) (\S+)$/m.exec(m[1]);
  const a = /^ angleY (\S+)$/m.exec(m[1]);
  return { x: +c[1], y: +c[2], z: +c[3], yaw: a ? +a[1] : 0 };
}
function toWorld(h, x, y, z) {
  const r = (h.yaw * Math.PI) / 180;
  const s = Math.sin(r);
  const c = Math.cos(r);
  return [num(h.x + x * c + z * s), num(h.y + y), num(h.z - x * s + z * c)];
}
const yawOf = (h, yaw) => { let a = (h.yaw + yaw) % 360; if (a > 180) a -= 360; if (a <= -180) a += 360; return a; };
const place = (h, x, y, z, yaw) => {
  const [wx, wy, wz] = toWorld(h, x, y, z);
  const a = yawOf(h, yaw);
  return [` coords ${wx} ${wy} ${wz}`, ...(a ? [` angleY ${num(a)}`] : [])];
};

function stripKit(text) {
  const lines = text.split("\n");
  const out = [];
  for (let i = 0; i < lines.length; i++) {
    if (new RegExp(`^\\w+ ${PREFIX}`).test(lines[i])) {
      while (i < lines.length && lines[i] !== "}") i++;
      continue;
    }
    out.push(lines[i]);
  }
  return out.join("\n").replace(/\n+$/, "\n");
}

async function build(h) {
  const vfs = await gameVfs();
  const db = gameDb();
  if (!db) throw new Error("the game's resource database is not readable here (ENFUSION_GAME_PATH)");
  const memo = new Map();
  const info = (path) => {
    if (memo.has(path)) return memo.get(path);
    const guid = db.guidOf(path);
    if (!guid) throw new Error(`no GUID for ${path} in the game's resource database`);
    const cls = /^(\w+)/.exec((readText(ROOT, vfs, path) || "").trimStart())?.[1];
    if (!cls) throw new Error(`cannot read the root class of ${path}`);
    const v = { ref: `{${guid}}${path}`, cls };
    memo.set(path, v);
    return v;
  };
  const furn = prefabChildren(readFileSync(FURNITURE_FILE, "utf8"));
  const items = props.map((p) => ({ ...p }));
  const snap = await snapItems(items, { fixed: [...furn, ...specSupports(SPEC)] });
  if (!args.includes("--check") && !args.includes("--report")) console.log(snap.summary);

  const blocks = [];
  for (const b of beds) {
    blocks.push([`GenericEntity ${PREFIX}${b.name} : "${FIRST_BED}" {`, " components {", `  LF_FirstSpawnBedComponent "${FIRST_BED_COMPONENT}" {`,
      `   m_vStandOffset ${b.stand}`, "  }", " }", ...place(h, b.x, b.y, b.z, b.yaw), "}"].join("\n"));
  }
  items.forEach((p, i) => {
    const { ref, cls } = info(p.path);
    blocks.push([`${cls} ${PREFIX}Decor_${i + 1} : "${ref}" {`, ...place(h, p.x, p.y, p.z, p.yaw), "}"].join("\n"));
  });
  lights.forEach((l, i) => {
    const [wx, wy, wz] = toWorld(h, l.x, l.y, l.z);
    blocks.push([`LightEntity ${PREFIX}Light_${i + 1} {`, ` coords ${wx} ${wy} ${wz}`, ` angles ${l.angles}`, ` Color ${l.color}`, ` LV ${l.lv}`, " ClipEV -10",
      ` Type ${l.type}`, ` Radius ${l.radius}`, " NearPlane 0.3", ` SpotAngle ${l.angle}`, ` CastShadow ${l.shadow ? 1 : 0}`, " LensFlareType Disabled", "}"].join("\n"));
  });
  return blocks;
}

const base = readFileSync(GRID_LAYER, "utf8");
const host = hostPlacement(base);
const kept = prefabChildren(readFileSync(FURNITURE_FILE, "utf8")).length;
const c = { furniture: kept, decor: props.length, systems: beds.length, lights: lights.length, shadows: lights.filter((l) => l.shadow).length };
const total = c.furniture + c.decor + c.systems + c.lights;
const over = c.lights > BUDGET.lights || c.shadows > BUDGET.shadow_casters || total > BUDGET.entities;
if (args.includes("--report")) {
  console.log(`hostel pass 1: ${total} entities of ${BUDGET.entities} (${c.furniture} shell furniture kept, ${c.decor} decor, ${c.systems} first-night beds, ${c.lights} lights), ${c.lights} dynamic lights of ${BUDGET.lights} (${c.shadows} cast shadows of ${BUDGET.shadow_casters})`);
  process.exit(over ? 1 : 0);
}
const next = `${stripKit(base)}${(await build(host)).join("\n")}\n`;
if (args.includes("--check")) {
  const same = next === base;
  console.log(same ? "hostel: the grid layer is current" : "hostel: the grid layer differs; run node tools/buildings/hostel/fitout.mjs");
  process.exit(same ? 0 : 1);
}
if (over) throw new Error(`over the budget: ${JSON.stringify(c)} (max ${JSON.stringify(BUDGET)})`);
writeFileSync(GRID_LAYER, next);
console.log(`hostel: pass 1: ${c.decor} decor, ${c.systems} first-night beds, ${c.lights} lights (${c.shadows} shadow) written to ${SPEC.grid.layer}`);
