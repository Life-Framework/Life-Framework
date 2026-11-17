// The bank's layout in building space (Office_E_01's own frame: front faces +z, x along the facade, floor at y 1.053).
// One place to change a position; decor.mjs writes `decor` into the furniture prefab, kit.mjs writes `kit` and the
// staging script writes `npcs`. Every prop is base-game, static and unphysical.
//
// Rooms (measured from the look pictures, 2026-11-16):
//   hall     x -1.4..3.4, z 1.3..6.6   ground, front-centre; its door is on the back wall (z 1.2), windows on the front
//   manager  x  3.55..7.7, z 1.3..6.6  ground, front-right; door on the back wall, windows front and right
//   vault    an annex on the east wall, as the playtest town's bank vault is built on that bank's east wall
export const F = 1.053;
export const P = {
  counter: "Prefabs/Props/Services/Post/CounterPostOffice_01_window.et",
  counterPlain: "Prefabs/Props/Services/Post/CounterPostOffice_01.et",
  cashier: "Prefabs/Props/Commercial/CashierShop_01.et",
  desk: "Prefabs/Props/Furniture/Desk_01_Base.et",
  chairOffice: "Prefabs/Props/Furniture/ChairOffice_01.et",
  chair: "Prefabs/Props/Furniture/Chair_01/Chair_01_base.et",
  bench: "Prefabs/Props/Furniture/BenchWooden_01.et",
  tableSmall: "Prefabs/Props/Furniture/TableSmall_01/TableSmall_01_DarkWood.et",
  cabinet: "Prefabs/Props/Furniture/CabinetMetal_01_grey_V3.et",
  safe: "Prefabs/Props/Furniture/Safe_01.et",
  carpet: "Prefabs/Props/Furniture/Carpet_03_red.et",
  plant: "Prefabs/Props/Furniture/PlantRoom_01.et",
  clock: "Prefabs/Props/Furniture/ClockWall_01.et",
  phone: "Prefabs/Props/Furniture/PhoneTable_01_red.et",
  notebook: "Prefabs/Props/Civilian/Notebook_01_Open.et",
  pen: "Prefabs/Props/Civilian/Pen_01/Pen_01_base.et",
  typewriter: "Prefabs/Props/Civilian/Typewriter_01/Typewriter_01_Base.et",
  tape: "{CD9C68B74E4AB068}Prefabs/Props/Roadblocks/RallySafteyTape_01/RallySafetyTape_01_2m.et",
  insurance: "Prefabs/Props/Commercial/Posters/PosterSingle_A3_Insurance_V1.et",
  noticeV: "Prefabs/Props/Commercial/Posters/Notice_A4_Vertical_V3.et",
  noticeH: "Prefabs/Props/Commercial/Posters/Notice_A4_Horizontal_V3.et",
  radioPoster: "Prefabs/Props/Commercial/Posters/PosterSingle_27x41_01_RadioEveron.et",
  frame: "Prefabs/Props/Civilian/TablePhotoFrame_01/TablePhotoFrame_01_V3.et",
  paintingBig: "Prefabs/Props/Decorations/Paintings/PaintingBig_01_V4.et",
  bin: "Prefabs/Props/Garbage/Bins/BinInterior_01_green.et",
  wallLamp: "Prefabs/Structures/BuildingParts/Lights/LampIndustrial_01_on_exterior.et",
  chalk: "Prefabs/Props/Services/ChalkBoard_01/ChalkBoard_01.et",
  bookshelf: "Prefabs/Props/Furniture/Bookshelf_01/Bookshelf_01.et",
  benchOut: "Prefabs/Props/Furniture/BenchWooden_03.et",
  pot: "Prefabs/Props/Decorations/Flowerpots/FlowerpotStreet_01/FlowerpotStreet_01_rect_long_begonia_red.et",
};

// ---- static decor, written into Furniture/Bank_furniture_01.et (items) ----
export const decor = { items: [], lights: [] };
const put = (path, x, y, z, yaw = 0, scale = 1, angles = null) => decor.items.push({ path, x, y, z, yaw, scale, angles });
const glow = (x, y, z, o = {}) => decor.lights.push({ x, y, z, ...o });

