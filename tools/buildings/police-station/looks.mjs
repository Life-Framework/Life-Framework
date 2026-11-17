// The police station's `look` set: the exterior by day and at night and one interior shot per room, the same points
// every pass, so a before and an after compare. Points are building space (fitout.mjs ORIGIN maps them to the claim).
//
//   node tools/buildings/police-station/looks.mjs <pass-name> [--only exterior,exterior-night,hall,office,office-night] [--no-night]
//
// Each picture set lands in tmp/ps/<pass-name>/<room>.jpg (the look's contact sheet) beside a copy of the PNG tiles.
// `tools\cli evidence <issue> before|after <png>` keeps the ones that go in the PR.
import { copyFileSync, cpSync, mkdirSync, existsSync } from "node:fs";
import { spawnSync } from "node:child_process";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { ORIGIN } from "./origin.mjs";

const ROOT = join(dirname(fileURLToPath(import.meta.url)), "..", "..", "..");
const name = process.argv[2];
if (!name || name.startsWith("--")) { console.error("usage: looks.mjs <pass-name> [--only a,b] [--no-night]"); process.exit(2); }
const only = (process.argv.includes("--only") ? process.argv[process.argv.indexOf("--only") + 1] : "").split(",").filter(Boolean);

// room: [x, floor y, z, radius] in building space; front toward +z
export const ROOMS = {
  hall: [-1.5, 0, 3, 3],
  counter: [-0.6, 0, -1.2, 2.2], // the hall's south end: the front counter, its notice board, the cell door
  office: [3, 0, 0, 5],
  cell: [-4.5, 0, -2, 2.5],
  canteen: [3.5, 0, 5.5, 3],
  records: [3, 3.8, 0, 5],
  interview: [-1.7, 3.8, -1, 3],
};
const OUT = join(ROOT, "tmp", "ps", name);
mkdirSync(OUT, { recursive: true });
const run = (label, extra, area = "grid:police-station-kit") => {
  const out = join(ROOT, "tmp", "ps", `.raw-${label}`);
  const r = spawnSync(process.execPath, [join(ROOT, "tools", "cli.mjs"), "look", area, ...extra, "--serve", "--out", out], { cwd: ROOT, encoding: "utf8", timeout: 1800000 }); // a look waits up to 20 min for a Workbench slot
  const m = (r.stdout || "").split("\n").filter((l) => /^look:|FAIL/.test(l)).slice(0, 2).join(" | ");
  console.log(`${label}: ${m || r.status}`);
  for (const d of ["", "grid-police-station-kit", "grid-police-station-kit-interior"]) {
    const dir = join(out, d);
    if (existsSync(join(dir, "contact.jpg"))) { cpSync(dir, join(OUT, label), { recursive: true }); copyFileSync(join(dir, "contact.jpg"), join(OUT, `${label}.jpg`)); break; }
  }
};
if (!only.length || only.includes("exterior")) run("exterior", ["--close", "0"]);
if ((!only.length || only.includes("exterior-night")) && !process.argv.includes("--no-night")) run("exterior-night", ["--close", "0", "--hour", "22"]);
for (const [room, [x, y, z, r]] of Object.entries(ROOMS)) {
  if (only.length && !only.includes(room)) continue;
  run(room, ["--interior", `${+(ORIGIN.x + x).toFixed(2)},${y},${+(ORIGIN.z + z).toFixed(2)}`, "--radius", String(r), "--close", "0"]);
}
if ((!only.length || only.includes("office-night")) && !process.argv.includes("--no-night")) run("office-night", ["--interior", `${+(ORIGIN.x + 3).toFixed(2)},0,${ORIGIN.z}`, "--radius", "5", "--close", "0", "--hour", "22"]);
console.log(`looks: pictures in tmp/ps/${name}/`);
