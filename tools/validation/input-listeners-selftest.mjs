// tools/validation/input-listeners-selftest.mjs — red proof for validate-input-listeners.
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
assert.deepEqual(problems([f("/Feature/Medic/LF_GiveUpController.c", 'input.AddActionListener("LF_GiveUp", EActionTrigger.DOWN, LF_OnGiveUpKey);')]), [], "a listed once-per-controller listener passes");

console.log("input-listeners-selftest: OK");