// Banking hall. The counter runs across at z 4.3 (the teller side is the windows), the public stands on the back side.
put(P.carpet, 1.0, F, 2.9, 0);
put(P.counter, 3.25, F, 4.3, 180);
put(P.counter, 1.74, F, 4.3, 180);
put(P.counter, 0.23, F, 4.3, 180);
// deposit-slip desk against the right wall, slips and pen on it
put(P.tableSmall, 3.0, F, 2.3, 0);
put(P.notebook, 3.0, F + 0.7, 2.3, 20);
put(P.pen, 2.9, F + 0.7, 2.45, 70);
put(P.noticeV, 3.34, F + 1.55, 2.3, 90, 0.8);
// queue rope (the base game has no stanchion rope; a red-and-white rally tape on its posts stands in), a bench and a plant for the waiting
put(P.tape, 0.1, F, 3.55, 0);
put(P.bench, 2.9, F, 3.35, -90);
put(P.plant, 3.05, F, 1.7, 0);
// walls: wall clock on the back wall, rates and insurance notices, a picture
put(P.clock, 1.0, F + 2.3, 1.34, 0);
put(P.insurance, 2.1, F + 1.5, 1.34, 0, 0.9);
put(P.noticeH, 0.2, F + 1.4, 1.34, 0, 0.9);
put(P.radioPoster, -1.35, F + 1.5, 3.0, 90);
put(P.paintingBig, 3.34, F + 1.7, 5.4, 90);
// teller side: ledger and phone on the counter, a chair behind each window
put(P.phone, 2.5, F + 1.0, 4.6, 180);
put(P.chairOffice, 2.45, F, 5.3, 190);
put(P.chairOffice, 0.75, F, 5.3, 170);
put(P.cabinet, 3.0, F, 6.2, 90);
put(P.typewriter, 0.7, F + 1.0, 4.62, 180);

// Manager's office.
put(P.carpet, 5.6, F, 4.4, 0);
put(P.desk, 6.7, F, 3.4, -90);
put(P.chairOffice, 6.0, F, 3.9, 90);
put(P.chair, 4.9, F, 3.4, -90);
put(P.chair, 4.9, F, 4.3, -90);
put(P.safe, 4.2, F, 6.1, 180);
put(P.cabinet, 7.35, F, 6.2, -90);
put(P.plant, 4.0, F, 1.9, 0);
put(P.frame, 6.7, F + 0.91, 3.9, 200);
put(P.phone, 6.6, F + 0.91, 3.0, 90);
put(P.insurance, 5.2, F + 1.6, 1.34, 0, 0.9);

// ---- Pass 1: walls dressed, the counters worked, the office and the pavement used ----
// hall walls: a rates board on the back wall, a calendar-style notice and a picture on the left wall
put(P.chalk, 2.7, F + 1.2, 1.36, 0, 0.9);
put(P.noticeV, -1.36, F + 1.5, 2.2, 90, 0.9);
put(P.noticeH, -1.36, F + 1.5, 3.7, 90, 0.9);
put(P.frame, 0.2, F + 1.0, 4.45, 180);
// counters: a till and a ledger with a pen at each window, a stamp pad's worth of paper beside them
put(P.cashier, 2.0, F + 1.0, 4.55, 180);
put(P.cashier, 0.45, F + 1.0, 4.55, 180);
put(P.notebook, 1.25, F + 1.0, 4.55, 170);
put(P.pen, 1.15, F + 1.0, 4.5, 100);
put(P.bin, -1.05, F, 2.9, 0);
// manager's office: shelves on the left wall, ledgers and a typewriter on the desk
put(P.bookshelf, 3.78, F, 5.2, 90);
put(P.notebook, 6.65, F + 0.91, 3.3, 0);
put(P.typewriter, 6.65, F + 0.91, 3.7, 90);
put(P.noticeH, 7.55, F + 1.5, 2.2, -90, 0.9);
// the pavement: a bench, a planter either side of the door, a bin
put(P.benchOut, 1.2, 0, 8.6, 180);
put(P.pot, -4.5, 0, 8.0, 0);
put(P.pot, -1.5, 0, 8.0, 0);
put(P.bin, 4.6, 0, 8.4, 0);

// Lights (point lights, CastShadow 0 written on each): the vanilla ceiling lamps are dead meshes. Two in the hall, one in
// the office, one over the door (kit.mjs) and the vault's fixture (an ambient and an up light): 6 in all, none casting.
glow(1.0, F + 2.9, 3.0, { lv: 7, radius: 7 });
glow(1.0, F + 2.9, 5.6, { lv: 5.5, radius: 5.5 });
glow(5.6, F + 2.9, 4.0, { lv: 6.5, radius: 6.5 });

// ---- kit children (Bank_Kit.et), building space ----
export const vault = { x: 9.95, y: 0.007, z: 4.4, yaw: 0, id: "bank_vault" };
export const kit = {
  alarmBox: { x: -0.4, y: 3.0, z: 7.42, yaw: 0 },
  sign: { x: -3.0, y: 4.1, z: 7.5, yaw: 0 },
  lamp: { x: -1.75, y: 3.3, z: 7.45, yaw: 0 },
  bag: [0, 0.49, -1.99], // in the vault's frame
  hood: [-0.1, 2.9, -0.344], // the vault's frame, scale 1.2
};
// NPCs stand in the layer, not in the prefab (the clerk precedent): building space x, z, yaw.
export const npcs = [
  { prefab: "{5B2A1E072C8DD6B7}Prefabs/Characters/Npcs/Outfits/LF_Npc_Teller_2.et", name: "Bank_Teller_1", x: 2.5, y: F, z: 5.0, yaw: 180 },
  { prefab: "{5B2A1E072C8DD6B7}Prefabs/Characters/Npcs/Outfits/LF_Npc_Teller_2.et", name: "Bank_Teller_2", x: 0.7, y: F, z: 5.0, yaw: 180 },
];
