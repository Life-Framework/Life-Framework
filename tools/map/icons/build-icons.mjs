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
  ATLAS_SIZE, CELL, CATEGORIES, CORE_HATCH, PACK_HATCH, FILES, PACK_KINDS, SHARED,
  catQuad, coreQuadNames, packQuadNames, poiQuad, layout,
} from "../lib/icons.mjs";

const ROOT = join(dirname(fileURLToPath(import.meta.url)), "..", "..", "..");
const sharp = createRequire(join(process.env.LF_SHARP_FROM || join(ROOT, "website"), "package.json"))("sharp");

// ---- the 1989 inks -----------------------------------------------------------------------
export const INK = {
  black: "#1d1a17", red: "#b3261e", blue: "#1f4e8c", green: "#2f6b34", brown: "#6b4423",
  graphite: "#46464e", grey: "#555555", core: "#3d3d3d", paper: "#efe6d0", badge: "#fffaf0",
};

// ---- shapes (64x64 viewBox) ----------------------------------------------------------------
const star = (cx, cy, R, r) => Array.from({ length: 10 }, (_, i) => {
  const a = -Math.PI / 2 + (i * Math.PI) / 5;
  const d = i % 2 ? r : R;
  return `${(cx + d * Math.cos(a)).toFixed(1)},${(cy + d * Math.sin(a)).toFixed(1)}`;
}).join(" ");
// Stroke weight factor: 1 for the core glyphs and the hand marks, BADGE_STROKE inside a pack badge, where the
// glyph is drawn smaller and needs a heavier line to read at 22 px.
let STROKE = 1;
const S = (ink, w = 4) => `stroke="${ink}" stroke-width="${(w * STROKE).toFixed(2)}" fill="none" stroke-linecap="round" stroke-linejoin="round"`;

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
  idcard: (k) => `<rect x="6" y="14" width="52" height="36" rx="4" ${S(k)}/><circle cx="21" cy="27" r="5" ${S(k, 3)}/><path d="M13 42Q21 33 29 42" ${S(k, 3)}/><path d="M37 25H52M37 33H52M37 41H47" ${S(k, 3)}/>`,
  atm: (k) => `<rect x="10" y="8" width="44" height="30" rx="3" ${S(k)}/><rect x="16" y="14" width="32" height="12" fill="${k}"/><path d="M8 46H56" ${S(k, 5)}/><path d="M26 38V56H38V38" ${S(k, 3)}/>`,
  garage: (k) => `<path d="M8 54V24L32 8L56 24V54" ${S(k)}/><path d="M18 54V32H46V54M18 40H46M18 47H46" ${S(k, 3)}/>`,
  bed: (k) => `<path d="M8 18V54M8 44H56V54M8 34H56V44" ${S(k)}/><rect x="14" y="25" width="14" height="9" fill="${k}"/>`,
  sawmill: (k) => `<path d="M8 22L13 12L18 22L23 12L28 22L33 12L38 22L43 12L48 22L53 12L56 22" ${S(k, 3)}/><path d="M8 42Q8 30 20 30H46Q56 30 56 42Q56 54 46 54H20Q8 54 8 42Z" ${S(k)}/><circle cx="46" cy="42" r="6" ${S(k, 3)}/>`,
  scrap: (k) => `<path d="M12 22H52L48 56H16Z" ${S(k)}/><path d="M8 22H56M26 22V12H38V22M24 32V48M32 32V48M40 32V48" ${S(k, 3)}/>`,
  // One glyph per place type (2026-11-22), drawn inside the pack badge.
  basket: (k) => `<path d="M8 28H56L50 54H14Z" ${S(k)}/><path d="M20 28L28 12M44 28L36 12" ${S(k)}/><path d="M24 36V46M32 36V46M40 36V46" ${S(k, 3)}/>`,
  bread: (k) => `<path d="M8 46V38Q8 20 32 20Q56 20 56 38V46Z" ${S(k)}/><path d="M22 28L26 38M32 27V38M42 28L38 38" ${S(k, 3)}/>`,
  cleaver: (k) => `<path d="M6 14H40V44H6Z" fill="${k}"/><path d="M40 22H58" ${S(k, 8)}/>`,
  cup: (k) => `<path d="M10 24H46V38Q46 52 28 52Q10 52 10 38Z" ${S(k)}/><path d="M46 28H51Q57 28 57 34Q57 41 50 41H45" ${S(k)}/><path d="M20 8Q25 13 20 18M32 8Q37 13 32 18" ${S(k, 3)}/><path d="M8 58H48" ${S(k)}/>`,
  stall: (k) => `<path d="M6 24L12 10H52L58 24Z" fill="${k}"/><path d="M12 28V56M52 28V56M6 56H58M16 42H48" ${S(k)}/>`,
  paper: (k) => `<rect x="12" y="8" width="40" height="48" rx="2" ${S(k)}/><path d="M19 18H45" ${S(k, 6)}/><path d="M19 30H29M19 38H29M19 46H29" ${S(k, 3)}/><rect x="35" y="29" width="10" height="18" fill="${k}"/>`,
  shirt: (k) => `<path d="M22 8L8 16L4 30L15 33L16 28V56H48V28L49 33L60 30L56 16L42 8Q32 18 22 8Z" ${S(k)}/>`,
  hammer: (k) => `<path d="M12 10H38L48 18V26H12Z" fill="${k}"/><path d="M28 26V58" ${S(k, 8)}/>`,
  rifle: (k) => `<path d="M4 30H40L46 26H60V34L50 38H34L28 52H18L22 38H4Z" fill="${k}"/>`,
  target: (k) => `<circle cx="32" cy="32" r="25" ${S(k)}/><circle cx="32" cy="32" r="15" ${S(k)}/><circle cx="32" cy="32" r="5.5" fill="${k}"/>`,
  capsule: (k) => `<g transform="rotate(-45 32 32)"><rect x="8" y="21" width="48" height="22" rx="11" ${S(k)}/><path d="M12 32Q12 25 19 25H32V39H19Q12 39 12 32Z" fill="${k}"/></g>`,
  scissors: (k) => `<circle cx="18" cy="47" r="8" ${S(k)}/><circle cx="46" cy="47" r="8" ${S(k)}/><path d="M23 41L50 6M41 41L14 6" ${S(k)}/>`,
  chair: (k) => `<path d="M18 6V58M18 34H46V58M18 22H34" ${S(k)}/>`,
  wheat: (k) => `<path d="M32 60V8" ${S(k)}/><ellipse cx="24" cy="14" rx="4.5" ry="8" transform="rotate(-35 24 14)" fill="${k}"/><ellipse cx="40" cy="14" rx="4.5" ry="8" transform="rotate(35 40 14)" fill="${k}"/><ellipse cx="24" cy="26" rx="4.5" ry="8" transform="rotate(-35 24 26)" fill="${k}"/><ellipse cx="40" cy="26" rx="4.5" ry="8" transform="rotate(35 40 26)" fill="${k}"/><ellipse cx="24" cy="38" rx="4.5" ry="8" transform="rotate(-35 24 38)" fill="${k}"/><ellipse cx="40" cy="38" rx="4.5" ry="8" transform="rotate(35 40 38)" fill="${k}"/>`,
  boxes: (k) => `<rect x="6" y="34" width="24" height="22" ${S(k)}/><rect x="34" y="34" width="24" height="22" ${S(k)}/><rect x="20" y="10" width="24" height="22" ${S(k)}/>`,
  appletree: (k) => `<path d="M32 18Q20 10 12 22Q4 38 18 54Q26 60 32 54Q38 60 46 54Q60 38 52 22Q44 10 32 18Z" fill="${k}"/><path d="M32 18Q31 10 36 5" ${S(k, 4)}/><path d="M36 12Q46 4 52 10Q44 16 36 12Z" fill="${k}"/>`,
  pine: (k) => `<path d="M32 4L50 30H41L56 50H8L23 30H14Z" fill="${k}"/><path d="M32 50V61" ${S(k, 7)}/>`,
  pickaxe: (k) => `<g transform="rotate(-35 32 32)"><path d="M6 22Q32 6 58 22" ${S(k, 7)}/><path d="M32 14V62" ${S(k, 6)}/></g>`,
  factory: (k) => `<path d="M6 58V30L20 38V30L34 38V30L46 38V8H56V58Z" ${S(k)}/><path d="M14 48H20M28 48H34M42 48H48" ${S(k)}/>`,
  smoked: (k) => `<path d="M8 44Q30 30 48 44Q30 58 8 44Z" fill="${k}"/><path d="M48 44L58 37V51Z" fill="${k}"/><path d="M20 28Q15 22 20 16M32 26Q27 19 32 10M44 28Q39 22 44 16" ${S(k, 3)}/>`,
  tractor: (k) => `<circle cx="20" cy="44" r="12" ${S(k)}/><circle cx="20" cy="44" r="3" fill="${k}"/><circle cx="50" cy="50" r="6" ${S(k)}/><path d="M12 30V12H30L34 30H56V44H40" ${S(k)}/><path d="M46 30V20" ${S(k, 5)}/>`,
  sack: (k) => `<path d="M18 18Q12 36 14 48Q16 58 32 58Q48 58 50 48Q52 36 46 18Z" ${S(k)}/><path d="M18 18Q32 10 46 18" ${S(k)}/><path d="M25 8L32 15L39 8" ${S(k, 3)}/><path d="M22 38H42M24 46H40" ${S(k, 3)}/>`,
  fork: (k) => `<path d="M32 60V28" ${S(k, 5)}/><path d="M16 6V20Q16 28 32 28Q48 28 48 20V6M32 6V28" ${S(k)}/>`,
  drum: (k) => `<path d="M16 12Q16 8 32 8Q48 8 48 12V52Q48 56 32 56Q16 56 16 52Z" ${S(k)}/><path d="M16 24Q32 28 48 24M16 40Q32 44 48 40" ${S(k, 3)}/><circle cx="40" cy="14" r="2.6" fill="${k}"/>`,
  trolley: (k) => `<path d="M4 12H14L20 42H50L57 20H16" ${S(k)}/><path d="M24 26V36M34 26V36M44 26V36" ${S(k, 3)}/><circle cx="24" cy="53" r="5" fill="${k}"/><circle cx="46" cy="53" r="5" fill="${k}"/>`,
  planks: (k) => `<rect x="6" y="12" width="52" height="11" rx="2" ${S(k, 3.4)}/><rect x="6" y="27" width="52" height="11" rx="2" ${S(k, 3.4)}/><rect x="6" y="42" width="52" height="11" rx="2" ${S(k, 3.4)}/><circle cx="12" cy="17.5" r="2" fill="${k}"/><circle cx="12" cy="32.5" r="2" fill="${k}"/><circle cx="12" cy="47.5" r="2" fill="${k}"/>`,
  axe: (k) => `<rect x="6" y="44" width="40" height="12" rx="6" ${S(k, 3.4)}/><path d="M24 40L50 10" ${S(k, 5)}/><path d="M42 6L58 4Q60 18 52 24L44 16Z" fill="${k}"/>`,
  bus: (k) => `<rect x="8" y="10" width="48" height="38" rx="6" ${S(k)}/><path d="M8 30H56M22 10V30M42 10V30" ${S(k, 3)}/><circle cx="20" cy="54" r="5" fill="${k}"/><circle cx="44" cy="54" r="5" fill="${k}"/>`,
  rocks: (k) => `<path d="M4 56L16 34L28 42L40 18L60 56Z" ${S(k)}/><path d="M16 34L22 56M40 18L36 56" ${S(k, 3)}/>`,
  board: (k) => `<rect x="6" y="8" width="52" height="36" rx="2" ${S(k)}/><path d="M16 44V60M48 44V60" ${S(k)}/><path d="M14 18H36M14 26H44M14 34H30" ${S(k, 3)}/>`,
  house: (k) => `<path d="M6 30L32 8L58 30" ${S(k)}/><path d="M14 25V56H50V25" ${S(k)}/><rect x="25" y="36" width="14" height="20" fill="${k}"/>`,
  car: (k) => `<path d="M6 42V34L16 22H44L56 34H58V42Z" fill="${k}"/><circle cx="18" cy="46" r="7" fill="${k}"/><circle cx="46" cy="46" r="7" fill="${k}"/>`,
  crate: (k) => `<rect x="10" y="14" width="44" height="40" ${S(k)}/><path d="M10 14L54 54M54 14L10 54" ${S(k, 3)}/>`,
};
// ---- the badge: every pack pictogram is the same disc, rim and line weight; the group says the colour -----
// (map rework 2026-11-22, "clean nice designed icons for each type of place"). Muted print inks, no neon.
const GROUP_FILL = {
  civic: "#1f9d9a", money: "#e0a00c", shop: "#2fa04a", food: "#ec7a1f", law: "#2d4fd0", health: "#e0352f",
  travel: "#3a9bd9", work: "#b87a34", home: "#8a5fd0", story: "#d0388c", underworld: "#6b5b8a",
};
const BADGE_STROKE = 1.4;
const BADGE_SCALE = 0.72;
// Kind -> [glyph, group]. Every kind has its own glyph (2026-11-22: "each thing needs a different icon");
// validate-map-icon-unique fails a glyph two kinds share or a public type without its own badge.
const KIND_BADGE = {
  Bank: ["coin", "money"], Atm: ["atm", "money"], PoliceStation: ["star", "law"], PoliceArmoury: ["shield", "law"], Range: ["target", "law"],
  HospitalSite: ["clinic", "health"], Pharmacy: ["capsule", "health"], Fire: ["flame", "health"],
  TownHall: ["hall", "civic"], IdCard: ["idcard", "civic"], Church: ["church", "civic"], Payphone: ["handset", "civic"],
  Depot: ["crate", "civic"], Board: ["board", "civic"],
  Shop: ["bag", "shop"], News: ["paper", "shop"], Clothing: ["shirt", "shop"], Hardware: ["hammer", "shop"], Guns: ["rifle", "shop"],
  Barber: ["scissors", "shop"], Furniture: ["chair", "shop"], FarmSupply: ["fork", "shop"], Wholesale: ["boxes", "shop"],
  SeedFeed: ["sack", "shop"], TimberMerchant: ["planks", "shop"],
  Grocer: ["basket", "food"], Bakery: ["bread", "food"], Butcher: ["cleaver", "food"], Cafe: ["cup", "food"], Pub: ["mug", "food"], Market: ["stall", "food"],
  Supermarket: ["trolley", "food"],
  Fuel: ["pump", "travel"], Garage: ["garage", "travel"], CarLot: ["car", "travel"], Taxi: ["taxi", "travel"], Harbour: ["anchor", "travel"], Bed: ["bed", "travel"],
  FuelDepot: ["drum", "travel"], BusStop: ["bus", "travel"],
  Workplace: ["hammerpick", "work"], Quarry: ["rocks", "work"], Mine: ["pickaxe", "work"], Plant: ["factory", "work"], Farm: ["barn", "work"],
  FarmYard: ["tractor", "work"], LoggingYard: ["axe", "work"],
  Orchard: ["appletree", "work"], Wheat: ["wheat", "work"], Woodlot: ["pine", "work"], Sawmill: ["sawmill", "work"], Scrap: ["scrap", "work"],
  Fish: ["fish", "work"], Smokehouse: ["smoked", "work"],
  PlotForSale: ["house", "home"], Story: ["flag", "story"], Fence: ["diamond", "underworld"],
};
function badge(glyph, group) {
  STROKE = BADGE_STROKE;
  const inner = SHAPES[glyph](INK.badge);
  STROKE = 1;
  // A light halo outside a dark rim: the disc pops on the grey map and on the yellow roads (2026-11-22, "near-black dots").
  return `<circle cx="32" cy="32" r="31" fill="#fffaf0"/>`
    + `<circle cx="32" cy="32" r="28.6" fill="${INK.black}"/>`
    + `<circle cx="32" cy="32" r="26.4" fill="${GROUP_FILL[group]}"/>`
    + `<g transform="translate(32 32) scale(${BADGE_SCALE}) translate(-32 -32)">${inner}</g>`;
}

