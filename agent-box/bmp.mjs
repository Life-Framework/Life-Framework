// Uncompressed 24/32-bit BMP to PNG. Reforger's MakeScreenshot writes a BMP;
// EL_ScreenshotCapture looks for the .png sibling. This converts the BMP when
// the png is not already on disk.

import { readFileSync, writeFileSync } from "node:fs";
import { deflateSync } from "node:zlib";

function crc32(buf) {
  let c = ~0;
  for (let i = 0; i < buf.length; i++) {
    c ^= buf[i];
    for (let k = 0; k < 8; k++) c = (c >>> 1) ^ (0xedb88320 & -(c & 1));
  }
  return ~c >>> 0;
}

function chunk(type, data) {
  const body = Buffer.concat([Buffer.from(type), data]);
  const len = Buffer.alloc(4);
  len.writeUInt32BE(data.length);
  const crc = Buffer.alloc(4);
  crc.writeUInt32BE(crc32(body));
  return Buffer.concat([len, body, crc]);
}

export function decodeBmp(buf) {
  if (buf.length < 54 || buf.toString("ascii", 0, 2) !== "BM") {
    throw new Error("not a bmp");
  }
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

export function encodePng(width, height, rgb) {
  const stride = width * 3;
  const raw = Buffer.alloc((stride + 1) * height);
  for (let y = 0; y < height; y++) {
    const dest = y * (stride + 1);
    raw[dest] = 0;
    rgb.copy(raw, dest + 1, y * stride, (y + 1) * stride);
  }
  const ihdr = Buffer.alloc(13);
  ihdr.writeUInt32BE(width, 0);
  ihdr.writeUInt32BE(height, 4);
  ihdr[8] = 8;
  ihdr[9] = 2;
  const signature = Buffer.from([137, 80, 78, 71, 13, 10, 26, 10]);
  return Buffer.concat([
    signature,
    chunk("IHDR", ihdr),
    chunk("IDAT", deflateSync(raw)),
    chunk("IEND", Buffer.alloc(0)),
  ]);
}

export function bmpFileToPng(bmpPath, pngPath) {
  const decoded = decodeBmp(readFileSync(bmpPath));
  writeFileSync(pngPath, encodePng(decoded.width, decoded.height, decoded.pixels));
  return pngPath;
}

export function syntheticBmp() {
  const width = 2;
  const height = 2;
  const rowStride = 8;
  const pixelOffset = 54;
  const buf = Buffer.alloc(pixelOffset + rowStride * height);
  buf.write("BM", 0);
  buf.writeUInt32LE(buf.length, 2);
  buf.writeUInt32LE(pixelOffset, 10);
  buf.writeUInt32LE(40, 14);
  buf.writeInt32LE(width, 18);
  buf.writeInt32LE(height, 22);
  buf.writeUInt16LE(1, 26);
  buf.writeUInt16LE(24, 28);
  // Bottom row (file order) is blue, top row is red. After decode, y=0 is red.
  const paint = (row, b, g, r) => {
    for (let x = 0; x < width; x++) {
      const i = pixelOffset + row * rowStride + x * 3;
      buf[i] = b;
      buf[i + 1] = g;
      buf[i + 2] = r;
    }
  };
  paint(0, 255, 0, 0);
  paint(1, 0, 0, 255);
  return buf;
}
