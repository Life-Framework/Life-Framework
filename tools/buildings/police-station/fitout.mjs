// The 1989 police station fit-out: the vanilla Everon police villa (Villa_E_2I01_Police) in its grid claim, with
// the police systems standing where a station keeps them and the dressing that makes it read as a working post.
//
//   node tools/buildings/police-station/fitout.mjs [--pass 1|2|3] [--with-cell]   rewrite the PoliceKit_* block
//   node tools/buildings/police-station/fitout.mjs --check [--pass N]             exit 1 when the layer differs
//   node tools/buildings/police-station/fitout.mjs --report                       entity, light and shadow counts
//
// Passes are cumulative: 1 BUILD (systems, yard, boards, cell), 2 POLISH 1 (lights, walls, desks, interview room),
// 3 POLISH 2 (night mood, facade, yard dressing, finishing touches). The default is the last pass.
//
// Every point is in building space: the villa's own origin, the front (porch, front door) toward +z, floor y 0, first
// floor y 3.8. The kit holds no world coordinates: ORIGIN below is only where this claim stands (Grid_police_station_kit
// c3r1 2x2), and `town fit` later maps the same rows onto a real building.
//
// Performance budget (MEDIUM building, maintainer 2026-11-16): at most 6 dynamic lights per building and only 1 of them casts
// shadows (a point light never does; CastShadow is set on every light), at most 300 decorative entities, vanilla props with LODs, static decor, no per-frame scripts. `--report`
// prints the counts and exits 1 over the budget. Decor here is a plain base-game prop with no script.
import { readFileSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { gameVfs, readText } from "../../lib/game-prefab.mjs";
import { gameDb } from "../pipeline/gamedb.mjs";

const ROOT = join(dirname(fileURLToPath(import.meta.url)), "..", "..", "..");
const LAYERS = join(ROOT, "addons", "LifeFramework", "Worlds");
const GRID_LAYER = join(LAYERS, "GridWorld", "GridWorld_Layers", "Grid_police_station_kit.layer");
const POLICE_LAYER = join(LAYERS, "DebugWorld", "DebugWorld_Layers", "Street_Police.layer");
const JAIL_LAYER = join(LAYERS, "DebugWorld", "DebugWorld_Layers", "Street_Jail.layer");
const LICENCE_LAYER = join(LAYERS, "GridWorld", "GridWorld_Layers", "Grid_licence_desk.layer");

import { ORIGIN } from "./origin.mjs";
const PREFIX = "PoliceKit_";
const VILLA = `${PREFIX}Villa`;
const F1 = 3.8; // first floor
const BUDGET = { lights: 6, shadows: 1, decor: 300 }; // MEDIUM, docs/architecture/performance.md

const args = process.argv.slice(2);
const flag = (n, d) => { const i = args.indexOf(n); return i >= 0 ? args[i + 1] : d; };
const PASS = Number(flag("--pass", "3"));

// ---- prop paths (base game) ----
const P = {
  lamp: "Prefabs/Props/Furniture/LampFluorescent/LampFluorescent_02.et",
  deskLamp: "Prefabs/Props/Furniture/LightTable_01.et",
  clock: "Prefabs/Props/Civilian/ClockUniversal_01.et",
  phone: "Prefabs/Props/Furniture/PhoneTable_01_grey.et",
  phoneRed: "Prefabs/Props/Furniture/PhoneTable_01_red.et",
  typewriter: "Prefabs/Props/Civilian/Typewriter_01/Typewriter_01_grey.et",
  notebook: "Prefabs/Props/Civilian/Notebook_01_Open.et",
  ashtray: "Prefabs/Props/Civilian/Ashtray_01_Full_yellow.et",
  tape: "Prefabs/Props/Civilian/TapeRecorder_01/TapeRecorder_01.et",
  computer: "Prefabs/Props/Civilian/Computer_E_01/Computer_E_01_on.et",
  radio: "Prefabs/Props/Civilian/RadioSmall_01_black.et",
  flipchart: "Prefabs/Props/Furniture/Flipchart_01/Flipchart_01.et",
  table: "Prefabs/Props/Furniture/Table_01.et",
  chairRed: "Prefabs/Props/Furniture/Chair_02/Chair_02_red.et",
  chairGreen: "Prefabs/Props/Furniture/Chair_02/Chair_02_green.et",
  coatRack: "Prefabs/Props/Furniture/CoatRack_05/CoatRack_05.et",
  binIn: "Prefabs/Props/Garbage/Bins/BinInterior_01_green.et",
  noticeA: "Prefabs/Props/Commercial/Posters/Notice_A4_Vertical_V3.et",
  noticeB: "Prefabs/Props/Commercial/Posters/Notice_A4_Horizontal_V4.et",
  noticeC: "Prefabs/Props/Commercial/Posters/Notice_A4_Vertical_V6.et",
  noticeD: "Prefabs/Props/Commercial/Posters/Notice_A4_Horizontal_V2.et",
  corkboard: "Prefabs/Props/Civilian/NoticeBoardCork_01/Filled/NoticeBoardCork_USSR_01_filled_V1.et",
  corkboard2: "Prefabs/Props/Civilian/NoticeBoardCork_01/Filled/NoticeBoardCork_USSR_01_filled_V2.et",
  poster: "Prefabs/Props/Commercial/Posters/PosterSingle_27x41_01_RadioEveron.et",
  bench: "Prefabs/Props/Civilian/BenchStreet/BenchStreet_02.et",
  binStreet: "Prefabs/Props/Garbage/Bins/BinStreet_01.et",
  pot: "Prefabs/Props/Decorations/Flowerpots/FlowerpotStreet_01/FlowerpotStreet_01_octa_begonia_red.et",
  wallLamp: "Prefabs/Structures/BuildingParts/Lights/LightIndustrial_02_on_exterior.et",
};
// The base game's lit diffuser for the fluorescent lamp's cover (the lamp meshes ship dark): a lamp that looks on and
// costs no dynamic light.
const LAMP_COVER_LIT = "Assets/Props/Furniture/LampFluorescent_02/Data/LampFluorescent_02_cover_emissive.emat";

// Props whose base-game RigidBody can simulate (small clutter that falls off a desk): forced static in the layer.
const NEEDS_STATIC = ["Ashtray_01_Full_yellow.et", "Notebook_01_Open.et", "Notice_A4_Horizontal_V2.et", "Notice_A4_Horizontal_V4.et", "Notice_A4_Vertical_V3.et",
  "Notice_A4_Vertical_V6.et", "PhoneTable_01_grey.et", "PhoneTable_01_red.et", "PosterSingle_27x41_01_RadioEveron.et", "RadioSmall_01_black.et", "Typewriter_01_grey.et"];

// ---- LF systems (core prefabs; GUIDs from the playtest town's layers) ----
const G = {
  dutyDesk: "{034D1397A95787DB}Prefabs/Police/LF_PoliceDutyDesk.et",
  armoryStore: "{700A3E2CC5C68520}Prefabs/Police/LF_ArmoryStore.et",
  wantedBoard: "{3F233EFE851CEBBE}Prefabs/Police/LF_WantedBoard.et",
  dispatchBoard: "{5BC90DB066B123D4}Prefabs/Comms/LF_DispatchBoard_Police.et",
  evidenceLocker: "{5D8B2F46A9E1C073}Prefabs/Investigation/LF_EvidenceLocker.et",
  caseBoard: "{C8B78C628082CA0D}Prefabs/Investigation/LF_CaseBoard.et",
  licenceDesk: "{0970D6C0210F9612}Prefabs/Props/Commercial/LF_LicenceDesk.et",
  payphone: "{8F2C6E1D4B7A5093}Prefabs/Comms/LF_Payphone.et",
  patrolCar: "{EFEB161F9BC2031D}Prefabs/Vehicles/Wheeled/UAZ469/LF_UAZ469_Police.et",
  spikeStrip: "{8FEC063EE8B0100B}Prefabs/Props/Roleplay/SpikeStrip/SpikeStrip.et",
  handcuffs: "{DE0B6FEBDCECC6FF}Prefabs/Items/Roleplay/Handcuffs.et",
  taser: "{6D583A40C5C5C150}Prefabs/Weapons/Handguns/Taser/Handgun_Taser.et",
};

const systems = []; // { kind: "clone", src, name, to, x,y,z,yaw } | { kind: "prefab", name, ref, x,y,z,yaw, extra }
const props = []; // { path, x,y,z, yaw, scale, lit }
const lights = []; // { x,y,z, type, color, lv, radius, angle, angles, shadow }

const sys = (kind, o) => systems.push({ kind, ...o });
const put = (path, x, y, z, yaw = 0, scale = 1, lit = false) => props.push({ path, x, y, z, yaw, scale, lit });
const spot = (x, y, z, o = {}) => lights.push({ type: "Spot", x, y, z, angles: "-90 0 0", color: "1 0.86 0.64 1", lv: 5, radius: 6, angle: 130, shadow: false, ...o });
const glow = (x, y, z, o = {}) => lights.push({ type: "Point", x, y, z, color: "1 0.88 0.68 1", lv: 2, radius: 5, shadow: false, ...o });
const lamp = (x, y, z) => put(P.lamp, x, y, z, 0, 1, true);

// ============================================================================================================
// PASS 1 - BUILD: the systems in their rooms, the yard, the boards.
// ============================================================================================================
if (PASS >= 1) {
  // Hall (x -3..0.85, z -2..7.3; the front door at z 7.3, the stairs on the west side): the public front desk.
  sys("clone", { src: LICENCE_LAYER, name: "LicenceDesk_Grid", to: `${PREFIX}LicenceDesk`, x: 0.74, y: 0, z: 4.9, yaw: 90 });
  sys("prefab", { name: `${PREFIX}WantedBoard`, ref: G.wantedBoard, x: 0.72, y: 1.15, z: 1.9, yaw: 90 });
  // Office (ground, x 0.9..6.1, z -3.7..3.3): the duty desk with its clock-in, the armory, the evidence locker.
  sys("clone", { src: POLICE_LAYER, name: "Roadmap_Police_Desk", to: `${PREFIX}DutyDesk`, x: 3.8, y: 0, z: -0.4, yaw: 0 });
  sys("clone", { src: POLICE_LAYER, name: "Roadmap_Police_Armory", to: `${PREFIX}Armory`, x: 5.8, y: 0, z: -3.2, yaw: 0 });
  sys("prefab", { name: `${PREFIX}ArmoryStore`, ref: G.armoryStore, x: 5.85, y: 0, z: 1.6, yaw: -90 });
  sys("clone", { src: POLICE_LAYER, name: "Roadmap_Police_EvidenceLocker", to: `${PREFIX}EvidenceLocker`, x: 1.5, y: 0, z: -3.4, yaw: 0 });
  sys("prefab", { name: `${PREFIX}DispatchBoard`, ref: G.dispatchBoard, x: 4.2, y: 1.25, z: -3.66, yaw: 0 });
  sys("prefab", { name: `${PREFIX}Handcuffs`, ref: G.handcuffs, x: 3.0, y: 0.907, z: -0.1, yaw: 20 });
  sys("prefab", { name: `${PREFIX}Taser`, ref: G.taser, x: 3.3, y: 0.907, z: 0.1, yaw: 0 });
  // First floor: the case board in the records room.
  sys("prefab", { name: `${PREFIX}CaseBoard`, ref: G.caseBoard, x: 5.9, y: F1 + 1.0, z: -1.2, yaw: 90 });
  // The cell (the villa's own barred cell, ground west). One placed jail cell per world, and GridWorld already has one
  // (Grid_test_fixtures), so it is staged only with --with-cell (the island world has none; `town fit` sets it).
  if (args.includes("--with-cell")) {
    sys("clone", { src: JAIL_LAYER, name: "Roadmap_Jail_Cell", to: `${PREFIX}Jail_Cell`, x: -4.5, y: 0, z: -2.0, yaw: 0, retarget: ["Roadmap_Jail_Door", `${PREFIX}Jail_Door`] });
    sys("clone", { src: JAIL_LAYER, name: "Roadmap_Jail_Door", to: `${PREFIX}Jail_Door`, x: -3.0, y: 0, z: -1.5, yaw: 90 });
  }
  // The yard in front (the claim's north gap): patrol car, the escort car, a spike strip, the payphone.
  sys("clone", { src: POLICE_LAYER, name: "Roadmap_Police_Car", to: `${PREFIX}PatrolCar`, x: 1.5, y: 0.001, z: 11.5, yaw: 0 });
  sys("clone", { src: POLICE_LAYER, name: "Roadmap_Police_Payphone", to: `${PREFIX}Payphone`, x: 5.5, y: 0, z: 9.0, yaw: 180 });
  sys("clone", { src: POLICE_LAYER, name: "Roadmap_Police_SpikeStrip", to: `${PREFIX}SpikeStrip`, x: 9.0, y: 0.002, z: 13.5, yaw: 0 });
}

// ============================================================================================================
// PASS 2 - POLISH 1: lights, walls, desks, the interview room.
// ============================================================================================================
if (PASS >= 2) {
  // Lights: five dynamic spots (hall, office, cell, records room upstairs, canteen), none shadow-casting but the
  // office one. A lit lamp mesh hangs under each (no light of its own).
  lamp(-1.0, 3.7, 4.5);
  spot(-1.0, 3.5, 4.5, { color: "1 0.9 0.72 1", lv: 5.5, radius: 6 });
  lamp(3.0, 3.7, 0.0);
  spot(3.0, 3.5, 0.0, { color: "1 0.86 0.64 1", lv: 6, radius: 6.5, angle: 140, shadow: true });
  lamp(-4.6, 2.9, -2.0);
  spot(-4.6, 2.7, -2.0, { color: "0.95 0.9 0.8 1", lv: 4, radius: 4.5, angle: 120 });
  lamp(3.0, F1 + 3.0, 0.0);
  spot(3.0, F1 + 2.8, 0.0, { color: "1 0.88 0.68 1", lv: 5, radius: 6 });
  lamp(3.8, 3.7, 5.5);
  spot(3.8, 3.5, 5.5, { color: "1 0.9 0.72 1", lv: 4.5, radius: 5 });

  // Walls: notices, the radio poster and the clock by the duty desk, a flipchart of the duty roster.
  put(P.clock, 5.9, 2.2, 2.4, -90);
  put(P.noticeA, 0.72, 1.55, -1.1, 90);
  put(P.noticeB, 0.72, 1.55, -1.6, 90);
  put(P.noticeC, 0.72, 1.55, -2.1, 90);
  put(P.poster, 3.2, 1.55, 7.25, 180); // inside the front window wall of the canteen

  // Desks: typewriters and phones, the dispatch radio, a computer on the commander's desk, ashtrays, ledgers.
  put(P.typewriter, 3.2, 0.907, 0.2, 200);
  put(P.phoneRed, 4.0, 0.907, 0.05, 160);
  put(P.notebook, 2.9, 0.907, 0.1, 15);
  put(P.ashtray, 3.7, 0.907, 0.3, 0);
  put(P.radio, 1.8, 0.907, -1.7, 180);
  put(P.notebook, 2.5, 0.907, -1.7, 0);
  put(P.binIn, 1.6, 0, 0.9, 0);
  put(P.computer, 4.1, F1 + 0.906, 4.4, 200);
  put(P.phone, 3.6, F1 + 0.906, 4.3, 190);

  // The interview room (first floor, the school desk): two chairs, a tape recorder, a desk lamp.
  put(P.chairRed, -1.7, F1, -0.1, 180);
  put(P.chairGreen, -1.7, F1, -1.9, 0);
  put(P.tape, -1.6, F1 + 0.75, -1.0, 90);
  put(P.deskLamp, -1.9, F1 + 0.75, -1.2, 60);
  put(P.ashtray, -1.7, F1 + 0.75, -0.8, 0);
  put(P.flipchart, 2.0, F1, 2.8, 150);
  put(P.corkboard2, 5.9, F1 + 1.2, 1.2, -90);

  // The cell: a bucket and a blanket-less bunk are the point; add a bench lamp-free corner and a notice of rules.
  put(P.noticeD, -3.0, 1.6, -3.69, 0);
}

// ============================================================================================================
// PASS 3 - POLISH 2: facade, yard dressing, night mood, finishing touches.
// ============================================================================================================
if (PASS >= 3) {
  // Night mood: the porch lamp looks lit (emissive cover) and one soft point glow under the porch roof is the
  // sixth and last light of the building; the canteen window gets a lit lamp behind the glass. No exterior lamp
  // prefab: each of those carries its own spot and point light.
  lamp(-1.8, 2.85, 6.6);
  glow(-1.8, 2.5, 6.9, { color: "1 0.85 0.6 1", lv: 2.6, radius: 5 });
  lamp(3.8, 3.5, 6.6);
  // Yard: a bench by the canteen wall, bins, planters, a second car.
  put(P.bench, 2.6, 0, 7.85, 180);
  put(P.binStreet, -3.0, 0.461, 6.2, 180);
  put(P.pot, -3.4, 0, 7.2, 0);
  put(P.pot, 0.4, 0, 7.2, 0);
  sys("clone", { src: POLICE_LAYER, name: "Roadmap_Police_Car", to: `${PREFIX}EscortCar`, x: 6.2, y: 0.001, z: 12.0, yaw: 0 });
  // Inside: more paper on the walls, a second phone and a desk tidy.
  put(P.noticeC, -3.0, 1.55, -3.69, 0);
  put(P.notebook, 4.6, 0.907, 1.2, 0);
  put(P.typewriter, 4.3, 0.907, 1.4, 170);
}

// ---------------------------------------------------------------------------------------------------
const num = (n) => String(+(+n).toFixed(3));
const world = (x, z) => [num(ORIGIN.x + x), num(ORIGIN.z + z)];

// A top-level entity of a layer text, by name: from its header line to the closing "}" at column 0.
export function extract(text, name) {
  const lines = text.split("\n");
  const start = lines.findIndex((l) => new RegExp(`^\\w+ ${name}\\b`).test(l));
  if (start < 0) throw new Error(`no entity ${name}`);
  let end = start;
  while (end < lines.length && lines[end] !== "}") end++;
  return lines.slice(start, end + 1);
}

// The block with a new name, coords and yaw (top-level lines only: nested children keep theirs).
export function retarget(block, to, x, y, z, yaw, renames = []) {
  const out = [];
  let placed = false;
  for (const raw of block) {
    let l = raw.replace(/^(\w+) \S+/, (_, cls) => `${cls} ${to}`);
    if (raw !== block[0]) l = raw;
    for (const [a, b] of renames) l = l.split(a).join(b);
    l = l.replace(/m_sTargetId "(?:Roadmap_|Annex_)([^"]+)"/, `m_sTargetId "${PREFIX}$1"`);
    if (/^ (coords|angles|angleY) /.test(l)) {
      if (!placed) {
        const [wx, wz] = world(x, z);
        out.push(` coords ${wx} ${num(y)} ${wz}`);
        if (yaw) out.push(` angleY ${num(yaw)}`);
        placed = true;
      }
      continue;
    }
    out.push(l);
  }
  return out;
}

