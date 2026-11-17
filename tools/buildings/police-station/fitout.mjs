// The 1989 police station fit-out: the vanilla Everon police villa (Villa_E_2I01_Police) in its grid claim, with
// the police systems standing where a station keeps them and the dressing that makes it read as a working post.
//
//   node tools/buildings/police-station/fitout.mjs [--pass 1|2|3] [--with-cell]   rewrite the PoliceKit_* block
//   node tools/buildings/police-station/fitout.mjs --check [--pass N]             exit 1 when the layer differs
//   node tools/buildings/police-station/fitout.mjs --report                       entity, light and shadow counts
//
// Passes are cumulative: 1 BUILD (systems, yard, boards, cell), 2 POLISH 1 (lights, walls, desks, interview room),
// 3 POLISH 2 (sign, lit facade, the hall's front counter, the cell, yard dressing, night mood). The default is 3, the shipped pass.
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
import { prefabChildren, snapItems, specSupports } from "../pipeline/surface.mjs";

const ROOT = join(dirname(fileURLToPath(import.meta.url)), "..", "..", "..");
const LAYERS = join(ROOT, "addons", "LifeFramework", "Worlds");
const GRID_LAYER = join(ROOT, "addons", "LF_Everon1989", "Worlds", "Everon1989Grid", "Everon1989Grid_Layers", "Grid_police_station_kit.layer"); // the pack grid cell (red line: pack)
const POLICE_LAYER = join(LAYERS, "DebugWorld", "DebugWorld_Layers", "Street_Police.layer");
const JAIL_LAYER = join(LAYERS, "DebugWorld", "DebugWorld_Layers", "Street_Jail.layer");
const STATION_DIR = join(ROOT, "addons", "LF_Everon1989", "Prefabs", "Buildings", "PoliceStation");
const FURNITURE_FILE = join(STATION_DIR, "Furniture", "PoliceStation_furniture_01.et");
const SPEC = JSON.parse(readFileSync(join(ROOT, "tools", "buildings", "police-station", "spec.json"), "utf8"));
const LICENCE_LAYER = join(LAYERS, "GridWorld", "GridWorld_Layers", "Grid_licence_desk.layer");

import { ORIGIN } from "./origin.mjs";
const PREFIX = "PoliceKit_";
const VILLA = `${PREFIX}Villa`;
const F1 = 3.8; // first floor
const BUDGET = { lights: 6, shadows: 1, total: 300 }; // MEDIUM, docs/architecture/performance.md: the villa's kept furniture (shell.mjs) plus every kit row stays under `total`

const args = process.argv.slice(2);
const flag = (n, d) => { const i = args.indexOf(n); return i >= 0 ? args[i + 1] : d; };
const PASS = Number(flag("--pass", "3")); // 3 is the shipped pass (POLISH 2, #2275)

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
  desk: "Prefabs/Props/Furniture/Desk_01_short.et",
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
  wallLamp: "Prefabs/Structures/BuildingParts/Lights/LightIndustrial_02.et", // the bare mesh: no light entities (see LAMP_GLASS_LIT)
  files: "Prefabs/Props/Civilian/FileOffice_01_Stack.et",
  corkboard3: "Prefabs/Props/Civilian/NoticeBoardCork_01/Filled/NoticeBoardCork_USSR_01_filled_V3.et",
  plant: "Prefabs/Props/Furniture/PlantRoom_02.et",
  cone: "Prefabs/Props/Infrastructure/ConeTraffic/ConeTraffic_01_red.et",
};
// The base game's lit diffuser for the fluorescent lamp's cover (the lamp meshes ship dark): a lamp that looks on and
// costs no dynamic light.
const LAMP_COVER_LIT = "Assets/Props/Furniture/LampFluorescent_02/Data/LampFluorescent_02_cover_emissive.emat";
// The exterior wall lamp's lit glass (what LightIndustrial_02_on_exterior assigns): the bare mesh looks on with no light.
const LAMP_GLASS_LIT = "Assets/Structures/BuildingsParts/Lights/LampIndustrial_01/Data/Glass_on_orange.emat";

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
  fascia: "{4697D9464B596C24}Prefabs/Props/Signs/LF_Sign_Fascia.et",
  jailCell: "{AA32D528D592E54C}Prefabs/Police/EL_JailCell.et",
};
// The building prefab the kit stands in: the vanilla police villa with its furniture cut to fit the budget (shell.mjs).
const STATION = "{%GUID%}Prefabs/Buildings/PoliceStation/PoliceStation.et";

