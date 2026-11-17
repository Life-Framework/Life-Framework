// tools/buildings/drugstore/budget.mjs - holds the drug store's furniture prefab to the medium building budget
// (docs/architecture/performance.md, #2201 / #2329): at most 300 entities, 6 lights, 1 shadow caster, no point light
// casting, CastShadow written on every light.
//
//   node tools/buildings/drugstore/budget.mjs            rewrite the furniture prefab
//   node tools/buildings/drugstore/budget.mjs --check    exit 1 when the prefab differs from what this gives
//   node tools/buildings/drugstore/budget.mjs --count    print what each step cuts
//
// It runs over the prefab polish2.mjs left (that script is a record, not a rebuild tool) and gives the same bytes
// on a second run, so it is also the proof the prefab is in budget form. It never touches a child it does not name:
//   1. attic and fittings: the attic (children at y >= 7, the flat's top floor), all light switches and radiators go.
//      The flat's bones (bed, kitchen, table, chairs, wardrobes, bath) and every ground-floor fixture stay.
//   2. thin dressing: shelf bottles, notebooks, cartons, tool cases, buckets and trays on the ground floor keep 60 %
//      of their 0.5 m cells, so a stack stays whole.
//   3. lights: the twelve lights become six, each with CastShadow written. One spot over the counter casts; the rest
//      do not. The two ambient points and their upward spots become a single upward spot (ceiling fill).
import { readFileSync, writeFileSync } from "node:fs";
import { createHash } from "node:crypto";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const here = dirname(fileURLToPath(import.meta.url));
const FILE = join(here, "../../../addons/LF_Everon1989/Prefabs/Buildings/Drugstore/Furniture/ShopHouse_E_2I01t_furniture_drugstore_lf_01.et");
const check = process.argv.includes("--check");
const count = process.argv.includes("--count");

const ATTIC_Y = 7;
const FLOOR_Y = 4; // below this is the shop; the flat starts at 4.299
const PRUNE = /\/(LightSwitch_02|RadiatorCastIron_01_\d+)\.et$/;
// Dressing on the ground floor keeps 60 % of its cells. The pick is one hash of the 0.5 m cell the child stands in,
// the same for every class, so a stack (a carton on a carton, a pen on a notebook) is kept or cut whole and nothing
// is left floating (`building check`).
const KEEP = 0.6;
const THIN = /\/(BeerBottle_01_empty_brown|BeerBottle_01_brown|Notebook_01_Single_Blue|CardboardBox_0[1256789]|ToolBox_01_grey|Bucket_02|MarsBox_01_white|Pen_01_base)\.et$/;

// The six lights, by the ID the prefab already used. Down-spots sit under the vanilla fluorescents (y 3.9).
const SPOT = { Color: "0.9 0.96 1 1", ClipEV: -10, Type: "Spot", NearPlane: 0.3, SpotAngle: 140, LensFlareType: "Disabled" };
const LIGHTS = [
  { id: "1AD1C720C7E814D9", coords: "2.2 3.7 0.9", angles: "-90 0 0", LV: 9, Radius: 7, shadow: 1, ...SPOT }, // over the counter: the one shadow
  { id: "811AEC498CCC281D", coords: "2.3 3.7 -1.7", angles: "-90 0 0", LV: 8, Radius: 7, shadow: 0, ...SPOT }, // back of the shop
  { id: "1FC5AFC6DEF0A438", coords: "2.1 3.7 4.1", angles: "-90 0 0", LV: 7.5, Radius: 7, shadow: 0, ...SPOT }, // mid shop
  { id: "A73D2CB48B90EE1F", coords: "2.3 3.7 6.6", angles: "-90 0 0", LV: 9, Radius: 7.5, shadow: 0, ...SPOT }, // front shop and window display (was four lights)
  { id: "C2D62AC5128977CE", coords: "4.9 3.55 8.4", angles: "-90 0 0", Color: "1 0.95 0.85 1", LV: 5, ClipEV: -10, Type: "Spot", Radius: 5, NearPlane: 0.3, SpotAngle: 120, LensFlareType: "Disabled", shadow: 0 }, // frontage, lights the pavement
  { id: "E63CBAFB4923848D", coords: "2.2 3.2 4.5", angles: "90 0 0", Color: "0.95 0.95 1 1", LV: 2.5, ClipEV: -10, Type: "Spot", Radius: 9, NearPlane: 0.35, SpotAngle: 150, LensFlareType: "Disabled", shadow: 0 }, // upward, so the night ceiling is not black
];
const lightBlock = (l) => {
  const o = ["  LightEntity {", `   ID "${l.id}"`, `   coords ${l.coords}`, `   angles ${l.angles}`, `   Color ${l.Color}`, `   LV ${l.LV}`, `   ClipEV ${l.ClipEV}`, `   CastShadow ${l.shadow}`];
  o.push(`   Type ${l.Type}`, `   Radius ${l.Radius}`, `   NearPlane ${l.NearPlane}`, `   SpotAngle ${l.SpotAngle}`, `   LensFlareType ${l.LensFlareType}`, "  }");
  return o;
};

