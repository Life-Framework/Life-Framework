// tools/town/vegetation.selftest.mjs - acceptance for #4724 (`town vegetation`: the island's trees and bushes in an open slot's clearing).
//
// The island's forest is `Tree`-class base-map entities with prefabs, so the box-mode clear hides them like a vanilla sign. This pins the pure
// parts of the step: the clearing's boxes in the kit's frame, which scanned entities fall in them, the boxed `clear` entries written into the
// kit (and that a second write replaces its own entries and leaves a door or a sign entry alone), and that `town fit` turns them into carriers
// that reach as far below the ground as asked. Run: node tools/town/vegetation.selftest.mjs
// red-proof: with isVegetation accepting every class the "camp box takes the two" case printed FAIL (the wrong-class entity came with it); with clearingBoxes ignoring the margin the
// "camp box: the footprint plus the margin" case printed FAIL; with clearBoxesOf dropping `below` the "the carrier reaches 4 m below" case printed FAIL (2026-11-24).
import { dirname, resolve } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";

let failed = 0;
const check = (cond, msg) => {
  if (!cond) {
    console.log(`FAIL  ${msg}`);
    failed++;
  }
};
const same = (a, b) => JSON.stringify(a) === JSON.stringify(b);

// The `town` dispatcher imports every tools/town/*.mjs to list its steps: run only when launched directly.
if (process.argv[1] && pathToFileURL(resolve(process.argv[1])).href === import.meta.url) {
  const here = dirname(fileURLToPath(import.meta.url));
  const veg = await import(pathToFileURL(resolve(here, "vegetation.mjs")).href);
  const fit = await import(pathToFileURL(resolve(here, "fit.mjs")).href);

  // A camp kit: footprint x -10..10, z 0..20 (kit frame), the gate rows on its -z side.
  const kit = {
    kit: "demo-camp",
    anchoring: { footprint: { min: [-10, 0], max: [10, 20] } },
    rows: [
      { id: "camp-gate-sandbag-w", offset: [-4, 0, 3] },
      { id: "camp-gate-sandbag-e", offset: [4, 0, 3] },
      { id: "camp-fire", offset: [0, 0, 10] },
    ],
  };
  const anchor = { x: 1000, z: 2000, yaw: 0 };

  // ---- the line format of the audit's `scan`
  const pine = "scan prefab={0572596E23DDDC10}PrefabLibrary/DefaultLibrary/Vegetation/Trees/t_pinus_sylvestris_2f.et name=t_pinus_sylvestris_2f p=1000,5,2010 top=1 class=Tree mesh={932DB6C25725E684}Assets/Vegetation/Tree/x.xob vis=1";
  const e = veg.parseScanLine(pine);
  check(e && e.cls === "Tree" && e.x === 1000 && e.z === 2010 && e.vis === 1, `parse: a scan line gives class, position and vis, got ${JSON.stringify(e)}`);
  check(veg.parseScanLine("scan prefab=Prefabs/Props/A.et name=A p=1,2,3 top=1")?.vis === 1, "parse: an older line without class and vis counts as drawn");
  check(veg.parseScanLine("door c=1,2,3") === null, "parse: another line is skipped");
  check(veg.isVegetation(e), "a pine is vegetation");
  const rock = veg.parseScanLine("scan prefab={AA}PrefabLibrary/DefaultLibrary/Rocks/Granite/GraniteStone_01.et name=r p=1000,5,2010 top=1 class=GenericEntity mesh=- vis=1");
  check(!veg.isVegetation(rock), "a rock is not vegetation");
  const sandbag = veg.parseScanLine("scan prefab={BB}Prefabs/Props/Military/Sandbags/S.et name=s p=1000,5,2010 top=1 class=GenericEntity mesh=- vis=1");
  check(!veg.isVegetation(sandbag), "our own prop is not vegetation");

  // ---- the clearing
  const boxes = veg.clearingBoxes(kit, { margin: 2, reach: 6, gate: "gate" });
  check(boxes.length === 2, `the camp and the approach make two boxes, got ${boxes.length}`);
  check(same(boxes[0].offset, [0, 10]) && same(boxes[0].half, [12, 12]), `camp box: the footprint plus the margin, got ${JSON.stringify(boxes[0])}`);
  check(boxes[1].side === "-z", `the approach goes out of the -z side (the gate rows are nearest it), got ${boxes[1].side}`);
  check(same(boxes[1].offset, [0, -5]) && same(boxes[1].half, [4.5, 3]), `approach box: the gate rows' x-extent, 6 m out from the camp box, got ${JSON.stringify(boxes[1])}`);
  check(veg.clearingBoxes(kit, { margin: 0, reach: 0 }).length === 1, "reach 0 leaves the camp box only");
  const east = veg.clearingBoxes({ ...kit, rows: [{ id: "gate-a", offset: [9, 0, 8] }, { id: "gate-b", offset: [9, 0, 12] }] }, { margin: 1, reach: 5 });
  check(east[1]?.side === "+x" && same(east[1].offset, [13.5, 10]), `a gate on the +x side clears outward on +x, got ${JSON.stringify(east[1])}`);
  let threw = false;
  try {
    veg.clearingBoxes({ kit: "x", rows: [] });
  } catch {
    threw = true;
  }
  check(threw, "a kit with no footprint is an error, not an empty clearing");

  // ---- what falls in it (the anchor is the plot centre; world = anchor + kit)
  const at = (kind, dx, dz, cls = "Tree") => veg.parseScanLine(`scan prefab={${kind.length ? "AB" : "CD"}}PrefabLibrary/DefaultLibrary/Vegetation/${kind}.et name=n p=${anchor.x + dx},5,${anchor.z + dz} top=1 class=${cls} mesh=- vis=1`);
  const scan = [
    at("Trees/in_camp", 3, 12), // inside the camp box
    at("Bushes/in_camp_bush", -11, 1), // inside only because of the margin
    at("Trees/in_approach", 1, -4), // in the gate approach
    at("Trees/far_south", 1, -12), // past the reach
    at("Trees/far_east", 14, 10), // outside the camp box
    at("Trees/not_a_tree", 3, 12, "GenericEntity"), // the right place, the wrong class
  ];
  const selected = veg.selectVegetation(scan, boxes, anchor);
  const names = (i) => selected[i].found.map((x) => x.prefab.split("/").pop()).sort();
  check(same(names(0), ["in_camp.et", "in_camp_bush.et"]), `camp box takes the two, got ${names(0)}`);
  check(same(names(1), ["in_approach.et"]), `approach box takes the one in front of the gate, got ${names(1)}`);

  // a turned anchor: the same kit-frame spot is somewhere else in the world, and a tree is found by its kit position
  const turned = { x: 1000, z: 2000, yaw: 90 };
  const [lx, lz] = veg.localOf(turned, 1000 + 12, 2000 + 0);
  check(Math.abs(lx - 0) < 0.01 && Math.abs(lz - 12) < 0.01, `localOf turns a world point into the kit's frame at yaw 90, got ${lx}, ${lz}`);

  // ---- the kit entries
  const entries = veg.clearEntries(selected, 12, 4);
  check(entries.length === 3, `one entry per box and prefab, got ${entries.length}`);
  check(entries.every((c) => c.box.height === 12 && c.box.below === 4), "every entry carries the height and the depth");
  const old = { prefab: "{DD}Prefabs/Structures/Doors/Door_01.et", box: { offset: [0, 0], half: [1, 1], height: 4 } };
  const withOld = veg.withVegetation({ ...kit, clear: [old] }, entries);
  check(withOld.clear.length === 4 && same(withOld.clear[0], old), "the door entry stays, the three vegetation entries follow");
  const again = veg.withVegetation(withOld, veg.clearEntries(selected, 12, 4));
  check(same(again, withOld), "a second write is the same kit (it replaces its own entries)");
  const fewer = veg.withVegetation(withOld, []);
  check(same(fewer.clear, [old]), "no vegetation left drops only the vegetation entries");
  check(!("clear" in veg.withVegetation(kit, [])), "no entries and none before leaves no `clear` key");

  // ---- the fit turns them into carriers
  const groups = fit.clearBoxesOf(withOld);
  const campGroup = groups.find((g) => same(g.offset, [0, 10]));
  check(campGroup && campGroup.prefabs.length === 2 && campGroup.below === 4, `fit: the two camp prefabs share one box that reaches 4 m below, got ${JSON.stringify(campGroup)}`);
  check(groups.find((g) => same(g.offset, [0, 0]))?.below === 1, "fit: an entry with no `below` keeps the 1 m of the old boxes");
  const carriers = fit.clearBoxCarriers({ place: "demo", slot: "camp", anchor: { x: 1000, y: 7, z: 2000, yaw: 0 }, boxes: groups });
  const deep = carriers.find((c) => c.comps[0].set.m_vBoxHalfM === "12 0 12");
  check(deep?.comps[0].set.m_fBoxBelowM === "4", `fit: the carrier reaches 4 m below, got ${JSON.stringify(deep?.comps[0].set)}`);
  check(deep && same(deep.coords, [1000, 7, 2010]), `fit: the carrier stands at the box centre in the world, got ${JSON.stringify(deep?.coords)}`);
  const door = carriers.find((c) => c.comps[0].set.m_vBoxHalfM === "1 0 1");
  check(door && !("m_fBoxBelowM" in door.comps[0].set), "fit: a box with the default depth writes no m_fBoxBelowM (the old layers stay byte-identical)");

  // ---- the verify
  const after = [
    { ...at("Trees/in_camp", 3, 12), vis: 0 },
    { ...at("Bushes/in_camp_bush", -11, 1), vis: 1 }, // the carrier missed it
    { ...at("Trees/far_east", 14, 10), vis: 0 }, // hidden by a box edge
    at("Trees/far_south", 1, -12),
  ];
  const v = veg.verifyClearing(after, boxes, anchor);
  check(v.hiddenInside.length === 1 && v.shown.length === 1 && v.hiddenOutside.length === 1, `verify: counts hidden ${v.hiddenInside.length}, shown ${v.shown.length}, outside ${v.hiddenOutside.length}`);

  // ---- the hostless anchor
  const a = veg.hostlessAnchor({ slots: { camp: { hostless: true, plot: "p3" } } }, { freePlots: { plots: [{ id: "p3", x: 7745, z: 8327 }] } }, "camp");
  check(same(a, { x: 7745, z: 8327, yaw: 0 }), `the hostless anchor is the plot centre, got ${JSON.stringify(a)}`);
  let refused = false;
  try {
    veg.hostlessAnchor({ slots: { shop: { building: "b01" } } }, { freePlots: { plots: [] } }, "shop");
  } catch {
    refused = true;
  }
  check(refused, "a slot with a building is refused (only open ground is supported)");

  console.log(`SUMMARY town-vegetation-selftest ${failed ? "fail" : "ok"} ${failed}`);
  process.exit(failed ? 1 : 0);
}
