// The far-distance texture of the vanilla ShopHouse_E_2I01t, without its baked "DROGERIE" sign.
//
//   node tools/buildings/shophouse-far/mlod.mjs          write the texture, its metas and the material
//   node tools/buildings/shophouse-far/mlod.mjs --check  exit 1 when the committed texture is not what this writes
//
// Why. The base game draws a building from far away with one merged model (the "Ext_MLOD" material, a single
// 512 px atlas of walls and roof). That atlas has the vanilla food-store sign baked into the shop front, a
// green plate reading DROGERIE under the shop window, so every ShopHouse reads DROGERIE from 30 m or more,
// whatever sign hangs on it up close. Overriding the sign's own MLOD (what the shops did) cannot reach it:
// the text is in the building's texture, not in the sign entity.
//
// What. The material is plain text (MatPBRBasic, one BCRMap). The texture is a BC7 .edds whose mips are
// stored raw ("COPY"), so the sign can be painted out block by block with no BC7 encoder: each 4 x 4 block
// of the sign is replaced by a block of the grey wall strip beside it, in every mip. The result is
// written as our own .edds, a .tif the same pixels (what Workbench builds the .edds from, as for every
// other texture in the pack) and an .emat the buildings assign to the "House_Town_E_2I01_Ext_MLOD" slot.
// Nothing of the vanilla files changes; a shop that does not assign the material keeps the vanilla one.
//
// Pixel boxes are in the 512 x 512 base mip (read from the atlas 2026-11-16).
import { randomBytes } from "node:crypto";
import { existsSync, mkdirSync, mkdtempSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join } from "node:path";
import { spawnSync } from "node:child_process";
import { fileURLToPath } from "node:url";
import { gameVfs, readBytes } from "../../lib/game-prefab.mjs";
import { blenderExe } from "../../lib/blender.mjs";
import { eddsInfo, eddsToDds } from "../../assets/pipeline/clothing-vanilla.mjs";

const ROOT = join(dirname(fileURLToPath(import.meta.url)), "..", "..", "..");
const VANILLA = "Assets/Structures/Commercial/Shop/ShopHouse_E_2I01/Data/ShopHouse_E_2I01_Ext_MLOD_BCR.edds";
const DIR = "Assets/Buildings/Facades";
const NAME = "LF_ShopHouse_Ext_MLOD";
const ADDON = join(ROOT, "addons", "LF_Everon1989");

// The sign plate is x 81..134, y 193..203. Its blocks are painted over with the wall strip left of the
// shop window (x 56..79, no window and no door in these rows), repeated across.
export const SIGN = { x0: 80, x1: 135, y0: 192, y1: 203 };
export const WALL = { x0: 56, x1: 79 };
const BLOCK_BYTES = 16; // BC7

// Patch one mip (raw BC7 blocks, width w) for the box, in place.
export function patchMip(data, w, level) {
  const s = 2 ** level;
  const bw = Math.max(1, w / 4);
  const range = (a, b) => [Math.floor(a / s / 4), Math.max(Math.floor(a / s / 4), Math.ceil((b + 1) / s / 4) - 1)];
  const [dx0, dx1] = range(SIGN.x0, SIGN.x1);
  const [dy0, dy1] = range(SIGN.y0, SIGN.y1);
  const [sx0, sx1] = range(WALL.x0, WALL.x1);
  const span = sx1 - sx0 + 1;
  if (span < 1 || dx1 >= bw) return 0;
  let n = 0;
  for (let by = dy0; by <= dy1; by++) {
    for (let bx = dx0; bx <= dx1; bx++) {
      const from = (by * bw + sx0 + ((bx - dx0) % span)) * BLOCK_BYTES;
      const to = (by * bw + bx) * BLOCK_BYTES;
      data.copy(data, to, from, from + BLOCK_BYTES);
      n++;
    }
  }
  return n;
}

