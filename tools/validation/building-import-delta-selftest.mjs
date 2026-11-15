// inputs: tools/buildings/pipeline/**, tools/assets/pipeline/**, tools/remote/desktop/in-session.mjs
// Proves offline what an incremental building import decides (tools/buildings/pipeline/import.mjs,
// docs/building-pipeline/README.md "Fast polish iterations"): which files it writes and which models
// it sends to Workbench, against the last import's manifest, and that an import with nothing changed
// never starts Workbench. Also the Blender start-up noise filter (archbuilder.mjs).
// red-proof: 2026-11-15, one line broken per block, each restored after its FAIL was seen:
//   fbxSansTime not zeroing the time stamp            -> FAIL hash: two exports of one scene (only the time differs) hash the same
//   importDelta comparing only the manifest's inputs  -> FAIL delta: an .xob replaced in the addon (git checkout) is compiled again
//   importDelta writing every model .xob.meta          -> FAIL delta: an unchanged model keeps Workbench's re-saved .xob.meta
//   withoutBlenderNoise keeping traceback lines        -> FAIL noise: the ar_mqa traceback is dropped, the real error kept
import { copyFileSync, mkdirSync, mkdtempSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join } from "node:path";
import { pathToFileURL } from "node:url";
import { contentHash, importBuilding, importDelta, planImport, readManifest, writeManifest, writePlan } from "../buildings/pipeline/import.mjs";
import { withoutBlenderNoise } from "../buildings/pipeline/archbuilder.mjs";

const ROOT = join(dirname(new URL(import.meta.url).pathname.replace(/^\/([A-Za-z]:)/, "$1")), "..", "..");
const SPEC = {
  slug: "shop", name: "Shop", concept: "A test shop.", red_line: "pack",
  archbuilder: { archetype: "SUPERMARKET", seed: 1989 }, budget: { tris: 60000, footprint_m: [19, 15], height_m: 9, min_doors: 1 },
  asset_dir: "Assets/Buildings/Shop", prefab_dir: "Prefabs/Buildings/Shop",
};
const A = "Assets/ArchBuilder/Shop", P = "Prefabs/ArchBuilder/Shop", M = "Assets/ArchBuilder/Materials";

function put(dir, rel, data) {
  mkdirSync(dirname(join(dir, rel)), { recursive: true });
  writeFileSync(join(dir, rel), data);
}
// A binary FBX header the way Blender writes it: each CreationTimeStamp field is its name, "I", an int32.
function fbx(mesh, { hour = 7, ms = 419 } = {}) {
  const int = (n) => { const b = Buffer.alloc(4); b.writeInt32LE(n); return b; };
  const field = (name, n) => Buffer.concat([Buffer.from([name.length]), Buffer.from(name + "I", "latin1"), int(n)]);
  return Buffer.concat([Buffer.from("Kaydara FBX Binary  \0\x1a\0", "latin1"), field("Year", 2026), field("Hour", hour), field("Minute", 12), field("Second", 5), field("Millisecond", ms), Buffer.from(mesh)]);
}
const xobMeta = (mat) => `MetaFileClass {\n Name "{1111111111111111}${A}/Shop.xob"\n Configurations {\n  FBXResourceClass PC {\n   MaterialAssigns {\n    MaterialAssignClass "{AAAA000000000001}" {\n     SourceMaterial "AB_Brick"\n     AssignedMaterial "{${mat === "AB_Oak" ? "2222222222222223" : "2222222222222222"}}${M}/${mat}.emat"\n    }\n   }\n  }\n }\n}\n`;
const meta = (guid, path) => `MetaFileClass {\n Name "{${guid}}${path}"\n}\n`;

function exportShop(dir, { mesh = "walls v1", hour, mat = "AB_Brick", base = "" } = {}) {
  rmSync(dir, { recursive: true, force: true });
  put(dir, `${A}/Shop.fbx`, fbx(mesh, { hour }));
  put(dir, `${A}/Shop.xob.meta`, xobMeta(mat));
  for (const m of ["AB_Brick", "AB_Oak"]) {
    put(dir, `${M}/${m}.emat`, `MatPBRBasic {\n Color 0.5 0.4 0.3 1\n}\n`);
    put(dir, `${M}/${m}.emat.meta`, meta(m === "AB_Brick" ? "2222222222222222" : "2222222222222223", `${M}/${m}.emat`));
  }
  put(dir, `${P}/Shop.et`, `SCR_DestructibleBuildingEntity : "{A43A100E3C377DB2}Prefabs/Structures/Core/Building_Base.et" {\n components {\n  MeshObject "{506EDA9D46FC6C06}" {\n   Object "{1111111111111111}${A}/Shop.xob"\n  }\n }\n${base}}\n`);
  put(dir, `${P}/Shop.et.meta`, meta("3333333333333333", `${P}/Shop.et`));
  put(dir, `${A}/EXPORT_INFO.txt`, "Licensed to: nobody\n");
}