function stripKit(text) {
  const lines = text.split("\n");
  const out = [];
  for (let i = 0; i < lines.length; i++) {
    if (new RegExp(`^\\w+ ${PREFIX}(?!Villa\\b)`).test(lines[i])) {
      while (i < lines.length && lines[i] !== "}") i++;
      continue;
    }
    out.push(lines[i]);
  }
  return out.join("\n").replace(/\n+$/, "\n");
}

async function build() {
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
  if (!litGuid) throw new Error(`no GUID for ${LAMP_COVER_LIT}`);
  const litRef = `{${litGuid}}${LAMP_COVER_LIT}`;

  const blocks = [];
  const sources = new Map();
  const srcText = (f) => { if (!sources.has(f)) sources.set(f, readFileSync(f, "utf8")); return sources.get(f); };
  for (const s of systems) {
    if (s.kind === "clone") {
      const renames = s.retarget ? [s.retarget] : [];
      blocks.push(retarget(extract(srcText(s.src), s.name), s.to, s.x, s.y, s.z, s.yaw, renames).join("\n"));
    } else {
      const [wx, wz] = world(s.x, s.z);
      const cls = /Vehicles\//.test(s.ref) ? "Vehicle" : "GenericEntity";
      blocks.push([`${cls} ${s.name} : "${s.ref}" {`, ` coords ${wx} ${num(s.y)} ${wz}`, ...(s.yaw ? [` angleY ${num(s.yaw)}`] : []), "}"].join("\n"));
    }
  }
  props.forEach((p, i) => {
    const { ref, cls } = info(p.path);
    const [wx, wz] = world(p.x, p.z);
    const l = [`${cls} ${PREFIX}Decor_${i + 1} : "${ref}" {`];
    // Static decor: the desk clutter, phones and notices share one RigidBody component in the base game
    // ({5872F0EB7DFB5A9D}); Static 1 takes the body out of the simulation (the maintainer's no-physics rule).
    const needStatic = NEEDS_STATIC.some((n) => p.path.endsWith(n));
    if (p.lit || needStatic) {
      l.push(" components {");
      if (p.lit) l.push(`  MeshObject "{5872F0EB7FA40561}" {`, "   Materials {", `    MaterialAssignClass "{${(0xD0C0000000000000n + BigInt(i + 1)).toString(16).toUpperCase()}}" {`,
        '     SourceMaterial "LampFluorescent_01_cover"', `     AssignedMaterial "${litRef}"`, "    }", "   }", "  }");
      if (needStatic) l.push(`  RigidBody "{5872F0EB7DFB5A9D}" {`, "   Static 1", "  }");
      l.push(" }");
    }
    if (p.scale !== 1) l.push(` scale ${num(p.scale)}`);
    l.push(` coords ${wx} ${num(p.y)} ${wz}`);
    if (p.yaw) l.push(` angleY ${num(p.yaw)}`);
    l.push("}");
    blocks.push(l.join("\n"));
  });
  lights.forEach((l, i) => {
    const [wx, wz] = world(l.x, l.z);
    const r = [`LightEntity ${PREFIX}Light_${i + 1} {`, ` coords ${wx} ${num(l.y)} ${wz}`];
    if (l.type === "Spot") r.push(` angles ${l.angles}`);
    r.push(` Color ${l.color}`, ` LV ${l.lv}`, " ClipEV -10", ` Type ${l.type}`, ` Radius ${l.radius}`, " NearPlane 0.3");
    if (l.type === "Spot") r.push(` SpotAngle ${l.angle}`);
    r.push(` CastShadow ${l.shadow ? 1 : 0}`, " LensFlareType Disabled", "}");
    blocks.push(r.join("\n"));
  });
  return blocks;
}

