// The bank's layout in building space (Office_E_01's own frame: front faces +z, x along the facade, floor at y 1.053).
// One place to change a position; decor.mjs writes `decor` into the furniture prefab, kit.mjs writes `kit` and
// stage rows (`npcs`, the rob kit) go into the pack grid layer. Every prop is base-game, static and unphysical.
//
// Walls, read from the model (door and window sockets, floor and sill colliders; pass 2, 2026-11-17):
//   front wall   inner z 6.29, outer face z 6.58     back wall  inner z -4.62
//   east wall    inner x 7.72                        corridor   z -1.37..1.03, the length of the ground floor
//   stair hall   x -4.86..-2.65: the street door (y 0) and six steps up to the floor, z 3.18..4.68
//   hall         x -2.28..2.6, z 1.3..6.29 (door at x -1.41 on the corridor wall)
//   manager      x 2.75..7.72, z 1.3..6.29 (door at x 5.21)
//   vault room   x 2.75..7.72, z -4.62..-1.55 (door at x 3.92; high, small windows only); the vault door closes it
// Heights snap at write time (tools/buildings/pipeline/surface.mjs): a `y` here is a hint unless the row is wall-hung.
export const F = 1.053; // ground floor
export const F1 = 4.254; // first floor
export const P = {
  counter: "Prefabs/Props/Services/Post/CounterPostOffice_01_window.et",
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
  ledgers: "Prefabs/Props/Civilian/Notebook_01_Stack.et",
  pen: "Prefabs/Props/Civilian/Pen_01/Pen_01_base.et",
  typewriter: "Prefabs/Props/Civilian/Typewriter_01/Typewriter_01_Base.et",
  tape: "{CD9C68B74E4AB068}Prefabs/Props/Roadblocks/RallySafteyTape_01/RallySafetyTape_01_2m.et",
  insurance: "Prefabs/Props/Commercial/Posters/PosterSingle_A3_Insurance_V1.et",
  noticeV: "Prefabs/Props/Commercial/Posters/Notice_A4_Vertical_V3.et",
  noticeH: "Prefabs/Props/Commercial/Posters/Notice_A4_Horizontal_V3.et",
  noticeV2: "Prefabs/Props/Commercial/Posters/Notice_A4_Vertical_V6.et",
  radioPoster: "Prefabs/Props/Commercial/Posters/PosterSingle_27x41_01_RadioEveron.et",
  frame: "Prefabs/Props/Civilian/TablePhotoFrame_01/TablePhotoFrame_01_V3.et",
  paintingBig: "Prefabs/Props/Decorations/Paintings/PaintingBig_01_V4.et",
  painting2: "Prefabs/Props/Decorations/Paintings/PaintingBig_02_V3.et",
  bin: "Prefabs/Props/Garbage/Bins/BinInterior_01_green.et",
  bookshelf: "Prefabs/Props/Furniture/Bookshelf_01/Bookshelf_01.et",
  benchOut: "Prefabs/Props/Furniture/BenchWooden_03.et",
  pot: "Prefabs/Props/Decorations/Flowerpots/FlowerpotStreet_01/FlowerpotStreet_01_rect_long_begonia_red.et",
  corkBoard: "Prefabs/Props/Civilian/NoticeBoardCork_01.et",
  extinguisher: "Prefabs/Props/Civilian/FireExtinguisher_01.et",
  fan: "Prefabs/Props/Civilian/Fan_01.et",
  ashtray: "Prefabs/Props/Civilian/Ashtray_01_Full_clear.et",
  box: "Prefabs/Props/Crates/CardboardBox/CardboardBox_03.et",
  coatRack: "Prefabs/Props/Furniture/CoatRack_01.et",
  radio: "Prefabs/Props/Civilian/RadioSmall_01_black.et",
  deskLamp: "Prefabs/Props/Furniture/LightTable_01.et",
  depositWall: "{5793F82B50123A5E}Prefabs/Props/Roleplay/BankVault/LF_BankVault_DepositWall.et",
  trolley: "{3C103D64C3AF2262}Prefabs/Props/Roleplay/BankVault/LF_BankVault_CashTrolley.et",
  wallLamp: "Prefabs/Props/Furniture/LightWall_01.et",
};

