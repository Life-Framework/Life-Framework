// tools/validation/validate-ui-back-path.mjs
// inputs: addons/**/*.c
//
// Back closes every LF screen through ONE path (LF_UIBack, LF_UIScreen.Back). A script that polls
// MenuBack or activates a menu input context on its own is a screen that drifts from it: the
// maintainer's "pressing back does not close dialogs" was a screen with its own input loop.
//
// Fails when
//   1. a .c outside ALLOWED names "MenuBack" or calls ActivateContext("Menu...") (build the screen
//      with LF_UIScreen.Open and let the kit own Back; a MenuManager menu uses LF_UIMenuInput), or
//   2. a script that opens a kit sheet (LF_UIScreen.Open) builds no CLOSE part (every screen has a
//      visible Close from the kit) and is not listed in CLOSE_EXEMPT / CLOSE_ELSEWHERE, or
//   3. the kit's two Back owners (LF_UIScreen.c, LF_DialogueScene.c) lose their
//      AddActionListener(LF_UIBack.ACTION, ...) registration or their LF_UIBack.Fresh() debounce.
// A screen with no Close names its reason below: a sheet that must not be put away (Back brings it
// back, so a Close would reopen it) or one whose button already is the close.
// red-proof: tools/validation/ui-back-path-selftest.mjs asserts a stray MenuBack poll, a stray
// menu context, a kit sheet with no Close part, and a Back owner without its listener or Backspace guard.

import { existsSync, readFileSync, readdirSync, statSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

// Files that may name MenuBack or a menu input context. Each is a Back owner or injects the press.
export const ALLOWED = [
  "/Feature/UIKit/LF_UIBack.c",
  "/Feature/UIKit/LF_UIScreen.c",
  "/Feature/UIKit/LF_UIMenuInput.c", // MenuManager menus (ATM, Shop, Quest, Police) and widget pickers
  "/Feature/Dialogue/LF_DialogueScene.c",
  "/Feature/Debug/LF_DebugMenu.c", // a MenuManager menu with its own poll (listeners do not fire there)
  "/Feature/AgentHarness/LF_AgentHarness.c", // injects the press
  "/Tests/",
];
// A file that opens a kit sheet whose parts are built elsewhere, and where the CLOSE part is.
export const CLOSE_ELSEWHERE = {
  "/Feature/Signs/LF_SignCard.c": "LF_SignCatalog.BuildParts ends every card with Close",
};
// A kit sheet without a Close, and why. Back still closes (or answers) it through LF_UIScreen.Back.
export const CLOSE_EXEMPT = {
  "/Feature/CharacterCreation/LF_RegistrationForm.c": "cannot be put away: Back brings the form back until it is signed",
  "/Feature/CharacterCreation/LF_CharacterSelectSheet.c": "cannot be put away: Back brings the sheet back until a character is picked",
  "/Feature/Rules/LF_QuizSheet.c": "cannot be put away: Back brings the same page back until the quiz is done",
  "/Feature/Identity/LF_IdCheck.c": "Refuse is its close: Back, and the lapse, answer Refuse",
};
export const OWNERS = ["/Feature/UIKit/LF_UIScreen.c", "/Feature/Dialogue/LF_DialogueScene.c"];

const USES_BACK = /"MenuBack"/;
const USES_CONTEXT = /ActivateContext\(\s*"Menu/;

// files: [{ path, text }]. Returns the failure lines.
export function problems(files) {
  const out = [];
  for (const f of files) {
    const allowed = ALLOWED.some((a) => f.path.includes(a));
    if (!allowed) {
      f.text.split("\n").forEach((line, i) => {
        const code = line.split("//")[0];
        if (USES_BACK.test(code) || USES_CONTEXT.test(code)) {
          out.push(`${f.path}:${i + 1}: reads MenuBack or activates a menu input context outside the UI kit; open the screen with LF_UIScreen.Open so Back closes it through LF_UIBack (docs/style-guide.md "Screens")`);
        }
      });
    }
  }
  for (const f of files) {
    if (f.path.includes("/Tests/") || f.path.includes("/Feature/UIKit/") || !/LF_UIScreen[.]Open[(]/.test(f.text)) continue;
    if (/EUIPartKind[.]CLOSE/.test(f.text)) continue;
    if (Object.keys(CLOSE_ELSEWHERE).some((k) => f.path.includes(k)) || Object.entries(CLOSE_EXEMPT).some(([k, why]) => f.path.includes(k) && why.length >= 15)) continue;
    out.push(`${f.path}: opens a kit sheet with no CLOSE part; add LF_UIPart.Create(LF_EUIPartKind.CLOSE, ...) so the screen has a visible Close, or list it in CLOSE_EXEMPT with the reason`);
  }
  for (const owner of OWNERS) {
    const f = files.find((x) => x.path.includes(owner));
    if (!f) continue;
    if (!/AddActionListener\(\s*LF_UIBack\.ACTION/.test(f.text)) out.push(`${f.path}: no AddActionListener(LF_UIBack.ACTION, ...); the screen would not hear the vanilla MenuBack action`);
    if (owner.endsWith("LF_UIScreen.c") && !/LF_UIBack[.]IsTyping[(]/.test(f.text)) out.push(`${f.path}: Back ignores the Backspace guard; a Backspace in a text field would close the sheet instead of deleting text`);
    if (!/LF_UIBack\.Fresh\(\)/.test(f.text)) out.push(`${f.path}: Back does not go through LF_UIBack.Fresh(); one press could close two screens`);
  }
  return out;
}

function walk(dir, out) {
  for (const name of readdirSync(dir)) {
    const p = join(dir, name);
    if (statSync(p).isDirectory()) walk(p, out);
    else if (name.endsWith(".c")) out.push(p);
  }
  return out;
}

export function scan(root) {
  const out = [];
  const addons = join(root, "addons");
  if (!existsSync(addons)) return out;
  for (const addon of readdirSync(addons)) {
    const dir = join(addons, addon, "Scripts");
    if (!existsSync(dir)) continue;
    for (const p of walk(dir, [])) out.push({ path: p.slice(root.length + 1).replaceAll("\\", "/"), text: readFileSync(p, "utf8") });
  }
  return out;
}

const isMain = process.argv[1] && fileURLToPath(import.meta.url) === process.argv[1];
if (isMain) {
  const root = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
  const files = scan(root);
  const bad = problems(files);
  if (bad.length) {
    console.log("validate-ui-back-path: FAIL");
    for (const b of bad) console.log(`  ${b}`);
    process.exit(1);
  }
  console.log(`validate-ui-back-path: OK (${files.length} scripts, ${OWNERS.length} Back owners)`);
}
