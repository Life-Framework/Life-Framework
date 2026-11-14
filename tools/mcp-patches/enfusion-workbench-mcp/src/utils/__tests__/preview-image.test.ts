import { describe, it, expect } from "vitest";
import { mkdtempSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { decodeBmp, decodePng, downscale, encodePng, isPng, lumaStdDev, waitForStableFile } from "../preview-image.js";

/** w x h 24-bit bottom-up BMP; `paint(x, y)` returns [r, g, b] with y=0 the top row. */
function makeBmp(w: number, h: number, paint: (x: number, y: number) => [number, number, number]): Buffer {
  const stride = Math.ceil((w * 3) / 4) * 4;
  const buf = Buffer.alloc(54 + stride * h);
  buf.write("BM", 0);
  buf.writeUInt32LE(buf.length, 2);
  buf.writeUInt32LE(54, 10);
  buf.writeUInt32LE(40, 14);
  buf.writeInt32LE(w, 18);
  buf.writeInt32LE(h, 22);
  buf.writeUInt16LE(1, 26);
  buf.writeUInt16LE(24, 28);
  for (let y = 0; y < h; y++) {
    for (let x = 0; x < w; x++) {
      const [r, g, b] = paint(x, y);
      const i = 54 + (h - 1 - y) * stride + x * 3;
      buf[i] = b;
      buf[i + 1] = g;
      buf[i + 2] = r;
    }
  }
  return buf;
}

describe("preview-image", () => {
  it("decodes a bottom-up BMP with y=0 at the top", () => {
    const img = decodeBmp(makeBmp(3, 2, (_x, y) => (y === 0 ? [255, 0, 0] : [0, 0, 255])));
    expect([img.pixels[0], img.pixels[1], img.pixels[2]]).toEqual([255, 0, 0]);
    expect(img.pixels[(1 * 3) * 3 + 2]).toBe(255);
  });

  it("measures a flat picture as ~0 and a gradient as clearly above", () => {
    const flat = decodeBmp(makeBmp(16, 16, () => [90, 90, 90]));
    const grad = decodeBmp(makeBmp(16, 16, (x) => [x * 16, x * 16, x * 16]));
    expect(lumaStdDev(flat)).toBeLessThan(0.5);
    expect(lumaStdDev(grad)).toBeGreaterThan(20);
  });

  it("downscales the longer side to maxPx and leaves smaller images alone", () => {
    const img = decodeBmp(makeBmp(40, 20, () => [10, 20, 30]));
    const small = downscale(img, 10);
    expect([small.width, small.height]).toEqual([10, 5]);
    expect([small.pixels[0], small.pixels[1], small.pixels[2]]).toEqual([10, 20, 30]);
    expect(downscale(img, 100)).toBe(img);
  });

  it("encodes a PNG with a valid signature", () => {
    const img = decodeBmp(makeBmp(4, 4, () => [1, 2, 3]));
    const png = encodePng(img);
    expect(isPng(png)).toBe(true);
    const back = decodePng(png);
    expect([back.width, back.height]).toEqual([4, 4]);
    expect([back.pixels[0], back.pixels[1], back.pixels[2]]).toEqual([1, 2, 3]);
  });

  it("waits for a size-stable file and gives up on a missing one", async () => {
    const dir = mkdtempSync(join(tmpdir(), "wb-preview-test-"));
    const f = join(dir, "a.bmp");
    writeFileSync(f, Buffer.alloc(10, 1));
    expect(await waitForStableFile(() => f, 2000, 20)).toBe(f);
    expect(await waitForStableFile(() => null, 120, 20)).toBeNull();
  });
});
