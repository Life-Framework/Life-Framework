// tools/validation/ui-back-path-selftest.mjs — red proof for validate-ui-back-path.
import assert from "node:assert/strict";
import { problems } from "./validate-ui-back-path.mjs";

const owner = (p, extra = "") => ({ path: `addons/LifeFramework/Scripts/Game${p}`, text: `input.AddActionListener(LF_UIBack.ACTION, EActionTrigger.DOWN, X); LF_UIBack.Fresh();${extra}` });
const good = [owner("/Feature/UIKit/LF_UIScreen.c", " LF_UIBack.IsTyping(IsFieldFocused());"), owner("/Feature/Dialogue/LF_DialogueScene.c")];

assert.deepEqual(problems(good), [], "owners with a listener and the debounce pass");

const stray = problems([...good, { path: "addons/LifeFramework/Scripts/Game/Feature/X/LF_Menu.c", text: 'if (input.GetActionTriggered("MenuBack")) Close();' }]);
assert.equal(stray.length, 1, "a screen polling MenuBack on its own fails");
assert.match(stray[0], /LF_Menu\.c:1/);

const ctx = problems([...good, { path: "addons/LifeFramework/Scripts/Game/Feature/X/LF_Sheet.c", text: 'input.ActivateContext("MenuContext");' }]);
assert.equal(ctx.length, 1, "a screen with its own menu input context fails");

assert.deepEqual(problems([...good, { path: "addons/LifeFramework/Scripts/Game/Feature/X/LF_Ok.c", text: '// MenuBack is handled by the kit\nvoid F() {}' }]), [], "a comment is not a poll");
assert.deepEqual(problems([...good, { path: "addons/LifeFramework/Scripts/Game/Tests/LF_Test_A.c", text: 'input.GetActionTriggered("MenuBack");' }]), [], "tests may name MenuBack");

const noListener = problems([{ path: "addons/LifeFramework/Scripts/Game/Feature/UIKit/LF_UIScreen.c", text: "LF_UIBack.Fresh(); LF_UIBack.IsTyping(x);" }, good[1]]);
assert.equal(noListener.length, 1, "an owner without the listener fails");
const noDebounce = problems([good[0], { path: "addons/LifeFramework/Scripts/Game/Feature/Dialogue/LF_DialogueScene.c", text: "input.AddActionListener(LF_UIBack.ACTION, a, b);" }]);
assert.equal(noDebounce.length, 1, "an owner without the debounce fails");

const sheet = (path, text) => ({ path: `addons/LifeFramework/Scripts/Game${path}`, text });
assert.deepEqual(problems([...good, sheet("/Feature/X/LF_A.c", "LF_UIScreen.Open(p); LF_UIPart.Create(LF_EUIPartKind.CLOSE, \"Close\", k);")]), [], "a sheet with a Close passes");
const noClose = problems([...good, sheet("/Feature/X/LF_B.c", "LF_UIScreen.Open(p);")]);
assert.equal(noClose.length, 1, "a kit sheet with no Close fails");
assert.match(noClose[0], /no CLOSE part/);
assert.deepEqual(problems([...good, sheet("/Feature/Rules/LF_QuizSheet.c", "LF_UIScreen.Open(p);")]), [], "a listed mandatory sheet passes");
assert.deepEqual(problems([...good, sheet("/Feature/Signs/LF_SignCard.c", "LF_UIScreen.Open(p);")]), [], "a sheet whose catalog builds the Close passes");
assert.deepEqual(problems([...good, sheet("/Tests/LF_Test_X.c", "LF_UIScreen.Open(p);")]), [], "tests may open a sheet without a Close");

const noGuard = problems([owner("/Feature/UIKit/LF_UIScreen.c"), good[1]]);
assert.equal(noGuard.length, 1, "Back without the Backspace guard fails");
assert.match(noGuard[0], /Backspace guard/);

console.log("ui-back-path-selftest: OK");