export const counts = () => ({
  decor: props.length,
  lights: lights.length,
  shadows: lights.filter((l) => l.shadow).length,
  systems: systems.length,
});

const base = readFileSync(GRID_LAYER, "utf8");
const next = `${stripKit(base)}${(await build()).join("\n")}\n`;
const c = counts();
if (args.includes("--report")) {
  console.log(`police-station pass ${PASS}: ${c.decor} decor, ${c.lights} lights (${c.shadows} cast shadows), ${c.systems} system entities`);
  process.exit(c.lights > BUDGET.lights || c.shadows > BUDGET.shadows || c.decor > BUDGET.decor ? 1 : 0);
}
if (args.includes("--check")) {
  const same = next === base;
  console.log(same ? "police-station: the grid layer is current" : "police-station: the grid layer differs; run node tools/buildings/police-station/fitout.mjs");
  process.exit(same ? 0 : 1);
}
if (lights.some((l) => l.type === "Point" && l.shadow)) throw new Error("a point light never casts a shadow");
if (c.lights > BUDGET.lights || c.shadows > BUDGET.shadows || c.decor > BUDGET.decor) throw new Error(`over the budget: ${JSON.stringify(c)} (max ${JSON.stringify(BUDGET)})`);
writeFileSync(GRID_LAYER, next);
console.log(`police-station: pass ${PASS}: ${c.decor} decor, ${c.lights} lights (${c.shadows} shadow), ${c.systems} system entities written to Grid_police_station_kit.layer`);
