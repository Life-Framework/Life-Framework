// Every player-facing LF_ action bound in chimeraInputCommon.conf has a row in keyBindingMenu.conf (#2510).
// inputs: addons/LifeFramework/Configs/System/chimeraInputCommon.conf, addons/LifeFramework/Configs/System/keyBindingMenu.conf
//
// An action missing from the keybinding menu cannot be rebound from the settings screen. The siren and
// emote actions shipped that way. Internal actions are listed in INTERNAL with the reason they stay hidden;
// an entry there that is no longer bound fails too, so the list cannot rot.
//
// red-proof: tools/validation/keybinding-menu-selftest.mjs (a menu missing one bound action fails, a stale
// allowlist entry fails, the real files pass). 2026-11-17: before the fix the real files failed on
// LF_CycleSirenModesSingle, LF_CycleSirenModesDouble and LF_OpenEmoteMenu.
import { readFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const KEYPAD = "on-screen keypad digit; driven by the keypad widget, not a player-chosen key";
export const INTERNAL = {
  LF_Keypad0: KEYPAD, LF_Keypad1: KEYPAD, LF_Keypad2: KEYPAD, LF_Keypad3: KEYPAD, LF_Keypad4: KEYPAD,
  LF_Keypad5: KEYPAD, LF_Keypad6: KEYPAD, LF_Keypad7: KEYPAD, LF_Keypad8: KEYPAD, LF_Keypad9: KEYPAD,
  LF_KeypadBack: "on-screen keypad back key; menu-context only",
  LF_KeypadClear: "on-screen keypad clear key; menu-context only",
  LF_UIBackspace: "text-edit backspace for LF menus; menu-context only",
};

// LF_ action names declared in chimeraInputCommon.conf (`Action LF_X {`).
export function boundActions(inputText) {
  return [...inputText.matchAll(/^\s*Action\s+(LF_\w+)\s*\{/gm)].map((m) => m[1]);
}

// LF_ action names with a keybinding-menu row (`actionName "LF_X"`).
export function menuActions(menuText) {
  return [...menuText.matchAll(/actionName\s+"(LF_\w+)"/g)].map((m) => m[1]);
}

// FAIL lines for bound actions with no menu row and for allowlist entries that are not bound.
export function menuProblems(inputText, menuText, internal = INTERNAL) {
  const bound = boundActions(inputText);
  const inMenu = new Set(menuActions(menuText));
  const out = [];
  for (const a of bound) {
    if (!inMenu.has(a) && !internal[a]) {
      out.push(`FAIL ${a} is bound in chimeraInputCommon.conf but has no row in keyBindingMenu.conf: add a row, or list it in INTERNAL with a reason`);
    }
  }
  for (const a of Object.keys(internal)) {
    if (!bound.includes(a)) out.push(`FAIL INTERNAL lists ${a} but chimeraInputCommon.conf does not bind it: remove it`);
    else if (inMenu.has(a)) out.push(`FAIL INTERNAL lists ${a} but it has a menu row: remove it from INTERNAL`);
  }
  return out;
}

if (process.argv[1] && process.argv[1].split("\\").join("/").endsWith("validate-keybinding-menu.mjs")) {
  const dir = join(dirname(fileURLToPath(import.meta.url)), "..", "..", "addons", "LifeFramework", "Configs", "System");
  const bad = menuProblems(
    readFileSync(join(dir, "chimeraInputCommon.conf"), "utf8"),
    readFileSync(join(dir, "keyBindingMenu.conf"), "utf8"),
  );
  if (bad.length) {
    console.log(bad.join("\n"));
    console.log(`validate-keybinding-menu: ${bad.length} problem(s).`);
    process.exit(1);
  }
  console.log("validate-keybinding-menu: OK");
}
