// Stage "prefab" for kind "gadget": one item a player carries and holds in the left hand. Written once
// and never overwritten, like the weapon scaffold (prefab.mjs).
//
// The base is the vanilla Item_Base.et: inventory item, pick-up action, sounds, replication. A held
// gadget is SCR_GadgetComponent plus an animation attachment bound as "Gadget". The vanilla held
// items this follows (Gauze_01_packed.xob, read with xob.mjs on 2026-11-13) have no bones and no
// sockets: the attachment puts the model's origin in the left hand, so the clean stage's centred
// origin is the grip point and HandAdditiveRotation/Translation turn and move the model in the hand.
// The repo's other held items (LF_Lockpick.et, LF_Scanner.et) use the same graph and m_eAnimVariable.
import { existsSync, mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { freshGuid, readGuid, usedGuids } from "./meta.mjs";
import { resourceMeta } from "./prefab.mjs";

// Vanilla resources (game 1.9; read with game_read 2026-11-13).
export const VANILLA = {
  itemBase: "{C95E11C60810F432}Prefabs/Items/Core/Item_Base.et",
  handGraph: "{883777E45A6C4DA1}Assets/Items/Medicine/Workspaces/Gauze_01.agr",
  handInstance: "{79BD98038DD60F88}Assets/Items/Medicine/Workspaces/Gauze_01_player.asi",
};
// Ids of Item_Base.et objects the scaffold overrides; a new id would add a second component or object
// (its ItemPhysAttributes is the named object PhysicalAttributes, as LF_Lockpick.et overrides it).
const BASE = { inventory: "5222EB4D0C73006B", attributes: "5222EB4D0A2B466B", display: "5222EB4D07D865FA" };
// The lockpick's hand rotation. The translation is zero: the clean stage centres a gadget on its grip, and the
// lockpick's -0.07 0.03 -0.01 floated the centred handcuffs 12 cm above the fingers (look 2026-11-13).
export const DEFAULT_HAND = { rotation: [-65, -60, -60], translation: [0, 0, 0] };

const fmt = (n) => String(+Number(n).toFixed(4));
const vec = (v) => v.map(fmt).join(" ");

function componentBlock(c, guid) {
  const props = Object.entries(c.props || {}).map(([k, v]) => `   ${k} ${typeof v === "string" ? JSON.stringify(v) : v}\n`).join("");
  return `  ${c.class} "{${guid}}" {\n${props}  }\n`;
}

// The inventory picture is rendered by the engine from PreviewRenderAttributes; the camera stands
// back about three lengths so the whole object fits at the vanilla gadgets' 20 degree FOV.
export function previewDistance(report) {
  const [length, height, width] = report.size;
  return Math.max(0.25, 3 * Math.max(length, height, width));
}

export function gadgetPrefab({ id, xob, g, report, guids }) {
  const hand = { ...DEFAULT_HAND, ...(g.hand || {}) };
  const extra = (g.components || []).map((c, i) => componentBlock(c, guids.components[i])).join("");
  return `GenericEntity : "${VANILLA.itemBase}" {
 ID "${id}"
 components {
  InventoryItemComponent "{${BASE.inventory}}" {
   Attributes SCR_ItemAttributeCollection "{${BASE.attributes}}" {
    ItemDisplayName UIInfo "{${BASE.display}}" {
     Name "${g.name_key}"
     Description "${g.description_key}"
    }
    ItemPhysAttributes ItemPhysicalAttributes PhysicalAttributes {
     Weight ${fmt(g.weight ?? 0.2)}
     SizeSetupStrategy Volume
     ItemVolume ${Math.round(g.volume ?? 100)}
     RestingUP Up
    }
    ItemAnimationAttributes ItemAnimationAttributes "{${guids.anim}}" {
     AnimationAttachments {
      AnimationAttachmentInfo "{${guids.attach}}" {
       AnimGraph "${VANILLA.handGraph}"
       AnimInstance "${VANILLA.handInstance}"
       BindingName "Gadget"
      }
     }
     HandAdditiveRotation ${vec(hand.rotation)}
     HandAdditiveTranslation ${vec(hand.translation)}
    }
    CustomAttributes {
     PreviewRenderAttributes "{${guids.preview}}" {
      CameraOrbitAngles -25 -25 0
      CameraDistanceToItem ${fmt(previewDistance(report))}
      FOV 20
     }
    }
    CommonItemType "LF_ITEM"
   }
  }
  MeshObject "{${guids.mesh}}" {
   Object "${xob}"
  }
  RigidBody "{${guids.body}}" {
   LayerPreset "ItemFireView"
   ModelGeometry 1
  }
  SCR_GadgetComponent "{${guids.gadget}}" {
   m_eAnimVariable LF_ITEM
  }
${extra} }
}
`;
}

export async function writePrefabs(ctx) {
  const { root, spec, dir } = ctx;
  const g = spec.gadget;
  if (!g?.name_key || !g?.description_key) throw new Error("spec.gadget needs name_key and description_key (string table keys, with the #)");
  const addon = join(root, "addons", "LifeFramework");
  const report = JSON.parse(readFileSync(join(dir, "clean", "report.json"), "utf8"));
  const xobRes = `${spec.asset_dir}/${spec.name}.xob`;
  const xobGuid = readGuid(join(addon, xobRes + ".meta"));
  if (!xobGuid || !existsSync(join(addon, xobRes))) throw new Error(`no imported model at ${xobRes}; run the import stage first`);

  const rel = `${spec.prefab_dir}/${spec.name}.et`;
  const abs = join(addon, rel);
  const used = usedGuids(addon);
  const guid = readGuid(abs + ".meta") || freshGuid(used);
  const written = [];
  if (!existsSync(abs)) {
    const guids = { components: (g.components || []).map(() => freshGuid(used)) };
    for (const k of ["anim", "attach", "preview", "mesh", "body", "gadget"]) guids[k] = freshGuid(used);
    mkdirSync(dirname(abs), { recursive: true });
    writeFileSync(abs, gadgetPrefab({ id: freshGuid(used), xob: `{${xobGuid}}${xobRes}`, g, report, guids }));
    written.push(rel);
  }
  if (!existsSync(abs + ".meta")) writeFileSync(abs + ".meta", resourceMeta(rel, guid, "EntityTemplateResourceClass"));
  // A scaffold that exists is not rewritten, so check the one thing a re-import can break.
  if (!readFileSync(abs, "utf8").includes(`{${xobGuid}}${xobRes}`)) throw new Error(`${rel} does not reference {${xobGuid}}${xobRes}`);
  const ref = `{${guid}}${rel}`;
  console.log(`prefab: ${written.length ? "wrote " + rel : rel + " exists, left as it is"}`);
  console.log(`  gadget ${ref}`);
  console.log(`  add the string table keys ${g.name_key}, ${g.description_key} and run tools\\cli regen-localization`);
  return { gadget: ref };
}
