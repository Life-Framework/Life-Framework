// tools/buildings/supermarket/perf.mjs - the playtest supermarket's lights meet the medium building budget (#2330).
//
//   node tools/buildings/supermarket/perf.mjs [--check]
//
// The export stage (pipeline/lighting.mjs) gives every ceiling fixture its own spot LightEntity with CastShadow unset
// (12 spots) and the facade carries two vanilla `LightIndustrial_02_on_exterior` lamps (2 lights each, a point light
// that casts). That is 16 lights, 16 casters and 4 casting points against the budget's 6 / 1 / 0
// (docs/architecture/performance.md "The budget"). This pass runs on the exported furniture prefab, after the
// pipeline's export and hand-prop stages, and rewrites only the light rows:
//
//   - the 12 fixture meshes stay; their 12 spots become 5 wider ones with CastShadow 0: three in the sales hall
//     (between the fixture rows), two in the stockroom (between fixture pairs);
//   - the two facade lamps become the plain `LightIndustrial_02` mesh with the lit orange glass (`Glass_on_orange`),
//     so they still glow, and one warm point light by the door (CastShadow 0) lights the pavement and the glass;
//   - no light shadows: unshadowed spots measured no cost (performance.md), a shadowed spot is the one the budget would still allow.
//
// The same bytes twice: the rewrite finds its own rows by ID and recognises its own output; `--check` exits 1 when the
// file on disk differs from what this pass would write.
import { createHash } from "node:crypto";
import { existsSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";

const ROOT = join(dirname(fileURLToPath(import.meta.url)), "..", "..", "..");
export const FURNITURE = join(ROOT, "addons/LF_Everon1989/Prefabs/Buildings/Supermarket/Furniture/Supermarket_furniture_01.et");

const id = (kind) => createHash("sha1").update(`supermarket|perf|${kind}`).digest("hex").slice(0, 16).toUpperCase();

// Ceiling spots: the fixture meshes hang at y 3.649, the preset's lights 0.2 m under them. More light per spot
// (about -0.2 EV each, balanced by the overlap) makes up for 12 becoming 5.
const CEILING = { y: 3.45, color: "1 0.96 0.88 1", radius: 8, angle: 150 };
export const SPOTS = [
  { name: "hall-west", x: -4.025, z: -2.25, lv: 5.8 },
  { name: "hall-mid", x: 0.05, z: -2.25, lv: 5.8 },
  { name: "hall-east", x: 4.125, z: -2.25, lv: 5.8 },
  { name: "stock-west", x: -4.5125, z: 4.45, lv: 5.7 },
  { name: "stock-east", x: 5.0625, z: 4.45, lv: 5.7 },
];
// The facade glow: one point light in front of the door, no shadow.
export const GLOW = { x: 0, y: 2.7, z: -7.5, color: "1 0.78 0.5", lv: 8.4, radius: 7 };

const LAMP_BASE = "{8F17372BAB5A9CF8}Prefabs/Structures/BuildingParts/Lights/LightIndustrial_02.et";
const LAMP_ON = "{9FA626D6505E54F1}Prefabs/Structures/BuildingParts/Lights/LightIndustrial_02_on_exterior.et";
const GLASS_ON = "{0097A51162CDB55B}Assets/Structures/BuildingsParts/Lights/LampIndustrial_01/Data/Glass_on_orange.emat";

const lightBlock = (kind, o) =>
  [
    "  LightEntity {",
    `   ID "${id(kind)}"`,
    `   coords ${o.x} ${o.y} ${o.z}`,
    ...(kind === "glow" ? [] : ["   angles -90 0 0"]),
    `   Color ${o.color}`,
    `   LV ${o.lv}`,
    "   ClipEV -10",
    ...(kind === "glow" ? [] : ["   Type Spot"]),
    `   Radius ${o.radius}`,
    "   NearPlane 0.3",
    ...(kind === "glow" ? [] : [`   SpotAngle ${o.angle}`]),
    "   CastShadow 0",
    "   LensFlareType Disabled",
    "  }",
  ].join("\n");

const lampBlock = (entityId, coords) =>
  [
    `  GameEntity : "${LAMP_BASE}" {`,
    `   ID "${entityId}"`,
    "   components {",
    '    MeshObject "{5111E74D5E891458}" {',
    "     Materials {",
    '      MaterialAssignClass "{651C73C66D2E488B}" {',
    '       SourceMaterial "Glass"',
    `       AssignedMaterial "${GLASS_ON}"`,
    "      }",
    "     }",
    "    }",
    "   }",
    `   coords ${coords}`,
    "   angleY 180",
    "  }",
  ].join("\n");

// One child entity block at the prefab's child indent: header line to its closing `  }`.
const child = (header) => new RegExp(`^  ${header}\\n(?: {3,}[^\\n]*\\n)*?  \\}\\n`, "gm");

export function applyPerf(text) {
  let out = text;
  // 1. every light the export wrote (ceiling spots) or this pass wrote: dropped, then written again below
  const lights = [...out.matchAll(child("LightEntity \\{"))];
  const ceilingBefore = lights.filter((m) => !m[0].includes(`ID "${id("glow")}"`));
  if (!ceilingBefore.length) throw new Error("the furniture prefab has no ceiling LightEntity rows to replace; run the pipeline export first");
  out = out.replace(child("LightEntity \\{"), "");
  // 2. the facade lamps: the lit vanilla prefab becomes the plain mesh with lit glass (both the export's and ours)
  const coords = [];
  out = out.replace(child(`GameEntity : "(?:${LAMP_ON.replace(/[{}]/g, "\\$&")}|${LAMP_BASE.replace(/[{}]/g, "\\$&")})" \\{`), (row) => {
    coords.push({ id: /ID "([0-9A-F]{16})"/.exec(row)[1], at: /coords ([^\n]+)/.exec(row)[1] });
    return "@@LAMP@@\n";
  });
  if (coords.length !== 2) throw new Error(`expected the 2 facade lamps, found ${coords.length}`);
  let i = 0;
  out = out.replace(/@@LAMP@@\n/g, () => `${lampBlock(coords[i].id, coords[i++].at)}\n`);
  // 3. the new lights go after the last fixture mesh, where the old ones were: at the end of the child list
  const close = out.lastIndexOf("\n }\n}");
  if (close < 0) throw new Error("the furniture prefab has no child list");
  const rows = [
    ...SPOTS.map((s) => lightBlock(`spot-${s.name}`, { ...CEILING, ...s, y: CEILING.y })),
    lightBlock("glow", GLOW),
  ];
  return { text: `${out.slice(0, close)}\n${rows.join("\n")}${out.slice(close)}`, spots: SPOTS.length, removed: ceilingBefore.length };
}

if (process.argv[1] && import.meta.url === pathToFileURL(process.argv[1]).href) {
  if (!existsSync(FURNITURE)) {
    console.error(`no ${FURNITURE}`);
    process.exit(1);
  }
  const before = readFileSync(FURNITURE, "utf8");
  const r = applyPerf(before);
  if (process.argv.includes("--check")) {
    const same = r.text === before;
    console.log(same ? "supermarket perf: the furniture prefab matches this pass" : "supermarket perf: the furniture prefab differs; run node tools/buildings/supermarket/perf.mjs");
    process.exit(same ? 0 : 1);
  }
  writeFileSync(FURNITURE, r.text);
  console.log(`supermarket perf: ${r.removed} light(s) replaced by ${r.spots} ceiling spot(s) and the facade glow; 2 facade lamps now lit meshes`);
}
