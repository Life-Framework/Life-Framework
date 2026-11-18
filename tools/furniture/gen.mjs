// tools/furniture/gen.mjs - writes the house furniture items of tools/furniture/items.json (vanilla props only).
//   node tools/furniture/gen.mjs [--check]     writes (or with --check, only compares) every file below
//
// per item: addons/LifeFramework/Prefabs/Placeables/Furniture/Furniture_<id>_{Item,Placed}.et + .meta,
//           the Furniture_<id> row of Configs/Placement/LF_PlacementPolicy.conf,
//           the LF-Furniture_<id>_Name row of Language/everonlife_localization.st (then `tools\cli regen-localization`),
//           the price rule, extra-prefab line and stock row in addons/LF_Everon1989/Configs/Shop/.
// Every GUID is the first 16 hex digits of sha1("lf-furniture:<id>:<slot>"): the same each run, never reused,
// so a rerun rewrites the same bytes. The footprint comes from the vanilla model's measured bounds
// (tools/lib/xob.mjs through `measure`), not from typed numbers.
import { createHash } from "node:crypto";
import { existsSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { measureOne } from "../commands/measure.mjs";
import { gameVfs, meshOf, ownComponents, readText } from "../lib/game-prefab.mjs";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "../..");
const FURN = join(ROOT, "addons", "LifeFramework", "Prefabs", "Placeables", "Furniture");
const POLICY = join(ROOT, "addons", "LifeFramework", "Configs", "Placement", "LF_PlacementPolicy.conf");
const STRINGS = join(ROOT, "addons", "LifeFramework", "Language", "everonlife_localization.st");
const PRICES = join(ROOT, "addons", "LF_Everon1989", "Configs", "Shop", "LF_ShopPrices_GeneralStore.conf");
const STOCK = join(ROOT, "addons", "LF_Everon1989", "Configs", "Shop", "LF_ShopStock_Everon1989.conf");

const guid = (id, slot) => createHash("sha1").update(`lf-furniture:${id}:${slot}`).digest("hex").slice(0, 16).toUpperCase();
const r3 = (n) => String(Math.round(n * 1000) / 1000);
const vec = (a) => a.map(r3).join(" ");
const check = process.argv.includes("--check");
let differs = 0;

function put(file, text) {
  const old = existsSync(file) ? readFileSync(file, "utf8") : null;
  if (old === text) return;
  differs++;
  if (!check) writeFileSync(file, text);
  else console.log(`differs: ${file}`);
}

const meta = (ref) => `MetaFileClass {
 Name "${ref}"
 Configurations {
  EntityTemplateResourceClass PC {
  }
  EntityTemplateResourceClass XBOX_ONE : PC {
  }
  EntityTemplateResourceClass XBOX_SERIES : PC {
  }
  EntityTemplateResourceClass PS4 : PC {
  }
  EntityTemplateResourceClass HEADLESS : PC {
  }
 }
}
`;

// The footprint in the placed prefab's frame from the model's bounds: a floor item's box starts 0.1 m up
// so the floor under it does not count; a wall item's back edge is the model's own back (z min), flush.
function footprint(item, b) {
  const pad = 0.03;
  const [x0, y0, z0] = b.min;
  const [x1, y1, z1] = b.max;
  if (item.snap === "WALL") {
    const bottom = item.hang ? Math.min(y0, 0) : 0.1;
    return { mins: [x0 - 0.02, bottom, z0], maxs: [x1 + 0.02, Math.max(y1, bottom + 0.1), z1 + pad] };
  }
  const bottom = y1 < 0.3 ? 0.01 : 0.1;
  return { mins: [x0 - pad, bottom, z0 - pad], maxs: [x1 + pad, Math.max(y1, bottom + 0.1), z1 + pad] };
}

