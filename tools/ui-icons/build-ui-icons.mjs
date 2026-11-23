// node tools/ui-icons/build-ui-icons.mjs [--preview-only]
// The UI kit's small icon set (a ledger row's icon: cash, bank, house, ...). Drawn from the vector
// shapes below in white, so LF_UIScreen tints each with the theme's ink and a pack theme recolours
// them with no new art. Writes, next to each other under addons/LifeFramework/UI/Imagesets/:
//   LF_UIIcons.tif             the source the Workbench imports (uncompressed RGBA)
//   LF_UIIcons.edds.meta       texture meta (TextureUI), its GUID kept on a rerun
//   LF_UIIcons.imageset(.meta) the atlas quads, one per name in ICONS
// The .edds itself is built by Workbench on a pool machine:
//   tools\cli ui-image --batch tools/ui-icons/ui-icons-batch.txt
// Deterministic: a rerun changes nothing. A contact sheet on paper goes to tmp/ui-icons.png.
// The look follows the map icons (tools/map/icons/build-icons.mjs): plain 1989 pictograms, round
// strokes, no gradients. sharp comes from website/node_modules (LF_SHARP_FROM overrides).
import { createRequire } from "node:module";
import { existsSync, mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { randomBytes } from "node:crypto";

const ROOT = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
const sharp = createRequire(join(process.env.LF_SHARP_FROM || join(ROOT, "website"), "package.json"))("sharp");

const ADDON = "addons/LifeFramework";
const BASE = `${ADDON}/UI/Imagesets/LF_UIIcons`;
const CELL = 64;
const ATLAS = 256;
const W = "#ffffff";
const S = (w = 5) => `stroke="${W}" stroke-width="${w}" fill="none" stroke-linecap="round" stroke-linejoin="round"`;

// Quad names are what LF_UIPart.WithIcon takes; keep them in LF_UITheme's comment in step.
export const ICONS = {
  cash: `<rect x="6" y="16" width="52" height="32" rx="3" ${S()}/><circle cx="32" cy="32" r="8" ${S(4)}/><path d="M14 24V40M50 24V40" ${S(4)}/>`,
  bank: `<path d="M6 24L32 8L58 24Z" fill="${W}"/><path d="M14 30V48M26 30V48M38 30V48M50 30V48" ${S(6)}/><path d="M8 56H56" ${S(6)}/>`,
  house: `<path d="M8 30L32 10L56 30" ${S()}/><path d="M14 26V56H50V26" ${S()}/><path d="M27 56V40H37V56" ${S(4)}/>`,
  garage: `<path d="M6 26L32 10L58 26V56H6Z" ${S()}/><path d="M16 56V34H48V56M16 41H48M16 48H48" ${S(4)}/>`,
  licence: `<rect x="6" y="14" width="52" height="36" rx="4" ${S()}/><circle cx="22" cy="30" r="6" ${S(4)}/><path d="M14 44Q22 36 30 44M36 26H50M36 34H50M36 42H46" ${S(4)}/>`,
  clock: `<circle cx="32" cy="32" r="24" ${S()}/><path d="M32 16V32L42 40" ${S()}/>`,
  calendar: `<rect x="8" y="12" width="48" height="44" rx="3" ${S()}/><path d="M8 24H56M20 6V16M44 6V16" ${S()}/><path d="M18 34H24M30 34H36M42 34H48M18 44H24M30 44H36" ${S(4)}/>`,
  job: `<rect x="6" y="20" width="52" height="34" rx="4" ${S()}/><path d="M24 20V14Q24 10 28 10H36Q40 10 40 14V20M6 34H58" ${S()}/>`,
  standing: `<polygon points="32,6 39,24 58,24 43,36 49,55 32,44 15,55 21,36 6,24 25,24" ${S(4)}/>`,
  wanted: `<circle cx="32" cy="32" r="22" ${S()}/><circle cx="32" cy="32" r="10" ${S(4)}/><path d="M32 4V16M32 48V60M4 32H16M48 32H60" ${S(4)}/>`,
  skill: `<g transform="rotate(-45 32 32)"><path d="M32 22V60" ${S(6)}/><rect x="18" y="6" width="28" height="16" rx="2" fill="${W}"/></g>`,
  errand: `<path d="M16 6V60" ${S()}/><path d="M18 8H52L44 20L52 32H18Z" fill="${W}"/>`,
  police: `<path d="M32 6L54 14V32C54 46 44 54 32 59C20 54 10 46 10 32V14Z" ${S()}/><path d="M32 20L35 29H44L37 34L40 43L32 38L24 43L27 34L20 29H29Z" fill="${W}"/>`,
  medic: `<rect x="8" y="8" width="48" height="48" rx="4" ${S()}/><path d="M32 18V46M18 32H46" stroke="${W}" stroke-width="9" fill="none"/>`,
  food: `<path d="M12 6V22Q12 31 20 31Q28 31 28 22V6M20 6V31M20 31V58" ${S(4)}/><path d="M44 58V6Q56 12 56 30Q56 36 50 36H44" ${S(4)}/>`,
  drink: `<path d="M14 8H50L45 56H19Z" ${S(4)}/><path d="M17 24H47" ${S(4)}/>`,
};

function layout(names) {
  const per = ATLAS / CELL;
  const pos = new Map();
  names.forEach((n, i) => pos.set(n, { x: (i % per) * CELL, y: Math.floor(i / per) * CELL, w: CELL, h: CELL }));
  return pos;
}

const guid = () => randomBytes(8).toString("hex").toUpperCase();
const metaGuid = (file) => (/Name\s+"\{([0-9A-F]{16})\}/.exec(readFileSync(file, "utf8")) ?? [])[1] ?? null;
const CONF = { PC: "3549C665CAA9A8EA", XBOX_ONE: "5E6D8E2B87D36B34", XBOX_SERIES: "03194CD39583675A", PS4: "A752014F057BCE60", HEADLESS: "E604D15366D985DF" };

function textureMeta(edds) {
  const file = join(ROOT, `${edds}.meta`);
  const rel = edds.slice(ADDON.length + 1);
  if (existsSync(file)) return { guid: metaGuid(file), rel };
  const g = guid();
  const cls = (p) => `  TIFFResourceClass ${p} : "{${CONF[p]}}Configs/System/ResourceTypes/${p}/TextureUI.conf" {\n  }\n`;
  writeFileSync(file, `MetaFileClass {\n Name "{${g}}${rel}"\n Configurations {\n${cls("PC")}${cls("XBOX_ONE")}${cls("XBOX_SERIES")}${cls("PS4")}  TIFFResourceClass PS5 : PC {\n  }\n${cls("HEADLESS")} }\n}\n`);
  return { guid: g, rel };
}

function imagesetMeta(imageset) {
  const file = join(ROOT, `${imageset}.meta`);
  if (existsSync(file)) return metaGuid(file);
  const rel = imageset.slice(ADDON.length + 1);
  const g = guid();
  writeFileSync(file, `MetaFileClass {\n Name "{${g}}${rel}"\n Configurations {\n  ImageSetResourceClass PC {\n  }\n  ImageSetResourceClass XBOX_ONE : PC {\n  }\n  ImageSetResourceClass XBOX_SERIES : PC {\n  }\n  ImageSetResourceClass PS4 : PC {\n  }\n  ImageSetResourceClass HEADLESS : PC {\n  }\n }\n}\n`);
  return g;
}

function imagesetText(tex, names, pos) {
  const defs = names.map((n) => {
    const p = pos.get(n);
    return `  ImageSetDefClass ${n} {\n   Name "${n}"\n   Pos ${p.x} ${p.y}\n   Size ${p.w} ${p.h}\n   Flags 0\n  }\n`;
  }).join("");
  return `ImageSetClass {\n RefSize ${ATLAS} ${ATLAS}\n Textures {\n  ImageSetTextureClass {\n   mpix 1\n   path "{${tex.guid}}${tex.rel}"\n  }\n }\n Images {\n${defs} }\n}`;
}

async function main() {
  const names = Object.keys(ICONS);
  if (names.length > (ATLAS / CELL) ** 2) throw new Error(`${names.length} icons do not fit a ${ATLAS} atlas`);
  const pos = layout(names);
  const layers = [];
  for (const n of names) {
    const svg = `<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64" width="64" height="64">${ICONS[n]}</svg>`;
    const input = await sharp(Buffer.from(svg), { density: 144 }).resize(CELL, CELL).png().toBuffer();
    const p = pos.get(n);
    layers.push({ input, left: p.x, top: p.y });
  }
  const base = sharp({ create: { width: ATLAS, height: ATLAS, channels: 4, background: { r: 0, g: 0, b: 0, alpha: 0 } } });
  const { data } = await base.composite(layers).raw().toBuffer({ resolveWithObject: true });

  // Contact sheet: the white glyphs tinted ink on paper, as the sheet draws them.
  const out = join(ROOT, "tmp", "ui-icons.png");
  mkdirSync(dirname(out), { recursive: true });
  const art = await sharp(data, { raw: { width: ATLAS, height: ATLAS, channels: 4 } }).negate({ alpha: false }).png().toBuffer();
  await sharp({ create: { width: ATLAS, height: ATLAS, channels: 4, background: "#eee6d4" } }).composite([{ input: art }]).resize(ATLAS * 2, ATLAS * 2).png().toFile(out);
  console.log(`ui-icons: ${names.length} quads, contact sheet ${out}`);
  if (process.argv.includes("--preview-only")) return;

  const tif = join(ROOT, `${BASE}.tif`);
  mkdirSync(dirname(tif), { recursive: true });
  await sharp(data, { raw: { width: ATLAS, height: ATLAS, channels: 4 } }).tiff({ compression: "none" }).toFile(tif);
  const tex = textureMeta(`${BASE}.edds`);
  writeFileSync(join(ROOT, `${BASE}.imageset`), imagesetText(tex, names, pos));
  const set = imagesetMeta(`${BASE}.imageset`);
  writeFileSync(join(ROOT, "tools", "ui-icons", "ui-icons-batch.txt"), `${BASE}.tif\n`);
  console.log(`SUMMARY ui-icons ok imageset={${set}}${BASE.slice(ADDON.length + 1)}.imageset (next: tools\\cli ui-image --batch tools/ui-icons/ui-icons-batch.txt)`);
}

await main();
