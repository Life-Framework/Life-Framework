// tools\cli asset <command>: roadmap item -> Meshy -> headless Blender -> Workbench import -> prefab.
// Playbook for the judgement steps: docs/asset-pipeline/README.md.
//
//   asset new <slug> --kind weapon|gadget|deployable --prompt "..." [--name N] [--no-run]
//   asset run <slug> [--from <stage>] [--only <stage>] [--force]
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

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..", "..");
export const STAGES = ["concept", "generate", "clean", "import", "prefab"];

export const specFile = (root, slug) => join(root, "tools", "assets", slug, "spec.json");
export const cacheDir = (root, slug) => join(root, "tmp", "assets", slug);

export function loadSpec(root, slug) {
  const file = specFile(root, slug);
  if (!existsSync(file)) throw new Error(`no spec at ${file}; start with: tools\\cli asset new ${slug} --kind ... --prompt "..."`);
  return JSON.parse(readFileSync(file, "utf8"));
}

// Everything wrong with a spec, as a list. Empty means the concept stage passes.
export function specProblems(spec) {
  const out = [];
  if (!/^[a-z][a-z0-9-]*$/.test(spec.slug || "")) out.push("slug must be lowercase letters, digits and dashes");
  if (!/^[A-Z][A-Za-z0-9]*$/.test(spec.name || "")) out.push("name must be PascalCase letters and digits (it becomes file and material names)");
  const kind = KINDS[spec.kind];
  if (!kind) out.push(`kind must be one of ${Object.keys(KINDS).join(", ")}`);
  if (!spec.prompt) out.push("prompt is empty");
  else if (spec.prompt.length > 800) out.push(`prompt is ${spec.prompt.length} characters; Meshy allows 800`);
  if ((spec.texture_prompt || "").length > 800) out.push("texture_prompt is over 800 characters");
  const s = spec.size_m || {};
  for (const k of ["length", "height", "width"]) {
    if (!(s[k] > 0.005 && s[k] < 20)) out.push(`size_m.${k} must be a size in metres`);
  }
  const b = spec.budget || {};
  if (!(b.lod0_tris >= 100 && b.lod0_tris <= 60000)) out.push("budget.lod0_tris must be 100 to 60000");
  if (kind && b.lod0_tris > kind.maxTris) out.push(`budget.lod0_tris ${b.lod0_tris} is over the ${spec.kind} cap of ${kind.maxTris}`);
  if (![256, 512, 1024, 2048].includes(b.texture_px)) out.push("budget.texture_px must be 256, 512, 1024 or 2048");
  if (!(b.credits > 0 && b.credits <= 300)) out.push("budget.credits must be 1 to 300 (the per-asset cap)");
  if (spec.prompt && !/no text/i.test(spec.prompt)) out.push('prompt should say "no text, no logos" (generated lettering is garbled)');
  for (const p of spec.parts || []) {
    if (!p.name || !p.bone || !Array.isArray(p.box) || p.box.length !== 6) out.push(`part ${p.name || "?"} needs name, bone and box [x0,y0,z0,x1,y1,z1]`);
    else if (kind && !kind.skeleton?.some((n) => n.name === p.bone)) out.push(`part ${p.name}: bone ${p.bone} is not in the ${spec.kind} skeleton`);
  }
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
async function stageConcept(ctx) {
  const problems = specProblems(ctx.spec);
  if (problems.length) throw new Error("spec problems:\n  " + problems.join("\n  "));
  const est = meshy.estimate(ctx.spec);
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
    date: now.toISOString().slice(0, 10), tool: "Meshy text-to-3D", model: spec.meshy?.ai_model || "meshy-6-lite",
    previewId: ids.previewId, refineId: ids.refineId, prompt: spec.prompt, texturePrompt: spec.texture_prompt,
  };
  const at = rec.generations.findIndex((x) => x.refineId === row.refineId);
  if (at >= 0) rec.generations[at] = { ...rec.generations[at], ...row };
  else rec.generations.push(row);
  writeFileSync(file, JSON.stringify(rec, null, 2) + "\n");
}

