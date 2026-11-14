// tools/validation/look-item-selftest.mjs
// inputs: tools/visual/**, agent-box/**, addons/**/*.et, addons/**/*.meta, tools/assets/**/spec.json
//
// `look item` (tools/visual/look-item.mjs): the dropped-item and inventory-preview pictures without a
// session. No Workbench here: the pure part (reading the prefab's attributes, the resting pose, the plan
// line the plugin parses) and that the plugin still reads the directive.
//
// red-proof: 2026-11-14, with BASIS.Right set to the Left basis the run printed
// `FAIL RestingUP Right: model axis x does not face +y` and exited 1; restored, it passes.
import { readFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { ITEM_STATES, RESTING, itemAttributes, itemLine, itemPlan, resolvePrefab, restingBasis } from "../visual/look-item.mjs";

const root = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
const failures = [];
const check = (ok, msg) => {
  if (!ok) failures.push(msg);
};

// The resting pose: the named model axis lands on +y, and the three axes stay a right-handed orthonormal frame.
const AXIS = { Up: 1, Down: 1, Right: 0, Left: 0, Forward: 2, Backward: 2 };
const SIGN = { Up: 1, Down: -1, Right: 1, Left: -1, Forward: 1, Backward: -1 };
for (const up of RESTING) {
  const b = restingBasis(up);
  const image = b[AXIS[up]];
  check(image[1] * SIGN[up] === 1 && Math.abs(image[0]) + Math.abs(image[2]) === 0, `RestingUP ${up}: model axis ${"xyz"[AXIS[up]]} does not face +y`);
  const dot = (p, q) => p[0] * q[0] + p[1] * q[1] + p[2] * q[2];
  const cross = (p, q) => [p[1] * q[2] - p[2] * q[1], p[2] * q[0] - p[0] * q[2], p[0] * q[1] - p[1] * q[0]];
  check(dot(b[0], b[1]) === 0 && dot(b[1], b[2]) === 0 && dot(b[0], b[2]) === 0, `RestingUP ${up}: axes are not perpendicular`);
  check(dot(cross(b[0], b[1]), b[2]) === 1, `RestingUP ${up}: a mirror, not a rotation`);
}
check(restingBasis("Sideways") === null, "an unknown RestingUP must not invent a pose");

// Reading the prefab: stated values, and the ones it leaves to the parent are named.
const stated = itemAttributes("PreviewRenderAttributes {\n CameraOrbitAngles -60 -40 0\n CameraDistanceToItem 0.4\n FOV 10\n}\nItemPhysAttributes {\n RestingUP Right\n RestingAdditiveOffsetLS -0.08 0 0\n}\n");
check(JSON.stringify(stated.preview.angles) === "[-60,-40,0]" && stated.preview.distance === 0.4 && stated.preview.fov === 10, "preview values not read");
check(stated.ground.up === "Right" && stated.ground.offset[0] === -0.08, "ground values not read");
check(stated.missing.join() === "CameraOffset", `unexpected missing list: ${stated.missing}`);
const bare = itemAttributes("Prefab {\n}\n");
check(bare.ground.up === "Up" && bare.preview.fov === 20 && bare.missing.length === 6, "a prefab with nothing stated must fall back to defaults and say so");
check(itemAttributes("ItemPhysAttributes {\n RestingUP Sideways\n}\n").missing.includes("RestingUP"), "an invalid RestingUP must count as not stated");
check(itemAttributes("RestingAdditiveRotationLS 0 90 0\n").ground.rotationIgnored, "a RestingAdditiveRotationLS must be reported as not applied");

// The plan line: fixed width, the nudge carried into the world through the pose.
const line = itemLine({ name: "n", mode: "ground", x: 1, z: 2, attrs: stated, prefab: "{0123456789ABCDEF}Prefabs/X.et" });
const f = line.split(" ");
check(f.length === 26 && f[0] === "item" && f[25].startsWith("{"), `item line has ${f.length} fields, expected 26`);
check(f.slice(14, 17).join(" ") === "0 -0.08 0", `Right pose must carry the x nudge to world -y, got ${f.slice(14, 17).join(" ")}`);

// A real prefab through the resolver and planner.
try {
  const r = resolvePrefab(root, "Prefabs/Items/Fence/LF_Drill.et");
  check(/^\{[0-9A-F]{16}\}Prefabs\/Items\/Fence\/LF_Drill\.et$/.test(r.ref), `resolved ref ${r.ref}`);
  const plan = itemPlan(root, [r.ref], { at: { x: 100, z: 200 } });
  check(plan.perArea[0].shots.map((s) => s.kind).join() === ITEM_STATES.join(), "both states must be planned by default");
  check(itemPlan(root, [r.ref, r.ref], { states: ["ground"], at: { x: 100, z: 200 } }).perArea[1].shots[0].line.split(" ")[3] === "103", "a second prefab must stand 3 m along x");
} catch (e) {
  failures.push(`real prefab: ${e.message}`);
}
try {
  itemPlan(root, ["Prefabs/Items/Fence/LF_Drill.et"], { states: ["hand"], at: { x: 0, z: 0 } });
  failures.push("an unknown state must be refused");
} catch {
  /* expected */
}
try {
  resolvePrefab(root, "Prefabs/Nope/Nothing.et");
  failures.push("a missing prefab must be refused");
} catch {
  /* expected */
}

// The plugin reads the directive the plan writes.
const plugin = readFileSync(join(root, "tools", "visual", "wb", "LF_VisualShotsPlugin.c"), "utf8");
check(plugin.includes('line.StartsWith("item ")') && plugin.includes('p[0] == "item"') && plugin.includes("TakeItem("), "the plugin does not read the item directive");

if (failures.length) {
  for (const x of failures) console.log(`FAIL ${x}`);
  process.exit(1);
}
console.log("look-item-selftest: OK (resting poses are rotations facing up, prefab attributes and defaults, plan line, real prefab, plugin directive)");
