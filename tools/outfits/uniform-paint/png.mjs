// A tiny RGBA raster and PNG writer for the uniform paint overlays (no dependencies).
import { deflateSync } from "node:zlib";
import { mkdirSync, writeFileSync } from "node:fs";
import { dirname } from "node:path";

const CRC = (() => {
  const t = new Uint32Array(256);
  for (let n = 0; n < 256; n++) {
    let c = n;
    for (let k = 0; k < 8; k++) c = c & 1 ? 0xedb88320 ^ (c >>> 1) : c >>> 1;
    t[n] = c >>> 0;
  }
  return t;
})();
function crc32(buf) {
  let c = 0xffffffff;
  for (const b of buf) c = CRC[(c ^ b) & 0xff] ^ (c >>> 8);
  return (c ^ 0xffffffff) >>> 0;
}
function chunk(type, data) {
  const out = Buffer.alloc(12 + data.length);
  out.writeUInt32BE(data.length, 0);
  out.write(type, 4, "latin1");
  data.copy(out, 8);
  out.writeUInt32BE(crc32(out.subarray(4, 8 + data.length)), 8 + data.length);
  return out;
}

export class Canvas {
  constructor(w, h = w) {
    this.w = w;
    this.h = h;
    this.px = new Uint8ClampedArray(w * h * 4); // straight alpha, transparent
  }
  // Source-over of one pixel, colour [r,g,b] 0..255, alpha 0..1.
  put(x, y, rgb, a = 1) {
    x |= 0; y |= 0;
    if (x < 0 || y < 0 || x >= this.w || y >= this.h || a <= 0) return;
    const i = (y * this.w + x) * 4, da = this.px[i + 3] / 255, oa = a + da * (1 - a);
    for (let k = 0; k < 3; k++) this.px[i + k] = (rgb[k] * a + this.px[i + k] * da * (1 - a)) / oa;
    this.px[i + 3] = oa * 255;
  }
  rect(x, y, w, h, rgb, a = 1) {
    for (let j = 0; j < h; j++) for (let i = 0; i < w; i++) this.put(x + i, y + j, rgb, a);
  }
  // Anti-aliased by 4x4 coverage on the edge: inside(px, py) -> bool.
  shape(x0, y0, x1, y1, inside, rgb, a = 1) {
    for (let y = Math.floor(y0); y <= Math.ceil(y1); y++)
      for (let x = Math.floor(x0); x <= Math.ceil(x1); x++) {
        let n = 0;
        for (let sy = 0; sy < 3; sy++) for (let sx = 0; sx < 3; sx++) if (inside(x + (sx + 0.5) / 3, y + (sy + 0.5) / 3)) n++;
        if (n) this.put(x, y, rgb, a * n / 9);
      }
  }
  disc(cx, cy, r, rgb, a = 1) {
    this.shape(cx - r, cy - r, cx + r, cy + r, (x, y) => (x - cx) ** 2 + (y - cy) ** 2 <= r * r, rgb, a);
  }
  ring(cx, cy, r0, r1, rgb, a = 1) {
    this.shape(cx - r1, cy - r1, cx + r1, cy + r1, (x, y) => { const d = (x - cx) ** 2 + (y - cy) ** 2; return d <= r1 * r1 && d >= r0 * r0; }, rgb, a);
  }
  // A convex or concave polygon, even-odd.
  poly(pts, rgb, a = 1) {
    const xs = pts.map((p) => p[0]), ys = pts.map((p) => p[1]);
    this.shape(Math.min(...xs), Math.min(...ys), Math.max(...xs), Math.max(...ys), (x, y) => {
      let c = false;
      for (let i = 0, j = pts.length - 1; i < pts.length; j = i++) {
        const [xi, yi] = pts[i], [xj, yj] = pts[j];
        if (yi > y !== yj > y && x < ((xj - xi) * (y - yi)) / (yj - yi) + xi) c = !c;
      }
      return c;
    }, rgb, a);
  }
  star(cx, cy, r, points, rgb, a = 1, inner = 0.45, rot = -Math.PI / 2) {
    const pts = [];
    for (let i = 0; i < points * 2; i++) {
      const rr = i % 2 ? r * inner : r, t = rot + (i * Math.PI) / points;
      pts.push([cx + rr * Math.cos(t), cy + rr * Math.sin(t)]);
    }
    this.poly(pts, rgb, a);
  }
  png() {
    const raw = Buffer.alloc((this.w * 4 + 1) * this.h);
    for (let y = 0; y < this.h; y++) {
      raw[y * (this.w * 4 + 1)] = 0;
      Buffer.from(this.px.buffer, y * this.w * 4, this.w * 4).copy(raw, y * (this.w * 4 + 1) + 1);
    }
    const ihdr = Buffer.alloc(13);
    ihdr.writeUInt32BE(this.w, 0); ihdr.writeUInt32BE(this.h, 4); ihdr[8] = 8; ihdr[9] = 6;
    return Buffer.concat([Buffer.from([137, 80, 78, 71, 13, 10, 26, 10]), chunk("IHDR", ihdr), chunk("IDAT", deflateSync(raw, { level: 9 })), chunk("IEND", Buffer.alloc(0))]);
  }
  save(path) {
    mkdirSync(dirname(path), { recursive: true });
    writeFileSync(path, this.png());
  }
}

// 7-segment digits, for the calibration grids.
const SEG = { 0: "abcdef", 1: "bc", 2: "abged", 3: "abgcd", 4: "fgbc", 5: "afgcd", 6: "afgedc", 7: "abc", 8: "abcdefg", 9: "abcdfg" };
export function digit(cv, n, x, y, h, rgb) {
  const t = h / 7, w = h * 0.55;
  const s = {
    a: [x, y, w, t], d: [x, y + h - t, w, t], g: [x, y + h / 2 - t / 2, w, t],
    f: [x, y, t, h / 2], b: [x + w - t, y, t, h / 2], e: [x, y + h / 2, t, h / 2], c: [x + w - t, y + h / 2, t, h / 2],
  };
  for (const k of SEG[n]) cv.rect(...s[k], rgb);
}
export function number(cv, n, x, y, h, rgb) {
  const str = String(n);
  for (let i = 0; i < str.length; i++) digit(cv, +str[i], x + i * h * 0.75, y, h, rgb);
}