async function stageGenerate(ctx) {
  const { spec, dir, state } = ctx;
  const ledger = join(dir, "credits.jsonl");
  const g = state.generate || (state.generate = {});
  const raw = join(dir, "raw");
  const before = meshy.spent(ledger);
  const est = (g.previewId ? 0 : meshy.COST.preview[spec.meshy?.ai_model || "meshy-6-lite"] ?? 20) + (g.refineId ? 0 : meshy.COST.refine);
  if (before + est > spec.budget.credits) {
    throw new Error(`this asset has spent ${before} credits and the next attempt needs about ${est}; the cap is ${spec.budget.credits}. Stop and report (docs/asset-pipeline/README.md "Credits").`);
  }
  const bal0 = await meshy.balance();
  console.log(`Meshy balance ${bal0} credits; spent on ${spec.slug} so far ${before}`);
  const save = () => writeState(dir, ctx.fullState);
  const progress = (label) => (t) => console.log(`  ${label} ${t.status} ${t.progress}%`);

  if (!g.previewId) {
    g.previewId = (await meshy.call("POST", "/openapi/v2/text-to-3d", meshy.previewBody(spec))).result;
    save();
  }
  const preview = await meshy.waitTask(g.previewId, { onProgress: progress("preview") });
  if (!g.previewLogged) {
    meshy.logCredits(ledger, { call: "text-to-3d preview", model: spec.meshy?.ai_model || "meshy-6-lite", task: g.previewId, credits: preview.consumed_credits ?? meshy.COST.preview[spec.meshy?.ai_model || "meshy-6-lite"] });
    g.previewLogged = true;
    save();
  }
  if (preview.thumbnail_url) await meshy.download(preview.thumbnail_url, join(raw, "preview.png"));

  if (!g.refineId) {
    g.refineId = (await meshy.call("POST", "/openapi/v2/text-to-3d", meshy.refineBody(spec, g.previewId))).result;
    save();
  }
  const refine = await meshy.waitTask(g.refineId, { onProgress: progress("refine") });
  if (!g.refineLogged) {
    meshy.logCredits(ledger, { call: "text-to-3d refine", task: g.refineId, credits: refine.consumed_credits ?? meshy.COST.refine });
    g.refineLogged = true;
    save();
  }
  if (!refine.model_urls?.glb) throw new Error("Meshy refine returned no GLB");
  await meshy.download(refine.model_urls.glb, join(raw, "model.glb"));
  if (refine.thumbnail_url) await meshy.download(refine.thumbnail_url, join(raw, "refined.png"));
  const tex = refine.texture_urls?.[0] || {};
  for (const [k, url] of Object.entries(tex)) await meshy.download(url, join(raw, `tex_${k}.png`));
  const bal1 = await meshy.balance();
  const total = meshy.spent(ledger);
  recordProvenance(ctx.root, spec, { previewId: g.previewId, refineId: g.refineId });
  console.log(`generated ${join(raw, "model.glb")}; balance ${bal0} -> ${bal1}; ${spec.slug} total ${total} credits`);
  console.log(`review ${join(raw, "refined.png")} against docs/asset-pipeline/review-generation.md before the clean stage`);
  return { previewId: g.previewId, refineId: g.refineId, credits: total, balance: bal1, maps: Object.keys(tex) };
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

async function stageClean(ctx) {
  const { spec, dir } = ctx;
  const glb = join(dir, "raw", "model.glb");
  if (!existsSync(glb)) throw new Error(`no ${glb}; run the generate stage first (or drop a GLB there)`);
  const out = join(dir, "clean");
  mkdirSync(out, { recursive: true });
  const job = join(out, "job.json");
  writeFileSync(job, JSON.stringify({ spec, kind: KINDS[spec.kind], glb, raw: join(dir, "raw"), out }, null, 1));
  runBlender(join(dirname(fileURLToPath(import.meta.url)), "blender", "clean.py"), [job], { log: join(out, "blender.log") });
  const report = JSON.parse(readFileSync(join(out, "report.json"), "utf8"));
  const { checkReport } = await import("./checks.mjs");
  const problems = checkReport(spec, KINDS[spec.kind], report);
  for (const p of problems) console.log(`FAIL  ${p}`);
  console.log(`clean: LODs ${report.lods.map((l) => l.tris).join("/")} tris, size ${report.size.map((v) => v.toFixed(3)).join(" x ")} m, ${report.sockets.length} sockets, review ${join(out, "review")}`);
  if (problems.length) throw new Error(`${problems.length} check(s) failed; fix the spec (fit, parts, budget) and rerun --only clean`);
  return { lods: report.lods, size: report.size };
}

async function stageImport(ctx) {
  const { importAsset } = await import("./import.mjs");
  return importAsset(ctx);
}

async function stagePrefab(ctx) {
  const { writePrefabs } = await import("./prefab.mjs");
  return writePrefabs(ctx);
}

const RUN = { concept: stageConcept, generate: stageGenerate, clean: stageClean, import: stageImport, prefab: stagePrefab };

export async function run(root, slug, { from, only, force, workbench } = {}) {
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
    const ctx = { root, slug, spec, dir, fullState, workbench, state: (fullState.work ||= {}) };
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
    meshy: { ai_model: "meshy-6-lite", target_polycount: 20000 },
    size_m: kind.defaultSize,
    budget: { lod0_tris: kind.defaultTris, lod_ratios: [0.5, 0.2], texture_px: 1024, credits: 300 },
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
  if (sub === "doctor") return cmdDoctor();
  if (sub === "status") return cmdStatus(root, rest[0]);
  if (sub === "new" || sub === "run") {
    const wt = await import("../../wt.mjs");
    if (wt.isMainCheckout(root)) { console.log("FAIL  asset runs only in a worktree"); return 1; }
    if (sub === "new") return cmdNew(root, rest, workbench);
    const slug = rest.find((f) => !f.startsWith("--") && f !== flag(rest, "--from") && f !== flag(rest, "--only"));
    if (!slug) { console.log("usage: tools\\cli asset run <slug> [--from <stage>] [--only <stage>] [--force]"); return 2; }
    return run(root, slug, { from: flag(rest, "--from"), only: flag(rest, "--only"), force: rest.includes("--force"), workbench });
  }
  console.log(`usage: tools\\cli asset new <slug> --kind ${Object.keys(KINDS).join("|")} --prompt "..."
       tools\\cli asset run <slug> [--from <stage>] [--only <stage>] [--force]     stages: ${STAGES.join(", ")}
       tools\\cli asset status [<slug>] | doctor | selftest
playbook: docs/asset-pipeline/README.md`);
  return sub ? 2 : 0;
}
