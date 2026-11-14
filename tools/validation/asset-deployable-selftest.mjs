// Proves the deployable kind of tools\cli asset offline (tools/assets/pipeline/prefab-deployable.mjs,
// inputs: tools/**, agent-box/**, .githooks/**, addons/**, docs/**, website/**, video/**
// the deployable profile in kinds.mjs, the import log filter): no Meshy, no Blender, no Workbench.
// The spike strip (tools/assets/spike-strip) is the worked example.
// red-proof: 2026-11-13, one line broken per block, each restored after its FAIL was seen:
//   deployableProblems without the item_actions test -> FAIL spec: a deployable nothing can deploy is refused
//   props() ignoring "$item"                         -> FAIL prefab: "$item" becomes the item prefab's {GUID}path
//   itemPrefab with a fresh ActionsManager GUID      -> FAIL prefab: the item overrides Item_Base's actions by GUID
//   writeDeployablePrefabs ignoring the variant      -> FAIL scaffold: the item carries the folded model
//   logProblems without the not-built-yet filter     -> FAIL log: a sibling's not-yet-built .meta is not a problem
import { existsSync, mkdirSync, mkdtempSync, readFileSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { pathToFileURL } from "node:url";
import { logProblems } from "../assets/pipeline/import.mjs";
import { KINDS } from "../assets/pipeline/kinds.mjs";
import { deployableProblems, deployedPrefab, itemPrefab, props, writeDeployablePrefabs } from "../assets/pipeline/prefab-deployable.mjs";

export async function main() {
  let failed = 0;
  const ok = (cond, what) => {
    if (cond) return;
    failed++;
    console.log(`FAIL ${what}`);
  };
  const balanced = (text) => (text.match(/{/g) || []).length === (text.match(/}/g) || []).length;
  const deployable = () => ({
    name_key: "#LF-Strip_Name", description_key: "#LF-Strip_Description", weight: 6, item_volume: 2500,
    components: [{ class: "LF_SpikeStripComponent", props: { m_sItemPrefab: "$item", m_fHalfLength: 1.75 } }],
    actions: [{ class: "LF_SpikeStripPickUpAction" }],
    item_components: [{ class: "LF_PlaceableComponent", props: { m_sPlacedPrefab: "$deployed" } }],
    item_actions: [{ class: "LF_SpikeStripDeployAction" }],
  });
  let n = 0;
  const guid = () => String(++n).padStart(16, "A");
  const bounds = { min: [-0.15, 0, -1.75], max: [0.15, 0.06, 1.75] };
  const refs = { deployed: "{1111111111111111}Prefabs/X/Strip.et", item: "{2222222222222222}Prefabs/X/Strip_Item.et" };

  // ---- the profile ------------------------------------------------------------------------------
  {
    const k = KINDS.deployable;
    ok(k.proven && k.fit === "ground" && k.collider.prefix === "UBX_", "kind: the deployable stands on the ground with a box collider");
    ok(k.collider.maxHeight > 0 && k.collider.maxHeight <= 0.05, "kind: the deployable's collider is a thin box a wheel rolls over");
  }

  // ---- spec ---------------------------------------------------------------------------------------
  {
    ok(deployableProblems({ deployable: deployable() }).length === 0, `spec: a complete deployable passes (${deployableProblems({ deployable: deployable() })[0]})`);
    ok(deployableProblems({}).length === 1, "spec: a deployable with no deployable block is refused");
    const none = deployable();
    none.item_actions = [];
    ok(deployableProblems({ deployable: none }).some((p) => /nothing would deploy/.test(p)), "spec: a deployable nothing can deploy is refused");
    const key = deployable();
    key.name_key = "LF-Strip_Name";
    ok(deployableProblems({ deployable: key }).some((p) => /string table key/.test(p)), "spec: a name without the # is refused");
  }

  // ---- prefab text ------------------------------------------------------------------------------
  {
    ok(props({ m_sItemPrefab: "$item", m_fHalfLength: 1.75, m_sName: "plain" }, " ", refs) === ` m_sItemPrefab "${refs.item}"\n m_fHalfLength 1.75\n m_sName "plain"\n`, "prefab: \"$item\" becomes the item prefab's {GUID}path");
    const strip = deployedPrefab({ id: "ABCDABCDABCDABCD", xob: "{3}Assets/X/Strip.xob", d: deployable(), bounds, guid, refs });
    ok(strip.startsWith("GenericEntity {\n ID \"ABCDABCDABCDABCD\""), "prefab: the deployed prop is a standalone entity");
    ok(/MeshObject "\{[0-9A-Z]+\}" \{\n   Object "\{3\}Assets\/X\/Strip\.xob"/.test(strip), "prefab: the deployed prop shows the model");
    ok(/RigidBody "\{[0-9A-Z]+\}" \{\n   ModelGeometry 1\n   Static 1\n/.test(strip), "prefab: the deployed prop is a static body on the model's collider");
    ok(strip.includes(`LF_SpikeStripComponent "{`) && strip.includes(`m_sItemPrefab "${refs.item}"`), "prefab: the feature component names what it folds back into");
    ok(strip.includes("Radius 2.15\n") && strip.includes("Offset 0 0.06 0\n"), "prefab: the action context sits on top of the middle and reaches both ends");
    ok(/LF_SpikeStripPickUpAction "\{[0-9A-Z]+\}" \{\n     ParentContextList \{\n      "default"/.test(strip), "prefab: the world action is in the default context");
    ok(strip.includes("RplComponent"), "prefab: the deployed prop replicates");
    ok(balanced(strip), "prefab: the deployed prop's braces balance");

    const item = itemPrefab({ id: "ABCDABCDABCDABCE", xob: "{4}Assets/X/Strip_Folded.xob", d: deployable(), bounds: { min: [-0.15, 0, -0.25], max: [0.15, 0.42, 0.25] }, guid, refs });
    ok(item.startsWith('GenericEntity : "{8B993DEBBF41116C}Prefabs/Items/Core/RoleplayItem_Base.et"'), "prefab: the item inherits RoleplayItem_Base");
    ok(item.includes('ActionsManagerComponent "{508AB5952B584B2E}"') && item.includes('UserActionContext "{508AC2ABEC97066C}"'), "prefab: the item overrides Item_Base's actions by GUID");
    ok(item.includes('ItemDisplayName UIInfo "{5222EB4D07D865FA}"') && item.includes("ItemPhysAttributes ItemPhysicalAttributes PhysicalAttributes {"), "prefab: the item overrides the base's name and physical attributes by id");
    ok(/Weight 6\n\s+SizeSetupStrategy Volume\n\s+ItemVolume 2500\n/.test(item), "prefab: the item's weight and volume come from the spec");
    ok(item.includes(`m_sPlacedPrefab "${refs.deployed}"`) && item.includes("LF_SpikeStripDeployAction"), "prefab: the item names what it deploys into and carries the deploy action");
    ok(item.includes("SCR_GadgetComponent"), "prefab: the item has a gadget component (LF_Test_ItemVolume: or it takes no room)");
    ok(balanced(item), "prefab: the item's braces balance");
  }

  // ---- scaffold: both files from an import, the folded model on the item -------------------------
  {
    const root = mkdtempSync(join(tmpdir(), "lf-deployable-selftest-"));
    const addon = join(root, "addons", "LifeFramework");
    const dir = join(root, "tmp", "assets", "strip");
    const assetDir = "Assets/Props/Roleplay/Strip";
    mkdirSync(join(addon, assetDir), { recursive: true });
    mkdirSync(join(dir, "clean"), { recursive: true });
    for (const [name, g] of [["Strip", "5555555555555555"], ["Strip_Folded", "6666666666666666"]]) {
      writeFileSync(join(addon, assetDir, `${name}.xob`), "");
      writeFileSync(join(addon, assetDir, `${name}.xob.meta`), `MetaFileClass {\n Name "{${g}}${assetDir}/${name}.xob"\n}\n`);
    }
    writeFileSync(join(dir, "clean", "report.json"), JSON.stringify({ bounds, variants: [{ name: "Strip_Folded", bounds: { min: [-0.15, 0, -0.25], max: [0.15, 0.42, 0.25] } }] }));
    const spec = { name: "Strip", kind: "deployable", asset_dir: assetDir, prefab_dir: "Prefabs/Props/Roleplay/Strip", deployable: deployable() };
    const log = console.log;
    console.log = () => {};
    let ref;
    try {
      ref = await writeDeployablePrefabs({ root, spec, dir });
    } finally {
      console.log = log;
    }
    const read = (rel) => readFileSync(join(addon, rel), "utf8");
    const stripText = read("Prefabs/Props/Roleplay/Strip/Strip.et");
    const itemText = read("Prefabs/Props/Roleplay/Strip/Strip_Item.et");
    ok(itemText.includes(`{6666666666666666}${assetDir}/Strip_Folded.xob`), "scaffold: the item carries the folded model");
    ok(stripText.includes(`{5555555555555555}${assetDir}/Strip.xob`), "scaffold: the deployed prop carries the strip model");
    ok(stripText.includes(`m_sItemPrefab "${ref.item}"`) && itemText.includes(`m_sPlacedPrefab "${ref.deployed}"`), "scaffold: the two prefabs name each other by {GUID}path");
    ok(read("Prefabs/Props/Roleplay/Strip/Strip_Item.et.meta").includes(`Name "${ref.item}"`), "scaffold: the item's .meta holds the GUID the prop names");
    writeFileSync(join(addon, "Prefabs/Props/Roleplay/Strip/Strip.et"), stripText + "// edited by hand\n");
    console.log = () => {};
    try {
      await writeDeployablePrefabs({ root, spec, dir });
    } finally {
      console.log = log;
    }
    ok(read("Prefabs/Props/Roleplay/Strip/Strip.et").endsWith("// edited by hand\n"), "scaffold: a second run leaves a prefab that exists alone");
    ok(existsSync(join(addon, "Prefabs/Props/Roleplay/Strip/Strip.et.meta")), "scaffold: the deployed prop has a .meta");
  }

  // ---- import log ---------------------------------------------------------------------------------
  {
    const log = [
      "12:00:02.000 SCRIPT       : LF_ASSET_IMPORT rebuilding C:/x/Data/Strip_BCR.tif",
      "12:00:02.100 RESOURCES (E): metafile without corresponding resource: '$LifeFramework:Assets/X/Strip_Folded.xob.meta'",
      "12:00:03.000 RESOURCES (W): Invalid collider UBX_Strip",
    ].join("\n");
    const problems = logProblems(log, "Strip");
    ok(problems.length === 1 && /Invalid collider/.test(problems[0]), `log: a sibling's not-yet-built .meta is not a problem (got ${JSON.stringify(problems)})`);
  }

  if (failed) {
    console.log(`asset-deployable-selftest: FAILED (${failed})`);
    return 1;
  }
  console.log("asset-deployable-selftest: OK");
  return 0;
}

if (process.argv[1] && import.meta.url === pathToFileURL(process.argv[1]).href) process.exit(await main());
