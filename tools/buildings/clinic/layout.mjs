// The clinic's layout in building space: House_Town_E_2I02_v2's own frame. The front faces +z, x runs along the
// facade, the ground floor is at y 0.26, the first floor at 4.06 and the attic (not used) at 7.48. One place to
// change a position. decor.mjs writes `decorOf(pass)` into the furniture prefab, kit.mjs writes `kitOf(pass)` into
// Clinic_Kit.et and layer.mjs writes `placed` (NPC, till, cabinet) into the grid layer. Every prop is a base-game
// static prop; the interactables are the existing hospital systems.
//
// Rooms (measured from the survey pictures, 2026-11-17; the house is a flat with a long east wing):
//   waiting    x 4.0..8.85, z 1.9..5.6   ground east wing, the living room: benches on the east wall, the TV, the
//                                         medic dispatch board; the reception counter closes its back at z 1.0..1.75
//   reception  x 4.7..8.0,  z 0.1..1.75  the duty desk and the till counter, the doctor stands behind them
//   treatment  x -0.6..3.4, z 0.6..5.6   ground, the old bedroom: exam couch, medicine cabinet, the doctor's desk
//   ward       x -8.9..-4.3, z -3.0..5.2 ground, the tiled west room: two hospital beds (respawn and bed delivery)
//   staff      x 2.5..8.9,  z -5.6..-1.5  the kitchen at the back: kept as the staff kitchen
//   flat       first floor               the doctor's flat, kept as the game has it
//
// Heights above a surface come from TOP (the real top faces from `tools\cli measure <prefab> --surfaces`), never a
// guess: a small prop stands at F + TOP[support] (+ a tabletop offset when the support is a stack).
export const F = 0.26;
// From POLISH 1 the ward's vanilla racks go (the medical cabinets stand on that wall)
export const RACKS_UNTIL = 0;

export const P = {
  dutyDesk: "{A0B78CD65C661B15}Prefabs/Medic/LF_MedicDutyDesk.et",
  bed: "{B643FBB194A8C8D9}Prefabs/Medic/LF_HospitalBed.et",
  board: "{38D6A62C995DACCA}Prefabs/Comms/LF_DispatchBoard_Medic.et",
  doctor: "{C001B9088678C653}Prefabs/Characters/Npcs/Outfits/LF_Npc_Medic_1.et",
  couch: "Prefabs/Props/Services/Healthcare/BedMedical_01.et",
  cabMed: "Prefabs/Props/Services/Healthcare/CabinetMedical_01.et",
  cabMedSingle: "Prefabs/Props/Services/Healthcare/CabinetMedical_01_single.et",
  scale: "Prefabs/Props/Services/Healthcare/ScaleMedical_01.et",
  binMed: "Prefabs/Props/Services/Healthcare/BinMedical_01.et",
  lampMed: "Prefabs/Props/Services/Healthcare/LampMedical_01.et",
  firstAid: "Prefabs/Props/Furniture/FirstAidBox_01.et",
  bench: "Prefabs/Props/Furniture/BenchOffice_01/BenchOffice_01_Base.et",
  chairOffice: "Prefabs/Props/Furniture/ChairOffice_01.et",
  phone: "Prefabs/Props/Furniture/PhoneTable_01_red.et",
  notebook: "Prefabs/Props/Civilian/Notebook_01_Open.et",
  pen: "Prefabs/Props/Civilian/Pen_01/Pen_01_base.et",
  typewriter: "Prefabs/Props/Civilian/Typewriter_01/Typewriter_01_Base.et",
  frame: "Prefabs/Props/Civilian/TablePhotoFrame_01/TablePhotoFrame_01_V3.et",
  radio: "Prefabs/Props/Civilian/RadioSmall_01_red.et",
  clock: "Prefabs/Props/Furniture/ClockWall_01.et",
  plant: "Prefabs/Props/Furniture/PlantRoom_02.et",
  bin: "Prefabs/Props/Garbage/Bins/BinInterior_01_green.et",
  noticeV: "Prefabs/Props/Commercial/Posters/Notice_A4_Vertical_V3.et",
  noticeH: "Prefabs/Props/Commercial/Posters/Notice_A4_Horizontal_V3.et",
  insurance: "Prefabs/Props/Commercial/Posters/PosterSingle_A3_Insurance_V1.et",
  radioPoster: "Prefabs/Props/Commercial/Posters/PosterSingle_27x41_01_RadioEveron.et",
  paintingSmall: "Prefabs/Props/Decorations/Paintings/PaintingSmall_01_V3.et",
  porchLamp: "Prefabs/Props/Furniture/LightWall_01.et",
  pot: "Prefabs/Props/Decorations/Flowerpots/FlowerpotStreet_01/FlowerpotStreet_01_rect_long_begonia_red.et",
  benchOut: "Prefabs/Props/Furniture/BenchWooden_03.et",
  fileOpen: "Prefabs/Props/Civilian/FileOffice_01_Open.et",
  fileStack: "Prefabs/Props/Civilian/FileOffice_01_Stack.et",
  deskLamp: "Prefabs/Props/Furniture/LightTable_01.et",
  carpet: "Prefabs/Props/Furniture/Carpet_03_red.et",
  cashier: "Prefabs/Props/Commercial/CashierShop_01.et",
  infusion: "Prefabs/Props/Medical/InfusionStand_01/InfusionStand_01.et",
  tableMed: "Prefabs/Props/Medical/TableMedical_01/TableMedical_01_small.et",
  ashtray: "Prefabs/Props/Civilian/Ashtray_01_Full_clear.et",
  magazines: "Prefabs/Props/Civilian/Notebook_01_Stack.et",
  coatRack: "Prefabs/Props/Furniture/CoatRack_02.et",
  binStreet: "Prefabs/Props/Garbage/Bins/BinStreet_01.et",
};

