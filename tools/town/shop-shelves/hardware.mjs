// Hardware store shelf stock: tools, paint and building goods on the room's own gondola (see tools/town/gen-shop-shelves.mjs).
export const goods = {
  PaintCan: { file: "LF_ShelfGoods_PaintCan.et", xob: "{F552AD57783AD277}Assets/Props/Civilian/PaintCan_01/PaintCan_01.xob", w: 0.302, d: 0.304, lift: 0.001, scale: 0.75 },
  Bucket: { file: "LF_ShelfGoods_Bucket.et", xob: "{8D4E2E836BBABBDD}Assets/Props/Civilian/Bucket_01/Bucket_01.xob", w: 0.428, d: 0.414, lift: 0.001, scale: 0.75 },
  ToolBox: { file: "LF_ShelfGoods_ToolBox.et", xob: "{DBEAA3CD4B3B08DF}Assets/Props/Industrial/ToolBox_01/ToolBox_01.xob", w: 0.551, d: 0.236, lift: 0, scale: 0.75 },
  EntrenchTool: { file: "LF_ShelfGoods_EntrenchTool.et", xob: "{E1972FBA9F5D4BBB}Assets/Items/Equipment/Accessories/ETool_ALICE/ETool_ALICE.xob", w: 0.553, d: 0.157, yawOff: 90, lift: 0.027, scale: 0.75 },
  Brick: { file: "LF_ShelfGoods_Brick.et", xob: "{2A301FAA4ED493E3}Assets/Props/Debris/Brick/Brick_dbr_02.xob", w: 0.293, d: 0.135, lift: 0.035, scale: 1 },
  Flashlight: { file: "LF_ShelfGoods_Flashlight.et", xob: "{30664C42D09E1831}Assets/Items/Equipment/Flashlights/Flashlight_Soviet_01/Flashlight_Soviet_01.xob", w: 0.07, d: 0.042, lift: 0, scale: 1.4 },
  CementBag: { file: "LF_ShelfGoods_CementBag.et", xob: "{645CDD79A583B1D5}Assets/Items/Resources/CementBag/CementBag.xob", w: 0.7, d: 0.463, yawOff: 90, lift: 0.09, scale: 0.75 },
  CardboardBox: { file: "LF_ShelfGoods_CardboardBox.et", xob: "{2C2903391E74AAC9}Assets/Props/Crates/CardboardBox_01.xob", w: 0.398, d: 0.666, yawOff: 90, lift: 0, scale: 0.75 },
};

// The kit's own gondola `hardware-shelf-1` (ShelfShop_01 at 3.285, 0.454, -2, yaw -90). Its boards are the model's measured tops
// (0.428, 0.820, 1.177) above the unit origin; the middle of a board is 0.22 in front of the origin and 0.025 along it.
const g1 = { id: "g1", at: [3.065, -1.975], yaw: -90, len: 2.1, kind: "gondola", boardY: { 1: 0.884, 2: 1.276, 3: 1.633 } };

export const shop = {
  kit: "tools/town/kits/hardware.json",
  prefix: "hardware-host",
  units: [g1],
  obstacles: [],
  cycle: ["PaintCan", "Brick", "ToolBox", "Flashlight", "EntrenchTool", "Bucket", "PaintCan", "CementBag", "Flashlight", "CardboardBox"],
  only: { Bucket: [1, 2], CementBag: [1], CardboardBox: [1] },
};
