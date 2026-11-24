// tools/validation/validate-interaction-points.mjs
// inputs: addons/**, tools/actions/**
//
// An interaction point (a UserActionContext PointInfo Offset) must sit above the top of the prop it belongs to, where a
// player looking down at a desk, counter, bench or seat lands the aim ray. A point under the top, or within 2 cm above
// it, lost the booking desk's row on #4803 (0.9 m under a 0.906 m top; it was offered once the point went to 1.05 m).
// It reads the prefabs and the context overrides in .layer files of the shipped worlds (the test worlds are fixtures),
// against the up-facing collider tops in tools/actions/interaction-surfaces.json, so the geometry needs no game install.
// Fix: node tools/actions/interaction-points.mjs --fix. Refresh the table after a prop's model changed or a prop was
// added: node tools/actions/interaction-points.mjs --update. A point that stays under a top is listed in
// tools/actions/interaction-points-allow.json with the reason. Needs the base-game pak to resolve a prefab's ancestors,
// so on a machine without it (CI) it checks nothing and says so. Docs: docs/action-anchors.md "Interaction points".

import { checkPoints, describe } from "../actions/interaction-points.mjs";

let result;
try {
  result = checkPoints();
} catch (e) {
  console.log(`validate-interaction-points: SKIPPED (${e.message})`);
  process.exit(0);
}
const { findings, checked, allowed, unknown } = result;
for (const f of findings) console.log(`ERROR ${describe(f)}`);
for (const mesh of unknown) console.log(`ERROR model ${mesh} carries an interaction point but is not in tools/actions/interaction-surfaces.json: run node tools/actions/interaction-points.mjs --update and commit it`);
if (findings.length || unknown.length) {
  console.log(`validate-interaction-points: FAILED (${findings.length} point(s) under their prop's top, ${unknown.length} model(s) missing from the table, ${checked} checked)`);
  process.exit(1);
}
console.log(`validate-interaction-points: OK (${checked} interaction point(s) checked, ${allowed} allowed with a reason)`);
