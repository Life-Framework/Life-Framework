// Gun store shelf stock: ammunition boxes, holsters, magazines and optics on the host's shelves (see tools/town/gen-shop-shelves.mjs).
// Guns themselves stay on the shop's wall rack and display case (`lf-weapon-rack-*`, `gun-display-*`): static displays already.
export const goods = {
  AmmoBoxSmall: { file: "LF_ShelfGoods_AmmoBoxSmall.et", xob: "{14EA4D87418CBB67}Assets/Props/Military/AmmoBox/AmmoBox_545x39_1080rnd/AmmoBox_545x39_1080rnd.xob", w: 0.36, d: 0.15, yawOff: 90, lift: 0, scale: 0.85 },
  AmmoBoxBig: { file: "LF_ShelfGoods_AmmoBoxBig.et", xob: "{9F3EBDC04E344A36}Assets/Props/Military/AmmoBox/AmmoBox_556x45_1680rnd/AmmoBox_556x45_1680rnd.xob", w: 0.319, d: 0.349, lift: 0.002, scale: 0.9 },
  HolsterPM: { file: "LF_ShelfGoods_HolsterPM.et", xob: "{77C616A2A56451AF}Assets/Items/Equipment/Accessories/Holster_PM/Holster_PM.xob", w: 0.145, d: 0.061, lift: 0.095, scale: 1.2 },
  HolsterM12: { file: "LF_ShelfGoods_HolsterM12.et", xob: "{3D88A90E23197FCD}Assets/Items/Equipment/Accessories/Holster_M12/Holster_M12.xob", w: 0.107, d: 0.063, lift: 0.178, scale: 1.2 },
  MagPouch: { file: "LF_ShelfGoods_MagPouch.et", xob: "{A89C472519F50877}Assets/Items/Equipment/Accessories/Pouch_ALICE_20rnd_M14/Pouch_ALICE_20rnd_M14.xob", w: 0.103, d: 0.081, lift: 0, scale: 1.3 },
  Optic4x20: { file: "LF_ShelfGoods_Optic4x20.et", xob: "{FB0E7050A887C7F5}Assets/Weapons/Attachments/Optics/4x20/Optic_4x20.xob", w: 0.154, d: 0.046, yawOff: 90, lift: 0.05, scale: 1.4 },
  OpticArt: { file: "LF_ShelfGoods_OpticArt.et", xob: "{2E43837CC7A79F69}Assets/Weapons/Attachments/Optics/ARTII/Optic_ARTII.xob", w: 0.341, d: 0.073, yawOff: 90, lift: 0.016, scale: 0.9 },
  Binoculars: { file: "LF_ShelfGoods_Binoculars.et", xob: "{746CE09C894304E4}Assets/Items/Equipment/Binoculars/Binoculars_B12/Binoculars_B12.xob", w: 0.199, d: 0.169, lift: 0.046, scale: 1.2 },
  Flashlight: { file: "LF_ShelfGoods_Flashlight.et", xob: "{30664C42D09E1831}Assets/Items/Equipment/Flashlights/Flashlight_Soviet_01/Flashlight_Soviet_01.xob", w: 0.07, d: 0.042, lift: 0, scale: 1.4 },
  MagM14: { file: "LF_ShelfGoods_MagM14.et", xob: "{9FB96EBAC79195C6}Assets/Weapons/Magazines/M14/Magazine_M14_20rnd.xob", w: 0.089, d: 0.026, yawOff: 90, lift: 0.074, scale: 1.3 },
  MagVz58: { file: "LF_ShelfGoods_MagVz58.et", xob: "{6DA97AD816499726}Assets/Weapons/Magazines/VZ58/Magazine_30rnd_VZ58.xob", w: 0.151, d: 0.03, yawOff: 90, lift: 0.152, scale: 1.1 },
  MagPM: { file: "LF_ShelfGoods_MagPM.et", xob: "{2482E01261AB2FB5}Assets/Weapons/Magazines/PM/Magazine_8rnd_PM.xob", w: 0.043, d: 0.016, yawOff: 90, lift: 0.061, scale: 1.6 },
};

// The room's own gondola `ShelfShop_01` on the east wall (5.752, 0.199, 3.354, yaw -90) beside the host's wide units: boards 0.428, 0.820 and 1.177
// above its origin, the middle of a board 0.22 in front of the origin and 0.025 along it.
const g1 = { id: "g1", at: [5.532, 3.379], yaw: -90, len: 2.1, kind: "gondola", boardY: { 1: 0.629, 2: 1.021, 3: 1.378 } };

