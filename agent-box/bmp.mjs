// Uncompressed 24/32-bit BMP to PNG. Reforger's MakeScreenshot writes a BMP;
// LF_ScreenshotCapture looks for the .png sibling. This converts the BMP when
// the png is not already on disk.

import { existsSync, readFileSync, writeFileSync } from "node:fs";
import { deflateSync, inflateSync } from "node:zlib";

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

function paeth(left, up, upLeft) {
  const estimate = left + up - upLeft;
  const dl = Math.abs(estimate - left);
  const du = Math.abs(estimate - up);
  const dlu = Math.abs(estimate - upLeft);
  if (dl <= du && dl <= dlu) return left;
  if (du <= dlu) return up;
  return upLeft;
}

export function decodePng(buf) {
  const signature = Buffer.from([137, 80, 78, 71, 13, 10, 26, 10]);
  if (buf.length < 8 || !buf.subarray(0, 8).equals(signature)) throw new Error("not a png");
  let offset = 8;
  let width = 0;
  let height = 0;
  let bitDepth = 0;
  let colorType = 0;
  let interlace = 0;
  const idat = [];
  while (offset + 8 <= buf.length) {
    const length = buf.readUInt32BE(offset);
    const type = buf.toString("ascii", offset + 4, offset + 8);
    const dataStart = offset + 8;
    const dataEnd = dataStart + length;
    if (dataEnd + 4 > buf.length) throw new Error("truncated png");
    const data = buf.subarray(dataStart, dataEnd);
    if (type === "IHDR") {
      width = data.readUInt32BE(0);
      height = data.readUInt32BE(4);
      bitDepth = data[8];
      colorType = data[9];
      interlace = data[12];
    } else if (type === "IDAT") {
      idat.push(data);
    } else if (type === "IEND") {
      break;
    }
    offset = dataEnd + 4;
  }
  if (width < 1 || height < 1) throw new Error("empty png");
  if (bitDepth !== 8 || interlace !== 0) throw new Error(`unsupported png ${bitDepth}/${interlace}`);
  let channels = 0;
  if (colorType === 0) channels = 1;
  else if (colorType === 2) channels = 3;
  else if (colorType === 6) channels = 4;
  else throw new Error(`unsupported png color ${colorType}`);

  const inflated = inflateSync(Buffer.concat(idat));
  const stride = width * channels;
  const pixels = Buffer.alloc(width * height * 3);
  let src = 0;
  let previous = Buffer.alloc(stride);
  for (let y = 0; y < height; y++) {
    if (src >= inflated.length) throw new Error("truncated png rows");
    const filter = inflated[src++];
    const row = Buffer.alloc(stride);
    for (let i = 0; i < stride; i++) {
      const value = inflated[src++];
      const left = i >= channels ? row[i - channels] : 0;
      const up = previous[i];
      const upLeft = i >= channels ? previous[i - channels] : 0;
      if (filter === 0) row[i] = value;
      else if (filter === 1) row[i] = (value + left) & 255;
      else if (filter === 2) row[i] = (value + up) & 255;
      else if (filter === 3) row[i] = (value + Math.floor((left + up) / 2)) & 255;
      else if (filter === 4) row[i] = (value + paeth(left, up, upLeft)) & 255;
      else throw new Error(`bad png filter ${filter}`);
    }
    for (let x = 0; x < width; x++) {
      const out = (y * width + x) * 3;
      if (channels === 1) {
        pixels[out] = row[x];
        pixels[out + 1] = row[x];
        pixels[out + 2] = row[x];
      } else {
        const i = x * channels;
        pixels[out] = row[i];
        pixels[out + 1] = row[i + 1];
        pixels[out + 2] = row[i + 2];
      }
    }
    previous = row;
  }
  return { width, height, pixels };
}

// Center of the frame. A placeholder material is a near-white blob there.
export function classifyMesh(decoded) {
  if (!decoded || decoded.width < 1 || decoded.height < 1 || !decoded.pixels) return "missing";
  const { width, height, pixels } = decoded;
  if (pixels.length < width * height * 3) return "missing";
  const x0 = Math.floor(width * 0.35);
  const x1 = Math.max(x0 + 1, Math.ceil(width * 0.65));
  const y0 = Math.floor(height * 0.35);
  const y1 = Math.max(y0 + 1, Math.ceil(height * 0.65));
  let total = 0;
  let white = 0;
  for (let y = y0; y < y1 && y < height; y++) {
    for (let x = x0; x < x1 && x < width; x++) {
      const o = (y * width + x) * 3;
      const r = pixels[o];
      const g = pixels[o + 1];
      const b = pixels[o + 2];
      total++;
      const max = Math.max(r, g, b);
      const min = Math.min(r, g, b);
      if (min >= 210 && max - min <= 18) white++;
    }
  }
  if (total === 0) return "missing";
  if (white / total >= 0.5) return "white";
  return "ok";
}

export function classifyMeshFile(file) {
  if (!file || !existsSync(file)) return "missing";
  try {
    const buf = readFileSync(file);
    if (buf.length >= 2 && buf.toString("ascii", 0, 2) === "BM") return classifyMesh(decodeBmp(buf));
    return classifyMesh(decodePng(buf));
  } catch {
    return "missing";
  }
}

// Pixel bytes of a capture, copied at the moment the file is read. A later
// shot must not be compared by path: the engine can reuse a file name.
export function capturePixels(file) {
  if (!file || !existsSync(file)) return null;
  try {
    const buf = readFileSync(file);
    if (buf.length >= 2 && buf.toString("ascii", 0, 2) === "BM") return decodeBmp(buf).pixels;
    return decodePng(buf).pixels;
  } catch {
    return null;
  }
}

