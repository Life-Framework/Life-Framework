// Writes records.json and out.txt beside it: two shops as the placement audit would have measured them (docs/architecture/placement-system.md).
// Run `node agent-box/fixtures/place-lanes/build.mjs` after changing a row; place-lanes.mjs `selftest` reads the two files.
//   Shop_Crammed (x 0..5, z 0..4, 20 m2): shelves across the whole shop, the till and its counter behind them, three overlapping locker
//     banks, a chair in their fronts, a wardrobe through the east wall, a radio 0.2 m behind the till, and the till's pay action on a
//     mesh-less entity 1.2 m from the register (the clothing store, #3901). Fails lanes, reach and crowding.
//   Shop_Base: a base-map shop with no layer row (a `root` line carries the bounds) whose prefab children put a cabinet in the door swing.
//   Shop_Clear (x 20..28, z 0..6, 48 m2): two shelves on the back wall, a counter with a till, room to walk. Passes.
import { writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const dir = dirname(fileURLToPath(import.meta.url));
const records = [];
const out = [];
const f = (n) => String(Math.round(n * 1000) / 1000);
const v = (a) => a.map(f).join(",");

// One measured prop: c centre, h half extents (axis aligned), floor 0.
function prop(name, prefab, c, h, { actions = [], point = false, building = false } = {}) {
  const idx = records.length;
  records.push({ name, prefab, layer: "Fixture.layer", x: c[0], y: building ? 0 : c[1] - h[1], z: c[2], actions });
  const lo = c.map((x, i) => x - h[i]);
  const hi = c.map((x, i) => x + h[i]);
  out.push(`${idx} bmin=${v(lo)} bmax=${v(hi)} gap=0 tilt=0 walk=-1 pgap=0 clip=none scale=1 on=terrain over=0 drop=0 backed=0${point ? " point=1" : ""} c=${v(c)} h=${v(h)} a0=1,0,0 a1=0,1,0 a2=0,0,1 flags=none found=${name}`);
}
const door = (name, root, rp, c, h, bounds) => {
  out.push(`door name=${name} root=${root} rp=${v(rp)} c=${v(c)} h=${v(h)} a0=1,0,0 a1=0,1,0 a2=0,0,1`);
  if (bounds) out.push(`root name=${root} rp=${v(rp)} rmin=${v(bounds[0])} rmax=${v(bounds[1])}`);
};
const child = (name, root, rp, c, h) => out.push(`child name=${name} root=${root} rp=${v(rp)} c=${v(c)} h=${v(h)} a0=1,0,0 a1=0,1,0 a2=0,0,1`);

const BUILDING = "Prefabs/Structures/Commercial/Shop_01.et";
const SHELF = "Prefabs/Props/Commercial/Shelf_01.et";
const COUNTER = "Prefabs/Props/Commercial/CounterShop_01.et";
const TILL = "Prefabs/Props/Commercial/Till_01.et";
const MISC = "Prefabs/Props/Furniture/Misc_01.et";
const TILL_ACTION = ["LF_StoreTillAction"];

// ---- Shop_Crammed
prop("Shop_Crammed", BUILDING, [2.5, 1.5, 2], [2.5, 1.5, 2], { building: true });
for (const [n, x] of [["Shelf_Crammed_A", 0.8], ["Shelf_Crammed_B", 2.5], ["Shelf_Crammed_C", 4.2]]) prop(n, SHELF, [x, 0.9, 1.6], [0.85, 0.9, 0.25]);
prop("Counter_Crammed", COUNTER, [3.5, 0.45, 2.7], [0.6, 0.45, 0.3]);
prop("Till_Crammed", TILL, [3.5, 0.95, 2.7], [0.2, 0.1, 0.15], { actions: TILL_ACTION });
prop("Radio_Crammed", MISC, [3.5, 1.0, 3.15], [0.12, 0.1, 0.1], { actions: ["LF_RadioAction"] });
prop("Pay_Crammed", MISC, [3.9, 0.9, 0.2], [0, 0, 0], { actions: TILL_ACTION, point: true });
for (const [n, x] of [["Locker_Crammed_1", 1.0], ["Locker_Crammed_2", 1.25], ["Locker_Crammed_3", 1.5]]) prop(n, "Prefabs/Props/Furniture/Locker_01.et", [x, 1, 3.6], [0.7, 1, 0.3]);
prop("Chair_Crammed", "Prefabs/Props/Furniture/Chair_01.et", [1.3, 0.45, 3.1], [0.25, 0.45, 0.25]);
prop("Wardrobe_Crammed", "Prefabs/Props/Furniture/Wardrobe_01.et", [4.7, 1, 2.2], [0.5, 1, 0.4]);
door("Door_Crammed", "Shop_Crammed", [2.5, 0, 2], [2.5, 1.1, 0], [0.5, 1.1, 0.05]);

// ---- Shop_Clear
prop("Shop_Clear", BUILDING, [24, 1.5, 3], [4, 1.5, 3], { building: true });
prop("Shelf_Clear_A", SHELF, [21.6, 0.9, 5.5], [0.9, 0.9, 0.25]);
prop("Shelf_Clear_B", SHELF, [26.4, 0.9, 5.5], [0.9, 0.9, 0.25]);
prop("Counter_Clear", COUNTER, [24, 0.45, 3.6], [0.8, 0.45, 0.3]);
prop("Till_Clear", TILL, [24, 0.95, 3.6], [0.2, 0.1, 0.15], { actions: TILL_ACTION });
prop("Pay_Clear", MISC, [24, 1.0, 3.3], [0, 0, 0], { actions: TILL_ACTION, point: true });
door("Door_Clear", "Shop_Clear", [24, 0, 3], [24, 1.1, 0], [0.5, 1.1, 0.05]);

// ---- Shop_Base: a base-map shop (no layer row for the building; a `root` line carries its bounds, x 40..46, z 0..5). The pack's
// shelves run across it with the till behind them, and the building prefab brought a cabinet that stands in the door's swing.
for (const [n, x] of [["Shelf_Base_A", 41], ["Shelf_Base_B", 43], ["Shelf_Base_C", 45]]) prop(n, SHELF, [x, 0.9, 1.5], [1.1, 0.9, 0.25]);
prop("Counter_Base", COUNTER, [43, 0.45, 3], [0.8, 0.45, 0.3]);
prop("Till_Base", TILL, [43, 0.95, 3], [0.2, 0.1, 0.15], { actions: TILL_ACTION });
door("Door_Base", "Shop_Base", [43, 0, 2.5], [43, 1.1, 0], [0.5, 1.1, 0.05], [[40, 0, 0], [46, 3, 5]]);
child("Furniture_Cabinet", "Shop_Base", [43, 0, 2.5], [43, 0.5, 0.35], [0.4, 0.5, 0.4]);
child("Furniture_Shelf_Back", "Shop_Base", [43, 0, 2.5], [43, 0.5, 4.8], [0.4, 0.5, 0.2]);

writeFileSync(join(dir, "records.json"), JSON.stringify(records, null, 1) + "\n");
writeFileSync(join(dir, "out.txt"), out.join("\n") + "\n");
console.log(`place-lanes fixture: ${records.length} props, ${out.length - records.length} door and child line(s)`);
