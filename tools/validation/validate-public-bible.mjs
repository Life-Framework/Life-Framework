// tools/validation/validate-public-bible.mjs
// inputs: docs/everon/**, docs/roadmap/**, website/content/**, website/scripts/build-content.mjs, website/scripts/live-now.mjs, addons/LF_Everon1989/Missions/**
//
// The World of Everon page is public; docs/everon/world-bible-internal.md holds the story spoilers and must
// not reach the site (docs/architecture/everon-world.md D4 and section 6). Fails when
//   1. any page the site renders (the docs build-content.mjs registers, docs/roadmap/**, website/content/**)
//      names world-bible-internal.md;
//   2. the public page (docs/everon/world-bible.md), a place dossier's picture caption, or the generated
//      "Live now" section contains a term from the internal file's `## Spoiler terms` list.
// A missing or empty term list is a failure: the check would pass everything.
// Proof: tools/validation/public-bible-selftest.mjs over tools/validation/fixtures/public-bible/.

import fs from "node:fs";
import path from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";
import { liveNowMarkdown, PLACES } from "../../website/scripts/live-now.mjs";

export const INTERNAL = "docs/everon/world-bible-internal.md";
const PUBLIC = "docs/everon/world-bible.md";

const read = (f) => fs.readFileSync(f, "utf8").replace(/\r\n/g, "\n");
const norm = (s) => s.toLowerCase().replace(/[‘’ʼ]/g, "'").replace(/\s+/g, " ");

function walk(dir) {
  if (!fs.existsSync(dir)) return [];
  return fs.readdirSync(dir, { withFileTypes: true }).flatMap((e) => {
    const p = path.join(dir, e.name);
    return e.isDirectory() ? walk(p) : [p];
  });
}

/** The bullets under the internal file's `## Spoiler terms` heading, backticks stripped; null when there is no such section. */
export function spoilerTerms(root) {
  const f = path.join(root, INTERNAL);
  if (!fs.existsSync(f)) return null;
  const lines = read(f).split("\n");
  const start = lines.findIndex((l) => /^## Spoiler terms\s*$/.test(l));
  if (start < 0) return null;
  const terms = [];
  for (const l of lines.slice(start + 1)) {
    if (/^## /.test(l)) break;
    const m = /^\s*[-*]\s+(.+?)\s*$/.exec(l);
    if (m) terms.push(m[1].replace(/`/g, ""));
  }
  return terms;
}

/** Repo paths of the docs the site renders: the addRepoDoc rows of build-content.mjs plus docs/roadmap/**. */
export function siteDocs(root) {
  const f = path.join(root, "website", "scripts", "build-content.mjs");
  const rows = fs.existsSync(f) ? [...read(f).matchAll(/^\s*addRepoDoc\("([^"]+)"/gm)].map((m) => m[1]) : [];
  const roadmap = walk(path.join(root, "docs", "roadmap")).filter((p) => p.endsWith(".md")).map((p) => path.relative(root, p).split(path.sep).join("/"));
  return [...new Set([...rows, ...roadmap])];
}

export function checkPublicBible(root) {
  const problems = [];
  const terms = spoilerTerms(root);
  if (!terms || !terms.length) problems.push(`${INTERNAL}: no \`## Spoiler terms\` list, so nothing can be checked; add one bullet per term`);

  const named = [];
  for (const rel of siteDocs(root)) {
    const f = path.join(root, rel);
    if (fs.existsSync(f) && /world-bible-internal/i.test(read(f))) named.push(rel);
  }
  for (const f of walk(path.join(root, "website", "content"))) {
    if (/\.(md|json|txt)$/i.test(f) && /world-bible-internal/i.test(read(f))) named.push(path.relative(root, f).split(path.sep).join("/"));
  }
  for (const rel of named) problems.push(`${rel}: names world-bible-internal.md; the site content must not link or mention it (D4)`);

  const surfaces = [];
  if (fs.existsSync(path.join(root, PUBLIC))) surfaces.push({ where: PUBLIC, text: read(path.join(root, PUBLIC)) });
  const places = path.join(root, PLACES);
  for (const slug of fs.existsSync(places) ? fs.readdirSync(places).sort() : []) {
    const f = path.join(places, slug, "dossier.json");
    if (!fs.existsSync(f)) continue;
    let d;
    try {
      d = JSON.parse(read(f));
    } catch {
      problems.push(`${PLACES}/${slug}/dossier.json: not valid JSON`);
      continue;
    }
    (d.pictures || []).forEach((p, i) => surfaces.push({ where: `${PLACES}/${slug}/dossier.json picture ${i + 1} caption`, text: String(p.caption || "") }));
  }
  surfaces.push({ where: "the generated Live now section", text: liveNowMarkdown(root) });

  for (const { where, text } of surfaces) {
    const hay = norm(text);
    for (const t of terms || []) if (hay.includes(norm(t))) problems.push(`${where}: uses the spoiler term "${t}"`);
  }
  return problems;
}

if (process.argv[1] && import.meta.url === pathToFileURL(path.resolve(process.argv[1])).href) {
  const rootArg = process.argv.indexOf("--root");
  const root = rootArg > 0 ? path.resolve(process.argv[rootArg + 1]) : path.resolve(fileURLToPath(import.meta.url), "..", "..", "..");
  const problems = checkPublicBible(root);
  if (problems.length) {
    for (const p of problems) console.log(`FAIL public-bible: ${p}`);
    console.log("  keep spoilers in docs/everon/world-bible-internal.md only; reword the public text or the caption");
    process.exit(1);
  }
  console.log("OK public-bible: the site content never names the internal file and uses no spoiler term");
}
