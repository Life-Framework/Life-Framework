// Contraband labels (#4383): the carton and the vodka bottle with English print on the vanilla meshes.
//   node tools/assets/contraband-labels/build.mjs [--no-import]
// 1. unpacks the two vanilla colour maps from the game paks (tools/livery/spike/edds.mjs),
// 2. paints the new patches over them in Blender (build.py) and writes the two .tif sources,
// 3. writes the resource metas and the two materials on the first run (fresh GUIDs, kept after),
// 4. rebuilds the .edds in one headless Workbench boot (`tools\cli ui-image --batch`) unless --no-import.
// The materials inherit the vanilla ones and swap only the colour map, so the normal map, the glass and
// every other slot stay vanilla. Credit rows: docs/credits.md "Contraband labels".
import { spawnSync } from "node:child_process";
import { existsSync, mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { ematMeta, freshGuid, readGuid, textureMeta, usedGuids } from "../pipeline/meta.mjs";

const here = dirname(fileURLToPath(import.meta.url));
const root = resolve(here, "..", "..", "..");
const pack = join(root, "addons", "LF_Everon1989");
const tmp = join(root, "tmp", "assets", "contraband-labels");
const vanilla = join(tmp, "vanilla");
mkdirSync(vanilla, { recursive: true });

const DIR = "Assets/Items/Contraband";
// name: our files; base: the vanilla material to inherit; bcr: the vanilla colour map to start from
const ITEMS = [
  {
    name: "LF_CigaretteCarton",
    baseEmat: "{84C7A4FD7BE5760F}Assets/Props/Garbage/Generic/LitterInterior_01/Data/LitterInterior_01_V1.emat",
    unpack: "Assets/Props/Garbage/Generic/LitterInterior_01/Data/LitterInterior_01_V1_BCR",
    kind: "MatPBRBasic",
  },
  {
    name: "LF_VodkaBottle",
    baseEmat: "{D3684FFCC0AC0E85}Assets/Props/S2/S2_Vodka/Data/S2_Vodka_paper.emat",
    unpack: "Assets/Props/S2/S2_Vodka/Data/S2_Vodka_paper_BCR",
    kind: "MatPBRBasic",
  },
];

for (const it of ITEMS) {
  const base = it.unpack.split("/").pop();
  const r = spawnSync(process.execPath, [join(root, "tools", "livery", "spike", "edds.mjs"), `${it.unpack}.edds`, join(vanilla, `${base}.dds`)], { encoding: "utf8" });
  if (r.status !== 0) { console.log(r.stdout + r.stderr); process.exit(1); }
}

const blend = spawnSync(process.execPath, [join(root, "tools", "cli.mjs"), "blender", "run", join(here, "build.py"), "--headless", "--", vanilla, join(tmp, "out")], { encoding: "utf8" });
process.stdout.write(blend.stdout.split("\n").filter((l) => /^wrote|FAIL|Error|Traceback/.test(l)).join("\n") + "\n");
if (blend.status !== 0) { console.log(blend.stdout + blend.stderr); process.exit(1); }

// Blender writes into tmp; the sources the Workbench rebuilds live next to their .edds in the pack.
const used = usedGuids(join(root, "addons"));
const tifs = [];
for (const it of ITEMS) {
  const dataDir = join(pack, DIR, it.name, "Data");
  mkdirSync(dataDir, { recursive: true });
  const bcr = join(dataDir, `${it.name}_BCR.tif`);
  writeFileSync(bcr, readFileSync(join(tmp, "out", `${it.name}_BCR.tif`)));
  tifs.push(bcr);
  const bcrRes = `${DIR}/${it.name}/Data/${it.name}_BCR.edds`;
  const emRes = `${DIR}/${it.name}/Data/${it.name}.emat`;
  const bcrMeta = `${bcr.replace(/\.tif$/, ".edds")}.meta`;
  const emMeta = join(dataDir, `${it.name}.emat.meta`);
  const bcrGuid = readGuid(bcrMeta) || freshGuid(used);
  const emGuid = readGuid(emMeta) || freshGuid(used);
  if (!existsSync(bcrMeta)) writeFileSync(bcrMeta, textureMeta(bcrRes, "TextureColorMap.conf", bcrGuid));
  if (!existsSync(emMeta)) writeFileSync(emMeta, ematMeta(emRes, emGuid));
  writeFileSync(join(dataDir, `${it.name}.emat`), `${it.kind} : "${it.baseEmat}" {\n BCRMap "{${bcrGuid}}${bcrRes}"\n}\n`);
  console.log(`{${emGuid}}${emRes}`);
}

if (process.argv.includes("--no-import")) process.exit(0);
const list = join(root, "tmp", "contraband-labels-batch.txt");
writeFileSync(list, tifs.map((t) => t.slice(root.length + 1).replace(/\\/g, "/")).join("\n") + "\n");
const imp = spawnSync(process.execPath, [join(root, "tools", "cli.mjs"), "ui-image", "--batch", "tmp/contraband-labels-batch.txt"], { cwd: root, encoding: "utf8", stdio: "inherit" });
// The Workbench import rewrites a texture meta's Name as "{GUID}.edds" (no path, which validate-meta-name-path
// rejects and no "{GUID}path" reference can name). Put the path back, keeping the GUID it chose, and point the
// material at that GUID.
for (const it of ITEMS) {
  const dataDir = join(pack, DIR, it.name, "Data");
  const meta = join(dataDir, `${it.name}_BCR.edds.meta`);
  const res = `${DIR}/${it.name}/Data/${it.name}_BCR.edds`;
  writeFileSync(meta, readFileSync(meta, "utf8").replace(/(Name "\{[0-9A-F]{16}\})\.edds"/, `$1${res}"`));
  const guid = readGuid(meta);
  writeFileSync(join(dataDir, `${it.name}.emat`), `${it.kind} : "${it.baseEmat}" {\n BCRMap "{${guid}}${res}"\n}\n`);
}
process.exit(imp.status ?? 1);