// The material overrides the vanilla prefab chain puts on its model (a poster's print, a sofa's colour), most
// derived first: the carried item shows the bare .xob, so without them it would be the default or a white placeholder.
function materialsOf(vfs, chain) {
  const found = new Map();
  for (const p of chain) {
    const text = readText(ROOT, vfs, p);
    const own = ownComponents(text);
    const at = own.indexOf("MeshObject");
    if (at < 0) continue;
    const re = /MaterialAssignClass\s+"\{[0-9A-Fa-f]{16}\}"\s*\{\s*SourceMaterial\s+"([^"]+)"\s*AssignedMaterial\s+"([^"]+)"/g;
    re.lastIndex = at;
    let hit;
    while ((hit = re.exec(own))) if (!found.has(hit[1])) found.set(hit[1], hit[2]);
  }
  return [...found].map(([source, assigned]) => ({ source, assigned }));
}

function itemText(item, m, fp) {
  const id = item.id;
  const span = Math.max(m.max[0] - m.min[0], m.max[1] - m.min[1], m.max[2] - m.min[2]);
  const distance = r3(Math.max(2, span * 1.6));
  const assigns = m.materials.map((x, i) => `    MaterialAssignClass "{${guid(id, "material-" + i)}}" {\n     SourceMaterial "${x.source}"\n     AssignedMaterial "${x.assigned}"\n    }\n`).join("");
  const materials = m.materials.length ? `\n   Materials {\n${assigns}   }` : "";
  const hang = item.hang ? `\n   m_fHangHeight ${r3(item.hang)}` : "";
  return `GenericEntity : "{8B993DEBBF41116C}Prefabs/Items/Core/RoleplayItem_Base.et" {
 ID "${guid(id, "item-id")}"
 components {
  LF_QuantityComponent "{5AD2B81F97B9381B}" {
   m_iMaxQuantity 1
  }
  InventoryItemComponent "{5222EB4D0C73006B}" {
   Attributes SCR_ItemAttributeCollection "{5222EB4D0A2B466B}" {
    CustomAttributes {
     PreviewRenderAttributes "{76C1B2BA574582F0}" {
      CameraPreset Front
      CameraOrbitAngles -35 60 0
      CameraDistanceToItem ${distance}
      FOV 20
     }
    }
    ItemDisplayName UIInfo "{5222EB4D07D865FA}" {
     Name "#LF-Furniture_${id}_Name"
    }
    ItemPhysAttributes ItemPhysicalAttributes PhysicalAttributes {
     Weight ${item.weight}
     SizeSetupStrategy Volume
     ItemVolume 2500
     RestingAdditiveRotationLS 0 0 0
     RestingAdditiveOffsetLS 0 0 0
    }
   }
  }
  MeshObject "{5A4ACD10B8DA304F}" {
   Object "${m.meshRef}"${materials}
  }
  LF_PlaceableComponent "{${guid(id, "placeable")}}" {
   m_sPlacedPrefab "{${guid(id, "placed-meta")}}Prefabs/Placeables/Furniture/Furniture_${id}_Placed.et"
   m_vFootprintMins ${vec(fp.mins)}
   m_vFootprintMaxs ${vec(fp.maxs)}
   m_fReach 1
   m_fAimReach 4
   m_eSnap ${item.snap}${hang}
   m_eWho HOUSE_OWNER_OR_KEY
   m_sKind "Furniture_${id}"
   m_eLifetime PERMANENT
  }
  ActionsManagerComponent "{508AB5952B584B2E}" {
   ActionContexts {
    UserActionContext "{508AC2ABEC97066C}" {
     ContextName "default"
     Position PointInfo "{508AC2ABEAC4A0F5}" {
      Offset 0 0.2119 0
     }
     Radius 1.07
    }
   }
   additionalActions {
    LF_PlacementAimAction "{${guid(id, "aim-action")}}" {
     ParentContextList {
      "default"
     }
    }
   }
  }
  SCR_GadgetComponent "{2098B1EEF3469A6F}" {
   m_eAnimVariable LF_ITEM
  }
 }
}
`;
}

// An existing ActionsManagerComponent in the vanilla chain (the radio's turn-on action) is overridden by its
// own GUID so the placed prop keeps it and gains one more context; otherwise a new one is added.
function placedText(item, m, existingActions, vanillaRef, rplGuid) {
  const id = item.id;
  const center = [0, 1, 2].map((k) => (m.min[k] + m.max[k]) / 2);
  const radius = r3(Math.max(1.4, Math.hypot(m.max[0] - m.min[0], m.max[2] - m.min[2]) / 2 + 0.9));
  const am = existingActions || guid(id, "placed-actions");
  const s = item.storage;
  const openAction = s ? `    LF_FurnitureStorageOpenAction "{${guid(id, "open-action")}}" {
     ParentContextList {
      "default"
     }
     UIInfo SCR_ActionUIInfo "{${guid(id, "open-ui")}}" {
      Name "#LF-FurnitureStorage_Open"
     }
    }
` : "";
  const storage = s ? `  LF_FurnitureStorageManagerComponent "{${guid(id, "storage-manager")}}" {
  }
  SCR_UniversalInventoryStorageComponent "{${guid(id, "storage")}}" {
   Attributes SCR_ItemAttributeCollection "{${guid(id, "storage-attrs")}}" {
    ItemDisplayName SCR_InventoryUIInfo "{${guid(id, "storage-ui")}}" {
     Name "#LF-Furniture_${id}_Name"
    }
   }
   UseCapacityCoefficient 0
   MaxCumulativeVolume ${s.volume}
   MaxItemSize ${s.size.join(" ")}
   m_fMaxWeight ${s.weight}
  }
  LF_PersistentStorageComponent "{${guid(id, "persistent")}}" {
   m_bKeyAssignedLater 1
  }
` : "";
  return `GenericEntity : "${vanillaRef}" {
 ID "${guid(id, "placed-id")}"
 components {
  ActionsManagerComponent "{${am}}" {
   ActionContexts {
    UserActionContext "{${guid(id, "placed-context")}}" {
     ContextName "default"
     Position PointInfo "{${guid(id, "placed-point")}}" {
      Offset ${vec(center)}
     }
     Radius ${radius}
    }
   }
   additionalActions {
    LF_PlacementPickupAction "{${guid(id, "pickup-action")}}" {
     ParentContextList {
      "default"
     }
    }
${openAction}   }
  }
${storage}  LF_PlacedOwnerComponent "{${guid(id, "owner")}}" {
  }
  RplComponent "{${rplGuid}}" {
   Enabled 1
  }
 }
}`;
}

// Inserts `block` before the first occurrence of `anchor` (or after it with `after`) unless `marker` is already there.
function insert(text, marker, anchor, block, { after = false, last = false } = {}) {
  if (text.includes(marker)) return text;
  const at = last ? text.lastIndexOf(anchor) : text.indexOf(anchor);
  if (at < 0) throw new Error(`anchor not found: ${JSON.stringify(anchor)}`);
  const cut = after ? at + anchor.length : at;
  return text.slice(0, cut) + block + text.slice(cut);
}

const spec = JSON.parse(readFileSync(join(ROOT, "tools", "furniture", "items.json"), "utf8"));
const vfs = await gameVfs();
let policy = readFileSync(POLICY, "utf8");
let strings = readFileSync(STRINGS, "utf8");
let prices = readFileSync(PRICES, "utf8");
let stock = readFileSync(STOCK, "utf8");
const lamp = '    LF_ShopStockItem "{7556E7877FE5A902}" {\n     m_Prefab "{03AB5928B9B6C230}Prefabs/Placeables/Furniture/Furniture_Lamp_Item.et"\n     m_iPrice 35\n    }\n';

for (const item of spec.items) {
  const id = item.id;
  const m = await measureOne(ROOT, item.vanilla);
  m.min = m.bounds.min;
  m.max = m.bounds.max;
  const fp = footprint(item, m.bounds);
  const itemRef = `{${guid(id, "item-meta")}}Prefabs/Placeables/Furniture/Furniture_${id}_Item.et`;
  const placedRef = `{${guid(id, "placed-meta")}}Prefabs/Placeables/Furniture/Furniture_${id}_Placed.et`;
  let existingActions = null;
  let rplGuid = null;
  const chain = meshOf(ROOT, vfs, m.path).chain;
  m.materials = materialsOf(vfs, chain);
  for (const p of chain) {
    const t = readText(ROOT, vfs, p);
    const hit = /ActionsManagerComponent\s+"\{([0-9A-F]{16})\}"/.exec(ownComponents(t));
    if (hit && !existingActions) existingActions = hit[1];
    // A vanilla RplComponent has its own GUID in some chains (the crate's): override that one, never add a second.
    const rpl = /RplComponent\s+"\{([0-9A-F]{16})\}"/.exec(ownComponents(t));
    if (rpl && !rplGuid) rplGuid = rpl[1];
  }
  put(join(FURN, `Furniture_${id}_Item.et`), itemText(item, m, fp));
  put(join(FURN, `Furniture_${id}_Item.et.meta`), meta(itemRef));
  put(join(FURN, `Furniture_${id}_Placed.et`), placedText(item, m, existingActions, m.ref, rplGuid || "5624A88DC2D9928D"));
  put(join(FURN, `Furniture_${id}_Placed.et.meta`), meta(placedRef));

  policy = insert(policy, `"Furniture_${id}"`, "\n }\n}", `\n  LF_PlacementPolicyEntry "{${guid(id, "policy")}}" {\n   m_sKind "Furniture_${id}"\n   m_eWho HOUSE_OWNER_OR_KEY\n   m_eWhere OWNED_PROPERTY\n  }`, { last: true });
  strings = insert(strings, `"LF-Furniture_${id}_Name"`, "\n }\n}", `\n    CustomStringTableItem "{${guid(id, "string")}}" {\n     Id "LF-Furniture_${id}_Name"\n     Target_en_us "${item.label}"\n     Comment "Inventory name of the carried ${item.label.toLowerCase()} a player places in their own house."\n    }`, { last: true });
  prices = insert(prices, `Furniture_${id}_Item.et"\n   m_iBuyPrice`, "\n }\n m_aExtraItemPrefabs {", `\n  LF_ShopPriceRule "{${guid(id, "price")}}" {\n   m_Prefab "${itemRef}"\n   m_iBuyPrice ${item.buy}\n   m_iSellPrice ${item.sell}\n  }`);
  prices = insert(prices, `  "${itemRef}"`, "\n }\n}", `\n  "${itemRef}"`, { last: true });
  stock = insert(stock, `Furniture_${id}_Item.et"\n     m_iPrice`, lamp, `    LF_ShopStockItem "{${guid(id, "stock")}}" {\n     m_Prefab "${itemRef}"\n     m_iPrice ${item.buy}\n    }\n`, { after: true });
}

if (spec.items.some((i) => i.storage)) {
  const fixed = [
    ["LF-FurnitureStorage_Open", "Open", "storage-open", "Action on a placed storage furniture (wardrobe, dresser, chest, fridge): opens its inventory for the player who placed it."],
    ["LF-Placement_EmptyFirst", "Empty it first", "empty-first", "Toast under Placing when the player picks up a storage furniture that still holds items."],
  ];
  for (const [key, text, slot, comment] of fixed)
    strings = insert(strings, `"${key}"`, "\n }\n}", `\n    CustomStringTableItem "{${guid("fixed", slot)}}" {\n     Id "${key}"\n     Target_en_us "${text}"\n     Comment "${comment}"\n    }`, { last: true });
}

put(POLICY, policy);
put(STRINGS, strings);
put(PRICES, prices);
put(STOCK, stock);
console.log(check ? (differs ? `${differs} file(s) differ` : "furniture: up to date") : `furniture: ${spec.items.length} item(s) written (${differs} file(s) changed)`);
process.exit(check && differs ? 1 : 0);
