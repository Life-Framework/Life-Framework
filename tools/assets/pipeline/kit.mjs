// tools\cli asset kit <slug> [--only build|import]
// A kit is several models that share materials and are built by one Blender script of their own
// (tools/assets/<slug>/kit.json and its "script"), instead of one generated model through the
// clean stage: a room, a wall with a tunnel, furniture made of boxes. Generated detail (the
// bank vault door) comes in as a GLB the script reads. The import half writes every .meta and
// .emat and rebuilds the sources in a hidden Workbench, as import.mjs does for one model.
// First kit: the bank vault (docs/asset-pipeline/README.md "Kits").
import { copyFileSync, existsSync, mkdirSync, readFileSync, readdirSync, statSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { ematMeta, ematText, freshGuid, readGuid, textureMeta, usedGuids, xobMeta, xobMetaIds } from "./meta.mjs";
import { logProblems, runLog } from "./import.mjs";
import { parse, worldPositions } from "./xob.mjs";

const HERE = dirname(fileURLToPath(import.meta.url));
const PLUGIN = "LF_AssetImportPlugin";

export function kitProblems(kit) {
  const out = [];
  if (!/^[a-z][a-z0-9-]*$/.test(kit.slug || "")) out.push("slug must be lowercase letters, digits and dashes");
  if (!/^Assets\//.test(kit.asset_dir || "")) out.push("asset_dir must start with Assets/");
  if (!/^Prefabs\//.test(kit.prefab_dir || "")) out.push("prefab_dir must start with Prefabs/");
  if (!kit.script) out.push("script names the Blender build script beside kit.json");
  for (const [name, c] of Object.entries(kit.colliders || {})) {
    if (!/^[A-Z][A-Za-z0-9]*$/.test(name)) out.push(`model ${name} must be PascalCase`);
    if (!c.layerPreset || !/^\{[0-9A-F]{16}\}.+\.gamemat$/.test(c.gamemat || "")) out.push(`model ${name} needs a layerPreset and a {GUID}path gamemat`);
  }
  return out;
}

// The text files one model needs: its .xob.meta with every material and collider. Material ids
// already in the meta are kept, so a re-import does not churn them.
export function kitModelMeta(text, used, { res, guid, materials, colliders, flags }) {
  const ids = xobMetaIds(text, used);
  while (ids.assign.length < materials.length) ids.assign.push(freshGuid(used));
  ids.assign = ids.assign.slice(0, materials.length);
  return xobMeta(res, guid, { materials, colliders, ids, flags });
}

function writeMetas(root, kit, report) {
  const addon = join(root, "addons", "LifeFramework");
  const data = `${kit.asset_dir}/Data`;
  const used = usedGuids(addon);
  const put = (rel, text) => writeFileSync(join(addon, rel), text);
  const guidFor = (rel) => readGuid(join(addon, rel + ".meta")) || freshGuid(used);
  const emats = {};
  for (const mat of Object.keys(report.materials)) {
    const tex = {};
    for (const [suffix, conf] of [["BCR", "TextureColorMap.conf"], ["NMO", "TextureNType.conf"]]) {
      const res = `${data}/${mat}_${suffix}.edds`;
      const guid = guidFor(res);
      if (!existsSync(join(addon, res + ".meta"))) put(res + ".meta", textureMeta(res, conf, guid));
      tex[suffix.toLowerCase()] = `{${guid}}${res}`;
    }
    const res = `${data}/${mat}.emat`;
    const guid = guidFor(res);
    if (!existsSync(join(addon, res))) put(res, ematText(tex));
    if (!existsSync(join(addon, res + ".meta"))) put(res + ".meta", ematMeta(res, guid));
    emats[mat] = `{${guid}}${res}`;
  }
  const xobs = {};
  for (const [name, m] of Object.entries(report.models)) {
    const res = `${kit.asset_dir}/${name}.xob`;
    const guid = guidFor(res);
    const metaFile = join(addon, res + ".meta");
    const c = kit.colliders[name];
    const text = kitModelMeta(existsSync(metaFile) ? readFileSync(metaFile, "utf8") : "", used, {
      res, guid,
      materials: m.materials.map((s) => ({ source: s, emat: emats[s] })),
      colliders: m.colliders.map((n) => ({ name: n, layerPreset: c.layerPreset, gamemat: c.gamemat })),
      flags: m.bones ? { ExportSceneHierarchy: true, ExportSkinning: true } : {},
    });
    put(res + ".meta", text);
    xobs[name] = `{${guid}}${res}`;
  }
  return { emats, xobs };
}

function build(root, kit, out, { runBlender }) {
  mkdirSync(out, { recursive: true });
  const job = join(out, "job.json");
  writeFileSync(job, JSON.stringify({ kit, root, out, blender_lib: join(HERE, "blender") }, null, 1));
  runBlender(join(root, "tools", "assets", kit.slug, kit.script), [job], { log: join(out, "blender.log"), timeoutMs: 1200000 });
  const report = JSON.parse(readFileSync(join(out, "report.json"), "utf8"));
  for (const [name, m] of Object.entries(report.models)) {
    console.log(`  ${name.padEnd(18)} LODs ${m.lods.map((l) => l.tris).join("/")} tris, ${m.colliders.length} collider(s), ${m.materials.join(" ")}`);
  }
  console.log(`build: review ${join(out, "review")}`);
  return report;
}

function importKit(root, kit, out, report, { workbench }) {
  if (!workbench) throw new Error("the import needs the Workbench launcher; run it through tools\\cli asset kit");
  const addon = join(root, "addons", "LifeFramework");
  const abs = join(addon, kit.asset_dir);
  mkdirSync(join(abs, "Data"), { recursive: true });
  const sources = [];
  for (const mat of Object.keys(report.materials)) {
    for (const s of ["BCR", "NMO"]) {
      const dst = join(abs, "Data", `${mat}_${s}.tif`);
      copyFileSync(join(out, "tex", `${mat}_${s}.tif`), dst);
      sources.push(dst);
    }
  }
  const models = Object.keys(report.models).map((name) => {
    const dst = join(abs, `${name}.fbx`);
    copyFileSync(report.models[name].fbx, dst);
    return { name, fbx: dst, xob: join(abs, `${name}.xob`) };
  });
  let refs = writeMetas(root, kit, report);
  const plugin = join(addon, "Scripts", "WorkbenchGame", "ResourceManager");
  mkdirSync(plugin, { recursive: true });
  copyFileSync(join(HERE, "wb", `${PLUGIN}.c`), join(plugin, `${PLUGIN}.c`));
  const logFile = join(out, "import-workbench.log");
  const timeoutMs = Number(process.env.LF_ASSET_IMPORT_TIMEOUT_S || 120) * 1000;
  // One Workbench run per source: with -exitAfterInit only the first of a batch is built (import.mjs).
  const rebuild = (source) => {
    const { profile } = workbench(root, PLUGIN, [`-lfAsset0=${source}`], { timeoutMs, logFile });
    const logs = join(profile, "logs");
    const dirs = readdirSync(logs).map((d) => join(logs, d)).filter((d) => statSync(d).isDirectory()).sort((a, b) => statSync(b).mtimeMs - statSync(a).mtimeMs);
    const one = dirs.length ? runLog(dirs[0]) : "";
    return /LF_ASSET_IMPORT done/.test(one) ? one : `LF_ASSET_IMPORT rebuilding\nLF_ASSET_IMPORT FAIL no done line for ${source}\n${one}`;
  };
  // An output counts when it is newer than its source copied above. The first Workbench run often
  // imports every new source it finds at start-up; a later rebuild of an unchanged source does not
  // rewrite the file, so "newer than before this rebuild" would fail a good import.
  const fresh = (out, src) => existsSync(out) && statSync(out).mtimeMs >= statSync(src).mtimeMs;
  const problems = [];
  for (const src of sources) {
    const edds = src.replace(/\.tif$/, ".edds");
    const log = rebuild(src);
    problems.push(...logProblems(log));
    if (!fresh(edds, src)) problems.push(`Workbench did not build ${edds}`);
    console.log(`  texture ${src.split(/[\\/]/).pop()} ${existsSync(edds) ? "ok" : "MISSING"}`);
  }
  for (const m of models) {
    for (let attempt = 1; attempt <= 2; attempt++) {
      const found = [...logProblems(rebuild(m.fbx), m.name)];
      if (!fresh(m.xob, m.fbx)) found.push(`Workbench did not build ${m.xob}`);
      else found.push(...checkKitModel(parse(readFileSync(m.xob)), report.models[m.name], refs.emats));
      if (!found.length || attempt === 2) {
        problems.push(...found);
        console.log(`  model ${m.name} ${found.length ? "FAIL" : "ok"}`);
        break;
      }
      // Workbench sometimes starts its own reimport beside the plugin's (import.mjs): rewrite and retry once.
      console.log(`  model ${m.name}: attempt 1 failed (${found[0]}); rewriting the .meta and trying once more`);
      refs = writeMetas(root, kit, report);
    }
  }
  return { problems, refs };
}

// What a built kit model must hold: every material it was exported with, every collider, the bones.
export function checkKitModel(model, info, emats) {
  const out = [];
  for (const s of info.materials) {
    const path = (emats[s] || "").replace(/^\{[0-9A-F]+\}/, "");
    if (!model.materials.some((m) => (m.path || "").endsWith(path))) out.push(`${info.fbx}: the .xob does not bake ${path}`);
  }
  for (const c of info.colliders) if (!model.colliders.includes(c)) out.push(`the .xob has no collider ${c} (Workbench logs "Invalid collider" when it drops one)`);
  const pos = worldPositions(model);
  for (const b of info.bones || []) if (!pos[b.name]) out.push(`the .xob has no bone ${b.name}`);
  return out;
}

export async function main(root, args, { workbench, runBlender }) {
  const slug = args.find((a) => !a.startsWith("--"));
  const only = args.includes("--only") ? args[args.indexOf("--only") + 1] : null;
  const file = slug && join(root, "tools", "assets", slug, "kit.json");
  if (!file || !existsSync(file)) {
    console.log("usage: tools\\cli asset kit <slug> [--only build|import]   (needs tools/assets/<slug>/kit.json)");
    return 2;
  }
  const kit = JSON.parse(readFileSync(file, "utf8"));
  const bad = kitProblems(kit);
  if (bad.length) { for (const p of bad) console.log(`FAIL  ${p}`); return 1; }
  const out = join(root, "tmp", "assets", slug, "kit");
  const report = only === "import" ? JSON.parse(readFileSync(join(out, "report.json"), "utf8")) : build(root, kit, out, { runBlender });
  if (only === "build") return 0;
  const { problems, refs } = importKit(root, kit, out, report, { workbench });
  for (const p of problems) console.log(`FAIL  ${p}`);
  for (const [name, ref] of Object.entries(refs.xobs)) console.log(`  ${name} ${ref}`);
  return problems.length ? 1 : 0;
}
