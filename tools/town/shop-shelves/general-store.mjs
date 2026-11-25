// General store shelf stock: groceries and household goods (see tools/town/gen-shop-shelves.mjs).
export const goods = {
  CoffeeTin: { file: "LF_ShelfGoods_CoffeeTin.et", xob: "{55A5285C23AB92DE}Assets/Items/Store1989/CoffeeTin/CoffeeTin.xob", w: 0.09, d: 0.09, lift: 0.055, scale: 1.3 },
  PickleJar: { file: "LF_ShelfGoods_PickleJar.et", xob: "{849C48833EFCFDD4}Assets/Items/Store1989/PickleJar/PickleJar.xob", w: 0.087, d: 0.089, lift: 0.072, scale: 1.3 },
  MilkBottle: { file: "LF_ShelfGoods_MilkBottle.et", xob: "{6383289C35D3CECE}Assets/Items/Store1989/MilkBottle/MilkBottle.xob", w: 0.064, d: 0.064, lift: 0.083, scale: 1.3 },
  CannedMeat: { file: "LF_ShelfGoods_CannedMeat.et", xob: "{4A6646A74DC7B489}Assets/Items/Store1989/CannedMeat/CannedMeat.xob", w: 0.1, d: 0.064, yawOff: 90, lift: 0.026, scale: 1.3 },
  CannedFish: { file: "LF_ShelfGoods_CannedFish.et", xob: "{EF54929D5051A697}Assets/Items/Store1989/CannedFish/CannedFish.xob", w: 0.105, d: 0.07, yawOff: 90, lift: 0.016, scale: 1.4 },
  Crackers: { file: "LF_ShelfGoods_Crackers.et", xob: "{A47ED22E6647430A}Assets/Items/Food/ArmyCrackers_Soviet_01.xob", w: 0.171, d: 0.088, lift: 0.003, scale: 1.2 },
  Chocolate: { file: "LF_ShelfGoods_Chocolate.et", xob: "{52F9D510E620CE32}Assets/Items/Store1989/ChocolateBar/ChocolateBar.xob", w: 0.155, d: 0.071, yawOff: 90, lift: 0.006, scale: 1.4 },
  SodaBottle: { file: "LF_ShelfGoods_SodaBottle.et", xob: "{1B98D6F1E0135F12}Assets/Props/Civilian/SodaBottle_01/SodaBottle_01.xob", w: 0.181, d: 0.137, lift: 0, scale: 0.85 },
  WaterBottle: { file: "LF_ShelfGoods_WaterBottle.et", xob: "{1F941A15AF4D93BF}Assets/Items/Drinks/WaterBottle/Bottle.xob", w: 0.063, d: 0.063, lift: 0, scale: 1.2 },
  BeerBottle: { file: "LF_ShelfGoods_BeerBottle.et", xob: "{240D853DBB045D05}Assets/Props/Civilian/BeerBottle_01/BeerBottle_01.xob", w: 0.083, d: 0.081, lift: 0.001, scale: 1 },
  TinCan: { file: "LF_ShelfGoods_TinCan.et", xob: "{FACB1ED350C087F9}Assets/Props/Garbage/Generic/LitterInterior_01/LitterInterior_01_TinCan_01.xob", w: 0.1, d: 0.1, lift: 0, scale: 1.3 },
  TinCanB: { file: "LF_ShelfGoods_TinCanB.et", xob: "{63D5A74A4D27B669}Assets/Props/Garbage/Generic/LitterInterior_01/LitterInterior_01_TinCan_02.xob", w: 0.099, d: 0.099, lift: 0, scale: 1.3 },
  Cigarettes: { file: "LF_ShelfGoods_Cigarettes.et", xob: "{10D42E6612680C88}Assets/Props/Garbage/Generic/LitterInterior_01/LitterInterior_01_Cigarettes_01.xob", w: 0.084, d: 0.075, lift: 0, scale: 1.5 },
  PaintCan: { file: "LF_ShelfGoods_PaintCan.et", xob: "{F552AD57783AD277}Assets/Props/Civilian/PaintCan_01/PaintCan_01.xob", w: 0.302, d: 0.304, lift: 0.001, scale: 0.7 },
  Bucket: { file: "LF_ShelfGoods_Bucket.et", xob: "{8D4E2E836BBABBDD}Assets/Props/Civilian/Bucket_01/Bucket_01.xob", w: 0.428, d: 0.414, lift: 0.001, scale: 0.75 },
};

export const shop = {
  kit: "tools/town/kits/general-store.json",
  prefix: "general-store-host",
  furniture: "Prefabs/Buildings/GeneralStore/Furniture/GeneralStore_furniture_01.et",
  // Groceries and household goods, in a cycle. A board starts at a different place in it so neighbours differ.
  cycle: ["CoffeeTin", "TinCan", "PickleJar", "MilkBottle", "Crackers", "CannedMeat", "SodaBottle", "TinCanB", "CannedFish", "WaterBottle", "Chocolate", "Cigarettes", "BeerBottle", "PaintCan", "Bucket"],
  // Tall goods stay off the top board's low neighbour: none here; these keep to the middle board and below.
  only: { Bucket: [1], PaintCan: [1], SodaBottle: [1, 2, 3] },
  // What the island audit refused after the first stamp: w4 (east wall by the stair corner: its boards read 2 to 4 cm off, spots
  // sat in the next one's goods) and four spots whose support varies under them.
  dropUnits: ["w4"],
  dropRows: ["general-store-host-w2-b1-5", "general-store-host-w2-b2-4", "general-store-host-w2-b2-5", "general-store-host-w3-b1-3"],
};
