// tools/map/lib/basemap.mjs - the Everon base layer, read once from the game paks into a cache outside the repo.
//
//   worlds/Eden/Eden/.Data/Eden_<n>_supertexture.edds   BC7 colour, 256x256 for a 256 m tile (1 m/px)
//   worlds/Eden/Eden/.Data/Eden_<n>.ttile               IFF TERR form, HGHT = 129x129 uint16 heights
//
// 2500 tiles in a 50x50 grid, n = row * 50 + col, columns run east, rows run (see ROWS_FROM) along z.
// The cache is ~/.life-framework/map-cache/everon-<game build>/ (terrain.png 3200x3200 at 4 m/px,
// height.bin 1601x1601 at 8 m); it is never committed. Without it the renderer draws the fallback layer.
import { existsSync, mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { homedir } from "node:os";
import { join } from "node:path";
import { blockMean } from "./bc7.mjs";
import { decodePng, encodePng } from "./png.mjs";

export const WORLD_M = 12800;          // Everon is 50 tiles of 256 m
export const TILES = 50;
export const TERRAIN_PX = 3200;        // 4 m per pixel
export const HEIGHT_N = 1601;          // 8 m per sample
export const HEIGHT_SCALE = 1000 / 65535; // metres per raw unit; the offset (sea level) is stored in the cache meta
const DATA = "worlds/Eden/Eden/.Data/";

export const cacheDir = (build, cacheRoot) => join(cacheRoot || process.env.LF_MAP_CACHE || join(homedir(), ".life-framework", "map-cache"), `everon-${build || "unknown"}`);

function hghtOf(b) {
  let o = 12;
  while (o + 8 <= b.length) {
    const t = b.toString("latin1", o, o + 4);
    const n = b.readUInt32BE(o + 4);
    if (t === "HGHT") return new Uint16Array(b.buffer.slice(b.byteOffset + o + 8, b.byteOffset + o + 8 + n));
    o += 8 + n + (n & 1);
  }
  return null;
}

// Standard LZ4 block decoder (EDDS stores the largest mip of a compressible tile that way).
export function lz4Block(src, outLen) {
  const out = Buffer.alloc(outLen);
  let i = 0, o = 0;
  while (i < src.length && o < outLen) {
    const tok = src[i++];
    let n = tok >> 4;
    if (n === 15) { let b; do { b = src[i++]; n += b; } while (b === 255); }
    src.copy(out, o, i, i + n);
    i += n; o += n;
    if (o >= outLen || i >= src.length) break;
    const off = src[i] | (src[i + 1] << 8);
    i += 2;
    let m = tok & 15;
    if (m === 15) { let b; do { b = src[i++]; m += b; } while (b === 255); }
    m += 4;
    for (let k = 0; k < m && o < outLen; k++, o++) out[o] = out[o - off];
  }
  return o === outLen ? out : null;
}

// The largest mip (65536 bytes of BC7 blocks) of a supertexture file, or null.
function largestMip(b) {
  let o = 148, total = 0;
  const sizes = [], types = [];
  for (let i = 0; i < 9; i++, o += 8) { types.push(b.toString("latin1", o, o + 4)); sizes.push(b.readUInt32LE(o + 4)); total += sizes[i]; }
  const last = sizes[8];
  const chunk = b.subarray(b.length - last);
  if (types[8] === "COPY") return chunk;
  return lz4Block(chunk, 65536) || lz4Block(chunk.subarray(4), 65536);
}

// One pass over the paks (about a minute).
export function extract(vfs, build, { log = () => {}, cacheRoot } = {}) {
  const dir = cacheDir(build, cacheRoot);
  mkdirSync(dir, { recursive: true });
  const tex = Buffer.alloc(TERRAIN_PX * TERRAIN_PX * 3);
  const hgt = new Uint16Array(HEIGHT_N * HEIGHT_N);
  const per = TERRAIN_PX / TILES; // 64 blocks per tile
  const hper = (HEIGHT_N - 1) / TILES; // 32 samples per tile
  const block = [0, 0, 0];
  let got = 0;
  for (let n = 0; n < TILES * TILES; n++) {
    const col = n % TILES;
    const row = Math.floor(n / TILES);
    const zRow = row; // tile row 0 is z 0 (south)
    const sp = `${DATA}Eden_${n}_supertexture.edds`;
    if (vfs.exists(sp)) {
      const mip = largestMip(vfs.readFile(sp));
      if (mip) for (let by = 0; by < per; by++) for (let bx = 0; bx < per; bx++) {
        blockMean(mip, (by * per + bx) * 16, block, 0);
        // Tile rows stack top to bottom in image order and the image is mirrored in z: tile row 0 is
        // z 0 (south), so the final image flips rows to put north on top.
        const px = col * per + bx;
        const py = TERRAIN_PX - 1 - (zRow * per + by);
        const k = (py * TERRAIN_PX + px) * 3;
        tex[k] = block[0]; tex[k + 1] = block[1]; tex[k + 2] = block[2];
      }
      got++;
    }
    const tp = `${DATA}Eden_${n}.ttile`;
    if (vfs.exists(tp)) {
      const h = hghtOf(vfs.readFile(tp));
      if (h) for (let j = 0; j <= hper; j++) for (let i = 0; i <= hper; i++) {
        hgt[(zRow * hper + j) * HEIGHT_N + col * hper + i] = h[j * 4 * 129 + i * 4];
      }
    }
    if (n % 500 === 499) log(`map extract: ${n + 1}/2500 tiles`);
  }
  writeFileSync(join(dir, "terrain.png"), encodePng({ w: TERRAIN_PX, h: TERRAIN_PX, ch: 3, data: tex }, 6));
  writeFileSync(join(dir, "height.bin"), Buffer.from(hgt.buffer));
  writeFileSync(join(dir, "meta.json"), JSON.stringify({ build, tiles: got, extracted: new Date().toISOString() }));
  return dir;
}

// The cached base layer, or null when it was never extracted (the fallback layer then draws).
export function loadBase(build, cacheRoot) {
  const dir = cacheDir(build, cacheRoot);
  try {
    if (!existsSync(join(dir, "terrain.png"))) return null;
    const terrain = decodePng(readFileSync(join(dir, "terrain.png")));
    const hb = readFileSync(join(dir, "height.bin"));
    const height = new Uint16Array(hb.buffer.slice(hb.byteOffset, hb.byteOffset + hb.length));
    return { terrain, height, dir };
  } catch {
    return null;
  }
}
