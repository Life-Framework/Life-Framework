// tools/validation/input-listeners-selftest.mjs — red proof for validate-input-listeners.
// inputs: imports
import assert from "node:assert/strict";
import { problems } from "./validate-input-listeners.mjs";

const f = (p, text) => ({ path: `addons/LifeFramework/Scripts/Game${p}`, text });

assert.deepEqual(problems([f("/Feature/X/LF_A.c", 'input.AddActionListener("A", EActionTrigger.DOWN, OnA);\ninput.RemoveActionListener("A", EActionTrigger.DOWN, OnA);')]), [], "an add with its remove passes");

const leak = problems([f("/Feature/X/LF_B.c", 'input.AddActionListener("A", EActionTrigger.DOWN, OnA);')]);
assert.equal(leak.length, 1, "an add with no remove fails");
assert.match(leak[0], /LF_B\.c/);

// The inventory menu bug: the close path copied the open lines, so the "remove" was another add.
const inventory = problems([f("/Feature/Quantity/SCR_InventoryMenuUI.c", [
  'GetGame().GetInputManager().AddActionListener("LF_Inventory_MoveQuantity", EActionTrigger.UP, LF_OnMoveQuantityPressed);',
  'GetGame().GetInputManager().AddActionListener("LF_Inventory_MoveQuantity", EActionTrigger.DOWN, LF_OnMoveQuantityPressed);',
  'GetGame().GetInputManager().AddActionListener("LF_Inventory_MoveQuantity", EActionTrigger.UP, LF_OnMoveQuantityPressed);',
  'GetGame().GetInputManager().AddActionListener("LF_Inventory_MoveQuantity", EActionTrigger.DOWN, LF_OnMoveQuantityPressed);',
].join("\n"))]);
assert.equal(inventory.length, 2, "add-on-open plus add-on-close fails once per listener");

const wrongTrigger = problems([f("/Feature/X/LF_C.c", 'input.AddActionListener("A", EActionTrigger.UP, OnA);\ninput.RemoveActionListener("A", EActionTrigger.DOWN, OnA);')]);
assert.equal(wrongTrigger.length, 1, "a remove with another trigger does not count");

assert.deepEqual(problems([f("/Feature/X/LF_D.c", '// input.AddActionListener("A", EActionTrigger.DOWN, OnA);')]), [], "a comment is not an add");
assert.deepEqual(problems([f("/Tests/LF_Test_X.c", 'input.AddActionListener("A", EActionTrigger.DOWN, OnA);')]), [], "tests are skipped");
// The once-per-controller files had an allowlist; reconnecting in one process left a second listener
// on a deleted controller. A controller file now removes its listeners like every other file.
const once = 'input.AddActionListener("LF_GiveUp", EActionTrigger.DOWN, LF_OnGiveUpKey);';
for (const p of ["/Feature/Medic/LF_GiveUpController.c", "/Feature/Moderation/LF_ClipStampController.c", "/Feature/Placement/LF_PlacementPlayerController.c", "/Feature/VehicleLock/LF_VehicleLockKeyController.c"]) {
  assert.equal(problems([f(p, once)]).length, 1, `${p}: a once-per-controller add with no remove fails`);
}
assert.deepEqual(problems([f("/Feature/Medic/LF_GiveUpController.c", `${once}
void ~SCR_PlayerController() { input.RemoveActionListener("LF_GiveUp", EActionTrigger.DOWN, LF_OnGiveUpKey); }`)]), [], "a controller that removes in its destructor passes");

console.log("input-listeners-selftest: OK");