// ---- static decor, written into Furniture/Bank_furniture_01.et ----
// items: { id?, path, x, y, z, yaw?, scale?, on?, lit? }   lights: { x, y, z, color?, lv?, radius? }
export const decor = { items: [], lights: [] };
const put = (path, x, y, z, yaw = 0, o = {}) => decor.items.push({ path, x, y, z, yaw, scale: 1, ...o });
const glow = (x, y, z, o = {}) => decor.lights.push({ x, y, z, ...o });

// ---- Banking hall: a counter run across the room (the tellers on the window side), the public on the corridor side ----
put(P.carpet, 0.3, F, 2.9, 0);
put(P.counter, 2.45, F, 4.3, 180, { id: "c1" }); // a counter runs 1.508 m to -x of its origin at yaw 180
put(P.counter, 0.94, F, 4.3, 180, { id: "c2" });
put(P.counter, -0.57, F, 4.3, 180, { id: "c3" });
// on the counters: a till and a ledger at each window, the phone, a typewriter, a framed photo
put(P.cashier, 2.1, F + 1, 4.55, 180, { on: "c1" });
put(P.phone, 1.3, F + 1, 4.6, 180, { on: "c1" });
put(P.cashier, 0.6, F + 1, 4.55, 180, { on: "c2" });
put(P.notebook, 0.1, F + 1, 4.64, 170, { on: "c2" });
put(P.pen, -0.05, F + 1, 4.64, 100, { on: "c2" });
put(P.typewriter, -1.0, F + 1, 4.62, 180, { on: "c3" });
put(P.frame, -1.8, F + 1, 4.6, 180, { on: "c3" });
// the teller side: a chair behind each window, a cabinet, ledgers on it
put(P.chairOffice, 1.7, F, 5.3, 190);
put(P.chairOffice, 0.1, F, 5.3, 170);
put(P.cabinet, -1.85, F, 5.85, 90, { id: "tcab" });
put(P.ledgers, -1.85, F + 2, 5.85, 80, { on: "tcab" });
// the public side: deposit-slip desk with slips, a pen and the rates notice, the queue tape, a bench and a plant
put(P.tableSmall, 2.1, F, 2.0, 0, { id: "slip" });
put(P.notebook, 2.05, F + 0.7, 2.0, 20, { on: "slip" });
put(P.pen, 2.2, F + 0.7, 2.1, 70, { on: "slip" });
put(P.ashtray, 1.95, F + 0.7, 1.8, 0, { on: "slip" });
put(P.tape, -0.6, F, 3.55, 0);
put(P.bench, -1.95, F, 2.55, 90);
put(P.plant, 2.2, F, 3.4, 0);
put(P.bin, -1.95, F, 1.6, 0);
// walls: the clock and rates board on the corridor wall, notices, a map of the island and pictures on the side walls
// (the hall door is at x -1.41..-0.5 on that wall, so everything hangs east of it)
put(P.clock, 0.0, F + 2.0, 1.4, 0);
put(P.corkBoard, 1.25, F + 0.75, 1.42, 0); // the rates board, its notices pinned on
put(P.noticeH, 0.85, F + 1.35, 1.43, 0);
put(P.noticeV, 1.3, F + 1.3, 1.43, 0);
put(P.noticeV2, 1.7, F + 1.35, 1.43, 0);
put(P.insurance, 2.25, F + 1.5, 1.4, 0, { scale: 0.9 });
put(P.radioPoster, -2.2, F + 1.5, 3.0, 90);
put(P.noticeV, -2.2, F + 1.5, 2.2, 90, { scale: 0.9 });
put(P.painting2, 2.42, F + 1.55, 2.9, -90); // (WallMap_01 renders as a blank roll-down screen, so a picture hangs here)
put(P.paintingBig, 2.42, F + 1.8, 5.4, -90);
put(P.noticeV2, 2.42, F + 1.45, 1.8, -90, { scale: 0.9 });

