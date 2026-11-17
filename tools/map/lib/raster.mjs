// tools/map/lib/raster.mjs - a small RGB canvas: blended lines, discs, polygons, hatching and 5x7 text.
// 5x7 glyphs: seven row bytes each (bit 4 is the left column).
const GLYPHS = {
  A: "0e 11 11 1f 11 11 11", B: "1e 11 11 1e 11 11 1e", C: "0e 11 10 10 10 11 0e", D: "1e 11 11 11 11 11 1e",
  E: "1f 10 10 1e 10 10 1f", F: "1f 10 10 1e 10 10 10", G: "0e 11 10 17 11 11 0f", H: "11 11 11 1f 11 11 11",
  I: "0e 04 04 04 04 04 0e", J: "07 02 02 02 02 12 0c", K: "11 12 14 18 14 12 11", L: "10 10 10 10 10 10 1f",
  M: "11 1b 15 15 11 11 11", N: "11 11 19 15 13 11 11", O: "0e 11 11 11 11 11 0e", P: "1e 11 11 1e 10 10 10",
  Q: "0e 11 11 11 15 12 0d", R: "1e 11 11 1e 14 12 11", S: "0f 10 10 0e 01 01 1e", T: "1f 04 04 04 04 04 04",
  U: "11 11 11 11 11 11 0e", V: "11 11 11 11 11 0a 04", W: "11 11 11 15 15 15 0a", X: "11 11 0a 04 0a 11 11",
  Y: "11 11 0a 04 04 04 04", Z: "1f 01 02 04 08 10 1f",
  0: "0e 11 13 15 19 11 0e", 1: "04 0c 04 04 04 04 0e", 2: "0e 11 01 02 04 08 1f", 3: "1f 02 04 02 01 11 0e",
  4: "02 06 0a 12 1f 02 02", 5: "1f 10 1e 01 01 11 0e", 6: "06 08 10 1e 11 11 0e", 7: "1f 01 02 04 08 08 08",
  8: "0e 11 11 0e 11 11 0e", 9: "0e 11 11 0f 01 02 0c",
  "-": "00 00 00 1f 00 00 00", ".": "00 00 00 00 00 0c 0c", ",": "00 00 00 00 0c 04 08", "'": "0c 04 08 00 00 00 00",
  ":": "00 0c 0c 00 0c 0c 00", "/": "01 01 02 04 08 10 10", "(": "02 04 08 08 08 04 02", ")": "08 04 02 02 02 04 08",
  "?": "0e 11 01 02 04 00 04", "+": "00 04 04 1f 04 04 00", " ": "00 00 00 00 00 00 00",
};
const glyph = new Map();
for (const [ch, hex] of Object.entries(GLYPHS)) glyph.set(ch, hex.split(" ").map((h) => parseInt(h, 16)));

