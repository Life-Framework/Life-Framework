// The clinic's building prefab, furniture and kit, from the base game's House_Town_E_2I02_v2 (a converted town house).
//
//   node tools/buildings/clinic/shell.mjs [--pass 0|1|2] [--check]   write (or check) Clinic.et, the furniture copy,
//                                                                    its decor block and Clinic_Kit.et
//
// Clinic.et is the vanilla building prefab (its doors, windows, entry and probe) with the furniture child pointed at
// our own copy: the vanilla set flattened to one child per prop, the props prune.mjs leaves out dropped, and the
// decor block (layout.mjs) appended. Clinic_Kit.et inherits Clinic.et and adds the interactables (kit.mjs). Writing
// twice gives the same bytes; a missing `.meta` is made with a fresh GUID and kept (rule 2). --pass picks how much of
// layout.mjs is written: 0 BUILD, 1 POLISH 1, 2 POLISH 2 (the default).
import { randomBytes } from "node:crypto";
import { existsSync, mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { gameVfs, readText } from "../../lib/game-prefab.mjs";
import { emitEntity, entities, floorOf } from "./furniture-lib.mjs";
import { renderBlock, swapBlock } from "./decor-lib.mjs";
import { decorOf } from "./layout.mjs";
import { keepFor } from "./prune.mjs";
import { kitText } from "./kit.mjs";

const ROOT = join(dirname(fileURLToPath(import.meta.url)), "..", "..", "..");
const DIR = join(ROOT, "addons", "LF_Everon1989", "Prefabs", "Buildings", "Clinic");
const VANILLA = "Prefabs/Structures/Houses/Town/House_Town_E_2I02/House_Town_E_2I02_v2.et";
const VANILLA_FURNITURE = "Prefabs/Structures/Houses/Town/House_Town_E_2I02/Furniture/House_Town_E_2I02_furniture_01.et";
export const BUILDING = "Clinic.et";
export const FURNITURE = "Furniture/Clinic_furniture_01.et";
export const KIT = "Clinic_Kit.et";
const PREFIX = "C11C";
const args = process.argv.slice(2);
const CHECK = args.includes("--check");
const PASS = args.includes("--pass") ? Number(args[args.indexOf("--pass") + 1]) : 2;

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

// The GUID a file already has, or a fresh one (kept from then on).
export function guidOf(rel) {
  const meta = join(DIR, `${rel}.meta`);
  if (existsSync(meta)) return /Name "\{([0-9A-F]{16})\}/.exec(readFileSync(meta, "utf8"))?.[1];
  const guid = randomBytes(8).toString("hex").toUpperCase();
  if (CHECK) throw new Error(`${rel}.meta is missing`);
  mkdirSync(dirname(meta), { recursive: true });
  writeFileSync(meta, metaText(guid, `Prefabs/Buildings/Clinic/${rel}`));
  return guid;
}

const strip = (t) => t.split("\n").filter((l) => !l.startsWith("//")).join("\n").replace(/^\n+/, "");

async function main() {
  const vfs = await gameVfs();
  const vanilla = strip(readText(ROOT, vfs, VANILLA));
  const vFurniture = strip(readText(ROOT, vfs, VANILLA_FURNITURE));
  const fGuid = guidOf(FURNITURE);
  guidOf(BUILDING);
  guidOf(KIT);

  const oldRef = /GenericEntity : "(\{[0-9A-F]+\})Prefabs\/Structures\/Houses\/Town\/House_Town_E_2I02\/Furniture\/House_Town_E_2I02_furniture_01\.et"/;
  if (!oldRef.test(vanilla)) throw new Error("the vanilla building has no furniture child");
  const building = vanilla.replace(oldRef, `GenericEntity : "{${fGuid}}Prefabs/Buildings/Clinic/${FURNITURE}"`).replace(/^ coords [-\d. ]+\n/m, "").replace(/^ angles [-\d. ]+\n/m, "");

  const root = /^(GenericEntity : "[^"]+" \{\n ID "[0-9A-F]+")/.exec(vFurniture)?.[1];
  if (!root) throw new Error("cannot read the vanilla furniture root");
  // A flat turn the base game writes as `angles 0 Y 0` is written `angleY Y` (the same turn; the surface check reads angleY)
  const flat = (e) => {
    const i = e.node.lines.findIndex((l) => /^angles -?[\d.]+ -?[\d.]+ -?[\d.]+$/.test(l));
    if (i < 0) return e;
    const [ax, ay, az] = e.node.lines[i].split(" ").slice(1).map(Number);
    if (ax !== 0 || az !== 0) return e;
    const lines = e.node.lines.slice();
    if (ay === 0) lines.splice(i, 1);
    else lines[i] = `angleY ${ay}`;
    return { ...e, node: { ...e.node, lines } };
  };
  const kept = entities(vFurniture).filter(keepFor(PASS)).map(flat);
  const decor = decorOf(PASS);
  const bare = `${root}\n {\n${kept.map(emitEntity).join("\n")}\n }\n}\n`;
  const furniture = swapBlock(bare, PREFIX, await renderBlock(ROOT, PREFIX, decor));

  const outs = [
    [join(DIR, BUILDING), `${building.trimEnd()}\n`],
    [join(DIR, FURNITURE), furniture],
    [join(DIR, KIT), kitText(PASS, guidOf(BUILDING))],
  ];
  let drift = false;
  for (const [file, text] of outs) {
    const cur = existsSync(file) ? readFileSync(file, "utf8") : null;
    if (cur === text) continue;
    drift = true;
    if (!CHECK) {
      mkdirSync(dirname(file), { recursive: true });
      writeFileSync(file, text);
    }
  }
  const floors = [0, 0, 0];
  for (const e of kept) floors[floorOf(e.y)]++;
  console.log(`clinic pass ${PASS}: ${kept.length} vanilla prop(s) kept of ${entities(vFurniture).length} (floors ${floors.join("/")}), ${decor.items.length} decor prop(s), ${decor.lights.length} light(s)${CHECK ? (drift ? " DRIFT" : " current") : " written"}`);
  if (CHECK && drift) process.exit(1);
}
main().catch((e) => {
  console.error(e.message);
  process.exit(1);
});