const raw = []; // verbatim entity blocks
const systems = []; // { kind: "clone", src, name, to, x,y,z,yaw } | { kind: "prefab", name, ref, x,y,z,yaw, extra }
const props = []; // { path, x,y,z, yaw, scale, lit }
const lights = []; // { x,y,z, type, color, lv, radius, angle, angles, shadow }

const sys = (kind, o) => systems.push({ kind, ...o });
// A wall time clock: the base game's punch clock with the duty action (the desk's DutyClock child, LF_Workplace.et).
// Outside a workplace the action uses its own role, POLICE.
const clock = (name, x, y, z, yaw) => sys("clock", { name, x, y, z, yaw });
// A display piece: a model only (no inventory item, no pickup, no body), seated on its surface by the mesh's own
// bounds: the pivot y is the surface top plus half the thickness it lies on.
const MESH = {
  taser: ["{6ABAE84C4278757F}Assets/Weapons/Handguns/Taser/Taser.xob", 0.02, [0, 0, 90]],
  m9: ["{1E38513800383D64}Assets/Weapons/Handguns/M9/M9_body.xob", 0.018, [0, 0, 90]],
  cuffs: ["{073577B8970A8B88}Assets/Items/Roleplay/Handcuffs/Handcuffs.xob", 0.017, [0, 0, 0]],
};
const display = (kind, x, top, z, yaw = 0) => raw.push({ kind, x, y: +(top + MESH[kind][1]).toFixed(3), z, yaw });
// A prop row; o.on names its support (an item id or a villa furniture entity ID) and o.id names it as one.
const put = (path, x, y, z, yaw = 0, scale = 1, lit = false, o = {}) => props.push({ path, x, y, z, yaw, scale, lit, ...o });
const wallLamp = (x, y, z, yaw) => put(P.wallLamp, x, y, z, yaw, 1, "glass");
// A readable sign (LF_SignComponent on a core sign prefab): the card id, and for a painted body its colours.
const sign = (o) => sys("sign", o);
const spot = (x, y, z, o = {}) => lights.push({ type: "Spot", x, y, z, angles: "-90 0 0", color: "1 0.86 0.64 1", lv: 5, radius: 6, angle: 130, shadow: false, ...o });
const glow = (x, y, z, o = {}) => lights.push({ type: "Point", x, y, z, color: "1 0.88 0.68 1", lv: 2, radius: 5, shadow: false, ...o });
const lamp = (x, y, z) => put(P.lamp, x, y, z, 0, 1, true);

