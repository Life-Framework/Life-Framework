// Builds the gallery from website/public/docs-images (the captures agents commit with their pull
// requests) and content/gallery.json, which lists the pictures the gallery shows.
//
// The gallery is a short curated set: `featured` in content/gallery.json names each picture by its
// path under docs-images (no extension) and gives it a plain caption. Everything else in docs-images
// stays where it is for the docs and pull requests that use it, but is not shown. Before shots and
// before/after pairs are never shown. To add a picture, convert it and add an entry; to drop one,
// remove its entry.
//
// `usedByPages` lists pictures that other pages embed (the Features page); they are converted to webp
// but are not shown in the gallery.
//
// Each featured picture is converted to webp at 640 and 1280 wide into public/gallery/ (git-ignored,
// rebuilt when the source is newer).

import fs from "node:fs";
import path from "node:path";
import sharp from "sharp";

const WIDTHS = [640, 1280];
const IMG = /\.(png|jpe?g|webp)$/i;

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
      await sharp(src).resize({ width, withoutEnlargement: true }).webp({ quality: 80 }).toFile(out);
    }
    widths.push(w);
  }
  return { widths, width: meta.width, height: meta.height };
}

export async function buildGallery({ site }) {
  const manifest = JSON.parse(fs.readFileSync(path.join(site, "content", "gallery.json"), "utf8"));
  const root = path.join(site, "public", "docs-images");
  const outRoot = path.join(site, "public", "gallery");

  const prepare = async (id) => {
    const dirName = path.dirname(id);
    const stem = path.basename(id);
    const file = fs.readdirSync(path.join(root, dirName)).find((f) => f.replace(IMG, "") === stem && IMG.test(f));
    if (!file) throw new Error(`gallery: picture "${id}" is not in public/docs-images`);
    return { dirName, stem, info: await convert(path.join(root, dirName, file), path.join(outRoot, dirName), stem) };
  };

  for (const id of manifest.usedByPages || []) await prepare(id);

  const pictures = [];
  for (const { id, caption } of manifest.featured || []) {
    if (/(^|[-/])before$/.test(id)) throw new Error(`gallery: "${id}" is a before shot; the gallery shows finished pictures only`);
    if (!caption) throw new Error(`gallery: featured picture "${id}" has no caption`);
    const { dirName, stem, info } = await prepare(id);
    const base = `/gallery/${dirName}/${stem}`;
    pictures.push({
      id,
      src: `${base}-${Math.max(...info.widths)}.webp`,
      srcSet: info.widths.map((w) => `${base}-${w}.webp ${w}w`).join(", "),
      width: info.width,
      height: info.height,
      caption,
    });
  }
  return { pictures, count: pictures.length };
}
