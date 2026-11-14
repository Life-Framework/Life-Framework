// Proves offline what tools\cli building decides without Blender, ArchBuilder or Workbench: the spec
// rules, where each exported file lands, the GUID rewrite (fresh, unique, stable on re-import, base-game
// references untouched), the license-watermark refusal, the model checks, the base-game database
// reader, and that every ArchBuilder stage skips on a machine without it (CI). The fixture export is
// written here by hand in the shape ArchBuilder 2.42.0 writes (docs/architecture/building-pipeline-spike.md);
// none of ArchBuilder's own files are in it.
// red-proof: 2026-11-14, one line broken per block, each restored after its FAIL was seen:
//   planImport keeping the staging GUID (guid: oldGuid)        -> FAIL guid: every resource gets a GUID the export did not have
//   skipped() not skipping EXPORT_INFO.txt                     -> FAIL map: EXPORT_INFO.txt (the buyer's name) is never copied
//   scanForMarks not lower-casing the marks                    -> FAIL watermark: a file with the buyer id in another case is refused
//   run() without the archBuilderSetup check                   -> FAIL skip: review skips without ArchBuilder
//   exportProblems ignoring props_mesh (2026-11-14, parity)     -> FAIL export: furniture ArchBuilder left out of the game fails, by kind
//   specProblems allowing vanilla props with furniture off       -> FAIL spec: vanilla props with furniture off are refused
import { mkdirSync, mkdtempSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join } from "node:path";
import { pathToFileURL } from "node:url";
import { checkBuildingModel, metaDrift, metaMaterials, metaSurfaces, planImport, skipped, targetPath, writePlan, importBuilding } from "../buildings/pipeline/import.mjs";
import { STAGES, badGameRefs, countOpenings, countProps, exportProblems, reviewProblems, run, specProblems } from "../buildings/pipeline/building.mjs";
import { archBuilderSetup, licenseMarks, scanForMarks } from "../buildings/pipeline/archbuilder.mjs";
import { guidIn, rdbRecord } from "../buildings/pipeline/gamedb.mjs";

const ROOT = join(dirname(new URL(import.meta.url).pathname.replace(/^\/([A-Za-z]:)/, "$1")), "..", "..");
const FAKE_BUYER = "Test.Buyer@example.invalid";
const FAKE_KEY = "TEST-KEY-0000-1111";

function put(dir, rel, text) {
  mkdirSync(dirname(join(dir, rel)), { recursive: true });
  writeFileSync(join(dir, rel), text);
}
const meta = (guid, path, cls = "EntityTemplateResourceClass") => `MetaFileClass {\n Name "{${guid}}${path}"\n Configurations {\n  ${cls} PC {\n  }\n }\n}\n`;