// ============================================================================================================
// PASS 1 - BUILD: the systems in their rooms, the yard, the boards.
// ============================================================================================================
if (PASS >= 1) {
  // Hall (x -1.94..0.75, z -3.7..3.5; the front door at z 3.5 opens on the loggia, z 3.5..6.9, where the public licence
  // desk stands against the canteen wall; the stairs rise west of the hall): the public front desk.
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
  // The cell (the villa's own barred cell, ground west, x -6.85..-2.12, z -3.7..-0.43, its barred door the villa's own).
  // The decision (#2275): the kit always shows that cell and door and carries no jail component, because LF_Places and
  // LF_JailCellComponent allow one placed cell per world and GridWorld has one (Grid_test_fixtures). `--with-cell` is for
  // `town fit` into a world with no cell: it stages the LF_JailCell marker only, its zone fitted to the room (2 m, not the
  // DebugWorld yard's 8 m), the release point in the hall, and no door target: the villa's barred door is the door, and an
  // EL_JailDoor would hang a second door in the same doorway. Breach and jailbreak on this cell need the villa door itself
  // to carry LF_BreachTarget (#2373); until then the zone returns a prisoner who walks out.
  if (args.includes("--with-cell")) sys("jail", { name: `${PREFIX}Jail_Cell`, x: -4.5, y: 0, z: -2.0, yaw: 0, radius: 2, release: "3 0 2.5" });
  // Time clocks: the duty action on a wall punch clock (ClockPunch_01), by the front door in the hall and by the door of
  // the duty room, so an officer clocks in on the way in. The desk keeps its own. Pivot y = mesh bottom - 0.692.
  clock(`${PREFIX}DutyClock_Hall`, 0.725, 1.2 - 0.692, -1.64, -90);
  clock(`${PREFIX}DutyClock_Room`, 1.005, 1.2 - 0.692, 1.0, 90);
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
  // office one. The villa's own ceiling lamps carry a lit cover (shell.mjs), so the fittings read as on. The hall spot
  // hangs inside the hall (x -3..0.75, z -3.7..3.5); in POLISH 1 it stood out on the porch and the hall stayed dark.
  spot(-1.0, 3.4, 0.3, { color: "1 0.9 0.72 1", lv: 5.5, radius: 6 });
  spot(3.0, 3.5, 0.0, { color: "1 0.86 0.64 1", lv: 6, radius: 6.5, angle: 140, shadow: true });
  spot(-4.6, 2.7, -2.0, { color: "0.95 0.9 0.8 1", lv: 4, radius: 4.5, angle: 120 });
  spot(3.0, F1 + 2.8, 0.0, { color: "1 0.88 0.68 1", lv: 5, radius: 6 });
  spot(3.8, 3.5, 5.5, { color: "1 0.9 0.72 1", lv: 4.5, radius: 5 });

  // Walls: notices, the radio poster and the clock by the duty desk, a flipchart of the duty roster.
  put(P.clock, 5.9, 2.2, 2.4, -90);
  // The duty room's west wall (x 1.0, faces +x): notices and the roster beside the door; the hall's east wall carries the
  // wanted board, the licence desk and a time clock.
  put(P.noticeA, 1.0, 1.5, 2.0, -90);
  put(P.noticeB, 1.0, 1.5, 2.35, -90);
  put(P.noticeC, 1.0, 1.5, 2.7, -90);

  // Desks: typewriters and phones, the dispatch radio, a computer on the commander's desk, ashtrays, ledgers. Heights are
  // hints: surface.mjs snaps each onto the top under it (the duty desk is a layer system, the others the villa's desks).
  put(P.typewriter, 3.2, 0.907, 0.2, 200);
  put(P.phoneRed, 3.55, 0.907, -0.2, 160);
  put(P.notebook, 2.9, 0.907, 0.1, 15);
  put(P.ashtray, 3.7, 0.907, 0.3, 0);
  put(P.radio, 1.8, 0.907, -1.7, 180);
  put(P.notebook, 2.5, 0.907, -1.7, 0);
  // The commander's desk (first floor, the villa's long desk 58549C7FABB20890: x 4.08..4.82, z 4.37..6.35; the villa's own
  // typewriter, phone and ashtray stand on its far end): the computer at the near end, its screen to the chair.
  put(P.computer, 4.45, F1 + 0.906, 4.75, -90, 1, false, { on: "58549C7FABB20890" });

  // The interview room (first floor, the school desk): two chairs, a tape recorder, a desk lamp.
  put(P.chairRed, -1.7, F1, -0.1, 180);
  put(P.chairGreen, -1.7, F1, -1.9, 0);
  put(P.tape, -1.6, F1 + 0.75, -1.0, 90, 1, false, { on: "58549C7FABB204AA" });
  put(P.deskLamp, -1.9, F1 + 0.75, -1.2, 60, 1, false, { on: "58549C7FABB204AA" });
  put(P.ashtray, -1.7, F1 + 0.75, -0.8, 0, 1, false, { on: "58549C7FABB204AA" });
  put(P.flipchart, 2.0, F1, 2.8, 150);
  // The armory desk (duty room, free floor by the door): what the issue desk hands out, laid out as models. A plain office
  // desk (Table_01 comes with a gingham cloth): its probed top is 0.906, its pivot the right-hand end, so it spans x 1.35..3.05
  // and z 1.4..2.15. Each piece sits on the top by its own thickness. Models only: nothing here can be taken.
  put(P.desk, 3.05, 0, 1.4, 0);
  display("taser", 1.65, 0.906, 1.62, 10);
  display("taser", 1.95, 0.906, 1.62, -8);
  display("taser", 2.25, 0.906, 1.62, 6);
  display("m9", 2.6, 0.906, 1.65, 90);
  display("m9", 2.9, 0.906, 1.65, 80);
  display("cuffs", 1.7, 0.906, 1.95, 0);
  display("cuffs", 2.0, 0.906, 1.95, 25);
  display("cuffs", 2.3, 0.906, 1.95, -15);
  put(P.corkboard2, 5.9, F1 + 1.2, 1.2, -90);

  // The cell's back wall: a notice.
  put(P.noticeD, -3.0, 1.6, -3.655, 0);
}

