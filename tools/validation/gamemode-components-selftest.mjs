// tools/validation/gamemode-components-selftest.mjs - validate-gamemode-components (#5154 follow-up).
// inputs: tools/validation/validate-gamemode-components.mjs, tools/lib/gamemode-components.mjs, tools/validation/gamemode-components-selftest.mjs
//
// Runs the validator over a throwaway tree under the OS temp dir:
//   - a game mode prefab without SCR_PossessingManagerComponent fails, naming prefab and class;
//   - the row on the prefab itself, or on a parent prefab of ours, passes;
//   - a child of a vanilla prefab passes (the base game's own components come with it);
//   - a row buried inside another component's block (a property, not a component) does not count;
//   - a prefab that is not a game mode is not its business.
//
// red-proof: 2026-11-27 with the validator loop skipping the REQUIRED check, "a game mode without the row fails"
// printed FAIL.
import { spawnSync } from "node:child_process";
import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { componentsOf } from "../lib/gamemode-components.mjs";
import { scratchDir } from "../lib/scratch.mjs";

let failed = 0;
const check = (ok, msg) => {
  console.log(`${ok ? "PASS" : "FAIL"}  ${msg}`);
  if (!ok) failed++;
};

const here = dirname(fileURLToPath(import.meta.url));
const validator = join(here, "validate-gamemode-components.mjs");
const dir = scratchDir("lf-gamemode-components-");
const modes = join(dir, "addons", "LifeFramework", "Prefabs", "MP", "Modes");
mkdirSync(modes, { recursive: true });
const put = (name, text) => writeFileSync(join(modes, name), text);
const run = () => spawnSync(process.execPath, [validator], { env: { ...process.env, LF_GAMEMODE_ROOT: dir }, encoding: "utf8" });

const POSSESS = '  SCR_PossessingManagerComponent "{9A5625D5A6562E4B}" {\n  }\n';
const mode = (extra) =>
  `LF_GameModeRoleplay {\n ID "59751433747FB462"\n components {\n  Persistence "{2A52ABC1969D9E91}" {\n  }\n${extra}  RplComponent "{597514327E500A0C}" {\n   Streamable Disabled\n  }\n }\n coords 0 0 0\n}\n`;

try {
  const sample = mode(POSSESS) + "";
  check(componentsOf(sample).join() === "Persistence,SCR_PossessingManagerComponent,RplComponent", `componentsOf lists the top-level rows (${componentsOf(sample).join()})`);
  check(!componentsOf('X {\n components {\n  A "{0000000000000001}" {\n   SCR_PossessingManagerComponent "{0000000000000002}" {\n   }\n  }\n }\n}\n').includes("SCR_PossessingManagerComponent"), "a nested row is not a component");
  check(componentsOf('X {\n components {\n  A "{0000000000000001}" {\n   m_rPrefab "{0000000000000002}Prefabs/a.et"\n  }\n }\n}\n').join() === "A", "a property with a GUID path is not a component");

  put("GameMode_Roleplay.et", mode(""));
  let r = run();
  check(r.status === 1 && /GameMode_Roleplay\.et: no SCR_PossessingManagerComponent/.test(r.stdout), `a game mode without the row fails (${r.stdout.trim().split("\n")[0]})`);

  put("GameMode_Roleplay.et", mode(POSSESS));
  r = run();
  check(r.status === 0, `the row on the prefab passes (${r.stdout.trim()})`);

  put("GameMode_Roleplay_Debug.et", 'LF_GameModeRoleplay : "{03913608C59017A3}Prefabs/MP/Modes/GameMode_Roleplay.et" {\n ID "7F729501AA82DA9A"\n}\n');
  r = run();
  check(r.status === 0, "a child prefab inherits the parent's row");

  put("GameMode_Roleplay.et", mode(""));
  r = run();
  check(r.status === 1 && /GameMode_Roleplay_Debug\.et: no SCR_PossessingManagerComponent/.test(r.stdout), "a child of a game mode that lacks the row fails too");

  put("GameMode_Roleplay.et", mode('  A "{0000000000000001}" {\n   SCR_PossessingManagerComponent "{0000000000000002}" {\n   }\n  }\n'));
  r = run();
  check(r.status === 1, "a row nested inside another component does not count");

  rmSync(join(modes, "GameMode_Roleplay.et"));
  put("GameMode_Roleplay_Debug.et", 'LF_GameModeRoleplay : "{03913608C59017A3}Prefabs/MP/Modes/GameMode_Plain.et" {\n ID "7F729501AA82DA9A"\n}\n');
  r = run();
  check(r.status === 0, "a child of a vanilla game mode passes");

  put("Chat_Roleplay.et", 'ScriptedChatEntity {\n components {\n }\n}\n');
  r = run();
  check(r.status === 0, "a prefab that is not a game mode is ignored");
} finally {
  rmSync(dir, { recursive: true, force: true });
}

if (failed) {
  console.log(`gamemode-components-selftest: ${failed} check(s) failed`);
  process.exit(1);
}
console.log("gamemode-components-selftest: OK");
