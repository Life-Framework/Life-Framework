// Stage "import" of tools\cli building: ArchBuilder's staging addon (tmp/buildings/<slug>/export)
// into the real addon.
//
// ArchBuilder writes Assets/ArchBuilder/... and Prefabs/ArchBuilder/... with GUIDs it derives from a
// hash of the path and a fixed salt, so every ArchBuilder user who exports a building of the same
// name gets the same GUIDs. planImport() moves every file to the spec's folders and gives every
// resource a GUID of ours: the one already in the addon's .meta for that path (a re-export keeps the
// references in saved worlds and layers), else a fresh one no repo addon holds (AGENTS.md rule 2).
// References to the base game ({GUID}Prefabs/Structures/...) are left alone and listed, so the
// prefab stage can check each one exists.
import { copyFileSync, existsSync, mkdirSync, readFileSync, readdirSync, rmSync, writeFileSync, statSync } from "node:fs";
import { createHash } from "node:crypto";
import { dirname, join, relative, sep } from "node:path";
import { fileURLToPath } from "node:url";
import { freshGuid, readGuid } from "../../assets/pipeline/meta.mjs";
import { newestLog, logProblems } from "../../assets/pipeline/import.mjs";
import { parse } from "../../assets/pipeline/xob.mjs";
import { addonOf, addonPath, guidsInUse, pluginDir } from "../../assets/pipeline/addon.mjs";
import { licenseMarks, listFiles, scanForMarks } from "./archbuilder.mjs";

const PLUGIN = "LF_AssetImportPlugin";
// Binary formats the export can hold. Anything else is read as text, rewritten and checked, whatever
// its extension: a .layer, .ent or .gamemat must not slip through unrewritten.
const BINARY = /\.(fbx|xob|txo|edds|dds|tif|tiff|png|jpg|tga|wav|ogg)$/i;
export const isText = (rel, buf) => !BINARY.test(rel) && !buf.subarray(0, 8192).includes(0);
const REF_RE = /\{([0-9A-F]{16})\}([A-Za-z0-9_\-./]+)/g;
export const MATERIALS_DIR = "Assets/Buildings/Materials";

// Staging files the import never copies: ArchBuilder's notes (EXPORT_INFO.txt carries the buyer's
// name), its Workbench queue and plugin (we compile with our own), and its project file.
export function skipped(rel) {
  return /(^|\/)EXPORT_INFO\.txt$/i.test(rel) || /^ArchBuilder_import\.(txt|log)$/i.test(rel) ||
    /\.gproj(\.user)?$/i.test(rel) || /^Scripts\/WorkbenchGame\/ArchBuilder\//.test(rel) || /^resourceDatabase\.rdb/i.test(rel);
}

export const materialsDir = (spec) => spec.materials_dir || MATERIALS_DIR;

// Where a staging file lands in the addon, or null when it is not one we know how to place.
export function targetPath(rel, spec) {
  const m = /^(Assets|Prefabs)\/ArchBuilder\/([^/]+)\/(.+)$/.exec(rel);
  if (!m) return null;
  const [, top, folder, rest] = m;
  if (top === "Assets" && folder === spec.name) return `${spec.asset_dir}/${rest}`;
  if (top === "Prefabs" && folder === spec.name) return `${spec.prefab_dir}/${rest}`;
  if (top === "Assets" && folder === "Materials") return `${materialsDir(spec)}/${rest}`;
  return `${top}/Buildings/${folder}/${rest}`; // a shared ArchBuilder folder (door models and the like)
}

const isShared = (target, spec) => !target.startsWith(spec.asset_dir + "/") && !target.startsWith(spec.prefab_dir + "/");
const isSharedMaterial = (target, spec) => target.startsWith(materialsDir(spec) + "/") && /\.emat(\.meta)?$/.test(target);

