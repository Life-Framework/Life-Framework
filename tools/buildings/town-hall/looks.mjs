// Re-shoot the town hall's pictures from the committed files alone.
//
//   node tools/buildings/town-hall/looks.mjs [--set exterior|rooms|all] [--rooms a,b] [--hour 14|22] [--tag pass1] [--run]
//
// The building stands in the pack grid (Grid_town_hall.layer in LF_Everon1989's Everon1989Grid, cell c11r2, PR #2277).
//
// Reads spec.json "looks" (centres in building space, the front is +z) and the building's placement in the grid
// layer (spec.grid), turns each centre into world space and prints the `tools\cli look` calls. With --run it runs them
// one after the other and copies each result to tmp/look/<tag>-<name>[-night]/ (default tag "shot"), the folders
// `tools\cli evidence` keeps. One call is one Workbench run, so a pass takes the exterior plus the rooms it changed.
import { spawnSync } from "node:child_process";
import { cpSync, existsSync, mkdirSync, readFileSync, rmSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const ROOT = join(dirname(fileURLToPath(import.meta.url)), "..", "..", "..");
const spec = JSON.parse(readFileSync(join(ROOT, "tools", "buildings", "town-hall", "spec.json"), "utf8"));
const args = process.argv.slice(2);
const flag = (k, d) => (args.includes(`--${k}`) ? args[args.indexOf(`--${k}`) + 1] : d);
const set = flag("set", "all");
const tag = flag("tag", "shot");
const only = flag("rooms") ? flag("rooms").split(",") : null; // --rooms corridor,first-floor: just those rooms
const hours = flag("hour") ? [Number(flag("hour"))] : spec.looks.hours;

// The placement: the building entity's coords and angleY in its grid layer.
export function placement(text, entity) {
  const m = new RegExp(`^[A-Za-z_]+ ${entity} : "[^"]+" \\{\\n((?: [^\\n]*\\n)*?)\\}`, "m").exec(text);
  if (!m) throw new Error(`no entity ${entity} in the layer`);
  const c = /^ coords (\S+) (\S+) (\S+)$/m.exec(m[1]);
  const a = /^ angleY (\S+)$/m.exec(m[1]);
  return { x: +c[1], y: +c[2], z: +c[3], yaw: a ? +a[1] : 0 };
}

// Building space to world space: the building's yaw turns +z (front) towards (sin, cos).
export function toWorld(p, [bx, by, bz]) {
  const r = (p.yaw * Math.PI) / 180;
  const s = Math.sin(r);
  const c = Math.cos(r);
  return [p.x + bx * c + bz * s, p.y + by, p.z - bx * s + bz * c].map((n) => Math.round(n * 1000) / 1000);
}

export function calls(p) {
  const out = [];
  for (const hour of hours) {
    const night = hour >= 18 || hour < 6;
    const h = hour === 14 ? "" : ` --hour ${hour}`;
    if (set === "exterior" || set === "all") out.push({ name: `exterior${night ? "-night" : ""}`, cmd: `look grid:town-hall --close 0${h}` });
    if (set === "rooms" || set === "all") {
      for (const r of spec.looks.rooms.filter((r) => !only || only.includes(r.name))) {
        const [x, y, z] = toWorld(p, r.at);
        out.push({ name: `${r.name}${night ? "-night" : ""}`, cmd: `look grid:town-hall --interior ${x},${y},${z} --radius ${r.radius} --close 0${h}`, shot: r.name });
      }
    }
  }
  return out;
}

const layer = readFileSync(join(ROOT, spec.grid.layer), "utf8");
const list = calls(placement(layer, spec.grid.entity));
for (const c of list) console.log(`tools\\cli ${c.cmd}   -> tmp/look/${tag}-${c.name}`);
if (args.includes("--run")) {
  for (const c of list) {
    const res = spawnSync(join(ROOT, "tools", "cli.cmd"), c.cmd.split(" "), { cwd: ROOT, stdio: "inherit", shell: true });
    const src = join(ROOT, "tmp", "look", c.shot ? "grid-town-hall-interior" : "grid-town-hall");
    const dst = join(ROOT, "tmp", "look", `${tag}-${c.name}`);
    if (res.status === 0 && existsSync(src)) {
      rmSync(dst, { recursive: true, force: true });
      mkdirSync(dirname(dst), { recursive: true });
      cpSync(src, dst, { recursive: true });
    }
  }
}
