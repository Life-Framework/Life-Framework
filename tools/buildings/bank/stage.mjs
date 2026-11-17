// The bank's layer rows beside the kit: the two tellers and the rob kit chest, in the pack grid layer.
//
//   node tools/buildings/bank/stage.mjs           rewrite the rows in Everon1989Grid_Layers/Grid_bank.layer
//   node tools/buildings/bank/stage.mjs --check   exit 1 when the layer's rows are not what this script writes
//
// `tools\cli grid place bank Prefabs/Buildings/Bank/Bank_Kit.et --pack` writes the cell (yard, sign, the kit). The tellers
// are layer entities, not kit children (the clerk precedent: an NPC is spawned by the world, not by a building), so
// `town fit` carries them as kit rows too. Their spots are building space (layout.mjs) turned into the world by the
// kit row's own coords and yaw. Rows are named Grid_bank_Stage_*, so a re-run replaces its own block.
import { existsSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { F, npcs, robKit } from "./layout.mjs";

const ROOT = join(dirname(fileURLToPath(import.meta.url)), "..", "..", "..");
export const LAYER = join(ROOT, "addons", "LF_Everon1989", "Worlds", "Everon1989Grid", "Everon1989Grid_Layers", "Grid_bank.layer");
const ROBKIT = "{6EAEEC207EBAB7C5}Prefabs/Props/Debug/LF_BankRobKitStand.et";
const n = (v) => String(+(+v).toFixed(3));

if (!existsSync(LAYER)) throw new Error("no Grid_bank.layer: run tools\\cli grid place bank Prefabs/Buildings/Bank/Bank_Kit.et --pack first");
const text = readFileSync(LAYER, "utf8");
const kitRow = /^\w+ \w+ : "\{[0-9A-F]{16}\}Prefabs\/Buildings\/Bank\/Bank_Kit\.et" \{\n((?: .*\n)*?)\}/m.exec(text);
if (!kitRow) throw new Error("Grid_bank.layer has no Bank_Kit row");
const prop = (k) => new RegExp(`^ ${k} (.+)$`, "m").exec(kitRow[1])?.[1];
const [kx, ky, kz] = prop("coords").split(" ").map(Number);
const kyaw = Number(prop("angleY") ?? 0);
const rad = (kyaw * Math.PI) / 180;
// building space to world: Enfusion's yaw turns +z towards +x
const world = (x, y, z) => [kx + x * Math.cos(rad) + z * Math.sin(rad), ky + y, kz - x * Math.sin(rad) + z * Math.cos(rad)];

const rows = [];
for (const p of npcs) {
  const [x, y, z] = world(p.x, p.y, p.z);
  rows.push(`LF_NpcBaseEntity Grid_bank_Stage_${p.name} : "${p.prefab}" {\n coords ${n(x)} ${n(y)} ${n(z)}\n angleY ${n((p.yaw + kyaw) % 360)}\n}`);
}
{
  const [x, y, z] = world(robKit.x, F, robKit.z);
  rows.push(`GenericEntity Grid_bank_Stage_RobKit : "${ROBKIT}" {\n coords ${n(x)} ${n(y)} ${n(z)}\n angleY ${n((robKit.yaw + kyaw) % 360)}\n}`);
}
const kept = text.replace(/^\w+ Grid_bank_Stage_\w+ : "[^"]+" \{\n(?: .*\n)*?\}\n/gm, "");
const next = `${kept.trimEnd()}\n${rows.join("\n")}\n`;
if (process.argv.includes("--check")) {
  console.log(next === text ? "bank stage: current" : "bank stage: DRIFT (run node tools/buildings/bank/stage.mjs)");
  process.exit(next === text ? 0 : 1);
}
writeFileSync(LAYER, next);
console.log(`bank stage: ${rows.length} row(s) written to Grid_bank.layer (kit at ${kx}, ${kz}, yaw ${kyaw})`);
