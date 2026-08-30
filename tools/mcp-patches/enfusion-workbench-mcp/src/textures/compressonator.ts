/**
 * `compressonator.ts` — thin FFI wrapper around AMD Compressonator, the BC7
 * codec the engine itself ships (ArmaReforgerWorkbench/Compressonator_MD_DLL.dll).
 *
 * We use the engine's own codec for BC7 decode *and* encode so the recolored
 * texture survives the round trip byte-faithfully — no hand-rolled BC7 bit
 * packing that could silently corrupt pixels.
 *
 * Exposed surface is deliberately small:
 *   - `decodeBC7Block(block16)` → 64 doubles (16 pixels × RGBA, 0–255)
 *   - `encodeBC7Block(rgba64)`   → 16-byte BC7 block
 *   - `encodeMip0Payload(info)`  → full-mip encode of a raw RGBA buffer
 *
 * The library must be initialized once (`init()`) and an encoder allocated
 * before any encode call (`createEncoder()`); destroy on shutdown. The DLL
 * path is resolved from the configured Workbench install.
 */

import koffi from "koffi";
import { existsSync } from "node:fs";
import { join } from "node:path";
import { BC7_BLOCK_BYTES, type EddsInfo } from "./edds.js";

// ── Types ────────────────────────────────────────────────────────────────────

/** One decoded BC7 block: 16 pixels × 4 components (RGBA), values 0–255. */
export type RgbaBlock = Float64Array;

/** One BC7 compressed block: 16 bytes. */
export type Bc7Block = Buffer;

// ── Constants ────────────────────────────────────────────────────────────────

const BC_ERROR_NONE = 0;

/**
 * The DLL name the engine ships next to ArmaReforgerWorkbenchSteamDiag.exe.
 */
const COMPRESSONATOR_DLL = "Compressonator_MD_DLL.dll";

// ── FFI wrapper ──────────────────────────────────────────────────────────────

export class Compressonator {
  private readonly lib: ReturnType<typeof koffi.load>;
  private readonly decodeFn: (block: Buffer, out: Buffer) => number;
  private readonly createEncoderFn: (
    quality: number,
    restrictColour: number,
    restrictAlpha: number,
    modeMask: number,
    performance: number,
    outPtr: Buffer,
  ) => number;
  private readonly encodeFn: (encoder: bigint, rgba: Buffer, out: Buffer) => number;
  private readonly destroyEncoderFn: (encoder: bigint) => number;
  private readonly shutdownFn: () => number;

  private initialized = false;
  private encoder: bigint | null = null;

  /**
   * Locate the Compressonator DLL under a Workbench install root.
   * @param workbenchPath  "Arma Reforger Tools" directory (or its Workbench subdir)
   */
  static findDll(workbenchPath: string): string | null {
    const candidates = [
      join(workbenchPath, "Workbench", COMPRESSONATOR_DLL),
      join(workbenchPath, COMPRESSONATOR_DLL),
    ];
    for (const c of candidates) {
      if (existsSync(c)) return c;
    }
    return null;
  }

  constructor(dllPath: string) {
    this.lib = koffi.load(dllPath);

    const u8 = koffi.pointer("uint8");
    const f64 = koffi.pointer("double");
    const voidPtr = koffi.pointer("void");

    this.decodeFn = this.lib.func("CMP_DecodeBC7Block", "int", [u8, f64]);
    this.createEncoderFn = this.lib.func("CMP_CreateBC7Encoder", "int", [
      "double",
      "int",
      "int",
      "uint32",
      "double",
      voidPtr,
    ]);
    this.encodeFn = this.lib.func("CMP_EncodeBC7Block", "int", [voidPtr, f64, u8]);
    this.destroyEncoderFn = this.lib.func("CMP_DestroyBC7Encoder", "int", [voidPtr]);
    this.shutdownFn = this.lib.func("CMP_ShutdownBCLibrary", "int", []);
  }

  /** Initialize the BC library. Must be called before any other method. */
  init(): void {
    if (this.initialized) return;
    const initFn = this.lib.func("CMP_InitializeBCLibrary", "int", []);
    const ret = initFn();
    if (ret !== BC_ERROR_NONE) {
      throw new Error(`CMP_InitializeBCLibrary failed with code ${ret}`);
    }
    this.initialized = true;
  }

  /**
   * Create a reusable BC7 encoder. Quality 0–1 (higher = slower, better).
   * Call once, reuse across all blocks, destroy via `shutdown()`.
   */
  createEncoder(quality = 0.5): void {
    if (!this.initialized) this.init();
    const outPtr = Buffer.alloc(8);
    const ret = this.createEncoderFn(
      quality,
      0, // restrictColour
      0, // restrictAlpha
      0xff, // modeMask — allow all modes for fidelity
      1.0, // performance
      outPtr,
    );
    if (ret !== BC_ERROR_NONE) {
      throw new Error(`CMP_CreateBC7Encoder failed with code ${ret}`);
    }
    this.encoder = outPtr.readBigUInt64LE(0);
  }