export class Canvas {
  constructor(w, h, fill = [232, 220, 190]) {
    this.w = w;
    this.h = h;
    this.ch = 3;
    this.data = new Uint8Array(w * h * 3);
    for (let i = 0; i < w * h; i++) { this.data[i * 3] = fill[0]; this.data[i * 3 + 1] = fill[1]; this.data[i * 3 + 2] = fill[2]; }
  }
  px(x, y, c, a = 1) {
    x |= 0; y |= 0;
    if (x < 0 || y < 0 || x >= this.w || y >= this.h) return;
    const k = (y * this.w + x) * 3, d = this.data;
    if (a >= 1) { d[k] = c[0]; d[k + 1] = c[1]; d[k + 2] = c[2]; return; }
    d[k] += (c[0] - d[k]) * a; d[k + 1] += (c[1] - d[k + 1]) * a; d[k + 2] += (c[2] - d[k + 2]) * a;
  }
  line(x0, y0, x1, y1, c, a = 1, wid = 1) {
    const n = Math.max(Math.abs(x1 - x0), Math.abs(y1 - y0), 1);
    const r = (wid - 1) / 2;
    for (let i = 0; i <= n; i++) {
      const x = x0 + ((x1 - x0) * i) / n, y = y0 + ((y1 - y0) * i) / n;
      if (wid <= 1) this.px(x, y, c, a);
      else for (let dy = -r; dy <= r; dy++) for (let dx = -r; dx <= r; dx++) this.px(x + dx, y + dy, c, a);
    }
  }
  disc(cx, cy, r, c, a = 1) {
    for (let y = Math.floor(cy - r); y <= cy + r; y++) for (let x = Math.floor(cx - r); x <= cx + r; x++) if ((x - cx) ** 2 + (y - cy) ** 2 <= r * r) this.px(x, y, c, a);
  }
  ring(cx, cy, r, c, a = 1, t = 1) {
    for (let y = Math.floor(cy - r - t); y <= cy + r + t; y++) for (let x = Math.floor(cx - r - t); x <= cx + r + t; x++) {
      const d = Math.hypot(x - cx, y - cy);
      if (d <= r && d >= r - t) this.px(x, y, c, a);
    }
  }
  rect(x, y, w, h, c, a = 1) {
    for (let j = 0; j < h; j++) for (let i = 0; i < w; i++) this.px(x + i, y + j, c, a);
  }
  // Even-odd polygon fill; `hatch` > 0 fills only diagonal stripes of that period.
  polygon(pts, c, a = 1, hatch = 0) {
    let minY = Infinity, maxY = -Infinity;
    for (const p of pts) { minY = Math.min(minY, p[1]); maxY = Math.max(maxY, p[1]); }
    minY = Math.max(0, Math.floor(minY)); maxY = Math.min(this.h - 1, Math.ceil(maxY));
    for (let y = minY; y <= maxY; y++) {
      const xs = [];
      for (let i = 0; i < pts.length; i++) {
        const [ax, ay] = pts[i], [bx, by] = pts[(i + 1) % pts.length];
        if ((ay <= y + 0.5 && by > y + 0.5) || (by <= y + 0.5 && ay > y + 0.5)) xs.push(ax + ((y + 0.5 - ay) / (by - ay)) * (bx - ax));
      }
      xs.sort((p, q) => p - q);
      for (let i = 0; i + 1 < xs.length; i += 2) {
        for (let x = Math.max(0, Math.ceil(xs[i])); x <= Math.min(this.w - 1, Math.floor(xs[i + 1])); x++) {
          if (hatch && (x + y) % hatch > 1) continue;
          this.px(x, y, c, a);
        }
      }
    }
  }
  outline(pts, c, a = 1, dash = 0, wid = 1) {
    let acc = 0;
    for (let i = 0; i < pts.length; i++) {
      const [ax, ay] = pts[i], [bx, by] = pts[(i + 1) % pts.length];
      const n = Math.max(Math.abs(bx - ax), Math.abs(by - ay), 1);
      for (let k = 0; k < n; k++, acc++) {
        if (dash && acc % (dash * 2) >= dash) continue;
        this.line(ax + ((bx - ax) * k) / n, ay + ((by - ay) * k) / n, ax + ((bx - ax) * (k + 1)) / n, ay + ((by - ay) * (k + 1)) / n, c, a, wid);
      }
    }
  }
  textWidth(s, scale = 1) { return s.length * 6 * scale - scale; }
  text(s, x, y, c, scale = 1, halo = null, a = 1) {
    const str = String(s).toUpperCase();
    for (const pass of halo ? [true, false] : [false]) {
      let cx = x;
      for (const chr of str) {
        const g = glyph.get(chr) || glyph.get("?");
        for (let r = 0; r < 7; r++) for (let col = 0; col < 5; col++) {
          if (!((g[r] >> (4 - col)) & 1)) continue;
          if (pass) {
            for (let dy = -1; dy <= 1; dy++) for (let dx = -1; dx <= 1; dx++) this.rect(cx + col * scale + dx, y + r * scale + dy, scale, scale, halo, a * 0.85);
          } else this.rect(cx + col * scale, y + r * scale, scale, scale, c, a);
        }
        cx += 6 * scale;
      }
    }
  }
  image() { return { w: this.w, h: this.h, ch: 3, data: this.data }; }
}
