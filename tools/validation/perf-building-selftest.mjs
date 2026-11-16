// tools/validation/perf-building-selftest.mjs
// inputs: tools/perf/building-count.mjs, tools/validation/perf-building-selftest.mjs
//
// Proves `tools\cli perf building` (tools/perf/building-count.mjs) on a fixture building written to a
// temp folder: entity blocks are counted and components are not, a child prefab reference adds its
// insides, CastShadow 0 / 1 / unset are told apart (unset counts as a caster), a point light casting a
// shadow is flagged, an unresolved vanilla reference is listed, and the budget verdict names each limit.
// red-proof: with countTree counting a light as a caster only on an explicit "CastShadow 1" this printed
// "FAIL shadow: want 3 (one explicit, two unset): want 3, got 1" (2026-11-17).

import { mkdirSync, mkdtempSync, readFileSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { BUDGET, countTree, findBuilding, makeResolver, parseEntities, verdict } from "../perf/building-count.mjs";

let failed = 0;
const want = (label, got, exp) => {
  if (got !== exp) {
    failed++;
    console.log(`FAIL ${label}: want ${exp}, got ${got}`);
  }
};

const root = mkdtempSync(join(tmpdir(), "lf-perf-building-"));
const dir = join(root, "addons", "LF_Pack", "Prefabs", "Buildings", "Shop");
mkdirSync(join(dir, "Furniture"), { recursive: true });
writeFileSync(join(dir, "Shop.et"), `SCR_DestructibleBuildingEntity : "{0000000000000001}Prefabs/Vanilla/House.et" {
 ID "1"
 components {
  MeshObject "{0000000000000002}" {
   Object "{0000000000000003}Assets/x.xob"
  }
 }
 {
  GenericEntity : "{00000000000000F1}Prefabs/Buildings/Shop/Furniture/Shop_furniture.et" {
   coords 0 0 0
  }
  GenericEntity : "{00000000000000F2}Prefabs/Vanilla/Chair.et" {
   coords 1 0 0
  }
 }
}
`);
writeFileSync(join(dir, "Furniture", "Shop_furniture.et"), `GenericEntity : "{0000000000000004}Prefabs/Structures/Core/BuildingFurniture_base.et" {
 ID "2"
 {
  GenericEntity : "{0000000000000005}Prefabs/Vanilla/Table.et" {
   coords 0 0 0
  }
  LightEntity {
   Type Spot
   CastShadow 1
  }
  LightEntity {
   Type Spot
   CastShadow 0
  }
  LightEntity {
   Type Point
  }
  LightEntity {
   Type Spot
  }
 }
}
`);

const blocks = parseEntities(`GenericEntity {\n components {\n  SCR_Foo "{0000000000000009}" {\n   m_bX 1\n  }\n }\n {\n  LightEntity {\n   CastShadow 0\n  }\n }\n}\n`);
want("one root entity", blocks.length, 1);
want("component is not an entity", blocks[0].children.length, 1);
want("child light class", blocks[0].children[0]?.cls, "LightEntity");

const found = findBuilding("shop", root);
want("one root prefab (the furniture is referenced)", found?.files.length, 1);
const resolve = await makeResolver(root);
const offline = (ref) => (ref.includes("/Vanilla/") || ref.includes("/Core/") ? null : resolve(ref));
const t = countTree(parseEntities(readFileSync(found.files[0], "utf8")), offline);
// shop, furniture child, chair, then inside the furniture: table + 4 lights
want("entities", t.entities, 8);
want("lights", t.lights, 4);
want("shadow: want 3 (one explicit, two unset)", t.shadow, 3);
want("unset", t.unset, 2);
want("point lights casting shadows", t.point, 1);
want("unresolved vanilla refs listed", t.unresolved.size >= 3, true);

const over = verdict(t, "small");
want("small budget is over on shadow casters", over.some((o) => o.startsWith("shadow casters 3 > 0")), true);
want("unset lights are named", over.some((o) => o.includes("leave CastShadow unset")), true);
want("budget sizes", Object.keys(BUDGET).join(","), "small,medium,large");
want("a clean count passes", verdict({ entities: 10, lights: 1, shadow: 0, unset: 0, point: 0 }, "small").length, 0);

console.log(failed ? `perf-building-selftest: ${failed} failure(s)` : "perf-building-selftest: OK (entities, refs, CastShadow 0/1/unset, point shadows, unresolved, verdict)");
process.exit(failed ? 1 : 0);
