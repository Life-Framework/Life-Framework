// tools/validation/validate-action-contexts.mjs
//
// A world interactable must not keep the vanilla door volume: Offset 0 0 0,
// Radius 0.650, Height 0. That point is the hinge. Run
// node tools/actions/place-action-contexts.mjs so the point sits on the mesh.

import { auditActionContexts } from "../actions/place-action-contexts.mjs";

const { failures, checked } = auditActionContexts();
for (const failure of failures) console.log(`ERROR ${failure}`);
if (failures.length) {
  console.log(`validate-action-contexts: FAILED (${failures.length} error(s), ${checked} interactable(s) checked)`);
  process.exit(1);
}
console.log(`validate-action-contexts: OK (${checked} interactable(s) checked)`);