const CAT_SHAPE = { civic: "hall", shop: "bag", work: "hammerpick", law: "star", health: "clinic", travel: "taxi", story: "flag", underworld: "diamond" };

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

// The pack's cluster stamp is a badge too (map clean-up 2026-11-22: "at wide zoom the clusters are still black-ring
// stamps"): the same halo and rim, a slate disc, three paper discs for "several places here"; the layer writes the count.
function clusterBadge() {
  const dot = (cx, cy) => `<circle cx="${cx}" cy="${cy}" r="7.2" fill="${INK.badge}" stroke="#3b4550" stroke-width="2"/>`;
  return `<circle cx="32" cy="32" r="31" fill="#fffaf0"/>`
    + `<circle cx="32" cy="32" r="28.6" fill="${INK.black}"/>`
    + `<circle cx="32" cy="32" r="26.4" fill="#4f5d6b"/>`
    + dot(32, 22) + dot(22, 38) + dot(42, 38);
}

function svgFor(side, name) {
  let body;
  if (side === "pack" && name === "stamp_town") body = clusterBadge();
  else if (FIXED[name]) body = FIXED[name]();
  else {
    const cat = CATEGORIES.find((c) => catQuad(c) === name);
    const kind = PACK_KINDS.find((k) => poiQuad(k) === name);
    if (side === "pack") {
      const spec = cat ? [CAT_SHAPE[cat], cat] : KIND_BADGE[kind];
      if (!spec || !SHAPES[spec[0]] || !GROUP_FILL[spec[1]]) throw new Error(`no badge for ${name}`);
      body = badge(spec[0], spec[1]);
    } else {
      if (!cat) throw new Error(`no core shape for ${name}`);
      body = SHAPES[CAT_SHAPE[cat]](INK.core);
    }
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
  const size = ATLAS_SIZE[side];
  const pos = layout(names, size);
  const layers = [];
  for (const n of names) {
    const input = await sharp(Buffer.from(svgFor(side, n)), { density: 144 }).resize(CELL, CELL).png().toBuffer();
    const p = pos.get(n);
    layers.push({ input, left: p.x, top: p.y });
  }
  const base = sharp({ create: { width: size, height: size, channels: 4, background: { r: 0, g: 0, b: 0, alpha: 0 } } });
  const { data } = await base.composite(layers).raw().toBuffer({ resolveWithObject: true });
  // The core placeholders keep their stamped print edge; the pack badges stay clean.
  if (side === "core") stamp(data, size, size, 1);
  return { names, pos, data, size };
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

function imagesetText(tex, names, pos, size) {
  const defs = names.map((n) => {
    const p = pos.get(n);
    return `  ImageSetDefClass ${n} {\n   Name "${n}"\n   Pos ${p.x} ${p.y}\n   Size ${p.w} ${p.h}\n   Flags 0\n  }\n`;
  }).join("");
  return `ImageSetClass {\n RefSize ${size} ${size}\n Textures {\n  ImageSetTextureClass {\n   mpix 1\n   path "{${tex.guid}}${tex.rel}"\n  }\n }\n Images {\n${defs} }\n}`;
}

async function contact(side, a) {
  const out = join(ROOT, "tmp", "map", `icons-${side}.png`);
  mkdirSync(dirname(out), { recursive: true });
  const art = await sharp(a.data, { raw: { width: a.size, height: a.size, channels: 4 } }).png().toBuffer();
  const flat = await sharp({ create: { width: a.size, height: a.size, channels: 4, background: "#9a9a9a" } }).composite([{ input: art }]).png().toBuffer();
  await sharp(flat).resize(a.size * 2, a.size * 2, { kernel: "nearest" }).png().toFile(out);
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
    await tif(join(ROOT, f.atlas.replace(/\.edds$/, ".tif")), a.data, a.size, a.size);
    const tex = textureMeta(f.addon, f.atlas);
    writeFileSync(join(ROOT, f.imageset), imagesetText(tex, a.names, a.pos, a.size));
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
