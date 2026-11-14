// tools\cli asset <command>: roadmap item -> Meshy -> headless Blender -> Workbench import -> prefab.
// Playbook for the judgement steps: docs/asset-pipeline/README.md.
//
//   asset new <slug> --kind weapon|gadget|deployable --prompt "..." [--name N] [--no-run]
//   asset run <slug> [--from <stage>] [--only <stage>] [--force] [--pick]
//   asset status [<slug>]      asset doctor      asset selftest
//
// The spec is tools/assets/<slug>/spec.json (tracked). Everything a stage produces is cached in
// tmp/assets/<slug>/ and a finished stage is skipped until --force or an earlier stage reruns.
import { existsSync, mkdirSync, readFileSync, writeFileSync, readdirSync, renameSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { spawnSync } from "node:child_process";
import * as meshy from "./meshy.mjs";
import { KINDS } from "./kinds.mjs";
import { BUDGETS, checkBudget, defaultBudget } from "./budgets.mjs";
import * as concepts from "./concepts.mjs";
import { pickedImage } from "./concepts.mjs";
import { materialProblems, resolveMaterials, variantPlans } from "./materials.mjs";
import { resolveFfmpeg } from "../../film/film.mjs";
import { animProblems, hasAnim, withAnim } from "./anim.mjs";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..", "..");
export const STAGES = ["spec", "concept", "generate", "remesh", "clean", "import", "prefab", "anim"];

export const specFile = (root, slug) => join(root, "tools", "assets", slug, "spec.json");
export const cacheDir = (root, slug) => join(root, "tmp", "assets", slug);

export function loadSpec(root, slug) {
  const file = specFile(root, slug);
  if (!existsSync(file)) throw new Error(`no spec at ${file}; start with: tools\\cli asset new ${slug} --kind ... --prompt "..."`);
  return JSON.parse(readFileSync(file, "utf8"));
}

// Everything wrong with a spec, as a list. Empty means the spec stage passes.
export function specProblems(spec) {
  const out = [];
  if (!/^[a-z][a-z0-9-]*$/.test(spec.slug || "")) out.push("slug must be lowercase letters, digits and dashes");
  if (!/^[A-Z][A-Za-z0-9]*$/.test(spec.name || "")) out.push("name must be PascalCase letters and digits (it becomes file and material names)");
  const kind = KINDS[spec.kind];
  if (!kind) out.push(`kind must be one of ${Object.keys(KINDS).join(", ")}`);
  const blender = !!spec.source?.blender; // modelled or painted by our own script: no Meshy call, no credits
  const textureOnly = kind?.fit === "none";
  if (!spec.prompt) out.push("prompt is empty");
  else if (spec.prompt.length > 800) out.push(`prompt is ${spec.prompt.length} characters; Meshy allows 800`);
  if ((spec.texture_prompt || "").length > 800) out.push("texture_prompt is over 800 characters");
  const s = spec.size_m || {};
  const b = spec.budget || {};
  if (!textureOnly) {
    for (const k of ["length", "height", "width"]) {
      if (!(s[k] > 0.005 && s[k] < 20)) out.push(`size_m.${k} must be a size in metres`);
    }
    if (!(b.lod0_tris >= 100 && b.lod0_tris <= 60000)) out.push("budget.lod0_tris must be 100 to 60000");
    if (kind) out.push(...checkBudget(spec));
    if (kind && spec.meshy?.target_polycount && spec.meshy.target_polycount * Math.max(spec.fit?.repeat || 1, 1) < b.lod0_tris) out.push(`meshy.target_polycount ${spec.meshy.target_polycount} is under budget.lod0_tris ${b.lod0_tris}: the generator would hand over less than LOD0 keeps`);
  }
  if (![256, 512, 1024, 2048].includes(b.texture_px)) out.push("budget.texture_px must be 256, 512, 1024 or 2048");
  if (!(b.credits >= (blender ? 0 : 1) && b.credits <= 300)) out.push(`budget.credits must be ${blender ? 0 : 1} to 300 (the per-asset cap)`);
  if (textureOnly && !blender) out.push("a material asset is painted by a Blender script: set source.blender");
  if (blender && !/^[\w-]+\.py$/.test(spec.source.blender)) out.push("source.blender names a .py file beside the spec");
  for (const t of spec.source?.textures || []) {
    if (!/^[A-Za-z]+\d+[A-Z]?$/.test(t.id || "")) out.push(`source.textures: ${JSON.stringify(t.id)} is not an ambientCG asset id (Metal046B)`);
  }
  const vnames = new Set();
  for (const v of spec.variants || []) {
    if (!/^[A-Z][A-Za-z0-9]*$/.test(v.name || "")) out.push(`variant ${v.name || "?"}: name must be PascalCase`);
    if (vnames.has(v.name)) out.push(`variant ${v.name} is listed twice`);
    vnames.add(v.name);
    if (v.emat !== undefined && !/^Assets\/.+\.emat$/.test(v.emat)) out.push(`variant ${v.name}: emat must be an Assets/... .emat path`);
  }
  if (textureOnly && !vnames.size) out.push("a material asset needs at least one variant");
  if (spec.prompt && !/no text/i.test(spec.prompt)) out.push('prompt should say "no text, no logos" (generated lettering is garbled)');
  for (const p of spec.parts || []) {
    if (!p.name || !p.bone || !Array.isArray(p.box) || p.box.length !== 6) out.push(`part ${p.name || "?"} needs name, bone and box [x0,y0,z0,x1,y1,z1]`);
    else if (kind && !kind.skeleton?.some((n) => n.name === p.bone)) out.push(`part ${p.name}: bone ${p.bone} is not in the ${spec.kind} skeleton`);
  }
  out.push(...materialProblems(spec));
  out.push(...animProblems(spec));
  if (!/^Assets\//.test(spec.asset_dir || "")) out.push("asset_dir must start with Assets/");
  if (!/^Prefabs\//.test(spec.prefab_dir || "")) out.push("prefab_dir must start with Prefabs/");
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
  const problems = specProblems(ctx.spec);
  if (problems.length) throw new Error("spec problems:\n  " + problems.join("\n  "));
  const est = ctx.spec.source?.blender ? 0 : meshy.estimate(ctx.spec);
  if (est > ctx.spec.budget.credits) throw new Error(`one attempt costs about ${est} credits, over the spec's budget of ${ctx.spec.budget.credits}`);
  console.log(`concept ok: ${ctx.spec.name} (${ctx.spec.kind}), ${ctx.spec.prompt.length}-char prompt, about ${est} credits per attempt`);
  return { estimate: est };
}

// tools/assets/<slug>/meshy.json is the committed provenance record: one row per paid generation,
// kept across attempts. docs/credits.md credits the asset "generated with Meshy"; this file is
// where its generation ids live (docs/decisions.md, 2026-11-13).
export function recordProvenance(root, spec, ids, now = new Date()) {
  const file = join(root, "tools", "assets", spec.slug, "meshy.json");
  const rec = existsSync(file) ? JSON.parse(readFileSync(file, "utf8")) : { slug: spec.slug, generations: [] };
  const row = {
    date: now.toISOString().slice(0, 10), tool: ids.imageTaskId ? "Meshy image-to-3D" : "Meshy text-to-3D", model: spec.meshy?.ai_model || "meshy-6-lite",
    previewId: ids.previewId, refineId: ids.refineId, prompt: spec.prompt, texturePrompt: spec.texture_prompt,
  };
  if (ids.imageTaskId) { // image-to-3D: one task, from a concept picture (tools/assets/<slug>/concepts.json)
    delete row.previewId; delete row.refineId; delete row.prompt;
    Object.assign(row, { imageTaskId: ids.imageTaskId, concept: ids.concept, conceptTaskId: ids.conceptTaskId, conceptPrompt: ids.conceptPrompt });
  }
  const same = (x) => (ids.imageTaskId ? x.imageTaskId === ids.imageTaskId : x.refineId === row.refineId);
  const at = rec.generations.findIndex(same);
  if (at >= 0) rec.generations[at] = { ...rec.generations[at], ...row };
  else rec.generations.push(row);
  writeFileSync(file, JSON.stringify(rec, null, 2) + "\n");
}

// ambientCG serves every asset as CC0 1.0 (https://docs.ambientcg.com/license/). The zip also holds a
// .blend and USD files; only the JPG maps are unpacked, and nothing from the zip is opened in Blender.
export const ambientCgUrl = (id) => `https://ambientcg.com/get?file=${id}_1K-JPG.zip`;

async function fetchTextures(spec, src) {
  const got = [];
  for (const t of spec.source.textures || []) {
    const dest = join(src, t.id);
    if (existsSync(dest) && readdirSync(dest).some((f) => f.endsWith("_Color.jpg"))) { got.push(t.id); continue; }
    mkdirSync(dest, { recursive: true });
    const res = await fetch(ambientCgUrl(t.id), { signal: AbortSignal.timeout(120000) });
    if (!res.ok) throw new Error(`ambientCG ${t.id}: HTTP ${res.status}`);
    const zip = join(src, `${t.id}.zip`);
    writeFileSync(zip, Buffer.from(await res.arrayBuffer()));
    // Windows' own tar (bsdtar) reads zip; Git's GNU tar does not.
    const tar = existsSync("C:/Windows/System32/tar.exe") ? "C:/Windows/System32/tar.exe" : "tar";
    const x = spawnSync(tar, ["-xf", zip, "-C", dest, "*.jpg"], { encoding: "utf8", windowsHide: true, timeout: 60000 });
    if (x.status !== 0) throw new Error(`unpacking ${zip}: ${(x.stderr || x.error?.message || "").trim()}`);
    got.push(t.id);
  }
  return got;
}

// A model or texture set made by our own bpy script (spec.source.blender): no Meshy call, no credits.
async function generateInBlender(ctx) {
  const { root, spec, dir } = ctx;
  const raw = join(dir, "raw");
  const src = join(dir, "src");
  mkdirSync(raw, { recursive: true });
  const textures = await fetchTextures(spec, src);
  const job = join(dir, "model-job.json");
  writeFileSync(job, JSON.stringify({ spec, raw, src, px: spec.budget.texture_px, variants: variantPlans(spec) }, null, 1));
  runBlender(join(root, "tools", "assets", spec.slug, spec.source.blender), [job], { log: join(dir, "model.log") });
  const textureOnly = KINDS[spec.kind].fit === "none";
  if (!textureOnly && !existsSync(join(raw, "model.glb"))) throw new Error(`${spec.source.blender} wrote no ${join(raw, "model.glb")}`);
  for (const v of spec.variants || []) {
    if (!existsSync(join(raw, v.name, "tex_base_color.png"))) throw new Error(`${spec.source.blender} wrote no maps for variant ${v.name}`);
  }
  console.log(`generated in Blender from ${textures.length} CC0 set(s), 0 credits; review ${join(raw, "review_*.png")}`);
  return { blender: spec.source.blender, textures, credits: 0 };
}

async function stageGenerate(ctx) {
  const { spec, dir, state } = ctx;
  if (spec.source?.blender) return generateInBlender(ctx);
  const ledger = join(dir, "credits.jsonl");
  const g = state.generate || (state.generate = {});
  const raw = join(dir, "raw");
  const before = meshy.spent(ledger);
  const model = spec.meshy?.ai_model || "meshy-6-lite";
  const concept = pickedImage(ctx.root, spec.slug, dir); // a picked concept picture means image-to-3D
  const est = concept
    ? (g.imageTaskId ? 0 : meshy.COST.imageTo3d[model] ?? 30)
    : (g.previewId ? 0 : meshy.COST.preview[model] ?? 20) + (g.refineId ? 0 : meshy.COST.refine);
  if (before + est > spec.budget.credits) {
    throw new Error(`this asset has spent ${before} credits and the next attempt needs about ${est}; the cap is ${spec.budget.credits}. Stop and report (docs/asset-pipeline/README.md "Credits").`);
  }
  const bal0 = await meshy.balance();
  console.log(`Meshy balance ${bal0} credits; spent on ${spec.slug} so far ${before}; ${concept ? `image-to-3D from concept ${concept.pick.n} (${concept.pick.label})` : "text-to-3D"}`);
  const save = () => writeState(dir, ctx.fullState);
  const progress = (label) => (t) => console.log(`  ${label} ${t.status} ${t.progress}%`);

  let final;
  const ids = {};
  if (concept) {
    if (!g.imageTaskId) {
      const uri = "data:image/png;base64," + readFileSync(concept.file).toString("base64");
      g.imageTaskId = (await meshy.call("POST", "/openapi/v1/image-to-3d", meshy.imageTo3dBody(spec, uri), { timeoutMs: 120000 })).result;
      save();
    }
    final = await meshy.waitTask(g.imageTaskId, { path: "/openapi/v1/image-to-3d", onProgress: progress("image-to-3d") });
    if (!g.imageLogged) {
      meshy.logCredits(ledger, { call: "image-to-3d", model, task: g.imageTaskId, credits: final.consumed_credits ?? meshy.COST.imageTo3d[model] ?? 30 });
      g.imageLogged = true;
      save();
    }
    Object.assign(ids, { imageTaskId: g.imageTaskId, concept: concept.pick.n, conceptTaskId: concept.variant.taskId, conceptPrompt: concept.variant.prompt });
    if (final.thumbnail_url) await meshy.download(final.thumbnail_url, join(raw, "refined.png"));
  } else {
    if (!g.previewId) {
      g.previewId = (await meshy.call("POST", "/openapi/v2/text-to-3d", meshy.previewBody(spec))).result;
      save();
    }
    const preview = await meshy.waitTask(g.previewId, { onProgress: progress("preview") });
    if (!g.previewLogged) {
      meshy.logCredits(ledger, { call: "text-to-3d preview", model, task: g.previewId, credits: preview.consumed_credits ?? meshy.COST.preview[model] });
      g.previewLogged = true;
      save();
    }
    if (preview.thumbnail_url) await meshy.download(preview.thumbnail_url, join(raw, "preview.png"));

    if (!g.refineId) {
      g.refineId = (await meshy.call("POST", "/openapi/v2/text-to-3d", meshy.refineBody(spec, g.previewId))).result;
      save();
    }
    final = await meshy.waitTask(g.refineId, { onProgress: progress("refine") });
    if (!g.refineLogged) {
      meshy.logCredits(ledger, { call: "text-to-3d refine", task: g.refineId, credits: final.consumed_credits ?? meshy.COST.refine });
      g.refineLogged = true;
      save();
    }
    Object.assign(ids, { previewId: g.previewId, refineId: g.refineId });
    if (final.thumbnail_url) await meshy.download(final.thumbnail_url, join(raw, "refined.png"));
  }
  if (!final.model_urls?.glb) throw new Error("Meshy returned no GLB");
  await meshy.download(final.model_urls.glb, join(raw, "model.glb"));
  const tex = final.texture_urls?.[0] || {};
  for (const [k, url] of Object.entries(tex)) await meshy.download(url, join(raw, `tex_${k}.png`));
  const bal1 = await meshy.balance();
  const total = meshy.spent(ledger);
  recordProvenance(ctx.root, spec, ids);
  console.log(`generated ${join(raw, "model.glb")}; balance ${bal0} -> ${bal1}; ${spec.slug} total ${total} credits`);
  console.log(`review ${join(raw, "refined.png")} against docs/asset-pipeline/review-generation.md before the clean stage`);
  return { ...ids, credits: total, balance: bal1, maps: Object.keys(tex) };
}

export function blenderExe() {
  return process.env.LF_BLENDER || "C:/Users/jaspe/Tools/blender-4.5.9-windows-x64/blender.exe";
}

// Run one of our bpy scripts headless; the script writes a JSON report we read back.
export function runBlender(script, args, { timeoutMs = 600000, log } = {}) {
  const exe = blenderExe();
  if (!existsSync(exe)) throw new Error(`Blender not found at ${exe}; set LF_BLENDER (docs/asset-pipeline/README.md "Setup")`);
  const res = spawnSync(exe, ["--background", "--factory-startup", "--python-exit-code", "3", "--python", script, "--", ...args],
    { encoding: "utf8", timeout: timeoutMs, windowsHide: true, maxBuffer: 64 * 1024 * 1024 });
  if (log) writeFileSync(log, (res.stdout || "") + "\n" + (res.stderr || ""));
  if (res.error) throw new Error(`Blender ${script}: ${res.error.message}`);
  if (res.status !== 0) {
    const tail = ((res.stdout || "") + (res.stderr || "")).trim().split("\n").slice(-12).join("\n");
    throw new Error(`Blender ${script} exited ${res.status}${log ? ` (log: ${log})` : ""}\n${tail}`);
  }
}

// The Meshy task the remesh starts from: the accepted generation recorded in tools/assets/<slug>/meshy.json.
export function acceptedTaskId(root, slug) {
  const file = join(root, "tools", "assets", slug, "meshy.json");
  if (!existsSync(file)) return null;
  const gens = JSON.parse(readFileSync(file, "utf8")).generations.filter((g) => g.accepted !== false);
  const g = gens[gens.length - 1];
  return g ? g.imageTaskId || g.refineId || null : null;
}

// LOD0 straight from Meshy: one remesh call to the budget's triangle count (per link for a repeat),
// so the clean stage never collapses a big raw mesh. spec.meshy.remesh: false skips it.
async function stageRemesh(ctx) {
  const { spec, dir, state } = ctx;
  if (spec.meshy?.remesh === false) return { skipped: "spec.meshy.remesh is false" };
  if (spec.source?.blender) return { skipped: "modelled in Blender, not by Meshy" };
  if (KINDS[spec.kind]?.fit === "none") return { skipped: "a material has no mesh" };
  const ledger = join(dir, "credits.jsonl");
  const out = join(dir, "raw-remesh");
  const g = state.remesh || (state.remesh = {});
  const target = Math.round(spec.budget.lod0_tris / Math.max(spec.fit?.repeat || 1, 1));
  if (!g.taskId) {
    const input = acceptedTaskId(ctx.root, spec.slug);
    if (!input) throw new Error(`no accepted Meshy task for ${spec.slug} in tools/assets/${spec.slug}/meshy.json`);
    const spentSoFar = meshy.spent(ledger);
    if (spentSoFar + meshy.COST.remesh > spec.budget.credits) throw new Error(`remesh costs ${meshy.COST.remesh} credits over the cap of ${spec.budget.credits}`);
    g.taskId = (await meshy.call("POST", "/openapi/v1/remesh", meshy.remeshBody(spec, input, target))).result;
    g.input = input;
    g.target = target;
    writeState(dir, ctx.fullState);
  }
  const final = await meshy.waitTask(g.taskId, { path: "/openapi/v1/remesh", onProgress: (t) => console.log(`  remesh ${t.status} ${t.progress}%`) });
  if (!g.logged) {
    meshy.logCredits(ledger, { call: "remesh", task: g.taskId, credits: final.consumed_credits ?? meshy.COST.remesh });
    g.logged = true;
    writeState(dir, ctx.fullState);
  }
  if (!final.model_urls?.glb) throw new Error("Meshy remesh returned no GLB");
  await meshy.download(final.model_urls.glb, join(out, "model.glb"));
  const tex = final.texture_urls?.[0] || {};
  for (const [k, url] of Object.entries(tex)) if (url) await meshy.download(url, join(out, `tex_${k}.png`));
  console.log(`remesh: ${join(out, "model.glb")} at ${target} triangles from ${g.input}; maps ${Object.keys(tex).join(",") || "embedded in the GLB"}`);
  return { task: g.taskId, input: g.input, target, maps: Object.keys(tex) };
}

async function stageClean(ctx) {
  const { spec, dir } = ctx;
  const out = join(dir, "clean");
  mkdirSync(out, { recursive: true });
  const variants = variantPlans(spec);
  const blenderDir = join(dirname(fileURLToPath(import.meta.url)), "blender");
  const packVariants = () => {
    if (!variants.length) return {};
    const vjob = join(out, "variants-job.json");
    writeFileSync(vjob, JSON.stringify({ name: spec.name, raw: join(dir, "raw"), out, px: spec.budget.texture_px, variants }, null, 1));
    runBlender(join(blenderDir, "variants.py"), [vjob], { log: join(out, "variants.log") });
    const v = JSON.parse(readFileSync(join(out, "variants.json"), "utf8"));
    for (const [n, st] of Object.entries(v)) console.log(`  variant ${n}: rough ${st.mean_roughness.toFixed(2)}, metal ${st.mean_metallic.toFixed(2)}, base ${st.mean_base.map((x) => x.toFixed(2)).join(" ")}`);
    return v;
  };
  if (KINDS[spec.kind].fit === "none") return { variants: packVariants() };
  // A remeshed model (raw-remesh/) is LOD0 already; otherwise the generator's raw output. A spec
  // with meshy.remesh false uses the raw output even when an earlier run left a remesh behind.
  const remeshed = spec.meshy?.remesh !== false && existsSync(join(dir, "raw-remesh", "model.glb"));
  const rawDir = remeshed ? join(dir, "raw-remesh") : join(dir, "raw");
  const glb = join(rawDir, "model.glb");
  if (!existsSync(glb)) throw new Error(`no ${glb}; run the generate stage first (or drop a GLB there)`);
  const job = join(out, "job.json");
  // The anim block adds a skeleton (a bone per moving part, the sockets) and the parts to split off.
  const { kind, parts } = withAnim(KINDS[spec.kind], spec);
  writeFileSync(job, JSON.stringify({ spec: { ...spec, parts }, kind, materials: resolveMaterials(spec), glb, raw: rawDir, out }, null, 1));
  runBlender(join(blenderDir, "clean.py"), [job], { log: join(out, "blender.log") });
  const report = JSON.parse(readFileSync(join(out, "report.json"), "utf8"));
  const { checkReport } = await import("./checks.mjs");
  const problems = checkReport(spec, kind, report);
  for (const p of problems) console.log(`FAIL  ${p}`);
  console.log(`clean: LODs ${report.lods.map((l) => l.tris).join("/")} tris, size ${report.size.map((v) => v.toFixed(3)).join(" x ")} m, ${report.sockets.length} sockets, review ${join(out, "review")}`);
  if (problems.length) throw new Error(`${problems.length} check(s) failed; fix the spec (fit, parts, budget) and rerun --only clean`);
  return { lods: report.lods, size: report.size, variants: packVariants() };
}

async function stageImport(ctx) {
  const { importAsset } = await import("./import.mjs");
  return importAsset(ctx);
}

async function stagePrefab(ctx) {
  // An asset that replaces the mesh or material of prefabs that already exist (the mining ingots).
  if (ctx.spec.prefab === false || KINDS[ctx.spec.kind].fit === "none") {
    console.log("prefab: skipped; the spec says prefab false (existing prefabs reference the import)");
    return { skipped: true };
  }
  const { writePrefabs } = await import("./prefab.mjs");
  return writePrefabs(ctx);
}

// Stage "anim": moving parts, the character's animation, effects and sounds (anim.mjs). An asset
// with no anim block has nothing to do here.
async function stageAnim(ctx) {
  if (!hasAnim(ctx.spec)) { console.log("anim: the spec has no anim block; nothing moves"); return { skipped: true }; }
  const { stageAnim: runAnim } = await import("./anim.mjs");
  return runAnim({ ...ctx, blender: runBlender }, { fix: ctx.fix });
}

async function stageConcepts(ctx) {
  return concepts.stageConcept(ctx, { writeState, resolveFfmpeg });
}

const RUN = { spec: stageSpec, concept: stageConcepts, generate: stageGenerate, remesh: stageRemesh, clean: stageClean, import: stageImport, prefab: stagePrefab, anim: stageAnim };

export async function run(root, slug, { from, only, force, workbench, pick, fix } = {}) {
  const spec = loadSpec(root, slug);
  const dir = cacheDir(root, slug);
  const fullState = readState(dir);
  let rerun = !!force;
  const list = only ? [only] : STAGES.slice(from ? STAGES.indexOf(from) : 0);
  if (list.includes(undefined) || list.some((s) => !RUN[s])) throw new Error(`stages are: ${STAGES.join(", ")}`);
  for (const stage of list) {
    const done = fullState.stages[stage];
    if (done?.ok && !rerun && !only && stage !== from) {
      console.log(`${stage}: cached (${done.secs}s on ${done.at.slice(0, 16)}); --force reruns it`);
      continue;
    }
    rerun = true; // a rerun stage invalidates every later one
    const started = Date.now();
    const ctx = { root, slug, spec, dir, fullState, workbench, pick, fix, state: (fullState.work ||= {}) };
    console.log(`--- ${stage}`);
    if (stage === "generate" && force && ctx.state.generate?.refineLogged) {
      // A forced generate is a new attempt: keep the old download beside it and start new tasks.
      let n = 1;
      while (existsSync(join(dir, `raw-attempt-${n}`))) n++;
      if (existsSync(join(dir, "raw"))) renameSync(join(dir, "raw"), join(dir, `raw-attempt-${n}`));
      (fullState.attempts ||= []).push({ n, ...ctx.state.generate, prompt: fullState.lastPrompt });
      ctx.state.generate = {};
    }
    if (stage === "generate") fullState.lastPrompt = spec.prompt;
    try {
      const result = await RUN[stage](ctx);
      fullState.stages[stage] = { ok: true, at: new Date().toISOString(), secs: Math.round((Date.now() - started) / 1000), result };
      writeState(dir, fullState);
    } catch (e) {
      fullState.stages[stage] = { ok: false, at: new Date().toISOString(), secs: Math.round((Date.now() - started) / 1000), error: e.message.split("\n")[0] };
      writeState(dir, fullState);
      throw e;
    }
  }
  return 0;
}

function flag(flags, name) {
  const i = flags.indexOf(name);
  return i >= 0 ? flags[i + 1] : undefined;
}

function cmdNew(root, flags, workbench) {
  const slug = flags.find((f) => !f.startsWith("--"));
  const kindName = flag(flags, "--kind");
  const prompt = flag(flags, "--prompt");
  const kind = KINDS[kindName];
  if (!slug || !kind || !prompt) {
    console.log('usage: tools\\cli asset new <slug> --kind weapon|gadget|deployable --prompt "..." [--name PascalName] [--no-run]');
    return 2;
  }
  const file = specFile(root, slug);
  if (existsSync(file)) throw new Error(`${file} exists; edit it and use: tools\\cli asset run ${slug}`);
  const name = flag(flags, "--name") || slug.split("-").map((p) => p[0].toUpperCase() + p.slice(1)).join("");
  const spec = {
    slug, name, kind: kindName, roadmap: flag(flags, "--roadmap") || "",
    concept: "", prompt, texture_prompt: "",
    meshy: { ai_model: "meshy-6-lite", target_polycount: BUDGETS[kindName] ? BUDGETS[kindName].source : 20000 },
    size_m: kind.defaultSize,
    budget: BUDGETS[kindName] ? defaultBudget(kindName) : { lod0_tris: kind.defaultTris, lod_ratios: [0.5, 0.25], texture_px: 1024, credits: 300 },
    fit: { offset: [0, 0, 0] }, parts: [],
    asset_dir: `${kind.assetRoot}/${name}`, prefab_dir: `${kind.prefabRoot}/${name}`,
  };
  mkdirSync(dirname(file), { recursive: true });
  writeFileSync(file, JSON.stringify(spec, null, 2) + "\n");
  console.log(`wrote ${file}; set size_m and the budget there (docs/asset-pipeline/prompt-writing.md)`);
  return flags.includes("--no-run") ? 0 : run(root, slug, { workbench });
}

function cmdStatus(root, slug) {
  const base = join(root, "tools", "assets");
  const slugs = slug ? [slug] : readdirSync(base).filter((d) => existsSync(join(base, d, "spec.json")));
  for (const s of slugs) {
    const dir = cacheDir(root, s);
    const st = readState(dir);
    console.log(`${s}: ${meshy.spent(join(dir, "credits.jsonl"))} credits`);
    for (const stage of STAGES) {
      const d = st.stages[stage];
      console.log(`  ${stage.padEnd(9)} ${d ? (d.ok ? "ok  " : "FAIL") + ` ${String(d.secs).padStart(4)}s  ${d.ok ? "" : d.error}` : "-"}`);
    }
  }
  return 0;
}

async function cmdDoctor() {
  let bad = 0;
  const say = (ok, what) => { if (!ok) bad++; console.log(`${ok ? "ok  " : "FAIL"}  ${what}`); };
  say(existsSync(blenderExe()), `Blender at ${blenderExe()} (LF_BLENDER overrides)`);
  say(existsSync(meshy.KEY_FILE()), `Meshy key file present (${meshy.KEY_FILE()})`);
  try { console.log(`ok    Meshy balance ${await meshy.balance()} credits`); } catch (e) { say(false, e.message); }
  return bad ? 1 : 0;
}

export async function main(flags, { workbench } = {}) {
  const [sub, ...rest] = flags;
  const root = ROOT;
  if (sub === "ambience") return (await import("./ambience.mjs")).main(root, rest); // docs/architecture/atmosphere.md
  if (sub === "selftest") return (await import("../../validation/asset-pipeline-selftest.mjs")).main(rest);
  if (sub === "dialogue") return (await import("./dialogue.mjs")).main(rest);
  if (sub === "pick") return concepts.cmdPick(root, rest);
  if (sub === "score") return concepts.cmdScore(root, rest);
  if (sub === "doctor") return cmdDoctor();
  if (sub === "status") return cmdStatus(root, rest[0]);
  if (sub === "sound") return (await import("./sound.mjs")).main(root, rest); // docs/asset-pipeline/sounds.md
  if (sub === "anim") { // the anim stage alone: tools\cli asset anim <slug> [--fix]
    const wt = await import("../../wt.mjs");
    if (wt.isMainCheckout(root)) { console.log("FAIL  asset runs only in a worktree"); return 1; }
    const slug = rest.find((f) => !f.startsWith("--"));
    if (!slug) { console.log("usage: tools\\cli asset anim <slug> [--fix]"); return 2; }
    return run(root, slug, { only: "anim", fix: rest.includes("--fix"), workbench });
  }
  if (sub === "kit") {
    const wt = await import("../../wt.mjs");
    if (wt.isMainCheckout(root)) { console.log("FAIL  asset runs only in a worktree"); return 1; }
    return (await import("./kit.mjs")).main(root, rest, { workbench, runBlender }); // docs/asset-pipeline/README.md "Kits"
  }
  if (sub === "new" || sub === "run") {
    const wt = await import("../../wt.mjs");
    if (wt.isMainCheckout(root)) { console.log("FAIL  asset runs only in a worktree"); return 1; }
    if (sub === "new") return cmdNew(root, rest, workbench);
    const slug = rest.find((f) => !f.startsWith("--") && f !== flag(rest, "--from") && f !== flag(rest, "--only"));
    if (!slug) { console.log("usage: tools\\cli asset run <slug> [--from <stage>] [--only <stage>] [--force] [--pick]"); return 2; }
    return run(root, slug, { from: flag(rest, "--from"), only: flag(rest, "--only"), force: rest.includes("--force"), pick: rest.includes("--pick"), workbench });
  }
  console.log(`usage: tools\\cli asset new <slug> --kind ${Object.keys(KINDS).join("|")} --prompt "..."
       tools\\cli asset run <slug> [--from <stage>] [--only <stage>] [--force] [--pick]     stages: ${STAGES.join(", ")}
       tools\\cli asset status [<slug>] | doctor | selftest
       tools\\cli asset anim <slug> [--fix]     moving parts, character clips, effects (docs/asset-pipeline/animation.md)
       tools\\cli asset kit <slug> [--only build|import]     several models from one Blender script (a room, a vault)
       tools\\cli asset sound <slug> [--init | --status | --only generate|process|wire]
       tools\\cli asset dialogue <npc> [--dry-run] [--force <line>|all] | dialogue status     voiced NPC lines (docs/asset-pipeline/dialogue.md)
playbook: docs/asset-pipeline/README.md`);
  return sub ? 2 : 0;
}