// Top faces, in metres above each prop's origin (tools\cli measure <prefab> --surfaces, 2026-11-17).
export const TOP = {
  dutyDesk: 0.906, // Desk_01 mesh: the desk top (x -1.70..0, z 0..0.75 from its origin)
  tillCounter: 1.039, // CounterShop_01: the counter top, where the cashier stands (the same counter the drug store uses)
  desk: 0.906, // Desk_01_Base
  tableSmall: 0.705, // TableSmall_01
  tableCoffee: 0.6, // TableCoffee_01
  cabMed: 1.77, // CabinetMedical_01 top
  bench: 0.505, // BenchOffice_01 seat
  deskLong: 0.906, // Desk_01_long (the doctor's desk, vanilla at x -0.24..0.51, z 2.37..4.35)
  tableMed: 1.001, // TableMedical_01_small: the instrument table's top
};

// A row's `pass` says which pass adds it (0 BUILD, 1 POLISH 1, 2 POLISH 2); `until` stops it after a pass.
const items = [];
const lights = [];
const put = (pass, path, x, y, z, yaw = 0, scale = 1, until = 9) => items.push({ pass, path, x, y, z, yaw, scale, until });
const glow = (pass, x, y, z, o = {}) => lights.push({ pass, x, y, z, ...o });

// ---- BUILD (pass 0): the clinic's furniture in the rooms the vanilla set left bare ----
// treatment room: the exam couch along the front wall, a medical cabinet and a scale
put(0, P.couch, 1.4, F, 3.9, 0);
put(0, P.cabMed, 0.55, F, 1.27, 0); // the room's back wall is at z 1.0 (the vanilla wardrobe and bookshelf stood against it)
// ward: the two hospital beds are interactables (kit.mjs); a medical bin between them
// waiting room: two office benches against the east wall
put(0, P.bench, 8.45, F, 3.3, -90);
put(0, P.bench, 8.45, F, 4.6, -90);

// ---- POLISH 1 (pass 1): lights, walls, signage, things on the surfaces ----
put(1, P.cashier, 5.1, F + TOP.tillCounter, 1.3, 180); // on the till counter (CounterShop_01 spans x 4.7..6.2, z 0.83..1.95)
// reception: a phone, the ledger and a pen on the duty desk (its top is TOP.dutyDesk above F; the desk spans x 6.2..7.9, z 1.0..1.75)
// the punch clock (the duty desk's own child) stands at the desk's middle, x 6.85..7.25: the dressing keeps to the two ends
put(1, P.phone, 7.7, F + TOP.dutyDesk, 1.45, 20);
put(1, P.notebook, 6.45, F + TOP.dutyDesk, 1.35, 180);
put(1, P.pen, 6.7, F + TOP.dutyDesk, 1.3, 40);
put(1, P.chairOffice, 8.1, F, 0.45, 160); // pushed back beside the doctor, who stands at x 7.05
put(1, P.clock, 6.4, F + 2.3, 0.45, 0);
put(1, P.insurance, 5.4, F + 1.55, 0.45, 0, 0.9);
put(1, P.noticeH, 7.9, F + 1.5, 0.45, 0, 0.9);
// waiting room walls
put(1, P.radioPoster, 8.74, F + 1.6, 5.0, -90, 0.9); // the east wall's face is at x 8.77 (the vanilla clock hangs there)
put(1, P.plant, 8.5, F, 5.2, 0);
// treatment room: the scale by the couch, a bin and the first-aid box on the cabinet top
put(1, P.scale, 2.7, F, 4.3, 90);
put(1, P.binMed, 0.4, F, 4.9, 0);
put(1, P.firstAid, 0.55, F + TOP.cabMed, 1.27, 0);
put(1, P.lampMed, 2.95, F, 5.25, 200);
// ward: a bin and a cabinet between the beds and the door
put(1, P.cabMedSingle, -8.5, F, -1.2, 90); // backs on the west wall (the cabinet's back is 0.25 behind its origin)
put(1, P.cabMed, -8.5, F, -2.3, 90);
put(1, P.binMed, -4.9, F, 4.6, 0);
put(1, P.noticeV, -8.72, F + 1.5, 3.4, 90, 0.9); // the west wall's face is at x -8.75
// lights: one per room, point lights, no shadows
glow(1, 6.6, F + 3.1, 3.4, { lv: 7, radius: 7 }); // waiting and reception
glow(1, 1.5, F + 3.1, 3.2, { lv: 6, radius: 6 }); // treatment room
glow(1, -6.6, F + 3.1, 0.8, { lv: 6, radius: 9 }); // ward (the long room: a wider reach)
glow(1, -2.4, F + 3.1, 3.0, { lv: 5, radius: 6 }); // the entrance corridor
glow(1, 5.8, F + 3.1, -3.4, { lv: 5, radius: 6 }); // the staff kitchen

