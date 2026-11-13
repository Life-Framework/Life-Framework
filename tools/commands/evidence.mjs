// tools\cli evidence: keep before and after pictures for a PR, and print the markdown that embeds them.
//
//   tools\cli look bank                                  (before the first edit)
//   tools\cli evidence 481 before bank                   keeps tmp/look/bank/wide.png as bank-before
//   ... the work ...
//   tools\cli look bank
//   tools\cli evidence 481 after bank
//   tools\cli evidence 481 after bank --shot c03         a close-up from the same look (prefix of the file name)
//   tools\cli evidence 481 after path\to\shot.png --name sheet
//   tools\cli evidence 481 table                         the before | after table for the PR body
//
// Pictures land in website/public/docs-images/<key>/ as <name>-before.png / <name>-after.png, at most
// 1280 px wide and 500 KB (docs/agent-docs-sweep.md), and are embedded by raw URL on main, which
// resolves once the PR merges. `wt ship` refuses a player-visible PR with no embedded picture
// (tools/lib/pr-quality.mjs), and a path in backticks is not an embedded picture.
import { existsSync, mkdirSync, readFileSync, readdirSync, writeFileSync } from "node:fs";
import { extname, join } from "node:path";

export const help = "evidence <key> before|after <area|image> | table   keep before/after pictures in docs-images/<key>/ and print the PR markdown";

const RAW = "https://raw.githubusercontent.com/Life-Framework/Life-Framework/main/";
const DIR = "website/public/docs-images";
const MAX_W = 1280;
const MAX_KB = 500;

const USAGE = `usage: tools\\cli evidence <key> before|after <area | image.png|jpg|bmp> [--shot wide|top|<Entity>] [--name n]
       tools\\cli evidence <key> table
  <key>  the issue number, or a short slug when there is no issue`;

export function embedLine(key, file, alt) {
  return `![${alt}](${RAW}${DIR}/${key}/${file})`;
}

// Pairs found in a folder listing: [{ name, before, after }] sorted by name.
export function pairs(files) {
  const by = new Map();
  for (const f of files) {
    const m = /^(.+)-(before|after)\.(png|jpg|jpeg)$/i.exec(f);
    if (!m) continue;
    const row = by.get(m[1]) ?? { name: m[1], before: null, after: null };
    row[m[2].toLowerCase()] = f;
    by.set(m[1], row);
  }
  return [...by.values()].sort((a, b) => a.name.localeCompare(b.name));
}

export function table(key, files) {
  const rows = pairs(files);
  if (!rows.length) return "";
  const cell = (row, tag) => (row[tag] ? embedLine(key, row[tag], `${row.name} ${tag}`) : "(none)");
  return ["| Before | After |", "|---|---|", ...rows.map((r) => `| ${cell(r, "before")} | ${cell(r, "after")} |`)].join("\n");
}

// Nearest-neighbour downscale of an RGB buffer to at most MAX_W wide.
export function fitWidth(width, height, rgb, max = MAX_W) {
  if (width <= max) return { width, height, rgb };
  const w = max;
  const h = Math.max(1, Math.round((height * max) / width));
  const out = Buffer.alloc(w * h * 3);
  for (let y = 0; y < h; y++) {
    const sy = Math.min(height - 1, Math.floor(((y + 0.5) * height) / h));
    for (let x = 0; x < w; x++) {
      const sx = Math.min(width - 1, Math.floor(((x + 0.5) * width) / w));
      const s = (sy * width + sx) * 3;
      const d = (y * w + x) * 3;
      out[d] = rgb[s];
      out[d + 1] = rgb[s + 1];
      out[d + 2] = rgb[s + 2];
    }
  }
  return { width: w, height: h, rgb: out };
}

function resolveSource(root, src, shot) {
  if (/\.(png|jpe?g|bmp)$/i.test(src)) {
    const p = existsSync(src) ? src : join(root, src);
    return existsSync(p) ? { path: p, name: null } : null;
  }
  const dir = join(root, "tmp", "look", src);
  if (!existsSync(dir)) return null;
  const want = shot ?? "wide";
  const hit = readdirSync(dir).filter((n) => n.endsWith(".png")).find((n) => n === `${want}.png` || n.startsWith(want) || n.replace(/^c\d+_/, "") === `${want}.png`);
  return hit ? { path: join(dir, hit), name: want === "wide" ? src : `${src}-${hit.replace(/\.png$/, "").replace(/^c\d+_/, "")}` } : null;
}

function arg(list, flag) {
  const i = list.indexOf(flag);
  return i === -1 ? null : (list[i + 1] ?? "");
}

export async function main(args, ctx) {
  const list = args ?? [];
  const root = ctx.root;
  const [key, verb, src] = list;
  if (!key || !/^[A-Za-z0-9][A-Za-z0-9-]*$/.test(key) || !verb) {
    console.log(USAGE);
    return key ? 1 : 0;
  }
  const outDir = join(root, DIR, key);
  if (verb === "table") {
    const md = existsSync(outDir) ? table(key, readdirSync(outDir)) : "";
    if (!md) {
      console.log(`evidence: no <name>-before / <name>-after pictures in ${DIR}/${key}/ yet`);
      return 1;
    }
    console.log(md);
    return 0;
  }
  if (verb !== "before" && verb !== "after") {
    console.log(USAGE);
    return 1;
  }
  const { isMainCheckout } = await import("../wt.mjs");
  if (isMainCheckout(root)) {
    console.log("FAIL  evidence writes into the checkout; run it in a worktree");
    return 1;
  }
  const found = src ? resolveSource(root, src, arg(list, "--shot")) : null;
  if (!found) {
    console.log(`FAIL  no picture for '${src ?? ""}'. Give an image path, or an area that 'tools\\cli look <area>' has shot (tmp/look/<area>/)`);
    return 1;
  }
  const name = (arg(list, "--name") ?? found.name ?? "").replace(/[^A-Za-z0-9-]+/g, "-").replace(/^-+|-+$/g, "").toLowerCase();
  if (!name) {
    console.log("FAIL  an image path needs --name <short-name>");
    return 1;
  }
  mkdirSync(outDir, { recursive: true });
  const ext = extname(found.path).toLowerCase();
  let file;
  if (ext === ".png" || ext === ".bmp") {
    const bmp = await import("../../agent-box/bmp.mjs");
    const img = ext === ".bmp" ? bmp.decodeBmp(readFileSync(found.path)) : bmp.decodePng(readFileSync(found.path));
    const fit = fitWidth(img.width, img.height, img.pixels);
    file = `${name}-${verb}.png`;
    writeFileSync(join(outDir, file), bmp.pngUnder(fit.width, fit.height, fit.rgb, MAX_KB).png);
  } else {
    file = `${name}-${verb}${ext}`;
    writeFileSync(join(outDir, file), readFileSync(found.path));
  }
  console.log(`evidence: ${DIR}/${key}/${file}`);
  console.log(embedLine(key, file, `${name} ${verb}`));
  const other = verb === "before" ? "after" : "before";
  if (pairs(readdirSync(outDir)).find((r) => r.name === name && r[other])) console.log(`pair complete: 'tools\\cli evidence ${key} table' prints the before | after table`);
  console.log("look at the picture before you keep it; commit it with the change");
  return 0;
}
