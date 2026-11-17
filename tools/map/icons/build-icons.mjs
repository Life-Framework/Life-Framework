// node tools/map/icons/build-icons.mjs [--preview-only]
// Card M3 (#2231). Draws the map icon set from the vector shapes below and writes, for the core
// placeholders and the 1989 pack:
//   <name>.tif        the source the Workbench imports (uncompressed RGBA)
//   <name>.edds.meta  texture meta (TextureUI), the GUID kept on a rerun
//   <name>.imageset   the atlas quads (the manifest is tools/map/lib/icons.mjs)
//   <name>.imageset.meta
// The .edds itself is built by Workbench on a pool machine: `tools\cli ui-image --batch
// tools/map/icons/icons-batch.txt` (one boot for every texture; docs/features/map.md "Icons").
// Output is deterministic, so a rerun changes nothing. A contact sheet of each atlas on paper goes to
// tmp/map/icons-<side>.png for a look without Workbench.
// sharp comes from website/node_modules (LF_SHARP_FROM=<dir holding package.json> points elsewhere).
import { createRequire } from "node:module";
import { existsSync, mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { randomBytes } from "node:crypto";
import {
  ATLAS, CELL, CATEGORIES, CORE_HATCH, PACK_HATCH, FILES, PACK_KINDS, SHARED,
  catQuad, coreQuadNames, packQuadNames, poiQuad, layout,
} from "../lib/icons.mjs";

const ROOT = join(dirname(fileURLToPath(import.meta.url)), "..", "..", "..");
const sharp = createRequire(join(process.env.LF_SHARP_FROM || join(ROOT, "website"), "package.json"))("sharp");

// ---- the 1989 inks -----------------------------------------------------------------------
export const INK = {
  black: "#1d1a17", red: "#b3261e", blue: "#1f4e8c", green: "#2f6b34", brown: "#6b4423",
  graphite: "#46464e", grey: "#555555", core: "#3d3d3d", paper: "#efe6d0",
};
const CAT_INK = { civic: "black", shop: "black", work: "green", law: "red", health: "red", travel: "blue", story: "red", underworld: "graphite" };
const KIND_INK = {
  Bank: "black", PoliceStation: "red", PoliceArmoury: "red", HospitalSite: "red", Workplace: "green", Fence: "graphite",
  Fuel: "black", Payphone: "black", Quarry: "brown", Fish: "blue", Story: "red", Taxi: "blue", Harbour: "blue",
  Farm: "green", Shop: "black", TownHall: "black", Church: "black", Pub: "black", Fire: "red", Depot: "brown",
};

// ---- shapes (64x64 viewBox) ----------------------------------------------------------------
const star = (cx, cy, R, r) => Array.from({ length: 10 }, (_, i) => {
  const a = -Math.PI / 2 + (i * Math.PI) / 5;
  const d = i % 2 ? r : R;
  return `${(cx + d * Math.cos(a)).toFixed(1)},${(cy + d * Math.sin(a)).toFixed(1)}`;
}).join(" ");
const S = (ink, w = 4) => `stroke="${ink}" stroke-width="${w}" fill="none" stroke-linecap="round" stroke-linejoin="round"`;

const SHAPES = {
  coin: (k) => `<circle cx="32" cy="32" r="24" ${S(k)}/><circle cx="32" cy="32" r="15" ${S(k, 3)}/><path d="M32 23v18" ${S(k, 3)}/>`,
  star: (k) => `<polygon points="${star(32, 33, 27, 11)}" fill="${k}"/>`,
  shield: (k) => `<path d="M32 6L54 14V32C54 46 44 54 32 59C20 54 10 46 10 32V14Z" ${S(k)}/><polygon points="${star(32, 32, 12, 5)}" fill="${k}"/>`,
  clinic: (k) => `<rect x="8" y="8" width="48" height="48" rx="3" ${S(k)}/><path d="M32 18v28M18 32h28" stroke="${k}" stroke-width="8" fill="none"/>`,
  hammerpick: (k) => `<g transform="rotate(45 32 32)"><path d="M32 8V58" ${S(k)}/><path d="M20 12H44" ${S(k, 7)}/></g><g transform="rotate(-45 32 32)"><path d="M32 8V58" ${S(k)}/><path d="M18 15Q32 3 46 15" ${S(k)}/></g>`,
  diamond: (k) => `<path d="M32 6L58 32L32 58L6 32Z" ${S(k)}/><path d="M26 27Q26 20 32 20Q38 20 38 27Q38 31 32 34V37" ${S(k, 3)}/><circle cx="32" cy="44" r="2.6" fill="${k}"/>`,
  pump: (k) => `<rect x="14" y="10" width="24" height="44" rx="3" ${S(k)}/><rect x="19" y="16" width="14" height="12" fill="${k}"/><path d="M38 24H46Q50 24 50 29V44Q50 48 54 48" ${S(k)}/><path d="M10 56H42" ${S(k)}/>`,
  handset: (k) => `<path d="M8 24Q32 6 56 24L51 34Q44 28 38 30V36H26V30Q20 28 13 34Z" fill="${k}"/><path d="M20 46H44M24 54H40" ${S(k, 3)}/>`,
  fish: (k) => `<ellipse cx="27" cy="32" rx="20" ry="12" ${S(k)}/><path d="M47 32L60 20V44Z" fill="${k}"/><circle cx="19" cy="29" r="2.6" fill="${k}"/>`,
  flag: (k) => `<path d="M16 6V60" ${S(k)}/><path d="M18 8H54L44 20L54 32H18Z" fill="${k}"/>`,
  taxi: (k) => `<rect x="8" y="26" width="48" height="24" rx="6" ${S(k)}/><rect x="24" y="12" width="16" height="10" rx="2" fill="${k}"/><circle cx="20" cy="52" r="5" fill="${k}"/><circle cx="44" cy="52" r="5" fill="${k}"/>`,
  anchor: (k) => `<circle cx="32" cy="12" r="5" ${S(k)}/><path d="M32 17V54M20 28H44M10 38Q12 54 32 54Q52 54 54 38" ${S(k)}/>`,
  barn: (k) => `<path d="M8 28L32 8L56 28V54H8Z" ${S(k)}/><path d="M24 54V38H40V54M24 38L40 54M40 38L24 54" ${S(k, 3)}/>`,
  bag: (k) => `<path d="M12 22H52L48 56H16Z" ${S(k)}/><path d="M22 22V16Q22 8 32 8Q42 8 42 16V22" ${S(k)}/>`,
  hall: (k) => `<path d="M6 24L32 8L58 24Z" fill="${k}"/><path d="M14 30V50M26 30V50M38 30V50M50 30V50" stroke="${k}" stroke-width="5" fill="none"/><path d="M8 56H56" ${S(k, 5)}/>`,
  church: (k) => `<path d="M12 56V32L32 22L52 32V56Z" ${S(k)}/><path d="M32 4V24M24 11H40" ${S(k)}/>`,
  mug: (k) => `<path d="M14 14H44V50Q44 56 38 56H20Q14 56 14 50Z" ${S(k)}/><path d="M44 22H52Q56 22 56 28V34Q56 40 50 40H44" ${S(k)}/><path d="M22 24V46M30 24V46" ${S(k, 3)}/>`,
  flame: (k) => `<path d="M32 4Q42 20 48 30Q54 42 44 54Q38 60 32 60Q22 60 16 52Q10 42 20 30Q22 38 28 38Q24 22 32 4Z" fill="${k}"/>`,
  crate: (k) => `<rect x="10" y="14" width="44" height="40" ${S(k)}/><path d="M10 14L54 54M54 14L10 54" ${S(k, 3)}/>`,
};
const CAT_SHAPE = { civic: "hall", shop: "bag", work: "hammerpick", law: "star", health: "clinic", travel: "taxi", story: "flag", underworld: "diamond" };
const KIND_SHAPE = {
  Bank: "coin", PoliceStation: "star", PoliceArmoury: "shield", HospitalSite: "clinic", Workplace: "hammerpick", Fence: "diamond",
  Fuel: "pump", Payphone: "handset", Quarry: "hammerpick", Fish: "fish", Story: "flag", Taxi: "taxi", Harbour: "anchor",
  Farm: "barn", Shop: "bag", TownHall: "hall", Church: "church", Pub: "mug", Fire: "flame", Depot: "crate",
};

// Hand marks: graphite wobble, no fill. Pins: a coloured teardrop with an ink outline.
const pencil = (d, extra = "") => `<g ${S(INK.graphite, 3.6)}>${d}${extra}</g>`;
const PENCIL_SHAPES = {
  pencil_cross: () => pencil(`<path d="M13 14Q32 31 51 51"/><path d="M51 12Q31 34 14 52"/>`),
  pencil_circle: () => pencil(`<path d="M32 10C48 8 56 22 54 34C52 50 36 56 24 52C10 48 8 28 18 18C22 14 30 11 37 12"/>`),
  pencil_arrow: () => pencil(`<path d="M10 51Q30 38 52 14"/><path d="M37 13H52V28"/>`),
  pencil_note: () => pencil(`<path d="M8 20q4 -7 8 0t8 0t8 0t8 0t8 0"/><path d="M8 34q4 -7 8 0t8 0t8 0t8 0"/><path d="M8 48q4 -7 8 0t8 0t8 0"/>`),
  pencil_question: () => pencil(`<path d="M20 22Q20 8 32 8Q46 8 46 22Q46 32 34 36Q32 40 32 44"/>`, `<circle cx="32" cy="55" r="2.6" fill="${INK.graphite}"/>`),
};
const pin = (fill) => `<path d="M32 60C32 60 12 38 12 24A20 20 0 0 1 52 24C52 38 32 60 32 60Z" fill="${fill}" stroke="${INK.black}" stroke-width="3" stroke-linejoin="round"/><circle cx="32" cy="24" r="7" fill="${INK.paper}"/>`;
const PIN_SHAPES = {
  pin_police: () => pin("#2a5aa8"), pin_org: () => pin("#d9d9d9"), pin_taxi: () => pin("#e2b412"), pin_waypoint: () => pin(INK.red),
};
const STAMP_SHAPES = {
  stamp_town: () => `<circle cx="32" cy="32" r="26" ${S(INK.black)}/><circle cx="32" cy="32" r="19" ${S(INK.black, 2)}/><circle cx="32" cy="32" r="7" fill="${INK.black}"/>`,
  stamp_nopolice: () => `<circle cx="32" cy="32" r="26" ${S(INK.red, 4)}/><polygon points="${star(32, 32, 15, 6)}" fill="${INK.red}"/><path d="M14 50L50 14" ${S(INK.red, 5)}/>`,
  stamp_restricted: () => `<rect x="6" y="14" width="52" height="36" ${S(INK.grey, 3)}/><path d="M6 32L24 14M6 48L40 14M20 50L56 18M38 50L58 32" ${S(INK.grey, 2.6)}/>`,
  stamp_you: () => `<path d="M32 6L52 56L32 44L12 56Z" fill="${INK.blue}"/>`,
};
const FIXED = { ...PENCIL_SHAPES, ...PIN_SHAPES, ...STAMP_SHAPES };

function svgFor(side, name) {
  let body;
  if (FIXED[name]) body = FIXED[name]();
  else {
    const cat = CATEGORIES.find((c) => catQuad(c) === name);
    const kind = PACK_KINDS.find((k) => poiQuad(k) === name);
    const shape = SHAPES[cat ? CAT_SHAPE[cat] : KIND_SHAPE[kind]];
    if (!shape) throw new Error(`no shape for ${name}`);
    const inkName = side === "core" ? "core" : cat ? CAT_INK[cat] : KIND_INK[kind];
    body = shape(INK[inkName]);
    // Cheap offset print: the red plate sits a pixel off its black neighbour.
    if (side === "pack" && inkName === "red") body = `<g transform="translate(1.6 1.3)" opacity="0.3">${shape(INK.red)}</g>${body}`;
  }
  return `<svg xmlns="http://www.w3.org/2000/svg" width="${CELL}" height="${CELL}" viewBox="0 0 64 64">${body}</svg>`;
}

// ---- stamped edges -------------------------------------------------------------------------
function hash(x, y, seed) {
  let h = (x * 374761393 + y * 668265263 + seed * 2147483647) | 0;
  h = Math.imul(h ^ (h >>> 13), 1274126177);
  return ((h ^ (h >>> 16)) >>> 0) / 4294967296;
}
// 1-2 px rough edge, a few ink dropouts: deterministic.
function stamp(buf, w, h, seed) {
  for (let y = 0; y < h; y++) {
    for (let x = 0; x < w; x++) {
      const i = (y * w + x) * 4 + 3;
      const a = buf[i];
      if (!a) continue;
      const n = hash(x, y, seed);
      if (a < 250) buf[i] = Math.min(255, Math.round(a * (0.5 + 0.95 * n)));
      else if (n > 0.985) buf[i] = 150;
      else if (n < 0.012) buf[i] = 205;
    }
  }
}

async function tif(file, buf, w, h) {
  mkdirSync(dirname(file), { recursive: true });
  await sharp(buf, { raw: { width: w, height: h, channels: 4 } }).tiff({ compression: "none" }).toFile(file);
}

async function atlas(side) {
  const names = side === "core" ? coreQuadNames() : packQuadNames();
  const pos = layout(names);
  const layers = [];
  for (const n of names) {
    const input = await sharp(Buffer.from(svgFor(side, n)), { density: 144 }).resize(CELL, CELL).png().toBuffer();
    const p = pos.get(n);
    layers.push({ input, left: p.x, top: p.y });
  }
  const base = sharp({ create: { width: ATLAS, height: ATLAS, channels: 4, background: { r: 0, g: 0, b: 0, alpha: 0 } } });
  const { data } = await base.composite(layers).raw().toBuffer({ resolveWithObject: true });
  stamp(data, ATLAS, ATLAS, side === "core" ? 1 : 2);
  return { names, pos, data };
}

// White-ink tiles the draw command tints (core), inked tiles (pack). 64x64, seamless: the period divides 64.
function hatchTile(kind) {
  const N = 64;
  const buf = Buffer.alloc(N * N * 4);
  const ink = {
    Diagonal: [255, 255, 255], Cross: [255, 255, 255], Dots: [255, 255, 255],
    Lawless: [179, 38, 30], Gated: [85, 85, 85],
  }[kind];
  for (let y = 0; y < N; y++) {
    for (let x = 0; x < N; x++) {
      let on = false;
      if (kind === "Diagonal" || kind === "Lawless") on = (x + y) % 8 < 3;
      else if (kind === "Cross" || kind === "Gated") on = (x + y) % 8 < 2 || (x - y + N) % 8 < 2;
      else if (kind === "Dots") { const dx = (x % 8) - 3.5, dy = (y % 8) - 3.5; on = dx * dx + dy * dy < 2.6; }
      if (!on) continue;
      let a = 255;
      const n = hash(x, y, 7);
      if (kind === "Lawless" || kind === "Gated") a = n < 0.1 ? 120 : n > 0.9 ? 215 : 255; // print grain
      const i = (y * N + x) * 4;
      buf[i] = ink[0]; buf[i + 1] = ink[1]; buf[i + 2] = ink[2]; buf[i + 3] = a;
    }
  }
  return { data: buf, w: N, h: N };
}

// ---- meta files ----------------------------------------------------------------------------
const CONF = { PC: "3549C665CAA9A8EA", XBOX_ONE: "5E6D8E2B87D36B34", XBOX_SERIES: "03194CD39583675A", PS4: "A752014F057BCE60", HEADLESS: "E604D15366D985DF" };
const guid = () => randomBytes(8).toString("hex").toUpperCase();
const metaGuid = (file) => (/Name\s+"\{([0-9A-F]{16})\}/.exec(readFileSync(file, "utf8")) ?? [])[1] ?? null;

// Writes <base>.edds.meta (kept on a rerun) and returns { guid, rel } of the .edds.
function textureMeta(addon, edds) {
  const file = join(ROOT, `${edds}.meta`);
  const rel = edds.slice(addon.length + 1);
  if (existsSync(file)) return { guid: metaGuid(file), rel };
  const g = guid();
  const cls = (p) => `  TIFFResourceClass ${p} : "{${CONF[p]}}Configs/System/ResourceTypes/${p}/TextureUI.conf" {\n  }\n`;
  writeFileSync(file, `MetaFileClass {\n Name "{${g}}${rel}"\n Configurations {\n${cls("PC")}${cls("XBOX_ONE")}${cls("XBOX_SERIES")}${cls("PS4")}  TIFFResourceClass PS5 : PC {\n  }\n${cls("HEADLESS")} }\n}\n`);
  return { guid: g, rel };
}

function imagesetMeta(addon, imageset) {
  const file = join(ROOT, `${imageset}.meta`);
  if (existsSync(file)) return;
  const rel = imageset.slice(addon.length + 1);
  writeFileSync(file, `MetaFileClass {\n Name "{${guid()}}${rel}"\n Configurations {\n  ImageSetResourceClass PC {\n  }\n  ImageSetResourceClass XBOX_ONE : PC {\n  }\n  ImageSetResourceClass XBOX_SERIES : PC {\n  }\n  ImageSetResourceClass PS4 : PC {\n  }\n  ImageSetResourceClass HEADLESS : PC {\n  }\n }\n}\n`);
}

function imagesetText(tex, names, pos) {
  const defs = names.map((n) => {
    const p = pos.get(n);
    return `  ImageSetDefClass ${n} {\n   Name "${n}"\n   Pos ${p.x} ${p.y}\n   Size ${p.w} ${p.h}\n   Flags 0\n  }\n`;
  }).join("");
  return `ImageSetClass {\n RefSize ${ATLAS} ${ATLAS}\n Textures {\n  ImageSetTextureClass {\n   mpix 1\n   path "{${tex.guid}}${tex.rel}"\n  }\n }\n Images {\n${defs} }\n}`;
}

async function contact(side, a) {
  const out = join(ROOT, "tmp", "map", `icons-${side}.png`);
  mkdirSync(dirname(out), { recursive: true });
  const art = await sharp(a.data, { raw: { width: ATLAS, height: ATLAS, channels: 4 } }).png().toBuffer();
  const flat = await sharp({ create: { width: ATLAS, height: ATLAS, channels: 4, background: INK.paper } }).composite([{ input: art }]).png().toBuffer();
  await sharp(flat).resize(ATLAS * 2, ATLAS * 2, { kernel: "nearest" }).png().toFile(out);
  return out;
}

async function main() {
  const batch = [];
  for (const side of ["core", "pack"]) {
    const f = FILES[side];
    const a = await atlas(side);
    const previewOnly = process.argv.includes("--preview-only");
    const png = await contact(side, a);
    console.log(`${side}: ${a.names.length} quads, contact sheet ${png}`);
    if (previewOnly) continue;
    await tif(join(ROOT, f.atlas.replace(/\.edds$/, ".tif")), a.data, ATLAS, ATLAS);
    const tex = textureMeta(f.addon, f.atlas);
    writeFileSync(join(ROOT, f.imageset), imagesetText(tex, a.names, a.pos));
    imagesetMeta(f.addon, f.imageset);
    batch.push(f.atlas.replace(/\.edds$/, ".tif"));
    const kinds = side === "core" ? CORE_HATCH : PACK_HATCH;
    for (let i = 0; i < kinds.length; i++) {
      const h = hatchTile(kinds[i]);
      await tif(join(ROOT, f.hatch[i].replace(/\.edds$/, ".tif")), h.data, h.w, h.h);
      textureMeta(f.addon, f.hatch[i]);
      batch.push(f.hatch[i].replace(/\.edds$/, ".tif"));
    }
  }
  if (batch.length) {
    writeFileSync(join(ROOT, "tools", "map", "icons", "icons-batch.txt"), `${batch.join("\n")}\n`);
    console.log(`SUMMARY map-icons ok textures=${batch.length} batch=tools/map/icons/icons-batch.txt (next: tools\\cli ui-image --batch tools/map/icons/icons-batch.txt on a Workbench pool machine)`);
  }
}

await main();
void SHARED;