// ---- Manager's office ----
put(P.carpet, 5.3, F, 4.1, 0);
put(P.desk, 6.75, F, 4.45, -90, { id: "mdesk" }); // a desk runs 1.7 m from its origin: at yaw -90, x 6.0..6.75, z 2.75..4.45
put(P.chairOffice, 7.25, F, 3.6, -90);
put(P.chair, 5.7, F, 3.1, 90);
put(P.chair, 5.7, F, 4.1, 90);
put(P.safe, 3.4, F, 5.75, 180);
put(P.cabinet, 7.35, F, 5.9, -90);
put(P.bookshelf, 3.1, F, 3.7, 90);
put(P.plant, 3.2, F, 1.75, 0);
put(P.coatRack, 7.35, F, 1.75, 0);
put(P.phone, 6.5, F + 1, 3.0, 90, { on: "mdesk" });
put(P.typewriter, 6.4, F + 1, 3.6, 90, { on: "mdesk" });
put(P.ledgers, 6.35, F + 1, 4.1, 0, { on: "mdesk" });
put(P.frame, 6.62, F + 1, 4.3, 250, { on: "mdesk" });
put(P.ashtray, 6.15, F + 1, 4.3, 0, { on: "mdesk" });
put(P.painting2, 7.7, F + 1.7, 3.6, -90);
put(P.noticeH, 4.6, F + 1.6, 1.4, 0, { scale: 0.9 });
put(P.insurance, 7.7, F + 1.5, 2.3, -90, { scale: 0.9 });

// ---- Vault room: safe-deposit walls down both long sides, the cash trolley at the back, a counting table ----
put(P.depositWall, 4.4, F, -4.6, 0);
put(P.depositWall, 7.08, F, -4.6, 0);
put(P.depositWall, 7.7, F, -2.85, -90, { scale: 0.85 });
put(P.trolley, 3.25, F, -3.6, 0);
put(P.tableSmall, 5.6, F, -2.9, 0, { id: "count" });
put(P.ledgers, 5.55, F + 0.7, -2.9, 10, { on: "count" });
put(P.pen, 5.75, F + 0.7, -2.8, 60, { on: "count" });

// ---- The corridor by the vault door: a fire extinguisher, the strongroom notice, a chair for the guard ----
put(P.extinguisher, 2.4, F, -1.2, 0);
put(P.noticeV, 5.6, F + 1.6, -1.3, 0, { scale: 0.9 });
put(P.chair, 6.8, F, 0.6, 180);
put(P.ashtray, 7.3, F, 0.75, 0);

// ---- Stair hall (street door, steps up to the floor): a notice board, opening hours, a plant at the top ----
put(P.corkBoard, -2.6, F + 0.6, 2.2, -90);
put(P.noticeV, -2.61, F + 1.2, 2.5, -90);
put(P.noticeH, -2.61, F + 1.25, 1.95, -90);
put(P.noticeV2, -4.8, F + 1.4, 2.3, 90, { scale: 0.9 });
put(P.plant, -4.45, F, 1.65, 0);
put(P.bin, -3.0, F, 1.6, 0);

// ---- First floor: the staff offices kept, with the work of a bank on the desks ----
// (vanilla desks stay; the props go on them by entity ID: the furniture prefab's own rows)
put(P.typewriter, 3.45, F1 + 1, 5.3, 90, { on: "515ED7A4C70097D6" });
put(P.ledgers, 3.4, F1 + 1, 5.75, 0, { on: "515ED7A4C70097D6" });
put(P.deskLamp, 4.3, F1 + 1, 4.5, 90, { on: "515ED7A4C70097E1" });
put(P.notebook, 4.2, F1 + 1, 5.2, 80, { on: "515ED7A4C70097E1" });
put(P.typewriter, -0.45, F1 + 1, 5.6, 90, { on: "515ED7A4C7009AFE" });
put(P.ledgers, -0.4, F1 + 1, 4.7, 0, { on: "515ED7A4C7009AFE" });
put(P.deskLamp, 0.6, F1 + 1, 4.4, 180, { on: "515ED7A4C70096B5" });
put(P.phone, 0.6, F1 + 1, 5.3, -90, { on: "515ED7A4C70096B5" });
put(P.deskLamp, -4.6, F1 + 1, 3.6, 0, { on: "515ED7A4C7009AA9" });
put(P.ledgers, -3.6, F1 + 1, 4.35, 20, { on: "515ED7A4C7009A82" });
put(P.radio, -3.35, F1 + 1, 4.4, 200, { on: "515ED7A4C7009A82" });
put(P.box, -3.9, F1, -3.7, 15, { id: "box1" });
put(P.box, -3.85, F1 + 0.4, -3.7, 40, { on: "box1" });
put(P.box, 0.4, F1, -4.1, 0);
put(P.fan, 7.1, F1 + 1, 5.4, 200, { on: "515ED7A4C7009476" });
put(P.paintingBig, 1.0, F1 + 1.6, 1.42, 0);
put(P.corkBoard, 5.0, F1 + 0.6, 1.42, 0);
put(P.noticeH, 4.7, F1 + 1.2, 1.43, 0);

