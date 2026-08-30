/**
 * `edds.ts` — parse and rebuild Enfusion `.edds` texture files.
 *
 * An `.edds` is a DDS container with an Enfusion-specific twist:
 *   - bytes 0..3    "DDS " magic
 *   - bytes 4..127  DDS_HEADER (124 bytes)
 *   - bytes 128..147 DDS_HEADER_DXT10 (20 bytes; dxgiFormat at +0)
 *   - bytes 148..   Enfusion "COPY" block table: one `COPY` + u32LE size
 *     entry per mip, sizes in ASCENDING order (smallest mip first). The
 *     highest mip (full resolution, largest size) is last.
 *   - after the table: the mip data, in the same ascending order.
 *
 * The BC7 pixel payload is standard — the engine ships AMD Compressonator
 * (`CMP_DecodeBC7Block` / `CMP_EncodeBC7Block`), which is what this module
 * pairs with in `compressonator.ts`.
 *
 * This module is pure byte parsing/serialization — no FFI, no Workbench. The
 * recolor pipeline is: parse → decode each mip block → transform RGBA →
 * encode each block → rebuild `.edds`.
 */

// ── Types ────────────────────────────────────────────────────────────────────

/** Parsed structure of an `.edds` file. */
export interface EddsInfo {
  /** 2D texture width (pixels). */
  width: number;
  /** 2D texture height (pixels). */
  height: number;
  /** Number of mip levels. */
  mipCount: number;
  /** DXGI format code (e.g. 98 = BC7_UNORM, 99 = BC7_UNORM_SRGB). */
  dxgiFormat: number;
  /**
   * The header + COPY table bytes verbatim (everything before the mip
   * payload). Pass this unchanged to `buildEdds` so the rebuilt file keeps
   * the source's Enfusion metadata exactly.
   */
  prefix: Buffer;
  /**
   * Per-mip block payload sizes in file order (ascending, smallest mip
   * first). Sum equals the payload byte length.
   */
  mipSizes: number[];
}

// ── Constants ────────────────────────────────────────────────────────────────

const MAGIC_DDS = 0x20534444; // "DDS "
const FOURCC_DX10 = "DX10";
const COPY_TABLE_OFFSET = 148; // 4 (magic) + 124 (DDS_HEADER) + 20 (DXT10)

/** dxgiFormat values this tool can recolor (BC7 family). */
export const BC7_FORMATS = new Set<number>([98, 99]); // BC7_UNORM, BC7_UNORM_SRGB

/** Human-readable label for a dxgiFormat code. */
export function dxgiFormatLabel(fmt: number): string {
  switch (fmt) {
    case 98:
      return "BC7_UNORM";
    case 99:
      return "BC7_UNORM_SRGB";
    case 95:
      return "BC6H_UF16";
    case 96:
      return "BC6H_SF16";
    case 71:
      return "BC1_UNORM";
    case 72:
      return "BC1_UNORM_SRGB";
    case 74:
      return "BC3_UNORM";
    case 75:
      return "BC3_UNORM_SRGB";
    case 80:
      return "BC5_UNORM";
    case 81:
      return "BC5_UNORM_SRGB";
    default:
      return `DXGI_${fmt}`;
  }
}

// ── Parsing ──────────────────────────────────────────────────────────────────

/**
 * Parse an `.edds` buffer into its structural parts. Throws a descriptive
 * error when the file is not a supported DDS/BC7 texture.
 */
