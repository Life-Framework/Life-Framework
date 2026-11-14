#!/usr/bin/env node
// tools\cli shots previews: draw every mod item's inventory preview on contact sheets.
//
//   shots previews [--out tmp/previews/<label>] [--only Apple,Plum] [--machine local] [--keep]
//   shots previews --prefabs Label={GUID}path.et,Label2={GUID}path.et   any prefabs, vanilla ones too (the clothing verify stage)
//
// The game's harness `previews` verb draws prefabs through the same ItemPreviewWidget the inventory
// slots use (LF_ItemPreviewSheet.c), 24 per page, each labelled with its prefab name. One page per
// screenshot: <out>/previews-1.png ... plus index.json (the order and the prefabs per page). Run it before and after a
// change to CameraDistanceToItem / CameraOrbitAngles / FOV on an item and compare the sheets.
import { spawnSync } from "node:child_process";
import { existsSync, mkdirSync, rmSync, readFileSync, readdirSync, statSync, writeFileSync } from "node:fs";
import { dirname, join, relative, resolve } from "node:path";
import { decodePng } from "./bmp.mjs";

import { fileURLToPath } from "node:url";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..");
const PREFABS = join(ROOT, "addons", "LifeFramework", "Prefabs");
export const PAGE = 24;

function walk(dir, out = []) {
  for (const e of readdirSync(dir)) {
    const p = join(dir, e);
    if (statSync(p).isDirectory()) walk(p, out);
    else if (p.endsWith(".et")) out.push(p);
  }
  return out;
}

