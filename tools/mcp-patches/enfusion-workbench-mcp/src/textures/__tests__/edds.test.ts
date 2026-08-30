import { describe, it, expect } from "vitest";
import {
  parseEdds,
  extractMipPayloads,
  buildEdds,
  mipBlockCount,
  mipDimensions,
  dxgiFormatLabel,
  BC7_BLOCK_BYTES,
} from "../edds.js";

/** Build a minimal synthetic .edds container: DDS header + DX10 + COPY table
 *  + fake BC7 payloads (contents don't matter for container round-trip). */
function buildSyntheticEdds(width: number, height: number, mipCount: number, dxgiFormat: number): Buffer {
  const buf = Buffer.alloc(4 + 124 + 20 + mipCount * 8);
  // DDS magic
  buf.write("DDS ", 0, "ascii");
  // DDS_HEADER
  buf.writeUInt32LE(124, 4); // dwSize
  buf.writeUInt32LE(0x000a1007, 8); // dwFlags
  buf.writeUInt32LE(height, 12); // dwHeight
  buf.writeUInt32LE(width, 16); // dwWidth
  buf.writeUInt32LE(0, 24); // dwDepth
  buf.writeUInt32LE(mipCount, 28); // dwMipMapCount
  // DDS_PIXELFORMAT → FOURCC "DX10"
  buf.writeUInt32LE(32, 76); // pfSize
  buf.writeUInt32LE(4, 80); // pfFlags = DDPF_FOURCC
  buf.write("DX10", 84, "ascii"); // pfFourCC
  // DDS_HEADER_DXT10 (starts at 128)
  buf.writeUInt32LE(dxgiFormat, 128);
  buf.writeUInt32LE(3, 132); // resourceDimension = DDS_DIMENSION_TEXTURE2D
  buf.writeUInt32LE(1, 140); // arraySize

  // COPY table (ascending mip sizes, smallest first — matches real .edds).
  // Slot i in the table holds the mip with the smallest remaining size, i.e.
  // the block count of mip (mipCount-1-i). Write entries + payloads so that
  // table slot 0 is the smallest mip and slot mipCount-1 is the largest.
  const tableOff = 4 + 124 + 20;
  const payloads: Buffer[] = [];
  for (let slot = 0; slot < mipCount; slot++) {
    const mip = mipCount - 1 - slot;
    const { w, h } = mipDimensions({ width, height, mipCount, dxgiFormat, prefix: Buffer.alloc(0), mipSizes: [] }, mip);
    const blocks = Math.ceil(w / 4) * Math.ceil(h / 4);
    const size = blocks * BC7_BLOCK_BYTES;
    buf.write("COPY", tableOff + slot * 8, "ascii");
    buf.writeUInt32LE(size, tableOff + slot * 8 + 4);
    payloads.push(Buffer.alloc(size, mip + 1)); // distinctive fake payload
  }
  return Buffer.concat([buf, ...payloads]);
}

describe("edds — parse container", () => {
  it("parses header + COPY table of a synthetic 256×256 BC7 texture", () => {
    const buf = buildSyntheticEdds(256, 256, 9, 98);
    const info = parseEdds(buf);
    expect(info.width).toBe(256);
    expect(info.height).toBe(256);
    expect(info.mipCount).toBe(9);
    expect(info.dxgiFormat).toBe(98);
    expect(info.mipSizes.length).toBe(9);
  });

  it("mip sizes are ascending (smallest first)", () => {
    const buf = buildSyntheticEdds(256, 256, 9, 98);
    const info = parseEdds(buf);
    const sorted = [...info.mipSizes].sort((a, b) => a - b);
    expect(info.mipSizes).toEqual(sorted);
    // smallest is 1x1 (1 block), largest is 256x256
    expect(info.mipSizes[0]).toBe(BC7_BLOCK_BYTES);
    expect(info.mipSizes[8]).toBe(64 * 64 * BC7_BLOCK_BYTES);
  });

  it("rejects non-DDS files", () => {
    expect(() => parseEdds(Buffer.from("not a dds file at all, definitely not long enough"))).toThrow();
  });

  it("rejects non-DX10 fourCC", () => {
    const buf = buildSyntheticEdds(8, 8, 1, 98);
    buf.write("DXT1", 84, "ascii");
    expect(() => parseEdds(buf)).toThrow(/DX10/);
  });

  it("rejects unsupported dxgi formats (BC5)", () => {
    const buf = buildSyntheticEdds(8, 8, 1, 80);
    expect(() => parseEdds(buf)).toThrow(/BC5|dxgiFormat/);
  });
});

describe("edds — extract + rebuild round-trip", () => {
  it("extracts payloads and rebuilds byte-identically", () => {
    const original = buildSyntheticEdds(256, 256, 9, 98);
    const info = parseEdds(original);
    const payloads = extractMipPayloads(original, info);
    const rebuilt = buildEdds(info, payloads);
    expect(rebuilt.equals(original)).toBe(true);
  });
});

describe("edds — block geometry", () => {
  it("computes block counts for 256×256", () => {
    const info = parseEdds(buildSyntheticEdds(256, 256, 9, 98));
    expect(mipBlockCount(info, 0)).toBe(64 * 64); // 256×256 / (4×4)
    expect(mipBlockCount(info, 1)).toBe(32 * 32); // 128×128
    expect(mipBlockCount(info, 8)).toBe(1); // 1×1
  });

  it("handles non-multiple-of-4 dims via ceil", () => {
    const info = parseEdds(buildSyntheticEdds(6, 6, 1, 98));
    expect(mipBlockCount(info, 0)).toBe(2 * 2); // ceil(6/4)=2
  });
});

describe("edds — format labels", () => {
  it("labels known formats", () => {
    expect(dxgiFormatLabel(98)).toBe("BC7_UNORM");
    expect(dxgiFormatLabel(99)).toBe("BC7_UNORM_SRGB");
    expect(dxgiFormatLabel(80)).toBe("BC5_UNORM");
  });
});