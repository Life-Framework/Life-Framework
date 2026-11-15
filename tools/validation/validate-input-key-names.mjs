// No input key names the engine rejects in addon .conf files (#1028 boot log).
// inputs: addons/**/*.conf
//
// The engine accepts only its own spellings: `Input "gamepad0:trigger_left"` boots with
// `INPUT (E): InputManager: unknown key 'gamepad0:trigger_left'` and the binding is dead.
// The list holds spellings the engine has refused in a real boot log, each with the name
// the base game's chimeraInputCommon.conf uses. Add a row when a boot log names a new one.
//
// red-proof: 2026-11-14, `Input "keyboard:KC_BACKSPACE"` in chimeraInputCommon.conf printed
// `FAIL addons/LifeFramework/Configs/System/chimeraInputCommon.conf:196 ...` and exited 1;
// with `keyboard:KC_BACK` it prints OK.
import { execFileSync } from "node:child_process";
import { existsSync, readFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const root = join(dirname(fileURLToPath(import.meta.url)), "..", "..");

const REFUSED = {
  "gamepad0:trigger_left": "gamepad0:left_trigger",
  "gamepad0:trigger_right": "gamepad0:right_trigger",
  "keyboard:KC_BACKSPACE": "keyboard:KC_BACK",
};

const files = execFileSync("git", ["ls-files", "-z", "--cached", "--others", "--exclude-standard", "addons"], {
  cwd: root,
  encoding: "utf8",
  maxBuffer: 1 << 28,
}).split("\0").filter((f) => f.endsWith(".conf"));

const bad = [];
for (const f of files) {
  if (!existsSync(join(root, f))) continue;
  readFileSync(join(root, f), "utf8").split("\n").forEach((line, i) => {
    for (const m of line.matchAll(/"((?:keyboard|gamepad0|mouse):[A-Za-z_0-9]+)"/g)) {
      if (REFUSED[m[1]]) bad.push(`FAIL ${f}:${i + 1} '${m[1]}' is not an engine key; use '${REFUSED[m[1]]}'`);
    }
  });
}

if (bad.length) {
  console.log(bad.join("\n"));
  console.log(`validate-input-key-names: ${bad.length} refused key name(s).`);
  process.exit(1);
}
console.log(`validate-input-key-names: OK (${files.length} conf file(s))`);
