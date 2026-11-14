// A prefab that overrides a vanilla loadout slot uses the vanilla slot class (issue #976).
//
// BaseLoadoutManagerComponent keys its slots by name, but the override has to be the same class the
// base declares: since 1.8 the base boots slot is SCR_FeetLoadoutSlotInfo, and a `LoadoutSlotInfo
// Boots` override does not replace it (a second slot, or an NPC that counts as barefoot and stumbles).
// The table below is read from Prefabs/Characters/Core/Character_Base.et (`game_read`, game 1.9.0.37);
// add a row when a game update changes a slot's class.
//
// red-proof: 2026-11-14, with `LoadoutSlotInfo Boots` put back in Npc_Default_01.et this printed
// "FAIL addons/LifeFramework/Prefabs/Characters/Npcs/Npc_Default_01.et:15 Boots is LoadoutSlotInfo,
// the base uses SCR_FeetLoadoutSlotInfo" and exited 1.
import { execFileSync } from "node:child_process";
import { existsSync, readFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

export const VANILLA_SLOT_CLASS = {
  Boots: "SCR_FeetLoadoutSlotInfo",
  Hat: "LoadoutSlotInfo",
  Jacket: "LoadoutSlotInfo",
  ArmoredVest: "LoadoutSlotInfo",
  Pants: "LoadoutSlotInfo",
  Back: "LoadoutSlotInfo",
  Vest: "LoadoutSlotInfo",
  HandWear: "LoadoutSlotInfo",
};

// Every `<Class> <Slot> {` line naming a vanilla slot in a prefab's text whose class is not the vanilla one.
export function wrongSlotClasses(text) {
  const out = [];
  text.split("\n").forEach((line, i) => {
    const m = /^\s*(\w*LoadoutSlotInfo)\s+(\w+)\s*\{/.exec(line);
    if (!m) return;
    const want = VANILLA_SLOT_CLASS[m[2]];
    if (want && want !== m[1]) out.push({ line: i + 1, slot: m[2], have: m[1], want });
  });
  return out;
}

function main() {
  const root = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
  const files = execFileSync("git", ["ls-files", "-z", "--cached", "--others", "--exclude-standard", "addons"], {
    cwd: root,
    encoding: "utf8",
    maxBuffer: 1 << 28,
  }).split("\0").filter((f) => f.endsWith(".et") && existsSync(join(root, f)));
  const bad = [];
  for (const f of files) {
    for (const w of wrongSlotClasses(readFileSync(join(root, f), "utf8"))) {
      bad.push(`FAIL ${f}:${w.line} ${w.slot} is ${w.have}, the base uses ${w.want}`);
    }
  }
  if (bad.length) {
    console.log(bad.slice(0, 60).join("\n"));
    if (bad.length > 60) console.log(`... ${bad.length - 60} more`);
    console.log(`validate-loadout-slot-class: ${bad.length} override(s) with the wrong slot class. For generated outfits fix tools/outfits/generate.mjs and run tools\\cli outfits generate.`);
    process.exit(1);
  }
  console.log(`validate-loadout-slot-class: OK (${files.length} prefab(s))`);
}

if (process.argv[1] && fileURLToPath(import.meta.url) === process.argv[1]) main();
