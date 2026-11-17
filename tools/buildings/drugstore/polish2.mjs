// Drug store second polish pass (2026-11-16): run once over the furniture prefab the first pass wrote by hand
// (there was no generator). It is kept as the record of what changed, not as a rebuild tool: running it again
// would swap the swaps. Usage: node tools/buildings/drugstore/polish2.mjs [--dry]
//
//   1. generic shelf clutter (soda bottles, alarm clocks, tape recorders, small radios, buckets) becomes
//      amber medicine bottles, scaled cartons, grey first-aid cases and white trays, in quiet colours;
//   2. the free gondola boards, gondola spines (tops) and the gondola ends are dressed (sold slots untouched);
//   3. ceiling fill: upward spots over three fluorescents so the night ceiling is not black;
//   4. a broom and bucket by the door.
// Placement numbers come from `tools\cli measure ShelfShop_01 --surfaces` (boards at 0.627, 1.019, 1.376 above
// the 0.199 floor; sold slots sit on 1.019 and 1.376 and, on the tourniquet face, 0.627).
import { readFileSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const here = dirname(fileURLToPath(import.meta.url));
const FILE = join(here, "../../../addons/LF_Everon1989/Prefabs/Buildings/Drugstore/Furniture/ShopHouse_E_2I01t_furniture_drugstore_lf_01.et");
const dry = process.argv.includes("--dry");

const P = {
  bottle: "{8963CD1DC1CDB614}Prefabs/Props/Civilian/BeerBottle_01_brown.et",
  bottleEmpty: "{18982CB5D0D29247}Prefabs/Props/Civilian/BeerBottle_01_empty_brown.et",
  bottleGroup: "{8FE6D62204173656}Prefabs/Props/Civilian/BeerBottle_01_group.et",
  box01: "{1284F9C5D25E263E}Prefabs/Props/Crates/CardboardBox/CardboardBox_01.et",
  box02: "{2BEA74B1FA12B7C4}Prefabs/Props/Crates/CardboardBox/CardboardBox_02.et",
  box05: "{16DE51898F7E3EF3}Prefabs/Props/Crates/CardboardBox/CardboardBox_05.et",
  box06: "{7A8A656933BAB1DD}Prefabs/Props/Crates/CardboardBox/CardboardBox_06.et",
  box07: "{9F1629903F5F26B6}Prefabs/Props/Crates/CardboardBox/CardboardBox_07.et",
  box08: "{00E22F19D963A3B3}Prefabs/Props/Crates/CardboardBox/CardboardBox_08.et",
  box09: "{E57E63E0D58634D8}Prefabs/Props/Crates/CardboardBox/CardboardBox_09.et",
  caseGrey: "{0F866B72D473D8C4}Prefabs/Props/Industrial/ToolBox_01/ToolBox_01_grey.et",
  trayWhite: "{46A95C6081A28586}Prefabs/Props/Industrial/Pallets/MarsBox_01_white.et",
  papers: "{876B096F32584485}Prefabs/Props/Civilian/FileOffice_01_Stack.et",
  tub: "{D0B44EB8DBEDDA97}Prefabs/Props/Civilian/Bucket_02.et",
  bucket: "{BCE07A58672955B9}Prefabs/Props/Civilian/Bucket_01.et",
  broom: "{9C231B6AC1BD0A26}Prefabs/Props/Civilian/Broom_01.et",
  basket: "{D5C3CF5996B43A27}Prefabs/Props/Civilian/BasketWicker_01.et",
  plant1: "{4549BE7A1D414EDA}Prefabs/Props/Furniture/PlantRoom_01.et",
  plant4: "{148E2BA12B48CFBA}Prefabs/Props/Furniture/PlantRoom_04.et",
  extinguisher: "{BD65F1D86388640D}Prefabs/Props/Civilian/FireExtinguisher_01.et",
};
const GUID_OF = (path) => /^\{(\w+)\}/.exec(path)[1];

let seed = 20261116;
const rnd = () => ((seed = (seed * 1664525 + 1013904223) >>> 0) / 4294967296);
const pick = (a) => a[Math.floor(rnd() * a.length)];
const usedIds = new Set();
const newId = () => {
  for (;;) {
    let s = "E6";
    for (let i = 0; i < 14; i++) s += "0123456789ABCDEF"[Math.floor(rnd() * 16)];
    if (!usedIds.has(s)) { usedIds.add(s); return s; }
  }
};

// a small, quiet-colour carton or case for a shelf board; scale keeps it under the 0.39 m board gap
const CARTONS = [P.box01, P.box02, P.box05, P.box06, P.box07, P.box08, P.box09];
function shelfThing(kind) {
  const r = rnd();
  if (kind === "bottle") return r < 0.7 ? { path: P.bottle } : r < 0.9 ? { path: P.bottleEmpty } : { path: pick(CARTONS), scale: 0.42 };
  if (kind === "small") { // was an alarm clock: a low item
    if (r < 0.7) return { path: pick(CARTONS), scale: 0.38 + rnd() * 0.06 };
    return { path: P.bottleEmpty };
  }
  if (kind === "case") return r < 0.6 ? { path: P.caseGrey, scale: 0.55 } : { path: pick(CARTONS), scale: 0.45 };
  if (kind === "radio") return r < 0.5 ? { path: P.trayWhite, scale: 0.28 } : { path: pick(CARTONS), scale: 0.5 };
  if (kind === "bucket") return { path: P.tub, scale: 0.5 };
  return null;
}

const text = readFileSync(FILE, "utf8");
const lines = text.split("\n");
// split the body into entity blocks (two-space-indented, the children of the root)
const head = [];
const blocks = [];
let cur = null;
for (let i = 0; i < lines.length; i++) {
  const l = lines[i];
  if (/^  \S.* \{$/.test(l)) { cur = [l]; blocks.push(cur); continue; }
  if (cur) { cur.push(l); if (l === "  }") cur = null; continue; }
  if (blocks.length === 0) head.push(l);
  else if (/^ \}$/.test(l) || /^\}$/.test(l) || l === "") { /* tail handled below */ }
}
// tail: everything after the last block
const lastBlockEnd = (() => { let idx = -1; for (let i = 0; i < lines.length; i++) if (lines[i] === "  }") idx = i; return idx; })();
const tail = lines.slice(lastBlockEnd + 1);
for (const b of blocks) { const m = /ID "(\w+)"/.exec(b.join("\n")); if (m) usedIds.add(m[1]); }