// ---- The pavement: a bench, a planter either side of the door, a bin ----
put(P.benchOut, 1.2, 0, 7.6, 180);
put(P.pot, -5.6, 0, 7.15, 0);
put(P.pot, -1.7, 0, 7.15, 0);
put(P.bin, 3.6, 0, 7.2, 0);

// ---- Lights: five point lights here and the door lamp in the kit, 6 of the medium budget's 6, none casting a shadow.
// The vanilla wall lamps (and the corridor's) glow through their lit glass (shell.mjs), which costs no light.
glow(0.2, F + 2.75, 3.8, { lv: 7, radius: 7.5 }); // the banking hall
glow(5.3, F + 2.75, 3.9, { lv: 6, radius: 6.5 }); // the manager's office
glow(3.9, F + 2.65, 0.0, { lv: 5, radius: 6, color: "1 0.9 0.74 1" }); // the corridor: the face of the vault door
glow(5.3, F + 2.6, -3.1, { lv: 4.5, radius: 5, color: "0.92 0.95 1 1" }); // inside the vault room
glow(-3.75, F + 2.6, 2.6, { lv: 5, radius: 6.5 }); // the stair hall and the street door

// ---- kit children (Bank_Kit.et), building space ----
// The vault: LF_BankVault_RoomDoor (bank_room) on the corridor face of the vault room's doorway. Its origin is the
// front's centre and the door hinges 1.271 m to its right, so x 3.785 centres the door on the doorway (x 3.92); the
// door's back sinks into the wall. The leaf swings into the corridor; 55 degrees keeps it off the far wall.
export const vault = { x: 3.785, y: F, z: -1.02, yaw: 0, id: "bank_vault", swing: 55 };
export const kit = {
  alarmBox: { x: 0.07, y: 3.0, z: 6.58, yaw: 0 }, // the pier between the hall's two windows
  sign: { x: -3.4, y: 3.95, z: 6.61, yaw: 0 }, // over the street door, above the stair-hall window
  lamp: { x: -2.42, y: 2.55, z: 6.58, yaw: 0 }, // the door lamp, right of the street door
  bag: [5.2, F, -3.75], // the cash bag on the vault room floor
};
// NPCs stand in the layer, not in the prefab (the clerk precedent): building space x, z, yaw.
export const npcs = [
  { prefab: "{5B2A1E072C8DD6B7}Prefabs/Characters/Npcs/Outfits/LF_Npc_Teller_2.et", name: "Bank_Teller_1", x: 1.7, y: F, z: 5.0, yaw: 180 },
  { prefab: "{5B2A1E072C8DD6B7}Prefabs/Characters/Npcs/Outfits/LF_Npc_Teller_2.et", name: "Bank_Teller_2", x: 0.1, y: F, z: 5.0, yaw: 180 },
];
// The rob kit chest (dev gate): in the corridor just west of the vault door; the robbery proof stands beside it (its
// north side is building +x, towards the door) so the rob action it uses is this bank's, not the grid's other vaults.
export const robKit = { x: 2.0, z: 0.3, yaw: 0 };
