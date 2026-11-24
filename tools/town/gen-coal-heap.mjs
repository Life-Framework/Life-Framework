// Writes the coal heap that stands beside the iron smelter (#3244 polish): a low mound of dark gravel under three shrinking
// layers of black coal chunks, mesh only (no inventory, no physics), so nothing in it can be picked up. The game has no coal
// heap (the kit `mine-coal-pile` says so: its piles read as dark stone), so this one is built from two vanilla meshes the coal
// items already use: the gravel pile for the body and the coal spill `c_coal_debris_01` (ten black lumps, 1.4 m across) for the
// chunks on top, each layer smaller and turned so the lumps sit on each other. The pack's own `Coal.xob` lump is not used: it
// renders as a red and white checker (its material is not found), which is a separate bug.
//
//   node tools/town/gen-coal-heap.mjs [--check]
//
// Deterministic (seeded jitter, IDs hashed from the file and place). A file keeps its resource GUID (read from its .meta).
// The work runs only when this file is the command: CI imports every tools module.
import { createHash, randomBytes } from "node:crypto";
import { existsSync, mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";

const root = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
const dir = "Prefabs/Props/Industrial";
const abs = join(root, "addons/LF_Everon1989", dir);

const GRAVEL = "{352A1A2A8D933FFE}Assets/Props/Construction/GravelPile_01/GravelPile_01_medium.xob"; // 8.3 x 1.1 x 5.0 m at 1; bounds x -3.71..4.56, y -0.08..1.05, z -2.26..2.70
const GRAVEL_SCALE = 0.22;
const SPILL = "{D895196B83A46FEA}Assets/Clutter/Coal/c_coal_debris_01.xob"; // 1.38 x 0.21 x 1.44 m, base at y -0.086
// Layers of coal chunks from the bottom up: [scale, base height above the heap's foot, yaw]. The base of a spill is 0.086 x scale below its pivot.
const LAYERS = [
  [1.15, 0.12, 20],
  [0.95, 0.2, 75],
  [0.75, 0.29, 110],
  [0.5, 0.38, 200],
];

const hex = () => randomBytes(8).toString("hex").toUpperCase();
const metaText = (guid, path) => `MetaFileClass {
 Name "{${guid}}${path}"
 Configurations {
  EntityTemplateResourceClass PC {
  }
  EntityTemplateResourceClass XBOX_ONE : PC {
  }
  EntityTemplateResourceClass XBOX_SERIES : PC {
  }
  EntityTemplateResourceClass PS4 : PC {
  }
  EntityTemplateResourceClass PS5 : PC {
  }
  EntityTemplateResourceClass HEADLESS : PC {
  }
 }
}
`;

function rng(seed) {
  let a = seed >>> 0;
  return () => {
    a = (a + 0x6d2b79f5) >>> 0;
    let t = a;
    t = Math.imul(t ^ (t >>> 15), t | 1);
    t ^= t + Math.imul(t ^ (t >>> 7), t | 61);
    return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
  };
}
const f3 = (n) => Number(n.toFixed(3));

// The planned files: [{ file, text }].
export function plan() {
  const file = "LF_CoalHeap.et";
  const meta = join(abs, file + ".meta");
  const guid = (existsSync(meta) ? /Name "{([0-9A-F]{16})}/.exec(readFileSync(meta, "utf8"))?.[1] : null) ?? hex();
  let i = 0;
  const id = () => createHash("sha1").update(`${dir}/${file}#${i++}`).digest("hex").slice(0, 16).toUpperCase();
  const r = rng(1989);
  const rootId = id();
  const part = (xob, coords, yaw, scale) => `  GenericEntity {
   ID "${id()}"
   components {
    MeshObject "{${id()}}" {
     Object "${xob}"
    }
   }
   coords ${coords.map(f3).join(" ")}
   angles 0 ${f3(yaw)} 0
   scale ${scale}
  }
`;
  // The gravel's own bounds are not centred on its pivot: shift it so the heap's middle is the prefab's origin.
  let kids = part(GRAVEL, [-0.4255 * GRAVEL_SCALE, 0.082 * GRAVEL_SCALE, -0.2195 * GRAVEL_SCALE], 0, GRAVEL_SCALE);
  for (const [scale, y, yaw] of LAYERS) kids += part(SPILL, [(r() - 0.5) * 0.12, y + 0.086 * scale, (r() - 0.5) * 0.12], yaw + (r() - 0.5) * 30, scale);
  const text = `GenericEntity {
 ID "${rootId}"
 coords 0 0 0
 {
${kids} }
}
`;
  return [
    { file, text },
    { file: file + ".meta", text: metaText(guid, `${dir}/${file}`) },
  ];
}

function main() {
  const check = process.argv.includes("--check");
  mkdirSync(abs, { recursive: true });
  let differ = 0;
  for (const f of plan()) {
    const p = join(abs, f.file);
    if (!existsSync(p) || readFileSync(p, "utf8") !== f.text) differ++;
    if (!check) writeFileSync(p, f.text);
  }
  console.log(`${check ? "check" : "wrote"} the coal heap, ${differ} file(s) differ`);
  if (check && differ) process.exit(1);
}

if (process.argv[1] && resolve(process.argv[1]) === fileURLToPath(import.meta.url)) main();