// The narrow `ShelfUniversal_01_Base` beside the wide unit behind the counter (3.296, 0.199, -3.003, yaw 0): same boards as the wide shelf
// (0.591, 1.088, 1.577), 0.9 m long, its middle 0.51 along from the origin and 0.213 in front.
const n1 = { id: "n1", at: [2.786, -2.79], yaw: 0, len: 0.85, kind: "narrow", boardY: { 1: 0.792, 2: 1.289, 3: 1.778 } };

// The host building's own drugstore shelving (the island shop-house keeps it: the same units the pharmacy kit dresses, tools/town/gen-pharmacy-shelves.mjs
// UNITS, kit frame), still bare behind the counter's side of the room: west wall x2, east wall and the narrow unit. The two that
// stand where this shop's own furniture prefab already has a unit (w4, w5, n1 there) are left out.
const WIDE = { 1: 0.789, 2: 1.282, 3: 1.775 };
const hostUnits = [
  { id: "h1", at: [-1.291, 3.927], yaw: 90, len: 1.22, kind: "wide", boardY: WIDE },
  { id: "h3", at: [-1.292, 5.381], yaw: 90, len: 1.22, kind: "wide", boardY: WIDE },
  { id: "h2", at: [5.536, -1.208], yaw: -90, len: 1.22, kind: "wide", boardY: WIDE },
  { id: "hn2", at: [5.547, -2.43], yaw: -90, len: 0.85, kind: "narrow", boardY: { 1: 0.792, 2: 1.289, 3: 1.778 } },
];

// The host's two double-sided gondola islands (ShelfShop_01 at 2.1, 5.4 and 2.1, 2.9: a pair back to back each, yaw 0 and 180, 0.47 m between
// their boards), bare until now and the first thing the door shows. Boards as g1 (same model, same floor); the board middle is 0.22 in front of
// the origin and 0.025 along it. The cell holds at most 150 entities (the audit's budget), so the islands carry two boards, spaced.
const BOARDS = { 1: 0.629, 2: 1.021, 3: 1.378 };
const islands = [
  { id: "i1", at: [2.124, 5.627], yaw: 0, len: 2.0, kind: "gondola", boardY: BOARDS },
  { id: "i2", at: [2.129, 5.153], yaw: 180, len: 2.0, kind: "gondola", boardY: BOARDS },
  { id: "i3", at: [2.124, 3.153], yaw: 0, len: 2.0, kind: "gondola", boardY: BOARDS },
  { id: "i4", at: [2.128, 2.679], yaw: 180, len: 2.0, kind: "gondola", boardY: BOARDS },
];

export const shop = {
  kit: "tools/town/kits/gun-counter.json",
  prefix: "gun-counter-host",
  furniture: "Prefabs/Buildings/GunStore/Furniture/GunStore_furniture_01.et",
  extraUnits: [g1, n1, ...hostUnits, ...islands],
  // Spread the wall units wider so the islands fit the same cell budget.
  tune: {
    i1: { gap: 0.3, boards: [1, 2] }, i2: { gap: 0.3, boards: [1, 2] }, i3: { gap: 0.3, boards: [1, 2] }, i4: { gap: 0.3, boards: [1, 2] },
    h1: { gap: 0.2 }, h3: { gap: 0.2 }, h2: { gap: 0.25 }, hn2: { gap: 0.3 }, w1: { gap: 0.2 }, w2: { gap: 0.2 }, g1: { gap: 0.25 },
  },
  // The shop's own furniture prefab stands none of its stock on the island host (it is the drugstore's), so nothing is in the way of w1 and w2.
  obstacles: [],
  cycle: ["AmmoBoxSmall", "HolsterPM", "MagM14", "Binoculars", "AmmoBoxBig", "HolsterM12", "MagVz58", "Optic4x20", "MagPouch", "OpticArt", "Flashlight", "MagPM"],
  only: { AmmoBoxBig: [1, 2], Binoculars: [1, 2, 3] },
};
// A spot the island audit refused (overlaps its neighbour by 4 cm).
shop.dropRows = ["gun-counter-host-g1-b2-9", "gun-counter-host-w1-b3-6", "gun-counter-host-w1-b3-5", "gun-counter-host-n1-b3-3", "gun-counter-host-n1-b1-4"];
// (the last four keep the cell under the audit's 150-entity budget)
