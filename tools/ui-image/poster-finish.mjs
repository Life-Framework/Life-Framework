// The poster finish (#3306): a transparent 512 x 724 overlay for the `poster` sign style, drawn over the paper
// ground: yellowed worn edges, a fold crease, a few stains and masking tape at the four corners. Deterministic,
// no browser: pixels are computed here and sharp writes the PNG.
//
//   node tools/ui-image/poster-finish.mjs tmp/poster-finish.png
//   tools\cli ui-image tmp/poster-finish.png LF_SignPosterFinish
import { createRequire } from "node:module";
import { join } from "node:path";

const W = 512;
const H = 724;

function hash(x, y, seed) {
  let h = (x * 374761393 + y * 668265263 + seed * 2147483647) | 0;
  h = Math.imul(h ^ (h >>> 13), 1274126177);
  return ((h ^ (h >>> 16)) >>> 0) / 4294967295;
}

function smooth(t) {
  return t * t * (3 - 2 * t);
}

// Value noise at a cell size, 0..1.
function noise(x, y, cell, seed) {
  const gx = x / cell;
  const gy = y / cell;
  const x0 = Math.floor(gx);
  const y0 = Math.floor(gy);
  const fx = smooth(gx - x0);
  const fy = smooth(gy - y0);
  const a = hash(x0, y0, seed);
  const b = hash(x0 + 1, y0, seed);
  const c = hash(x0, y0 + 1, seed);
  const d = hash(x0 + 1, y0 + 1, seed);
  return (a * (1 - fx) + b * fx) * (1 - fy) + (c * (1 - fx) + d * fx) * fy;
}

// Source-over of colour (r g b, 0..255) at alpha a onto a straight-alpha pixel.
function over(px, i, r, g, b, a) {
  const outA = a + px[i + 3] / 255 * (1 - a);
  if (outA <= 0) return;
  const k = px[i + 3] / 255 * (1 - a);
  px[i] = (r * a + px[i] * k) / outA;
  px[i + 1] = (g * a + px[i + 1] * k) / outA;
  px[i + 2] = (b * a + px[i + 2] * k) / outA;
  px[i + 3] = outA * 255;
}

// A strip of masking tape: centre, half length and half width (px), turn in degrees.
function tape(px, cx, cy, half, thick, deg, seed) {
  const rad = (deg * Math.PI) / 180;
  const cos = Math.cos(rad);
  const sin = Math.sin(rad);
  const reach = Math.ceil(half + thick + 4);
  for (let y = Math.max(0, Math.floor(cy) - reach); y < Math.min(H, Math.floor(cy) + reach); y++) {
    for (let x = Math.max(0, Math.floor(cx) - reach); x < Math.min(W, Math.floor(cx) + reach); x++) {
      const dx = x - cx;
      const dy = y - cy;
      const u = dx * cos + dy * sin;
      const v = -dx * sin + dy * cos;
      // Torn, uneven ends.
      const ragged = (noise(x, y, 3, seed) - 0.5) * 7;
      if (Math.abs(u) > half + ragged || Math.abs(v) > thick) continue;
      const edge = Math.min(half - Math.abs(u), thick - Math.abs(v));
      const fibre = 0.82 + 0.18 * noise(x, y, 2, seed + 5);
      let a = 0.78 * fibre;
      if (edge < 2) a *= 0.6 + 0.2 * edge;
      over(px, (y * W + x) * 4, 226 * fibre, 214 * fibre, 160 * fibre, a);
      // A faint shadow line along the long edges, where the tape lifts.
      if (Math.abs(v) > thick - 2) over(px, (y * W + x) * 4, 120, 105, 70, 0.18);
    }
  }
}

function render() {
  const px = new Float32Array(W * H * 4);
  for (let y = 0; y < H; y++) {
    for (let x = 0; x < W; x++) {
      const i = (y * W + x) * 4;
      const d = Math.min(x, y, W - 1 - x, H - 1 - y);
      // Worn, yellowed edge: wide where the noise is high, so it never runs in a straight line.
      const wobble = noise(x, y, 12, 7) * 11 + noise(x, y, 4, 9) * 4;
      const edge = Math.max(0, Math.min(1, (wobble + 3 - d) / 12));
      if (edge > 0) over(px, i, 150, 128, 88, edge * 0.42);
      // Fray: a few specks at the very rim are darker.
      if (d < 3 && hash(x, y, 3) < 0.3) over(px, i, 110, 92, 62, 0.35);
      // Fold creases: one across, one down, faint.
      const crease = Math.min(Math.abs(y - H / 2 - noise(x, 0, 60, 11) * 6) , 99);
      if (crease < 1.6) over(px, i, 110, 96, 70, 0.2 * (1 - crease / 1.6));
      const crease2 = Math.abs(x - W / 2 - noise(0, y, 60, 13) * 6);
      if (crease2 < 1.3) over(px, i, 110, 96, 70, 0.12 * (1 - crease2 / 1.3));
      // Stains: soft low-frequency blotches.
      const stain = noise(x, y, 70, 21) * 0.6 + noise(x, y, 24, 22) * 0.4;
      if (stain > 0.66) over(px, i, 150, 126, 84, Math.min(0.07, (stain - 0.66) * 0.4));
    }
  }
  tape(px, 44, 40, 62, 17, -38, 31);
  tape(px, W - 44, 40, 62, 17, 38, 32);
  tape(px, 44, H - 40, 62, 17, 38, 33);
  tape(px, W - 44, H - 40, 62, 17, -38, 34);

  const out = Buffer.alloc(W * H * 4);
  for (let i = 0; i < px.length; i++) out[i] = Math.max(0, Math.min(255, Math.round(px[i])));
  return out;
}

const target = process.argv[2];
if (!target) {
  console.error("usage: node tools/ui-image/poster-finish.mjs <out.png>");
  process.exit(2);
}
const sharp = createRequire(join(process.cwd(), "website", "package.json"))("sharp");
await sharp(render(), { raw: { width: W, height: H, channels: 4 } }).png().toFile(target);
console.log(`poster finish ${W}x${H} -> ${target}`);
