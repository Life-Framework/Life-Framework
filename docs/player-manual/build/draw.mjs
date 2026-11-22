// docs/player-manual/build/draw.mjs - one rendered map of the real island with numbered markers on it.
//
// The base is the repo's own offline renderer (tools/map/render/render.mjs, the terrain, coast, contours and
// buildings of the real Everon), rendered with no POIs; this file paints numbered markers, name labels and a
// colour key over it with the same small canvas (tools/map/lib/raster.mjs).
import { renderMap } from "../../../tools/map/render/render.mjs";
import { decodePng, encodePng } from "../../../tools/map/lib/png.mjs";
import { Canvas } from "../../../tools/map/lib/raster.mjs";
import { GROUPS, ROOT } from "./places.mjs";

const INK = [52, 40, 30];
const PAPER = [244, 236, 214];
const WHITE = [255, 255, 255];

/** Render a bbox [x0, z0, x1, z1] (world metres) at `scale` pixels per metre and return a Canvas. */
async function base(bbox, scale) {
  const w = Math.round((bbox[2] - bbox[0]) * scale);
  const h = Math.round((bbox[3] - bbox[1]) * scale);
  const out = await renderMap({ root: ROOT, world: "main", files: [], bbox, size: [w, h], zoom: Math.log2(12800 / (bbox[2] - bbox[0])) });
  if (out.layer !== "raster") throw new Error(`manual: the base map is the ${out.layer} layer; run "tools\\cli map status" (the map cache is missing)`);
  const img = decodePng(out.png);
  const cv = new Canvas(img.w, img.h);
  cv.data = new Uint8Array(img.data.buffer, img.data.byteOffset, img.w * img.h * 3).slice();
  return { cv, scale, bbox, w: img.w, h: img.h };
}

const rectsOverlap = (a, b) => a[0] < b[0] + b[2] && a[0] + a[2] > b[0] && a[1] < b[1] + b[3] && a[1] + a[3] > b[1];

/**
 * opts: bbox, scale, title, markers [{ n, group, name, x, z, label }], textScale (label size), radius (disc),
 * labelAll (draw the name beside every marker that has room), key (draw the colour key), maxLabelChars.
 * Returns { png, placed: [{ n, px, py }] }.
 */
export async function drawMap(opts) {
  const { cv, scale, bbox, w, h } = await base(opts.bbox, opts.scale);
  const ts = opts.textScale || 1;
  const r = opts.radius || 8;
  const sx = (x) => (x - bbox[0]) * scale;
  const sy = (z) => (bbox[3] - z) * scale;

  // The renderer's own corner caption ("EVERON NEW Z4.0") is replaced by the map title.
  const title = (opts.title || "").toUpperCase();
  const tw = cv.textWidth(title, 2) + 16;
  cv.rect(0, 0, Math.max(tw, 190), 26, PAPER, 0.96);
  cv.text(title, 8, 6, INK, 2);

  const occupied = [[0, 0, Math.max(tw, 190), 26], [w - 40, 0, 40, 50]];
  const discs = [];
  const markers = opts.markers.map((m) => ({ ...m, tx: sx(m.x), ty: sy(m.z) }));
  const inside = (m) => m.tx > -r && m.ty > -r && m.tx < w + r && m.ty < h + r;
  for (const m of markers.filter(inside)) {
    // Nudge a disc clear of the others; a leader line keeps its true spot.
    let px = m.tx, py = m.ty;
    const free = (x, y) => x > r + 2 && y > r + 2 && x < w - r - 2 && y < h - r - 2 && discs.every((d) => Math.hypot(d.px - x, d.py - y) >= 2 * r + 1) && !occupied.some((q) => rectsOverlap([x - r, y - r, 2 * r, 2 * r], q));
    if (!free(px, py)) {
      let found = false;
      for (let ring = 1; ring <= 6 && !found; ring++) {
        for (let a = 0; a < 16 && !found; a++) {
          const ang = (a / 16) * Math.PI * 2;
          const x = m.tx + Math.cos(ang) * ring * (r + 2), y = m.ty + Math.sin(ang) * ring * (r + 2);
          if (free(x, y)) { px = x; py = y; found = true; }
        }
      }
    }
    discs.push({ ...m, px, py });
  }
  for (const d of discs) {
    if (Math.hypot(d.px - d.tx, d.py - d.ty) > 2) {
      cv.line(d.tx, d.ty, d.px, d.py, INK, 0.9, 1);
      cv.disc(d.tx, d.ty, 2.2, INK, 1);
    }
  }
  for (const d of discs) {
    const col = GROUPS[d.group].color;
    cv.disc(d.px, d.py, r + 1.5, WHITE, 0.95);
    cv.disc(d.px, d.py, r, col, 1);
    const s = String(d.n);
    const nsc = r >= 11 ? 2 : 1;
    cv.text(s, d.px - cv.textWidth(s, nsc) / 2, d.py - (7 * nsc) / 2 + 0.5, WHITE, nsc);
    occupied.push([d.px - r - 1, d.py - r - 1, 2 * r + 2, 2 * r + 2]);
  }
  if (opts.labelAll) {
    const maxc = opts.maxLabelChars || 26;
    for (const d of discs) {
      if (d.noLabel) continue;
      let label = (d.label || d.name).toUpperCase();
      if (label.length > maxc) label = label.slice(0, maxc - 1) + ".";
      const lw = cv.textWidth(label, ts), lh = 7 * ts;
      const tries = [[r + 3, -lh / 2], [-r - 3 - lw, -lh / 2], [-lw / 2, r + 3], [-lw / 2, -r - 3 - lh], [r + 1, r + 1], [-r - 1 - lw, r + 1], [r + 1, -r - 1 - lh], [-r - 1 - lw, -r - 1 - lh]];
      for (const [ox, oy] of tries) {
        const rect = [d.px + ox, d.py + oy, lw, lh];
        if (rect[0] < 2 || rect[1] < 2 || rect[0] + lw > w - 2 || rect[1] + lh > h - 2) continue;
        if (occupied.some((q) => rectsOverlap([rect[0] - 1, rect[1] - 1, rect[2] + 2, rect[3] + 2], q))) continue;
        cv.text(label, rect[0], rect[1], INK, ts, PAPER);
        occupied.push(rect);
        d.labelled = true;
        break;
      }
    }
  }
  if (opts.key !== false) {
    const used = [...new Set(discs.map((d) => d.group))];
    let y = h - 12 - used.length * 12;
    const kx = w - 190;
    cv.rect(kx - 6, y - 6, 188, used.length * 12 + 10, PAPER, 0.9);
    for (const g of used) {
      cv.disc(kx + 4, y + 3, 4, GROUPS[g].color, 1);
      cv.text(GROUPS[g].name, kx + 14, y, INK, 1);
      y += 12;
    }
  }
  const q = opts.quant ?? 2;
  if (q) for (let i = 0; i < cv.data.length; i++) cv.data[i] = Math.min(255, ((cv.data[i] >> q) << q) + (1 << (q - 1)));
  return { png: encodePng(cv.image(), 9), placed: discs.map((d) => ({ n: d.n, name: d.name, labelled: !!d.labelled })), w, h };
}
