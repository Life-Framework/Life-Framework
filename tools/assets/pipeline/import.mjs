// Stage "import": copy the cleaned sources into the addon, write the .meta files and the .emat,
// rebuild them in one headless Workbench, then read the compiled .xob back and check it.
import { copyFileSync, existsSync, mkdirSync, readFileSync, readdirSync, statSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { KINDS } from "./kinds.mjs";
import { writeAssetMetas, writeVariantMetas } from "./meta.mjs";
import { checkModel, parse } from "./xob.mjs";

const PLUGIN = "LF_AssetImportPlugin";

// Lines in a Workbench log that mean the import is wrong even though a file came out.
// Only lines between the plugin's start and Workbench's exit count: before it, Workbench lists
// every .meta whose resource is not built yet, and at exit it prints its own UI texture leaks.
export function logProblems(text, name = "") {
  const out = [];
  let live = false;
  for (const line of text.split(/\r?\n/)) {
    if (/LF_ASSET_IMPORT rebuilding/.test(line)) live = true;
    if (/Resource leaks|Game destroyed/.test(line)) live = false;
    if (!live) continue;
    // A source rebuilt before its sibling lists the sibling's .meta as having no resource yet (the
    // folded model's .meta while the textures build). The checks after the build catch a real miss.
    if (/metafile without corresponding resource/.test(line)) continue;
    const named = name && line.includes(name) && /\((E|W)\)/.test(line);
    if (named || /Invalid collider|DefaultGeneratedMaterial|SCRIPT\s+\(E\)|LF_ASSET_IMPORT FAIL|Build failed/.test(line)) out.push(line.trim().replace(/^[\d:.]+\s+/, "").slice(0, 240));
  }
  return [...new Set(out)];
}

// One run's log. console.log already holds every line of script.log and error.log in order; joining
// the three put error.log's start-up lines ("metafile without corresponding resource" for each
// resource not built yet) after script.log's last line, inside the window logProblems reads, and
// failed a clean import of the handcuffs (2026-11-13).
export function runLog(dir) {
  for (const n of ["console.log", "script.log", "error.log"]) {
    if (existsSync(join(dir, n))) return readFileSync(join(dir, n), "utf8");
  }
  return "";
}

function newestLog(profile) {
  const logs = join(profile, "logs");
  if (!existsSync(logs)) return "";
  const dirs = readdirSync(logs).map((d) => join(logs, d)).filter((d) => statSync(d).isDirectory()).sort((a, b) => statSync(b).mtimeMs - statSync(a).mtimeMs);
  for (const d of dirs) {
    const text = runLog(d);
    if (text) return text;
  }
  return "";
}

// The variant texture sets (docs/asset-pipeline/README.md "Variants"): copy the packed maps, write the
// texture metas and one emat per variant. Returns the refs and the .tif paths to rebuild.
export function placeVariants({ root, spec, clean, addon }) {
  const variants = spec.variants || [];
  if (!variants.length) return { refs: [], tifs: [] };
  const tifs = [];
  for (const v of variants) {
    for (const s of ["BCR", "NMO"]) {
      const file = join(addon, spec.asset_dir, "Data", `${spec.name}_${v.name}_${s}.tif`);
      copyFileSync(join(clean, `${spec.name}_${v.name}_${s}.tif`), file);
      tifs.push(file);
    }
  }
  const templates = {};
  for (const v of variants) {
    if (v.emat_template) templates[v.name] = readFileSync(join(root, "tools", "assets", spec.slug, v.emat_template), "utf8");
  }
  const refs = writeVariantMetas(addon, { assetDir: spec.asset_dir, name: spec.name, variants, templates });
  return { refs, tifs };
}

export async function importAsset(ctx) {
  const { root, spec, dir } = ctx;
  if (!ctx.workbench) throw new Error("the import stage needs the Workbench launcher; run it through tools\\cli asset");
  const kind = KINDS[spec.kind];
  const clean = join(dir, "clean");
  const addon = join(root, "addons", "LifeFramework");
  const assetAbs = join(addon, spec.asset_dir);
  mkdirSync(join(assetAbs, "Data"), { recursive: true });
  const placed = placeVariants({ root, spec, clean, addon });
  const textureOnly = kind.fit === "none";
  const report = textureOnly ? null : JSON.parse(readFileSync(join(clean, "report.json"), "utf8"));

  const fbx = join(assetAbs, `${spec.name}.fbx`);
  // With variants the model's own material is the first variant's, so there is no base set.
  const tifs = placed.tifs.length ? [...placed.tifs] : ["BCR", "NMO"].map((s) => join(assetAbs, "Data", `${spec.name}_${s}.tif`));
  if (!textureOnly) copyFileSync(join(clean, `${spec.name}.fbx`), fbx);
  if (!placed.tifs.length) ["BCR", "NMO"].forEach((s, i) => copyFileSync(join(clean, `${spec.name}_${s}.tif`), tifs[i]));

  const gamemat = spec.collider?.gamemat || kind.collider?.gamemat;
  const writeMetas = () => writeAssetMetas(addon, {
    assetDir: spec.asset_dir, name: spec.name,
    collider: { name: report.collider.name, layerPreset: kind.collider.layerPreset, gamemat },
    flags: kind.metaFlags, defaultEmat: placed.refs[0]?.emat,
  });
  let refs = textureOnly ? {} : writeMetas();

  const pluginDir = join(addon, "Scripts", "WorkbenchGame", "ResourceManager");
  mkdirSync(pluginDir, { recursive: true });
  copyFileSync(join(dirname(fileURLToPath(import.meta.url)), "wb", `${PLUGIN}.c`), join(pluginDir, `${PLUGIN}.c`));

  const xob = join(assetAbs, `${spec.name}.xob`);
  const outputs = [xob, ...tifs.map((t) => t.replace(/\.tif$/, ".edds"))];
  const before = outputs.map((f) => (existsSync(f) ? statSync(f).mtimeMs : 0));
  const logFile = join(dir, "import-workbench.log");
  const started = Date.now();
  // One Workbench run per source: with -exitAfterInit only the first resource of a batch is built
  // before Workbench exits (seen 2026-11-12: "Reimporting 3 resources", then one "Rebuilding" line).
  // Textures go first so the model's material finds them.
  const timeoutMs = Number(process.env.LF_ASSET_IMPORT_TIMEOUT_S || 90) * 1000;
  const rebuild = (source) => {
    const { profile } = ctx.workbench(root, PLUGIN, [`-lfAsset0=${source}`], { timeoutMs, logFile });
    const one = newestLog(profile);
    return /LF_ASSET_IMPORT done/.test(one) ? one : `LF_ASSET_IMPORT rebuilding\nLF_ASSET_IMPORT FAIL no done line for ${source}\n${one}`;
  };
  let log = tifs.map(rebuild).join("\n");
  const problems = [];
  if (textureOnly) {
    problems.push(...logProblems(log, spec.name));
    outputs.slice(1).forEach((f, i) => {
      if (!existsSync(f)) problems.push(`Workbench produced no ${f}`);
      else if (statSync(f).mtimeMs <= before[i + 1]) problems.push(`${f} was not rebuilt`);
    });
    const secs = ((Date.now() - started) / 1000).toFixed(0);
    for (const p of problems) console.log(`FAIL  ${p}`);
    console.log(`import: ${secs}s in Workbench; ${placed.refs.map((r) => r.emat).join(", ")}`);
    if (problems.length) throw new Error(`${problems.length} import check(s) failed`);
    return { variants: placed.refs, workbenchSecs: Number(secs) };
  }
  // The model gets a second try. Workbench sometimes starts its own reimport of a changed .fbx
  // beside the plugin's; the two collide, the log says "1 Materials deleted", Workbench hangs at
  // exit and the .xob comes out with no material and no collider (seen 2026-11-12, 1 run in 8).
  for (let attempt = 1; attempt <= 2; attempt++) {
    problems.length = 0;
    const modelLog = rebuild(fbx);
    if (!existsSync(xob)) problems.push(`Workbench produced no ${xob}`);
    else if (statSync(xob).mtimeMs <= before[0]) problems.push(`${xob} was not rebuilt`);
    problems.push(...logProblems(log + "\n" + modelLog, spec.name));
    if (existsSync(xob)) {
      const model = parse(readFileSync(xob));
      problems.push(...checkModel(model, kind, { sockets: report.sockets, emat: refs.emat.replace(/^\{[0-9A-F]+\}/, ""), colliderName: report.collider.name }));
    }
    if (!problems.length || attempt === 2) break;
    console.log(`import: model attempt 1 failed (${problems[0]}); rewriting the .meta and trying once more`);
    refs = writeMetas();
  }
  outputs.slice(1).forEach((f, i) => {
    if (!existsSync(f)) problems.push(`Workbench produced no ${f}`);
    else if (statSync(f).mtimeMs <= before[i + 1]) problems.push(`${f} was not rebuilt`);
  });
  // Further models from the clean stage (a deployable's folded state): same textures and material.
  const variants = {};
  for (const v of report.variants || []) {
    const vfbx = join(assetAbs, `${v.name}.fbx`);
    const vxob = join(assetAbs, `${v.name}.xob`);
    copyFileSync(v.fbx, vfbx);
    const vBefore = existsSync(vxob) ? statSync(vxob).mtimeMs : 0;
    const vMetas = () => writeAssetMetas(addon, {
      assetDir: spec.asset_dir, name: spec.name, model: v.name,
      collider: { name: v.collider.name, layerPreset: kind.collider.layerPreset, gamemat },
      flags: kind.metaFlags, defaultEmat: placed.refs[0]?.emat,
    });
    let vRefs = vMetas();
    let vProblems = [];
    for (let attempt = 1; attempt <= 2; attempt++) {
      vProblems = [];
      const vLog = rebuild(vfbx);
      if (!existsSync(vxob)) vProblems.push(`Workbench produced no ${vxob}`);
      else if (statSync(vxob).mtimeMs <= vBefore) vProblems.push(`${vxob} was not rebuilt`);
      vProblems.push(...logProblems(vLog, v.name));
      if (existsSync(vxob)) vProblems.push(...checkModel(parse(readFileSync(vxob)), kind, { sockets: [], emat: vRefs.emat.replace(/^\{[0-9A-F]+\}/, ""), colliderName: v.collider.name }));
      if (!vProblems.length || attempt === 2) break;
      console.log(`import: ${v.name} attempt 1 failed (${vProblems[0]}); rewriting the .meta and trying once more`);
      vRefs = vMetas();
    }
    problems.push(...vProblems);
    variants[v.name] = vRefs.xob;
  }
  const secs = ((Date.now() - started) / 1000).toFixed(0);
  for (const p of problems) console.log(`FAIL  ${p}`);
  console.log(`import: ${secs}s in Workbench; ${refs.xob}`);
  if (problems.length) throw new Error(`${problems.length} import check(s) failed`);
  return { ...refs, variants, materials: placed.refs, workbenchSecs: Number(secs) };
}
