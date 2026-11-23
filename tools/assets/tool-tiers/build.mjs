// Writes the nine tool-tier prefabs, their tinted .emat and the .meta files (#4188).
// `node tools/assets/tool-tiers/build.mjs`. Re-running keeps the GUIDs already on disk.
import { mkdirSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ematMeta, freshGuid, readGuid, usedGuids } from "../pipeline/meta.mjs";
import { OUT_DIR, OUT_RES, TIERS, TOOLS, name } from "./spec.mjs";

const root = process.argv[2] || process.cwd();
const out = join(root, OUT_DIR);
mkdirSync(out, { recursive: true });
const used = usedGuids(join(root, "addons"));

const etMeta = (res, guid) => {
  const plat = ["PC", "XBOX_ONE : PC", "XBOX_SERIES : PC", "PS4 : PC", "HEADLESS : PC"];
  return `MetaFileClass {\n Name "{${guid}}${res}"\n Configurations {\n${plat.map((p) => `  EntityTemplateResourceClass ${p} {\n  }\n`).join("")} }\n}\n`;
};

for (const tool of TOOLS) {
  for (const tier of TIERS) {
    const n = name(tool, tier);
    const ematRes = `${OUT_RES}/${n}.emat`;
    const etRes = `${OUT_RES}/${n}.et`;
    const ematGuid = readGuid(join(out, `${n}.emat.meta`)) || freshGuid(used);
    const etGuid = readGuid(join(out, `${n}.et.meta`)) || freshGuid(used);
    const nmo = tool.nmo ? ` NMOMap "${tool.nmo}"\n` : "";
    const metal = tool.nmo ? ` MetalnessScale ${tier.metal}\n` : "";
    const rgb = tier.color.split(" ").slice(0, 3).map((c) => +(Number(c) * (tool.gain ?? 1)).toFixed(3));
    writeFileSync(join(out, `${n}.emat`), `MatPBRBasic {\n Color ${rgb.join(" ")} 1\n BCRMap "${tool.bcr}"\n${nmo}${metal}}\n`);
    writeFileSync(join(out, `${n}.emat.meta`), ematMeta(ematRes, ematGuid));
    const entityId = (etGuid + ematGuid).slice(0, 16);
    writeFileSync(
      join(out, `${n}.et`),
      `GenericEntity : "${tool.parent}" {\n ID "${entityId}"\n components {\n  MeshObject "${tool.meshComp}" {\n   Materials {\n    MaterialAssignClass "{${freshGuid(used)}}" {\n     SourceMaterial "${tool.source}"\n     AssignedMaterial "{${ematGuid}}${ematRes}"\n    }\n   }\n  }\n }\n}\n`,
    );
    writeFileSync(join(out, `${n}.et.meta`), etMeta(etRes, etGuid));
  }
}
console.log(`wrote ${TOOLS.length * TIERS.length} tier prefabs to ${OUT_DIR}`);