// An export in ArchBuilder's layout: Assets/ArchBuilder/<Name>, Assets/ArchBuilder/Materials, Prefabs/ArchBuilder/<Name>.
function fixtureExport(dir, name = "Shop") {
  const A = `Assets/ArchBuilder/${name}`, P = `Prefabs/ArchBuilder/${name}`, M = "Assets/ArchBuilder/Materials";
  put(dir, `${A}/${name}.fbx`, "binary fbx stand-in");
  put(dir, `${A}/${name}.xob.meta`, `MetaFileClass {\n Name "{1111111111111111}${A}/${name}.xob"\n Configurations {\n  FBXResourceClass PC {\n   SourceFile "${name}.fbx"\n   MaterialAssigns {\n    MaterialAssignClass "{AAAA000000000001}" {\n     SourceMaterial "AB_Brick_Red"\n     AssignedMaterial "{2222222222222222}${M}/AB_Brick_Red.emat"\n    }\n   }\n   GeometryParams {\n    GeometryParam UTM_BD_brick {\n     LayerPreset "BuildingFireView"\n     SurfaceProperties {\n      "{B393F062AAE9EB74}Common/Materials/Game/brick.gamemat"\n     }\n     Mass 0\n     Margin 0\n    }\n   }\n   Common TXOCommonClass "{AAAA000000000002}" : "{0877E7C4BB2B2C9A}Configs/System/ResourceTypes/PC/MeshObjectCommon.conf" {\n   }\n  }\n }\n}\n`);
  put(dir, `${M}/AB_Brick_Red.emat`, `MatPBRBasic {\n BCRMap "{63A5BA526E991FD8}Assets/_SharedData/Brick/ST_Bricks_Clean_01_BCR.edds"\n}\n`);
  put(dir, `${M}/AB_Brick_Red.emat.meta`, meta("2222222222222222", `${M}/AB_Brick_Red.emat`, "EMATResourceClass"));
  put(dir, `${P}/${name}_Base.et`, `SCR_DestructibleBuildingEntity : "{A43A100E3C377DB2}Prefabs/Structures/Core/Building_Base.et" {\n ID "D35129981D992CF0"\n components {\n  MeshObject "{506EDA9D46FC6C06}" {\n   Object "{1111111111111111}${A}/${name}.xob"\n  }\n }\n {\n  GenericEntity : "{9EB592E7C7CD1C0F}Prefabs/Structures/BuildingParts/Doors/DoorSet_Village_E_01_R_EXT_COV_B.et" {\n   ID "9F3054A37A993A96"\n  }\n  Building : "{6372EE72EAEB0049}Prefabs/Structures/BuildingParts/Windows/Win_village_E_A/Win_village_e_a_130x182_2_B.et" {\n   ID "178F9EC76933ED89"\n  }\n }\n}\n`);
  put(dir, `${P}/${name}_Base.et.meta`, meta("3333333333333333", `${P}/${name}_Base.et`));
  put(dir, `${P}/${name}.et`, `SCR_DestructibleBuildingEntity : "{3333333333333333}${P}/${name}_Base.et" {\n ID "EA275703F7D144D9"\n}\n`);
  put(dir, `${P}/${name}.et.meta`, meta("4444444444444444", `${P}/${name}.et`));
  put(dir, `${A}/EXPORT_INFO.txt`, `Licensed to: ${FAKE_BUYER} [IND]\n`);
  put(dir, "addon.gproj", 'GameProject {\n ID "export"\n}\n');
  put(dir, "ArchBuilder_import.txt", `${A}/${name}.fbx\n`);
  put(dir, "Scripts/WorkbenchGame/ArchBuilder/ArchBuilderImportPlugin.c", "// generated\n");
}

const SPEC = {
  slug: "shop", name: "Shop", concept: "A test shop.", red_line: "pack",
  archbuilder: { archetype: "SUPERMARKET", region: "AUTO", seed: 1989, storeys: 1, width: 18, depth: 14 },
  budget: { tris: 60000, footprint_m: [19, 15], height_m: 9, min_doors: 1 },
  asset_dir: "Assets/Buildings/Shop", prefab_dir: "Prefabs/Buildings/Shop",
};

