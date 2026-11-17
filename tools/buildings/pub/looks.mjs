// Prints (or with --run, runs) the `tools\cli look` calls for the pub's grid cell: the exterior, then each room of
// spec.json `looks.rooms` turned from building space into world space by the grid entity's coords and angleY in
// Grid_pub.layer. Each room's pictures are copied to tmp/look/pub-<tag>-<room> because `look` reuses one folder.
//
//   node tools/buildings/pub/looks.mjs [--hour 22] [--tag before] [--run]
import { spawnSync } from "node:child_process";
import { cpSync, existsSync, readFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { toWorld } from "../pipeline/scenario.mjs";

const ROOT = join(dirname(fileURLToPath(import.meta.url)), "..", "..", "..");
const arg = (k, d) => (process.argv.includes(k) ? process.argv[process.argv.indexOf(k) + 1] : d);
const hour = arg("--hour", null);
const tag = arg("--tag", "look");
const spec = JSON.parse(readFileSync(join(ROOT, "tools", "buildings", "pub", "spec.json"), "utf8"));
const layer = readFileSync(join(ROOT, spec.grid.layer), "utf8");
const m = /Grid_pub_Main : "[^"]+" \{\n coords ([-\d.]+) ([-\d.]+) ([-\d.]+)\n(?: angleY ([-\d.]+)\n)?/.exec(layer);
if (!m) throw new Error("Grid_pub_Main not found in the grid layer");
const place = { x: +m[1], y: +m[2], z: +m[3], yaw: +(m[4] ?? 0) };
const h = hour ? ["--hour", hour] : [];
const calls = [{ name: "exterior", args: ["look", "grid:pub", "--close", "0", ...h], dir: "grid-pub" }];
for (const r of spec.looks.rooms) {
  const w = toWorld(place, r.at);
  calls.push({ name: r.name, args: ["look", "grid:pub", "--interior", w.join(","), "--radius", String(r.radius), "--close", "0", ...h], dir: "grid-pub-interior" });
}
for (const c of calls) {
  console.log(`tools\cli ${c.args.join(" ")}`);
  if (!process.argv.includes("--run")) continue;
  const res = spawnSync(process.execPath, [join(ROOT, "tools", "cli.mjs"), ...c.args], { cwd: ROOT, stdio: "inherit", windowsHide: true });
  const src = join(ROOT, "tmp", "look", c.dir);
  if (res.status === 0 && existsSync(src)) cpSync(src, join(ROOT, "tmp", "look", `pub-${tag}${hour ? `-h${hour}` : ""}-${c.name}`), { recursive: true });
  else console.log(`look ${c.name}: exit ${res.status}`);
}
