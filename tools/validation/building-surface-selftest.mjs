// tools/validation/building-surface-selftest.mjs
// inputs: tools/buildings/pipeline/surface.mjs, tools/buildings/pipeline/floaters.mjs, tools/lib/pr-quality.mjs, tools/validation/building-surface-selftest.mjs
//
// Proves the floating-prop guard on a fixture desk, phone and crates (a reader of fixtures stands in for the paks, so no game
// is read): a phone hovering over a desk fails `building check`, the same phone snapped onto the desk passes, a phone on the
// floor passes; the write-time snap drops a typed-too-high prop onto the desk (named `on`, or automatically within the band),
// leaves a wall poster and a prop within 2 cm alone, stacks a crate on an open crate's rim, refuses a named support with
// nothing under the prop, and rewrites only the y of the entity it is told to.
// red-proof: with snapItems computing y = level.y + f.bottom - 0.2 this printed "FAIL named snap: want 0.951, got 0.749" and 4 more
// (snapped phone passes, auto snap, crate rim) (2026-11-17).

import { mkdirSync, mkdtempSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { checkBuilding } from "../buildings/pipeline/floaters.mjs";
import { buildingCheckProblems, buildingSlugsTouched } from "../lib/pr-quality.mjs";
import { floaters, prefabChildren, setHeights, snapItems, specSupports } from "../buildings/pipeline/surface.mjs";

let failed = 0;
const want = (label, got, exp) => {
  const same = typeof exp === "number" ? Math.abs(got - exp) < 0.0006 : got === exp;
  if (!same) {
    failed++;
    console.log(`FAIL ${label}: want ${exp}, got ${got}`);
  }
};

const DESK = "Prefabs/Fixture/Desk.et";
const PHONE = "Prefabs/Fixture/PhoneTable_01.et";
const CRATE = "Prefabs/Fixture/Crate_01.et";
const POSTER = "Prefabs/Fixture/PosterSingle_01.et";
const PLATFORM = "Prefabs/Fixture/Platform.et"; // the forecourt slab, top 0.2
const SHOPHOST = "Prefabs/Fixture/ShopHost.et"; // the shop's own shell, floor 0.1 in the shop's frame
const MODELS = {
  [PLATFORM]: { bounds: { min: [-20, 0, -10], max: [20, 0.2, 10] }, surfaces: [{ y: 0.2, x: [-20, 20], z: [-10, 10] }] },
  [SHOPHOST]: { bounds: { min: [-4, 0, -3], max: [4, 3, 3] }, surfaces: [{ y: 0.1, x: [-4, 4], z: [-3, 3] }] },
  [DESK]: { bounds: { min: [-0.6, 0, -0.3], max: [0.6, 0.75, 0.3] }, surfaces: [{ y: 0.75, x: [-0.6, 0.6], z: [-0.3, 0.3] }] },
  [PHONE]: { bounds: { min: [-0.1, -0.001, -0.1], max: [0.1, 0.12, 0.1] }, surfaces: [] },
  [CRATE]: { bounds: { min: [-0.3, 0, -0.2], max: [0.3, 0.22, 0.2] }, surfaces: [{ y: 0.012, x: [-0.28, 0.28], z: [-0.18, 0.18] }] }, // open: only the inside floor
  [POSTER]: { bounds: { min: [-0.2, 0, -0.01], max: [0.2, 0.6, 0.01] }, surfaces: [] },
};
const reader = {
  info: (p) => (MODELS[p] ? { bounds: MODELS[p].bounds, scale: 1 } : null),
  surfaces: async (p) => MODELS[p]?.surfaces ?? null,
};
const FLOOR = 0.2;
const DESK_TOP = FLOOR + 0.75;

const prefab = (rows) =>
  `GenericEntity : "{0000000000000001}Prefabs/Base.et" {
 ID "0000000000000001"
 {
${rows.map((r, i) => `  GenericEntity : "{00000000000000${String(i + 10).padStart(2, "0")}}${r.path}" {
   ID "A0000000000000${String(i + 10).padStart(2, "0")}"
   coords ${r.x} ${r.y} ${r.z}
  }`).join("\n")}
 }
}
`;
const desk = { path: DESK, x: 0, y: FLOOR, z: 0 };
const judge = async (rows) => floaters(prefabChildren(prefab(rows)), { reader, floors: [FLOOR] });

// 1. a fixture desk plus a floating phone fails
let bad = await judge([desk, { path: PHONE, x: 0.2, y: DESK_TOP + 0.15, z: 0 }]);
want("hover is floating: count", bad.length, 1);
want("hover kind", bad[0]?.kind, "floating");
want("hover gap", bad[0]?.gap, 0.149); // 15 cm, less the 1 mm the phone mesh reaches under its pivot
want("hover names the support", bad[0]?.support, "A000000000000010");

// 2. the snapped phone passes
const snapped = [{ path: PHONE, x: 0.2, y: DESK_TOP + 0.15, z: 0, on: "desk" }];
const r1 = await snapItems(snapped, { fixed: [{ id: "desk", ...desk, scale: null }], reader });
want("named snap", snapped[0].y, DESK_TOP + 0.001); // the phone's mesh bottom is 1 mm under its pivot
want("named snap reports one move", r1.changes.length, 1);
want("snapped phone passes", (await judge([desk, { path: PHONE, x: 0.2, y: snapped[0].y, z: 0 }])).length, 0);

// 3. a phone on the floor passes
want("phone on the floor passes", (await judge([desk, { path: PHONE, x: 2, y: FLOOR, z: 0 }])).length, 0);
want("phone with nothing under it, off the floor, fails", (await judge([desk, { path: PHONE, x: 2, y: 1.4, z: 0 }]))[0]?.kind, "unsupported");
want("phone inside the desk top is sunk", (await judge([desk, { path: PHONE, x: 0.2, y: DESK_TOP - 0.05, z: 0 }]))[0]?.kind, "sunk");

// 4. the automatic snap: within the band it drops, beyond it the prop is left (it is hung or placed on purpose)
const auto = [{ path: PHONE, x: 0.2, y: DESK_TOP + 0.3, z: 0 }, { path: PHONE, x: -0.2, y: DESK_TOP + 0.9, z: 0 }, { path: PHONE, x: 0, y: DESK_TOP + 0.015, z: 0.1 }];
await snapItems(auto, { fixed: [{ id: "desk", ...desk, scale: null }], reader });
want("auto snap inside the band", auto[0].y, DESK_TOP + 0.001);
want("auto snap leaves a prop far above", auto[1].y, DESK_TOP + 0.9);
want("auto snap leaves a prop within 2 cm", auto[2].y, DESK_TOP + 0.015);

// 5. a wall poster over the desk is neither moved nor flagged
const hung = [{ path: POSTER, x: 0, y: DESK_TOP + 0.2, z: 0 }];
await snapItems(hung, { fixed: [{ id: "desk", ...desk, scale: null }], reader });
want("poster stays", hung[0].y, DESK_TOP + 0.2);
want("poster is not flagged", (await judge([desk, { path: POSTER, x: 0, y: 1.9, z: 0 }])).length, 0);

// 6. an open crate: the next one stands on its rim (bounds top), not in it
const stack = [{ path: CRATE, x: 3, y: FLOOR, z: 0, id: "low" }, { path: CRATE, x: 3, y: FLOOR + 0.25, z: 0 }];
await snapItems(stack, { fixed: [], reader });
want("crate on crate rim", stack[1].y, FLOOR + 0.22);
want("stacked crates pass", (await judge(stack.map((s) => ({ ...s })))).length, 0);

// 7. a named support with nothing under the prop stops the generator
let threw = "";
try {
  await snapItems([{ path: PHONE, x: 4, y: 1, z: 0, on: "desk" }], { fixed: [{ id: "desk", ...desk, scale: null }], reader });
} catch (e) {
  threw = e.message;
}
want("named support with nothing under it throws", /stands on "desk"/.test(threw), true);

// 8. the allow list, and the spec's extra supports (a counter the layer places)
const floating = [desk, { path: PHONE, x: 0.2, y: DESK_TOP + 0.15, z: 0 }];
want("allow by entity ID", (await floaters(prefabChildren(prefab(floating)), { reader, floors: [FLOOR], allow: [{ match: "A000000000000011", why: "test" }] })).length, 0);
want("allow by prefab regex", (await floaters(prefabChildren(prefab(floating)), { reader, floors: [FLOOR], allow: [{ match: "PhoneTable", why: "test" }] })).length, 0);
const sup = specSupports({ vanilla: { base: "{0123456789ABCDEF}Prefabs/Host.et" }, check_supports: [{ prefab: DESK, at: [5, FLOOR, 0], id: "layer-desk" }] });
want("specSupports lists the host and the extra", sup.map((s) => s.id).join(","), "host,layer-desk");
want("a prop on the layer's desk passes", (await floaters(prefabChildren(prefab([{ path: PHONE, x: 5, y: DESK_TOP, z: 0 }])), { reader, floors: [FLOOR], extra: sup })).length, 0);

// 9. setHeights changes the one entity's y and nothing else
const text = prefab([desk, { path: PHONE, x: 0.2, y: 1.5, z: 0.1 }]);
const edited = setHeights(text, [{ id: "A000000000000011", to: 0.951 }]);
want("setHeights count", edited.done, 1);
want("setHeights rewrote the y", edited.text.includes("coords 0.2 0.951 0.1"), true);
want("setHeights left the desk", edited.text.includes(`coords 0 ${FLOOR} 0`), true);

// 10. the command over a building on disk: a spec, its prefabs, the floor from the spec
const root = mkdtempSync(join(tmpdir(), "lf-building-check-"));
mkdirSync(join(root, "tools", "buildings", "shop"), { recursive: true });
mkdirSync(join(root, "addons", "LF_Pack", "Prefabs", "Buildings", "Shop", "Furniture"), { recursive: true });
writeFileSync(join(root, "tools", "buildings", "shop", "spec.json"), JSON.stringify({ slug: "shop", addon: "LF_Pack", prefab_dir: "Prefabs/Buildings/Shop", floors: [FLOOR] }));
writeFileSync(join(root, "addons", "LF_Pack", "Prefabs", "Buildings", "Shop", "Furniture", "Shop_furniture_01.et"), prefab(floating));
const res = await checkBuilding(root, "shop", { reader });
want("checkBuilding finds the hover", res.problems.length, 1);
want("checkBuilding reads the spec's floors", res.floors.join(","), String(FLOOR));

// 11. wt ship: a PR that touches a building's generator or its pack prefabs runs the check and fails on a floater
const specs = [{ slug: "shop", addon: "LF_Pack", prefab_dir: "Prefabs/Buildings/Shop" }, { slug: "bank", addon: "LF_Pack", prefab_dir: "Prefabs/Buildings/Bank" }];
want("slugs from a generator and a prefab", buildingSlugsTouched(["tools/buildings/shop/decor.mjs", "addons/LF_Pack/Prefabs/Buildings/Bank/Furniture/Bank_furniture_01.et", "tools/buildings/pipeline/surface.mjs", "docs/x.md"], specs).join(","), "bank,shop");
const asked = [];
want("a failing check blocks the ship", buildingCheckProblems({ files: ["tools/buildings/shop/decor.mjs"], specs, run: (s) => (asked.push(s), { code: 1, out: "  A1 PhoneTable_01 at 0 1 0: floats 0.15 m above the top of Desk\nSUMMARY building check fail shop: 1" }) }).length, 1);
want("the failing check names the prop", /floats 0.15 m/.test(buildingCheckProblems({ files: ["tools/buildings/shop/decor.mjs"], specs, run: () => ({ code: 1, out: "  A1 PhoneTable_01 at 0 1 0: floats 0.15 m above the top of Desk" }) })[0]), true);
want("a passing check is quiet", buildingCheckProblems({ files: ["tools/buildings/shop/decor.mjs"], specs, run: () => ({ code: 0, out: "" }) }).length, 0);
want("a check that cannot run here is skipped", buildingCheckProblems({ files: ["tools/buildings/shop/decor.mjs"], specs, run: () => ({ code: 2, out: "" }) }).length, 0);
want("an unrelated PR runs nothing", buildingCheckProblems({ files: ["docs/x.md"], specs, run: () => { throw new Error("should not run"); } }).length, 0);


// 12. a building placed at 0, 90 and -90 degrees whose fit-out rows are in a grid layer: the rows are carried into the building's
// frame (its coords, its yaw in `angleY` or in `angles`, its raised plot), so a phone on a desk the prefab holds passes and one hovering fails
const rotated = (name, angle, form, extra = "") => {
  const dir = mkdtempSync(join(tmpdir(), `lf-building-rot-${name}-`));
  mkdirSync(join(dir, "tools", "buildings", "rot"), { recursive: true });
  mkdirSync(join(dir, "addons", "LF_Pack", "Prefabs", "Buildings", "Rot", "Furniture"), { recursive: true });
  mkdirSync(join(dir, "layers"), { recursive: true });
  const spec = { slug: "rot", addon: "LF_Pack", prefab_dir: "Prefabs/Buildings/Rot", floors: [FLOOR], check_layer: { file: "layers/rot.layer", host: "Rot_Main", prefix: "RotKit_" } };
  writeFileSync(join(dir, "tools", "buildings", "rot", "spec.json"), JSON.stringify(spec));
  writeFileSync(join(dir, "addons", "LF_Pack", "Prefabs", "Buildings", "Rot", "Furniture", "Rot_furniture_01.et"), prefab([{ path: DESK, x: 2, y: FLOOR, z: 1 }]));
  const HOST = { x: 100, y: 0.05, z: 50 }; // the plot is 5 cm up, like the hostel's
  const world = (lx, ly, lz) => {
    const c = Math.cos((angle * Math.PI) / 180);
    const sn = Math.sin((angle * Math.PI) / 180);
    return [HOST.x + lx * c + lz * sn, HOST.y + ly, HOST.z - lx * sn + lz * c];
  };
  const row = (id, path, lx, ly, lz) => `GenericEntity RotKit_${id} : "{00000000000000AA}${path}" {
 coords ${world(lx, ly, lz).map((v) => Math.round(v * 1e4) / 1e4).join(" ")}
 ${form === "angles" ? `angles 0 ${angle} 0` : `angleY ${angle}`}
}
`;
  const text = `SCR_DestructibleBuildingEntity Rot_Main : "{00000000000000BB}Prefabs/Fixture/Host.et" {
 coords ${HOST.x} ${HOST.y} ${HOST.z}
 ${form === "angles" ? `angles 0 ${angle} 0` : `angleY ${angle}`}
}
${row("Phone", PHONE, 2.2, DESK_TOP, 1)}${extra && row("Float", PHONE, 1.7, DESK_TOP + 0.15, 1)}`;
  writeFileSync(join(dir, "layers", "rot.layer"), text);
  return dir;
};
for (const angle of [0, 90, -90]) {
  for (const form of ["angleY", "angles"]) {
    const ok = await checkBuilding(rotated(`ok${angle}`, angle, form), "rot", { reader });
    want(`rotated ${angle} ${form}: phone on the desk passes`, ok.problems.length, 0);
    want(`rotated ${angle} ${form}: the phone row is judged`, ok.ents.some((e) => e.name === "RotKit_Phone"), true);
    const hover = await checkBuilding(rotated(`bad${angle}`, angle, form, "float"), "rot", { reader });
    want(`rotated ${angle} ${form}: a hovering phone fails`, hover.problems.length, 1);
    want(`rotated ${angle} ${form}: it is the hovering one`, hover.problems[0]?.id, "RotKit_Float");
    want(`rotated ${angle} ${form}: it floats by the 15 cm`, hover.problems[0]?.gap, 0.149);
  }
}

// 13. two buildings in one prefab: a platform places a shop at an offset and a turn; the shop's furniture is judged in the shop's
// frame (its floor 0.1, its shell), not on the platform's slab (top 0.2), while the platform's own props are judged on the slab
const entity = (path, id, coords, angles = "") => `  GenericEntity : "{00000000000000CC}${path}" {
   ID "${id}"
   coords ${coords}
${angles ? `   angles ${angles}
` : ""}  }
`;
const wrap = (rows) => `GenericEntity : "{0000000000000001}Prefabs/Base.et" {
 ID "0000000000000001"
 {
${rows}
 }
}
`;
const two = (shopRows) => {
  const dir = mkdtempSync(join(tmpdir(), "lf-building-two-"));
  const base = join(dir, "addons", "LF_Pack", "Prefabs", "Buildings", "Two");
  mkdirSync(join(base, "Furniture"), { recursive: true });
  mkdirSync(join(dir, "tools", "buildings", "two"), { recursive: true });
  writeFileSync(join(dir, "tools", "buildings", "two", "spec.json"), JSON.stringify({ slug: "two", addon: "LF_Pack", prefab_dir: "Prefabs/Buildings/Two", vanilla: { base: PLATFORM, shop: SHOPHOST } }));
  writeFileSync(join(base, "Two.et"), wrap(entity("Prefabs/Buildings/Two/Two_Shop.et", "B000000000000001", "10 0.3 4", "0 -180 0") + entity(DESK, "B000000000000002", "-6 0.2 0")));
  writeFileSync(join(base, "Two_Shop.et"), wrap(entity("Prefabs/Buildings/Two/Furniture/Two_Shop_furniture_01.et", "B000000000000003", "0 0 0")));
  writeFileSync(join(base, "Furniture", "Two_Shop_furniture_01.et"), prefab(shopRows));
  return dir;
};
// a phone on the shop floor (0.1) and one on a shop desk: they stand in the shop's frame
const shopOk = [{ path: PHONE, x: 1, y: 0.1, z: 1 }, { path: DESK, x: -2, y: 0.1, z: 0 }, { path: PHONE, x: -2, y: 0.1 + 0.75, z: 0 }];
const r13 = await checkBuilding(two(shopOk), "two", { reader });
want("two buildings: the parts", r13.parts.join(","), ",Two_Shop");
want("two buildings: the platform's floor is the slab", r13.floors.join(","), "0.2");
want("two buildings: the shop has its own floor", r13.partFloors.Two_Shop?.includes(0.1), true);
want("two buildings: shop props on the shop floor pass", r13.problems.length, 0);
want("two buildings: every entity is judged", r13.ents.length, 6);
const r13bad = await checkBuilding(two([...shopOk, { path: PHONE, x: 1, y: 0.4, z: -1 }]), "two", { reader });
want("two buildings: a hovering shop phone still fails", r13bad.problems.length, 1);
want("two buildings: the shop desk's own phone is not the fault", r13bad.problems[0]?.y, 0.4);
// the platform's own props stay on the slab: a phone at the shop floor height standing in the platform's frame floats
const r13dir = two(shopOk);
writeFileSync(join(r13dir, "addons", "LF_Pack", "Prefabs", "Buildings", "Two", "Two.et"), wrap(entity("Prefabs/Buildings/Two/Two_Shop.et", "B000000000000001", "10 0.3 4", "0 -180 0") + entity(DESK, "B000000000000002", "-6 0.2 0") + entity(PHONE, "B000000000000009", "3 0.1 0")));
want("two buildings: a platform prop at 0.1 is judged on the slab", (await checkBuilding(r13dir, "two", { reader })).problems.map((p) => p.id).join(","), "B000000000000009");

console.log(failed ? `building-surface-selftest: ${failed} failure(s)` : "building-surface-selftest: OK (hover fails, snapped and floor pass, auto band, rim stack, hung kinds, allow, extra supports, setHeights, command, rotated hosts, two buildings in one prefab)");
export async function main() {
  return failed ? 1 : 0;
}
if (failed) process.exit(1);
