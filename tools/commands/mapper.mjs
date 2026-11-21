// tools\cli mapper: bring a Reforger Mapper layout into our pipeline (docs/architecture/reforger-mapper-import.md).
//
//   mapper import <project.json> [--cell cXrY] [--span WxH] [--slug s] [--dry] [--no-verify] [--no-fetch]
//   mapper import <project.json> --slot <Layer.layer> --at x,z [--dry] [--no-verify]      (a `town-` branch only)
//   mapper import <project.json> --slot Place_<place>_<slot>.layer [--at x,z] [--dry] [--no-verify] [--keep]   (an island slot; a `town-draft-<place>-<slot>[-word]` branch)
//
// Mapper (https://github.com/benjibossk/mappingenfusiontest, MIT) sketches site layouts in a browser and sends a
// "mapgen.json" project to Workbench. This reads the placed-prefab part of that project (`entities`: prefab, x, z,
// yaw) and puts it where ours go, through the same writer and checks as `tools\cli place` and `grid place`:
//   1. every prefab ref is resolved against addons/ and the game's paks; an unknown one, or one with no mesh the
//      placement tool can measure, is dropped and reported (never written);
//   2. the layout's centre is moved onto the target (a grid cell's centre, or --at) and its yaw kept;
//   3. cell mode claims the cell with `grid place` (yard fence and sign, the world-zone claim), then `place ground`
//      stands each prop on the probed ground with the offline checks (overlap, carriageway, door swing, scale), one
//      `place settle` rests them, and `audit placement --grid --strict` judges the whole layer;
//   4. slot mode needs a `ws/town-*` branch, because a town layer is written only by a town-placement pass
//      (docs/architecture/world-zones.md "The guard"). A playtest-town layer goes through `place`; an island slot
//      (`Place_<place>_<slot>.layer`, branch town-draft-<place>-<slot>[-word]) through tools/mapper/island.mjs.
// All props of one import go through ONE `place ground-many` run (one probe boot), not one boot per prop (#3851).
// Vectors are written by the same `appendEntity` as every placement, bare, never quoted. Shapes, terrain, masks and
// roads in the project are not imported (a generator is Workbench's to run). Nothing of Mapper's code is used.
import { existsSync, mkdirSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { basename, dirname, join, resolve } from "node:path";
import { spawnSync } from "node:child_process";
import { fileURLToPath } from "node:url";
import { centreOf, entityName, fit, parseProject, translate } from "../mapper/convert.mjs";
import { SLUG_RX, claimRect, entityPrefix, isPackPrefab, layerOfSlug, readCells } from "../lib/world-zones.mjs";
import { islandLayersDir } from "../lib/worlds.mjs";
import { intoIslandSlot } from "../mapper/island.mjs";

export const help = "mapper import <project.json> [--cell cXrY] [--span WxH] [--slug s] [--dry] | --slot <Layer.layer> --at x,z | --slot Place_<place>_<slot>.layer [--at x,z]   a Reforger Mapper layout into a debug-grid cell, a playtest-town layer (town- branch) or an island slot (town-draft- branch), through the placement checks";

const ROOT = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
const VALUE_FLAGS = new Set(["cell", "span", "slug", "slot", "at"]); // --keep, --dry, --no-verify, --no-fetch are switches
const r2 = (n) => Math.round(n * 100) / 100;

function parseArgs(argv) {
  const out = { _: [], flags: {} };
  for (let i = 0; i < argv.length; i++) {
    const a = argv[i];
    if (!a.startsWith("--")) out._.push(a);
    else {
      const k = a.slice(2);
      out.flags[k] = VALUE_FLAGS.has(k) ? argv[++i] : true;
    }
  }
  return out;
}

const pair = (text, what) => {
  const m = /^(-?\d+(?:\.\d+)?),(-?\d+(?:\.\d+)?)$/.exec(String(text ?? ""));
  if (!m) throw new Error(`${what} is not x,z (like 120.5,88)`);
  return [Number(m[1]), Number(m[2])];
};

// One prefab row of the project: { ref } to place it, or { drop: reason }. `P` is the placement tool's reader
// (prefabInfo); the context's `inspect` replaces the whole step in the selftest.
async function inspectPrefab(root, P, prefab) {
  const { resolvePrefab } = await import("../lib/game-prefab.mjs");
  let r;
  try {
    // The project's GUID is a claim: the bare path says which GUID the game really holds for it (when it can tell).
    r = await resolvePrefab(root, prefab.guid ? `{${prefab.guid}}${prefab.path}` : prefab.path);
    const truth = prefab.guid ? await resolvePrefab(root, prefab.path).catch(() => null) : null;
    if (truth?.guid && truth.guid !== r.guid) r = truth;
  } catch (e) {
    return { drop: `unknown prefab (${String(e.message).split("(")[0].trim()})` };
  }
  if (!r.ref) return { drop: `no GUID known for ${r.path}` };
  const note = prefab.guid && prefab.guid !== r.guid ? `GUID {${prefab.guid}} in the project is not ${r.path}'s; used {${r.guid}}` : null;
  const info = P.prefabInfo(P.ROOT, r.path);
  if (!info.bounds) return { drop: `no mesh the placement tool can measure (${info.error || "no MeshObject in its chain"})`, ref: r.ref };
  return { ref: r.ref, note };
}

const gitBranch = (root) => {
  const r = spawnSync("git", ["rev-parse", "--abbrev-ref", "HEAD"], { cwd: root, encoding: "utf8", windowsHide: true });
  return r.status === 0 ? r.stdout.trim() : "";
};

export const isTownPass = (branch) => /^ws\/town-[a-z0-9]/.test(String(branch ?? ""));

async function importProject(root, args, flags, ctx) {
  const [file] = args;
  if (!file) return usage();
  const path = resolve(process.cwd(), file);
  if (!existsSync(path)) {
    console.log(`mapper: no file ${file}`);
    return 1;
  }
  if (flags.cell && flags.slot) {
    console.log("mapper: --cell and --slot are two targets; give one (the default is a debug-grid cell)");
    return 1;
  }
  const project = parseProject(readFileSync(path, "utf8"));
  for (const n of project.notes) console.log(`mapper: note: ${n}`);
  if (project.error) {
    console.log(`mapper: ${project.error}`);
    return 1;
  }
  const P = ctx.P ?? (await import("../place/place.mjs"));
  // 1. prefabs: one lookup per distinct path.
  const verdicts = new Map();
  for (const e of project.entities) {
    if (verdicts.has(e.prefab.path)) continue;
    verdicts.set(e.prefab.path, await (ctx.inspect ?? ((p) => inspectPrefab(root, P, p)))(e.prefab));
  }
  const kept = [];
  const dropped = [];
  for (const e of project.entities) {
    const v = verdicts.get(e.prefab.path);
    if (v.drop) dropped.push({ e, why: v.drop });
    else kept.push({ ...e, ref: v.ref });
  }
  for (const [p, v] of verdicts) if (v.note) console.log(`mapper: note: ${p}: ${v.note}`);
  for (const d of dropped) console.log(`mapper: dropped ${d.e.id} (${d.e.prefab.path}): ${d.why}`);
  if (!kept.length) {
    console.log(`mapper: nothing left to place: all ${project.entities.length} prefab row(s) were dropped`);
    return 1;
  }
  const f = fit(kept);
  console.log(`mapper: ${kept.length} prop(s) of ${project.entities.length}, ${project.shapes} shape(s) skipped; layout ${r2(f.w)} x ${r2(f.d)} m, needs ${f.span[0]}x${f.span[1]} cell(s)`);
  return flags.slot ? intoSlot(root, P, kept, dropped, flags, ctx) : intoCell(root, P, path, kept, dropped, f, flags, ctx);
}

// Runs the placement tool for a plan: ONE `place ground-many` (one probe boot, one offline review, one write) for every prop.
// The batch is all or nothing; when it refuses, nothing is written, so the props go one at a time (`place ground`) to find
// and name the refused one and keep the others. Returns the names placed and the refusals.
async function placeAll(root, P, plan, layer, prefix, extra, ctx) {
  const run = ctx.placeMain ?? P.main;
  const names = plan.map((_, i) => entityName(prefix, i + 1));
  const rows = plan.map((e, i) => ({ at: [r2(e.x), r2(e.z)], prefab: e.ref, yaw: e.yaw, name: names[i] }));
  const dir = join(root, "tmp");
  mkdirSync(dir, { recursive: true });
  const file = join(dir, `mapper-plan-${process.pid}.json`);
  writeFileSync(file, `${JSON.stringify(rows, null, 1)}
`);
  let code;
  try {
    const argv = ["ground-many", file, "--layer", layer, "--no-verify", ...extra];
    console.log(`  tools\\cli place ${argv.join(" ")}   (${plan.length} prop(s), one probe boot)`);
    code = await run(argv);
  } finally {
    rmSync(file, { force: true });
  }
  if (code === 0) return { placed: names, refused: [] };
  console.log("mapper: the batch was refused (nothing written); placing the props one at a time to name the refused ones");
  const placed = [];
  const refused = [];
  for (let i = 0; i < plan.length; i++) {
    const e = plan[i];
    const argv = ["ground", `${r2(e.x)},${r2(e.z)}`, e.ref, "--layer", layer, "--yaw", String(e.yaw), "--name", names[i], "--no-verify", ...extra];
    console.log(`  tools\\cli place ${argv.join(" ")}   (${e.id})`);
    if ((await run(argv)) === 0) placed.push(names[i]);
    else refused.push({ e, name: names[i] });
  }
  return { placed, refused };
}

function report(kind, placed, dropped, refused) {
  console.log(`mapper: ${placed.length} placed in ${kind}, ${dropped.length} dropped (prefab), ${refused.length} refused by the placement checks`);
  for (const r of refused) console.log(`  refused ${r.e.id} (${r.e.prefab.path}) at ${r2(r.e.x)},${r2(r.e.z)}: see the place line above`);
}

async function intoCell(root, P, path, kept, dropped, f, flags, ctx) {
  const slug = flags.slug && flags.slug !== true ? String(flags.slug) : `mapper-${basename(path).replace(/\.json$/i, "").toLowerCase().replace(/[^a-z0-9]+/g, "-").replace(/^-+|-+$/g, "")}`.slice(0, 41);
  if (!SLUG_RX.test(slug)) {
    console.log(`mapper: the cell slug "${slug}" must be lower-case letters, digits and dashes (2 to 41); pass --slug`);
    return 1;
  }
  const layer = layerOfSlug(slug);
  const prefix = entityPrefix(slug);
  const span = flags.span ? String(flags.span) : `${f.span[0]}x${f.span[1]}`;
  if (!/^[0-9]+x[0-9]+$/.test(span)) {
    console.log(`mapper: --span ${flags.span} is not cols x rows, like 2x2`);
    return 1;
  }
  const pack = kept.some((e) => isPackPrefab(root, e.ref));
  const grid = ["place", slug, ...(flags.cell ? ["--cell", String(flags.cell)] : []), "--span", span, "--no-verify", ...(flags["no-fetch"] ? ["--no-fetch"] : []), ...(pack ? ["--pack"] : [])];
  if (readCells(root).some((c) => c.slug === slug)) {
    console.log(`mapper: ${slug} already has a cell; tools\\cli grid free ${slug} first (the import writes a fresh layer), or pass another --slug`);
    return 1;
  }
  if (flags.dry) {
    console.log(`mapper: dry run. would run: tools\\cli grid ${grid.join(" ")}`);
    const rect = flags.cell ? claimRect({ cell: String(flags.cell), span: span.split("x").map(Number) }) : null;
    const plan = rect ? translate(kept, centreOf(rect)) : kept;
    console.log(rect ? `mapper: centre of ${flags.cell}+${span} is ${centreOf(rect).map(r2).join(",")}; props:` : "mapper: the cell is chosen at claim time; props (project coordinates):");
    plan.forEach((e, i) => console.log(`  ${entityName(prefix, i + 1)}  ${e.prefab.path.split("/").pop()}  ${r2(e.x)},${r2(e.z)}  yaw ${e.yaw}  (${e.id})`));
    console.log("mapper: nothing written.");
    return 0;
  }
  if (!ctx.placeMain && (await import("../wt.mjs")).isMainCheckout(root)) {
    console.log("mapper: REFUSE: this is the main checkout (the world-editor copy). Create a worktree first: tools\\cli wt new <slug>");
    return 1;
  }
  const G = ctx.gridMain ?? (await import("./grid.mjs")).main;
  const gcode = await G(grid.slice(), { root });
  if (gcode !== 0) return gcode;
  const claim = readCells(root).find((c) => c.slug === slug);
  const rect = claim ? claimRect(claim) : null;
  if (!rect) {
    console.log(`mapper: the claim of ${slug} cannot be read after grid place; stopping`);
    return 1;
  }
  const plan = translate(kept, centreOf(rect));
  console.log(`mapper: ${slug} -> cell ${rect.id}${rect.w * rect.h > 1 ? `+${rect.w}x${rect.h}` : ""}; centre ${centreOf(rect).map(r2).join(",")}`);
  const { placed, refused } = await placeAll(root, P, plan, layer, prefix, ["--y-hint", "0"], ctx);
  if (!placed.length) {
    console.log(`mapper: nothing was placed, so the claim ${slug} is freed again`);
    await G(["free", slug], { root });
    report(`cell ${rect.id}`, placed, dropped, refused);
    return 1;
  }
  let code = refused.length ? 1 : 0;
  if (!flags["no-verify"]) {
    console.log(`  resting the props (place settle) and auditing the cell (audit placement --grid --strict --layer ${layer})`);
    const settle = await (ctx.placeMain ?? P.main)(["settle", ...placed, "--layer", layer]);
    if (settle !== 0) console.log("mapper: warning: place settle reported a problem above; the audit below judges the layer as it stands");
    const audit = ctx.auditMain ?? (async (argv) => (await import("../../agent-box/place-audit.mjs")).main(argv));
    const a = await audit(["placement", "--grid", "--layer", layer, "--strict"]);
    if (a !== 0) code = a;
  }
  report(`cell ${rect.id}`, placed, dropped, refused);
  console.log(code === 0 ? `mapper: done. tools\\cli look grid:${slug} for the picture.` : "mapper: the cell has findings above; fix or --move the props the checks name, or grid free it");
  return code;
}

async function intoSlot(root, P, kept, dropped, flags, ctx) {
  const layer = String(flags.slot);
  if (!/^[A-Za-z0-9_]+\.layer$/.test(layer)) {
    console.log(`mapper: --slot takes a layer file name like Street_Police.layer; got ${layer}`);
    return 1;
  }
  if (/^Grid_/.test(layer)) {
    console.log("mapper: a Grid_ layer is a debug-grid cell: use --cell (or nothing), not --slot");
    return 1;
  }
  const branch = ctx.branch ?? gitBranch(root);
  if (!isTownPass(branch)) {
    console.log(`mapper: --slot writes a town layer, which only a town-placement pass may do (worktree slug town-<chapter>, branch ws/town-<chapter>; this branch is ${branch || "unknown"}). Import into a grid cell instead, or ask the lead for a town- worktree (docs/architecture/world-zones.md "The guard")`);
    return 1;
  }
  const known = (ctx.layerExists ?? ((name) => existsSync(P.layerPath(root, name))))(layer);
  if (!known) {
    // An island slot (MainWorld_Layers/Place_<place>_<slot>.layer) is written by its own writer: terrain-seated, inside the
    // slot's claim, audited on the island (tools/mapper/island.mjs). `--at` is optional there (the slot's ground centre).
    if ((ctx.islandLayerExists ?? ((name) => existsSync(join(islandLayersDir(root), name))))(layer)) return intoIslandSlot({ root, P, layer, kept, flags, ctx, branch, translate });
    console.log(`mapper: no layer ${layer} in the playtest town (DebugWorld_Layers or Everon1989Town_Layers) or among the island slots (MainWorld_Layers)`);
    return 1;
  }
  if (flags.at == null) {
    console.log("mapper: --slot needs --at x,z, the point the layout's centre goes to (the tool never invents a town coordinate)");
    return 1;
  }
  let at;
  try {
    at = pair(flags.at, "--at");
  } catch (e) {
    console.log(`mapper: ${e.message}`);
    return 1;
  }
  const plan = translate(kept, at);
  const prefix = `Mapper_${layer.replace(/\.layer$/, "").replace(/[^A-Za-z0-9]/g, "_")}`;
  if (flags.dry) {
    console.log(`mapper: dry run, ${layer} on ${branch}; centre ${at.join(",")}; props:`);
    plan.forEach((e, i) => console.log(`  ${entityName(prefix, i + 1)}  ${e.prefab.path.split("/").pop()}  ${r2(e.x)},${r2(e.z)}  yaw ${e.yaw}  (${e.id})`));
    console.log("mapper: nothing written.");
    return 0;
  }
  if (!ctx.placeMain && (await import("../wt.mjs")).isMainCheckout(root)) {
    console.log("mapper: REFUSE: this is the main checkout. Create a worktree first: tools\\cli wt new town-<chapter>");
    return 1;
  }
  const { placed, refused } = await placeAll(root, P, plan, layer, prefix, [], ctx);
  let code = refused.length ? 1 : 0;
  if (placed.length && !flags["no-verify"]) {
    const v = await (ctx.placeMain ?? P.main)(["verify", ...placed]);
    if (v !== 0) code = v;
  }
  report(layer, placed, dropped, refused);
  return code;
}

function usage() {
  console.log("usage: tools\\cli mapper import <project.json> [--cell cXrY] [--span WxH] [--slug s] [--dry] [--no-verify] [--no-fetch]\n       tools\\cli mapper import <project.json> --slot <Layer.layer> --at x,z [--dry] [--no-verify]   (a ws/town-* branch)\n       tools\\cli mapper import <project.json> --slot Place_<place>_<slot>.layer [--at x,z] [--dry] [--no-verify] [--keep]   (an island slot; a ws/town-draft-<place>-<slot>[-word] branch)\n(docs/architecture/reforger-mapper-import.md)");
  return 1;
}

// `inspect(prefab)`, `gridMain`, `placeMain`, `auditMain`, `branch`, `layerExists` and `P` in the context replace the
// resolver, the grid and place tools, the audit, `git` and the layer lookup (the selftest's seams).
export async function main(argv, { root = ROOT, ...ctx } = {}) {
  const [sub, ...rest] = argv;
  const { _: args, flags } = parseArgs(rest);
  try {
    if (sub === "import") return await importProject(root, args, flags, ctx);
  } catch (e) {
    console.log(`mapper: ${e.message}`);
    return 1;
  }
  return usage();
}
