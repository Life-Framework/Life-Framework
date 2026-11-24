// tools/validation/hospital-cabinet-action-order-selftest.mjs: the clinic cabinet lists its own row first.
// The vanilla action list sorts by each action's "Sort Priority", lowest first, and rows with no priority come in no
// promised order. The cabinet's "Repair breach" row (greyed, "Only police or the owner can repair this") led the list
// ahead of "Breach the cabinet" in playtest round 3 (G3, #4180). The prefab and the debug copies of the cabinet give
// the cabinet's own row a priority below the repair row's, so repair is last.
// inputs: addons/LifeFramework/Prefabs/Props/Commercial/LF_HospitalCabinet.et plus the GridWorld layer that copies its action list (the frozen DebugWorld annex and town layers are not touched)
// red-proof: with the Sort Priority lines removed from the prefab, the "own row is first" case prints FAIL (2026-11-24).
import { readFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const root = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
const FILES = [
  ["prefab", "addons/LifeFramework/Prefabs/Props/Commercial/LF_HospitalCabinet.et"],
  ["grid layer", "addons/LifeFramework/Worlds/GridWorld/GridWorld_Layers/Grid_hospital.layer"],
];
const OWN = "LF_HospitalCabinetAction";
const REPAIR = "LF_BreachRepairAction";
let failed = 0;
const check = (cond, what) => {
  if (!cond) {
    failed++;
    console.log(`FAIL ${what}`);
  }
};

// The Sort Priority of the action block of class `cls` that follows line `from`, or null when it has none.
function priorityOf(lines, cls, from) {
  const at = lines.findIndex((l, i) => i > from && new RegExp(`^\\s*${cls} "`).test(l));
  if (at < 0) return undefined;
  const indent = lines[at].match(/^\s*/)[0];
  let end = at + 1;
  while (end < lines.length && lines[end] !== `${indent}}`) end++;
  const m = lines.slice(at, end).join("\n").match(/"Sort Priority" (\d+)/);
  return m ? Number(m[1]) : null;
}

for (const [label, rel] of FILES) {
  const lines = readFileSync(join(root, rel), "utf8").split("\n");
  // The cabinet's own action marks where its block starts; the repair row is the one after it.
  const start = lines.findIndex((l) => new RegExp(`^\\s*${OWN} "`).test(l));
  check(start >= 0, `${label}: the cabinet action is in the file`);
  if (start < 0) continue;
  const own = priorityOf(lines, OWN, start - 1);
  const repair = priorityOf(lines, REPAIR, start);
  check(repair !== undefined, `${label}: the repair row follows the cabinet action`);
  check(own !== null && own !== undefined, `${label}: the cabinet's own row has a Sort Priority`);
  check(repair !== null && repair !== undefined, `${label}: the repair row has a Sort Priority`);
  check(own != null && repair != null && own < repair, `${label}: the cabinet's own row is first, repair last (own row is first)`);
}

if (failed) {
  console.log(`SUMMARY hospital-cabinet-action-order-selftest fail ${failed}`);
  process.exit(1);
}
console.log("SUMMARY hospital-cabinet-action-order-selftest ok");
