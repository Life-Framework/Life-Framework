/**
 * Image helpers for `wb_preview`: BMP decode, box downscale, PNG encode,
 * luminance spread (a "is this picture flat?" check) and a stable-file wait.
 *
 * Mirrors the BMP logic of agent-box/bmp.mjs (24/32-bit BI_RGB, bottom-up or
 * top-down) but keeps the RGB buffer so the result can be downscaled and
 * measured before it is encoded. Pure Node, no dependencies.
 */

import { statSync } from "node:fs";
import { deflateSync } from "node:zlib";

export interface Rgb {
  width: number;
  height: number;
  /** width * height * 3 bytes, R G B, top-down. */
  pixels: Buffer;
}

const PNG_SIGNATURE = Buffer.from([137, 80, 78, 71, 13, 10, 26, 10]);

export function isPng(buf: Buffer): boolean {
  return buf.length >= 8 && buf.subarray(0, 8).equals(PNG_SIGNATURE);
}

export function isBmp(buf: Buffer): boolean {
  return buf.length >= 54 && buf.toString("ascii", 0, 2) === "BM";
}

// ── BMP decode ───────────────────────────────────────────────────────────────

export function decodeBmp(buf: Buffer): Rgb {
  if (!isBmp(buf)) throw new Error("not a bmp");
  const offset = buf.readUInt32LE(10);
  const width = buf.readInt32LE(18);
  const heightRaw = buf.readInt32LE(22);
  const bpp = buf.readUInt16LE(28);
  const compression = buf.readUInt32LE(30);
  if (compression !== 0) throw new Error(`compressed bmp (${compression})`);
  if (bpp !== 24 && bpp !== 32) throw new Error(`unsupported bmp bpp ${bpp}`);
  if (width <= 0 || heightRaw === 0) throw new Error("empty bmp");

  const bottomUp = heightRaw > 0;
  const height = Math.abs(heightRaw);
  const bytesPerPixel = bpp / 8;
  const rowStride = Math.ceil((width * bytesPerPixel) / 4) * 4;
  if (offset + rowStride * height > buf.length) throw new Error("truncated bmp");

  const pixels = Buffer.alloc(width * height * 3);
  for (let y = 0; y < height; y++) {
    const srcY = bottomUp ? height - 1 - y : y;
    const row = offset + srcY * rowStride;
    for (let x = 0; x < width; x++) {
      const i = row + x * bytesPerPixel;
      const o = (y * width + x) * 3;
      pixels[o] = buf[i + 2];
      pixels[o + 1] = buf[i + 1];
      pixels[o + 2] = buf[i];
    }
  }
  return { width, height, pixels };
}

// ── Downscale ────────────────────────────────────────────────────────────────

/** Box-filter downscale so the longer side is at most `maxPx`. No-op when it already fits. */
export function downscale(img: Rgb, maxPx: number): Rgb {
  const longest = Math.max(img.width, img.height);
  if (!(maxPx > 0) || longest <= maxPx) return img;
  const scale = maxPx / longest;
  const w = Math.max(1, Math.round(img.width * scale));
  const h = Math.max(1, Math.round(img.height * scale));
  const out = Buffer.alloc(w * h * 3);
  for (let y = 0; y < h; y++) {
    const y0 = Math.floor((y * img.height) / h);
    const y1 = Math.max(y0 + 1, Math.floor(((y + 1) * img.height) / h));
    for (let x = 0; x < w; x++) {
      const x0 = Math.floor((x * img.width) / w);
      const x1 = Math.max(x0 + 1, Math.floor(((x + 1) * img.width) / w));
      let r = 0;
      let g = 0;
      let b = 0;
      let n = 0;
      for (let yy = y0; yy < y1; yy++) {
        for (let xx = x0; xx < x1; xx++) {
          const i = (yy * img.width + xx) * 3;
          r += img.pixels[i];
          g += img.pixels[i + 1];
          b += img.pixels[i + 2];
          n++;
        }
      }
      const o = (y * w + x) * 3;
      out[o] = Math.round(r / n);
      out[o + 1] = Math.round(g / n);
      out[o + 2] = Math.round(b / n);
    }
  }
  return { width: w, height: h, pixels: out };
}

