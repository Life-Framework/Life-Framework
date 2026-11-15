// tools\cli building review <slug> --shots: look at a building without starting the game.
//
// Renders a fixed set of views straight from the building's .blend in headless Blender (Workbench,
// no add-on needed) in a few seconds: the street front, a three-quarter view from across the street,
// the entrance close up, the hall from the entrance and from the back, each back room, and a plan with
// the rooms labelled. A contact sheet and the single shots land in tmp/buildings/<slug>/review/.
// Camera positions come from the building's own bounds, its entrance gap and the rooms found in a
// plan slice (blender/review_shots.py), never from numbers kept per building. Colours are the game's:
// each material's Blender colour times the scale the export gives it (materials.mjs), and exposure
// set so a wall reads about as bright as in the game. docs/building-pipeline/polish.md "Review shots".
import { existsSync, mkdirSync, readFileSync, readdirSync, rmSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { spawnSync } from "node:child_process";
import { archBuilderSetup } from "./archbuilder.mjs";
import { cacheDir, polishedBlend } from "./building.mjs";
import { ALBEDO_SCALE, OUTSIDE_SHARE } from "./materials.mjs";

export const SHOTS_SCRIPT = join(dirname(fileURLToPath(import.meta.url)), "blender", "review_shots.py");

// What the Blender side needs to colour a material like the game (one scale for a material that faces
// outside, one for a material only inside).
export const albedoJob = () => ({ exterior: ALBEDO_SCALE.exterior, interior: ALBEDO_SCALE.interior, outside_share: OUTSIDE_SHARE });

// The shots in reading order: outside, entrance, inside, the plan last.
const ORDER = ["street_front", "street_3q", "entrance", "hall", "hall_back"];
export function sheetTiles(report) {
  const by = new Map(report.shots.map((s) => [s.name, s]));
  const names = [...ORDER.filter((n) => by.has(n)), ...report.shots.map((s) => s.name).filter((n) => n.startsWith("room")).sort((a, b) => a.localeCompare(b, "en", { numeric: true })), ...(by.has("plan") ? ["plan"] : [])];
  return names.map((n) => ({ file: by.get(n).file, label: by.get(n).label, marks: [] }));
}

export function sheetCols(count) {
  return count <= 6 ? 3 : 4;
}

// The .blend the review reads: the hand-polished scene when there is one, else the generated one.
export function reviewBlend(root, slug) {
  const polished = polishedBlend(root, slug);
  return existsSync(polished) ? polished : join(cacheDir(root, slug), "building.blend");
}

export async function reviewShots(root, slug, { setup = archBuilderSetup(), say = console.log } = {}) {
  const blend = reviewBlend(root, slug);
  if (!existsSync(blend)) return say(`FAIL  no ${blend}; run tools\\cli building run ${slug} --only generate first`) ?? 1;
  // Only Blender itself is needed (the .blend is read as plain meshes), not the add-on or its licence.
  if (!existsSync(setup.exe)) {
    say(`SKIP  review --shots: Blender not found at ${setup.exe} (LF_BLENDER_ARCHBUILDER overrides)`);
    return 0;
  }
  const out = join(cacheDir(root, slug), "review");
  mkdirSync(out, { recursive: true });
  for (const f of readdirSync(out)) if (/^(room\d+|street_front|street_3q|entrance|hall|hall_back|plan|contact)\.(png|jpg)$/.test(f)) rmSync(join(out, f));
  const jobFile = join(out, "shots-job.json");
  writeFileSync(jobFile, JSON.stringify({ out, albedo: albedoJob() }, null, 1));
  const started = Date.now();
  const res = spawnSync(setup.exe, ["--background", "--factory-startup", blend, "--python-exit-code", "3", "--python", SHOTS_SCRIPT, "--", jobFile],
    { encoding: "utf8", timeout: 180000, windowsHide: true, maxBuffer: 64 * 1024 * 1024 });
  const log = join(out, "shots-blender.log");
  writeFileSync(log, (res.stdout || "") + "\n" + (res.stderr || ""));
  const reportFile = join(out, "shots.json");
  if (res.error || res.status !== 0 || !existsSync(reportFile)) {
    const tail = ((res.stdout || "") + (res.stderr || "")).trim().split("\n").slice(-10).join("\n");
    return say(`FAIL  review --shots: Blender exited ${res.status ?? res.error?.message} (log ${log})\n${tail}`) ?? 1;
  }
  const report = JSON.parse(readFileSync(reportFile, "utf8"));
  const tiles = sheetTiles(report);
  const contact = join(out, "contact.png");
  const { image } = await import("../../../agent-box/shots.mjs");
  image({ op: "sheet", tiles, out: contact, cols: sheetCols(tiles.length), maxWidth: sheetCols(tiles.length) * 640, quality: 90 });
  const secs = Math.round((Date.now() - started) / 100) / 10;
  say(`review --shots: ${report.shots.length} views of ${blend.endsWith("polished.blend") ? "the hand-polished scene" : "the generated building"} in ${secs}s`);
  say(`  ${report.building_m.join(" x ")} m; entrance ${report.entrance.found ? `${report.entrance.w} m wide at x ${report.entrance.x.toFixed(1)}` : "not found (front middle)"}; hall ${report.hall ? `${report.hall.area} m2` : "none"}; ${report.rooms.length} back room(s)`);
  say(`  contact sheet: ${contact}`);
  say(`  single shots:  ${out}\\<name>.png   (camera positions in prefab space: ${reportFile})`);
  return 0;
}
