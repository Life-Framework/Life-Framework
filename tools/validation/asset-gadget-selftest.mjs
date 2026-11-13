// Proves the gadget kind of tools\cli asset offline (tools/assets/pipeline/prefab-gadget.mjs and the
// gadget rules in checks.mjs): the scaffold text, the prefab stage's dispatch to it, the size check,
// and the handcuffs this kind shipped with their generation record (#664). No Meshy, no Blender, no Workbench.
// red-proof: 2026-11-13, one line broken per block, each restored after its FAIL was seen:
//   gadgetPrefab with a fresh InventoryItemComponent id -> FAIL gadget: the inventory item overrides Item_Base's component by its id
//   writePrefabs without the prefab-<kind>.mjs dispatch  -> FAIL gadget: the prefab stage writes the gadget scaffold
//   checkReport with the pistol tolerances for a gadget  -> FAIL gadget: a gadget 10 % off its length is flagged
//   no tools/assets/handcuffs/meshy.json (before it was written) -> FAIL gadget: the handcuffs record their accepted Meshy generation id
import { existsSync, mkdirSync, mkdtempSync, readFileSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";
import { checkReport } from "../assets/pipeline/checks.mjs";
import { KINDS } from "../assets/pipeline/kinds.mjs";
import { DEFAULT_HAND, VANILLA, gadgetPrefab, previewDistance } from "../assets/pipeline/prefab-gadget.mjs";
import { writePrefabs } from "../assets/pipeline/prefab.mjs";
import { parse } from "../assets/pipeline/xob.mjs";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");

export async function main() {
  let failed = 0;
  const ok = (cond, what) => {
    if (cond) return;
    failed++;
    console.log(`FAIL ${what}`);
  };
  const gadget = KINDS.gadget;
  const report = () => ({
    size: [0.18, 0.032, 0.1],
    bounds: { min: [-0.05, -0.016, -0.09], max: [0.05, 0.016, 0.09] },
    uv: { min: 0, max: 1 }, textures: { found: ["base_color", "normal"] },
    lods: [{ name: "Cuffs_LOD0", tris: 3000 }, { name: "Cuffs_LOD1", tris: 1500 }],
    collider: { name: "UBX_Cuffs", usage: "ItemFireView", faces: 6 },
    parts: [], bones: [], sockets: [],
  });
  const spec = () => ({
    slug: "cuffs", name: "Cuffs", kind: "gadget", size_m: { length: 0.18, height: 0.032, width: 0.1 },
    budget: { lod0_tris: 3000, lod_ratios: [0.5], texture_px: 1024, credits: 30 },
    asset_dir: "Assets/Items/Roleplay/Cuffs", prefab_dir: "Prefabs/Items/Roleplay",
    gadget: { name_key: "#N", description_key: "#D", weight: 0.3, volume: 150, components: [{ class: "LF_HandcuffsComponent", props: {} }] },
  });

  // ---- scaffold text: a held item on the vanilla base ---------------------------------------------
  {
    const g = spec().gadget;
    const guids = { anim: "A1", attach: "A2", preview: "A3", mesh: "A4", body: "A5", gadget: "A6", components: ["A7"] };
    const text = gadgetPrefab({ id: "AAAAAAAAAAAAAAAA", xob: "{1}x.xob", g, report: report(), guids });
    ok(text.startsWith(`GenericEntity : "${VANILLA.itemBase}"`), "gadget: the item inherits the vanilla Item_Base.et");
    ok(text.includes('InventoryItemComponent "{5222EB4D0C73006B}"') && text.includes('Attributes SCR_ItemAttributeCollection "{5222EB4D0A2B466B}"'), "gadget: the inventory item overrides Item_Base's component by its id");
    ok(text.includes("ItemPhysAttributes ItemPhysicalAttributes PhysicalAttributes {") && /SizeSetupStrategy Volume\n\s+ItemVolume 150\n/.test(text), "gadget: the volume is set (validate-item-volume) on the inherited physical attributes");
    ok(text.includes(`AnimGraph "${VANILLA.handGraph}"`) && text.includes('BindingName "Gadget"') && text.includes("m_eAnimVariable LF_ITEM"), "gadget: the item is held through the vanilla gadget binding");
    ok(text.includes(`HandAdditiveRotation ${DEFAULT_HAND.rotation.join(" ")}`), "gadget: the default hand pose is the lockpick's");
    ok(gadgetPrefab({ id: "A", xob: "{1}x.xob", g: { ...g, hand: { rotation: [1, 2, 3] } }, report: report(), guids }).includes("HandAdditiveRotation 1 2 3"), "gadget: the spec's hand pose is written");
    ok(/MeshObject "\{A4\}" \{\n\s+Object "\{1\}x\.xob"/.test(text), "gadget: the mesh is the imported model");
    ok(text.includes('LF_HandcuffsComponent "{A7}" {'), "gadget: the spec's extra component is written");
    ok(text.includes(`CameraDistanceToItem ${previewDistance(report())}`) && previewDistance({ size: [0.01, 0.01, 0.01] }) === 0.25, "gadget: the preview camera stands back three lengths, at least 25 cm");
    ok((text.match(/{/g) || []).length === (text.match(/}/g) || []).length, "gadget: the braces balance");
  }

  // ---- the prefab stage dispatches to prefab-gadget.mjs and writes once ----------------------------
  {
    const root = mkdtempSync(join(tmpdir(), "lf-asset-gadget-"));
    const addon = join(root, "addons", "LifeFramework");
    const s = spec();
    mkdirSync(join(addon, s.asset_dir), { recursive: true });
    writeFileSync(join(addon, s.asset_dir, "Cuffs.xob"), "model");
    writeFileSync(join(addon, s.asset_dir, "Cuffs.xob.meta"), 'MetaFileClass {\n Name "{0123456789ABCDEF}Assets/Items/Roleplay/Cuffs/Cuffs.xob"\n}\n');
    const dir = join(root, "tmp", "assets", "cuffs");
    mkdirSync(join(dir, "clean"), { recursive: true });
    writeFileSync(join(dir, "clean", "report.json"), JSON.stringify(report()));
    const log = console.log;
    console.log = () => {};
    let ref;
    try {
      ref = await writePrefabs({ root, spec: s, dir });
    } finally {
      console.log = log;
    }
    const et = join(addon, "Prefabs", "Items", "Roleplay", "Cuffs.et");
    ok(existsSync(et) && existsSync(et + ".meta") && /^\{[0-9A-F]{16}\}Prefabs\/Items\/Roleplay\/Cuffs\.et$/.test(ref?.gadget || ""), `gadget: the prefab stage writes the gadget scaffold (got ${JSON.stringify(ref)})`);
    if (existsSync(et)) {
      ok(readFileSync(et, "utf8").includes("{0123456789ABCDEF}Assets/Items/Roleplay/Cuffs/Cuffs.xob"), "gadget: the scaffold names the imported model by its GUID");
      writeFileSync(et, readFileSync(et, "utf8").replace("Weight 0.3", "Weight 0.4"));
      console.log = () => {};
      try {
        await writePrefabs({ root, spec: s, dir });
      } finally {
        console.log = log;
      }
      ok(readFileSync(et, "utf8").includes("Weight 0.4"), "gadget: a second run leaves an edited prefab as it is");
    }
  }

  // ---- size: a gadget is scaled on its length, so the length is the tight axis ----------------------
  {
    const s = spec();
    ok(checkReport(s, gadget, report()).length === 0, `gadget: a good clean passes (${checkReport(s, gadget, report())[0]})`);
    const r = report();
    r.size[0] = 0.198;
    ok(checkReport(s, gadget, r).some((p) => /length is 0\.198 m/.test(p)), "gadget: a gadget 10 % off its length is flagged");
    const thick = report();
    thick.size[1] = 0.04;
    ok(!checkReport(s, gadget, thick).some((p) => /height/.test(p)), "gadget: a gadget 25 % thicker than the spec passes; only the length is scaled to fit");
  }

  // ---- the handcuffs this kind shipped, and their generation record (the #664 sign-off) -----------
  {
    const xob = join(ROOT, "addons", "LifeFramework", "Assets", "Items", "Roleplay", "Handcuffs", "Handcuffs.xob");
    ok(existsSync(xob), "gadget: the handcuffs model is in the addon");
    if (existsSync(xob)) {
      const model = parse(readFileSync(xob));
      ok(model.colliders.includes("UBX_Handcuffs"), "gadget: the handcuffs have their box collider");
      ok(model.materials.some((m) => /Handcuffs\/Data\/Handcuffs\.emat$/.test(m.path)), "gadget: the handcuffs bake their own material");
    }
    const meshyFile = join(ROOT, "tools", "assets", "handcuffs", "meshy.json");
    const rec = existsSync(meshyFile) ? JSON.parse(readFileSync(meshyFile, "utf8")) : { generations: [] };
    ok(rec.generations.some((g) => /^[0-9a-f-]{36}$/.test(g.refineId || "") && g.accepted === true), "gadget: the handcuffs record their accepted Meshy generation id");
  }

  if (failed) {
    console.log(`asset-gadget-selftest: FAILED (${failed})`);
    return 1;
  }
  console.log("asset-gadget-selftest: OK");
  return 0;
}

if (process.argv[1] && import.meta.url === pathToFileURL(process.argv[1]).href) process.exit(await main());