// The whole move, computed without writing. `files` are { rel, from, to, text? } (text: rewritten
// content; no text: a binary copy). `problems` empty means the plan is safe to write.
export function planImport({ staging, spec, addonDir, used, random }) {
  const problems = [];
  const files = [];
  const all = listFiles(staging).map((f) => ({ from: f, rel: relative(staging, f).split(sep).join("/") }));
  for (const f of all) {
    if (skipped(f.rel)) continue;
    const to = targetPath(f.rel, spec);
    if (!to) {
      problems.push(`unexpected file in the export: ${f.rel} (the import only knows Assets/ArchBuilder and Prefabs/ArchBuilder)`);
      continue;
    }
    if (isShared(to, spec) && !isSharedMaterial(to, spec)) {
      // A model or prefab other buildings would share (ArchBuilder's own door leaves, with vanilla
      // openings off). Workbench re-saves a compiled model's .meta, so "unchanged" cannot be judged,
      // and a second building would rebuild the first one's model: not supported yet.
      problems.push(`${f.rel} would be shared with other buildings (${to}); only shared .emat materials are supported`);
      continue;
    }
    files.push({ ...f, to });
  }
  // Every resource the export defines: its staging GUID -> our GUID and path.
  const map = new Map();
  const taken = new Set(used);
  for (const f of files.filter((x) => x.rel.endsWith(".meta"))) {
    const m = /Name "\{([0-9A-F]{16})\}([^"]+)"/.exec(readFileSync(f.from, "utf8"));
    if (!m) {
      problems.push(`${f.rel} has no Name "{GUID}path" line`);
      continue;
    }
    const [, oldGuid, oldPath] = m;
    if (oldPath + ".meta" !== f.rel) problems.push(`${f.rel} names ${oldPath}, not its own resource`);
    const newPath = f.to.replace(/\.meta$/, "");
    const kept = readGuid(join(addonDir, f.to));
    if (map.has(oldGuid)) problems.push(`the export gives ${oldPath} and ${map.get(oldGuid).oldPath} the same GUID ${oldGuid}`);
    map.set(oldGuid, { oldPath, newPath, guid: kept || freshGuid(taken, random), kept: !!kept });
  }
  const external = new Map(); // path -> guid
  const staged = new RegExp([...map.keys()].join("|") || "^$", "g");
  for (const f of files) {
    const buf = readFileSync(f.from);
    if (!isText(f.rel, buf)) {
      // Binary: copied as is, so it must not hold a staging GUID. (An .fbx records the path it was
      // exported to in its header; Workbench never resolves it, so that path is not checked.)
      const raw = buf.toString("latin1");
      if (map.size && staged.test(raw)) problems.push(`${f.rel} is binary and holds a staging GUID; it cannot be rewritten`);
      staged.lastIndex = 0;
      continue;
    }
    const text = buf.toString("utf8");
    f.text = text.replace(REF_RE, (all, guid, path) => {
      const r = map.get(guid);
      if (r) {
        if (r.oldPath !== path) problems.push(`${f.rel}: {${guid}} names ${path}, but its .meta says ${r.oldPath}`);
        return `{${r.guid}}${r.newPath}`;
      }
      if (/^(Assets|Prefabs)\/ArchBuilder\//.test(path)) problems.push(`${f.rel}: ${all} points at an ArchBuilder resource the export did not write`);
      else if (/^(Assets|Prefabs|Configs|Common|Sounds|UI|Terrains|Particles|scripts)\//i.test(path)) external.set(path, guid);
      return all;
    });
    if (/[/"]ArchBuilder\//.test(f.text)) problems.push(`${f.rel} still names an ArchBuilder path after the move`);
    // A staging GUID left anywhere (a bare "{GUID}", a ref the pattern above did not match) would
    // resolve to another mod's ArchBuilder export: they share GUIDs.
    for (const m of f.text.matchAll(staged)) problems.push(`${f.rel} still holds the staging GUID ${m[0]}`);
  }
  // Shared materials belong to every building that uses them: a different one is refused when
  // another building's model names it. One only this building uses is this building's to change.
  const others = otherBuildingMetas(addonDir, spec);
  for (const f of files) {
    const dest = join(addonDir, f.to);
    if (!isSharedMaterial(f.to, spec) || !existsSync(dest)) continue;
    const ematPath = f.to.replace(/\.meta$/, "");
    if (!others.some((m) => m.text.includes(ematPath))) continue;
    const now = f.text ?? readFileSync(f.from);
    const before = f.text !== undefined ? readFileSync(dest, "utf8") : readFileSync(dest);
    if (f.text !== undefined ? now !== before : !now.equals(before)) {
      problems.push(`${f.to} is shared by other buildings and this export changes it; keep the material or give it a new name`);
    }
  }
  const fresh = [...map.values()].filter((r) => !r.kept).map((r) => r.guid);
  if (new Set(fresh).size !== fresh.length) problems.push("two new resources drew the same GUID");
  for (const g of fresh) if (used.has(g)) problems.push(`new GUID ${g} is already used in the repo`);
  return { files, map, external: [...external].map(([path, guid]) => ({ path, guid })), problems };
}

// The .xob.meta of every other building in Assets/Buildings (their material assignments).
export function otherBuildingMetas(addonDir, spec) {
  const base = join(addonDir, "Assets", "Buildings");
  if (!existsSync(base)) return [];
  const out = [];
  for (const d of readdirSync(base, { withFileTypes: true })) {
    if (!d.isDirectory() || `Assets/Buildings/${d.name}` === spec.asset_dir || `Assets/Buildings/${d.name}` === materialsDir(spec)) continue;
    for (const f of readdirSync(join(base, d.name))) {
      if (f.endsWith(".xob.meta")) out.push({ path: `Assets/Buildings/${d.name}/${f}`, text: readFileSync(join(base, d.name, f), "utf8") });
    }
  }
  return out;
}

// Writes the plan into the addon. Returns the paths written (relative to the addon).
export function writePlan(addonDir, plan) {
  const written = [];
  for (const f of plan.files) {
    const dest = join(addonDir, f.to);
    mkdirSync(dirname(dest), { recursive: true });
    if (f.text !== undefined) writeFileSync(dest, f.text);
    else copyFileSync(f.from, dest);
    written.push(f.to);
  }
  return written;
}

// The .emat references a .meta's MaterialAssigns name.
export const metaMaterials = (metaText) => [...metaText.matchAll(/AssignedMaterial "([^"]+)"/g)].map((m) => m[1]);

// The physics surfaces (.gamemat references) a .meta's GeometryParams ask for, by collider name.
export function metaSurfaces(metaText) {
  return Object.fromEntries([...metaText.matchAll(/GeometryParam (\w+) \{([\s\S]*?)\n    \}/g)]
    .map((m) => [m[1], (/"(\{[0-9A-F]{16}\}[^"]+\.gamemat)"/.exec(m[2]) || [])[1] || ""]));
}

// What the compiled building model is missing against the planned .meta. Empty means usable.
// The xob's string table names one collider only, whatever the count (2026-11-14), so the check
// reads the surfaces it baked: one .gamemat per collider surface the .meta asks for.
export function checkBuildingModel(model, { surfaces = {}, materials = [] }) {
  const out = [];
  if (!model.materials.length) out.push("the .xob bakes no material: it would render white");
  // Each source material bakes the .emat the .meta assigns to it (ours, or a vanilla one such as
  // window glass). One with no assignment bakes nothing or Workbench's default and renders white.
  for (const m of model.materials) {
    if (!m.path || /DefaultGeneratedMaterial/.test(m.path) || (materials.length && !materials.includes(m.path))) {
      out.push(`material ${m.name} bakes ${m.path || "nothing"}, which the .meta does not assign`);
    }
  }
  if (!model.colliders.length) out.push("the .xob has no collider: a player walks through the walls");
  const baked = new Set(model.strings.filter((x) => /\.gamemat$/.test(x)));
  for (const [name, gm] of Object.entries(surfaces)) {
    if (gm && !baked.has(gm)) out.push(`the .xob did not bake ${gm} for collider ${name}`);
  }
  for (const gm of baked) {
    if (!Object.values(surfaces).includes(gm)) out.push(`the .xob bakes ${gm}, which the .meta does not ask for (Workbench's default surface)`);
  }
  return out;
}

// The collider surfaces the planned .meta asks for that Workbench's re-saved .meta lost.
export function metaDrift(planned, saved) {
  const want = metaSurfaces(planned);
  const got = metaSurfaces(saved);
  return Object.keys(want).filter((k) => want[k] !== got[k]).map((k) => `Workbench re-saved collider ${k} with ${got[k] || "no entry"}, not ${want[k]}`);
}

// ---------------------------------------------------------------------- what changed
// A polish iteration changes one thing (a mesh, a material colour, a prop), and recompiling the model
// in Workbench takes one to two minutes. The import writes only the files whose content changed and
// compiles only the models whose source changed. The last good import is recorded in
// tmp/buildings/<slug>/import-manifest.json: a hash of every planned file, and per model the hash of
// its inputs (the .fbx and the planned .xob.meta) and of the .xob and re-saved .meta Workbench made.
export const MANIFEST = "import-manifest.json";
const MANIFEST_VERSION = 1;

// Blender's FBX writer stamps the export time into the header (CreationTimeStamp: Year ... Millisecond,
// each an int32 after its name and an "I"). Zeroed before hashing, so two exports of one scene match.
export function fbxSansTime(buf) {
  const out = Buffer.from(buf);
  const head = out.toString("latin1", 0, Math.min(out.length, 4096));
  for (const m of head.matchAll(/(Year|Month|Day|Hour|Minute|Second|Millisecond)I/g)) out.fill(0, m.index + m[0].length, m.index + m[0].length + 4);
  return out;
}

export const contentHash = (rel, buf) => createHash("sha256").update(/\.fbx$/i.test(rel) ? fbxSansTime(buf) : buf).digest("hex").slice(0, 32);
const fileHash = (abs, rel = abs) => (existsSync(abs) ? contentHash(rel, readFileSync(abs)) : null);
const plannedBytes = (f) => (f.text !== undefined ? Buffer.from(f.text, "utf8") : readFileSync(f.from));
const modelMeta = (fbxRel) => fbxRel.replace(/\.fbx$/i, ".xob.meta");

export function readManifest(dir) {
  try {
    const m = JSON.parse(readFileSync(join(dir, MANIFEST), "utf8"));
    return m.version === MANIFEST_VERSION ? m : null;
  } catch {
    return null;
  }
}

// What this import has to do, from the plan, the addon as it is and the last manifest (null: first
// import, or --full). { write: [plan files], models: [{ rel, why }], same: [rels], hashes }.
// A model's .xob.meta is written only when the model is compiled: the addon holds Workbench's
// re-saved copy, which differs from the planned one by the lines Workbench adds.
export function importDelta(plan, { addonDir, manifest, full = false }) {
  const hashes = {};
  for (const f of plan.files) hashes[f.to] = contentHash(f.to, plannedBytes(f));
  const models = [];
  for (const f of plan.files.filter((x) => /\.fbx$/i.test(x.to))) {
    const rel = f.to;
    const xob = join(addonDir, rel.replace(/\.fbx$/i, ".xob"));
    const was = manifest?.models?.[rel];
    const inputs = `${hashes[rel]}:${hashes[modelMeta(rel)] || ""}`;
    let why = "";
    if (!manifest) why = full ? "--full" : "no earlier import to compare with";
    else if (!was) why = "new model";
    else if (was.inputs !== inputs) why = hashes[rel] !== was.inputs.split(":")[0] ? "the mesh changed" : "its .xob.meta changed (materials or colliders)";
    else if (fileHash(xob) !== was.xob) why = "the .xob in the addon is not the one the last import made";
    else if (fileHash(xob + ".meta") !== was.meta) why = "the .xob.meta in the addon is not the one the last import made";
    if (why) models.push({ rel, why, inputs });
  }
  const compiled = new Set(models.map((m) => modelMeta(m.rel)));
  const metas = new Set(plan.files.filter((x) => /\.fbx$/i.test(x.to)).map((x) => modelMeta(x.to)));
  const write = [];
  const same = [];
  for (const f of plan.files) {
    if (metas.has(f.to)) {
      if (compiled.has(f.to)) write.push(f);
      else same.push(f.to);
      continue;
    }
    if (fileHash(join(addonDir, f.to), f.to) === hashes[f.to]) same.push(f.to);
    else write.push(f);
  }
  return { write, models, same, hashes };
}

// The record of a good import: the planned hashes, and per model its inputs and what is in the addon now.
export function writeManifest(dir, addonDir, delta) {
  const models = {};
  for (const rel of Object.keys(delta.hashes).filter((r) => /\.fbx$/i.test(r))) {
    const xob = join(addonDir, rel.replace(/\.fbx$/i, ".xob"));
    models[rel] = { inputs: `${delta.hashes[rel]}:${delta.hashes[modelMeta(rel)] || ""}`, xob: fileHash(xob), meta: fileHash(xob + ".meta") };
  }
  const m = { version: MANIFEST_VERSION, at: new Date().toISOString(), files: delta.hashes, models };
  writeFileSync(join(dir, MANIFEST), JSON.stringify(m, null, 1) + "\n");
  return m;
}

// One model through Workbench until the .xob holds every planned collider surface and Workbench has
// re-saved the .meta. Used here and by `building compile` on a pool machine. { problems, result }.
export async function compileModel({ root, spec, dir, workbench }, rel, meta) {
  const addon = addonPath(root, spec);
  const plugins = pluginDir(root);
  mkdirSync(plugins, { recursive: true });
  copyFileSync(join(dirname(fileURLToPath(import.meta.url)), "..", "..", "assets", "pipeline", "wb", `${PLUGIN}.c`), join(plugins, `${PLUGIN}.c`));
  const timeoutMs = Number(process.env.LF_BUILDING_IMPORT_TIMEOUT_S || 300) * 1000;
  const fbx = join(addon, rel);
  const xob = fbx.replace(/\.fbx$/i, ".xob");
  const xobRel = rel.replace(/\.fbx$/i, ".xob");
  // Workbench writes a model's collider surfaces after "Build successful". Told to exit after
  // init, it stops part way and the .meta and .xob keep the surfaces of the first one to four
  // colliders only (2026-11-14, docs/architecture/building-pipeline-spike.md). So Workbench stays
  // open until the .xob is rebuilt with every planned surface and the re-saved .meta matches.
  const surfaces = metaSurfaces(meta);
  let before = 0; // set per attempt: attempt 2 must not judge attempt 1's .xob
  let metaWritten = 0;
  const verdict = () => {
    if (!existsSync(xob)) return [`Workbench produced no ${xobRel}`];
    if (statSync(xob).mtimeMs <= before) return [`${xobRel} was not rebuilt`];
    // Done means Workbench has re-saved the .meta itself (it adds its application lines), not that
    // the file we just wrote still matches the plan.
    if (statSync(xob + ".meta").mtimeMs <= metaWritten) return [`Workbench has not re-saved ${xobRel}.meta yet`];
    let model;
    try {
      model = parse(readFileSync(xob));
    } catch (e) {
      return [`${xobRel}: ${e.message}`]; // still being written
    }
    return [...checkBuildingModel(model, { surfaces, materials: metaMaterials(meta) }), ...metaDrift(meta, readFileSync(xob + ".meta", "utf8"))];
  };
  let mine = [];
  for (let attempt = 1; attempt <= 2; attempt++) {
    before = existsSync(xob) ? statSync(xob).mtimeMs : 0;
    writeFileSync(xob + ".meta", meta);
    metaWritten = statSync(xob + ".meta").mtimeMs;
    const run = await workbench(root, PLUGIN, [`-lfAsset0=${fbx}`], {
      timeoutMs, logFile: join(dir, "import-workbench.log"), addon: addonOf(spec), doneWhen: () => verdict().length === 0,
    });
    const log = newestLog(run.profile);
    mine = [];
    if (!/LF_ASSET_IMPORT done/.test(log)) mine.push(`no "LF_ASSET_IMPORT done" line for ${rel} (Workbench log in ${run.profile}/logs)`);
    mine.push(...logProblems(log, spec.name), ...verdict());
    if (run.timedOut) mine.push(`Workbench did not finish ${rel} in ${timeoutMs / 1000}s (LF_BUILDING_IMPORT_TIMEOUT_S)`);
    if (!mine.length || attempt === 2) break;
    console.log(`import: ${rel} attempt ${attempt} failed (${mine[0]}); rewriting the .meta and building again`);
  }
  return { problems: mine, result: modelResult(addon, rel, meta) };
}

function modelResult(addon, rel, meta) {
  const xobRel = rel.replace(/\.fbx$/i, ".xob");
  try {
    const model = parse(readFileSync(join(addon, xobRel)));
    return { xob: xobRel, bounds: model.bounds, materials: model.materials.length, surfaces: Object.values(metaSurfaces(meta)) };
  } catch (e) {
    return { xob: xobRel, error: `${xobRel} cannot be read back: ${e.message}` };
  }
}

// `tools\cli building compile <slug> <model.fbx>...` on the machine that runs Workbench: compiles the
// models whose .fbx and planned .xob.meta the routed sync carried (importBuilding wrote them first).
export async function cmdCompile(root, slug, rels, { workbench }) {
  const desk = await import("../../remote/desktop/in-session.mjs");
  if (desk.needsDesktopSession()) return desk.cliInDesktopSession(root, ["building", "compile", slug, ...rels], { name: `building-${slug}` });
  const spec = JSON.parse(readFileSync(join(root, "tools", "buildings", slug || "", "spec.json"), "utf8"));
  const dir = join(root, "tmp", "buildings", slug);
  mkdirSync(dir, { recursive: true });
  let bad = 0;
  for (const rel of rels) {
    const meta = readFileSync(join(addonPath(root, spec), modelMeta(rel)), "utf8");
    const t0 = Date.now();
    const r = await compileModel({ root, spec, dir, workbench }, rel, meta);
    for (const p of r.problems) console.log(`FAIL  ${p}`);
    if (r.problems.length || r.result.error) bad++;
    console.log(`compile: ${rel} in ${Math.round((Date.now() - t0) / 1000)} s${r.problems.length ? `, ${r.problems.length} problem(s)` : ""}`);
  }
  return bad ? 1 : 0;
}

// The models through Workbench: here when this PC may run one, else on a pool machine with the
// workbench cap (tools/lib/wb-route.mjs), the .xob, its re-saved .meta and the .txo copied back.
async function compileModels(ctx, models, metas) {
  const { root, spec, slug } = ctx;
  const allowed = ctx.wbAllowed ?? (await import("../../lib/wb-route.mjs")).localWorkbench(root).allowed;
  if (allowed) {
    const problems = [];
    for (const m of models) {
      const r = await compileModel(ctx, m.rel, metas[m.rel]);
      problems.push(...r.problems);
      if (r.result.error) problems.push(r.result.error);
    }
    return problems;
  }
  const route = ctx.routeWorkbench ?? (await import("../../lib/wb-route.mjs")).routeWorkbench;
  const r = await route(root, { purpose: `building import ${slug}`, suffix: "-building", args: ["building", "compile", slug, ...models.map((m) => m.rel)], runLocal: async () => 1 });
  if (!r.rDir) return [`the Workbench step found no pool machine to run on (exit ${r.code})`];
  const addon = addonPath(root, spec);
  const remoteAddon = `${r.rDir.replace(/\\/g, "/")}/${relative(root, addon).split(sep).join("/")}`;
  const problems = r.code === 0 ? [] : [`the Workbench step on ${r.machine?.name || "the pool machine"} exited ${r.code}`];
  const fetch = ctx.fetchRemote ?? (async (m, from, to) => (await import("../../remote/testbox.mjs")).scp([`${m.host}:${from}`, to]).status === 0);
  for (const m of models) {
    for (const ext of [".xob", ".xob.meta", ".txo"]) {
      const rel = m.rel.replace(/\.fbx$/i, ext);
      const ok = await fetch(r.machine, `${remoteAddon}/${rel}`, join(addon, rel));
      if (!ok && ext !== ".txo") problems.push(`could not copy ${rel} back from ${r.machine?.name}`);
    }
    const back = join(addon, m.rel.replace(/\.fbx$/i, ".xob"));
    if (existsSync(back)) {
      try {
        problems.push(...checkBuildingModel(parse(readFileSync(back)), { surfaces: metaSurfaces(metas[m.rel]), materials: metaMaterials(metas[m.rel]) }), ...metaDrift(metas[m.rel], readFileSync(back + ".meta", "utf8")));
      } catch (e) {
        problems.push(`${m.rel}: the .xob copied back cannot be read: ${e.message}`);
      }
    }
  }
  return problems;
}

export async function importBuilding(ctx) {
  const { root, spec, dir } = ctx;
  const staging = join(dir, "export");
  if (!existsSync(staging)) throw new Error(`no ArchBuilder export at ${staging}; run the export stage on a machine with ArchBuilder`);
  if (!ctx.workbench) throw new Error("the import stage needs the Workbench launcher; run it through tools\\cli building");
  const addon = addonPath(root, spec);
  const marks = (ctx.licenseMarks || licenseMarks)();
  const copied = listFiles(staging).filter((f) => !skipped(relative(staging, f).split(sep).join("/")));
  const marked = scanForMarks(copied, marks);
  if (marked.length) throw new Error(`refused: ${marked.length} exported file(s) carry the ArchBuilder license watermark (the buyer id or key): ${marked.map((f) => relative(staging, f)).join(", ")}`);
  // With no licence file there is nothing to scan for, and an export made on another machine would
  // go in unchecked. Import on the machine that made the export, or point LF_ARCHBUILDER_DIR at a
  // folder holding the buyer's LICENSE.json.
  if (!marks.length) throw new Error("refused: no ArchBuilder licence file on this machine to read the buyer watermark from, so the export cannot be checked; run the import where ArchBuilder is installed, or set LF_ARCHBUILDER_DIR to a folder with its LICENSE.json");

  const plan = planImport({ staging, spec, addonDir: addon, used: guidsInUse(root) });
  if (plan.problems.length) throw new Error("import plan problems:\n  " + plan.problems.join("\n  "));
  const fbxs = plan.files.filter((f) => /\.fbx$/i.test(f.to)).map((f) => f.to);
  if (!fbxs.length) throw new Error("the export holds no .fbx model");
  const manifest = ctx.full ? null : readManifest(dir);
  const delta = importDelta(plan, { addonDir: addon, manifest, full: !!ctx.full });
  const kept = [...plan.map.values()].filter((r) => r.kept).length;
  const metas = Object.fromEntries(fbxs.map((rel) => [rel, plan.files.find((f) => f.to === modelMeta(rel)).text]));
  rmSync(join(dir, MANIFEST), { force: true }); // an import that stops part way leaves no manifest: the next one compiles
  const written = writePlan(addon, { files: delta.write });
  const started = Date.now();
  if (!delta.write.length && !delta.models.length) {
    console.log(`import: nothing changed since the last import (${plan.files.length} files, ${fbxs.length} model(s)); Workbench skipped`);
  } else {
    console.log(`import: ${written.length} of ${plan.files.length} files changed into ${addonOf(spec)} (${plan.map.size} resources: ${kept} kept their GUID, ${plan.map.size - kept} new); ${plan.external.length} base-game references`);
    for (const m of delta.models) console.log(`        compile ${m.rel}: ${m.why}`);
    const same = fbxs.filter((rel) => !delta.models.some((m) => m.rel === rel));
    if (same.length) console.log(`        ${same.join(", ")} unchanged: kept the compiled .xob${ctx.full ? "" : " (--full compiles it again)"}`);
  }
  const problems = delta.models.length ? await compileModels(ctx, delta.models, metas) : [];
  const results = fbxs.map((rel) => modelResult(addon, rel, metas[rel]));
  for (const r of results) if (r.error) problems.push(r.error);
  const secs = Math.round((Date.now() - started) / 1000);
  for (const p of problems) console.log(`FAIL  ${p}`);
  if (delta.models.length) console.log(`import: ${secs}s in Workbench; ${results.map((r) => `${r.xob} (${r.materials} materials, ${r.surfaces?.length} collider surfaces)`).join(", ")}`);
  if (problems.length) throw new Error(`${problems.length} import check(s) failed`);
  writeManifest(dir, addon, delta);
  const prefab = plan.map.get([...plan.map.keys()].find((g) => plan.map.get(g).newPath === `${spec.prefab_dir}/${spec.name}.et`));
  return {
    written: written.length, resources: plan.map.size, keptGuids: kept, workbenchSecs: secs, compiled: delta.models.map((m) => m.rel), models: results,
    prefab: prefab ? `{${prefab.guid}}${prefab.newPath}` : null, external: plan.external,
  };
}
