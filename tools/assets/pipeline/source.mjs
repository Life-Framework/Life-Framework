// Stage "source": take an existing CC0 model from Poly Haven instead of generating one with Meshy.
// Playbook: docs/asset-pipeline/sources.md (what the API allows, the catalogue fit, how to pick).
//
//   spec.source = { provider: "polyhaven", id: "cement_bag", resolution: "2k" }   fetch it
//   spec.source = "search"                                                         search, contact sheet, wait for a pick
//
//   tools\cli asset source search "<words>" | <slug> [--size <m>]   candidates and a contact sheet
//   tools\cli asset source pick <slug> <n|id> [--resolution 2k]     writes spec.source
//   tools\cli asset source info <id>                                 one asset: licence, size, files
//
// The stage downloads the glTF and its maps into tmp/assets/<slug>/polyhaven/, converts them with
// blender/polyhaven_prep.py into raw/model.glb and raw/tex_*.png (the files the Meshy generate stage
// leaves), then the clean stage carries on as for a generated model. Nothing from here is committed
// except tools/assets/<slug>/source.json, the provenance record.
//
// API terms (github.com/Poly-Haven/Public-API ToS.md, read 2026-11-14): free for any use; calls carry a
// user agent that names the software; a tool that surfaces Poly Haven content says where it comes from
// (the contact sheet and every printed list say "Poly Haven"). The models are CC0.
import { createHash } from "node:crypto";
import { copyFileSync, existsSync, mkdirSync, readFileSync, statSync, writeFileSync } from "node:fs";
import { basename, dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { buildSheet } from "./concepts.mjs";
import { resolveFfmpeg } from "../../film/film.mjs";
import { searchAll } from "./sources.mjs";
import { creditPath } from "./addon.mjs";

export const API = "https://api.polyhaven.com";
export const USER_AGENT = "LifeFrameworkAssetPipeline/1.0 (+https://life-framework.org)";
export const RESOLUTIONS = ["1k", "2k", "4k", "8k"];
export const MAX_DOWNLOAD_BYTES = 400 * 1024 * 1024;
const CACHE_HOURS = 24;

export const isPolyHaven = (spec) => spec?.source?.provider === "polyhaven";
export const isVanilla = (spec) => spec?.source?.provider === "vanilla";
// Stages a found model makes pointless: a library model needs no concept, generation or remesh; a vanilla
// model is already compiled in the engine, so it needs no clean or import either.
export function skipsStage(spec, stage) {
  if (isPolyHaven(spec)) return ["concept", "generate", "remesh"].includes(stage) ? "source is Poly Haven" : null;
  if (isVanilla(spec)) return ["concept", "generate", "remesh", "clean", "import"].includes(stage) ? "source is a vanilla base-game model" : null;
  return null;
}
export const wantsSearch = (spec) => spec?.source === "search";
export const sourceFile = (root, slug) => join(root, "tools", "assets", slug, "source.json");
const specPath = (root, slug) => join(root, "tools", "assets", slug, "spec.json");
const cacheRoot = (root) => join(root, "tmp", "polyhaven-cache");
const assetCache = (root, slug) => join(root, "tmp", "assets", slug);
export const candidatesFile = (root, slug) => join(assetCache(root, slug), "polyhaven-candidates.json");

// What the spec stage may say about a source. A Poly Haven spec needs no prompt and no credits.
export function sourceProblems(spec) {
  const s = spec.source;
  if (s === undefined || s === "search") return [];
  if (typeof s === "string") return [`source "${s}" is not "search"; use { provider: "polyhaven", id, resolution } or "search"`];
  if (s.provider === undefined) return []; // { blender, textures }: another source, checked elsewhere
  const out = [];
  if (s.provider === "vanilla") {
    if (!/^Assets\/.+\.xob$/.test(s.path || "")) out.push("source.path is the vanilla model, Assets/Props/.../Name.xob (asset_search finds it)");
    if (!/^(Prefabs|PrefabLibrary)\/.+\.et$/.test(s.prefab || "") && s.prefab !== undefined) out.push("source.prefab, when set, is the vanilla prefab to inherit (Prefabs/.../Name.et)");
  } else if (s.provider !== "polyhaven") out.push(`source.provider ${JSON.stringify(s.provider)} is not "polyhaven" or "vanilla" (Smithsonian and Sketchfab are listed by the search but fetched by hand: docs/asset-pipeline/sources.md)`);
  else {
    if (!/^[A-Za-z0-9_]+$/.test(s.id || "")) out.push("source.id is a Poly Haven asset id such as cement_bag");
    if (s.resolution !== undefined && !RESOLUTIONS.includes(s.resolution)) out.push(`source.resolution must be one of ${RESOLUTIONS.join(", ")}`);
    if (s.blender) out.push("source.blender and source.provider polyhaven are two different sources; pick one");
  }
  return out;
}

// ---------------------------------------------------------------- the API
async function get(url, { timeoutMs = 60000, binary = false } = {}) {
  const res = await fetch(url, { headers: { "User-Agent": USER_AGENT }, signal: AbortSignal.timeout(timeoutMs) });
  if (!res.ok) throw new Error(`Poly Haven ${url}: HTTP ${res.status}`);
  return binary ? Buffer.from(await res.arrayBuffer()) : res.json();
}

// One request per path per day: the catalogue is a single 550 KB call and each asset's info and files
// are fetched only for the asset in hand.
export async function api(root, path, { hours = CACHE_HOURS, fetcher = get } = {}) {
  const file = join(cacheRoot(root), path.replace(/[^A-Za-z0-9]+/g, "_") + ".json");
  if (existsSync(file) && Date.now() - statSync(file).mtimeMs < hours * 3600e3) return JSON.parse(readFileSync(file, "utf8"));
  const data = await fetcher(API + path);
  mkdirSync(dirname(file), { recursive: true });
  writeFileSync(file, JSON.stringify(data));
  return data;
}

// ---------------------------------------------------------------- search
const STOP = new Set("a an and the of for with in on at to from by or is are as it its this that into onto small large big old new late early one two three some any each plain simple single real item prop thing no text logos logo model models used use used uses look looks like made make".split(" "));
export const stem = (w) => w.replace(/(ies)$/, "y").replace(/(es|s)$/, (m, a, off, s) => (s.length - m.length >= 3 ? "" : m));
export function tokens(text) {
  const words = String(text || "").replace(/([a-z])([A-Z])/g, "$1 $2").toLowerCase().split(/[^a-z0-9]+/);
  return [...new Set(words.filter((w) => w.length >= 3 && !STOP.has(w) && !/^\d+$/.test(w)).map(stem))];
}

// Score a catalogue entry against weighted query terms. Name and id words count most, then tags,
// then categories, then the description. Dimensions are Blender-axis millimetres.
export function scoreEntry(id, e, terms) {
  const fields = [
    [4, tokens(id + " " + (e.name || ""))],
    [3, (e.tags || []).flatMap((t) => tokens(t))],
    [2, [...(e.categories || []), e.category || ""].flatMap((t) => tokens(t))],
    [1, tokens(e.description)],
  ];
  let score = 0;
  const hit = [];
  for (const [term, w] of terms) {
    const best = Math.max(0, ...fields.map(([fw, list]) => (list.includes(term) ? fw : 0)));
    if (best) { score += best * w; hit.push(term); }
  }
  return { score, hit };
}

export function queryTerms({ name = "", search = "", text = "" }) {
  const terms = new Map();
  for (const t of tokens(name)) terms.set(t, 3);
  for (const t of tokens(search)) terms.set(t, 3);
  for (const t of tokens(text)) if (!terms.has(t)) terms.set(t, 1);
  return [...terms];
}

export const maxDimM = (e) => Math.max(...(e.dimensions || [0])) / 1000;

// size is the spec's longest side in metres, when it knows one: an entry more than 4x off is the wrong
// object however well its words match (the clean stage rescales, but a 9 m cliff is not a 25 cm tin).
export function searchCatalogue(catalogue, terms, { size, limit = 8 } = {}) {
  const out = [];
  for (const [id, e] of Object.entries(catalogue)) {
    const { score, hit } = scoreEntry(id, e, terms);
    if (!score) continue;
    const m = maxDimM(e);
    const ratio = size && m ? size / m : null;
    if (ratio && (ratio > 4 || ratio < 0.25)) continue;
    out.push({ id, name: e.name, score, hit, tris: e.polycount, dims_cm: (e.dimensions || []).map((v) => Math.round(v / 10)), scale: ratio ? +ratio.toFixed(2) : null,
      categories: e.categories || [], thumbnail: e.thumbnail_url, author: Object.keys(e.authors || {}).join(", ") });
  }
  // Fewer triangles first on a tie: an asset that fits the budget needs less collapsing.
  return out.sort((a, b) => b.score - a.score || a.tris - b.tris || a.id.localeCompare(b.id)).slice(0, limit);
}

export const assetUrl = (id) => `https://polyhaven.com/a/${id}`;
export const thumbUrl = (e, px = 400) => e.thumbnail.replace(/width=\d+/, `width=${px}`).replace(/height=\d+/, `height=${px}`).replace("format=webp", "format=png");

export const rawWords = (text) => [...new Set(String(text || "").replace(/([a-z])([A-Z])/g, "$1 $2").toLowerCase().split(/[^a-z0-9]+/).filter((w) => w.length >= 3 && !STOP.has(w) && !/^\d+$/.test(w)))];
export function ladderQuery(q) {
  return { words: [...new Set([...rawWords(q.name), ...rawWords(q.search), ...rawWords(q.text)])], vanilla: q.vanilla || [] };
}

export function specQuery(spec) {
  return { name: spec.name, search: spec.search || "", text: [spec.concept, spec.prompt].filter(Boolean).join(" "), vanilla: spec.search_vanilla || [] };
}
export const specSize = (spec) => (spec.size_m ? Math.max(spec.size_m.length || 0, spec.size_m.height || 0, spec.size_m.width || 0) || undefined : undefined);

export function describe(c, n) {
  if (c.source && c.source !== "polyhaven") return `${String(n).padStart(2)}  ${String(c.id).slice(0, 44).padEnd(44)} ${c.licence}${c.credit ? " (credit needed)" : ""}  ${c.url || ""}${c.note ? `  [${c.note}]` : ""}`;
  return `${String(n).padStart(2)}  ${c.id.padEnd(28)} ${String(c.tris).padStart(7)} tris  ${c.dims_cm.join("x")} cm${c.scale ? `  size_m ${c.scale}x` : ""}  [${c.hit.join(", ")}]  ${assetUrl(c.id)}`;
}

// Thumbnails to <folder>/<NN>.png, numbered from `first`. A failed download drops that candidate.
async function fetchThumbs(folder, list, first, fetcher = get) {
  mkdirSync(folder, { recursive: true });
  const ok = [];
  for (const c of list) {
    const n = first + ok.length;
    try {
      writeFileSync(join(folder, `${String(n).padStart(2, "0")}.png`), await fetcher(c.thumb || thumbUrl(c), { binary: true, timeoutMs: 30000 }));
      ok.push({ ...c, n });
    } catch (e) {
      console.log(`  thumbnail of ${c.id} failed (${e.message}); left off the sheet`);
    }
  }
  return ok;
}

// Candidates for the concept sheet: they are numbered after the generated concepts and drawn as extra
// cells of the same sheet, so an existing model can win the pick (concepts.mjs calls this). Any failure
// (no network, an API change) leaves the sheet as it was.
export async function conceptMatches(ctx, folder, count) {
  try {
    const { root, spec } = ctx;
    const catalogue = await api(root, "/assets?t=models");
    const found = searchCatalogue(catalogue, queryTerms(specQuery(spec)), { size: specSize(spec), limit: 5 }).filter((c) => c.score >= 9);
    if (!found.length) { console.log("Poly Haven: no model in the catalogue fits this spec's words"); return []; }
    const shown = await fetchThumbs(folder, found, count + 1);
    mkdirSync(assetCache(root, spec.slug), { recursive: true });
    writeFileSync(candidatesFile(root, spec.slug), JSON.stringify(shown, null, 2) + "\n");
    console.log(`Poly Haven models (CC0) on the sheet beside the concepts; pick one with: tools\\cli asset pick ${spec.slug} <n>`);
    for (const c of shown) console.log("  " + describe(c, c.n));
    return shown.map((c) => ({ label: `PH ${c.name}`.slice(0, 22) }));
  } catch (e) {
    console.log(`Poly Haven search skipped: ${e.message}`);
    return [];
  }
}

// ---------------------------------------------------------------- the spec file
function writeSpecSource(root, slug, source) {
  const file = specPath(root, slug);
  const spec = JSON.parse(readFileSync(file, "utf8"));
  spec.source = source;
  writeFileSync(file, JSON.stringify(spec, null, 2) + "\n");
}

// `asset pick <slug> 12` for a number past the concepts, and `asset source pick <slug> <n|id>`.
export function pickCandidate(root, slug, which, resolution = "2k") {
  const file = candidatesFile(root, slug);
  const list = existsSync(file) ? JSON.parse(readFileSync(file, "utf8")) : [];
  if (/^Assets\/.+\.xob$/.test(String(which))) {
    writeSpecSource(root, slug, { provider: "vanilla", path: String(which) });
    console.log(`${slug}: source is the vanilla model ${which}; next: tools\\cli asset run ${slug} (no import), then point the prefab at it`);
    return 0;
  }
  const hit = list.find((c) => String(c.n) === String(which) || c.id === which);
  const id = hit?.id || (/^[A-Za-z][A-Za-z0-9_]+$/.test(String(which)) ? String(which) : null);
  if (!id) throw new Error(`no Poly Haven candidate ${which} for ${slug}; run: tools\\cli asset source search ${slug}`);
  writeSpecSource(root, slug, { provider: "polyhaven", id, resolution });
  console.log(`${slug}: source is now Poly Haven ${id} (${resolution}); next: tools\\cli asset run ${slug}   (concept and generate are skipped)`);
  return 0;
}

// ---------------------------------------------------------------- the stage
const sha = (buf) => createHash("md5").update(buf).digest("hex");

export function pickFiles(files, id, resolution) {
  const g = files.gltf;
  if (!g) throw new Error(`Poly Haven ${id} has no glTF download`);
  const have = Object.keys(g);
  const res = g[resolution] ? resolution : null;
  if (!res) throw new Error(`Poly Haven ${id} has no ${resolution} glTF (it has ${have.join(", ")})`);
  const root = g[res].gltf;
  const list = [{ path: basename(root.url), url: root.url, md5: root.md5, size: root.size }];
  for (const [path, f] of Object.entries(root.include || {})) list.push({ path, url: f.url, md5: f.md5, size: f.size });
  return { resolution: res, files: list };
}

// One material, one set of maps: the clean stage packs one atlas. Read from the glTF's own JSON.
export function gltfProblems(gltf, id) {
  const out = [];
  const mats = gltf.materials || [];
  const sets = new Set(mats.map((m) => m.pbrMetallicRoughness?.baseColorTexture?.index));
  if (sets.size > 1) out.push(`${id} has ${sets.size} texture sets (${mats.length} materials); the clean stage packs one atlas. Choose a single-material model, or merge the sets in Blender first`);
  if (mats.some((m) => m.alphaMode && m.alphaMode !== "OPAQUE")) out.push(`${id} uses an alpha ${mats.find((m) => m.alphaMode !== "OPAQUE").alphaMode} material (cut-out or glass); the clean stage drops alpha`);
  if (!(gltf.meshes || []).length) out.push(`${id} has no mesh`);
  return out;
}

export function creditRow(spec, rec) {
  const model = creditPath(spec, `${spec.asset_dir}/${spec.name}.xob`); // a pack asset's row names its addon (#971)
  return `| ${rec.name} (\`${model}\`) | ${rec.authors} | ${rec.url} | CC0 1.0 | \`tools/assets/${spec.slug}/source.json\` | `;
}

const CREDITS_HEADING = "## Poly Haven models";
// Adds the asset's row to docs/credits.md under CREDITS_HEADING, once. Returns true when it wrote.
export function appendCredit(file, row, url) {
  const text = readFileSync(file, "utf8");
  if (text.includes(url)) return false;
  const at = text.indexOf(CREDITS_HEADING);
  if (at < 0) throw new Error(`${file} has no "${CREDITS_HEADING}" section`);
  const lines = text.split("\n");
  let i = text.slice(0, at).split("\n").length; // the line after the heading
  while (i < lines.length && !lines[i].startsWith("|")) i++;
  while (i < lines.length && lines[i].startsWith("|")) i++;
  lines.splice(i, 0, row.replace(/ \| $/, " |"));
  writeFileSync(file, lines.join("\n"));
  return true;
}

export async function stageSource(ctx, { runBlender, blenderDir }) {
  const { root, spec, dir } = ctx;
  if (wantsSearch(spec)) {
    await searchStage(ctx);
    throw new Error(`awaiting a pick from the source ladder (a number from the sheet, or a vanilla path): tools\\cli asset source pick ${spec.slug} <n>   (or set source.provider and source.id in the spec); or drop source to generate instead`);
  }
  if (isVanilla(spec)) {
    // The base game already has it: the prefab points at (or inherits) the vanilla model. Nothing to
    // download, clean or import; the record is what a reviewer reads.
    const rec = { slug: spec.slug, provider: "vanilla", path: spec.source.path, prefab: spec.source.prefab, licence: "Bohemia Interactive base game", note: spec.source.note, fetched: new Date().toISOString().slice(0, 10) };
    writeFileSync(sourceFile(root, spec.slug), JSON.stringify(rec, null, 2) + "\n");
    console.log(`source: vanilla ${spec.source.path}; no import. Point the prefab at it (or inherit ${spec.source.prefab || "its vanilla prefab"}) and add the LF components.`);
    return { vanilla: spec.source.path, credits: 0 };
  }
  if (!isPolyHaven(spec)) return { skipped: "the spec has no Poly Haven or vanilla source" };
  const { id } = spec.source;
  const resolution = spec.source.resolution || "2k";
  const info = await api(root, `/info/${id}`);
  if (info.type !== 2) throw new Error(`${id} is not a model (Poly Haven type ${info.type})`);
  const picked = pickFiles(await api(root, `/files/${id}`), id, resolution);
  const total = picked.files.reduce((n, f) => n + (f.size || 0), 0);
  if (total > MAX_DOWNLOAD_BYTES) throw new Error(`${id} at ${resolution} is ${(total / 1048576).toFixed(0)} MB; pick a lower resolution or another model (cap ${MAX_DOWNLOAD_BYTES / 1048576} MB)`);

  const src = join(dir, "polyhaven");
  let fetched = 0;
  for (const f of picked.files) {
    const dest = join(src, f.path);
    if (existsSync(dest) && sha(readFileSync(dest)) === f.md5) continue;
    mkdirSync(dirname(dest), { recursive: true });
    const buf = await get(f.url, { binary: true, timeoutMs: 180000 });
    if (sha(buf) !== f.md5) throw new Error(`${f.path}: checksum differs from the one Poly Haven lists`);
    writeFileSync(dest, buf);
    fetched++;
  }
  const gltfFile = join(src, picked.files[0].path);
  const problems = gltfProblems(JSON.parse(readFileSync(gltfFile, "utf8")), id);
  if (problems.length) throw new Error(problems.join("\n"));

  const raw = join(dir, "raw");
  mkdirSync(raw, { recursive: true });
  const job = join(dir, "polyhaven-job.json");
  writeFileSync(job, JSON.stringify({ gltf: gltfFile, textures: join(src, "textures"), raw }, null, 1));
  runBlender(join(blenderDir, "polyhaven_prep.py"), [job], { log: join(dir, "polyhaven-prep.log") });
  const prep = JSON.parse(readFileSync(join(raw, "prep.json"), "utf8"));
  // A retextured library model (a modern label swapped for a period one): tools/assets/<slug>/base_color.png replaces the base colour map.
  const retex = join(root, "tools", "assets", spec.slug, "base_color.png");
  if (existsSync(retex)) { copyFileSync(retex, join(raw, "tex_base_color.png")); console.log(`source: base colour replaced by tools/assets/${spec.slug}/base_color.png`); }

  const rec = {
    slug: spec.slug, provider: "Poly Haven", id, name: info.name, url: assetUrl(id), licence: "CC0 1.0 (https://polyhaven.com/license)",
    authors: Object.keys(info.authors || {}).join(", "), resolution, filesHash: info.files_hash, published: info.date_published ? new Date(info.date_published * 1000).toISOString().slice(0, 10) : undefined,
    fetched: new Date().toISOString().slice(0, 10), polycount: info.polycount, dimensions_cm: (info.dimensions || []).map((v) => +(v / 10).toFixed(1)), categories: info.categories, tags: info.tags,
  };
  writeFileSync(sourceFile(root, spec.slug), JSON.stringify(rec, null, 2) + "\n");
  const credits = join(root, "docs", "credits.md");
  const wrote = existsSync(credits) && appendCredit(credits, creditRow(spec, rec), rec.url);
  console.log(`source: Poly Haven ${id} at ${resolution}, ${fetched} file(s) fetched (${(total / 1048576).toFixed(1)} MB), ${prep.tris} triangles, ${prep.size_m.map((v) => v.toFixed(3)).join(" x ")} m native (x, y, z)`);
  console.log(`source: CC0, ${rec.url}${wrote ? "; row added to docs/credits.md" : ""}. Powered by Poly Haven.`);
  const want = spec.size_m || {};
  if (want.length && Math.abs(Math.max(...prep.size_m.slice(0, 2)) - want.length) > want.length * 0.05) {
    console.log(`source: note size_m.length ${want.length} differs from the native ${Math.max(...prep.size_m.slice(0, 2)).toFixed(3)} m; the clean stage scales to size_m`);
  }
  return { id, resolution, tris: prep.tris, size_m: prep.size_m, credits: 0 };
}

// ---------------------------------------------------------------- search command and stage
export async function runSearch(root, label, folder, { terms, size, limit = 8, query }) {
  const q = query || ladderQuery({ search: label });
  console.log(`Source ladder for "${label}" (docs/asset-pipeline/sources.md): 1 vanilla, 2 open source, 3 generate`);
  const lib = await searchAll(root, q, { queries: q.vanilla, limit: 4 });
  console.log("-- rung 1: Arma Reforger base game (native, no import; pick with the path)");
  for (const c of lib.vanilla || []) console.log(`     ${c.id}`);
  if (!(lib.vanilla || []).length) console.log("     none");
  const catalogue = await api(root, "/assets?t=models");
  const ph = searchCatalogue(catalogue, terms, { size, limit }).map((c) => ({ ...c, source: "polyhaven" }));
  const others = [...(lib.smithsonian || []), ...(lib.sketchfab || [])];
  const shown = await fetchThumbs(folder, [...ph, ...others].filter((c) => c.thumb || c.thumbnail), 1);
  console.log("-- rung 2: open-source libraries (CC0, or CC-BY with its credit; thumbnails on the sheet)");
  for (const c of shown) console.log("  " + describe(c, c.n));
  if (!shown.length) console.log("     none");
  if ((lib.ambientcg || []).length) {
    console.log("-- textures (ambientCG, CC0): " + lib.ambientcg.map((c) => c.id).join(", "));
  }
  console.log("-- rung 3: generate (concept sheet, then Meshy) when nothing above fits");
  const ff = resolveFfmpeg();
  if (ff && shown.length) {
    const rec = { variants: shown.map((c) => ({ label: `${c.source === "polyhaven" ? "PH" : c.source.slice(0, 2).toUpperCase()} ${c.name}`.slice(0, 22) })) };
    console.log(`contact sheet ${buildSheet(ff.path, folder, rec, "sheet.png")}  (models from Poly Haven and the libraries listed, licences above)`);
  } else if (!ff) console.log("ffmpeg not found: no contact sheet (node tools/film/film.mjs fetch)");
  return [...(lib.vanilla || []).map((c) => ({ ...c, n: null })), ...shown];
}

async function searchStage(ctx) {
  const { root, spec, dir } = ctx;
  const shown = await runSearch(root, spec.name, join(dir, "polyhaven-search"), { terms: queryTerms(specQuery(spec)), size: specSize(spec), query: ladderQuery(specQuery(spec)) });
  mkdirSync(dir, { recursive: true });
  writeFileSync(candidatesFile(root, spec.slug), JSON.stringify(shown, null, 2) + "\n");
}

export async function main(root, args) {
  const [sub, ...rest] = args;
  const opt = (name) => { const i = rest.indexOf(name); return i >= 0 ? rest[i + 1] : undefined; };
  const free = rest.filter((a, i) => !a.startsWith("--") && !rest[i - 1]?.startsWith("--"));
  if (sub === "search" && free.length) {
    const specFileName = specPath(root, free[0]);
    if (existsSync(specFileName) && free.length === 1) {
      const spec = JSON.parse(readFileSync(specFileName, "utf8"));
      const dir = assetCache(root, spec.slug);
      const shown = await runSearch(root, spec.name, join(dir, "polyhaven-search"), { terms: queryTerms(specQuery(spec)), size: specSize(spec), query: ladderQuery(specQuery(spec)) });
      mkdirSync(dir, { recursive: true });
      writeFileSync(candidatesFile(root, spec.slug), JSON.stringify(shown, null, 2) + "\n");
      if (shown.length) console.log(`pick with: tools\\cli asset source pick ${spec.slug} <n>`);
      return 0;
    }
    const text = free.join(" ");
    const size = opt("--size") ? Number(opt("--size")) : undefined;
    const folder = join(cacheRoot(root), "search-" + text.toLowerCase().replace(/[^a-z0-9]+/g, "-").slice(0, 40));
    await runSearch(root, text, folder, { terms: queryTerms({ search: text }), size });
    return 0;
  }
  if (sub === "pick" && free.length >= 2) return pickCandidate(root, free[0], free[1], opt("--resolution") || "2k");
  if (sub === "info" && free[0]) {
    const info = await api(root, `/info/${free[0]}`);
    const picked = pickFiles(await api(root, `/files/${free[0]}`), free[0], opt("--resolution") || "2k");
    console.log(`${info.name} (${free[0]}) by ${Object.keys(info.authors || {}).join(", ")}: ${info.polycount} tris, ${(info.dimensions || []).map((v) => Math.round(v / 10)).join("x")} cm (Blender x, y, z), ${info.categories?.join(", ")}`);
    console.log(`${assetUrl(free[0])}  CC0; ${picked.files.length} files, ${(picked.files.reduce((n, f) => n + f.size, 0) / 1048576).toFixed(1)} MB at ${picked.resolution}`);
    return 0;
  }
  console.log(`usage: tools\\cli asset source search "<words>" [--size <metres>]   or   search <slug>   (a spec's own words)
       tools\\cli asset source pick <slug> <n|id> [--resolution 2k]
       tools\\cli asset source info <id> [--resolution 2k]
playbook: docs/asset-pipeline/sources.md`);
  return sub ? 2 : 0;
}

