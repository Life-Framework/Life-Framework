// Builds the gallery from website/public/docs-images (the captures agents commit with their pull
// requests) and content/gallery.json (captions, sections, skips).
//
//   public/docs-images/<issue number>/<name>.png       one issue's captures
//   public/docs-images/area-<n>-<slug>/<name>-before|after.png   an area-polish pair
//   other files at the root of docs-images              ignored (setup screenshots)
//
// Each kept picture is converted to webp at 640 and 1280 wide into public/gallery/ (git-ignored,
// rebuilt when the source is newer). A picture that is not in the manifest still appears, with its
// file name as the caption and its issue's title as the heading, so a new capture needs no site
// change. List it under "skip" to hide it.

import fs from "node:fs";
import path from "node:path";
import sharp from "sharp";

const WIDTHS = [640, 1280];
const IMG = /\.(png|jpe?g|webp)$/i;

const humanize = (stem) => {
  const s = stem
    .replace(/^m\d+_\d+_/i, "")
    .replace(/_client\d+_\d+$/i, "")
    .replace(/[-_]+/g, " ")
    .trim();
  return s.charAt(0).toUpperCase() + s.slice(1);
};

// "M2-Medic: Downed state on lethal damage" -> "Downed state on lethal damage"
const cleanTitle = (t) =>
  t
    .replace(/^M\d+:\s*M\d+-\d+:?\s*/, "")
    .replace(/^(?:M\d+|Dev)-[A-Za-z][\w &]*?:\s*/, "")
    .replace(/^M\d+\s+epic:\s*/i, "")
    .replace(/\s*\(M\d+\)$/, "");

async function convert(src, outDir, stem) {
  const widths = [];
  const meta = await sharp(src).metadata();
  const srcTime = fs.statSync(src).mtimeMs;
  for (const w of WIDTHS) {
    if (w > meta.width && widths.length > 0) continue;
    const width = Math.min(w, meta.width);
    const out = path.join(outDir, `${stem}-${w}.webp`);
    if (!fs.existsSync(out) || fs.statSync(out).mtimeMs < srcTime) {
      fs.mkdirSync(outDir, { recursive: true });
      await sharp(src).resize({ width, withoutEnlargement: true }).webp({ quality: 78 }).toFile(out);
    }
    widths.push(w);
  }
  return { widths, width: meta.width, height: meta.height };
}

export async function buildGallery({ site, github, repoUrl }) {
  const manifest = JSON.parse(fs.readFileSync(path.join(site, "content", "gallery.json"), "utf8"));
  const root = path.join(site, "public", "docs-images");
  const outRoot = path.join(site, "public", "gallery");
  const skip = new Set(manifest.skip || []);

  // Issue title and epic from GitHub, so a folder named by an issue number heads itself.
  const issues = new Map();
  const epicOf = new Map();
  for (const m of [...github.milestones, ...(github.tracks || [])]) {
    for (const i of m.issues) {
      issues.set(i.number, i);
      for (const s of i.subIssues || []) {
        if (!epicOf.has(s.number)) epicOf.set(s.number, i);
        if (!issues.has(s.number)) issues.set(s.number, s);
      }
    }
  }

  const picture = async (dirName, file, caption) => {
    const stem = path.basename(file).replace(IMG, "");
    const info = await convert(path.join(root, dirName, file), path.join(outRoot, dirName), stem);
    const base = `/gallery/${dirName}/${stem}`;
    return {
      id: `${dirName}/${stem}`,
      src: `${base}-${Math.max(...info.widths)}.webp`,
      srcSet: info.widths.map((w) => `${base}-${w}.webp ${w}w`).join(", "),
      width: info.width,
      height: info.height,
      caption,
    };
  };

  const entries = fs.readdirSync(root, { withFileTypes: true }).filter((e) => e.isDirectory());
  const sectionsById = new Map((manifest.sections || []).map((s) => [s.id, { ...s, issues: [] }]));
  const other = { id: "town", title: "The town", blurb: "", issues: [] };
  const sectionFor = (n) => {
    const manual = (manifest.issueSection || {})[String(n)];
    if (manual && sectionsById.has(manual)) return sectionsById.get(manual);
    const epic = epicOf.get(n);
    if (epic && sectionsById.has(`epic-${epic.number}`)) return sectionsById.get(`epic-${epic.number}`);
    return sectionsById.get(other.id) || other;
  };
  if (!sectionsById.has(other.id)) sectionsById.set(other.id, other);

  // Issue folders, in number order.
  const numbered = entries.filter((e) => /^\d+$/.test(e.name)).sort((a, b) => Number(a.name) - Number(b.name));
  for (const e of numbered) {
    const n = Number(e.name);
    const files = fs.readdirSync(path.join(root, e.name)).filter((f) => IMG.test(f)).sort();
    const pics = [];
    for (const f of files) {
      const key = `${e.name}/${f}`;
      if (skip.has(key)) continue;
      const caption = (manifest.captions || {})[key.replace(IMG, "")] || humanize(f.replace(IMG, ""));
      pics.push(await picture(e.name, f, caption));
    }
    if (pics.length === 0) continue;
    const issue = issues.get(n);
    const override = (manifest.issueTitles || {})[String(n)];
    sectionFor(n).issues.push({
      number: n,
      title: override || (issue ? cleanTitle(issue.title) : `Issue ${n}`),
      url: `${repoUrl}/issues/${n}`,
      pictures: pics,
    });
  }

  // Area-polish folders: pairs by "<name>-before" and "<name>-after".
  const areas = [];
  const areaDirs = entries
    .map((e) => ({ name: e.name, m: /^area-(\d+)-(.+)$/.exec(e.name) }))
    .filter((x) => x.m)
    .sort((a, b) => Number(a.m[1]) - Number(b.m[1]));
  for (const { name, m } of areaDirs) {
    const files = fs.readdirSync(path.join(root, name)).filter((f) => IMG.test(f)).sort();
    const stems = new Map();
    for (const f of files) {
      if (skip.has(`${name}/${f}`)) continue;
      const stem = f.replace(IMG, "");
      const pm = /^(.*?)-?(before|after)$/.exec(stem);
      const key = pm ? pm[1] || "view" : stem;
      if (!stems.has(key)) stems.set(key, {});
      stems.get(key)[pm ? pm[2] : "single"] = f;
    }
    const pairs = [];
    const singles = [];
    for (const [key, v] of stems) {
      const caption = (manifest.captions || {})[`${name}/${key}`] || humanize(key);
      if (v.before && v.after) pairs.push({ name: caption, before: await picture(name, v.before, `${caption}, before`), after: await picture(name, v.after, `${caption}, after`) });
      else for (const f of Object.values(v)) singles.push(await picture(name, f, caption));
    }
    const title = (manifest.areaTitles || {})[name] || humanize(m[2]);
    areas.push({ id: name, number: Number(m[1]), title, pairs, singles });
  }

  const sections = [...sectionsById.values()]
    .filter((s) => s.issues.length > 0)
    .map(({ id, title, blurb, issues: list }) => ({ id, title, blurb: blurb || "", issues: list }));
  const count = sections.reduce((n, s) => n + s.issues.reduce((k, i) => k + i.pictures.length, 0), 0) + areas.reduce((n, a) => n + a.pairs.length * 2 + a.singles.length, 0);
  return { sections, areas, count };
}