const coordsOf = (b) => { const m = /^   coords (\S+) (\S+) (\S+)$/m.exec(b.join("\n")); return m ? [+m[1], +m[2], +m[3]] : null; };
const angleOf = (b) => { const m = /^   angleY (\S+)$/m.exec(b.join("\n")); return m ? +m[1] : 0; };
const pathOf = (b) => { const m = /^  \w+ : "\{(\w+)\}([^"]+)"/.exec(b[0]); return m ? m[2] : null; };

function make(path, [x, y, z], ay = 0, scale = 0, ent = "GenericEntity") {
  const o = [`  ${ent} : "${path}" {`, `   ID "${newId()}"`, `   coords ${x.toFixed(3)} ${y.toFixed(3)} ${z.toFixed(3)}`];
  if (ay) o.push(`   angleY ${ay.toFixed(3).replace(/\.?0+$/, "")}`);
  if (scale && scale !== 1) o.push(`   scale ${scale.toFixed(3).replace(/\.?0+$/, "")}`);
  o.push("  }");
  return o;
}

// 1. swap the generic clutter on the ground floor
const KIND = {
  "Prefabs/Props/Civilian/SodaBottle_01.et": "bottle",
  "Prefabs/Props/Civilian/AlarmClock_01/AlarmClock_01_green_V1.et": "small",
  "Prefabs/Props/Civilian/AlarmClock_01/AlarmClock_01_red_V1.et": "small",
  "Prefabs/Props/Civilian/AlarmClock_01/AlarmClock_01_blue_V1.et": "small",
  "Prefabs/Props/Civilian/TapeRecorder_01/TapeRecorder_01.et": "case",
  "Prefabs/Props/Civilian/RadioSmall_01_red.et": "radio",
  "Prefabs/Props/Civilian/RadioSmall_01_black.et": "radio",
  "Prefabs/Props/Civilian/Bucket_01.et": "bucket",
};
let swapped = 0;
for (let bi = 0; bi < blocks.length; bi++) {
  const b = blocks[bi];
  const kind = KIND[pathOf(b)];
  if (!kind) continue;
  const c = coordsOf(b);
  if (!c || c[1] > 3.4) continue; // upstairs stays
  if (Math.abs(c[0] - 2.1) < 0.01 && Math.abs(c[1] - 1.238) < 0.01) continue; // the radio on the till counter
  if (kind === "bucket" && c[1] < 0.35 && c[2] > 7) continue;
  const thing = shelfThing(kind);
  const ay = angleOf(b);
  blocks[bi] = make(thing.path, c, ay, thing.scale);
  swapped++;
}

