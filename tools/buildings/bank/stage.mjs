// Stage the bank kit in GridWorld for `look`, or take it out again.
//
//   node tools/buildings/bank/stage.mjs [<prefab>] [--yaw A] [--at X,Z]   write the staging layer
//   node tools/buildings/bank/stage.mjs --remove                           delete it (always before a commit)
//
// The kit is pack content (LF_Everon1989) and the GridWorld layers are core, so a committed grid layer must not
// name it (validate-addon-boundary: pack-side grid layers do not exist yet, three-worlds.md). The staging layer
// is a local, uncommitted file in GridWorld_Layers; `tools\cli look <x,z> --world <GridWorld.ent>` then shows
// the real kit. <prefab> defaults to the pack's Bank/Bank.et (building, furniture, vault and interactables);
// pass `vanilla` for the base-game Office_E_01 alone. Default spot: 300,300, flat ground outside the grid.
import { existsSync, readFileSync, unlinkSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { npcs, vault } from "./layout.mjs";
import { fileURLToPath } from "node:url";

const ROOT = join(dirname(fileURLToPath(import.meta.url)), "..", "..", "..");
export const STAGE_LAYER = join(ROOT, "addons", "LifeFramework", "Worlds", "GridWorld", "GridWorld_Layers", "Grid_zz_bankstage.layer");
const args = process.argv.slice(2);
const flag = (n, d) => { const i = args.indexOf(n); return i >= 0 ? args[i + 1] : d; };

if (args.includes("--remove")) {
  if (existsSync(STAGE_LAYER)) unlinkSync(STAGE_LAYER);
  console.log("bank-stage: removed");
  process.exit(0);
}
const name = args.find((a, i) => !a.startsWith("--") && !["--yaw", "--at"].includes(args[i - 1])) ?? "Bank";
const guid = (rel) => /Name "\{([0-9A-F]{16})\}/.exec(readFileSync(join(ROOT, "addons", "LF_Everon1989", rel + ".meta"), "utf8"))[1];
let cls = "GenericEntity";
let ref;
if (name === "vanilla") {
  cls = "SCR_DestructibleBuildingEntity";
  ref = "{51F233A0BA73532A}Prefabs/Structures/Industrial/Houses/Office_E_01/Office_E_01.et";
} else {
  const rel = name === "Bank" ? "Prefabs/Buildings/Bank/Bank_Kit.et" : name === "shell" ? "Prefabs/Buildings/Bank/Bank.et" : name;
  if (name === "shell" || name === "Bank") cls = "SCR_DestructibleBuildingEntity";
  ref = `{${guid(rel)}}${rel}`;
}
const [x, z] = flag("--at", "300,300").split(",").map(Number);
const yaw = Number(flag("--yaw", "0"));
// the kit also needs its tellers (layer entities) and, for the robbery proof, the rob kit chest; building space to world
const rad = (yaw * Math.PI) / 180;
const world = (lx, lz) => [x + lx * Math.cos(rad) + lz * Math.sin(rad), z - lx * Math.sin(rad) + lz * Math.cos(rad)];
let extra = "";
if (name === "Bank") {
  for (const n of npcs) {
    const [wx, wz] = world(n.x, n.z);
    extra += `LF_NpcBaseEntity Grid_zz_bankstage_${n.name} : "${n.prefab}" {
 coords ${wx.toFixed(3)} ${n.y} ${wz.toFixed(3)}
 angleY ${(n.yaw + yaw) % 360}
}
`;
  }
  const [kx, kz] = world(vault.x, vault.z + 1.8);
  extra += `GenericEntity Grid_zz_bankstage_RobKit : "{6EAEEC207EBAB7C5}Prefabs/Props/Debug/LF_BankRobKitStand.et" {
 coords ${kx.toFixed(3)} 0 ${kz.toFixed(3)}
 angleY ${yaw}
}
`;
}
writeFileSync(STAGE_LAYER, `// Local staging (never committed): tools/buildings/bank/stage.mjs\n${cls} Grid_zz_bankstage_Kit : "${ref}" {\n coords ${x} 0 ${z}\n${yaw ? ` angleY ${yaw}\n` : ""}}\n${extra}`);
console.log(`bank-stage: ${name} at ${x},${z} yaw ${yaw}`);