// ---- POLISH 2 (pass 2): lived-in detail. Every small prop stands at F + TOP[support]. ----
// treatment room: the doctor's desk is used (lamp, notes, a file), an instrument table by the east wall, a drip stand by the couch
put(2, P.deskLamp, 0.1, F + TOP.deskLong, 2.75, 270);
put(2, P.notebook, 0.15, F + TOP.deskLong, 3.45, 90);
put(2, P.pen, 0.3, F + TOP.deskLong, 3.7, 20);
put(2, P.fileStack, 0.05, F + TOP.deskLong, 3.95, 95);
put(2, P.tableMed, 3.3, F, 3.4, -90);
put(2, P.notebook, 3.3, F + TOP.tableMed, 3.4, 0, 0.8);
put(2, P.infusion, 0.45, F, 4.5, 0);
// waiting room: magazines and an ashtray on the coffee table, a carpet
put(2, P.magazines, 6.9, F + TOP.tableCoffee, 3.2, 20);
put(2, P.ashtray, 7.6, F + TOP.tableCoffee, 3.25, 0);
put(2, P.carpet, 6.6, F, 4.6, 0, 0.8);
put(2, P.paintingSmall, 4.3, F + 1.9, 0.45, 0, 0.9);
// the entrance corridor: a coat rack on the west wall (the vanilla one upstairs hangs at the same place)
put(2, P.coatRack, -4.18, F + 0.27, 4.2, 90);
// ward: a drip stand at the foot of the beds
put(2, P.infusion, -6.85, F, 3.55, 0);
// the facade: the lamp the glow comes from, planters either side of the door, a bench and a bin by the wall
put(2, P.porchLamp, -3.6, 2.9, 6.66, 0);
put(2, P.pot, -4.0, 0, 7.9, 0);
put(2, P.pot, -0.8, 0, 7.9, 0);
put(2, P.benchOut, 1.6, 0, 7.5, 0);
put(2, P.binStreet, 4.6, 0, 7.6, 0);

export const decorOf = (pass) => ({
  items: items.filter((i) => i.pass <= pass && i.until >= pass),
  lights: lights.filter((l) => l.pass <= pass),
});

// ---- the kit (Clinic_Kit.et children, building space) ----
export const beds = [
  { name: "Ward_Bed_1", x: -7.7, y: F, z: 2.0, yaw: 0 },
  { name: "Ward_Bed_2", x: -6.0, y: F, z: 2.0, yaw: 0 },
];
export const dutyDesk = { x: 7.9, y: F, z: 1.0, yaw: 0 };
export const board = { x: 8.74, y: F + 1.15, z: 3.9, yaw: -90 };
export const alarmBox = { x: 3.5, y: 3.05, z: 6.66, yaw: 0 }; // between the two front windows (x 1.5 and 5.6), above the ground floor's lintels
export const sign = { x: -2.4, y: 3.1, z: 6.74, yaw: 0 }; // the fascia over the entrance door (door at x -2.4)
export const facade = { x: -3.6, y: 2.85, z: 6.95 }; // in front of the porch lamp mesh (pass 2), beside the sign and off the door // pass 1: one warm point light over the entrance

// ---- layer rows (grid layer entities, building space x, z, yaw) ----
export const placed = {
  doctor: { name: "Clinic_Doctor", prefab: P.doctor, cls: "LF_NpcBaseEntity", x: 7.05, y: F, z: 0.35, yaw: 0 },
  till: { name: "Clinic_Till", x: 5.45, y: F, z: 1.35, yaw: 0 }, // CounterShop_01, centred: x 4.7..6.2
  cabinet: { name: "Clinic_Cabinet", x: 3.2, y: F, z: 1.05, yaw: 0 }, // origin at its back, which stands on the wall at z 1.0
};
