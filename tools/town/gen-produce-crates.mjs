// Writes the market stall's produce crates (first-two-hours review A20): a wooden crate heaped with
// apples, plums or cabbages. The fruit is mesh only (no inventory, no physics), so nothing on the stall
// can be picked up and sold back. Deterministic: a seeded jitter, so a re-run writes the same files.
//
//   node tools/town/gen-produce-crates.mjs
//
// A file keeps its resource GUID (read from its .meta); a new file gets a new one. Entity and component IDs are
// a hash of the file and their place in it, so a re-run writes the same bytes. The work runs only when this file
// is the command: CI imports every tools module, and an import that wrote the prefabs failed its clean-tree check.
import { existsSync, mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { createHash, randomBytes } from "node:crypto";

const root = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
const dir = "Prefabs/Props/Market";
const abs = join(root, "addons/LF_Everon1989", dir);

const CRATE_XOB = "{CEC8A3DF893B9C96}Assets/Props/Crates/BoxWooden_01.xob";
// BoxWooden_01: 0.505 x 0.28 x 0.368 m, pivot at the base centre (tools\cli place bounds).
const INNER = { x: 0.2, z: 0.14, top: 0.28 };

const FRUIT = {
  Apple: { xob: "{D0566D3CAE967F4D}Assets/Items/Food/Apple/Apple.xob", size: 0.082, layers: [[5, 4, 0.19], [4, 3, 0.235]] },
  Plum: { xob: "{3EB653D9ECE9E4E9}Assets/Items/Food/Plum/Plum.xob", size: 0.046, layers: [[8, 6, 0.2], [7, 5, 0.235]] },
  Cabbage: { xob: "{446E8F1AEE3A1EEF}Assets/Items/Food/Cabbage/Cabbage.xob", size: 0.25, layers: [[2, 1, 0.15]], scale: 0.85, inner: { x: 0.11, z: 0 } },
};

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

// A file's resource GUID (kept from its .meta) and its stable entity and component IDs.
function existing(file) {
  const meta = join(abs, file + ".meta");
  const guid = existsSync(meta) ? /Name "\{([0-9A-F]{16})\}/.exec(readFileSync(meta, "utf8"))?.[1] : null;
  let i = 0;
  const id = () => createHash("sha1").update(`${dir}/${file}#${i++}`).digest("hex").slice(0, 16).toUpperCase();
  return { guid: guid ?? hex(), id };
}

function write(file, body, guid) {
  writeFileSync(join(abs, file), body);
  writeFileSync(join(abs, file + ".meta"), metaText(guid, `${dir}/${file}`));
  console.log(`{${guid}}${dir}/${file}`);
}

// Seeded jitter (mulberry32).
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

function main() {
mkdirSync(abs, { recursive: true });
let seed = 1989;
for (const [name, fruit] of Object.entries(FRUIT)) {
  // The mesh-only fruit.
  const fruitFile = `LF_ProduceFruit_${name}.et`;
  const fe = existing(fruitFile);
  write(fruitFile, `GenericEntity {
 ID "${fe.id()}"
 components {
  MeshObject "{${fe.id()}}" {
   Object "${fruit.xob}"
  }
 }
 coords 0 0 0
}
`, fe.guid);

  // The crate, heaped.
  const crateFile = `LF_ProduceCrate_${name}.et`;
  const ce = existing(crateFile);
  const r = rng(seed++);
  const inner = fruit.inner ?? INNER;
  let kids = "";
  for (const [nx, nz, y] of fruit.layers) {
    for (let ix = 0; ix < nx; ix++) {
      for (let iz = 0; iz < nz; iz++) {
        const x = nx === 1 ? 0 : -inner.x + (2 * inner.x * ix) / (nx - 1);
        const z = nz === 1 ? 0 : -inner.z + (2 * inner.z * iz) / (nz - 1);
        const j = fruit.size * 0.15;
        const scale = fruit.scale ? ` scale ${fruit.scale}` : "";
        kids += `  GenericEntity : "{${fe.guid}}${dir}/${fruitFile}" {
   ID "${ce.id()}"
   coords ${f3(x + (r() - 0.5) * j)} ${f3(y + (r() - 0.5) * j * 0.5)} ${f3(z + (r() - 0.5) * j)}
   angles ${f3((r() - 0.5) * 40)} ${f3(r() * 360)} ${f3((r() - 0.5) * 40)}
  }
`.replace("\n   angles", `${scale ? "\n  " + scale.trim().replace(/^/, " ") : ""}\n   angles`);
      }
    }
  }
  write(crateFile, `GenericEntity {
 ID "${ce.id()}"
 components {
  MeshObject "{${ce.id()}}" {
   Object "${CRATE_XOB}"
  }
  RigidBody "{${ce.id()}}" {
   Mass 10
   ModelGeometry 1
   Static 1
  }
 }
 coords 0 0 0
 {
${kids} }
}
`, ce.guid);
}
}

if (process.argv[1] && resolve(process.argv[1]) === fileURLToPath(import.meta.url)) main();
