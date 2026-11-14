// The picture half of the clothing verify stage (docs/asset-pipeline/clothing.md "Verify").
//
// One hidden Workbench (tools/assets/pipeline/wb/LF_ClothingShotsPlugin.c) opens DebugWorld in play
// mode, spawns a vanilla civilian as the mannequin and, pose by pose, puts each garment of the job on
// it and photographs it from the front, the side and the back; then it drops each garment on the
// ground and photographs that. Every garment goes through the same EquipCloth call the Equip action
// makes. With two garments (the vanilla parent and ours) every state is a pair, and `pairDiff`
// measures how far ours is from vanilla in each.
//
// The inventory preview is not drawn here: Workbench play captures no UI. `previews` in this file
// runs `tools\cli shots previews --prefabs` on a session client for that.
import { existsSync, mkdirSync, readFileSync, readdirSync, renameSync, rmSync, writeFileSync, cpSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";

const HERE = dirname(fileURLToPath(import.meta.url));
const ROOT = resolve(HERE, "..", "..", "..");
const PLUGIN = "LF_ClothingShotsPlugin";

// A vanilla civilian with a fixed loadout (grey suit), so a picture differs only in the garment under test.
export const MANNEQUIN = "{E024A74F8A4BC644}Prefabs/Characters/Factions/CIV/Businessman/Character_CIV_Businessman_1.et";
// Open grass west of the DebugWorld village, facing north (yaw 0): nothing within 20 m of the run.
export const STAGE = { x: 60, z: 240, yaw: 0, ground: { x: 64, z: 236 } };

// Camera per pose: az relative to the mannequin's facing (0 = in front), metres away, metres up, aim height, vertical fov.
export const POSES = [
  { name: "idle", stance: "erect", move: 0, settle: 1500, views: [["front", 0], ["side", 90], ["back", 180]], dist: 2.8, up: 1.2, look: 1.0, fov: 45 },
  { name: "crouch", stance: "crouch", move: 0, settle: 2500, views: [["front", 0], ["side", 90], ["back", 180]], dist: 2.4, up: 0.9, look: 0.65, fov: 45 },
  { name: "prone", stance: "prone", move: 0, settle: 4500, views: [["side", 90], ["front", 20], ["back", 160]], dist: 2.8, up: 1.4, look: 0.25, fov: 45 },
  { name: "run", stance: "erect", move: 3, settle: 2500, views: [["side", 90], ["front", 30]], dist: 3.2, up: 1.2, look: 0.95, fov: 45 },
];
export const GROUND_VIEWS = [["top", 0, 0.5, 1.1], ["side", 135, 1.0, 0.6]];

const safe = (s) => String(s).replace(/[^A-Za-z0-9_-]/g, "_");

// garments: [{ tag, prefab }] in the order they are worn per pose (vanilla first).
export function planText({ garments, mannequin = MANNEQUIN, stage = STAGE, poses = POSES, hour = 13, world = "Worlds/DebugWorld/DebugWorld.ent" }) {
  if (!garments.length) throw new Error("no garments to photograph");
  for (const g of garments) if (!/^[A-Za-z0-9_]+$/.test(g.tag) || !/^\{[0-9A-F]{16}\}\S+\.et$/.test(g.prefab)) throw new Error(`bad garment ${JSON.stringify(g)}`);
  const lines = [`world ${world}`, `hour ${hour}`, `park ${stage.x - 40} ${stage.z - 40}`, "first 4000"];
  // A fresh mannequin on the same spot for every garment in every pose: a run carries it away, and
  // each garment then starts from the same place, stance and moment of the idle loop.
  for (const pose of poses) {
    for (const g of garments) {
      lines.push(`spawn ${stage.x} ${stage.z} ${stage.yaw} ${mannequin}`, `wear ${g.prefab}`, `stance ${pose.stance}`, `move ${pose.move}`, `wait ${pose.settle}`);
      for (const [view, az] of pose.views) lines.push(`shot ${g.tag}__${pose.name}_${view} ${az} ${pose.dist} ${pose.up} ${pose.look} ${pose.fov}`);
      lines.push("move 0");
    }
  }
  lines.push("despawn");
  garments.forEach((g, i) => {
    lines.push(`drop ${stage.ground.x} ${stage.ground.z} 30 ${g.prefab}`);
    for (const [view, az, dist, up] of GROUND_VIEWS) lines.push(`groundshot ${g.tag}__ground_${view} ${az} ${dist} ${up} 50`);
    lines.push("clear");
  });
  return lines.join("\n") + "\n";
}

// Every state the plan photographs, as "<pose>_<view>" (the part after the tag).
export function stateNames(poses = POSES) {
  return [...poses.flatMap((p) => p.views.map(([v]) => `${p.name}_${v}`)), ...GROUND_VIEWS.map(([v]) => `ground_${v}`)];
}

// How far two pictures of the same state are apart: each is averaged into a 16 x 9 grid of blocks
// (grass blades, the weave and sub-pixel aliasing cancel out; a garment of another colour does not), and
// mean and max are the mean and the largest |dRGB| (0..255) over the blocks.
export const GRID = [16, 9];
export function blocks(img, [gx, gy] = GRID) {
  const sum = new Float64Array(gx * gy * 3), cnt = new Float64Array(gx * gy);
  for (let y = 0; y < img.h; y += 2) {
    for (let x = 0; x < img.w; x += 2) {
      const k = Math.min(gy - 1, Math.floor((y * gy) / img.h)) * gx + Math.min(gx - 1, Math.floor((x * gx) / img.w));
      const o = (y * img.w + x) * img.channels;
      sum[k * 3] += img.px[o]; sum[k * 3 + 1] += img.px[o + 1]; sum[k * 3 + 2] += img.px[o + 2];
      cnt[k]++;
    }
  }
  return Array.from(sum, (v, i) => v / (cnt[Math.floor(i / 3)] || 1));
}
export function pairDiff(a, b) {
  if (a.w !== b.w || a.h !== b.h) return { mean: 255, max: 255, note: "sizes differ" };
  const A = blocks(a), B = blocks(b);
  let sum = 0, max = 0;
  for (let k = 0; k < A.length / 3; k++) {
    const d = (Math.abs(A[k * 3] - B[k * 3]) + Math.abs(A[k * 3 + 1] - B[k * 3 + 1]) + Math.abs(A[k * 3 + 2] - B[k * 3 + 2])) / 3;
    sum += d;
    if (d > max) max = d;
  }
  const r1 = (v) => Math.round(v * 10) / 10;
  return { mean: Math.round((sum / (A.length / 3)) * 100) / 100, max: r1(max) };
}

// Run the plan in a hidden Workbench; move the pictures to outDir; return { shots: {name: file}, status }.
export async function shoot(root, { garments, outDir, budgetS, mannequin, poses, stage }) {
  const look = await import("../../visual/look.mjs");
  const wt = await import("../../wt.mjs");
  if (wt.isMainCheckout(root)) throw new Error("clothing shots run only in a worktree (they open a second Workbench on the project)");
  const exe = look.workbenchExe();
  if (!exe) throw new Error("Workbench not found (set ENFUSION_WORKBENCH_PATH)");
  const profile = join(root, "server", "profile", "clothing");
  const raw = join(profile, "profile", "lf-clothing");
  const injected = join(root, "addons", "LifeFramework", "Scripts", "WorkbenchGame", "LFClothing");
  const plan = planText({ garments, mannequin, poses, stage });
  const shotCount = plan.split("\n").filter((l) => /^(shot|groundshot) /.test(l)).length;
  const budget = budgetS ?? 120 + Math.ceil(plan.split("\n").reduce((s, l) => s + (/^wait (\d+)/.exec(l)?.[1] ?? 0) / 1000 + (/^wear /.test(l) ? 2 : 0) + (/^(shot|groundshot) /.test(l) ? 1 : 0), 0));
  wt.acquireLock(root, "build", { waitMs: 600000 });
  let res;
  try {
    mkdirSync(raw, { recursive: true });
    for (const f of readdirSync(raw)) rmSync(join(raw, f), { force: true });
    writeFileSync(join(raw, "plan.txt"), plan);
    mkdirSync(injected, { recursive: true });
    cpSync(join(HERE, "wb", `${PLUGIN}.c`), join(injected, `${PLUGIN}.c`));
    console.log(`clothing shots: ${shotCount} shot(s) of ${garments.length} garment(s), hidden Workbench (budget ${budget} s)`);
    res = await look.runWorkbench(exe, profile, budget, false, { plugin: PLUGIN });
  } finally {
    rmSync(join(injected, `${PLUGIN}.c`), { force: true });
    try { if (existsSync(injected) && !readdirSync(injected).length) rmSync(injected, { recursive: false }); } catch {}
    wt.releaseLock(root, "build");
  }
  const statusFile = join(raw, "status.txt");
  const statusText = existsSync(statusFile) ? readFileSync(statusFile, "utf8") : "";
  const status = look.parseStatus(statusText);
  if (!status.world) {
    const log = look.latestLog(profile);
    const errs = look.compileErrors(log);
    throw new Error(`Workbench exit ${res.code} after ${Math.round(res.seconds)} s and the plugin never opened the world${errs.length ? `: ${errs.join(" | ")}` : ""}${log ? ` (log ${log})` : ""}`);
  }
  mkdirSync(outDir, { recursive: true });
  const shots = {};
  for (const [name, st] of Object.entries(status.shots)) {
    const file = join(outDir, `${safe(name)}.png`);
    try {
      renameSync(join(raw, st.file), file);
      shots[name] = file;
    } catch {}
  }
  const wears = statusText.split(/\r?\n/).filter((l) => / ok wear /.test(l)).map((l) => l.replace(/^\d+ ok wear /, ""));
  return { shots, status, wears, timedOut: res.timedOut, seconds: res.seconds };
}
