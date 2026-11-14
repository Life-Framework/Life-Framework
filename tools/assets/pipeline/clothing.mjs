// Kind "clothing": a garment a character wears (docs/asset-pipeline/clothing.md).
//
//   tools\cli asset new <slug> --kind clothing --base "{GUID}Prefabs/...et" --slot Jacket [--route a|b|c] [--name N]
//   tools\cli asset run <slug> [--from <stage>] [--only <stage>] [--previews]
//
// Every garment starts from a vanilla one (spec.base) and inherits its prefab: the slot, the body
// parts it hides, the inventory item and its pockets, the physics, the sounds, the actions, the
// preview camera and the faction value all come from vanilla. The routes differ in what replaces:
//   a  retexture: new materials (inheriting the vanilla ones) on the vanilla meshes
//   b  modify a mesh: a Bohemia-published source mesh (the Samples) kitbashed, re-weighted to the skeleton
//   c  new mesh: our own model, fitted to the base body and skinned from its weights
// Stages: spec, vanilla (read the base garment from the paks), textures (route a's maps, plus a paint
// template), fit (b and c: weights, LODs, colliders, FBX), import (Workbench), prefab, verify (checks
// against vanilla and the state pictures).
import { copyFileSync, existsSync, mkdirSync, readFileSync, writeFileSync, statSync, readdirSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { BODY_PARTS, SLOTS, describeGarment, eddsToDds, gameData, pathOf } from "./clothing-vanilla.mjs";
import { ematMeta, freshGuid, readGuid, textureMeta, xobMeta, xobMetaIds } from "./meta.mjs";
import { addonOf as targetAddon, addonPath, guidsInUse, pluginDir as wbPluginDir } from "./addon.mjs";
import { resourceMeta } from "./prefab.mjs";
import { parse as parseXob } from "./xob.mjs";

const HERE = dirname(fileURLToPath(import.meta.url));
export const CLOTHING_STAGES = ["spec", "vanilla", "textures", "fit", "import", "prefab", "verify"];
export const ROUTES = { a: "retexture a vanilla garment", b: "modify a Bohemia-published source mesh", c: "a new mesh fitted to the base body" };
// Maps route (a) may repaint: colour maps on the vanilla UV layout. Normal and mask maps stay vanilla.
export const COLOUR_MAPS = ["BCR_1", "BCRMap", "DirtBCRMap"];
// Bohemia's skinning template (Arma Reforger Samples, Arma Public License): the vanilla skeleton and the
// base body with the game's weights. Downloaded once into tmp/assets/_samples/, never committed.
export const SAMPLES = {
  repo: "https://github.com/BohemiaInteractive/Arma-Reforger-Samples",
  raw: "https://raw.githubusercontent.com/BohemiaInteractive/Arma-Reforger-Samples/main/SampleMod_NewCharacter/Assets/Characters",
  files: {
    weights: "SampleCharacter/Character_Weights_Template.blend", body: "SampleCharacter/Character_Template.fbx", jacket: "SampleCharacter/Jacket_Deflated_Template.fbx",
    vest: "Vests/Vest_Sample/VestSample.blend", vest_item: "Vests/Vest_Sample/Vest_SampleVest_01_item.fbx",
    vest_bcr: "Vests/Vest_Sample/Data/SampleVest_01_BCR.tif", vest_nmo: "Vests/Vest_Sample/Data/SampleVest_01_NMO.tif",
  },
};
// The base of a route (b)/(c) garment's own material: what vanilla clothing's MatPBRBasic materials inherit.
export const CHARACTER_MATERIAL = "{5CDDE2709BA9A5EE}Assets/_BaseMaterials/Characters_MatPBRBasic.emat";
// Vanilla game materials for the colliders (read from Jacket_Suit_01.xob and its _item.xob, 2026-11-14).
export const GAMEMATS = {
  worn: "{13581C35484F3000}Common/Materials/Game/fabric.gamemat",
  item: "{ABA11261AC43D0E6}Common/Materials/Game/Fabric/fabric_2mm_min.gamemat",
};
// Two pictures of one state count as the same garment when their block difference (clothing-shots.mjs
// pairDiff, mean |dRGB| over a 16 x 9 grid) stays under these. Measured 2026-11-14 on the parity run
// (vanilla against the same vanilla materials): still states 0.0 to 0.2; prone 1.4 to 1.5 and run 1.2 to
// 1.9, where the animation phase and the grass under the camera move. docs/asset-pipeline/clothing.md "Verify".
export const SAME_LIMIT = { still: 1, motion: 3 };
export const isMotion = (state) => /^(prone|run)_/.test(state);

const isRef = (r, ext) => new RegExp(`^\\{[0-9A-F]{16}\\}[\\w/ .-]+\\.${ext}$`).test(r || "");
const unit = (v) => typeof v === "number" && v >= 0 && v <= 1;
const range = (r) => Array.isArray(r) && r.length === 2 && unit(r[0]) && unit(r[1]) && r[0] <= r[1];

// Everything wrong with a clothing spec, as a list. Empty means the spec stage passes.
export function clothingSpecProblems(spec) {
  const out = [];
  if (!/^[a-z][a-z0-9-]*$/.test(spec.slug || "")) out.push("slug must be lowercase letters, digits and dashes");
  if (!/^[A-Z][A-Za-z0-9]*$/.test(spec.name || "")) out.push("name must be PascalCase letters and digits");
  if (!ROUTES[spec.route]) out.push(`route must be one of ${Object.keys(ROUTES).join(", ")} (${Object.entries(ROUTES).map(([k, v]) => `${k}: ${v}`).join("; ")})`);
  if (!isRef(spec.base, "et") || !/^\{[0-9A-F]{16}\}Prefabs\/Characters\//.test(spec.base || "")) out.push('base must be a vanilla garment prefab as {GUID}Prefabs/Characters/...et (the one this garment inherits)');
  if (!SLOTS[spec.slot]) out.push(`slot must be one of ${Object.keys(SLOTS).join(", ")}`);
  if (spec.hide !== undefined && spec.hide !== null) {
    if (!Array.isArray(spec.hide)) out.push("hide must be null (keep vanilla's) or a list of body parts");
    else for (const p of spec.hide) if (!BODY_PARTS.includes(p)) out.push(`hide: ${p} is not a body part (${BODY_PARTS.join(", ")})`);
  }
  const b = spec.budget || {};
  if (![512, 1024, 2048, 4096].includes(b.texture_px)) out.push("budget.texture_px must be 512, 1024, 2048 or 4096 (vanilla clothing is 2048)");
  for (const [i, m] of (spec.materials || []).entries()) {
    const at = `materials[${i}]`;
    if (!["worn", "item"].includes(m.use)) out.push(`${at}.use must be worn or item`);
    if (m.index !== undefined && !(Number.isInteger(m.index) && m.index >= 0)) out.push(`${at}.index must be a material slot number`);
    for (const [map, how] of Object.entries(m.maps || {})) {
      if (!COLOUR_MAPS.includes(map)) out.push(`${at}.maps.${map}: route a repaints only the colour maps ${COLOUR_MAPS.join(", ")}`);
      if (how.tint) {
        const t = how.tint;
        if (!(Array.isArray(t.rgb) && t.rgb.length === 3 && t.rgb.every(unit))) out.push(`${at}.maps.${map}.tint.rgb must be three numbers 0..1`);
        if (t.strength !== undefined && !unit(t.strength)) out.push(`${at}.maps.${map}.tint.strength must be 0..1`);
        for (const k of ["lum", "sat"]) if (t.mask?.[k] !== undefined && !range(t.mask[k])) out.push(`${at}.maps.${map}.tint.mask.${k} must be [lo, hi] in 0..1`);
      }
      if (how.paint !== undefined && !/^[\w-]+\.png$/.test(how.paint)) out.push(`${at}.maps.${map}.paint names a .png beside the spec`);
    }
  }
  if (spec.route === "b" || spec.route === "c") {
    const m = spec.mesh || {};
    if (!m.source) out.push("mesh.source names the garment's .blend/.fbx/.glb beside the spec (or samples:vest, samples:jacket)");
    if (!m.object) out.push("mesh.object names the worn mesh object in the source");
    if (!m.item_object) out.push("mesh.item_object names the ground (item) mesh object in the source (or in mesh.item_source): vanilla garments have a separate folded model");
    for (const [k, v] of Object.entries(m.maps || {})) {
      if (!["BCRMap", "NMOMap"].includes(k)) out.push(`mesh.maps.${k}: a route ${spec.route} material takes BCRMap and NMOMap`);
      if (!/^(samples:\w+|[\w-]+\.tif)$/.test(v)) out.push(`mesh.maps.${k} names a .tif beside the spec or a samples: file`);
    }
    const r = b.lod_ratios;
    if (!(Array.isArray(r) && r.length >= 2 && r.every((x, k) => x > 0 && x < 1 && (k === 0 || x < r[k - 1])))) out.push("budget.lod_ratios must be at least two falling ratios of LOD0 under 1, for LOD1 and on (vanilla clothing ships 4 or 5 LODs)");
    if (!(b.lod0_tris >= 500 && b.lod0_tris <= 25000)) out.push("budget.lod0_tris must be 500 to 25000 (vanilla jackets are 8,000 to 17,000)");
  }
  if (!/^Assets\//.test(spec.asset_dir || "")) out.push("asset_dir must start with Assets/");
  if (!/^Prefabs\//.test(spec.prefab_dir || "")) out.push("prefab_dir must start with Prefabs/");
  for (const k of ["name_key", "description_key"]) {
    const v = spec.prefab?.[k];
    if (v !== undefined && v !== null && !/^#LF-[\w-]+$/.test(v)) out.push(`prefab.${k} must be a #LF- string key (or null to keep vanilla's)`);
  }
  if (spec.verify?.expect !== undefined && !["same", "changed"].includes(spec.verify.expect)) out.push('verify.expect must be "same" (a parity run) or "changed"');
  return out;
}

export function newClothingSpec({ slug, name, base, slot, route = "a" }) {
  return {
    slug, name, kind: "clothing", route, roadmap: "", concept: "",
    base, slot, hide: null,
    materials: [{ use: "worn", maps: {} }, { use: "item", maps: {} }],
    budget: { texture_px: 2048, lod0_tris: 9000, lod_ratios: [0.5, 0.25, 0.12] },
    mesh: route === "a" ? undefined : { source: "model.blend", object: `${name}`, item_object: `${name}_item`, weights_from: "body", smooth: 2 },
    asset_dir: `Assets/Characters/Uniforms/LF/${name}`, prefab_dir: "Prefabs/Characters/Uniforms/LF",
    prefab: { name_key: null, description_key: null },
    verify: { expect: "changed" },
  };
}

// ------------------------------------------------------------------ stage helpers

const addonOf = (root, spec) => addonPath(root, spec); // the spec's addon (#971), the core by default
const json = (file) => JSON.parse(readFileSync(file, "utf8"));
const save = (file, obj) => { mkdirSync(dirname(file), { recursive: true }); writeFileSync(file, JSON.stringify(obj, null, 1) + "\n"); };
const useName = (m) => `${m.use === "worn" ? "Worn" : "Item"}${m.index ? m.index : ""}`;
const texName = (spec, m, map) => `${spec.name}_${useName(m)}_${map.replace(/_1$/, "").replace(/Map$/, "")}`;

function vanillaRecord(dir) {
  const f = join(dir, "vanilla.json");
  if (!existsSync(f)) throw new Error(`no ${f}; run the vanilla stage first`);
  return json(f);
}

// The vanilla material slot a spec material entry repaints.
function slotOf(vanilla, m) {
  const list = vanilla.materials[m.use] || [];
  const rec = list[m.index || 0];
  if (!rec) throw new Error(`the vanilla ${m.use} model has ${list.length} material(s); materials entry ${m.use}[${m.index || 0}] has none to inherit`);
  return rec;
}

// ------------------------------------------------------------------ stages

async function stageSpec(ctx) {
  const problems = clothingSpecProblems(ctx.spec);
  if (problems.length) throw new Error("spec problems:\n  " + problems.join("\n  "));
  console.log(`spec ok: ${ctx.spec.name}, route ${ctx.spec.route} (${ROUTES[ctx.spec.route]}), ${ctx.spec.slot} on ${pathOf(ctx.spec.base)}`);
  return { route: ctx.spec.route };
}

async function stageVanilla(ctx) {
  const data = await gameData({ root: ctx.root });
  const rec = describeGarment(data, ctx.spec.base);
  const want = SLOTS[ctx.spec.slot].area;
  if (rec.cloth.area !== want) throw new Error(`${pathOf(ctx.spec.base)} goes in ${rec.cloth.area}, the spec's slot ${ctx.spec.slot} takes ${want}`);
  save(join(ctx.dir, "vanilla.json"), rec);
  const tex = (list) => list.map((m) => `${pathOf(m.ref).split("/").pop()} (${m.cls}; ${Object.entries(m.textures).map(([k, t]) => `${k} ${t.width}x${t.height} ${t.format}`).join(", ")})`).join("; ");
  console.log(`vanilla: ${pathOf(rec.ref)} in ${rec.cloth.area}, hides ${(rec.cloth.hides || []).join(" ") || "nothing"}`);
  console.log(`  worn ${pathOf(rec.cloth.wornModel)} (${rec.worn?.nodes} nodes, colliders ${rec.worn?.colliders.join(",") || "none"}); item ${pathOf(rec.cloth.itemModel)} (colliders ${rec.itemModel?.colliders.join(",") || "none"})`);
  console.log(`  worn materials ${tex(rec.materials.worn)}`);
  console.log(`  item materials ${tex(rec.materials.item)}`);
  console.log(`  inventory ${rec.item?.cls || "none"} (${rec.item?.name}), storage ${rec.storage ? `${rec.storage.maxVolume} volume` : "none"}, ${rec.components.length} components`);
  return { area: rec.cloth.area, components: rec.components.length };
}

// The maps to paint and where they go. Pure; the selftest checks it.
export function textureJobs(spec, vanilla, dir, specDir) {
  const items = [];
  for (const m of spec.materials || []) {
    const rec = slotOf(vanilla, m);
    for (const [map, how] of Object.entries(m.maps || {})) {
      const t = rec.textures[map];
      if (!t || t.error) throw new Error(`${pathOf(rec.ref)} has no ${map} texture to repaint (it has ${Object.keys(rec.textures).join(", ")})`);
      const stem = texName(spec, m, map);
      items.push({
        use: m.use, index: m.index || 0, map, vanilla: t.ref, size: [t.width, t.height],
        dds: join(dir, "vanilla", `${stem}.dds`), tif: join(dir, "clean", `${stem}.tif`),
        template: join(dir, "template", `${stem}.png`), swatch: join(dir, "clean", `${stem}_swatch.png`),
        tint: how.tint || null, paint: how.paint ? join(specDir, how.paint) : null,
      });
    }
  }
  return items;
}

async function stageTextures(ctx) {
  const { spec, dir, root } = ctx;
  const vanilla = vanillaRecord(dir);
  const items = textureJobs(spec, vanilla, dir, join(root, "tools", "assets", spec.slug));
  if (!items.length) {
    console.log("textures: no map is repainted; the materials inherit every vanilla map");
    return { items: 0 };
  }
  const data = await gameData({ root });
  for (const f of ["vanilla", "clean", "template"]) mkdirSync(join(dir, f), { recursive: true });
  for (const it of items) {
    writeFileSync(it.dds, eddsToDds(data.read(it.vanilla)));
    if (it.paint && !existsSync(it.paint)) throw new Error(`paint file ${it.paint} is missing`);
  }
  const job = join(dir, "textures-job.json");
  save(job, { items });
  ctx.blender(join(HERE, "blender", "clothing_textures.py"), [job], { log: join(dir, "textures.log") });
  const report = json(join(dir, "clean", "textures.json"));
  for (const r of report) console.log(`  ${r.tif.split(/[\\/]/).pop()}: ${r.size.join("x")}, ${Math.round(r.changed * 100)} % of pixels changed${r.mask_share !== undefined ? `, mask ${Math.round(r.mask_share * 100)} %` : ""}`);
  console.log(`textures: ${items.length} map(s); paint templates on the vanilla UV layout in ${join(dir, "template")} (local only: Bohemia's art, never committed)`);
  return { items: items.length, report };
}

// Bohemia's Samples files, downloaded on first use (Arma Public License; credited in docs/credits.md).
export async function samplesFile(root, key) {
  const rel = SAMPLES.files[key];
  if (!rel) throw new Error(`no Samples file ${key}`);
  const file = join(root, "tmp", "assets", "_samples", rel.split("/").pop());
  if (existsSync(file) && statSync(file).size > 1000) return file;
  mkdirSync(dirname(file), { recursive: true });
  console.log(`fit: downloading ${rel} from ${SAMPLES.repo} (Arma Public License, Bohemia Interactive)`);
  const res = await fetch(`${SAMPLES.raw}/${rel}`, { signal: AbortSignal.timeout(300000) });
  if (!res.ok) throw new Error(`Samples ${rel}: HTTP ${res.status}`);
  writeFileSync(file, Buffer.from(await res.arrayBuffer()));
  return file;
}

// Bohemia's Enfusion Blender Tools as Arma Reforger Tools installs them (LF_EBT_ADDONS overrides).
export function ebtAddons() {
  const dir = process.env.LF_EBT_ADDONS || join(process.env.ENFUSION_WORKBENCH_PATH || "C:/Program Files (x86)/Steam/steamapps/common/Arma Reforger Tools", "Blender", "scripts", "addons");
  if (!existsSync(join(dir, "EnfusionBlenderTools", "__init__.py"))) throw new Error(`Enfusion Blender Tools not found in ${dir} (they ship with Arma Reforger Tools; LF_EBT_ADDONS overrides)`);
  return dir;
}

// A source file named in a spec: "samples:<key>" (Bohemia's Samples) or a file beside the spec.
async function sourceFile(root, spec, name) {
  const file = /^samples:/.test(name) ? await samplesFile(root, name.slice(8)) : join(root, "tools", "assets", spec.slug, name);
  if (!existsSync(file)) throw new Error(`source ${file} is missing`);
  return file;
}

async function stageFit(ctx) {
  const { spec, dir, root } = ctx;
  if (spec.route === "a") {
    console.log("fit: route a keeps the vanilla meshes");
    return { skipped: true };
  }
  const m = spec.mesh;
  const source = await sourceFile(root, spec, m.source);
  const itemSource = m.item_source ? await sourceFile(root, spec, m.item_source) : null;
  const out = join(dir, "fit");
  const slash = (p) => p && p.replaceAll("\\", "/");
  const job = {
    template_blend: slash(await samplesFile(root, "weights")), body_fbx: slash(await samplesFile(root, "body")), ebt_addons: slash(ebtAddons()),
    source: slash(source), item_source: slash(itemSource), out: slash(out),
    name: spec.name, object: m.object, item_object: m.item_object, weights_from: m.weights_from || "body", compare_to: m.compare_to || null,
    smooth: m.smooth ?? 2, clearance_m: m.clearance_m || 0, keep_head: spec.slot === "Hat", max_influences: 4, exporter: m.exporter || "ebt",
    lod_ratios: spec.budget.lod_ratios, item_lod_ratios: [0.5, 0.25],
  };
  save(join(dir, "fit-job.json"), job);
  // EBT_TEST keeps Bohemia's Blender tools from installing PySide6 and opening a Qt window in a headless run.
  const prevTest = process.env.EBT_TEST;
  process.env.EBT_TEST = "1";
  try {
    ctx.blender(join(HERE, "blender", "clothing_fit.py"), [join(dir, "fit-job.json")], { log: join(dir, "fit.log"), timeoutMs: 1200000 });
  } finally {
    if (prevTest === undefined) delete process.env.EBT_TEST;
    else process.env.EBT_TEST = prevTest;
  }
  const report = json(join(out, "fit.json"));
  const lod0 = report.lods[0].tris;
  const problems = [];
  if (lod0 > spec.budget.lod0_tris * 1.1) problems.push(`LOD0 has ${lod0} triangles, the budget is ${spec.budget.lod0_tris}: decimate the source first`);
  if (!report.item) problems.push("no ground model was exported");
  for (const p of problems) console.log(`FAIL  ${p}`);
  console.log(`fit: LODs ${report.lods.map((l) => l.tris).join("/")} tris, ${report.bones_used.length} bones, ${report.colliders.length} FireGeo pieces; item ${report.item ? report.item.lods.map((l) => l.tris).join("/") : "-"}${report.weights_vs_reference ? `; weights vs reference: ${Math.round(report.weights_vs_reference.dominant_bone_agreement * 100)} % same dominant bone` : ""}`);
  if (problems.length) throw new Error(`${problems.length} fit check(s) failed`);
  return { lods: report.lods, item: report.item };
}

// One Workbench rebuild per source, the way import.mjs does it (a batch builds only its first file).
async function rebuildAll(ctx, sources, logFile) {
  const { newestLog } = await import("./import.mjs");
  const PLUGIN = "LF_AssetImportPlugin";
  const pluginDir = wbPluginDir(ctx.root); // always the core's copy; a pack project loads it as a dependency
  mkdirSync(pluginDir, { recursive: true });
  copyFileSync(join(HERE, "wb", `${PLUGIN}.c`), join(pluginDir, `${PLUGIN}.c`));
  const timeoutMs = Number(process.env.LF_ASSET_IMPORT_TIMEOUT_S || 120) * 1000;
  let log = "";
  for (const s of sources) {
    const { profile } = ctx.workbench(ctx.root, PLUGIN, [`-lfAsset0=${s}`], { timeoutMs, logFile, addon: targetAddon(ctx.spec) });
    const one = newestLog(profile);
    log += /LF_ASSET_IMPORT done/.test(one) ? one : `LF_ASSET_IMPORT rebuilding\nLF_ASSET_IMPORT FAIL no done line for ${s}\n${one}`;
  }
  return log;
}

// The emat text of one route (a) material: the vanilla emat as parent, our maps over it. Pure.
export function ematOver(cls, parentRef, maps) {
  const lines = Object.entries(maps).map(([k, v]) => ` ${k} "${v}"\n`).join("");
  return `${cls} : "${parentRef}" {\n${lines}}\n`;
}

async function stageImport(ctx) {
  const { spec, dir, root } = ctx;
  if (!ctx.workbench) throw new Error("the import stage needs the Workbench launcher; run it through tools\\cli asset");
  const vanilla = vanillaRecord(dir);
  const addon = addonOf(root, spec);
  const used = guidsInUse(root);
  const dataDir = join(addon, spec.asset_dir, "Data");
  mkdirSync(dataDir, { recursive: true });
  const guidFor = (rel) => readGuid(join(addon, rel + ".meta")) || freshGuid(used);
  const { logProblems } = await import("./import.mjs");
  const started = Date.now();
  const problems = [];

  // Textures: the painted maps become .edds beside the emats.
  const items = textureJobs(spec, vanilla, dir, join(root, "tools", "assets", spec.slug));
  const edds = {};
  const tifs = [];
  for (const it of items) {
    if (!existsSync(it.tif)) throw new Error(`no ${it.tif}; run the textures stage`);
    const stem = it.tif.split(/[\\/]/).pop().replace(/\.tif$/, "");
    const tif = join(dataDir, `${stem}.tif`);
    copyFileSync(it.tif, tif);
    const res = `${spec.asset_dir}/Data/${stem}.edds`;
    const guid = guidFor(res);
    if (!existsSync(join(addon, res + ".meta"))) writeFileSync(join(addon, res + ".meta"), textureMeta(res, /NMO/.test(it.map) ? "TextureNType.conf" : "TextureColorMap.conf", guid));
    edds[`${it.use}:${it.index}:${it.map}`] = `{${guid}}${res}`;
    tifs.push(tif);
  }

  // Materials: one emat per spec materials entry, inheriting the vanilla one and naming our maps.
  const emats = { worn: {}, item: {} };
  for (const m of spec.materials || []) {
    const rec = slotOf(vanilla, m);
    const maps = {};
    for (const map of Object.keys(m.maps || {})) maps[map] = edds[`${m.use}:${m.index || 0}:${map}`];
    const res = `${spec.asset_dir}/Data/${spec.name}_${useName(m)}.emat`;
    const guid = guidFor(res);
    writeFileSync(join(addon, res), ematOver(rec.cls, rec.ref, maps));
    if (!existsSync(join(addon, res + ".meta"))) writeFileSync(join(addon, res + ".meta"), ematMeta(res, guid));
    emats[m.use][m.index || 0] = `{${guid}}${res}`;
  }

  let log = tifs.length ? await rebuildAll(ctx, tifs, join(dir, "import-workbench.log")) : "";
  for (const t of tifs) {
    const out = t.replace(/\.tif$/, ".edds");
    if (!existsSync(out)) problems.push(`Workbench produced no ${out}`);
  }

  // Routes b and c: the garment's own material (mesh.maps, on its own UV layout), then the worn and the ground model.
  const models = {};
  if (spec.route !== "a") {
    const fit = json(join(dir, "fit", "fit.json"));
    let own = null;
    if (Object.keys(spec.mesh.maps || {}).length) {
      const maps = {};
      const own_tifs = [];
      for (const [map, src] of Object.entries(spec.mesh.maps)) {
        const stem = `${spec.name}_${map.replace(/Map$/, "")}`;
        const tif = join(dataDir, `${stem}.tif`);
        copyFileSync(await sourceFile(root, spec, src), tif);
        const res = `${spec.asset_dir}/Data/${stem}.edds`;
        const guid = guidFor(res);
        if (!existsSync(join(addon, res + ".meta"))) writeFileSync(join(addon, res + ".meta"), textureMeta(res, map === "NMOMap" ? "TextureNType.conf" : "TextureColorMap.conf", guid));
        maps[map] = `{${guid}}${res}`;
        own_tifs.push(tif);
      }
      log += await rebuildAll(ctx, own_tifs, join(dir, "import-workbench.log"));
      for (const t of own_tifs) if (!existsSync(t.replace(/\.tif$/, ".edds"))) problems.push(`Workbench produced no ${t.replace(/\.tif$/, ".edds")}`);
      const res = `${spec.asset_dir}/Data/${spec.name}.emat`;
      const guid = guidFor(res);
      writeFileSync(join(addon, res), ematOver("MatPBRBasic", CHARACTER_MATERIAL, maps));
      if (!existsSync(join(addon, res + ".meta"))) writeFileSync(join(addon, res + ".meta"), ematMeta(res, guid));
      own = `{${guid}}${res}`;
    }
    // Without mesh.maps the model wears the vanilla material: right only when it kept the vanilla UV layout.
    const wornMat = emats.worn[0] || own || slotOf(vanilla, { use: "worn" }).ref;
    const itemMat = emats.item[0] || own || slotOf(vanilla, { use: "item" }).ref;
    const plan = [
      { key: "worn", fbx: `${spec.name}.fbx`, mesh: spec.mesh.mesh_param || spec.name, source: spec.mesh.material || spec.name, emat: wornMat, flags: { ExportSkinning: true, ExportSceneHierarchy: true },
        colliders: fit.colliders.map((name) => ({ name, layerPreset: "FireGeo", gamemat: GAMEMATS.worn })) },
      { key: "item", fbx: `${spec.name}_item.fbx`, mesh: `${spec.name}_item`, source: spec.mesh.item_material || `${spec.name}_item`, emat: itemMat, flags: {},
        colliders: [{ name: fit.item.collider, layerPreset: "ItemFireView", gamemat: GAMEMATS.item }] },
    ];
    for (const p of plan) {
      const fbx = join(addon, spec.asset_dir, p.fbx);
      copyFileSync(join(dir, "fit", p.fbx), fbx);
      const res = `${spec.asset_dir}/${p.fbx.replace(/\.fbx$/, ".xob")}`;
      const guid = guidFor(res);
      const metaFile = join(addon, res + ".meta");
      const xob = join(addon, res);
      // Two tries, as import.mjs does: Workbench sometimes reimports a changed .fbx on its own beside the
      // plugin, logs "1 Materials deleted" and leaves the .xob with no material and no collider (seen here
      // too on 2026-11-14). Rewriting the .meta and building again has fixed it every time.
      for (let attempt = 1; attempt <= 2; attempt++) {
        // The second try touches no file: a changed .meta or .fbx is what sets off Workbench's own reimport.
        if (attempt === 1) {
          const ids = xobMetaIds(existsSync(metaFile) ? readFileSync(metaFile, "utf8") : "", used);
          writeFileSync(metaFile, withMeshParams(xobMeta(res, guid, { materials: [{ source: p.source, emat: p.emat }], colliders: p.colliders, ids, flags: p.flags }), p.mesh));
        }
        log += await rebuildAll(ctx, [fbx], join(dir, "import-workbench.log"));
        if (!existsSync(xob)) continue;
        const model = parseXob(readFileSync(xob));
        models[p.key] = { ref: `{${guid}}${res}`, nodes: model.nodes.map((n) => n.name), materials: model.materials, colliders: model.colliders };
        if (model.materials.length && model.colliders.length) break;
        if (attempt === 1) console.log(`import: ${p.fbx} came out with ${model.materials.length} material(s) and ${model.colliders.length} collider(s); building it once more`);
      }
      if (!models[p.key]) problems.push(`Workbench produced no ${xob}`);
    }
    problems.push(...modelProblems(vanilla, models));
  }
  problems.push(...logProblems(log, spec.name));
  for (const p of problems) console.log(`FAIL  ${p}`);
  const secs = Math.round((Date.now() - started) / 1000);
  console.log(`import: ${tifs.length} texture(s), ${Object.values(emats.worn).length + Object.values(emats.item).length} material(s)${Object.keys(models).length ? `, ${Object.keys(models).join(" and ")} model` : ""} in ${secs}s`);
  const result = { emats, edds, models: Object.fromEntries(Object.entries(models).map(([k, v]) => [k, v.ref])) };
  save(join(dir, "import.json"), result);
  if (problems.length) throw new Error(`${problems.length} import check(s) failed`);
  return result;
}

// A model meta with a MeshParams entry for the mesh named `mesh` (its LOD objects are <mesh>_LOD<n>), as
// Bohemia's metas for the sample vest and helmet have it. Pure.
export function withMeshParams(text, mesh) {
  if (!mesh || /MeshParams \{/.test(text)) return text;
  return text.replace("   GeometryParams {", `   MeshParams {
    MeshParam ${mesh} {
    }
   }
   GeometryParams {`);
}

// What a route (b)/(c) model lacks against the vanilla garment it replaces. Pure.
export function modelProblems(vanilla, models) {
  const out = [];
  if (models.worn) {
    const missing = (vanilla.worn?.nodeNames || []).filter((n) => !models.worn.nodes.includes(n));
    if (missing.length) out.push(`the worn model lacks ${missing.length} node(s) of the vanilla skeleton (${missing.slice(0, 5).join(", ")}): every worn model carries the whole skeleton, helpers included`);
    if (!models.worn.colliders.some((c) => /^UTM_/.test(c))) out.push("the worn model has no UTM_ FireGeo collider: bullets would pass through the garment");
    if (!models.worn.materials.length) out.push("the worn model bakes no material");
  }
  if (models.item) {
    if (!models.item.colliders.length) out.push("the ground model has no collider: it falls through the floor and cannot be picked up");
    if (models.item.nodes.length) out.push("the ground model has bones; vanilla ground models have none");
  }
  return out;
}

// The prefab text. Pure: the selftest checks it. Overrides the vanilla components by their GUIDs.
export function clothingPrefab(spec, vanilla, imp) {
  const L = [];
  L.push(`${vanilla.entityClass} : "${spec.base}" {`);
  if (vanilla.id) L.push(` ID "${vanilla.id}"`);
  L.push(" components {");
  const ownModels = spec.route !== "a" && imp.models?.worn;
  if (ownModels && vanilla.mesh) L.push(`  MeshObject "{${vanilla.mesh.guid}}" {`, `   Object "${imp.models.item}"`, "  }");
  const name = spec.prefab?.name_key, desc = spec.prefab?.description_key;
  if ((name || desc) && vanilla.item?.attributesGuid && vanilla.item?.displayGuid) {
    L.push(`  ${vanilla.item.cls} "{${vanilla.item.guid}}" {`, `   Attributes SCR_ItemAttributeCollection "{${vanilla.item.attributesGuid}}" {`, `    ItemDisplayName UIInfo "{${vanilla.item.displayGuid}}" {`);
    if (name) L.push(`     Name "${name}"`);
    if (desc) L.push(`     Description "${desc}"`);
    L.push("    }", "   }", "  }");
  }
  L.push(`  BaseLoadoutClothComponent "{${vanilla.cloth.guid}}" {`);
  if (ownModels) L.push(`   WornModel "${imp.models.worn}"`, `   ItemModel "${imp.models.item}"`);
  // An override list replaces vanilla's whole list, slot by slot: keep vanilla's emat where the spec repaints nothing.
  for (const use of ["worn", "item"]) {
    if (ownModels) break;
    const ours = imp.emats?.[use] || {};
    if (!Object.keys(ours).length) continue;
    const list = vanilla.materials[use].map((m, i) => ours[i] || m.ref);
    L.push(`   ${use === "worn" ? "WornMaterialsOverride" : "ItemMaterialsOverride"} {`, ...list.map((r) => `    "${r}"`), "   }");
  }
  if (Array.isArray(spec.hide)) L.push("   OverrideMeshesToHide {", `    ${spec.hide.map((p) => `"${p}"`).join(" ")}`, "   }");
  L.push("  }", " }", "}");
  return L.join("\n") + "\n";
}

async function stagePrefab(ctx) {
  const { spec, dir, root } = ctx;
  const vanilla = vanillaRecord(dir);
  const imp = json(join(dir, "import.json"));
  const addon = addonOf(root, spec);
  const res = `${spec.prefab_dir}/${spec.name}.et`;
  mkdirSync(dirname(join(addon, res)), { recursive: true });
  const guid = readGuid(join(addon, res + ".meta")) || freshGuid(guidsInUse(root));
  writeFileSync(join(addon, res), clothingPrefab(spec, vanilla, imp));
  if (!existsSync(join(addon, res + ".meta"))) writeFileSync(join(addon, res + ".meta"), resourceMeta(res, guid, "EntityTemplateResourceClass"));
  const ref = `{${guid}}${res}`;
  save(join(dir, "prefab.json"), { ref });
  console.log(`prefab: ${ref} inherits ${pathOf(spec.base)} (generated from the spec on every run: edit the spec, not the file)`);
  return { ref };
}

// What our prefab lacks against the vanilla garment. Pure: both records come from describeGarment.
export function garmentProblems(spec, vanilla, ours) {
  const out = [];
  const key = (c) => `${c.cls}{${c.guid}}`;
  const have = new Set(ours.components.map(key));
  for (const c of vanilla.components) if (!have.has(key(c))) out.push(`component ${c.cls} {${c.guid}} of the vanilla garment is missing`);
  const extra = ours.components.filter((c) => !vanilla.components.some((v) => key(v) === key(c)));
  for (const c of extra) out.push(`component ${c.cls} {${c.guid}} is not on the vanilla garment: a second component of a class shadows the inherited one`);
  if (ours.cloth.area !== SLOTS[spec.slot].area) out.push(`the garment goes in ${ours.cloth.area}, the spec's slot ${spec.slot} takes ${SLOTS[spec.slot].area}`);
  if ((spec.slot === "Jacket" || spec.slot === "Pants") && !(ours.cloth.hides || []).length) out.push(`a ${spec.slot.toLowerCase()} that hides no body part: the skin pokes through the cloth`);
  if (!ours.item) out.push("no inventory item component: the garment cannot be picked up");
  else {
    if (!ours.item.preview) out.push("no PreviewRenderAttributes: the inventory shows an empty slot");
    if (!ours.item.name) out.push("no display name");
  }
  if (vanilla.storage && !ours.storage) out.push("the vanilla garment has pockets (a storage component) and ours has none");
  if (vanilla.storage && ours.storage && ours.storage.maxVolume !== vanilla.storage.maxVolume) out.push(`pocket volume ${ours.storage.maxVolume} differs from vanilla's ${vanilla.storage.maxVolume}`);
  for (const use of ["worn", "item"]) {
    const model = use === "worn" ? ours.worn : ours.itemModel;
    const over = use === "worn" ? ours.cloth.wornMaterialsOverride : ours.cloth.itemMaterialsOverride;
    if (model?.missing) out.push(`the ${use} model ${pathOf(model.ref)} does not resolve`);
    if (over && model && !model.missing && over.length !== model.materials.length) out.push(`${use} material override has ${over.length} entr${over.length === 1 ? "y" : "ies"}, the model has ${model.materials.length} material slot(s): the engine maps them by order`);
    for (const m of ours.materials[use]) {
      if (m.missing) out.push(`${use} material ${pathOf(m.ref)} does not resolve`);
      for (const [k, t] of Object.entries(m.textures || {})) {
        if (t.error) out.push(`${use} material ${pathOf(m.ref)}: ${k} ${pathOf(t.ref)} does not resolve (${t.error})`);
        else if (spec.budget?.texture_px && /BCR/.test(k) && Math.max(t.width, t.height) < spec.budget.texture_px) out.push(`${use} ${k} is ${t.width}x${t.height}, under the budget's ${spec.budget.texture_px}`);
      }
    }
  }
  if (!ours.itemModel?.colliders?.length) out.push("the ground model has no collider");
  return out;
}

async function stageVerify(ctx) {
  const { spec, dir, root } = ctx;
  const vanilla = vanillaRecord(dir);
  const { ref } = json(join(dir, "prefab.json"));
  const data = await gameData({ root, addonDir: addonOf(root, spec) }); // the prefab the prefab stage wrote is in the target addon (#971)
  const ours = describeGarment(data, ref);
  save(join(dir, "ours.json"), ours);
  const problems = garmentProblems(spec, vanilla, ours);
  const staticCount = problems.length;
  for (const p of problems) console.log(`FAIL  ${p}`);
  console.log(`verify: ${problems.length ? problems.length + " problem(s)" : "every vanilla component, the slot, the hidden parts, the pockets, the preview and the materials check out"} against ${pathOf(spec.base)}`);
  if (ctx.flags?.includes("--no-shots")) return { problems, shots: "skipped (--no-shots)" };

  // The pictures: vanilla and ours, every state, from the same mannequin on the same spot.
  const shots = await import("./clothing-shots.mjs");
  const out = join(dir, "verify");
  // --reuse-shots measures and sheets the pictures of the last run again without a Workbench (after a limit change).
  const reuse = ctx.flags?.includes("--reuse-shots") && existsSync(join(out, "vanilla__idle_front.png"));
  const r = reuse
    ? { shots: Object.fromEntries(readdirSync(out).filter((f) => /__.+\.png$/.test(f)).map((f) => [f.replace(/\.png$/, ""), join(out, f)])), status: { errors: [] }, wears: [] }
    : await shots.shoot(root, { garments: [{ tag: "vanilla", prefab: spec.base }, { tag: "ours", prefab: ref }], outDir: out });
  const { decodePng } = await import("../../visual/look.mjs");
  const states = shots.stateNames();
  const rows = [];
  for (const s of states) {
    const a = r.shots[`vanilla__${s}`], b = r.shots[`ours__${s}`];
    if (!a || !b) { rows.push({ state: s, missing: !a ? "vanilla" : "ours" }); continue; }
    rows.push({ state: s, vanilla: a, ours: b, ...shots.pairDiff(decodePng(readFileSync(a)), decodePng(readFileSync(b))) });
  }
  const { image } = await import("../../../agent-box/shots.mjs");
  const sheets = [];
  for (const [label, filter] of [["worn", /^(idle|crouch)_/], ["motion", /^(prone|run)_/], ["ground", /^ground_/]]) {
    const pick = rows.filter((x) => filter.test(x.state) && x.vanilla);
    if (!pick.length) continue;
    const file = join(out, `sheet-${label}.jpg`);
    image({ op: "sheet", tiles: pick.flatMap((x) => [{ file: x.vanilla, label: `vanilla  ${x.state}`, marks: [] }, { file: x.ours, label: `${spec.name}  ${x.state}  diff ${x.mean}`, marks: [] }]), out: file, cols: 4, maxWidth: 1920, quality: 82 });
    sheets.push(file);
  }
  for (const x of rows) console.log(`  ${x.state.padEnd(14)} ${x.missing ? `MISSING (${x.missing})` : `diff ${String(x.mean).padStart(5)} (max block ${x.max})`}`);
  const expect = spec.verify?.expect || "changed";
  const missing = rows.filter((x) => x.missing);
  if (missing.length) problems.push(`${missing.length} state picture(s) missing: ${missing.map((x) => x.state).join(", ")}`);
  if (r.status.errors.length) problems.push(...r.status.errors.map((e) => `shots: ${e}`));
  const worn = rows.filter((x) => !x.missing && !x.state.startsWith("ground"));
  const limit = (x) => (isMotion(x.state) ? SAME_LIMIT.motion : SAME_LIMIT.still);
  if (expect === "same") for (const x of rows.filter((y) => !y.missing && y.mean > limit(y))) problems.push(`${x.state} differs from vanilla by ${x.mean} (limit ${limit(x)}): the parity run should look the same`);
  if (expect === "changed" && worn.length && worn.filter((x) => !isMotion(x.state)).every((x) => x.mean <= limit(x))) problems.push("every still worn picture looks like vanilla: the materials did not take");

  // The inventory preview needs a game client (Workbench play draws no UI).
  let previews = null;
  if (ctx.flags?.includes("--previews")) {
    const pdir = join(out, "previews");
    const { spawnSync } = await import("node:child_process");
    const res = spawnSync(process.execPath, [join(root, "agent-box", "shots.mjs"), "previews", "--prefabs", `Vanilla=${spec.base},${spec.name}=${ref}`, "--out", pdir], { stdio: "inherit", windowsHide: true, cwd: root });
    previews = res.status === 0 ? join(pdir, "previews-1.png") : `failed (exit ${res.status})`;
    if (res.status !== 0) problems.push(`inventory previews: ${previews}`);
  } else console.log("  inventory preview: run with --previews (opens a session client: tools\\cli shots previews)");

  save(join(out, "report.json"), { spec: spec.slug, expect, limit: SAME_LIMIT, rows, sheets, previews, problems, wears: r.wears });
  for (const s of sheets) console.log(`  sheet: ${s}`);
  for (const p of problems.slice(staticCount)) console.log(`FAIL  ${p}`);
  if (problems.length) throw new Error(`${problems.length} verify problem(s); report ${join(out, "report.json")}`);
  return { sheets, rows: rows.length, previews };
}

export const CLOTHING_RUN = { spec: stageSpec, vanilla: stageVanilla, textures: stageTextures, fit: stageFit, import: stageImport, prefab: stagePrefab, verify: stageVerify };