// 2. gondola boards, spines and ends
const add = [];
const xs = (from, to, step) => { const o = []; for (let x = from; x <= to + 1e-6; x += step) o.push(x + (rnd() - 0.5) * 0.04); return o; };
function row(y, z, ay, x0, x1, step, mix) {
  for (const x of xs(x0, x1, step)) {
    const t = shelfThing(mix);
    add.push(make(t.path, [x, y, z + (rnd() - 0.5) * 0.04], ay + (rnd() - 0.5) * 14, t.scale));
  }
}
// gondola 1 (z 5.4): both faces, board at 0.627 (mid z 5.674 / 5.106)
row(0.627, 5.674, 0, 1.2, 3.1, 0.2, "small");
row(0.627, 5.106, 180, 1.2, 3.1, 0.2, "small");
// gondola 2 (z 2.9): +z face board at 0.627 (z 3.2); -z face board at 1.376 (z 2.707)
row(0.627, 3.2, 0, 1.2, 3.1, 0.2, "small");
row(1.376, 2.707, 180, 1.25, 3.05, 0.3, "small");
// the spines (tops): a run of cartons and bottle groups straddling the rail
for (const z of [5.39, 2.916]) {
  let x = 1.15;
  while (x < 3.15) {
    const r = rnd();
    const t = r < 0.75 ? { path: pick(CARTONS), scale: 0.38 + rnd() * 0.1 } : { path: P.caseGrey, scale: 0.5 };
    add.push(make(t.path, [x, 1.63, z], 90 + (rnd() - 0.5) * 30, t.scale));
    x += 0.3 + rnd() * 0.12;
  }
}
// the ends: stacked cartons at the west ends, a basket and a plant at the east ends
for (const z of [5.4, 2.9]) {
  add.push(make(P.box02, [0.62, 0.199, z], 90, 0.8));
  add.push(make(P.box07, [0.62, 0.199 + 0.345, z], 85, 0.75));
}
add.push(make(P.basket, [3.64, 0.199, 5.4], 0, 0.9));
add.push(make(P.plant1, [3.64, 0.199, 2.9], 30));

// 4. a broom and a bucket inside the front wall, on the pier between the arched shop door (west end) and the window
add.push(make(P.broom, [0.55, 0.199, 7.8], 0));
add.push(make(P.bucket, [0.8, 0.199, 7.65], 0));

// 3. ceiling fill: the vanilla LightCeiling_01_on light pair (a soft ambient point and an upward spot) without its
// chandelier mesh, twice, between the fluorescents. Their own spots point down and left the ceiling black at night.
// (An ambient point at LV 1.6 made the shell vanish in the night look; the vanilla LV 0.25 is what lights it.)
for (const [x, z] of [[2.2, 2.6], [2.2, 6.4]]) {
  add.push([
    "  LightEntity {", `   ID "${newId()}"`, `   coords ${x} 3.4 ${z}`,
    "   Color 0.95 0.95 1 1", "   LV 0.25", "   ClipEV -10", "   Radius 10", "   NearPlane 0.45", "   DistanceAttenuation 0.8", "  }",
  ]);
  add.push([
    "  LightEntity {", `   ID "${newId()}"`, `   coords ${x} 3.2 ${z}`, "   angles 90 0 0",
    "   Color 0.95 0.95 1 1", "   LV 2", "   ClipEV -10", "   Type Spot", "   Radius 8", "   NearPlane 0.35", "   SpotAngle 150", "   LensFlareType Disabled", "  }",
  ]);
}

const out = [...head, ...blocks.flat(), ...add.flat(), ...tail].join("\n");
console.log(`swapped ${swapped}, added ${add.length} entities`);
if (!dry) writeFileSync(FILE, out);