// The patched .edds, from the vanilla bytes (every mip is stored raw: the table is checked).
export function patchEdds(buf) {
  const info = eddsInfo(buf);
  if (info.format !== "BC7_SRGB" || info.width !== 512 || info.height !== 512) throw new Error(`unexpected texture ${JSON.stringify(info)}`);
  const out = Buffer.from(buf);
  let o = info.headerBytes;
  const table = [];
  for (let i = 0; i < info.mips; i++, o += 8) table.push({ kind: buf.toString("latin1", o, o + 4), size: buf.readUInt32LE(o + 4) });
  o = info.headerBytes + 8 * info.mips;
  const patched = [];
  for (const [i, t] of table.entries()) {
    if (t.kind !== "COPY") throw new Error(`mip block ${i} is ${t.kind}, not COPY: the block patch needs raw mips`);
    const level = table.length - 1 - i; // the table is smallest first
    const w = 512 >> level;
    if (w >= 16) patched.push({ level, blocks: patchMip(out.subarray(o, o + t.size), w, level) });
    o += t.size;
  }
  return { edds: out, patched };
}

const guidOf = (metaPath) => (existsSync(metaPath) ? /Name "\{([0-9A-F]{16})\}/.exec(readFileSync(metaPath, "utf8"))?.[1] : null) || randomBytes(8).toString("hex").toUpperCase();

function metaText(guid, path, kind) {
  if (kind === "emat") {
    return `MetaFileClass {\n Name "{${guid}}${path}"\n Configurations {\n  EMATResourceClass PC {\n  }\n  EMATResourceClass XBOX_ONE : PC {\n  }\n  EMATResourceClass XBOX_SERIES : PC {\n  }\n  EMATResourceClass PS4 : PC {\n  }\n  EMATResourceClass PS5 : PC {\n  }\n  EMATResourceClass HEADLESS : PC {\n  }\n }\n}\n`;
  }
  const conf = { PC: "EAB5DE3219F9CBA8", XBOX_ONE: "91D862F89991BFBE", XBOX_SERIES: "5FEAED1642ECE679", PS4: "12273E1A0928F0C4", HEADLESS: "BEAF5CD0C438676E" };
  const names = { PC: "PC", XBOX_ONE: "XBOX_ONE", XBOX_SERIES: "XBOX_SERIES", PS4: "PS4", HEADLESS: "HEADLESS" };
  const rows = Object.keys(conf).map((k) => `  TIFFResourceClass ${names[k]} : "{${conf[k]}}Configs/System/ResourceTypes/${k}/TextureColorMap.conf" {\n  }\n`);
  rows.push("  TIFFResourceClass PS5 : PC {\n  }\n");
  return `MetaFileClass {\n Name "{${guid}}${path}"\n Configurations {\n${rows.join("")} }\n}`;
}

// An uncompressed 8-bit RGBA TIFF (the layout the pack's other .tif textures have) from top-down RGBA bytes.
export function tiff(rgba, w, h) {
  const tags = 11;
  const ifd = 8;
  const bps = ifd + 2 + tags * 12 + 4;
  const data = bps + 8;
  const buf = Buffer.alloc(data + w * h * 4);
  buf.write("II", 0, "latin1");
  buf.writeUInt16LE(42, 2);
  buf.writeUInt32LE(ifd, 4);
  buf.writeUInt16LE(tags, ifd);
  const rows = [
    [0x100, 4, 1, w], [0x101, 4, 1, h], [0x102, 3, 4, bps], [0x103, 3, 1, 1], [0x106, 3, 1, 2], [0x111, 4, 1, data],
    [0x115, 3, 1, 4], [0x116, 4, 1, h], [0x117, 4, 1, w * h * 4], [0x11c, 3, 1, 1], [0x152, 3, 1, 2],
  ];
  rows.forEach(([tag, type, count, value], i) => {
    const o = ifd + 2 + i * 12;
    buf.writeUInt16LE(tag, o);
    buf.writeUInt16LE(type, o + 2);
    buf.writeUInt32LE(count, o + 4);
    if (type === 3 && count === 1) buf.writeUInt16LE(value, o + 8);
    else buf.writeUInt32LE(value, o + 8);
  });
  for (let i = 0; i < 4; i++) buf.writeUInt16LE(8, bps + i * 2);
  rgba.copy(buf, data);
  return buf;
}

// The patched texture's pixels as a TIFF, through Blender (which reads the BC7 DDS).
function writeTif(dds, tif, w, h) {
  const tmp = mkdtempSync(join(tmpdir(), "lf-mlod-"));
  try {
    const ddsFile = join(tmp, "in.dds");
    const rawFile = join(tmp, "out.rgba");
    const py = join(tmp, "conv.py");
    writeFileSync(ddsFile, dds);
    writeFileSync(py, [
      "import bpy, sys, numpy as np",
      "src, dst = sys.argv[-2], sys.argv[-1]",
      "img = bpy.data.images.load(src)",
      "img.colorspace_settings.name = 'Non-Color'",
      "px = np.array(img.pixels[:], dtype=np.float32).reshape(img.size[1], img.size[0], 4)",
      "open(dst, 'wb').write((np.flipud(px) * 255.0 + 0.5).clip(0, 255).astype(np.uint8).tobytes())",
    ].join("\n"));
    const r = spawnSync(blenderExe(), ["-b", "--python", py, "--", ddsFile, rawFile], { encoding: "utf8", windowsHide: true, timeout: 120000 });
    if (r.status !== 0 || !existsSync(rawFile)) throw new Error(`Blender could not read the texture: ${(r.stderr || r.stdout || "").slice(-400)}`);
    const rgba = readFileSync(rawFile);
    if (rgba.length !== w * h * 4) throw new Error(`Blender gave ${rgba.length} bytes, expected ${w * h * 4}`);
    writeFileSync(tif, tiff(rgba, w, h));
  } finally {
    rmSync(tmp, { recursive: true, force: true });
  }
}

const vfs = await gameVfs();
const vanilla = readBytes(ROOT, vfs, VANILLA);
if (!vanilla) throw new Error(`the game has no ${VANILLA}`);
const { edds, patched } = patchEdds(vanilla);
const eddsRel = `${DIR}/${NAME}_BCR.edds`;
const ematRel = `${DIR}/${NAME}.emat`;
const eddsAbs = join(ADDON, eddsRel);

if (process.argv.includes("--check")) {
  const same = existsSync(eddsAbs) && readFileSync(eddsAbs).equals(edds);
  console.log(same ? "shophouse-mlod: the committed texture is current" : "shophouse-mlod: the texture differs; run node tools/buildings/shophouse-far/mlod.mjs");
  process.exit(same ? 0 : 1);
}

mkdirSync(join(ADDON, DIR), { recursive: true });
const eddsGuid = guidOf(`${eddsAbs}.meta`);
const ematGuid = guidOf(`${join(ADDON, ematRel)}.meta`);
writeFileSync(eddsAbs, edds);
writeFileSync(`${eddsAbs}.meta`, metaText(eddsGuid, eddsRel, "edds"));
writeTif(eddsToDds(edds), join(ADDON, `${DIR}/${NAME}_BCR.tif`), 512, 512);
writeFileSync(join(ADDON, ematRel), `MatPBRBasic {\n DisableInteriorProbes 1\n MetalnessScale 0\n BCRMap "{${eddsGuid}}${eddsRel}"\n}\n`);
writeFileSync(`${join(ADDON, ematRel)}.meta`, metaText(ematGuid, ematRel, "emat"));
console.log(`shophouse-mlod: ${patched.map((p) => `mip ${p.level}: ${p.blocks} blocks`).join(", ")}`);
console.log(`shophouse-mlod: material {${ematGuid}}${ematRel}`);