const text = readFileSync(FILE, "utf8");
const lines = text.split("\n");
const head = [];
const blocks = [];
let cur = null;
for (const l of lines) {
  if (/^  \S.* \{$/.test(l)) { cur = [l]; blocks.push(cur); continue; }
  if (cur) { cur.push(l); if (l === "  }") cur = null; continue; }
  if (blocks.length === 0) head.push(l);
}
let lastEnd = -1;
for (let i = 0; i < lines.length; i++) if (lines[i] === "  }") lastEnd = i;
const tail = lines.slice(lastEnd + 1);

const body = (b) => b.join("\n");
const pathOf = (b) => /^  \w+ : "\{\w+\}([^"]+)"/.exec(b[0])?.[1] ?? null;
const idOf = (b) => /ID "(\w+)"/.exec(body(b))?.[1] ?? "";
const coordsOf = (b) => { const m = /^   coords (\S+) (\S+) (\S+)$/m.exec(body(b)); return m ? [+m[1], +m[2], +m[3]] : null; };
const frac = (s) => parseInt(createHash("sha1").update(s).digest("hex").slice(0, 8), 16) / 0x100000000;
const isLight = (b) => b[0].startsWith("  LightEntity");

const cut = { attic: 0, fittings: 0, thinned: 0, lights: 0 };
const kept = [];
const placed = new Set();
for (const b of blocks) {
  if (isLight(b)) {
    const id = idOf(b);
    const want = LIGHTS.find((l) => l.id === id);
    if (!want) { cut.lights++; continue; }
    placed.add(id);
    kept.push(lightBlock(want));
    continue;
  }
  const p = pathOf(b);
  const c = coordsOf(b);
  if (p && PRUNE.test(p)) { cut.fittings++; continue; }
  if (c && c[1] >= ATTIC_Y) { cut.attic++; continue; }
  if (p && c && c[1] < FLOOR_Y) {
    if (THIN.test(p) && frac(`drugstore-budget|${Math.floor(c[0] * 2)}|${Math.floor(c[2] * 2)}`) >= KEEP) { cut.thinned++; continue; }
  }
  kept.push(b);
}
const missing = LIGHTS.filter((l) => !placed.has(l.id));
for (const l of missing) kept.push(lightBlock(l));

const out = [...head, ...kept.flat(), ...tail].join("\n");
if (count) console.log(`cut: attic ${cut.attic}, switches and radiators ${cut.fittings}, thinned ${cut.thinned}, lights dropped ${cut.lights}; children ${blocks.length} -> ${kept.length}`);
if (check) {
  if (out !== text) { console.error("drugstore budget: the furniture prefab differs; run node tools/buildings/drugstore/budget.mjs"); process.exit(1); }
  console.log("drugstore budget: ok");
} else {
  writeFileSync(FILE, out);
  console.log(`drugstore budget: wrote ${kept.length} children (${blocks.length} before)`);
}