export async function main() {
  let failed = 0;
  const ok = (cond, what) => {
    if (cond) return;
    failed++;
    console.log(`FAIL ${what}`);
  };
  const tmp = mkdtempSync(join(tmpdir(), "lf-bp-delta-"));
  try {
    // ---- hash
    ok(contentHash("x.fbx", fbx("walls v1", { hour: 7, ms: 1 })) === contentHash("x.fbx", fbx("walls v1", { hour: 13, ms: 999 })), "hash: two exports of one scene (only the time differs) hash the same");
    ok(contentHash("x.fbx", fbx("walls v1")) !== contentHash("x.fbx", fbx("walls v2")), "hash: a changed mesh changes the hash");
    ok(contentHash("x.et", fbx("walls v1", { hour: 7 })) !== contentHash("x.et", fbx("walls v1", { hour: 8 })), "hash: only an .fbx has its time stamp ignored");

    // ---- delta
    const staging = join(tmp, "export");
    const addon = join(tmp, "addon");
    const dir = join(tmp, "cache");
    mkdirSync(dir, { recursive: true });
    const plan = () => {
      const p = planImport({ staging, spec: SPEC, addonDir: addon, used: new Set() });
      if (p.problems.length) throw new Error(p.problems.join("; "));
      return p;
    };
    const paths = (fs) => fs.map((f) => f.to).sort().join(", ");
    // Workbench compiling: the .xob, and its .meta re-saved with the lines Workbench adds.
    const compile = (p) => {
      const m = p.files.find((f) => f.to.endsWith(".xob.meta"));
      put(addon, "Assets/Buildings/Shop/Shop.xob", `XOB of ${contentHash("Shop.fbx", readFileSync(join(addon, "Assets/Buildings/Shop/Shop.fbx")))}`);
      put(addon, m.to, m.text.replace(/\n  \}\n \}\n\}\n$/, '\n   ApplicationName "Blender (stable FBX IO)"\n  }\n }\n}\n'));
    };
    exportShop(staging);
    let p = plan();
    let d = importDelta(p, { addonDir: addon, manifest: null });
    ok(d.models.length === 1 && /no earlier import/.test(d.models[0].why) && d.write.length === p.files.length, `delta: the first import writes every file and compiles the model (${d.write.length}/${p.files.length})`);
    ok(/--full/.test(importDelta(p, { addonDir: addon, manifest: null, full: true }).models[0].why), "delta: --full compiles every model and says so");
    writePlan(addon, { files: d.write });
    compile(p);
    writeManifest(dir, addon, d);
    const resaved = readFileSync(join(addon, "Assets/Buildings/Shop/Shop.xob.meta"), "utf8");

    exportShop(staging, { hour: 18 }); // the same scene exported again, an hour later
    d = importDelta(plan(), { addonDir: addon, manifest: readManifest(dir) });
    ok(!d.write.length && !d.models.length, `delta: a re-export of an unchanged scene writes nothing and compiles nothing (wrote ${paths(d.write)}; compiled ${d.models.map((m) => m.rel)})`);

    exportShop(staging, { base: " {\n  GenericEntity : \"{9EB592E7C7CD1C0F}Prefabs/Structures/BuildingParts/Doors/DoorSet_Village_E_01_R_EXT_COV_B.et\" {\n  }\n }\n" });
    d = importDelta(plan(), { addonDir: addon, manifest: readManifest(dir) });
    ok(paths(d.write) === "Prefabs/Buildings/Shop/Shop.et" && !d.models.length, `delta: a prefab-only change writes that prefab and starts no Workbench (wrote ${paths(d.write)})`);
    ok(!d.write.some((f) => f.to.endsWith(".xob.meta")) && readFileSync(join(addon, "Assets/Buildings/Shop/Shop.xob.meta"), "utf8") === resaved, "delta: an unchanged model keeps Workbench's re-saved .xob.meta");

    exportShop(staging, { mesh: "walls v2" });
    p = plan();
    d = importDelta(p, { addonDir: addon, manifest: readManifest(dir) });
    ok(d.models.length === 1 && d.models[0].why === "the mesh changed" && paths(d.write) === "Assets/Buildings/Shop/Shop.fbx, Assets/Buildings/Shop/Shop.xob.meta", `delta: a changed mesh writes the .fbx and its planned .meta and compiles it (wrote ${paths(d.write)}; ${d.models[0]?.why})`);
    writePlan(addon, { files: d.write });
    compile(p);
    writeManifest(dir, addon, d);

    exportShop(staging, { mesh: "walls v2", mat: "AB_Oak" });
    d = importDelta(plan(), { addonDir: addon, manifest: readManifest(dir) });
    ok(d.models.length === 1 && /\.xob\.meta changed/.test(d.models[0].why), `delta: a changed material assignment compiles the model again (${d.models[0]?.why})`);

    exportShop(staging, { mesh: "walls v2" });
    put(addon, "Assets/Buildings/Shop/Shop.xob", "an older .xob, from a git checkout");
    d = importDelta(plan(), { addonDir: addon, manifest: readManifest(dir) });
    ok(d.models.length === 1 && /not the one the last import made/.test(d.models[0].why), `delta: an .xob replaced in the addon (git checkout) is compiled again (${d.models[0]?.why})`);
    rmSync(join(addon, "Prefabs/Buildings/Shop/Shop.et"));
    ok(paths(importDelta(plan(), { addonDir: addon, manifest: readManifest(dir) }).write).includes("Prefabs/Buildings/Shop/Shop.et"), "delta: a file deleted from the addon is written again");

    // ---- importBuilding with nothing changed starts no Workbench and keeps the manifest
    const root = join(tmp, "repo");
    const real = join(ROOT, "addons", "LF_Everon1989", "Assets", "Buildings", "Supermarket", "Supermarket.xob"); // a parseable .xob
    const cache = join(root, "tmp", "buildings", "shop");
    exportShop(join(cache, "export"));
    const raddon = join(root, "addons", "LifeFramework");
    const p2 = planImport({ staging: join(cache, "export"), spec: SPEC, addonDir: raddon, used: new Set() });
    const d2 = importDelta(p2, { addonDir: raddon, manifest: null });
    writePlan(raddon, { files: d2.write });
    mkdirSync(join(raddon, "Assets/Buildings/Shop"), { recursive: true });
    copyFileSync(real, join(raddon, "Assets/Buildings/Shop/Shop.xob"));
    writeManifest(cache, raddon, d2);
    const lines = [];
    const log = console.log;
    console.log = (...a) => lines.push(a.join(" "));
    let error = "";
    let r = null;
    try {
      r = await importBuilding({ root, spec: SPEC, dir: cache, workbench: () => { throw new Error("Workbench must not start"); }, routeWorkbench: () => { throw new Error("no pool run"); }, wbAllowed: true, licenseMarks: () => ["TEST-KEY-0000-1111"] });
    } catch (e) {
      error = e.message;
    } finally {
      console.log = log;
    }
    ok(!error && r && !r.compiled.length && lines.some((l) => /nothing changed since the last import/.test(l)), `import: nothing changed starts no Workbench and says so in one line (${error || lines.join(" | ")})`);
    ok(readManifest(cache) !== null, "import: a good import leaves its manifest for the next one");

    // ---- noise
    const noisy = [
      "Blender 5.3.0 (hash 02f0899d78e3 built 2026-11-10 01:33:43)",
      "\x1b[0m[EBT] [INFO]: Initialized Logger\x1b[0m",
      "\x1b[31m\x1b[1m[EnfusionBlenderTools.ebt.ebt_modules] [CRITICAL]: Error importing EnfusionBlenderTools.ar_mqa: No module named 'EnfusionBlenderTools.core.types'\x1b[0m",
      "\x1b[31m\x1b[1m[EnfusionBlenderTools.ebt.ebt_modules] [CRITICAL]: Traceback (most recent call last):",
      '  File "C:\\x\\ebt_modules.py", line 82, in import_modules_from_directory',
      "ModuleNotFoundError: No module named 'EnfusionBlenderTools.core.types'",
      "\x1b[0m",
      "Traceback (most recent call last):",
      "RuntimeError: the .blend holds no ArchBuilder building (no object with ab_anchor)",
    ].join("\n");
    const quiet = withoutBlenderNoise(noisy);
    ok(!/EnfusionBlenderTools|EBT|ebt_modules|ModuleNotFound/.test(quiet) && /no object with ab_anchor/.test(quiet) && /^Blender 5\.3/.test(quiet), `noise: the ar_mqa traceback is dropped, the real error kept (${JSON.stringify(quiet)})`);
  } finally {
    rmSync(tmp, { recursive: true, force: true });
  }
  if (failed) {
    console.log(`building-import-delta-selftest: ${failed} FAILED`);
    return 1;
  }
  console.log("building-import-delta-selftest: OK");
  return 0;
}

if (process.argv[1] && import.meta.url === pathToFileURL(process.argv[1]).href) process.exit(await main());
