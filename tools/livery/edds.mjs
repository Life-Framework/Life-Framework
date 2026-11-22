// tools/livery/edds.mjs - unpack a game .edds (a DDS header, a per-mip chunk table, COPY or LZ4 chunks,
// smallest mip first) into a plain one-mip .dds that Blender's OpenImageIO opens. From the livery spike
// (#4226, tools/livery/spike/edds.mjs); the mask step of `tools\cli livery build --mask` uses it.
// An LZ4 chunk is: uint32 unpacked size, then blocks of [uint32 packed size, high bit = last][LZ4 block],
// each allowed to copy from everything unpacked before it (a linked block stream).

const BYTES_PER_PX = { 98: 1, 99: 1, 71: 0.5, 72: 0.5, 77: 1, 78: 1, 83: 1, 80: 0.5 };

export function lz4Chunk(src) {
  const total = src.readUInt32LE(0);
  const out = Buffer.alloc(total);
  let i = 4, o = 0;
  while (i < src.length && o < total) {
    const word = src.readUInt32LE(i);
    const end = i + 4 + (word & 0x7fffffff);
    i += 4;
    while (i < end && o < total) {
      const tok = src[i++];
      let n = tok >> 4;
      if (n === 15) { let b; do { b = src[i++]; n += b; } while (b === 255); }
      src.copy(out, o, i, i + n);
      i += n; o += n;
      if (i >= end) break;
      const off = src[i] | (src[i + 1] << 8);
      i += 2;
      let m = tok & 15;
      if (m === 15) { let b; do { b = src[i++]; m += b; } while (b === 255); }
      m += 4;
      for (let k = 0; k < m; k++, o++) out[o] = out[o - off];
    }
    i = end;
    if (word & 0x80000000) break;
  }
  return o === total ? out : null;
}

export function eddsToDds(b) {
  const h = b.readUInt32LE(12), w = b.readUInt32LE(16), mips = b.readUInt32LE(28);
  const dxgi = b.readUInt32LE(128);
  const bpp = BYTES_PER_PX[dxgi];
  if (!bpp) throw new Error(`dxgi format ${dxgi} not handled`);
  let o = 148;
  const table = [];
  for (let i = 0; i < mips; i++, o += 8) table.push({ type: b.toString("latin1", o, o + 4), size: b.readUInt32LE(o + 4) });
  // Data follows the table, smallest mip first, so the largest is the last chunk.
  const last = table[mips - 1];
  const raw = b.subarray(b.length - last.size);
  const want = Math.max(1, w / 4) * Math.max(1, h / 4) * 16 * bpp;
  const mip0 = last.type === "COPY" ? raw : lz4Chunk(raw);
  if (!mip0 || mip0.length !== want) throw new Error(`mip 0: ${last.type} chunk did not unpack to ${want} bytes`);
  const head = Buffer.from(b.subarray(0, 148));
  head.writeUInt32LE(1, 28); // one mip
  head.writeUInt32LE(head.readUInt32LE(8) & ~0x20000, 8); // drop DDSD_MIPMAPCOUNT
  return { dds: Buffer.concat([head, mip0]), w, h, dxgi };
}
