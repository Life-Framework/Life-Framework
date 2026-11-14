// Concept stage: 10 prompts -> 10 Meshy text-to-image pictures -> a numbered contact sheet -> a rubric
// score and one pick, before any 3D credit is spent (docs/asset-pipeline/concepts.md). The picked
// image is what the generate stage feeds to Meshy image-to-3D. With a `reference` picture in
// concepts.json the ten go through image-to-image instead, so a kit matches a piece already chosen.
// Endpoints and costs were read from docs.meshy.ai on 2026-11-13; recheck there before changing them.
import { existsSync, mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { extname, isAbsolute, join } from "node:path";
import { spawnSync } from "node:child_process";
import * as meshy from "./meshy.mjs";
import { conceptMatches, pickCandidate } from "./source.mjs";

export const COUNT = 10;
export const COLS = 5;
export const IMAGE_MODEL = "nano-banana-2-1"; // 4 credits; nano-banana is 3, nano-banana-2 is 6
// Every picture is the same kind of shot, because image-to-3D reads a plain product photo best.
export const FRAME = "Single object, orthographic-style three-quarter product shot from slightly above, the whole object in frame, plain flat light grey background, soft even studio light, no hands, no stand, no text, no logos, no watermark.";
export const RUBRIC = [
  { key: "silhouette", what: "a clear, readable silhouette (the object reads at a glance)" },
  { key: "era", what: "fits the period (late 1980s, no modern industrial design)" },
  { key: "era_current", what: "era-current: what a 1989 local would use or buy new, not antique or rustic (a 0 is never picked)" },
  { key: "scale_grip", what: "plausible scale and grip (a hand could hold it where the grip is)" },
  { key: "background", what: "a clean plain background, one object, nothing cropped" },
  { key: "no_text", what: "no text, lettering, logo or watermark" },
  { key: "image_to_3d", what: "friendly to image-to-3D (solid form, no thin or hidden parts, even light)" },
];
export const MAX_SCORE = 2; // each criterion 0 (fails), 1 (so-so), 2 (good)

export const conceptsFile = (root, slug) => join(root, "tools", "assets", slug, "concepts.json");
export const conceptsDir = (dir) => join(dir, "concepts");
export const pad = (n) => String(n).padStart(2, "0");

// The 10 variants hold only what differs; the subject and the shared frame are added here.
export function buildPrompts(subject, variants) {
  return variants.map((v, i) => ({ n: i + 1, label: v.label, prompt: `${subject} ${v.detail.replace(/[\s.]+$/, "")}. ${FRAME}` }));
}

export function conceptProblems(rec) {
  const out = [];
  if (!rec.subject) out.push("subject is empty");
  const v = rec.variants || [];
  if (v.length !== COUNT) out.push(`need exactly ${COUNT} variants, found ${v.length}`);
  const dup = (f) => new Set(v.map(f)).size !== v.length;
  if (dup((x) => (x.label || "").toLowerCase())) out.push("variant labels must be distinct");
  if (dup((x) => (x.detail || "").toLowerCase())) out.push("variant details must be distinct");
  for (const [i, x] of v.entries()) {
    if (!x.label || x.label.length > 22) out.push(`variant ${i + 1}: label needs 1 to 22 characters`);
    if (!x.detail) out.push(`variant ${i + 1}: detail is empty`);
  }
  if (rec.reference !== undefined) {
    const f = rec.reference?.file;
    if (typeof f !== "string" || !REF_TYPES[extname(f).toLowerCase()]) out.push("reference.file must be a .png, .jpg, .jpeg or .webp path");
    else if (isAbsolute(f) || f.includes("..")) out.push("reference.file must be a path inside the repo, relative to its root");
  }
  if (!out.length) for (const p of buildPrompts(rec.subject, v)) if (p.prompt.length > 800) out.push(`variant ${p.n}: prompt is ${p.prompt.length} characters (limit 800)`);
  return out;
}

// ---- the Meshy request -------------------------------------------------------------------------
// image-to-image takes 1 to 5 references as URLs or data URIs and costs what text-to-image does on
// the nano-banana models (docs.meshy.ai/en/api/image-to-image and /pricing, read 2026-11-13).
export const REF_TYPES = { ".png": "image/png", ".jpg": "image/jpeg", ".jpeg": "image/jpeg", ".webp": "image/webp" };
export const referenceUri = (root, rec) =>
  `data:${REF_TYPES[extname(rec.reference.file).toLowerCase()]};base64,` + readFileSync(join(root, rec.reference.file)).toString("base64");

export function conceptRequest(model, prompt, refUri) {
  if (!refUri) return { call: "text-to-image", path: "/openapi/v1/text-to-image", body: { ai_model: model, prompt, aspect_ratio: "1:1" } };
  return { call: "image-to-image", path: "/openapi/v1/image-to-image", body: { ai_model: model, prompt, reference_image_urls: [refUri], aspect_ratio: "1:1" } };
}

// The realism gate (docs/asset-pipeline/concepts.md): before any picture is paid for, concepts.json
// answers how the thing was actually done or bought in a 1989 Eastern-bloc island town, and why a
// local would see it as current rather than antique. The 2026-11-13 juicer sheet drew ten hand-cranked
// wooden presses because nobody asked.
export function realismProblems(rec) {
  const r = rec.realism || {};
  const out = [];
  if (!r.how || r.how.trim().length < 20) out.push('realism.how is missing: how would this actually be done or bought in a 1989 Eastern-bloc island town?');
  if (!r.current || r.current.trim().length < 20) out.push('realism.current is missing: why would a 1989 local recognise it as current, not antique?');
  return out;
}

// ---- contact sheet ----------------------------------------------------------------------------
export function sheetLayout(count = COUNT, cols = COLS, cell = 400, banner = 44) {
  const rows = Math.ceil(count / cols);
  const cells = [];
  for (let i = 0; i < count; i++) cells.push({ n: i + 1, x: (i % cols) * cell, y: Math.floor(i / cols) * (cell + banner) });
  return { cols, rows, cell, banner, width: cols * cell, height: rows * (cell + banner), cells };
}

// ffmpeg arguments for the sheet. Labels go in textfile= inputs (no quoting trouble); run it with cwd
// set to the folder holding lab01.txt ... and the images, so no drive letter reaches the filter.
export function sheetArgs({ images, out, fontfile, cols = COLS, cell = 400, banner = 44 }) {
  const L = sheetLayout(images.length, cols, cell, banner);
  const args = ["-y", "-hide_banner", "-loglevel", "error"];
  for (const f of images) args.push("-i", f);
  const font = fontfile ? `fontfile='${fontfile}':` : "";
  const parts = images.map((_, i) =>
    `[${i}:v]scale=${cell}:${cell}:force_original_aspect_ratio=decrease,pad=${cell}:${cell + banner}:(ow-iw)/2:${banner}+(${cell}-ih)/2:color=0xdddddd,` +
    `drawbox=x=0:y=0:w=${cell}:h=${banner}:color=0x202020:t=fill,` +
    `drawtext=${font}textfile=lab${pad(i + 1)}.txt:fontcolor=white:fontsize=24:x=10:y=(${banner}-text_h)/2,drawbox=x=0:y=0:w=${cell}:h=${cell + banner}:color=0x202020:t=3[v${i}]`);
  const layout = L.cells.map((c) => `${c.x}_${c.y}`).join("|");
  parts.push(`${images.map((_, i) => `[v${i}]`).join("")}xstack=inputs=${images.length}:layout=${layout}:fill=0x202020[sheet]`);
  args.push("-filter_complex", parts.join(";"), "-map", "[sheet]", "-frames:v", "1", out);
  return args;
}

export function buildSheet(ffmpeg, folder, rec, out) {
  const labels = rec.variants.map((v, i) => `${i + 1}  ${v.label}`);
  labels.forEach((t, i) => writeFileSync(join(folder, `lab${pad(i + 1)}.txt`), t));
  const font = process.platform === "win32" ? "C\\:/Windows/Fonts/arial.ttf" : "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf";
  const images = labels.map((_, i) => `${pad(i + 1)}.png`);
  const res = spawnSync(ffmpeg, sheetArgs({ images, out, fontfile: font }), { cwd: folder, encoding: "utf8", windowsHide: true, timeout: 120000 });
  if (res.status !== 0 || !existsSync(join(folder, out))) throw new Error(`ffmpeg contact sheet failed: ${(res.stderr || res.error?.message || "").trim().slice(0, 400)}`);
  return join(folder, out);
}

// ---- scores and the pick ----------------------------------------------------------------------
export const total = (v) => (v.scores ? RUBRIC.reduce((n, r) => n + (v.scores[r.key] ?? 0), 0) : null);
export const maxTotal = () => RUBRIC.length * MAX_SCORE;
export const scored = (rec) => rec.variants.every((v) => v.scores);

export function applyScore(rec, n, list, note) {
  const v = rec.variants[n - 1];
  if (!v) throw new Error(`no concept ${n}; the range is 1 to ${rec.variants.length}`);
  if (list.length !== RUBRIC.length || list.some((x) => !Number.isInteger(x) || x < 0 || x > MAX_SCORE)) {
    throw new Error(`score needs ${RUBRIC.length} integers 0 to ${MAX_SCORE}, in this order: ${RUBRIC.map((r) => r.key).join(", ")}`);
  }
  v.scores = Object.fromEntries(RUBRIC.map((r, i) => [r.key, list[i]]));
  if (note) v.note = note;
  return rec;
}

// Highest total wins; a tie goes to the lower number so the choice is reproducible.
export function ranking(rec) {
  return rec.variants.map((v, i) => ({ n: i + 1, label: v.label, total: total(v) ?? -1 })).sort((a, b) => b.total - a.total || a.n - b.n);
}

export function pickConcept(rec, n, { by = "human", why = "", now = new Date() } = {}) {
  const v = rec.variants[n - 1];
  if (!v) throw new Error(`no concept ${n}; the range is 1 to ${rec.variants.length}`);
  rec.pick = { n, label: v.label, by, why, at: now.toISOString().slice(0, 10) };
  return rec;
}

export function pickByRubric(rec, now = new Date()) {
  if (!scored(rec)) throw new Error(`--pick needs all ${rec.variants.length} concepts scored first: tools\\cli asset score <slug> <n> <${RUBRIC.map((r) => r.key).join(",")}>`);
  const best = ranking(rec).find((r) => rec.variants[r.n - 1].scores.era_current !== 0);
  if (!best) throw new Error("--pick: every concept scores 0 on era_current; rewrite the subject and variants against the realism gate");
  const v = rec.variants[best.n - 1];
  const weak = RUBRIC.filter((r) => v.scores[r.key] < MAX_SCORE).map((r) => r.key);
  const why = `highest rubric total ${best.total}/${maxTotal()}${weak.length ? `; weaker on ${weak.join(", ")}` : "; full marks"}${v.note ? `; ${v.note}` : ""}`;
  return pickConcept(rec, best.n, { by: "rubric", why, now });
}

export function pickedImage(root, slug, dir) {
  const rec = loadConcepts(root, slug);
  if (!rec?.pick) return null;
  const img = join(conceptsDir(dir), `${pad(rec.pick.n)}.png`);
  return existsSync(img) ? { file: img, pick: rec.pick, variant: rec.variants[rec.pick.n - 1], model: rec.image_model || IMAGE_MODEL } : null;
}

export const imageCost = (model = IMAGE_MODEL) => meshy.COST.image[model] ?? 9;

// ---- the stage ---------------------------------------------------------------------------------
export function loadConcepts(root, slug) {
  const f = conceptsFile(root, slug);
  return existsSync(f) ? JSON.parse(readFileSync(f, "utf8")) : null;
}
export const saveConcepts = (root, slug, rec) => writeFileSync(conceptsFile(root, slug), JSON.stringify(rec, null, 2) + "\n");

export async function stageConcept(ctx, { writeState, resolveFfmpeg }) {
  const { root, spec, dir } = ctx;
  const rec = loadConcepts(root, spec.slug);
  if (!rec) {
    console.log(`no tools/assets/${spec.slug}/concepts.json: the generate stage uses text-to-3D from spec.prompt`);
    return { skipped: true };
  }
  const problems = conceptProblems(rec);
  if (problems.length) throw new Error("concepts.json problems:\n  " + problems.join("\n  "));
  const model = rec.image_model || IMAGE_MODEL;
  if (rec.reference && !existsSync(join(root, rec.reference.file))) throw new Error(`concepts.json reference ${rec.reference.file} does not exist`);
  const refUri = rec.reference ? referenceUri(root, rec) : null;
  const folder = conceptsDir(dir);
  mkdirSync(folder, { recursive: true });
  const ledger = join(dir, "credits.jsonl");
  const st = (ctx.state.concepts ||= {});
  const prompts = buildPrompts(rec.subject, rec.variants);
  const todo = prompts.filter((p) => !existsSync(join(folder, `${pad(p.n)}.png`)));
  const need = todo.filter((p) => !st[p.n]?.taskId).length * imageCost(model);
  if (todo.length) {
    const gate = realismProblems(rec);
    if (gate.length) throw new Error("concepts.json fails the realism gate (docs/asset-pipeline/concepts.md):\n  " + gate.join("\n  "));
  }
  const before = meshy.spent(ledger);
  if (before + need > spec.budget.credits) throw new Error(`concepts need about ${need} credits and ${before} are spent; the cap is ${spec.budget.credits}. Stop and report.`);
  if (todo.length) console.log(`Meshy balance ${await meshy.balance()} credits; ${todo.length} concept image(s) to make at ${imageCost(model)} each (${model}${refUri ? `, image-to-image from ${rec.reference.file}` : ""})`);
  const save = () => writeState(dir, ctx.fullState);
  // Start every task first so Meshy renders them side by side, then collect.
  for (const p of todo) {
    if (st[p.n]?.taskId) continue;
    const req = conceptRequest(model, p.prompt, refUri);
    st[p.n] = { taskId: (await meshy.call("POST", req.path, req.body, { timeoutMs: 120000 })).result, call: req.call };
    save();
  }
  for (const p of todo) {
    const call = st[p.n].call || "text-to-image";
    const t = await meshy.waitTask(st[p.n].taskId, { path: `/openapi/v1/${call}`, budgetS: 600 });
    if (!t.image_urls?.[0]) throw new Error(`concept ${p.n}: Meshy returned no image`);
    await meshy.download(t.image_urls[0], join(folder, `${pad(p.n)}.png`));
    if (!st[p.n].logged) {
      meshy.logCredits(ledger, { call, model, task: st[p.n].taskId, credits: t.consumed_credits ?? imageCost(model) });
      st[p.n].logged = true;
    }
    save();
    console.log(`  concept ${p.n} ${p.label} ok`);
  }
  rec.image_model = model;
  rec.variants.forEach((v, i) => { v.prompt = prompts[i].prompt; if (st[i + 1]?.taskId) v.taskId = st[i + 1].taskId; });
  const ff = resolveFfmpeg();
  if (!ff) throw new Error("ffmpeg not found (tools/film/.bin, or set FFMPEG_PATH): node tools/film/film.mjs fetch");
  // Models the source ladder found (Poly Haven and the libraries) sit on the same sheet, numbered after the concepts.
  const matches = await conceptMatches(ctx, folder, rec.variants.length);
  const sheet = buildSheet(ff.path, folder, { variants: [...rec.variants, ...matches] }, "sheet.png");
  console.log(`contact sheet ${sheet}`);
  if (!rec.pick && ctx.pick) {
    pickByRubric(rec);
    console.log(`picked ${rec.pick.n} (${rec.pick.label}) by rubric: ${rec.pick.why}`);
  }
  saveConcepts(root, spec.slug, rec);
  if (!rec.pick) {
    const ranked = ranking(rec).filter((r) => r.total >= 0).slice(0, 3).map((r) => `${r.n}=${r.total}`).join(" ");
    throw new Error(`awaiting a pick. Look at ${sheet}${ranked ? ` (rubric leaders ${ranked})` : ", then score each concept: tools\\cli asset score <slug> <n> <scores>"}; choose with: tools\\cli asset pick ${spec.slug} <n>   (or rerun with --pick)`);
  }
  return { pick: rec.pick, credits: meshy.spent(ledger), sheet };
}

// tools\cli asset pick <slug> <n> [--why text]   and   asset score <slug> <n> a,b,c,d,e,f [note]
export function cmdPick(root, args) {
  const slug = args[0];
  const rec = slug && loadConcepts(root, slug);
  const n = Number(args[1]);
  if (rec && Number.isInteger(n) && n > rec.variants.length) return pickCandidate(root, slug, n);
  if (rec && /^Assets\//.test(args[1] || "")) return pickCandidate(root, slug, args[1]);
  if (!rec || !Number.isInteger(n)) { console.log('usage: tools\\cli asset pick <slug> <n> [--why "..."]   (needs tools/assets/<slug>/concepts.json)'); return 2; }
  const i = args.indexOf("--why");
  pickConcept(rec, n, { by: "human", why: i >= 0 ? args.slice(i + 1).join(" ") : "" });
  saveConcepts(root, slug, rec);
  console.log(`picked concept ${n} (${rec.pick.label}) for ${slug}; next: tools\\cli asset run ${slug} --from concept`);
  return 0;
}

export function cmdScore(root, args) {
  const slug = args[0];
  const rec = slug && loadConcepts(root, slug);
  if (!rec || !args[2]) { console.log(`usage: tools\\cli asset score <slug> <n> <${RUBRIC.map((r) => r.key).join(",")}> [note]   (each 0-${MAX_SCORE})`); return 2; }
  applyScore(rec, Number(args[1]), args[2].split(",").map(Number), args.slice(3).join(" "));
  saveConcepts(root, slug, rec);
  const v = rec.variants[Number(args[1]) - 1];
  console.log(`concept ${args[1]} ${v.label}: ${total(v)}/${maxTotal()}`);
  return 0;
}
