// tools/validation/validate-action-reach.mjs
// inputs: addons/**, docs/action-anchors.md, tools/actions/**
//
// A world interactable's action context should sit on its mesh. A point more than
// 0.5 m outside the mesh bounds, a radius under 0.1 m or one that does not touch the
// mesh fails (it was a warning until 2026-11-27, when the last 28 were cleared).
// The engine picks a context by the aim ray hitting its sphere, so there is no "player
// stands 0.5 m off" model (`session.mjs prop-reach-proof`).
// Fix with: node tools/actions/place-action-contexts.mjs --anchor front <entity>
// (docs/action-anchors.md).

import { checkContexts } from "../actions/action-anchor.mjs";
import { checkCounterNpcs, checkCounterStaffPrefabs } from "../actions/counter-npc.mjs";

const { warnings, errors, checked, skipped } = checkContexts();
// A clerk behind a counter must be talkable from the customer side (docs/action-anchors.md "Counter NPCs").
const counter = checkCounterNpcs();
const staff = checkCounterStaffPrefabs();
errors.push(...warnings, ...counter.errors, ...staff.errors);
warnings.length = 0;
for (const e of errors) console.log(`ERROR ${e}`);
if (errors.length) {
  console.log(`validate-action-reach: FAILED (${errors.length} error(s), ${checked} checked)`);
  process.exit(1);
}
console.log(`validate-action-reach: OK (${checked} interactable(s) checked, ${counter.checked} counter NPC(s) and ${staff.checked} staff prefab(s) checked, ${skipped} without bounds)`);
