// Every game mode prefab carries the vanilla components vanilla scripts read from it without a null check
// (#5154 follow-up). The list and its evidence are `REQUIRED` in tools/lib/gamemode-components.mjs.
// inputs: addons/**/Prefabs/**/*.et, tools/lib/gamemode-components.mjs
//
// GameMode_Roleplay.et is built from scratch, so it has only the components it lists. It lacked
// SCR_PossessingManagerComponent and every give-up or bleed-out death raised a VME in vanilla
// SCR_CharacterHelper.GetCharacterControlType (#5154, fixed in #5162). A prefab inheriting another game mode
// prefab of ours, or a vanilla one, gets the parent's components; the check follows the chain.
//
// LF_GAMEMODE_ROOT points the scan at another tree (the selftest uses a temp one).
//
// red-proof: 2026-11-27, tools/validation/gamemode-components-selftest.mjs runs this over a tree whose game mode
// prefab lacks SCR_PossessingManagerComponent; it fails naming the prefab and the class, and passes with the row.
import { dirname, join } from "node:path";
import { readFileSync } from "node:fs";
import { fileURLToPath } from "node:url";
import { REQUIRED, findPrefabs, isGameModeRoot, resolve } from "../lib/gamemode-components.mjs";

const root = process.env.LF_GAMEMODE_ROOT || join(dirname(fileURLToPath(import.meta.url)), "..", "..");
const prefabs = findPrefabs(root).filter((rel) => isGameModeRoot(readFileSync(join(root, rel), "utf8")));

const bad = [];
for (const rel of prefabs) {
  const { classes, vanillaParent } = resolve(root, rel);
  if (vanillaParent) continue;
  for (const [cls, why] of Object.entries(REQUIRED)) {
    if (!classes.has(cls)) bad.push(`FAIL ${rel}: no ${cls}. ${why}`);
  }
}

if (bad.length) {
  console.log(bad.join("\n"));
  console.log(`validate-gamemode-components: ${bad.length} missing component(s). Add the row to the game mode prefab (inherited rows count; never shadow one).`);
  process.exit(1);
}
console.log(`validate-gamemode-components: OK (${prefabs.length} game mode prefab(s), ${Object.keys(REQUIRED).length} required component(s))`);