  /** Decode one 16-byte BC7 block into 64 RGBA doubles (16 px × 4). */
  decodeBlock(block: Bc7Block): RgbaBlock {
    if (!this.initialized) this.init();
    if (block.length !== BC7_BLOCK_BYTES) {
      throw new Error(`BC7 block must be ${BC7_BLOCK_BYTES} bytes, got ${block.length}`);
    }
    const out = Buffer.alloc(64 * 8);
    const ret = this.decodeFn(block, out);
    if (ret !== BC_ERROR_NONE) {
      throw new Error(`CMP_DecodeBC7Block failed with code ${ret}`);
    }
    return new Float64Array(out.buffer, out.byteOffset, 64);
  }

  /** Encode 64 RGBA doubles (16 px × 4) into a 16-byte BC7 block. */
  encodeBlock(rgba: RgbaBlock): Bc7Block {
    if (!this.initialized) this.init();
    if (!this.encoder) this.createEncoder();
    const encoder = this.encoder!;
    if (rgba.length !== 64) {
      throw new Error(`RGBA block must have 64 doubles, got ${rgba.length}`);
    }
    const inBuf = Buffer.from(rgba.buffer, rgba.byteOffset, 64 * 8);
    const out = Buffer.alloc(BC7_BLOCK_BYTES);
    const ret = this.encodeFn(encoder, inBuf, out);
    if (ret !== BC_ERROR_NONE) {
      throw new Error(`CMP_EncodeBC7Block failed with code ${ret}`);
    }
    return out;
  }

  /**
   * Encode a full-resolution RGBA image (width×height, 4 bytes/px, 0–255)
   * into the BC7 payload for mip 0 of the given texture. Returns raw bytes
   * of exactly `mipBlockCount(info, 0) * 16`.
   */
  encodeMip0Rgba(info: EddsInfo, rgba: Buffer): Buffer {
    if (!this.initialized) this.init();
    if (!this.encoder) this.createEncoder();
    const encoder = this.encoder!;
    if (rgba.length !== info.width * info.height * 4) {
      throw new Error(
        `RGBA buffer is ${rgba.length} bytes, expected ${info.width * info.height * 4} ` +
          `(${info.width}×${info.height}×4)`,
      );
    }

    const blockCount = Math.ceil(info.width / 4) * Math.ceil(info.height / 4);
    const out = Buffer.alloc(blockCount * BC7_BLOCK_BYTES);
    const inBuf = Buffer.alloc(64 * 8);

    for (let by = 0; by < Math.ceil(info.height / 4); by++) {
      for (let bx = 0; bx < Math.ceil(info.width / 4); bx++) {
        for (let py = 0; py < 4; py++) {
          for (let px = 0; px < 4; px++) {
            const sx = bx * 4 + px;
            const sy = by * 4 + py;
            const src = (sy * info.width + sx) * 4;
            const dst = ((py * 4 + px) * 4) * 8;
            if (sx >= info.width || sy >= info.height) {
              inBuf.writeDoubleLE(0, dst);
              inBuf.writeDoubleLE(0, dst + 8);
              inBuf.writeDoubleLE(0, dst + 16);
              inBuf.writeDoubleLE(255, dst + 24);
              continue;
            }
            inBuf.writeDoubleLE(rgba[src], dst);
            inBuf.writeDoubleLE(rgba[src + 1], dst + 8);
            inBuf.writeDoubleLE(rgba[src + 2], dst + 16);
            inBuf.writeDoubleLE(rgba[src + 3], dst + 24);
          }
        }
        const ret = this.encodeFn(encoder, inBuf, out.subarray((by * Math.ceil(info.width / 4) + bx) * BC7_BLOCK_BYTES));
        if (ret !== BC_ERROR_NONE) {
          throw new Error(`CMP_EncodeBC7Block failed at block (${bx},${by}) with code ${ret}`);
        }
      }
    }
    return out;
  }

  /** Release the encoder and shut down the BC library. Safe to call twice. */
  shutdown(): void {
    if (this.encoder !== null) {
      try {
        this.destroyEncoderFn(this.encoder);
      } catch {
        // ignore teardown errors
      }
      this.encoder = null;
    }
    if (this.initialized) {
      try {
        this.shutdownFn();
      } catch {
        // ignore
      }
      this.initialized = false;
    }
  }
}