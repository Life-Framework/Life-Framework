// Stage "prefab" for the deployable kind: two prefabs, each written once and never overwritten.
//   <Name>.et       the deployed prop: the model on a static body, replicated, with the spec's
//                   feature components and its world actions (pick it up again, for one)
//   <Name>_Item.et  what is carried: an inventory item on RoleplayItem_Base with the folded model
//                   (the clean stage's <Name>_Folded variant, else the model itself) and the action
//                   that deploys it
// What the deploy and pick-up do is feature code; the scaffold only wires the classes the spec
// names (docs/asset-pipeline/README.md "Kinds").
import { existsSync, mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { freshGuid, readGuid, usedGuids } from "./meta.mjs";
import { resourceMeta } from "./prefab.mjs";

// RoleplayItem_Base: Item_Base with the mod's quantity and sounds (read 2026-11-13).
const ITEM_BASE = "{8B993DEBBF41116C}Prefabs/Items/Core/RoleplayItem_Base.et";
// Item_Base's ActionsManagerComponent and its "default" context: overridden by GUID, never doubled.
const ITEM_ACTIONS = "508AB5952B584B2E";
const ITEM_CONTEXT = "508AC2ABEC97066C";
// Item_Base's display name object, also overridden by its id.
const ITEM_NAME = "5222EB4D07D865FA";

const fmt = (n) => String(+Number(n).toFixed(4));

// "$deployed" and "$item" in a string value become the two prefabs' {GUID}paths, so the item can
// name what it deploys into and the prop what it folds back into.
export function props(p = {}, indent, refs = {}) {
  const value = (v) => {
    if (typeof v !== "string") return v;
    return JSON.stringify(v.startsWith("$") && refs[v.slice(1)] ? refs[v.slice(1)] : v);
  };
  return Object.entries(p).map(([k, v]) => `${indent}${k} ${value(v)}\n`).join("");
}

function actionBlock(a, guid, indent, refs) {
  return `${indent}${a.class} "{${guid}}" {\n${indent} ParentContextList {\n${indent}  "default"\n${indent} }\n${props(a.props, indent + " ", refs)}${indent}}\n`;
}

function componentBlock(c, guid, refs) {
  return `  ${c.class} "{${guid}}" {\n${props(c.props, "   ", refs)}  }\n`;
}

// The deployed prop. bounds are the clean report's (Enfusion model space); the one action context
// sits on the middle of the top face and reaches both ends of a long strip.
export function deployedPrefab({ id, xob, d, bounds, guid, refs = {} }) {
  const { min, max } = bounds;
  const half = Math.max(max[0] - min[0], max[2] - min[2]) / 2;
  const comps = (d.components || []).map((c) => componentBlock(c, guid(), refs)).join("");
  const actions = (d.actions || []).map((a) => actionBlock(a, guid(), "    ", refs)).join("");
  return `GenericEntity {
 ID "${id}"
 components {
  MeshObject "{${guid()}}" {
   Object "${xob}"
  }
  RigidBody "{${guid()}}" {
   ModelGeometry 1
   Static 1
  }
${comps}  ActionsManagerComponent "{${guid()}}" {
   ActionContexts {
    UserActionContext "{${guid()}}" {
     ContextName "default"
     Position PointInfo "{${guid()}}" {
      Offset 0 ${fmt(max[1])} 0
     }
     Radius ${fmt(half + 0.4)}
    }
   }
   additionalActions {
${actions}   }
  }
  RplComponent "{${guid()}}" {
  }
 }
}
`;
}

// The carried item. Size and weight come from the spec, so it fits a uniform as the feature needs.
export function itemPrefab({ id, xob, d, bounds, guid, refs = {} }) {
  const { min, max } = bounds;
  const comps = (d.item_components || []).map((c) => componentBlock(c, guid(), refs)).join("");
  const actions = (d.item_actions || []).map((a) => actionBlock(a, guid(), "    ", refs)).join("");
  return `GenericEntity : "${ITEM_BASE}" {
 ID "${id}"
 components {
  LF_QuantityComponent "{5AD2B81F97B9381B}" {
   m_iMaxQuantity 1
  }
  InventoryItemComponent "{5222EB4D0C73006B}" {
   Attributes SCR_ItemAttributeCollection "{5222EB4D0A2B466B}" {
    CustomAttributes {
     PreviewRenderAttributes "{${guid()}}" {
      CameraPreset Front
      CameraOrbitAngles -35 60 0
      CameraDistanceToItem ${fmt(Math.max(max[0] - min[0], max[1] - min[1], max[2] - min[2]) * 3)}
      FOV 20
     }
    }
    ItemDisplayName UIInfo "{${ITEM_NAME}}" {
     Name "${d.name_key}"
     Description "${d.description_key}"
    }
    ItemPhysAttributes ItemPhysicalAttributes PhysicalAttributes {
     Weight ${fmt(d.weight ?? 1)}
     SizeSetupStrategy Volume
     ItemVolume ${fmt(d.item_volume ?? 1000)}
     RestingAdditiveRotationLS 0 0 0
     RestingAdditiveOffsetLS 0 0 0
    }
   }
  }
  MeshObject "{5A4ACD10B8DA304F}" {
   Object "${xob}"
  }
${comps}  ActionsManagerComponent "{${ITEM_ACTIONS}}" {
   ActionContexts {
    UserActionContext "{${ITEM_CONTEXT}}" {
     ContextName "default"
     Position PointInfo "{508AC2ABEAC4A0F5}" {
      Offset 0 ${fmt((min[1] + max[1]) / 2)} 0
     }
     Radius ${fmt(Math.max(max[0] - min[0], max[2] - min[2]) / 2 + 0.1)}
    }
   }
   additionalActions {
${actions}   }
  }
  SCR_GadgetComponent "{${guid()}}" {
   m_eAnimVariable LF_ITEM
  }
 }
}
`;
}

// Everything wrong with spec.deployable, as a list.
export function deployableProblems(spec) {
  const d = spec.deployable;
  if (!d) return ["spec.deployable is missing: name_key, description_key, weight, item_volume, components, actions, item_actions"];
  const out = [];
  for (const k of ["name_key", "description_key"]) if (!/^#\S+$/.test(d[k] || "")) out.push(`deployable.${k} must be a string table key with the #`);
  for (const [k, list] of [["components", d.components], ["actions", d.actions], ["item_components", d.item_components], ["item_actions", d.item_actions]]) {
    for (const c of list || []) if (!/^[A-Z][A-Za-z0-9_]+$/.test(c.class || "")) out.push(`deployable.${k}: "${c.class}" is not a class name`);
  }
  if (!(d.item_actions || []).length) out.push("deployable.item_actions is empty: nothing would deploy the carried item");
  return out;
}

export async function writeDeployablePrefabs(ctx) {
  const { root, spec, dir } = ctx;
  const problems = deployableProblems(spec);
  if (problems.length) throw new Error("spec problems:\n  " + problems.join("\n  "));
  const d = spec.deployable;
  const addon = join(root, "addons", "LifeFramework");
  const report = JSON.parse(readFileSync(join(dir, "clean", "report.json"), "utf8"));
  const model = (name) => {
    const rel = `${spec.asset_dir}/${name}.xob`;
    const g = readGuid(join(addon, rel + ".meta"));
    if (!g || !existsSync(join(addon, rel))) throw new Error(`no imported model at ${rel}; run the import stage first`);
    return `{${g}}${rel}`;
  };
  const folded = (report.variants || []).find((v) => v.name === `${spec.name}_Folded`);
  const xob = { deployed: model(spec.name), item: model(folded ? folded.name : spec.name) };
  const used = usedGuids(addon);
  const guid = () => freshGuid(used);
  const files = { deployed: `${spec.prefab_dir}/${spec.name}.et`, item: `${spec.prefab_dir}/${spec.name}_Item.et` };
  const ref = {};
  for (const [k, rel] of Object.entries(files)) ref[k] = `{${readGuid(join(addon, rel + ".meta")) || guid()}}${rel}`;
  const text = {
    deployed: deployedPrefab({ id: guid(), xob: xob.deployed, d, bounds: report.bounds, guid, refs: ref }),
    item: itemPrefab({ id: guid(), xob: xob.item, d, bounds: folded ? folded.bounds : report.bounds, guid, refs: ref }),
  };
  const written = [];
  for (const [k, rel] of Object.entries(files)) {
    const abs = join(addon, rel);
    mkdirSync(dirname(abs), { recursive: true });
    if (!existsSync(abs)) { writeFileSync(abs, text[k]); written.push(rel); }
    if (!existsSync(abs + ".meta")) writeFileSync(abs + ".meta", resourceMeta(rel, ref[k].slice(1, 17), "EntityTemplateResourceClass"));
    // A scaffold that exists is not rewritten, so check the one thing a re-import can break.
    if (!readFileSync(abs, "utf8").includes(xob[k])) throw new Error(`${rel} does not reference ${xob[k]}`);
  }
  console.log(`prefab: ${written.length ? "wrote " + written.join(", ") : "both files exist, left as they are"}`);
  console.log(`  deployed ${ref.deployed}`);
  console.log(`  item     ${ref.item}`);
  console.log(`  add the string table keys ${d.name_key}, ${d.description_key} and run tools\\cli regen-localization`);
  return ref;
}
