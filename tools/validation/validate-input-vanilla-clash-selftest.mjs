// Selftest for validate-input-vanilla-clash (#3089): the 2026-11-18 U and F9 bindings clash; the new keys do not.
// red-proof: with `keyboard:KC_U` on LF_VehicleLockToggle and `keyboard:KC_F9` on LF_ClipStamp the real conf fails
// (check 1); with the keys the card moved them to it passes (check 2).
// inputs: addons/LifeFramework/Configs/System/chimeraInputCommon.conf
import { readFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { CONF, FIXTURE, extractFixture, findClashes, parseInputConf } from "./validate-input-vanilla-clash.mjs";

const root = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
const fixture = JSON.parse(readFileSync(join(root, FIXTURE), "utf8"));
const lfText = readFileSync(join(root, CONF), "utf8");
let failed = 0;
function check(name, ok) {
  if (!ok) failed++;
  console.log(`${ok ? "ok  " : "FAIL"} ${name}`);
}

// 1. Today's bindings (U and F9) fail.
const old = lfText
  .replace(/(LF_VehicleLockToggle[\s\S]*?Input "keyboard:)KC_\w+"/, '$1KC_U"')
  .replace(/(Action LF_ClipStamp[\s\S]*?Input "keyboard:)KC_\w+"/, '$1KC_F9"');
const oldClashes = findClashes(old, fixture);
check("U on the vehicle lock clashes with GroupAcceptInvite", oldClashes.some((c) => c.lf === "LF_VehicleLockToggle" && c.key === "KC_U" && c.vanilla === "GroupAcceptInvite"));
check("F9 on the clip stamp clashes with InstantVote", oldClashes.some((c) => c.lf === "LF_ClipStamp" && c.key === "KC_F9" && c.vanilla === "InstantVote"));
check("F9 on the clip stamp clashes with AutotuneRadio", oldClashes.some((c) => c.lf === "LF_ClipStamp" && c.key === "KC_F9" && c.vanilla === "AutotuneRadio"));

// 2. The conf as checked in has no clash.
const now = findClashes(lfText, fixture);
check(`the checked-in conf has no clash (${now.map((c) => `${c.lf}/${c.key}`).join(", ") || "none"})`, now.length === 0);
const lf = parseInputConf(lfText);
const lockKeys = lf.actions.LF_VehicleLockToggle.map((b) => b.key);
const stampKeys = lf.actions.LF_ClipStamp.map((b) => b.key);
check("the vehicle lock no longer takes U", !lockKeys.includes("KC_U") && lockKeys.length > 0);
check("the clip stamp no longer takes F9", !stampKeys.includes("KC_F9") && stampKeys.length > 0);
check("the gamepad bindings stay", /gamepad0:thumb_right/.test(lfText) && /gamepad0:shoulder_right/.test(lfText));

// 3. The parser: a combo key is not a bare key, a new LF action on a used key fails, an allowed one passes.
const sample = `ActionManager {
 Actions {
  Action LF_Test {
   InputSource InputSourceSum "{1}" {
    Sources {
     InputSourceCombo "{2}" {
      Sources {
       InputSourceValue "{3}" {
        Input "keyboard:KC_U"
       }
      }
     }
    }
   }
  }
  Action LF_Test2 {
   InputSource InputSourceValue "{4}" {
    Input "keyboard:KC_U"
   }
  }
 }
 Contexts {
  ActionContext IngameContext {
   ActionRefs + {
    "LF_Test" "LF_Test2"
   }
  }
 }
}`;
const s = findClashes(sample, fixture);
check("a combo binding is not compared", !s.some((c) => c.lf === "LF_Test"));
check("a bare key on a live vanilla key fails", s.some((c) => c.lf === "LF_Test2" && c.key === "KC_U"));
const fx = extractFixture('ActionManager {\n Actions {\n  Action Foo {\n   InputSource InputSourceValue "{1}" {\n    Input "keyboard:KC_Q"\n   }\n  }\n }\n Contexts {\n  ActionContext CarContext {\n   ActionRefs {\n    "Foo"\n   }\n  }\n }\n}');
check("extract reads an action into its context", JSON.stringify(fx) === '{"CarContext":{"Foo":["KC_Q"]}}');

if (failed) {
  console.log(`validate-input-vanilla-clash-selftest: ${failed} check(s) failed.`);
  process.exit(1);
}
console.log("validate-input-vanilla-clash-selftest: OK");
