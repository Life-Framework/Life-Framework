// tools/validation/shelf-stock-selftest.mjs
// inputs: tools/town/shelf-stock.mjs, tools/buildings/pipeline/store-slots.mjs
//
// Proves tools/town/shelf-stock.mjs (#3516) on an invented furniture dump and invented measurements: no game files.
//   - instancesOf reads the single and the `$grp` entities of a vanilla furniture prefab, with `angles` or `angleY`;
//   - planShelves stands one item per point directly over a board (a point is HINT above the board, never a pile),
//     puts each line on its own shelf unit, never more points than boards x perBoard, and skips an item the board cannot hold;
//   - a unit that faces the other way (yaw 180) gets points on its own side;
//   - the prefab text carries the store key, the stock id for prices, one line per item and the boundary.
// red-proof: 2026-11-24 with the gondola units dropped from the plan this prints "SUMMARY shelf-stock-selftest fail 2"; restored, ok. Earlier: 2026-11-19 with planShelves given `rows: 2` this printed "SUMMARY shelf-stock-selftest fail 3" (first line "FAIL a line has more than one row of copies"); restored, it prints ok.
import { shelvesPrefab, instancesOf, planShelves, sortUnits, stockItems } from "../town/shelf-stock.mjs";

let failed = 0;
const ok = (c, m) => {
  if (!c) {
    failed++;
    console.log(`FAIL  ${m}`);
  }
};

const dump = `GenericEntity : "{28411B0C79460DF8}Prefabs/Structures/Core/BuildingFurniture_base.et" {
 ID "56A8CBB92342240A"
 {
  $grp GenericEntity : "{D6E93EF9D3232D16}Prefabs/Props/Commercial/ShelfUniversal_01_wide.et" {
   {
    ID "1"
    coords 4.17 0 -3.472
    angles 0 0 0
   }
   {
    ID "2"
    coords 4.151 0 -3.473
    angles 0 180 0
   }
   {
    ID "3"
    coords 12.257 0 3.349
    angles 0 -90 0
   }
  }
  GenericEntity : "{D6E93EF9D3232D16}Prefabs/Props/Commercial/ShelfUniversal_01_wide.et" {
   ID "4"
   coords 1 0 -3.47
   angleY 180
  }
  GenericEntity : "{B7669B7F8B7923F6}Prefabs/Props/Commercial/ShelfShop_01.et" {
   ID "5"
   coords 9 0 9
  }
 }
}`;
const units = instancesOf(dump, "Prefabs/Props/Commercial/ShelfUniversal_01_wide.et");
ok(units.length === 4, `instancesOf found ${units.length} shelf units, the dump has 4 (3 in a $grp, 1 single; the other prefab is not one)`);
ok(units.some((u) => u.yaw === 180 && u.at[0] === 4.151), "a $grp block's yaw (angles 0 180 0) was lost");
ok(units.some((u) => u.yaw === 180 && u.at[0] === 1), "a single entity's angleY was lost");

const boards = [0.112, 0.588, 1.081, 1.574, 2.067, 2.592].map((y) => ({ y, x: [-1.383, -0.06], z: [0.026, 0.4], clear: 0.49 }));
const small = { size: [0.083, 0.259, 0.081], center: [0, 0] };
const huge = { size: [0.42, 0.19, 0.46], center: [0, 0] };
const items = { "Prefabs/Items/A.et": small, "Prefabs/Items/B.et": small, "Prefabs/Items/Big.et": huge, "Prefabs/Items/C.et": small };
const refs = ["{0000000000000001}Prefabs/Items/A.et", "{0000000000000002}Prefabs/Items/Big.et", "{0000000000000003}Prefabs/Items/B.et", "{0000000000000004}Prefabs/Items/C.et"];
const plan = planShelves({ units, items, itemRefs: refs, boards, boardIdx: [1, 2, 3], perBoard: 8, gondolaZ: -3.47 });
ok(plan.problems.length === 0, `planShelves problems: ${plan.problems.join("; ")}`);
ok(plan.skipped.length === 1 && /Big/.test(plan.skipped[0].item), "the item wider than the board was not skipped, or something else was");
// the gondola run holds the three units at z -3.47 (4.17, 4.151, 1): A, B and C each take one, so three lines
ok(plan.lines.length === 3, `${plan.lines.length} lines, the gondola run has 3 units for 3 fitting items`);
const sortedGondola = sortUnits(units.filter((u) => Math.abs(u.at[2] + 3.47) < 0.1));
ok(sortedGondola.length === 3, "the gondola filter kept the wall unit at z 3.349");
for (const line of plan.lines) {
  ok(line.slots.length <= 3 * 8, `a line has ${line.slots.length} points, at most 3 boards x 8`);
  ok(line.slots.length === line.max, "a line's stock is not one copy per point");
  const ys = new Set(line.slots.map((p) => p[1]));
  ok(ys.size === 3, `a line stands on ${ys.size} boards, not the 3 asked for`);
  // one row: every point of one board shares one z
  const perBoard = new Map();
  for (const p of line.slots) (perBoard.get(p[1]) ?? perBoard.set(p[1], new Set()).get(p[1])).add(p[2]);
  ok([...perBoard.values()].every((zs) => zs.size === 1), "a line has more than one row of copies");
  // no two copies share a point: nothing is stacked
  ok(new Set(line.slots.map((p) => p.join(" "))).size === line.slots.length, "two copies share one point (a pile)");
  for (const p of line.slots) ok(boards.some((b) => p[1] > b.y && p[1] < b.y + 0.06), `a point at y ${p[1]} is not just over a board`);
}
// a facing-away unit (yaw 180) puts its goods on its own front: z below its own z, the yaw 0 unit's above
ok(plan.lines.some((l) => l.slots.every((p) => p[2] > -3.47)) && plan.lines.some((l) => l.slots.every((p) => p[2] < -3.47)), "both faces of the gondola run carry goods");