export function parseEdds(buf: Buffer): EddsInfo {
  if (buf.length < COPY_TABLE_OFFSET) {
    throw new Error(`File too small to be an .edds texture (${buf.length} bytes)`);
  }
  if (buf.readUInt32LE(0) !== MAGIC_DDS) {
    throw new Error("Not a DDS file: missing 'DDS ' magic");
  }

  const width = buf.readUInt32LE(16);
  const height = buf.readUInt32LE(12);
  const mipCount = buf.readUInt32LE(28);

  const fourCC = buf.toString("ascii", 84, 88);
  if (fourCC !== FOURCC_DX10) {
    throw new Error(`Unsupported DDS pixel format fourCC "${fourCC}" — expected "DX10"`);
  }
  const dxgiFormat = buf.readUInt32LE(128);
  if (!BC7_FORMATS.has(dxgiFormat)) {
    throw new Error(
      `Unsupported dxgiFormat ${dxgiFormat} (${dxgiFormatLabel(dxgiFormat)}) — ` +
        "only BC7 (98 UNORM / 99 UNORM_SRGB) can be recolored by this tool",
    );
  }
  if (mipCount < 1 || mipCount > 32) {
    throw new Error(`Implausible mip count ${mipCount}`);
  }

  // COPY block table: mipCount entries, each "COPY" + u32LE size.
  const mipSizes: number[] = [];
  for (let i = 0; i < mipCount; i++) {
    const off = COPY_TABLE_OFFSET + i * 8;
    const tag = buf.toString("ascii", off, off + 4);
    if (tag !== "COPY") {
      throw new Error(`Bad COPY table at mip ${i}: expected "COPY" at ${off}, got "${tag}"`);
    }
    const size = buf.readUInt32LE(off + 4);
    if (size < 16 || size > 1 << 28) {
      throw new Error(`Implausible mip ${i} size ${size}`);
    }
    mipSizes.push(size);
  }

  const prefixLen = COPY_TABLE_OFFSET + mipCount * 8;
  const payloadLen = mipSizes.reduce((a, b) => a + b, 0);
  if (prefixLen + payloadLen > buf.length) {
    throw new Error(
      `Declared mip payload (${payloadLen} bytes) overruns file (${buf.length} bytes)`,
    );
  }

  return {
    width,
    height,
    mipCount,
    dxgiFormat,
    prefix: buf.subarray(0, prefixLen),
    mipSizes,
  };
}

/**
 * Return each mip's payload bytes in file order (ascending mip size).
 * @param buf  the full `.edds` buffer (as parsed by `parseEdds`)
 */
export function extractMipPayloads(buf: Buffer, info: EddsInfo): Buffer[] {
  const out: Buffer[] = [];
  let off = COPY_TABLE_OFFSET + info.mipCount * 8;
  for (const size of info.mipSizes) {
    out.push(buf.subarray(off, off + size));
    off += size;
  }
  return out;
}

/**
 * Rebuild an `.edds` buffer from a parsed prefix + per-mip payloads.
 * @param info   parsed structure (prefix is preserved verbatim)
 * @param mips   per-mip payload bytes in the same ascending order as
 *               `info.mipSizes`
 */
export function buildEdds(info: EddsInfo, mips: Buffer[]): Buffer {
  if (mips.length !== info.mipSizes.length) {
    throw new Error(
      `Mip count mismatch: have ${mips.length} payloads, header declares ${info.mipSizes.length}`,
    );
  }
  for (let i = 0; i < mips.length; i++) {
    if (mips[i].length !== info.mipSizes[i]) {
      throw new Error(
        `Mip ${i} payload is ${mips[i].length} bytes, table declares ${info.mipSizes[i]}`,
      );
    }
  }
  return Buffer.concat([info.prefix, ...mips]);
}

// ── Block geometry ───────────────────────────────────────────────────────────

/**
 * Dimensions of a given mip (mip 0 = full resolution).
 * Standard halving with a floor of 1.
 */
export function mipDimensions(info: EddsInfo, mip: number): { w: number; h: number } {
  return {
    w: Math.max(1, info.width >> mip),
    h: Math.max(1, info.height >> mip),
  };
}

/**
 * Number of 4×4 BC7 blocks in a mip.
 */
export function mipBlockCount(info: EddsInfo, mip: number): number {
  const { w, h } = mipDimensions(info, mip);
  return Math.ceil(w / 4) * Math.ceil(h / 4);
}

/** Bytes per BC7 block (16 bytes = one 4×4 block). */
export const BC7_BLOCK_BYTES = 16;