// ── PNG encode ───────────────────────────────────────────────────────────────

const CRC_TABLE = (() => {
  const table = new Uint32Array(256);
  for (let n = 0; n < 256; n++) {
    let c = n;
    for (let k = 0; k < 8; k++) c = c & 1 ? 0xedb88320 ^ (c >>> 1) : c >>> 1;
    table[n] = c >>> 0;
  }
  return table;
})();

function crc32(buf: Buffer): number {
  let c = 0xffffffff;
  for (let i = 0; i < buf.length; i++) c = CRC_TABLE[(c ^ buf[i]) & 0xff] ^ (c >>> 8);
  return (c ^ 0xffffffff) >>> 0;
}

function chunk(type: string, data: Buffer): Buffer {
  const len = Buffer.alloc(4);
  len.writeUInt32BE(data.length, 0);
  const typeBuf = Buffer.from(type, "ascii");
  const crc = Buffer.alloc(4);
  crc.writeUInt32BE(crc32(Buffer.concat([typeBuf, data])), 0);
  return Buffer.concat([len, typeBuf, data, crc]);
}

export function encodePng(img: Rgb): Buffer {
  const stride = img.width * 3;
  const raw = Buffer.alloc((stride + 1) * img.height);
  for (let y = 0; y < img.height; y++) {
    const dest = y * (stride + 1);
    raw[dest] = 0;
    img.pixels.copy(raw, dest + 1, y * stride, (y + 1) * stride);
  }
  const ihdr = Buffer.alloc(13);
  ihdr.writeUInt32BE(img.width, 0);
  ihdr.writeUInt32BE(img.height, 4);
  ihdr[8] = 8; // bit depth
  ihdr[9] = 2; // RGB
  return Buffer.concat([
    PNG_SIGNATURE,
    chunk("IHDR", ihdr),
    chunk("IDAT", deflateSync(raw)),
    chunk("IEND", Buffer.alloc(0)),
  ]);
}

// ── Flatness ─────────────────────────────────────────────────────────────────

/** Standard deviation of Rec.601 luminance, 0..255. A flat (single-colour) picture is ~0. */
export function lumaStdDev(img: Rgb): number {
  const n = img.width * img.height;
  if (n === 0) return 0;
  let sum = 0;
  let sumSq = 0;
  for (let i = 0; i < n; i++) {
    const l = 0.299 * img.pixels[i * 3] + 0.587 * img.pixels[i * 3 + 1] + 0.114 * img.pixels[i * 3 + 2];
    sum += l;
    sumSq += l * l;
  }
  const mean = sum / n;
  return Math.sqrt(Math.max(0, sumSq / n - mean * mean));
}

// ── Stable file wait ─────────────────────────────────────────────────────────

const sleep = (ms: number) => new Promise<void>((r) => setTimeout(r, ms));

/**
 * Poll until `locate()` returns a file whose size is > 0 and unchanged across
 * two reads `stableMs` apart. Returns the path, or null at `timeoutMs`.
 * `locate` is re-run every round because the render thread may add its own
 * extension after the handler returned.
 */
export async function waitForStableFile(
  locate: () => string | null,
  timeoutMs: number,
  stableMs = 150,
): Promise<string | null> {
  const deadline = Date.now() + timeoutMs;
  let lastPath: string | null = null;
  let lastSize = -1;
  while (Date.now() < deadline) {
    const p = locate();
    let size = -1;
    if (p) {
      try {
        size = statSync(p).size;
      } catch {
        size = -1;
      }
    }
    if (p && size > 0 && p === lastPath && size === lastSize) return p;
    lastPath = p;
    lastSize = size;
    await sleep(stableMs);
  }
  return null;
}