// ============================================================================================================
// PASS 3 - POLISH 2: facade, sign, the hall's front counter, the cell, yard dressing, night mood.
// ============================================================================================================
if (PASS >= 3) {
  // The facade. The porch is the villa's recessed loggia (x -2.9..0.75, from the hall's front door at z 3.5 out to the step
  // at z 6.9; the first floor rests over it). The fascia (LF_Sign_Fascia, 4.3 m, origin its bottom edge, face +z) runs over
  // the loggia mouth; its card is the pack's police_front (the place name lives in the pack table, never the core).
  sign({ ref: G.fascia, name: `${PREFIX}Sign_Front`, id: "police_front", x: -1.07, y: 2.95, z: 6.93, yaw: 0, rim: "0.85 0.85 0.88", paint: "0.08 0.16 0.38", text: "0.95 0.95 0.97" });
  // A lit facade at night inside the light budget: two wall lamps whose glass is the base game's lit orange (the bare
  // LightIndustrial_02 mesh, no light entities of its own: the `_on_exterior` variant brings a spot and a point each), and
  // one warm point glow in front of the loggia, the sixth and last light.
  wallLamp(1.45, 2.55, 6.9, 0);
  wallLamp(0.25, 2.45, 3.76, 0);
  // The glow stands just out from the fascia (z 7.6) so it washes the sign, the loggia mouth and the step at night.
  glow(-1.0, 3.3, 7.6, { color: "1 0.84 0.6 1", lv: 3.4, radius: 7 });

  // The hall (x -1.94..0.75 between the cell wall and the duty-room wall; the cell's barred door is in the west wall at
  // z -2.7..-1.26, the front door at z 3.5): a front counter across the far end, facing the door as the public walks in (a short
  // office desk, x -1.55..0.15, z -2.98..-2.23, clear of the cell doorway and the east bench), the desk officer's chair behind
  // it, the paper of a working counter on it, the station's notice board on the wall above, a plant in the corner.
  put(P.desk, 0.15, 0, -2.98, 0, 1, false, { id: "counter" });
  put(P.chairGreen, -0.7, 0, -3.35, 0);
  put(P.phone, -0.15, 0.907, -2.5, 170, 1, false, { on: "counter" });
  put(P.typewriter, -0.75, 0.907, -2.6, 180, 1, false, { on: "counter" });
  put(P.notebook, -1.2, 0.907, -2.45, 200, 1, false, { on: "counter" });
  put(P.files, -1.35, 0.907, -2.8, 90, 1, false, { on: "counter" });
  put(P.corkboard3, -0.7, 1.2, -3.66, 0);
  put(P.plant, 0.45, 0, -3.3, 30);
  put(P.binIn, -1.75, 0, -3.4, 0);

  // The cell (x -6.85..-2.12, its barred door the villa's own in the east wall): the rules pinned over the benches on the
  // back wall (z -3.69, faces +z, the depth the hall's notice board reads at). An LF_Sign plate drew its face black in the look
  // (the render-target face), so the cell keeps paper until that is checked in a client.
  put(P.noticeA, -4.3, 1.55, -3.655, 0);
  put(P.noticeC, -3.95, 1.55, -3.655, 0);

  // Yard: a street bench against the canteen's front wall, a wall bin by the loggia, planters either side of the step,
  // traffic cones by the spike strip, the escort car beside the patrol car.
  put(P.bench, 3.0, 0, 7.3, 0);
  put(P.binStreet, 1.45, 0.461, 6.92, 0);
  put(P.pot, -3.3, 0, 7.4, 0);
  put(P.pot, 1.05, 0, 7.4, 0);
  put(P.cone, 7.8, 0, 13.0, 0);
  put(P.cone, 10.2, 0, 13.0, 0);
  put(P.cone, 7.8, 0, 14.0, 0);
  sys("clone", { src: POLICE_LAYER, name: "Roadmap_Police_Car", to: `${PREFIX}EscortCar`, x: 4.6, y: 0.001, z: 12.0, yaw: 0 });
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

// The wall time clock: components copied from LF_Workplace.et's DutyClock (sound, the clock-in action).
function clockBlock(name, wx, y, wz, yaw) {
  return [`GenericEntity ${name} : "{E7EA9EA108001D70}Prefabs/Props/Civilian/ClockPunch_01.et" {`, " components {",
    '  SCR_SoundDataComponent "{BC459247BBA4457F}" {', "   m_aAudioSourceConfiguration {", '    SCR_AudioSourceConfiguration "{B46B5DD6F8897BFD}" {',
    '     m_sSoundProject "{5A46144CB8BFC66B}Sounds/Structures/Military/Training/RangeCP/Structures_RangeCP.acp"', '     m_sSoundEventName "SOUND_RANGECP_STARTBUTTON"',
    "    }", "   }", "  }", '  ActionsManagerComponent "{775836274AF8BE66}" {', "   ActionContexts {", '    UserActionContext "{22A17EEEA2399F2D}" {',
    '     ContextName "LF_DutyClock"', '     Position PointInfo "{B046DE7E017B07C6}" {', "      Offset -0.0228 0.928 0.1792", "     }", "     Radius 1.58", "    }", "   }",
    "   additionalActions {", '    LF_DutyAction "{E0CCF06A3D9A2E06}" {', "     ParentContextList {", '      "LF_DutyClock"', "     }", '     UIInfo UIInfo "{857016BF95624B25}" {',
    '      Name "#LF-Duty_ClockIn"', '      Description "#LF-Duty_ClockDesc"', "     }", '     "Sort Priority" 1', "    }", "   }", "  }", " }",
    ` coords ${wx} ${num(y)} ${wz}`, ` angleY ${num(yaw)}`, "}"].join("\n");
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
  const glassGuid = db.guidOf(LAMP_GLASS_LIT);
  if (!glassGuid) throw new Error(`no GUID for ${LAMP_GLASS_LIT}`);
  const glassRef = `{${glassGuid}}${LAMP_GLASS_LIT}`;

  const blocks = [];
  const sources = new Map();
  const srcText = (f) => { if (!sources.has(f)) sources.set(f, readFileSync(f, "utf8")); return sources.get(f); };
  for (const s of systems) {
    if (s.kind === "clock") {
      const [wx, wz] = world(s.x, s.z);
      blocks.push(clockBlock(s.name, wx, s.y, wz, s.yaw));
      continue;
    }
    if (s.kind === "sign") {
      const [wx, wz] = world(s.x, s.z);
      const paint = s.paint ? [`  m_vRimColor ${s.rim}`, `  m_vPaintColor ${s.paint}`, `  m_vTextColor ${s.text}`] : [];
      blocks.push([`GenericEntity ${s.name} : "${s.ref}" {`, " components {", '  LF_SignComponent "{6E3FF3980D87CACE}" {', `   m_sSignId "${s.id}"`,
        ...paint.map((l) => ` ${l}`), "  }", " }", ` coords ${wx} ${num(s.y)} ${wz}`, ...(s.yaw ? [` angleY ${num(s.yaw)}`] : []), "}"].join("\n"));
      continue;
    }
    if (s.kind === "jail") {
      // The cell marker alone (the DebugWorld clone carried a crate as a debug marker and a door target): see --with-cell above.
      const [wx, wz] = world(s.x, s.z);
      blocks.push([`GenericEntity ${s.name} : "${G.jailCell}" {`, " components {", '  LF_JailCellComponent "{540EBBB989D3A80A}" {', `   m_fZoneRadius ${s.radius}`,
        '   m_sDoorTargetId ""', `   m_vReleaseOffset ${s.release}`, "  }", " }", ` coords ${wx} ${num(s.y)} ${wz}`, ...(s.yaw ? [` angleY ${num(s.yaw)}`] : []), "}"].join("\n"));
      continue;
    }
    if (s.kind === "clone") {
      const renames = s.retarget ? [s.retarget] : [];
      blocks.push(retarget(extract(srcText(s.src), s.name), s.to, s.x, s.y, s.z, s.yaw, renames).join("\n"));
    } else {
      const [wx, wz] = world(s.x, s.z);
      const cls = /Vehicles\//.test(s.ref) ? "Vehicle" : "GenericEntity";
      blocks.push([`${cls} ${s.name} : "${s.ref}" {`, ` coords ${wx} ${num(s.y)} ${wz}`, ...(s.yaw ? [` angleY ${num(s.yaw)}`] : []), "}"].join("\n"));
    }
  }
  // Heights: each prop stands on the top under it, read offline (surface.mjs: the villa's kept furniture, the kit's systems,
  // the villa's own colliders); `tools\cli building check police-station` judges the written layer with the same tops.
  const furn = prefabChildren(readFileSync(FURNITURE_FILE, "utf8"));
  const sysEnts = prefabChildren(blocks.join("\n")).map((e) => ({ ...e, x: e.x - ORIGIN.x, z: e.z - ORIGIN.z }));
  const items = props.map((p) => ({ path: p.path, x: p.x, y: p.y, z: p.z, yaw: p.yaw, scale: p.scale, id: p.id, on: p.on }));
  const snap = await snapItems(items, { fixed: [...furn, ...sysEnts, ...specSupports(SPEC)] });
  items.forEach((it, i) => { props[i].y = it.y; });
  if (!args.includes("--check") && !args.includes("--report")) console.log(snap.summary);
  raw.forEach((d, i) => {
    const [ref, , angles] = MESH[d.kind];
    const [wx, wz] = world(d.x, d.z);
    const id = (0xD15C000000000000n + BigInt(i + 1)).toString(16).toUpperCase();
    blocks.push([`GenericEntity ${PREFIX}Display_${d.kind}_${i + 1} {`, " components {", `  MeshObject "{${id}}" {`, `   Object "${ref}"`, "  }", " }",
      ` coords ${wx} ${num(d.y)} ${wz}`, ` angles ${angles[0]} ${num(d.yaw)} ${angles[2]}`, "}"].join("\n"));
  });
  props.forEach((p, i) => {
    const { ref, cls } = info(p.path);
    const [wx, wz] = world(p.x, p.z);
    const l = [`${cls} ${PREFIX}Decor_${i + 1} : "${ref}" {`];
    // Static decor: the desk clutter, phones and notices share one RigidBody component in the base game
    // ({5872F0EB7DFB5A9D}); Static 1 takes the body out of the simulation (the maintainer's no-physics rule).
    const needStatic = NEEDS_STATIC.some((n) => p.path.endsWith(n));
    if (p.lit || needStatic) {
      l.push(" components {");
      if (p.lit === "glass") l.push(`  MeshObject "{5111E74D5E891458}" {`, "   Materials {", `    MaterialAssignClass "{${(0xD0C0000000000000n + BigInt(i + 1)).toString(16).toUpperCase()}}" {`,
        '     SourceMaterial "Glass"', `     AssignedMaterial "${glassRef}"`, "    }", "   }", "  }");
      else if (p.lit) l.push(`  MeshObject "{5872F0EB7FA40561}" {`, "   Materials {", `    MaterialAssignClass "{${(0xD0C0000000000000n + BigInt(i + 1)).toString(16).toUpperCase()}}" {`,
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

const villaProps = () => prefabChildren(readFileSync(FURNITURE_FILE, "utf8")).length;
export const counts = () => ({
  villa: villaProps(),
  decor: props.length + raw.length,
  lights: lights.length,
  shadows: lights.filter((l) => l.shadow).length,
  systems: systems.length,
});

const base = readFileSync(GRID_LAYER, "utf8");
// The villa entity stands in our building prefab (the vanilla villa with its furniture cut to fit, shell.mjs).
const stationGuid = /Name "\{([0-9A-F]{16})\}/.exec(readFileSync(join(STATION_DIR, "PoliceStation.et.meta"), "utf8"))?.[1];
if (!stationGuid) throw new Error("PoliceStation.et.meta is missing: run node tools/buildings/police-station/shell.mjs");
const station = STATION.replace("%GUID%", stationGuid);
const villaLine = new RegExp(String.raw`^(\w+ ${VILLA}) : "[^"]+"`, "m");
const next = `${stripKit(base).replace(villaLine, `$1 : "${station}"`)}${(await build()).join("\n")}\n`;
const c = counts();
if (args.includes("--report")) {
  console.log(`police-station pass ${PASS}: ${c.villa + c.decor + c.systems + c.lights} entities of ${BUDGET.total} (${c.villa} villa furniture kept, ${c.decor} decor, ${c.systems} system entities, ${c.lights} lights), ${c.lights} dynamic lights of ${BUDGET.lights} (${c.shadows} cast shadows of ${BUDGET.shadows})`);
  process.exit(c.lights > BUDGET.lights || c.shadows > BUDGET.shadows || c.villa + c.decor + c.systems + c.lights > BUDGET.total ? 1 : 0);
}
if (args.includes("--check")) {
  const same = next === base;
  console.log(same ? "police-station: the grid layer is current" : "police-station: the grid layer differs; run node tools/buildings/police-station/fitout.mjs");
  process.exit(same ? 0 : 1);
}
if (lights.some((l) => l.type === "Point" && l.shadow)) throw new Error("a point light never casts a shadow");
if (c.lights > BUDGET.lights || c.shadows > BUDGET.shadows || c.villa + c.decor + c.systems + c.lights > BUDGET.total) throw new Error(`over the budget: ${JSON.stringify(c)} (max ${JSON.stringify(BUDGET)})`);
writeFileSync(GRID_LAYER, next);
console.log(`police-station: pass ${PASS}: ${c.decor} decor, ${c.lights} lights (${c.shadows} shadow), ${c.systems} system entities written to Grid_police_station_kit.layer`);