export function samePixels(left, right) {
  if (!left || !right || left.length !== right.length) return false;
  return left.equals(right);
}

// ---- palette PNG under a size cap (tools\cli shots: website images stay under 500 KB) ----

// Median cut over a 5-bit-per-channel histogram: fast on a 1280x720 frame and good enough for a render.
export function quantize(width, height, rgb, colors) {
  const BITS = 5;
  const N = 1 << (3 * BITS);
  const count = new Uint32Array(N);
  const sum = new Float64Array(N * 3);
  const binOf = new Uint16Array(width * height);
  for (let i = 0, p = 0; i < width * height; i++, p += 3) {
    const r = rgb[p];
    const g = rgb[p + 1];
    const b = rgb[p + 2];
    const bin = ((r >> 3) << 10) | ((g >> 3) << 5) | (b >> 3);
    binOf[i] = bin;
    count[bin]++;
    sum[bin * 3] += r;
    sum[bin * 3 + 1] += g;
    sum[bin * 3 + 2] += b;
  }
  const used = [];
  for (let b = 0; b < N; b++) if (count[b]) used.push(b);
  const mean = (b, c) => sum[b * 3 + c] / count[b];
  const chan = (b, c) => (c === 0 ? (b >> 10) & 31 : c === 1 ? (b >> 5) & 31 : b & 31);
  let boxes = [used];
  while (boxes.length < colors) {
    // Split the box with the most pixels (and more than one colour) along its widest channel.
    let pick = -1;
    let best = 0;
    for (let i = 0; i < boxes.length; i++) {
      if (boxes[i].length < 2) continue;
      let pop = 0;
      for (const b of boxes[i]) pop += count[b];
      if (pop > best) {
        best = pop;
        pick = i;
      }
    }
    if (pick < 0) break;
    const box = boxes[pick];
    let axis = 0;
    let widest = -1;
    for (let c = 0; c < 3; c++) {
      let lo = 31;
      let hi = 0;
      for (const b of box) {
        const v = chan(b, c);
        if (v < lo) lo = v;
        if (v > hi) hi = v;
      }
      if (hi - lo > widest) {
        widest = hi - lo;
        axis = c;
      }
    }
    box.sort((a, b) => chan(a, axis) - chan(b, axis));
    let total = 0;
    for (const b of box) total += count[b];
    let acc = 0;
    let cut = 1;
    for (let i = 0; i < box.length - 1; i++) {
      acc += count[box[i]];
      cut = i + 1;
      if (acc >= total / 2) break;
    }
    boxes.splice(pick, 1, box.slice(0, cut), box.slice(cut));
  }
  const palette = Buffer.alloc(boxes.length * 3);
  boxes.forEach((box, i) => {
    let pop = 0;
    const acc = [0, 0, 0];
    for (const b of box) {
      pop += count[b];
      for (let c = 0; c < 3; c++) acc[c] += mean(b, c) * count[b];
    }
    for (let c = 0; c < 3; c++) palette[i * 3 + c] = Math.round(acc[c] / pop);
  });
  const toIndex = new Int16Array(N).fill(-1);
  for (const b of used) {
    const r = mean(b, 0);
    const g = mean(b, 1);
    const bl = mean(b, 2);
    let bestIdx = 0;
    let bestD = Infinity;
    for (let i = 0; i < boxes.length; i++) {
      const dr = r - palette[i * 3];
      const dg = g - palette[i * 3 + 1];
      const db = bl - palette[i * 3 + 2];
      const d = dr * dr * 0.3 + dg * dg * 0.59 + db * db * 0.11;
      if (d < bestD) {
        bestD = d;
        bestIdx = i;
      }
    }
    toIndex[b] = bestIdx;
  }
  const indices = new Uint8Array(width * height);
  for (let i = 0; i < indices.length; i++) indices[i] = toIndex[binOf[i]];
  return { palette, indices };
}

export function encodePngIndexed(width, height, palette, indices) {
  const raw = Buffer.alloc((width + 1) * height);
  for (let y = 0; y < height; y++) {
    const dest = y * (width + 1);
    raw[dest] = 0;
    Buffer.from(indices.buffer, indices.byteOffset + y * width, width).copy(raw, dest + 1);
  }
  const ihdr = Buffer.alloc(13);
  ihdr.writeUInt32BE(width, 0);
  ihdr.writeUInt32BE(height, 4);
  ihdr[8] = 8;
  ihdr[9] = 3;
  const signature = Buffer.from([137, 80, 78, 71, 13, 10, 26, 10]);
  return Buffer.concat([signature, chunk("IHDR", ihdr), chunk("PLTE", palette), chunk("IDAT", deflateSync(raw, { level: 9 })), chunk("IEND", Buffer.alloc(0))]);
}

// Full-colour PNG when it fits, else the most colours (256 down to 32) that still fit under maxKb.
export function pngUnder(width, height, rgb, maxKb) {
  const full = encodePng(width, height, rgb);
  if (full.length <= maxKb * 1024) return { png: full, colors: 0 };
  let last = null;
  for (const colors of [256, 192, 128, 96, 64, 48, 32]) {
    const q = quantize(width, height, rgb, colors);
    last = { png: encodePngIndexed(width, height, q.palette, q.indices), colors: q.palette.length / 3 };
    if (last.png.length <= maxKb * 1024) return last;
  }
  return last;
}
