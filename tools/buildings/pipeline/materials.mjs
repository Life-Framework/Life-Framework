// The game colour of ArchBuilder's materials (tools\cli building, export stage).
//
// ArchBuilder writes one MatPBRBasic per material over a base-game _SharedData texture, with
// `Color` = its own tint, the same number it gives the material in Blender. Two things make that
// read wrong in the game (docs/architecture/building-pipeline-spike.md "Why the walls read white"):
//   1. `Color` multiplies the texture. ArchBuilder means Color = tint / texture mean, but its bundled
//      library has no texture means (they come only from scanning an extracted copy of the game
//      data), so the tint is written raw, and a material with no tint gets the bare texture. The
//      _SharedData plaster, bitumen and paint textures are near white (mean 0.69 to 0.79): a flat
//      roof that is dark grey in Blender is white in the game.
//   2. A Blender colour is not a game albedo. The base game's own white exterior plaster comes out
//      at about 0.37 (tint x texture mean), its beige houses at 0.26, its white interiors at about
//      0.61. ArchBuilder's white stucco is 0.85 in Blender, and its beige (0.76 0.66 0.50 over a 0.69
//      texture) lands at 0.46, brighter than the game's white church.
// So every AB_* material gets Color = k x (its Blender colour) / (its texture's measured linear
// mean), with k scaling ArchBuilder's white plaster to the base game's white plaster: one k for a
// material on any outside face, one for a material only inside. A channel that would need more
// than 1 scales the three down together (the hue stays; Color tops at 1).
import { existsSync, mkdirSync, readFileSync, readdirSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { eddsInfo, gameData, lz4Block } from "../../assets/pipeline/clothing-vanilla.mjs";

// Measured 2026-11-14 (game 1.9) with tools\cli building's materials job on the base game's own
// materials: albedo = Color x linear mean of the BCR texture, luminance (Rec. 709).
//   exterior white plaster: Church_01_ExtWalls_White 0.375, Chapel_01_Ext_White 0.360 -> 0.367
//   interior white plaster: House_Town_E_2I01_Int 0.645, House_Village_E_1L03_interior_1 0.575 -> 0.61
//   ArchBuilder white plaster in Blender: AB_Stucco_White 0.849, AB_Plaster_Interior 0.888
// Inside, the same albedo does not give the same picture. Our exported interiors are lit far more
// than the base game's (2026-11-14, one full-profile session at noon, DebugWorld): the supermarket's
// cream hall wall at albedo 0.57 rendered at sRGB 181 and its ceiling (0.61) at 233, while the white
// plaster of House_Town_E_2I01 rendered at 93 (wall) and 74 (ceiling) and a window-lit vanilla
// kitchen ceiling at 141. So the interior white is calibrated on the picture, not the albedo: half
// the base game's interior plaster albedo brings our hall to about the window-lit vanilla room
// (docs/building-pipeline/polish.md "Interior brightness").
//
// The room whites (a light finish on the walls and ceiling of a room: plaster, cream paint) go lower
// still, 2026-11-15 (docs/building-pipeline/polish.md "Interior brightness"). Probe settings and
// ceiling lights change nothing in a full-profile picture at noon: the room is lit by flat sky
// ambient, so albedo is the only lever, and at 0.30 the ceiling still read 204 and the walls 160 of
// 255 where the base game's read about 105 and 98. At 0.10 with a warm tint (the ambient is blue) a hall
// ceiling reads 134 and the walls sit in the base game's range. Floors, tiles and dark finishes keep
// the interior scale above.
export const GAME_WHITE = { exterior: 0.367, interior: 0.30, room: 0.10 };
export const BLENDER_WHITE = { exterior: 0.849, interior: 0.888, room: 0.888 };
export const ROOM_TINT = [1, 0.93, 0.8];
// A room white: faces looking mostly sideways or down, and a light Blender colour (luminance).
export const ROOM_UP_SHARE = 0.3;
export const ROOM_LIGHT = 0.6;
// ... on a plaster-like texture (a tile or granite texture is dark and needs its Color near 1) ...
export const ROOM_TEXTURE = 0.5;
// ... and the warm tint goes on near-neutral whites only: a cream or blue paint already has its hue.
export const ROOM_NEUTRAL = 0.15;
export const saturation = (c) => (Math.max(...c) ? (Math.max(...c) - Math.min(...c)) / Math.max(...c) : 0);
export const lum = (c) => 0.2126 * c[0] + 0.7152 * c[1] + 0.0722 * c[2];
export function surfaceOf(m, outsideShare = OUTSIDE_SHARE) {
  if (m.outside_share >= outsideShare) return "exterior";
  if ((m.up_share ?? 1) <= ROOM_UP_SHARE && lum(m.colour) >= ROOM_LIGHT && m.mean && Math.min(...m.mean) >= ROOM_TEXTURE) return "room";
  return "interior";
}
// A material is an outside one when at least this share of its area looks out of the building (the
// job casts a ray along each face's normal). Sunlit faces are where an over-bright albedo reads white.
export const OUTSIDE_SHARE = 0.25;
export const ALBEDO_SCALE = {
  exterior: GAME_WHITE.exterior / BLENDER_WHITE.exterior,
  interior: GAME_WHITE.interior / BLENDER_WHITE.interior,
  room: GAME_WHITE.room / BLENDER_WHITE.room,
};

const fmt = (v) => {
  const s = v.toFixed(4).replace(/0+$/, "").replace(/\.$/, "");
  return s === "-0" || s === "" ? "0" : s;
};

// The emat Color for a Blender colour over a texture of this mean (null mean: a flat material, the
// Color is the albedo). Returns { color, capped }.
export function gameColor(colour, mean, where) {
  const k = ALBEDO_SCALE[where];
  const target = colour.map((c, i) => c * k * (where === "room" && saturation(colour) < ROOM_NEUTRAL ? ROOM_TINT[i] : 1));
  let ratio = mean ? target.map((t, i) => t / Math.max(0.02, mean[i])) : target;
  const top = Math.max(...ratio);
  const capped = top > 1;
  if (capped) ratio = ratio.map((r) => r / top);
  return { color: ratio.map((r) => Math.max(0, r)), capped };
}

// An emat with its Color line set (replaced, or added after the class line).
export function withColor(ematText, color) {
  const line = ` Color ${color.map(fmt).join(" ")} 1`;
  if (/^ Color [^\n]*$/m.test(ematText)) return ematText.replace(/^ Color [^\n]*$/m, line);
  return ematText.replace(/^(\w+ \{)\n/, `$1\n${line}\n`);
}

export const bcrOf = (ematText) => (/^ BCRMap "\{[0-9A-F]{16}\}([^"]+)"/m.exec(ematText) || [])[1] || null;

// A plain one-mip .dds of the texture's mip nearest 64 px, enough for a mean and quick to read in
// Blender. The .edds mip table runs smallest first.
export function smallMipDds(buf, want = 64) {
  const info = eddsInfo(buf);
  let o = info.headerBytes;
  const table = [];
  for (let i = 0; i < info.mips; i++, o += 8) table.push({ kind: buf.toString("latin1", o, o + 4), size: buf.readUInt32LE(o + 4) });
  let level = 0;
  while (level < info.mips - 1 && Math.max(info.width, info.height) >> level > want) level++;
  const t = info.mips - 1 - level;
  for (let i = 0; i < t; i++) o += table[i].size;
  const raw = buf.subarray(o, o + table[t].size);
  const data = table[t].kind === "COPY" ? raw : lz4Block(raw);
  const head = Buffer.from(buf.subarray(0, info.headerBytes));
  head.fill(0, 32, 36); // Enfusion's "ENF1" tag in the reserved words
  head.writeUInt32LE(Math.max(1, info.height >> level), 12);
  head.writeUInt32LE(Math.max(1, info.width >> level), 16);
  head.writeUInt32LE(data.length, 20);
  head.writeUInt32LE(1, 28);
  return Buffer.concat([head, data]);
}

// Rewrites the Color of every AB_*.emat in the staging export. `run` is runArchBuilder; the job reads
// the Blender colours from building.blend and measures the textures. Returns one row per material;
// `.rooms` on the array is the building's rooms from the same run (blender/lf_rooms.py).
export async function colourMaterials({ staging, dir, spec, blend, run }) {
  const matDir = join(staging, "Assets", "ArchBuilder", "Materials");
  const emats = existsSync(matDir) ? readdirSync(matDir).filter((f) => /^AB_.*\.emat$/.test(f)) : [];
  const texDir = join(dir, "textures");
  mkdirSync(texDir, { recursive: true });
  const textures = {};
  const game = await gameData();
  for (const f of emats) {
    const bcr = bcrOf(readFileSync(join(matDir, f), "utf8"));
    if (!bcr || textures[bcr]) continue;
    const out = join(texDir, bcr.replace(/[\\/]/g, "_").replace(/\.edds$/, ".dds"));
    if (!existsSync(out)) {
      const buf = game.read(bcr);
      if (!buf) throw new Error(`${f} names ${bcr}, which the game does not have`);
      writeFileSync(out, smallMipDds(buf));
    }
    textures[bcr] = out;
  }
  const r = run("materials", { spec, out: dir, blend, extra: { textures }, timeoutMs: 180000 });
  const rows = [];
  for (const f of emats) {
    const name = f.replace(/\.emat$/, "");
    const m = r.materials[name];
    if (!m) throw new Error(`${name} is in the export but on no part of the building in Blender`);
    const text = readFileSync(join(matDir, f), "utf8");
    const bcr = bcrOf(text);
    const mean = bcr ? r.means[bcr] : null;
    if (bcr && !mean) throw new Error(`no measured mean for ${bcr}`);
    const where = surfaceOf({ ...m, mean });
    const { color, capped } = gameColor(m.colour, mean, where);
    writeFileSync(join(matDir, f), withColor(text, color));
    rows.push({ name, where, outside: m.outside_share, blender: m.colour, mean, color: color.map((c) => +fmt(c)), capped });
  }
  rows.rooms = r.rooms || [];
  return rows;
}
