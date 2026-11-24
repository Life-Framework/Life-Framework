// Writes the fish stall's dressing (#4646 polish): whole fish in the market's green crates. The fish are mesh only
// (no inventory, no physics), so nothing on the stall can be picked up and sold at the wholesale. Same idea as
// gen-produce-crates.mjs (apples, plums, cabbages), same deterministic seeded jitter: a re-run writes the same bytes.
//
//   node tools/town/gen-fish-crates.mjs [--check]
//
// A file keeps its resource GUID (read from its .meta); a new file gets a new one. Entity and component IDs are a hash
// of the file and their place in it. The work runs only when this file is the command: CI imports every tools module.
// There is no ice mesh in the game, so the crates are not iced: the fish lie belly-down in layers, three deep, heaped
// to the rim, which is how a 1989 fish crate looks from the street.
import { createHash, randomBytes } from "node:crypto";
import { existsSync, mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";

const root = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
const dir = "Prefabs/Props/Market";
const abs = join(root, "addons/LF_Everon1989", dir);

// The vanilla green crate the stall already uses (Crate_01_green: 0.61 x 0.41 x 0.22 m, inner floor about 0.02 m).
const CRATE = "{AA3D2C0BB0D2B56B}Prefabs/Props/Crates/Crate_01_green.et";
const INNER = { x: 0.26, z: 0.15 };
// The pack's own fish models (addons/LifeFramework/Assets/Items/Fishing/*). `w` is the body width across the crate (m),
// `h` the half height, `len` the length: all measured with `tools\\cli measure`.
const FISH = {
  Mackerel: { xob: "{65E57DC2E990CC91}Assets/Items/Fishing/FishMackerel/FishMackerel.xob", w: 0.066, h: 0.046, len: 0.28 },
  Trout: { xob: "{C21C28B41B464093}Assets/Items/Fishing/FishTrout/FishTrout.xob", w: 0.078, h: 0.046, len: 0.3 },
  Perch: { xob: "{886A87FB02374A92}Assets/Items/Fishing/FishPerch/FishPerch.xob", w: 0.086, h: 0.064, len: 0.24 },
  Bass: { xob: "{844FBF1EB8480E5C}Assets/Items/Fishing/FishBass/FishBass.xob", w: 0.16, h: 0.08, len: 0.36 },
  TunaSmall: { xob: "{8286F367851FF333}Assets/Items/Fishing/FishTunaSmall/FishTunaSmall.xob", w: 0.158, h: 0.074, len: 0.364 },
};
// One crate per species: how many fish across (along the crate) in each of three layers.
const CRATES = {
  Mackerel: { fish: "Mackerel", layers: [7, 6, 7] },
  Trout: { fish: "Trout", layers: [6, 5, 6] },
  Perch: { fish: "Perch", layers: [6, 5] },
};
// Layer centre heights above the crate's origin: the first lies on the floor (0.02 + half height), the next sit in the gaps.
const FLOOR = 0.022;

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

// A file's GUID is chosen once per run, so a crate names the decor mesh the same GUID that mesh's own .meta gets.
const chosen = new Map();
function existing(file) {
  const meta = join(abs, file + ".meta");
  const guid = existsSync(meta) ? /Name "\{([0-9A-F]{16})\}/.exec(readFileSync(meta, "utf8"))?.[1] : null;
  let i = 0;
  const id = () => createHash("sha1").update(`${dir}/${file}#${i++}`).digest("hex").slice(0, 16).toUpperCase();
  if (!chosen.has(file)) chosen.set(file, guid ?? hex());
  return { guid: chosen.get(file), id };
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

// The planned files: [{ file, text }], meta files included.
export function plan() {
  const out = [];
  const add = (file, text, guid) => {
    out.push({ file, text });
    out.push({ file: file + ".meta", text: metaText(guid, `${dir}/${file}`) });
  };
  for (const [name, fish] of Object.entries(FISH)) {
    const file = `LF_FishDecor_${name}.et`;
    const e = existing(file);
    add(file, `GenericEntity {
 ID "${e.id()}"
 components {
  MeshObject "{${e.id()}}" {
   Object "${fish.xob}"
  }
 }
 coords 0 0 0
}
`, e.guid);
  }
  let seed = 4646;
  for (const [name, c] of Object.entries(CRATES)) {
    const fish = FISH[c.fish];
    const fe = existing(`LF_FishDecor_${c.fish}.et`);
    const file = `LF_FishCrate_${name}.et`;
    const ce = existing(file);
    const r = rng(seed++);
    const pitch = fish.w + 0.012;
    let kids = "";
    c.layers.forEach((n, layer) => {
      // A layer one fish short sits half a pitch off the one below, so the fish nest; heads alternate along each layer.
      const span = (n - 1) * pitch;
      const y = FLOOR + fish.h + layer * fish.h * 1.05;
      for (let i = 0; i < n; i++) {
        const x = -span / 2 + i * pitch;
        const z = (r() - 0.5) * 0.04;
        const head = (i + layer) % 2 ? 180 : 0;
        kids += `  GenericEntity : "{${fe.guid}}${dir}/LF_FishDecor_${c.fish}.et" {
   ID "${ce.id()}"
   coords ${f3(x)} ${f3(y + (r() - 0.5) * 0.008)} ${f3(z)}
   angles ${f3((r() - 0.5) * 16)} ${f3(head + (r() - 0.5) * 22)} ${f3((r() - 0.5) * 24)}
  }
`;
      }
    });
    add(file, `GenericEntity : "${CRATE}" {
 ID "F0DBA538AC2A0552"
 coords 0 0 0
 {
${kids} }
}
`, ce.guid);
  }
  return out;
}

function main() {
  const check = process.argv.includes("--check");
  mkdirSync(abs, { recursive: true });
  const files = plan();
  let differ = 0;
  for (const f of files) {
    const p = join(abs, f.file);
    if (!existsSync(p) || readFileSync(p, "utf8") !== f.text) differ++;
    if (!check) writeFileSync(p, f.text);
  }
  console.log(`${check ? "check" : "wrote"} ${files.length} files, ${differ} differ`);
  if (check && differ) process.exit(1);
}

if (process.argv[1] && resolve(process.argv[1]) === fileURLToPath(import.meta.url)) main();