// A second fixture type (the sloped gondolas): its units take the fitting items in turn, the points join that item's line, never a pile.
const gBoards = [0.1, 0.43, 0.82, 1.18].map((y) => ({ y, x: [-1.2, 1.2], z: [0.05, 0.45], clear: 0.35 }));
const gUnits = [0, 1, 2, 3, 4].map((i) => ({ at: [9 + i * 2.5, 0, 9], yaw: 90 }));
const gPlan = planShelves({ units, items, itemRefs: refs, boards, boardIdx: [1, 2, 3], perBoard: 8, gondolaZ: -3.47, gondola: { units: gUnits, boards: gBoards, boardIdx: [1, 2, 3], perBoard: 6 } });
ok(gPlan.gondolaUnits === 5, `gondola units stocked: ${gPlan.gondolaUnits}, wanted 5`);
ok(gPlan.lines.length === 3, `${gPlan.lines.length} lines, the gondolas add points to the existing items, not new lines for the same item`);
ok(gPlan.lines.every((l) => l.max === l.slots.length), "a gondola line's stock is not one copy per point");
ok(new Set(gPlan.lines.flatMap((l) => l.slots.map((p) => p.join(" ")))).size === gPlan.lines.reduce((n, l) => n + l.slots.length, 0), "two copies share one point on the gondolas");
ok(gPlan.lines.reduce((n, l) => n + l.slots.length, 0) > plan.lines.reduce((n, l) => n + l.slots.length, 0), "the gondolas added no points");

const text = shelvesPrefab({ storeKey: "k_test", stockId: "stock_test", restockSeconds: 90 }, plan, { center: [1, 0, -3], half: [12, 0, 9] });
ok(/m_sStoreKey "k_test"/.test(text), "the store key is not in the prefab");
ok(/m_sStockId "stock_test"/.test(text), "the stock id (the prices) is not in the prefab");
ok((text.match(/LF_StoreLine /g) ?? []).length === plan.lines.length, "the prefab has not one line per planned line");
ok(/m_vBoundaryHalf 12 0 9/.test(text) && /m_iRestockSeconds 90/.test(text), "the boundary or the restock interval is wrong");
ok(shelvesPrefab({ storeKey: "k_test", stockId: "s", restockSeconds: 1 }, plan, { center: [0, 0, 0], half: [1, 0, 1] }).match(/LF_StoreLine "(\{[0-9A-F]{16}\})"/)?.[1] === text.match(/LF_StoreLine "(\{[0-9A-F]{16}\})"/)?.[1], "component GUIDs changed between two runs");
ok(!/m_iStack/.test(text), "the prefab asks for piles");

const conf = `LF_ShopStock "{1}" {\n m_sId "other"\n m_aItems {\n  LF_ShopStockItem "{2}" {\n   m_Prefab "{AAAAAAAAAAAAAAAA}Prefabs/Items/X.et"\n  }\n }\n}\nLF_ShopStock "{3}" {\n m_sId "mine"\n m_aItems {\n  LF_ShopStockItem "{4}" {\n   m_Prefab "{BBBBBBBBBBBBBBBB}Prefabs/Items/Y.et"\n   m_iPrice 4\n  }\n  LF_ShopStockItem "{5}" {\n   m_Prefab "{CCCCCCCCCCCCCCCC}Prefabs/Items/Z.et"\n  }\n }\n}\nLF_ShopStock "{6}" {\n m_sId "later"\n m_aItems {\n  LF_ShopStockItem "{7}" {\n   m_Prefab "{DDDDDDDDDDDDDDDD}Prefabs/Items/W.et"\n  }\n }\n}\n`;
const got = stockItems(conf, "mine");
ok(got.length === 2 && /Y\.et$/.test(got[0]) && /Z\.et$/.test(got[1]), `stockItems read ${JSON.stringify(got)}, wanted Y and Z only (not the stocks before or after)`);

console.log(failed ? `SUMMARY shelf-stock-selftest fail ${failed}` : "SUMMARY shelf-stock-selftest ok");
process.exit(failed ? 1 : 0);