export async function main() {
  let failed = 0;
  const ok = (cond, what) => {
    if (cond) return;
    failed++;
    console.log(`FAIL ${what}`);
  };
  const tmp = mkdtempSync(join(tmpdir(), "lf-building-"));
  const savedEnv = { ...process.env };
  try {
    // ---- spec
    const committed = JSON.parse(readFileSync(join(ROOT, "tools", "buildings", "supermarket", "spec.json"), "utf8"));
    ok(specProblems(committed).length === 0, `spec: the committed supermarket spec passes (${specProblems(committed).join("; ")})`);
    ok(specProblems(SPEC).length === 0, "spec: the fixture spec passes");
    const bad = (patch, re, what) => ok(specProblems({ ...SPEC, ...patch, archbuilder: { ...SPEC.archbuilder, ...(patch.archbuilder || {}) } }).some((p) => re.test(p)), `spec: ${what}`);
    bad({ archbuilder: { proportions: "REAL" } }, /REFORGER/, "real-world proportions are refused (the character does not fit the doors)");
    bad({ archbuilder: { vanilla_openings: false } }, /vanilla_openings/, "own doors and windows are refused");
    bad({ export: { light_system: true } }, /light_system/, "the light-system script (ArchBuilder's class names) is refused");
    bad({ budget: { ...SPEC.budget, min_doors: 0 } }, /min_doors/, "a building with no door is refused");
    bad({ asset_dir: "Assets/ArchBuilder/Shop" }, /asset_dir/, "asset_dir must be under Assets/Buildings");
    bad({ red_line: undefined }, /red_line/, "every building is classified core or pack");
    bad({ name: "Materials", asset_dir: "Assets/Buildings/Materials" }, /Materials/, "a building called Materials is refused (the shared materials folder)");
    bad({ export: { furniture: false, vanilla_props: true } }, /records no vanilla props/, "vanilla props with furniture off are refused (ArchBuilder then records none, the shop sign included)");
    bad({ archbuilder: { do_furniture: false }, export: { furniture: true } }, /do_furniture/, "exporting furniture from a building generated without it is refused");
    ok(!specProblems({ ...SPEC, export: { furniture: false, vanilla_props: false } }).some((p) => /furniture/.test(p)), "spec: a deliberately bare shell (vanilla_props false) passes");

    // ---- export report
    const furn = [
      "GenericEntity : \"{28411B0C79460DF8}Prefabs/Structures/Core/BuildingFurniture_base.et\" {",
      " {",
      "  GenericEntity : \"{AAAAAAAAAAAAAAAA}Prefabs/Props/Commercial/ShelfShop_01.et\" {",
      "  }",
      "  GenericEntity : \"{AAAAAAAAAAAAAAAA}Prefabs/Props/Commercial/ShelfShop_01.et\" {",
      "  }",
      "  GenericEntity : \"{BBBBBBBBBBBBBBBB}Prefabs/Structures/BuildingParts/Signs/Commercial/SignCommercial_01_GroceryStore.et\" {",
      "  }",
      " }",
      "}",
    ].join("\n") + "\n";
    const props = countProps(furn);
    ok(props.total === 3 && props.kinds.ShelfShop_01 === 2 && !props.kinds.BuildingFurniture_base, `export: props are the furniture prefab's children, not its parent (${JSON.stringify(props)})`);
    const clean = { warnings: [], props: 3, props_mesh: {} };
    ok(exportProblems(SPEC, clean, [], props).length === 0, `export: a clean report passes (${exportProblems(SPEC, clean, [], props).join("; ")})`);
    ok(exportProblems(SPEC, null).length === 1, "export: no report from the export fails");
    ok(exportProblems(SPEC, { ...clean, warnings: ["Sin prefab vanilla para win (90, 142): hueco vacio"] }, [], props).length === 1, "export: an ArchBuilder warning fails the stage");
    ok(exportProblems(SPEC, { ...clean, props_mesh: { cooler: 2 } }, [], props).some((p) => /2 cooler/.test(p)), "export: furniture ArchBuilder left out of the game fails, by kind");
    ok(exportProblems(SPEC, { ...clean, props: 0 }, [], { total: 0, kinds: {} }).some((p) => /no vanilla props/.test(p)), "export: furniture on but no vanilla props fails");
    ok(reviewProblems(SPEC, { tris: 70000, footprint_m: [18, 14], height_m: 8 }).length === 1, "review: over the triangle budget fails");
    ok(reviewProblems(SPEC, { tris: 3000, footprint_m: [18, 14], height_m: 8 }).length === 0, "review: inside the budget passes");

    // ---- map
    ok(targetPath("Assets/ArchBuilder/Shop/Shop.fbx", SPEC) === "Assets/Buildings/Shop/Shop.fbx", "map: the model lands in asset_dir");
    ok(targetPath("Prefabs/ArchBuilder/Shop/Furniture/Shop_furniture_01.et", SPEC) === "Prefabs/Buildings/Shop/Furniture/Shop_furniture_01.et", "map: prefabs land in prefab_dir");
    ok(targetPath("Assets/ArchBuilder/Materials/AB_Oak.emat", SPEC) === "Assets/Buildings/Materials/AB_Oak.emat", "map: materials are shared under Assets/Buildings/Materials");
    ok(targetPath("Scripts/Game/Foo.c", SPEC) === null, "map: anything outside Assets/ and Prefabs/ArchBuilder is not placed");
    for (const f of ["Assets/ArchBuilder/Shop/EXPORT_INFO.txt", "addon.gproj", "ArchBuilder_import.txt", "Scripts/WorkbenchGame/ArchBuilder/ArchBuilderImportPlugin.c"]) {
      ok(skipped(f), `map: ${f} is never copied${f.includes("EXPORT_INFO") ? " (EXPORT_INFO.txt carries the buyer's name)" : ""}`);
    }

    // ---- guid
    const staging = join(tmp, "export");
    const addon = join(tmp, "addon");
    fixtureExport(staging);
    mkdirSync(addon, { recursive: true });
    const used = new Set(["5555555555555555"]);
    let seq = 0;
    const random = () => Buffer.from(seq++ === 0 ? "5555555555555555" : (0x6000000000000000n + BigInt(seq)).toString(16), "hex"); // first draw collides
    const plan = planImport({ staging, spec: SPEC, addonDir: addon, used, random });
    ok(plan.problems.length === 0, `guid: the fixture plans cleanly (${plan.problems.join("; ")})`);
    const newGuids = [...plan.map.values()].map((r) => r.guid);
    ok(plan.map.size === 4, `guid: four resources (model, material, two prefabs), got ${plan.map.size}`);
    ok(newGuids.every((g) => !/^(1111|2222|3333|4444)/.test(g)), "guid: every resource gets a GUID the export did not have");
    ok(!newGuids.includes("5555555555555555"), "guid: a fresh GUID is never one the repo holds");
    ok(new Set(newGuids).size === newGuids.length, "guid: no two resources share a GUID");
    const text = plan.files.filter((f) => f.text !== undefined).map((f) => f.text).join("\n");
    ok(!/ArchBuilder\//.test(text), "guid: no file names an ArchBuilder path after the move");
    ok(!/\{(1111111111111111|2222222222222222|3333333333333333|4444444444444444)\}/.test(text), "guid: no reference keeps a staging GUID");
    ok(text.includes("{A43A100E3C377DB2}Prefabs/Structures/Core/Building_Base.et") && text.includes("{B393F062AAE9EB74}Common/Materials/Game/brick.gamemat"), "guid: base-game references are left as they are");
    ok(text.includes('MeshObject "{506EDA9D46FC6C06}"'), "guid: inherited component GUIDs are not touched (they must equal the base's)");
    ok(plan.external.some((e) => e.path === "Prefabs/Structures/BuildingParts/Doors/DoorSet_Village_E_01_R_EXT_COV_B.et"), "guid: base-game references are listed for the prefab check");
    const base = plan.files.find((f) => f.to === "Prefabs/Buildings/Shop/Shop_Base.et").text;
    const model = plan.map.get("1111111111111111");
    ok(base.includes(`{${model.guid}}Assets/Buildings/Shop/Shop.xob`), "guid: the prefab names the model by its new GUID and path");
    writePlan(addon, plan);
    const again = planImport({ staging, spec: SPEC, addonDir: addon, used: new Set([...used, ...newGuids]) });
    ok([...again.map.values()].every((r) => r.kept && r.guid === plan.map.get([...plan.map.keys()].find((k) => plan.map.get(k).oldPath === r.oldPath)).guid), "guid: a re-import keeps every GUID (saved worlds keep resolving)");
    // a changed shared material is refused
    put(staging, "Assets/ArchBuilder/Materials/AB_Brick_Red.emat", 'MatPBRBasic {\n BCRMap "{0000000000000001}Assets/Other.edds"\n}\n');
    ok(!planImport({ staging, spec: SPEC, addonDir: addon, used: new Set() }).problems.some((p) => /shared by other buildings/.test(p)), "guid: a material only this building uses may change (a recolour)");
    put(addon, "Assets/Buildings/Other/Other.xob.meta", `MetaFileClass {
 Name "{4444444444444444}Assets/Buildings/Other/Other.xob"
  AssignedMaterial "{0000000000000002}Assets/Buildings/Materials/AB_Brick_Red.emat"
}
`);
    ok(planImport({ staging, spec: SPEC, addonDir: addon, used: new Set() }).problems.some((p) => /shared by other buildings/.test(p)), "guid: an export that changes a material another building uses is refused");
    fixtureExport(staging);
    put(staging, "Prefabs/ArchBuilder/Shop/Extra.et", 'GenericEntity : "{9999999999999999}Prefabs/ArchBuilder/Gone/Gone.et" {\n}\n');
    ok(planImport({ staging, spec: SPEC, addonDir: addon, used: new Set() }).problems.some((p) => /did not write/.test(p)), "guid: a reference to an ArchBuilder resource the export lacks is refused");
    rmSync(join(staging, "Prefabs/ArchBuilder/Shop/Extra.et"));
    put(staging, "Sounds/stray.wav", "x");
    ok(planImport({ staging, spec: SPEC, addonDir: addon, used: new Set() }).problems.some((p) => /unexpected file/.test(p)), "guid: an unexpected file in the export is refused");
    rmSync(join(staging, "Sounds"), { recursive: true });
    put(staging, "Assets/ArchBuilder/Doors/Door_A.fbx", "binary fbx stand-in");
    ok(planImport({ staging, spec: SPEC, addonDir: addon, used: new Set() }).problems.some((p) => /only shared \.emat materials/.test(p)), "guid: a model other buildings would share is refused (not supported yet)");
    rmSync(join(staging, "Assets/ArchBuilder/Doors"), { recursive: true });
    put(staging, "Prefabs/ArchBuilder/Shop/Shop.layer", 'GenericEntity Shop_1 : "{4444444444444444}Prefabs/ArchBuilder/Shop/Shop.et" {\n}\n');
    const layered = planImport({ staging, spec: SPEC, addonDir: addon, used: new Set() });
    const layerText = layered.files.find((f) => f.to.endsWith("Shop.layer"))?.text || "";
    ok(layerText.includes("Prefabs/Buildings/Shop/Shop.et") && !layerText.includes("4444444444444444"), "guid: a text file of any extension (.layer) is rewritten too");
    put(staging, "Prefabs/ArchBuilder/Shop/Shop.layer", 'GenericEntity Shop_1 {\n Ref "{4444444444444444}"\n}\n');
    ok(planImport({ staging, spec: SPEC, addonDir: addon, used: new Set() }).problems.some((p) => /staging GUID 4444444444444444/.test(p)), "guid: a bare staging GUID left in a file is refused");
    rmSync(join(staging, "Prefabs/ArchBuilder/Shop/Shop.layer"));

    // ---- watermark
    const ext = join(tmp, "ext");
    put(ext, "__init__.py", "");
    put(ext, "LICENSE.json", JSON.stringify({ product: "ABLD", user: FAKE_BUYER, key: FAKE_KEY, edition: "IND" }));
    process.env.LF_ARCHBUILDER_DIR = ext;
    const marks = licenseMarks(archBuilderSetup());
    ok(marks.includes(FAKE_BUYER) && marks.includes(FAKE_KEY), "watermark: the buyer id and key are read from LICENSE.json");
    put(tmp, "w/a.et", `// made for ${FAKE_BUYER.toUpperCase()}\n`);
    put(tmp, "w/b.fbx", Buffer.concat([Buffer.from("FBX"), Buffer.from(FAKE_KEY, "utf16le")]));
    put(tmp, "w/c.et", "clean\n");
    put(tmp, "w/d.et", Buffer.from("// für JÖRG.MÜLLER\n", "utf8"));
    ok(scanForMarks([join(tmp, "w", "d.et")], ["Jörg.Müller"]).length === 1, "watermark: a non-ASCII buyer id written as UTF-8 is refused");
    const hits = scanForMarks(["a.et", "b.fbx", "c.et"].map((f) => join(tmp, "w", f)), marks).map((f) => f.split(/[\\/]/).pop());
    ok(hits.includes("a.et"), "watermark: a file with the buyer id in another case is refused");
    ok(hits.includes("b.fbx"), "watermark: a binary file with the key as UTF-16 is refused");
    ok(!hits.includes("c.et"), "watermark: a clean file passes");
    put(staging, "Prefabs/ArchBuilder/Shop/Shop.et", `SCR_DestructibleBuildingEntity : "{3333333333333333}Prefabs/ArchBuilder/Shop/Shop_Base.et" {\n // ${FAKE_BUYER}\n}\n`);
    let refused = "";
    try {
      await importBuilding({ root: tmp, spec: SPEC, dir: tmp, workbench: () => { throw new Error("Workbench must not start"); }, licenseMarks: () => marks });
    } catch (e) {
      refused = e.message;
    }
    ok(/watermark/.test(refused), `watermark: the import refuses a watermarked export before Workbench (got: ${refused.split("\n")[0]})`);
    ok(!refused.includes(FAKE_BUYER) && !refused.includes(FAKE_KEY), "watermark: the refusal never prints the mark itself");
    fixtureExport(staging);
    refused = "";
    try {
      await importBuilding({ root: tmp, spec: SPEC, dir: tmp, workbench: () => { throw new Error("Workbench must not start"); }, licenseMarks: () => [] });
    } catch (e) {
      refused = e.message;
    }
    ok(/no ArchBuilder licence file/.test(refused), `watermark: with no licence file to read the marks from, the import refuses (got: ${refused.split("\n")[0]})`);
    fixtureExport(staging);

    // ---- model checks
    const plannedMeta = plan.files.find((f) => f.to.endsWith(".xob.meta")).text;
    const surfaces = metaSurfaces(plannedMeta);
    const mats = metaMaterials(plannedMeta);
    const goodModel = { materials: [{ name: "AB_Brick_Red", path: mats[0] }], colliders: ["UTM_BD_brick"], strings: ["{B393F062AAE9EB74}Common/Materials/Game/brick.gamemat"] };
    ok(checkBuildingModel(goodModel, { surfaces, materials: mats }).length === 0, "model: a model that bakes what the .meta asks passes");
    ok(checkBuildingModel({ ...goodModel, strings: ["{536BF67B2052B869}material/metal.gamemat"] }, { surfaces, materials: mats }).length === 2, "model: Workbench's default surface in place of the planned one fails");
    ok(checkBuildingModel({ ...goodModel, materials: [{ name: "AB_Brick_Red", path: "" }] }, { surfaces, materials: mats }).length === 1, "model: a material that baked nothing fails");
    const cut = plannedMeta.replace(/    GeometryParam UTM_BD_brick \{[\s\S]*?\n    \}\n/, "");
    ok(metaDrift(plannedMeta, cut).length === 1, "model: a collider Workbench dropped from the re-saved .meta is caught");
    ok(metaDrift(plannedMeta, plannedMeta).length === 0, "model: an unchanged .meta has no drift");
    ok(JSON.stringify(countOpenings(base)) === JSON.stringify({ doors: 1, windows: 1, probes: 0 }), `model: openings are counted from the prefab (${JSON.stringify(countOpenings(base))})`);

    // ---- game database
    const db = [Buffer.concat([rdbRecord("Prefabs/Structures/Core/Building_Base.et", "A43A100E3C377DB2"), rdbRecord("X/Prefabs/Structures/Core/Other.et", "1234123412341234")])];
    ok(guidIn(db, "Prefabs/Structures/Core/Building_Base.et") === "A43A100E3C377DB2", "gamedb: a path's GUID is read");
    ok(guidIn(db, "Prefabs/Structures/Core/Other.et") === undefined, "gamedb: a longer path that ends the same does not count");
    const fakeDb = { guidOf: (p) => guidIn(db, p) };
    ok(badGameRefs([["Prefabs/Structures/Core/Building_Base.et", "A43A100E3C377DB2"]], fakeDb).length === 0, "gamedb: a right path and GUID pass");
    ok(/GUID for it is/.test(badGameRefs([["Prefabs/Structures/Core/Building_Base.et", "0000000000000001"]], fakeDb)[0] || ""), "gamedb: a wrong GUID is caught");
    ok(/has no/.test(badGameRefs([["Prefabs/Nope.et", "0000000000000001"]], fakeDb)[0] || ""), "gamedb: a missing path is caught");
    ok(badGameRefs([], null) === null, "gamedb: no database means the check skips, not passes");

    // ---- skip: a machine without ArchBuilder (CI)
    process.env.LF_ARCHBUILDER_DIR = join(tmp, "nowhere");
    // CI has no Blender either: point at a file that exists so the add-on check is the one that fails.
    process.env.LF_BLENDER_ARCHBUILDER = process.execPath;
    ok(!archBuilderSetup().ok && /arribas22\.github\.io\/archbuilder/.test(archBuilderSetup().why), "skip: the missing add-on is reported with where to get it");
    const root = join(tmp, "repo");
    put(root, "tools/buildings/shop/spec.json", JSON.stringify(SPEC));
    mkdirSync(join(root, "addons", "LifeFramework"), { recursive: true });
    const lines = [];
    const log = console.log;
    console.log = (...a) => lines.push(a.join(" "));
    let code;
    try {
      code = await run(root, "shop", { workbench: () => { throw new Error("no Workbench in the selftest"); } });
    } finally {
      console.log = log;
    }
    const out = lines.join("\n");
    for (const s of ["generate", "review", "export"]) ok(new RegExp(`SKIP  ${s}: the ArchBuilder add-on is not installed`).test(out), `skip: ${s} skips without ArchBuilder`);
    ok(/FAIL  import: no ArchBuilder export/.test(out) && code === 1, "skip: import then says the export must come from the ArchBuilder machine");
    const state = JSON.parse(readFileSync(join(root, "tmp", "buildings", "shop", "state.json"), "utf8"));
    ok(state.stages.spec.ok && state.stages.generate.skipped, "skip: status records the skip, not a failure");
    ok(STAGES.join() === "spec,generate,review,export,import,prefab", "stages: the order the docs describe");
  } finally {
    for (const k of Object.keys(process.env)) if (!(k in savedEnv)) delete process.env[k];
    Object.assign(process.env, savedEnv);
    rmSync(tmp, { recursive: true, force: true });
  }
  if (failed) {
    console.log(`building-pipeline-selftest: ${failed} FAILED`);
    return 1;
  }
  console.log("building-pipeline-selftest: OK");
  return 0;
}

if (process.argv[1] && import.meta.url === pathToFileURL(process.argv[1]).href) process.exit(await main());
