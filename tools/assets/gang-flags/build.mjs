// node tools/assets/gang-flags/build.mjs [--textures] [--check]
// The twenty preset gang flags (#4219, gangs C1, decision N20), end to end:
//   --textures   first runs the asset pipeline (paint.py in Blender, then the Workbench texture import:
//                tools\cli asset run gang-flags --from generate), which writes the 40 maps, the 20 flag
//                materials (Data/GangFlag_<Name>.emat, one per variant of spec.json) and their metas.
//   (always)     writes one look prefab per flag, Assets/Props/Flags/Gang/LF_GangFlag_<Name>.et: the vanilla
//                flag pole with the vanilla flag cloth on it wearing that material, for `look item`. A re-run
//                keeps every GUID it finds, so nothing churns. The game flies a gang's flag by assigning the
//                material to the cloth at run time (SCR_FlagComponent.ChangeMaterial); the prefab is the picture.
//   --check      write nothing; exit 1 when a material, texture or prefab is missing.
// Core vs pack: a pack resource (this server's gang look), so it lives in LF_Everon1989 (core-pack-inventory.md).
import { existsSync, readFileSync, writeFileSync } from "node:fs";
import { spawnSync } from "node:child_process";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { freshGuid, readGuid, usedGuids } from "../pipeline/meta.mjs";
import { resourceMeta } from "../pipeline/prefab.mjs";

const HERE = dirname(fileURLToPath(import.meta.url));
const ROOT = resolve(HERE, "..", "..", "..");
const PACK = join(ROOT, "addons", "LF_Everon1989");
const DIR = "Assets/Props/Flags/Gang";
const POLE = "{31CE12B3D1F6789E}Prefabs/Structures/Military/Flags/FlagPole_01/FlagPole_01_base.et";
const CLOTH = "{AE09DA7896F9DB37}Prefabs/Props/Fabric/Flags/Flag_1_2.et";
const CLOTH_MATERIAL = "Flag_1_2"; // the source material slot name on Flag_1_2.xob
// Where the cloth hangs on the pole (metres, pole-local); tuned by `look item` pictures.
export const HOIST = "0 5.45 -0.1";

export const spec = () => JSON.parse(readFileSync(join(HERE, "spec.json"), "utf8"));
export const flagNames = () => spec().variants.map((v) => v.name);
export const prefabRes = (name) => `${DIR}/LF_GangFlag_${name}.et`;
export const ematRes = (name) => `${DIR}/Data/GangFlag_${name}.emat`;

export function prefabText(name, emat, ids) {
  return `GenericEntity : "${POLE}" {
 ID "${ids.entity}"
 {
  GenericEntity Cloth : "${CLOTH}" {
   ID "${ids.cloth}"
   coords ${HOIST}
   components {
    MeshObject "{${ids.mesh}}" {
     Materials {
      MaterialAssignClass "{${ids.assign}}" {
       SourceMaterial "${CLOTH_MATERIAL}"
       AssignedMaterial "${emat}"
      }
     }
    }
   }
  }
 }
}
`;
}

function idsOf(text, used) {
  const pick = (re) => (re.exec(text) || [])[1] || freshGuid(used);
  return {
    entity: pick(/^ID "([0-9A-F]{16})"/m),
    cloth: pick(/Cloth : [^\n]*\{\s*ID "([0-9A-F]{16})"/),
    mesh: pick(/MeshObject "\{([0-9A-F]{16})\}"/),
    assign: pick(/MaterialAssignClass "\{([0-9A-F]{16})\}"/),
  };
}

export function missing() {
  const out = [];
  for (const n of flagNames()) {
    for (const f of [`${ematRes(n)}`, `${ematRes(n)}.meta`, `${DIR}/Data/GangFlag_${n}_BCR.edds`, `${DIR}/Data/GangFlag_${n}_BCR.tif`]) {
      if (!existsSync(join(PACK, f))) out.push(f);
    }
  }
  return out;
}

function main() {
  const args = process.argv.slice(2);
  if (args.includes("--textures")) {
    const r = spawnSync(process.execPath, [join(ROOT, "tools", "cli.mjs"), "asset", "run", "gang-flags", "--from", "generate"], { stdio: "inherit", cwd: ROOT });
    if (r.status !== 0) process.exit(r.status || 1);
  }
  const gone = missing();
  if (gone.length) {
    console.log(`gang-flags: ${gone.length} file(s) missing, run with --textures:\n  ${gone.slice(0, 8).join("\n  ")}`);
    process.exit(1);
  }
  if (args.includes("--check")) { console.log(`gang-flags: ${flagNames().length} flags ok`); return; }
  const used = usedGuids(PACK);
  let n = 0;
  for (const name of flagNames()) {
    const emat = `{${readGuid(join(PACK, ematRes(name) + ".meta"))}}${ematRes(name)}`;
    const file = join(PACK, prefabRes(name));
    const old = existsSync(file) ? readFileSync(file, "utf8") : "";
    const text = prefabText(name, emat, idsOf(old, used));
    if (text !== old) { writeFileSync(file, text); n++; }
    const meta = file + ".meta";
    if (!existsSync(meta)) writeFileSync(meta, resourceMeta(prefabRes(name), freshGuid(used), "EntityTemplateResourceClass"));
  }
  console.log(`gang-flags: ${flagNames().length} look prefabs, ${n} written`);
  console.log(flagNames().map((nm) => `  ${nm}: ${prefabRes(nm)}`).join("\n"));
}

if (process.argv[1] && resolve(process.argv[1]) === fileURLToPath(import.meta.url)) main();
