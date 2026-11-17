// Re-shoot the bank's pictures from the committed files alone.
//
//   node tools/buildings/bank/looks.mjs [--set exterior|rooms|all] [--hour 14|22] [--only a,b] [--tag pass2] [--run]
//
// Reads spec.json "looks" (centres in building space, the front is +z) and the kit's placement in the pack grid layer
// (Grid_bank.layer, entity Grid_bank_Main), turns each centre into world space and prints the `tools\cli look` calls.
// With --run it runs them one after the other and copies each result to tmp/look/<tag>-<name>[-night]/, the folders
// `tools\cli evidence` keeps. One call is one Workbench run.
import { spawnSync } from "node:child_process";
import { cpSync, existsSync, mkdirSync, readFileSync, rmSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const ROOT = join(dirname(fileURLToPath(import.meta.url)), "..", "..", "..");
const spec = JSON.parse(readFileSync(join(ROOT, "tools", "buildings", "bank", "spec.json"), "utf8"));
const LAYER = join(ROOT, "addons", "LF_Everon1989", "Worlds", "Everon1989Grid", "Everon1989Grid_Layers", "Grid_bank.layer");
const args = process.argv.slice(2);
const flag = (k, d) => (args.includes(`--${k}`) ? args[args.indexOf(`--${k}`) + 1] : d);
const set = flag("set", "all");
const tag = flag("tag", "shot");
const only = flag("only") ? flag("only").split(",") : null;
const hours = flag("hour") ? [Number(flag("hour"))] : spec.looks.hours;

// The kit's coords and angleY in the layer; building space to world (the yaw turns +z, the front, towards (sin, cos)).
function placement(text, entity) {
  const m = new RegExp(`^[A-Za-z_]+ ${entity} : "[^"]+" \\{\\n((?: [^\\n]*\\n)*?)\\}`, "m").exec(text);
  if (!m) throw new Error(`no entity ${entity} in the layer`);
  const c = /^ coords (\S+) (\S+) (\S+)$/m.exec(m[1]);
  const a = /^ angleY (\S+)$/m.exec(m[1]);
  return { x: +c[1], y: +c[2], z: +c[3], yaw: a ? +a[1] : 0 };
}
function toWorld(p, [bx, by, bz]) {
  const r = (p.yaw * Math.PI) / 180;
  return [p.x + bx * Math.cos(r) + bz * Math.sin(r), p.y + by, p.z - bx * Math.sin(r) + bz * Math.cos(r)].map((n) => Math.round(n * 1000) / 1000);
}

const p = placement(readFileSync(LAYER, "utf8"), "Grid_bank_Main");
const list = [];
for (const hour of hours) {
  const night = hour >= 18 || hour < 6;
  const h = hour === 14 ? "" : ` --hour ${hour}`;
  if (set === "exterior" || set === "all") list.push({ name: `exterior${night ? "-night" : ""}`, cmd: `look grid:bank --close 0${h}` });
  if (set === "rooms" || set === "all") {
    for (const r of spec.looks.rooms) {
      if (night && r.day) continue;
      const [x, y, z] = toWorld(p, r.at);
      list.push({ name: `${r.name}${night ? "-night" : ""}`, cmd: `look grid:bank --interior ${x},${y},${z} --radius ${r.radius} --close 0${h}`, shot: r.name });
    }
  }
}
const chosen = only ? list.filter((c) => only.includes(c.name)) : list;
for (const c of chosen) console.log(`tools\\cli ${c.cmd}   -> tmp/look/${tag}-${c.name}`);
if (args.includes("--run")) {
  for (const c of chosen) {
    const res = spawnSync(join(ROOT, "tools", "cli.cmd"), c.cmd.split(" "), { cwd: ROOT, stdio: "inherit", shell: true });
    const src = join(ROOT, "tmp", "look", c.shot ? "grid-bank-interior" : "grid-bank");
    const dst = join(ROOT, "tmp", "look", `${tag}-${c.name}`);
    if (res.status === 0 && existsSync(src)) {
      rmSync(dst, { recursive: true, force: true });
      mkdirSync(dirname(dst), { recursive: true });
      cpSync(src, dst, { recursive: true });
    }
  }
}
