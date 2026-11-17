// Re-shoot the hostel's pictures from the committed files alone.
//
//   node tools/buildings/hostel/looks.mjs [--set exterior|rooms|all] [--hour 14|22] [--tag before] [--run]
//
// Reads spec.json "looks" (room centres in building space, the front is +z) and the building's placement in its pack
// grid layer (spec.grid.layer, entity Grid_hostel_Main), turns each centre into world space and prints the
// `tools\cli look` calls. With --run it runs them one after the other and copies each result to
// tmp/look/<tag>-<name>[-night]/, the folders `tools\cli evidence` keeps.
import { spawnSync } from "node:child_process";
import { cpSync, existsSync, readFileSync, rmSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const ROOT = join(dirname(fileURLToPath(import.meta.url)), "..", "..", "..");
const spec = JSON.parse(readFileSync(join(ROOT, "tools", "buildings", "hostel", "spec.json"), "utf8"));
const args = process.argv.slice(2);
const flag = (k, d) => (args.includes(`--${k}`) ? args[args.indexOf(`--${k}`) + 1] : d);
const set = flag("set", "all");
const tag = flag("tag", "shot");
const hours = flag("hour") ? [Number(flag("hour"))] : spec.looks.hours;
const ENTITY = "Grid_hostel_Main";

function placement(text) {
  const m = new RegExp(`^[A-Za-z_]+ ${ENTITY} : "[^"]+" \\{\\n((?: [^\\n]*\\n)*?)\\}`, "m").exec(text);
  if (!m) throw new Error(`no entity ${ENTITY} in ${spec.grid.layer}`);
  const c = /^ coords (\S+) (\S+) (\S+)$/m.exec(m[1]);
  const a = /^ angleY (\S+)$/m.exec(m[1]);
  return { x: +c[1], y: +c[2], z: +c[3], yaw: a ? +a[1] : 0 };
}

// Building space to world space: the building's yaw turns +z (front) towards (sin, cos).
function toWorld(p, [bx, by, bz]) {
  const r = (p.yaw * Math.PI) / 180;
  const s = Math.sin(r);
  const c = Math.cos(r);
  return [p.x + bx * c + bz * s, p.y + by, p.z - bx * s + bz * c].map((n) => Math.round(n * 1000) / 1000);
}

const p = placement(readFileSync(join(ROOT, spec.grid.layer), "utf8"));
const out = [];
for (const hour of hours) {
  const night = hour >= 18 || hour < 6;
  const h = hour === 14 ? "" : ` --hour ${hour}`;
  if (set === "exterior" || set === "all") out.push({ name: `exterior${night ? "-night" : ""}`, cmd: `look grid:hostel --close 0${h}`, dir: "grid-hostel" });
  if (set === "rooms" || set === "all") {
    for (const r of spec.looks.rooms) {
      const [x, y, z] = toWorld(p, r.at);
      out.push({ name: `${r.name}${night ? "-night" : ""}`, cmd: `look grid:hostel --interior ${x},${y},${z} --radius ${r.radius} --close 0${h}`, dir: "grid-hostel-interior" });
    }
  }
}

for (const c of out) {
  console.log(`${c.name.padEnd(18)} tools\\cli ${c.cmd}`);
  if (!args.includes("--run")) continue;
  const r = spawnSync(join(ROOT, "tools", "cli.cmd"), c.cmd.split(" "), { cwd: ROOT, stdio: "inherit", shell: true, windowsHide: true });
  if (r.status !== 0) {
    console.error(`look ${c.name} failed (exit ${r.status})`);
    process.exit(1);
  }
  const from = join(ROOT, "tmp", "look", c.dir);
  const to = join(ROOT, "tmp", "look", `${tag}-${c.name}`);
  if (existsSync(to)) rmSync(to, { recursive: true });
  cpSync(from, to, { recursive: true });
}
