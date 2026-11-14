// Proves the clothing kind of tools\cli asset offline (docs/asset-pipeline/clothing.md): the spec rules,
// inputs: tools/**, agent-box/**, .githooks/**
// the Enfusion text reader and the prefab-chain merge, the EDDS unpacking (COPY and LZ4 mips), the route
// (a) material and prefab text, the checks against the vanilla garment, the route (b)/(c) model checks,
// the state-picture plan and its pair difference, and the committed fixture specs. No game paks, no
// Blender, no Workbench: it runs in CI.
// red-proof: 2026-11-14, one line broken per block, each restored after its FAIL was seen:
//   clothingPrefab writing a fresh BaseLoadoutClothComponent GUID -> FAIL prefab: the cloth component overrides vanilla's by its GUID
//   lz4Chunk copying matches with Buffer.copy (no self-overlap)   -> FAIL lz4: an overlapping match repeats the bytes before it
import { existsSync, readFileSync, readdirSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";
import {
  CLOTHING_STAGES, COLOUR_MAPS, SAME_LIMIT, clothingPrefab, clothingSpecProblems, ematOver, garmentProblems, isMotion,
  modelProblems, newClothingSpec, textureJobs, withMeshParams,
} from "../assets/pipeline/clothing.mjs";
import { SLOTS, describeGarment, eddsInfo, eddsToDds, lz4Block, mergedComponents, lookup, parseText, prefabChain } from "../assets/pipeline/clothing-vanilla.mjs";
import { GROUND_VIEWS, POSES, pairDiff, planText, stateNames } from "../assets/pipeline/clothing-shots.mjs";
import { prefabList } from "../../agent-box/previews.mjs";
import { stagesFor } from "../assets/pipeline/asset.mjs";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");
const BASE = "{E652D625A46A00FC}Prefabs/Characters/Uniforms/Jacket_Suit_01/Jacket_Suit_01_Grey_V1.et";

// A two-file vanilla chain, in the text layout game_read shows (2026-11-14), header split after the colon as in Uniform_Base.et.
const FILES = {
  "Prefabs/Characters/Uniforms/Jacket_Suit_01/Jacket_Suit_01_Grey_V1.et": `GameEntity : "{702F013CB1492541}Prefabs/Characters/Uniforms/Jacket_Suit_01/Jacket_Suit_01_base.et" {
 ID "5104869D194C6243"
 components {
  BaseLoadoutClothComponent "{5104869D194C627A}" {
   WornMaterialsOverride {
    "{99EBF808CF181832}Assets/X/Grey.emat"
   }
   ItemMaterialsOverride {
    "{F69B6D0D11861AEF}Assets/X/Item_Grey.emat"
   }
  }
 }
}`,
  "Prefabs/Characters/Uniforms/Jacket_Suit_01/Jacket_Suit_01_base.et": `GameEntity {
 ID "5104869D194C6243"
 components {
  MeshObject "{5104869D194C6262}" {
   Object "{3B76B43FE1866AC4}Assets/X/Jacket_item.xob"
  }
  SCR_UniversalInventoryStorageComponent "{5222CB01D5B9A306}" {
   Attributes SCR_ItemAttributeCollection "{5222CB01245CDF32}" {
    ItemDisplayName UIInfo "{5222CB0109A04AB2}" {
     Name "#AR-Suit_Jacket_Name"
    }
    CustomAttributes {
     PreviewRenderAttributes "{555292467DA11B7C}" {
      PreviewWornModel 1
     }
    }
   }
   MaxCumulativeVolume 600
  }
  BaseLoadoutClothComponent "{5104869D194C627A}" {
   AreaType LoadoutJacketArea "{5D83003BD88828FF}" {
   }
   WornModel "{7449DAACFEBE36CC}Assets/X/Jacket.xob"
   ItemModel "{3B76B43FE1866AC4}Assets/X/Jacket_item.xob"
   OverrideMeshesToHide {
    "Torso" "ArmUpper_L" "ArmUpper_R" "Arm_L" "Arm_R"
   }
  }
  ActionsManagerComponent "{5104869D194C6245}" {
   additionalActions {
    SCR_PickUpItemAction "{52E6EC76FD9968D7}" :
"{F39A4E9B190C1846}Prefabs/Characters/Core/Actions/PickUpItemAction.conf" {
    }
   }
  }
 }
}`,
  "Assets/X/Grey.emat": `MatPBRMulti : "{0FB93B007F4E3A39}Assets/_BaseMaterials/Characters_MatPBRMulti.emat" {\r\n BCR_1 "{7D0C1E898455C301}Assets/X/Grey_BCR.edds"\r\n NMO_1 "{28D9DE435E3A33E8}Assets/X/NMO.edds"\r\n}\r\n`,
  "Assets/X/Item_Grey.emat": `MatPBRBasic : "{5CDDE2709BA9A5EE}Assets/_BaseMaterials/Characters_MatPBRBasic.emat" {\n BCRMap "{0314AC4195799A44}Assets/X/Item_BCR.edds"\n}\n`,
  "Assets/_BaseMaterials/Characters_MatPBRMulti.emat": "MatPBRMulti {\n EnableFarHiding 1\n}\n",
  "Assets/_BaseMaterials/Characters_MatPBRBasic.emat": "MatPBRBasic {\n EnableFarHiding 1\n}\n",
};

// A synthetic EDDS: the DDS header with a DX10 extension, a mip table smallest first, then the mips.
function edds(mips, { dxgi = 99, size = 4 } = {}) {
  const head = Buffer.alloc(148);
  head.write("DDS ", 0, "latin1");
  head.writeUInt32LE(124, 4);
  head.writeUInt32LE(size, 12);
  head.writeUInt32LE(size, 16);
  head.writeUInt32LE(mips.length, 28);
  head.write("ENF1", 32, "latin1");
  head.write("DX10", 84, "latin1");
  head.writeUInt32LE(dxgi, 128);
  const table = Buffer.alloc(8 * mips.length);
  mips.forEach((m, i) => { table.write(m.kind, i * 8, "latin1"); table.writeUInt32LE(m.data.length, i * 8 + 4); });
  return Buffer.concat([head, table, ...mips.map((m) => m.data)]);
}

// "abcabcabcabcX" as one LZ4 chunk: literals "abc", a 9-byte match 3 back (it overlaps itself), then "X".
function lz4Sample() {
  const chunk = Buffer.from([0x35, 0x61, 0x62, 0x63, 0x03, 0x00, 0x10, 0x58]);
  const block = Buffer.alloc(8 + chunk.length);
  block.writeUInt32LE(13, 0);
  block.writeUInt32LE((chunk.length | 0x80000000) >>> 0, 4);
  chunk.copy(block, 8);
  return block;
}

function image(w, h, colour) {
  const px = Buffer.alloc(w * h * 3);
  for (let i = 0; i < w * h; i++) colour(i % w, Math.floor(i / w)).forEach((v, c) => (px[i * 3 + c] = v));
  return { w, h, channels: 3, px };
}

export async function main() {
  let failed = 0;
  const ok = (cond, what) => {
    if (cond) return;
    failed++;
    console.log(`FAIL ${what}`);
  };
  const data = { text: (p) => FILES[String(p).replace(/^\{[0-9A-F]{16}\}/, "")] ?? null, read: (p) => (FILES[String(p).replace(/^\{[0-9A-F]{16}\}/, "")] !== undefined ? Buffer.from(FILES[String(p).replace(/^\{[0-9A-F]{16}\}/, "")]) : null) };
  // An .xob or .edds the stub does not hold reads as null: describeGarment records it as missing.
  const stub = { text: data.text, read: (p) => (/\.(xob|edds)$/.test(p) ? null : data.read(p)) };

  // ---- spec rules -----------------------------------------------------------------------------------
  {
    const good = newClothingSpec({ slug: "police-shirt", name: "PoliceShirt", base: BASE, slot: "Jacket" });
    ok(clothingSpecProblems(good).length === 0, `spec: a new route a spec passes (${clothingSpecProblems(good).join("; ")})`);
    const bad = (patch, re, what) => {
      const p = clothingSpecProblems({ ...good, ...patch });
      ok(p.some((m) => re.test(m)), `spec: ${what} (${p.join("; ") || "no problem found"})`);
    };
    bad({ route: "d" }, /route must be/, "an unknown route is refused");
    bad({ base: "Prefabs/Characters/Uniforms/X.et" }, /base must be/, "a base without its GUID is refused");
    bad({ slot: "Shirt" }, /slot must be/, "a slot Character_Base has not is refused");
    bad({ hide: ["Torso", "Tail"] }, /Tail is not a body part/, "a body part the base body has not is refused");
    bad({ materials: [{ use: "worn", maps: { NMO_1: { tint: { rgb: [0, 0, 0] } } } }] }, /colour maps/, "route a may not repaint a normal map");
    bad({ materials: [{ use: "worn", maps: { BCR_1: { tint: { rgb: [2, 0, 0] } } } }] }, /tint.rgb/, "a tint outside 0..1 is refused");
    bad({ route: "c", mesh: { source: "x.blend", object: "X" } }, /item_object/, "a route c garment needs a ground model");
    bad({ route: "c", mesh: { source: "x.blend", object: "X", item_object: "Y" }, budget: { texture_px: 2048, lod0_tris: 9000, lod_ratios: [0.5, 0.6] } }, /lod_ratios/, "rising LOD ratios are refused");
    bad({ prefab: { name_key: "#AR-Foo" } }, /#LF- string key/, "a name key outside the LF table is refused");
    ok(COLOUR_MAPS.includes("BCR_1") && COLOUR_MAPS.includes("DirtBCRMap"), "spec: the blood layer's colour map can be repainted with the cloth");
    ok(SLOTS.Jacket.area === "LoadoutJacketArea" && SLOTS.Pants.hides.includes("Groin"), "spec: the slot table carries Character_Base's areas and hidden parts");
  }

  // ---- the committed fixtures --------------------------------------------------------------------------
  {
    const dir = join(ROOT, "tools", "assets");
    const specs = readdirSync(dir).filter((d) => existsSync(join(dir, d, "spec.json"))).map((d) => JSON.parse(readFileSync(join(dir, d, "spec.json"), "utf8"))).filter((s) => s.kind === "clothing");
    ok(specs.length >= 3, `fixtures: the three clothing fixtures are committed (found ${specs.length})`);
    for (const s of specs) ok(clothingSpecProblems(s).length === 0, `fixtures: ${s.slug} passes the spec stage (${clothingSpecProblems(s).join("; ")})`);
    ok(specs.some((s) => s.verify?.expect === "same" && !(s.materials || []).some((m) => Object.keys(m.maps || {}).length)), "fixtures: one parity run repaints nothing and expects vanilla's pictures");
    const { stages } = await stagesFor({ kind: "clothing" });
    ok(stages.join() === CLOTHING_STAGES.join() && (await stagesFor({ kind: "gadget" })).stages.includes("generate"), "dispatch: a clothing spec runs the clothing stages, a prop its own");
  }

  // ---- Enfusion text and the chain ---------------------------------------------------------------------
  {
    const root = parseText(FILES["Prefabs/Characters/Uniforms/Jacket_Suit_01/Jacket_Suit_01_base.et"]);
    const actions = root.children[0].children.find((c) => c.head[0] === "ActionsManagerComponent");
    const pick = actions?.children[0]?.children[0];
    ok(pick && pick.head[0] === "SCR_PickUpItemAction" && pick.head.includes("{F39A4E9B190C1846}Prefabs/Characters/Core/Actions/PickUpItemAction.conf"), "text: a header split after its colon is joined");
    const chain = prefabChain(stub, BASE);
    ok(chain.length === 2, `text: the chain is the variant and its base (got ${chain.length})`);
    const comps = mergedComponents(chain);
    const cloth = comps.find((c) => c.cls === "BaseLoadoutClothComponent");
    ok(cloth.layers.length === 2, "chain: a component declared in both files merges by GUID");
    ok(lookup(cloth, ["WornMaterialsOverride"], "[]")?.[0] === "{99EBF808CF181832}Assets/X/Grey.emat" && lookup(cloth, [], "WornModel") === "{7449DAACFEBE36CC}Assets/X/Jacket.xob", "chain: the child's list and the base's property both resolve");
    const rec = describeGarment(stub, BASE);
    ok(rec.cloth.area === "LoadoutJacketArea" && rec.cloth.hides.length === 5 && rec.cloth.guid === "5104869D194C627A", "describe: slot, hidden parts and the cloth component GUID");
    ok(rec.item?.displayGuid === "5222CB0109A04AB2" && rec.item.previewWorn && rec.storage?.maxVolume === "600", "describe: the inventory item, its preview and the pockets");
    ok(rec.materials.worn[0].cls === "MatPBRMulti" && rec.materials.worn[0].textures.BCR_1 && rec.materials.worn[0].textures.BCR_1.error, "describe: the worn material comes from the override, its maps from the emat (an unreadable texture is recorded, not thrown)");
    ok(rec.worn?.missing === true, "describe: a model the paks lack is recorded as missing");
  }

  // ---- EDDS ----------------------------------------------------------------------------------------------
  {
    ok(lz4Block(lz4Sample()).toString("latin1") === "abcabcabcabcX", "lz4: an overlapping match repeats the bytes before it");
    const small = Buffer.from([1, 2, 3, 4]);
    const big = Buffer.from([5, 6, 7, 8, 9, 10, 11, 12]);
    const file = edds([{ kind: "COPY", data: small }, { kind: "LZ4 ", data: (() => { const b = lz4Sample(); return b; })() }, { kind: "COPY", data: big }]);
    const info = eddsInfo(file);
    ok(info.width === 4 && info.mips === 3 && info.format === "BC7_SRGB" && info.headerBytes === 148, `edds: the header reads (${JSON.stringify(info)})`);
    const dds = eddsToDds(file);
    ok(dds.toString("latin1", 32, 36) === "\0\0\0\0", "edds: Enfusion's ENF1 tag is cleared");
    ok(dds.subarray(148).equals(Buffer.concat([big, Buffer.from("abcabcabcabcX"), small])), "edds: the mips come out largest first, the LZ4 one unpacked");
    let threw = "";
    try { eddsToDds(edds([{ kind: "ZSTD", data: small }])); } catch (e) { threw = e.message; }
    ok(/not COPY or LZ4/.test(threw), "edds: an unknown block kind is refused");
  }

  // ---- route a: maps, materials, prefab --------------------------------------------------------------------
  {
    const rec = describeGarment(stub, BASE);
    rec.materials.worn[0].textures.BCR_1 = { ref: "{7D0C1E898455C301}Assets/X/Grey_BCR.edds", width: 2048, height: 2048, format: "BC7_SRGB" };
    const spec = { ...newClothingSpec({ slug: "t", name: "Tint", base: BASE, slot: "Jacket" }), materials: [{ use: "worn", maps: { BCR_1: { tint: { rgb: [0.1, 0.1, 0.2] } } } }] };
    const jobs = textureJobs(spec, rec, "D", "S");
    ok(jobs.length === 1 && /Tint_Worn_BCR\.tif$/.test(jobs[0].tif) && jobs[0].vanilla.endsWith("Grey_BCR.edds"), `textures: one job per repainted map, named after the use (${jobs.map((j) => j.tif).join()})`);
    let threw = "";
    try { textureJobs({ ...spec, materials: [{ use: "item", maps: { DirtBCRMap: {} } }] }, rec, "D", "S"); } catch (e) { threw = e.message; }
    ok(/has no DirtBCRMap/.test(threw), "textures: a map the vanilla material does not have is refused");
    ok(ematOver("MatPBRMulti", "{99EBF808CF181832}Assets/X/Grey.emat", { BCR_1: "{1}a.edds" }) === 'MatPBRMulti : "{99EBF808CF181832}Assets/X/Grey.emat" {\n BCR_1 "{1}a.edds"\n}\n', "emat: our material inherits the vanilla one and names only our maps");
    const text = clothingPrefab(spec, rec, { emats: { worn: { 0: "{AAAAAAAAAAAAAAAA}Assets/O/Tint_Worn.emat" }, item: {} } });
    ok(text.startsWith(`GameEntity : "${BASE}" {\n ID "5104869D194C6243"`), "prefab: inherits the vanilla garment with its entity ID");
    ok(text.includes('BaseLoadoutClothComponent "{5104869D194C627A}"'), "prefab: the cloth component overrides vanilla's by its GUID");
    ok(text.includes('"{AAAAAAAAAAAAAAAA}Assets/O/Tint_Worn.emat"') && !text.includes("ItemMaterialsOverride"), "prefab: only the repainted use gets an override list");
    const named = clothingPrefab({ ...spec, prefab: { name_key: "#LF-Police_Shirt_Name" }, hide: ["Torso"] }, rec, { emats: { worn: {}, item: {} } });
    ok(named.includes('SCR_UniversalInventoryStorageComponent "{5222CB01D5B9A306}"') && named.includes('ItemDisplayName UIInfo "{5222CB0109A04AB2}"') && named.includes('Name "#LF-Police_Shirt_Name"'), "prefab: a name key overrides the vanilla display name in place");
    ok(/OverrideMeshesToHide \{\n\s+"Torso"\n/.test(named), "prefab: the spec's hidden parts replace vanilla's");
    ok((text.match(/\{/g) || []).length === (text.match(/\}/g) || []).length && (named.match(/\{/g) || []).length === (named.match(/\}/g) || []).length, "prefab: the braces balance");
    const own = clothingPrefab({ ...spec, route: "c" }, rec, { emats: { worn: {}, item: {} }, models: { worn: "{B}Assets/O/W.xob", item: "{C}Assets/O/I.xob" } });
    ok(own.includes('MeshObject "{5104869D194C6262}"') && own.includes('WornModel "{B}Assets/O/W.xob"') && own.includes('ItemModel "{C}Assets/O/I.xob"') && !own.includes("MaterialsOverride"), "prefab: routes b and c replace the worn and the ground model and bake their own material");
  }

  // ---- checks against vanilla ----------------------------------------------------------------------------
  {
    const rec = describeGarment(stub, BASE);
    const spec = newClothingSpec({ slug: "t", name: "T", base: BASE, slot: "Jacket" });
    // Models as the paks would give them: one material slot each (the overrides name one emat each).
    const clean = (r) => ({ ...r, worn: { materials: [{}], colliders: ["UTM_x"] }, itemModel: { colliders: ["UBX_x"], materials: [{}] }, materials: { worn: [], item: [] } });
    ok(garmentProblems(spec, clean(rec), clean(rec)).length === 0, `checks: vanilla against itself has no problem (${garmentProblems(spec, clean(rec), clean(rec)).join("; ")})`);
    const lost = { ...clean(rec), components: rec.components.filter((c) => c.cls !== "ActionsManagerComponent") };
    ok(garmentProblems(spec, clean(rec), lost).some((m) => /ActionsManagerComponent/.test(m)), "checks: a vanilla component that went missing is named");
    const twice = { ...clean(rec), components: [...rec.components, { cls: "BaseLoadoutClothComponent", guid: "0000000000000001" }] };
    ok(garmentProblems(spec, clean(rec), twice).some((m) => /shadows the inherited one/.test(m)), "checks: a second component of a class is flagged");
    const wrongSlot = { ...clean(rec), cloth: { ...rec.cloth, area: "LoadoutPantsArea" } };
    ok(garmentProblems(spec, clean(rec), wrongSlot).some((m) => /LoadoutPantsArea/.test(m)), "checks: the wrong slot is flagged");
    const bare = { ...clean(rec), cloth: { ...rec.cloth, hides: [] } };
    ok(garmentProblems(spec, clean(rec), bare).some((m) => /pokes through/.test(m)), "checks: a jacket that hides no body part is flagged");
    const shortList = { ...clean(rec), worn: { materials: [{}, {}] }, cloth: { ...rec.cloth, wornMaterialsOverride: ["{A}x.emat"] } };
    ok(garmentProblems(spec, clean(rec), shortList).some((m) => /maps them by order/.test(m)), "checks: an override list shorter than the model's material slots is flagged");
    const noPockets = { ...clean(rec), storage: null };
    ok(garmentProblems(spec, clean(rec), noPockets).some((m) => /pockets/.test(m)), "checks: lost pockets are flagged");
    const skeleton = ["Scene_Root", "Root", "Hips", "Spine1"];
    ok(modelProblems({ worn: { nodeNames: skeleton } }, { worn: { nodes: ["Scene_Root"], colliders: ["UTM_x"], materials: [{}] } }).some((m) => /lacks 3 node/.test(m)), "models: a worn model missing skeleton nodes is flagged");
    ok(modelProblems({ worn: { nodeNames: skeleton } }, { worn: { nodes: skeleton, colliders: [], materials: [] } }).length === 2, "models: no FireGeo collider and no material are both flagged");
    ok(modelProblems({}, { item: { nodes: ["Root"], colliders: [] } }).length === 2, "models: a ground model with bones and no collider is flagged");
    const meta = 'MetaFileClass {\n Configurations {\n  FBXResourceClass PC {\n   GeometryParams {\n   }\n  }\n }\n}\n';
    const once = withMeshParams(meta, "Vest");
    ok(/MeshParams \{\n\s+MeshParam Vest \{/.test(once) && withMeshParams(once, "Vest") === once, "meta: one MeshParams entry, written once");
  }

  // ---- state pictures ---------------------------------------------------------------------------------------
  {
    const garments = [{ tag: "vanilla", prefab: BASE }, { tag: "ours", prefab: "{0123456789ABCDEF}Prefabs/X/Ours.et" }];
    const plan = planText({ garments });
    const lines = plan.split("\n");
    ok(lines.filter((l) => l.startsWith("spawn ")).length === POSES.length * garments.length, "shots: a fresh mannequin for every garment in every pose");
    ok(lines.filter((l) => /^(shot|groundshot) /.test(l)).length === stateNames().length * garments.length, "shots: every state is shot once per garment");
    ok(stateNames().length === POSES.reduce((n, p) => n + p.views.length, 0) + GROUND_VIEWS.length && ["prone", "run", "crouch", "idle"].every((p) => POSES.some((x) => x.name === p)), "shots: the plan covers idle, crouch, prone, run and the ground");
    let threw = "";
    try { planText({ garments: [{ tag: "a b", prefab: BASE }] }); } catch (e) { threw = e.message; }
    ok(/bad garment/.test(threw), "shots: a tag with a space is refused (it would split the plan line)");
    const a = image(64, 36, () => [90, 120, 60]);
    const b = image(64, 36, (x, y) => (x > 24 && x < 40 && y > 8 && y < 30 ? [30, 40, 110] : [90, 120, 60]));
    ok(pairDiff(a, a).mean === 0, "diff: a picture against itself is 0");
    ok(pairDiff(a, b).mean > SAME_LIMIT.still, `diff: a recoloured figure is over the still limit (${pairDiff(a, b).mean})`);
    ok(isMotion("run_side") && isMotion("prone_front") && !isMotion("idle_front") && !isMotion("ground_top"), "diff: prone and run get the motion limit");
    const plugin = readFileSync(join(ROOT, "tools", "assets", "pipeline", "wb", "LF_ClothingShotsPlugin.c"), "utf8");
    ok(!/protected (void|bool|int) (Run|Wait)\(/.test(plugin), "plugin: no method named like a WorldEditorPlugin one (Run would not compile; Wait returned at once and no pose was reached)");
  }

  // ---- previews of any prefab -------------------------------------------------------------------------------
  {
    const list = prefabList(`Vanilla=${BASE},Ours={0123456789ABCDEF}Prefabs/X/Ours.et`);
    ok(list.length === 2 && list[0].name === "Vanilla" && list[1].prefab.endsWith("Ours.et"), "previews: --prefabs takes Label={GUID}path pairs");
    let threw = "";
    try { prefabList("Vanilla=Prefabs/X.et"); } catch (e) { threw = e.message; }
    ok(/Label=\{GUID\}path/.test(threw), "previews: a prefab without its GUID is refused");
  }

  if (failed) {
    console.log(`asset-clothing-selftest: FAILED (${failed})`);
    return 1;
  }
  console.log("asset-clothing-selftest: OK");
  return 0;
}

if (process.argv[1] && import.meta.url === pathToFileURL(process.argv[1]).href) process.exit(await main());
