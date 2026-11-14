// tools\cli building <command>: a committed spec -> ArchBuilder (headless Blender 5.3) -> the addon.
// Playbook: docs/building-pipeline/README.md. Spike: docs/architecture/building-pipeline-spike.md.
//
//   building run <slug> [--from <stage>] [--only <stage>] [--force]
//   building status [<slug>]     building doctor     building selftest
//
// The spec is tools/buildings/<slug>/spec.json (tracked). Every stage writes into
// tmp/buildings/<slug>/ and a finished stage is skipped until --force or an earlier stage reruns.
// A stage that needs ArchBuilder skips with a message on a machine without it (CI, the test laptop).
import { existsSync, mkdirSync, readFileSync, writeFileSync, readdirSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { ArchBuilderMissing, archBuilderSetup, licenseMarks, runArchBuilder } from "./archbuilder.mjs";
import { safeRemoveTree } from "../../link-safety.mjs";
import { addonOf, addonPath, addonProblems } from "../../assets/pipeline/addon.mjs";
import { readGuid } from "../../assets/pipeline/meta.mjs";
import { gameDb } from "./gamedb.mjs";
import { materialsDir } from "./import.mjs";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..", "..");
export const STAGES = ["spec", "generate", "review", "export", "import", "prefab"];
const NEEDS_ARCHBUILDER = new Set(["generate", "review", "export"]);

export const specFile = (root, slug) => join(root, "tools", "buildings", slug, "spec.json");
export const cacheDir = (root, slug) => join(root, "tmp", "buildings", slug);

export function loadSpec(root, slug) {
  const file = specFile(root, slug);
  if (!existsSync(file)) throw new Error(`no spec at ${file}; copy tools/buildings/village-house-01/spec.json and edit it`);
  return JSON.parse(readFileSync(file, "utf8"));
}

const REGIONS = ["AUTO", "AMERICAN", "EUROPEAN", "SOUTH_AMERICAN", "EASTERN"];

// Everything wrong with a spec, as a list. Empty means the spec stage passes.
export function specProblems(spec) {
  const out = [];
  if (!/^[a-z][a-z0-9-]*$/.test(spec.slug || "")) out.push("slug must be lowercase letters, digits and dashes");
  if (!/^[A-Z][A-Za-z0-9]*$/.test(spec.name || "")) out.push("name must be PascalCase letters and digits (it becomes the file names)");
  if (!spec.concept) out.push("concept is empty: one paragraph on what the building is in 1989 Everon and what it is for");
  const ab = spec.archbuilder;
  if (!ab || typeof ab !== "object") return [...out, "archbuilder must hold the ArchBuilder settings (scene.ab_gen names)"];
  if (!ab.landmark && !ab.archetype) out.push("archbuilder.archetype (or archbuilder.landmark for a real-plan building) is required");
  if (ab.landmark && typeof ab.landmark !== "string") out.push("archbuilder.landmark must be a landmark id such as TOWN_HALL");
  if (ab.region && !REGIONS.includes(ab.region)) out.push(`archbuilder.region must be one of ${REGIONS.join(", ")}`);
  if (!Number.isInteger(ab.seed) || ab.seed < 1 || ab.seed > 999999) out.push("archbuilder.seed must be a whole number 1 to 999999 (the same seed builds the same building)");
  if (ab.storeys !== undefined && !(Number.isInteger(ab.storeys) && ab.storeys >= 1 && ab.storeys <= 12)) out.push("archbuilder.storeys must be 1 to 12");
  for (const k of ["width", "depth"]) if (ab[k] !== undefined && !(ab[k] >= 4 && ab[k] <= 60)) out.push(`archbuilder.${k} must be 4 to 60 metres`);
  if (ab.proportions !== undefined && ab.proportions !== "REFORGER") out.push("archbuilder.proportions must be REFORGER (vanilla-sized doors the character fits through)");
  if (ab.vanilla_openings === false) out.push("archbuilder.vanilla_openings must stay true (game doors and destructible windows)");
  if (ab.archetype === "HIGHRISE") out.push("HIGHRISE is not a 1989 Everon building");
  if (spec.export?.light_system) out.push("export.light_system writes an Enforce script with ArchBuilder's own class names; leave it off (AGENTS.md rule 5)");
  // ArchBuilder records its furniture calls (shelves, counters, lamps, the fascia sign) as vanilla
  // props only while it exports furniture: with export.furniture off it records none and the
  // vanilla_props setting does nothing, silently (docs/architecture/building-pipeline-spike.md).
  if (spec.export?.vanilla_props !== false && spec.export?.furniture === false) out.push("export.furniture is false, so ArchBuilder records no vanilla props at all (not even lamps or the shop sign); turn it on, or set export.vanilla_props false to say the shell is meant to be bare");
  if (spec.export?.furniture !== false && ab.do_furniture === false && spec.export?.vanilla_props !== false) out.push("archbuilder.do_furniture is false, so the rooms get no furniture to export; turn it on (empty shelves are what our shop fills)");
  const b = spec.budget || {};
  if (!(b.tris >= 1000 && b.tris <= 400000)) out.push("budget.tris must be 1000 to 400000 (the whole building, every part)");
  if (!Array.isArray(b.footprint_m) || b.footprint_m.length !== 2 || b.footprint_m.some((v) => !(v > 2 && v < 120))) out.push("budget.footprint_m must be [width, depth] in metres");
  if (!(b.height_m > 2 && b.height_m < 80)) out.push("budget.height_m must be a height in metres");
  if (!(Number.isInteger(b.min_doors) && b.min_doors >= 1)) out.push("budget.min_doors must be 1 or more: a building a player cannot enter is a prop");
  if (!/^Assets\/Buildings\/[A-Za-z0-9_]+$/.test(spec.asset_dir || "")) out.push("asset_dir must be Assets/Buildings/<Name>");
  if (!/^Prefabs\/Buildings\/[A-Za-z0-9_]+$/.test(spec.prefab_dir || "")) out.push("prefab_dir must be Prefabs/Buildings/<Name>");
  if (spec.name === "Materials" || /\/Materials$/.test(spec.asset_dir || "")) out.push("a building cannot be called Materials: Assets/Buildings/Materials holds the shared materials");
  if (!["core", "pack"].includes(spec.red_line)) out.push('red_line must be "core" or "pack" (docs/architecture/core-vs-pack.md; a building is the server\'s look, so usually pack)');
  return out;
}

function readState(dir) {
  const f = join(dir, "state.json");
  return existsSync(f) ? JSON.parse(readFileSync(f, "utf8")) : { stages: {} };
}
function writeState(dir, state) {
  mkdirSync(dir, { recursive: true });
  writeFileSync(join(dir, "state.json"), JSON.stringify(state, null, 2) + "\n");
}

// ---------------------------------------------------------------- stages
async function stageSpec(ctx) {
  const problems = [...specProblems(ctx.spec), ...addonProblems(ctx.root, ctx.spec)];
  if (problems.length) throw new Error("spec problems:\n  " + problems.join("\n  "));
  const ab = ctx.spec.archbuilder;
  console.log(`spec ok: ${ctx.spec.name}, ${ab.landmark ? `landmark ${ab.landmark}` : `${ab.archetype} ${ab.region || "AUTO"}`}, seed ${ab.seed}, into ${addonOf(ctx.spec)}`);
  return { addon: addonOf(ctx.spec) };
}

async function stageGenerate(ctx) {
  const r = (ctx.runArchBuilder || runArchBuilder)("generate", { spec: ctx.spec, out: ctx.dir, timeoutMs: 180000 });
  console.log(`generate: ${r.anchor} in ${r.secs}s with ArchBuilder ${r.version}; walls ${r.resolved.wall}, roof ${r.resolved.roof_style} ${r.resolved.roof_mat}; ${r.blend}`);
  return { anchor: r.anchor, secs: r.secs, version: r.version, resolved: r.resolved };
}

// Over budget is a failure: a 1989 village house with 300 000 triangles or a 20 m footprint is the
// wrong building, and more seeds will not fix the spec.
export function reviewProblems(spec, stats) {
  const b = spec.budget;
  const out = [];
  if (stats.tris > b.tris) out.push(`${stats.tris} triangles, over budget.tris ${b.tris}`);
  if (stats.footprint_m[0] > b.footprint_m[0] + 0.01 || stats.footprint_m[1] > b.footprint_m[1] + 0.01) out.push(`footprint ${stats.footprint_m.join(" x ")} m is over ${b.footprint_m.join(" x ")} m`);
  if (stats.height_m > b.height_m) out.push(`height ${stats.height_m} m is over ${b.height_m} m`);
  return out;
}

async function stageReview(ctx) {
  const blend = join(ctx.dir, "building.blend");
  if (!existsSync(blend)) throw new Error(`no ${blend}; run the generate stage`);
  const r = (ctx.runArchBuilder || runArchBuilder)("review", { spec: ctx.spec, out: ctx.dir, blend, timeoutMs: 300000 });
  const s = r.stats;
  writeFileSync(join(ctx.dir, "review", "stats.json"), JSON.stringify(s, null, 1) + "\n");
  console.log(`review: ${s.tris} tris, ${s.storeys} storeys, footprint ${s.footprint_m.join(" x ")} m, height ${s.height_m} m`);
  console.log(`        look at ${r.shots.join(", ")} (docs/building-pipeline/README.md "Review")`);
  const problems = reviewProblems(ctx.spec, s);
  for (const p of problems) console.log(`FAIL  ${p}`);
  if (problems.length) throw new Error(`${problems.length} budget check(s) failed; change the spec (seed, size, storeys, archetype) and rerun --from generate`);
  return { stats: s, shots: r.shots };
}

// Counts in the exported building prefab: vanilla door sets and windows, probes.
export function countOpenings(prefabText) {
  const refs = [...prefabText.matchAll(/:\s*"\{[0-9A-F]{16}\}([^"]+)"/g)].map((m) => m[1]);
  return {
    doors: refs.filter((p) => /\/Doors\/DoorSet_|\/Doors\/Door_/i.test(p)).length,
    windows: refs.filter((p) => /\/Windows\//i.test(p)).length,
    probes: refs.filter((p) => /Probe/i.test(p)).length,
  };
}

async function stageExport(ctx) {
  const blend = join(ctx.dir, "building.blend");
  if (!existsSync(blend)) throw new Error(`no ${blend}; run the generate stage`);
  const staging = join(ctx.dir, "export");
  if (existsSync(staging)) safeRemoveTree(staging); // a stale file from an older export must not be imported
  const r = (ctx.runArchBuilder || runArchBuilder)("export", { spec: ctx.spec, out: ctx.dir, blend, extra: { staging }, timeoutMs: 300000 });
  const log = readFileSync(r.log, "utf8");
  const between = log.split("LF_BUILDING export start")[1]?.split("LF_BUILDING export end")[0] || "";
  const warnings = between.split(/\r?\n/).filter((l) => /^(Warning|Error):/.test(l.trim())).map((l) => l.trim());
  const info = between.split(/\r?\n/).find((l) => /^Info: .*doors .*colliders/.test(l.trim()))?.trim().replace(/ -> .*$/, "") || "";
  // ArchBuilder's Color is its Blender colour, raw: give every material the game colour that looks
  // like Blender (tools/buildings/pipeline/materials.mjs).
  const { colourMaterials } = await import("./materials.mjs");
  const colours = await colourMaterials({ staging, dir: ctx.dir, spec: ctx.spec, blend, run: ctx.runArchBuilder || runArchBuilder });
  for (const c of colours) console.log(`        ${c.name.padEnd(20)} ${c.where.padEnd(8)} ${String(Math.round(c.outside * 100)).padStart(3)}% out  Blender ${c.blender.join(" ")} -> Color ${c.color.join(" ")}${c.mean ? ` over a ${Math.max(...c.mean).toFixed(2)} texture` : " (flat)"}${c.capped ? " (texture darker than Blender: Color capped at 1)" : ""}`);
  const base = join(staging, "Prefabs", "ArchBuilder", ctx.spec.name, `${ctx.spec.name}_Base.et`);
  if (!existsSync(base)) throw new Error(`the export wrote no ${base}`);
  const counts = countOpenings(readFileSync(base, "utf8"));
  console.log(`export: ${r.secs}s; ${counts.doors} door sets, ${counts.windows} windows, ${counts.probes} probe(s); ${info.replace(/^Info: /, "")}`);
  const furniture = join(staging, "Prefabs", "ArchBuilder", ctx.spec.name, "Furniture", `${ctx.spec.name}_furniture_01.et`);
  const props = existsSync(furniture) ? countProps(readFileSync(furniture, "utf8")) : { total: 0, kinds: {} };
  if (props.total) console.log(`        ${props.total} vanilla props: ${Object.entries(props.kinds).map(([k, n]) => `${n} ${k}`).join(", ")}`);
  const problems = exportProblems(ctx.spec, r.report, warnings, props);
  for (const p of problems) console.log(`FAIL  ${p}`);
  if (counts.doors < ctx.spec.budget.min_doors) problems.push(`${counts.doors} door(s); the spec wants at least ${ctx.spec.budget.min_doors}`);
  if (problems.length) throw new Error(`${problems.length} export check(s) failed: what ArchBuilder shows in Blender would not reach the game`);
  return { secs: r.secs, ...counts, props: props.total, propKinds: props.kinds, report: r.report, colours };
}

// The vanilla props in the furniture prefab, by prefab file name: its child entities (indented),
// not the BuildingFurniture_base.et the prefab itself inherits (column 0).
export function countProps(furnitureText) {
  const kinds = {};
  let total = 0;
  for (const m of furnitureText.matchAll(/^[ 	]+\w+ : "\{[0-9A-F]{16}\}[^"]*\/([^"/]+)\.et"/gm)) {
    kinds[m[1]] = (kinds[m[1]] || 0) + 1;
    total++;
  }
  return { total, kinds };
}

// What the export dropped or warned about. ArchBuilder reports, it does not stop: a warning (no
// vanilla prefab for an opening, a cutaway export) and a furniture record it found no prefab or no
// room for (props_mesh) each mean the game gets less than Blender shows. Each one fails the stage.
export function exportProblems(spec, report, logWarnings = [], props = { total: 0 }) {
  const out = [];
  if (!report) return ["the export returned no report (is this the ArchBuilder version the pipeline knows?)"];
  for (const w of [...(report.warnings || []), ...logWarnings.filter((l) => !(report.warnings || []).some((w) => l.includes(w)))]) out.push(`export warning: ${w}`);
  const dropped = Object.entries(report.props_mesh || {}).filter(([, n]) => n > 0);
  if (dropped.length) out.push(`furniture left out of the game (no vanilla prefab or no room for it): ${dropped.map(([k, n]) => `${n} ${k}`).join(", ")}`);
  const e = spec.export || {};
  if (e.vanilla_props !== false && e.furniture !== false && !props.total) out.push("the export has no vanilla props although furniture and vanilla_props are on");
  if (e.vanilla_props !== false && e.furniture !== false && report.props !== props.total) out.push(`the export reports ${report.props} props but its furniture prefab holds ${props.total}`);
  return out;
}

async function stageImport(ctx) {
  const { importBuilding } = await import("./import.mjs");
  return importBuilding(ctx);
}

// The base-game references ({GUID}path) our files name that the game's resource database does not
// hold with that GUID. Returns null when there is no database to read (the check skips).
export function badGameRefs(refs, db = gameDb()) {
  if (!db) return null;
  const out = [];
  for (const [path, guid] of refs) {
    const real = db.guidOf(path);
    if (real === undefined) out.push(`the base game has no ${path}`);
    else if (real !== guid) out.push(`{${guid}}${path}: the base game's GUID for it is ${real}`);
  }
  return out;
}

async function stagePrefab(ctx) {
  const { root, spec } = ctx;
  const addon = addonPath(root, spec);
  const imp = ctx.fullState.stages.import?.result;
  if (!imp) throw new Error("no import result; run the import stage");
  const problems = [];
  const prefabs = [`${spec.prefab_dir}/${spec.name}.et`, `${spec.prefab_dir}/${spec.name}_Base.et`];
  for (const p of prefabs) if (!existsSync(join(addon, p))) problems.push(`no ${p}`);
  if (problems.length) throw new Error(problems.join("; "));
  // Every reference inside our own files resolves to a .meta in the addon with that GUID.
  const ours = [];
  const walk = (rel) => {
    const abs = join(addon, rel);
    if (!existsSync(abs)) return;
    for (const e of readdirSync(abs, { withFileTypes: true })) {
      const r = `${rel}/${e.name}`;
      if (e.isDirectory()) walk(r);
      else if (/\.(et|emat|meta)$/.test(e.name)) ours.push(r);
    }
  };
  walk(spec.prefab_dir);
  walk(spec.asset_dir);
  walk(materialsDir(spec)); // shared by every building; its .emat files name vanilla textures
  const external = new Map(); // path -> GUID
  for (const rel of ours) {
    for (const m of readFileSync(join(addon, rel), "utf8").matchAll(/\{([0-9A-F]{16})\}([A-Za-z0-9_\-./]+)/g)) {
      const [, guid, path] = m;
      if (rel.endsWith(".meta") && path + ".meta" === rel) continue;
      if (existsSync(join(addon, path))) {
        const g = readGuid(join(addon, path + ".meta"));
        if (g !== guid) problems.push(`${rel} names {${guid}}${path}, but its .meta holds ${g || "no GUID"}`);
      } else if (/^Configs\/System\/ResourceTypes\//.test(path)) {
        continue; // the platform configs every .meta names
      } else if ([spec.asset_dir, spec.prefab_dir, materialsDir(spec)].some((d) => path.startsWith(d + "/"))) {
        problems.push(`${rel} names ${path}, which is not in the addon`); // ours, never the base game's
      } else {
        if (external.has(path) && external.get(path) !== guid) problems.push(`${path} is named with two GUIDs, ${external.get(path)} and ${guid}`);
        external.set(path, guid);
      }
    }
  }
  const base = readFileSync(join(addon, `${spec.prefab_dir}/${spec.name}_Base.et`), "utf8");
  const counts = countOpenings(base);
  if (counts.doors < spec.budget.min_doors) problems.push(`${counts.doors} door set(s), the spec wants ${spec.budget.min_doors}`);
  if (!/MeshObject "\{[0-9A-F]{16}\}" \{\s*Object "\{[0-9A-F]{16}\}/.test(base)) problems.push(`${spec.name}_Base.et has no MeshObject with the model`);
  const model = imp.models?.find((m) => m.xob === `${spec.asset_dir}/${spec.name}.xob`);
  if (!model?.surfaces?.length) problems.push("the compiled model has no collider surface");
  const bad = badGameRefs([...external], ctx.gameDb === undefined ? gameDb() : ctx.gameDb);
  if (bad === null) console.log(`SKIP  base-game check: no game resource database to read (ENFUSION_GAME_PATH); ${external.size} references unchecked`);
  else problems.push(...bad);
  for (const p of problems) console.log(`FAIL  ${p}`);
  console.log(`prefab: ${imp.prefab}; ${counts.doors} door sets, ${counts.windows} windows, ${counts.probes} probe(s); model ${model?.xob} with ${model?.surfaces?.length || 0} collider surfaces; ${external.size} base-game references${bad && !bad.length ? ", every path and GUID found in the game" : ""}`);
  if (problems.length) throw new Error(`${problems.length} prefab check(s) failed`);
  console.log(`place it with: tools\\cli place ground ${imp.prefab} --at <x,z>   (docs/placement-tool.md)`);
  return { prefab: imp.prefab, ...counts, external: external.size, gameChecked: bad !== null };
}

const RUN = { spec: stageSpec, generate: stageGenerate, review: stageReview, export: stageExport, import: stageImport, prefab: stagePrefab };

export async function run(root, slug, { from, only, force, workbench, hooks = {} } = {}) {
  const spec = loadSpec(root, slug);
  const dir = cacheDir(root, slug);
  const fullState = readState(dir);
  let rerun = !!force;
  const list = only ? [only] : STAGES.slice(from ? STAGES.indexOf(from) : 0);
  if (list.includes(undefined) || list.some((s) => !RUN[s])) throw new Error(`stages are: ${STAGES.join(", ")}`);
  let failed = 0;
  for (const stage of list) {
    const done = fullState.stages[stage];
    if (done?.ok && !rerun && !only && stage !== from) {
      console.log(`${stage}: cached (${done.secs}s on ${done.at.slice(0, 16)}); --force reruns it`);
      continue;
    }
    rerun = true;
    const started = Date.now();
    console.log(`--- ${stage}`);
    const ctx = { root, slug, spec, dir, fullState, workbench, ...hooks };
    try {
      if (NEEDS_ARCHBUILDER.has(stage)) {
        const setup = (hooks.archBuilderSetup || archBuilderSetup)();
        if (!setup.ok) throw new ArchBuilderMissing(setup.why);
      }
      const result = await RUN[stage](ctx);
      fullState.stages[stage] = { ok: true, at: new Date().toISOString(), secs: Math.round((Date.now() - started) / 1000), result };
      writeState(dir, fullState);
    } catch (e) {
      if (e instanceof ArchBuilderMissing && NEEDS_ARCHBUILDER.has(stage)) {
        // Not a failure of the building: this machine cannot run the stage. Later stages run if
        // their input is already here (an export made on the ArchBuilder machine).
        console.log(`SKIP  ${stage}: ${e.message}. Run it on the machine with ArchBuilder (docs/building-pipeline/README.md "Setup").`);
        fullState.stages[stage] = { ok: false, skipped: true, at: new Date().toISOString(), secs: 0, error: e.message };
        writeState(dir, fullState);
        continue;
      }
      fullState.stages[stage] = { ok: false, at: new Date().toISOString(), secs: Math.round((Date.now() - started) / 1000), error: e.message.split("\n")[0] };
      writeState(dir, fullState);
      console.log(`FAIL  ${stage}: ${e.message}`);
      failed = 1;
      break;
    }
  }
  return failed;
}

function flag(flags, name) {
  const i = flags.indexOf(name);
  return i >= 0 ? flags[i + 1] : undefined;
}

function cmdStatus(root, slug) {
  const base = join(root, "tools", "buildings");
  const slugs = slug ? [slug] : readdirSync(base).filter((d) => existsSync(join(base, d, "spec.json")));
  for (const s of slugs) {
    const st = readState(cacheDir(root, s));
    console.log(s);
    for (const stage of STAGES) {
      const d = st.stages[stage];
      const word = !d ? "-" : d.ok ? "ok  " : d.skipped ? "SKIP" : "FAIL";
      console.log(`  ${stage.padEnd(9)} ${word}${d ? ` ${String(d.secs).padStart(4)}s  ${d.ok ? "" : d.error}` : ""}`);
    }
  }
  return 0;
}

function cmdDoctor() {
  let bad = 0;
  const say = (ok, what) => { if (!ok) bad++; console.log(`${ok ? "ok  " : "FAIL"}  ${what}`); };
  const s = archBuilderSetup();
  say(existsSync(s.exe), `Blender at ${s.exe} (LF_BLENDER_ARCHBUILDER overrides)`);
  say(existsSync(join(s.ext, "__init__.py")), `ArchBuilder add-on${s.addonVersion ? ` ${s.addonVersion}` : ""} at ${s.ext} (LF_ARCHBUILDER_DIR overrides)`);
  if (s.ok) {
    say(licenseMarks(s).length > 0, "a license file to read the buyer watermark from (the import refuses any file that carries it)");
    try {
      const r = runArchBuilder("generate", { spec: { name: "Doctor", archbuilder: { archetype: "TOWNHOUSE", seed: 1, storeys: 1, width: 6, depth: 6 } }, out: join(ROOT, "tmp", "buildings", "_doctor"), timeoutMs: 120000 });
      say(true, `headless generate works (${r.secs}s), ArchBuilder ${r.version}`);
    } catch (e) {
      say(false, `headless generate: ${e.message.split("\n")[0]}`);
    }
  }
  return bad ? 1 : 0;
}

export async function main(flags, { workbench } = {}) {
  const [sub, ...rest] = flags;
  const root = ROOT;
  if (sub === "selftest") return (await import("../../validation/building-pipeline-selftest.mjs")).main(rest);
  if (sub === "doctor") return cmdDoctor();
  if (sub === "status") return cmdStatus(root, rest[0]);
  if (sub === "run") {
    const wt = await import("../../wt.mjs");
    if (wt.isMainCheckout(root)) { console.log("FAIL  building runs only in a worktree"); return 1; }
    const slug = rest.find((f) => !f.startsWith("--") && f !== flag(rest, "--from") && f !== flag(rest, "--only"));
    if (!slug) { console.log("usage: tools\\cli building run <slug> [--from <stage>] [--only <stage>] [--force]"); return 2; }
    return run(root, slug, { from: flag(rest, "--from"), only: flag(rest, "--only"), force: rest.includes("--force"), workbench });
  }
  console.log(`usage: tools\\cli building run <slug> [--from <stage>] [--only <stage>] [--force]     stages: ${STAGES.join(", ")}
       tools\\cli building status [<slug>] | doctor | selftest
playbook: docs/building-pipeline/README.md`);
  return sub ? 2 : 0;
}
