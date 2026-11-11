// tools/validation/validate-action-reach.mjs
//
// Warning-level check: a world interactable's action context should sit on its
// mesh and reach a standing player. A point more than 0.5 m outside the mesh
// bounds, or a radius too short for a player 0.5 m off the mesh, is a WARN.
// Only a gross miss (point more than 3 m outside the bounds) fails.
// Fix with: node tools/actions/place-action-contexts.mjs --anchor front <entity>
// (docs/action-anchors.md).

import { checkContexts } from "../actions/action-anchor.mjs";

const { warnings, errors, checked, skipped } = checkContexts();
for (const w of warnings) console.log(`WARN ${w}`);
for (const e of errors) console.log(`ERROR ${e}`);
if (errors.length) {
  console.log(`validate-action-reach: FAILED (${errors.length} gross error(s), ${warnings.length} warning(s), ${checked} checked)`);
  process.exit(1);
}
console.log(`validate-action-reach: OK (${warnings.length} warning(s), ${checked} interactable(s) checked, ${skipped} without bounds)`);