// Every inventory item prefab of the core addon: a prefab that sets InventoryItemComponent
// attributes or PreviewRenderAttributes, outside the bases, characters, traders and buildings.
export function modItems(root = PREFABS) {
  const items = [];
  for (const file of walk(root).sort()) {
    const rel = relative(root, file).split("\\").join("/");
    if (/_Base\.et$/.test(rel) || /^(Characters|Trader|Building|MP|Processor|Processing)\//.test(rel)) continue;
    const text = readFileSync(file, "utf8");
    if (!/InventoryItemComponent|PreviewRenderAttributes/.test(text)) continue;
    if (!/MeshObject|:\s*"/.test(text)) continue;
    const metaFile = `${file}.meta`;
    if (!existsSync(metaFile)) continue;
    const m = /Name "\{([0-9A-F]{16})\}/.exec(readFileSync(metaFile, "utf8"));
    if (!m) continue;
    items.push({ name: rel.replace(/^.*\//, "").replace(/\.et$/, ""), prefab: `{${m[1]}}Prefabs/${rel}` });
  }
  return items;
}

// Where each item sits in its cell: the bounding box of pixels that differ from the cell background
// (the preview rect corner colour), as fractions of the preview rect. fill = the larger side; off = centre offset from the middle.
export function measure(png, count, cols = 6, rows = 4) {
  const { width, height, pixels } = decodePng(png);
  const out = [];
  for (let i = 0; i < count; i++) {
    const c = i % cols, r = Math.floor(i / cols);
    const cw = width / cols, ch = (height * 220) / 900, half = (Math.min(cw, ch * 0.86) / 2) * 0.94;
    const cx = (c + 0.5) * cw, cy = (r + 0.43) * ch;
    const x0 = Math.round(cx - half), x1 = Math.round(cx + half), y0 = Math.round(cy - half), y1 = Math.round(cy + half);
    const at = (x, y) => { const o = (y * width + x) * 3; return [pixels[o], pixels[o + 1], pixels[o + 2]]; };
    // Background = the most common quantised colour in the rect (the cell backdrop).
    const votes = new Map();
    for (let y = y0; y < y1; y += 3) for (let x = x0; x < x1; x += 3) {
      const p = at(x, y); const k = (p[0] >> 3) * 1024 + (p[1] >> 3) * 32 + (p[2] >> 3);
      votes.set(k, (votes.get(k) ?? 0) + 1);
    }
    let best = -1, bk = 0;
    for (const [k, n] of votes) if (n > best) { best = n; bk = k; }
    const bg = [((bk >> 10) << 3) + 4, (((bk >> 5) & 31) << 3) + 4, ((bk & 31) << 3) + 4];
    let minX = 1e9, maxX = -1, minY = 1e9, maxY = -1;
    for (let y = y0; y < y1; y++) for (let x = x0; x < x1; x++) {
      const p = at(x, y);
      if (Math.abs(p[0] - bg[0]) + Math.abs(p[1] - bg[1]) + Math.abs(p[2] - bg[2]) > 36) {
        if (x < minX) minX = x; if (x > maxX) maxX = x; if (y < minY) minY = y; if (y > maxY) maxY = y;
      }
    }
    const w = x1 - x0, h = y1 - y0;
    if (maxX < 0) { out.push({ fill: 0, dx: 0, dy: 0, w: 0, h: 0 }); continue; }
    const bw = (maxX - minX + 1) / w, bh = (maxY - minY + 1) / h;
    const edge = minX <= x0 + 1 || minY <= y0 + 1 || maxX >= x1 - 2 || maxY >= y1 - 2;
    out.push({ fill: +Math.max(bw, bh).toFixed(2), w: +bw.toFixed(2), h: +bh.toFixed(2), dx: +((minX + maxX) / 2 / w - x0 / w - 0.5 + 0.0).toFixed(2), dy: 0, edge });
    const o = out[out.length - 1];
    o.dx = +(((minX + maxX) / 2 - x0) / w - 0.5).toFixed(2);
    o.dy = +(((minY + maxY) / 2 - y0) / h - 0.5).toFixed(2);
  }
  return out;
}

// "Label={GUID}path.et,..." -> [{ name, prefab }]: prefabs from anywhere, vanilla included.
export function prefabList(text) {
  return String(text).split(",").filter(Boolean).map((pair) => {
    const at = pair.indexOf("=");
    const name = pair.slice(0, at), prefab = pair.slice(at + 1);
    if (at < 1 || !/^\{[0-9A-F]{16}\}\S+\.et$/.test(prefab)) throw new Error(`--prefabs takes Label={GUID}path.et pairs, not ${pair}`);
    return { name, prefab };
  });
}

export function pages(items, size = PAGE) {
  const out = [];
  for (let i = 0; i < items.length; i += size) out.push(items.slice(i, i + size));
  return out;
}

function parse(argv) {
  const flags = {};
  for (let i = 0; i < argv.length; i++) {
    if (!argv[i].startsWith("--")) continue;
    const k = argv[i].slice(2);
    flags[k] = ["out", "only", "machine", "prefabs"].includes(k) ? argv[++i] : true;
  }
  return flags;
}

export async function main(argv) {
  const f = parse(argv);
  if (f.selftest) return selftest();
  let items = f.prefabs ? prefabList(f.prefabs) : modItems();
  if (f.only) {
    const want = new Set(f.only.split(","));
    items = items.filter((i) => want.has(i.name));
  }
  if (!items.length) {
    console.log("previews: no items matched");
    return 1;
  }
  const outDir = resolve(f.out ?? join(ROOT, "tmp", "previews", "latest"));
  mkdirSync(outDir, { recursive: true });
  const shots = await import("./shots.mjs");
  let h, remote = false, started = false;
  if (f.inner) {
    h = await import("./harness.mjs");
    const s = await import("./session.mjs");
    h.setBoxes([s.clientBox(s.sessionDir(), 1)]);
  } else {
    ({ h, remote, started } = await shots.openClient(f.machine ?? "local"));
  }
  if (remote) return runRemote(f, outDir);
  const index = [];
  try {
    shots.arrive(h);
    const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
    let n = 0;
    for (const page of pages(items)) {
      n++;
      const r = h.sendCommand({ verb: "previews", mode: "show", prefabs: page.map((i) => i.prefab) }, 20000);
      if (r.status !== "ok") throw new Error(`previews show page ${n} -> ${r.status} ${r.detail}`);
      if (n === 1) wait(3000);
      wait(2500); // the preview manager renders each prefab over a few frames
      const cap = h.sendCommand({ verb: "capture", mode: "ui", name: `previews_${n}` }, 30000);
      const src = cap.status === "ok" ? h.resolveCapture(cap.detail) : "";
      if (!src) throw new Error(`capture page ${n} -> ${cap.status} ${cap.detail}`);
      const file = `previews-${n}.png`;
      shots.image({ op: "norm", in: src, out: join(outDir, file), width: 1600 });
      const m = measure(readFileSync(join(outDir, file)), page.length);
      index.push({ file, items: page.map((i, k) => ({ name: i.name, ...m[k] })) });
      console.log(`previews: ${file}`);
      page.forEach((i, k) => console.log(`  ${i.name.padEnd(20)} fill ${m[k].fill} w ${m[k].w} h ${m[k].h} off ${m[k].dx},${m[k].dy}${m[k].edge ? " CLIPPED" : ""}${m[k].fill < 0.25 ? " TINY/EMPTY" : ""}`));
    }
    h.sendCommand({ verb: "previews", mode: "hide" }, 8000);
  } finally {
    if (started && !f.keep) shots.sessionTool(["down"]);
  }
  writeFileSync(join(outDir, "index.json"), JSON.stringify(index, null, 1));
  console.log(`previews: ${items.length} items on ${index.length} sheet(s) in ${outDir}`);
  return 0;
}

// The session is on a pool machine: run this command there (--inner), then copy its out dir back.
async function runRemote(f, outDir) {
  const s = await import("./session.mjs");
  const tb = await import("../tools/remote/testbox.mjs");
  const ptr = s.readRemotePointer();
  const rel = relative(ROOT, outDir).split("\\").join("/");
  const args = ["run", "agent-box/previews.mjs", "--inner", "--out", rel];
  if (f.only) args.push("--only", f.only);
  if (f.prefabs) args.push("--prefabs", f.prefabs);
  const r = spawnSync(process.execPath, [join(ROOT, "agent-box", "session.mjs"), ...args], { stdio: "inherit", windowsHide: true, cwd: ROOT });
  const parent = dirname(outDir);
  mkdirSync(parent, { recursive: true });
  rmSync(outDir, { recursive: true, force: true });
  const remoteDir = String(ptr.dir).split("\\").join("/").replace(/\/+$/, "");
  const c = tb.scp(["-r", `${ptr.host}:${remoteDir}/${rel}`, parent]);
  if (c.status !== 0) console.log(`previews: copying ${rel} from ${ptr.machine} failed: ${(c.stderr || "").trim()}`);
  return r.status ?? 1;
}

function selftest() {
  const items = modItems();
  const names = new Set(items.map((i) => i.name));
  for (const want of ["Apple", "Plum", "AppleJuice", "GoldNugget", "Pickaxe"]) if (!names.has(want)) throw new Error(`modItems misses ${want}`);
  for (const bad of ["Food_Base", "BlackMarketTrader"]) if (names.has(bad)) throw new Error(`modItems includes ${bad}`);
  if (!items.every((i) => /^\{[0-9A-F]{16}\}Prefabs\/.+\.et$/.test(i.prefab))) throw new Error("bad prefab path");
  if (pages(items, 24).flat().length !== items.length) throw new Error("pages lose items");
  console.log(`previews selftest ok: ${items.length} items, ${pages(items).length} page(s)`);
  return 0;
}

if (process.argv[1] && resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
  main(process.argv.slice(2)).then((c) => process.exit(c), (e) => { console.error(`previews: ${e.message}`); process.exit(1); });
}
