// tools/validation/town-marker-offset-selftest.mjs: a further place marker kind may stand away from the building.
// The orchard's waypoint kind (OrchardTrees) stands on its first apple tree, 8.5 m east and 1.5 m north of the barn in
// the kit frame, while the first kind (Orchard) stays on the barn where LF_FittedBuildingComponent needs it.
// inputs: tools/town/fit.mjs, tools/town/kits/orchard.json
// red-proof: with markerCoords returning anchor.marker for every kind, the "turned with the anchor" and "kit offset" cases print FAIL (2026-11-22).
import { readFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { markerCoords } from "../town/fit.mjs";

const root = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
let failed = 0;
const check = (cond, what) => {
  if (!cond) {
    failed++;
    console.log(`FAIL ${what}`);
  }
};
const near = (a, b) => a.every((v, i) => Math.abs(v - b[i]) < 0.002);

const kit = JSON.parse(readFileSync(join(root, "tools/town/kits/orchard.json"), "utf8"));
const anchor = { x: 100, y: 10, z: 200, yaw: 0, marker: [100, 10, 200] };
check(near(markerCoords(kit, "Orchard", 0, anchor), [100, 10, 200]), "the first kind stays on the building");
check(near(markerCoords(kit, "OrchardTrees", 1, anchor), [108.5, 10, 201.5]), "the kit offset moves a further kind (kit offset)");
const turned = { ...anchor, yaw: 90 };
const t = markerCoords(kit, "OrchardTrees", 1, turned);
check(Math.hypot(t[0] - 100, t[2] - 200) > 8.6 && Math.hypot(t[0] - 100, t[2] - 200) < 8.8 && !near(t, [108.5, 10, 201.5]), "the offset turns with the anchor (turned with the anchor)");
check(near(markerCoords(kit, "Nothing", 1, anchor), [100, 10, 200]), "a kind with no offset stays on the building");
check(near(markerCoords({ anchoring: {} }, "OrchardTrees", 1, anchor), [100, 10, 200]), "a kit with no offsets leaves every marker on the building");

if (failed) {
  console.log(`SUMMARY town-marker-offset-selftest fail ${failed}`);
  process.exit(1);
}
console.log("SUMMARY town-marker-offset-selftest ok");
