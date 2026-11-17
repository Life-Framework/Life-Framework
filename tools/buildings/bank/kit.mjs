// Bank_Kit.et: the bank as one placeable prefab. It inherits Bank.et (the Office_E_01 override with its furniture)
// and adds the interactables as children: the vault room's door (LF_BankVault_RoomDoor, safe type bank_room, drill
// or charge) and the cash bag inside it, the bell box and the fascia sign on the front wall, and the lit lamp by the
// street door. Everything is existing systems; nothing here is a new component. Tellers and the rob kit stand in the
// pack grid layer (stage.mjs), and are kit rows of `town fit`.
//
//   node tools/buildings/bank/kit.mjs          write Prefabs/Buildings/Bank/Bank_Kit.et (and its .meta once)
//   node tools/buildings/bank/kit.mjs --check  exit 1 when the file is not what this script writes
import { createHash, randomBytes } from "node:crypto";
import { existsSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { kit, vault } from "./layout.mjs";

const ROOT = join(dirname(fileURLToPath(import.meta.url)), "..", "..", "..");
const DIR = join(ROOT, "addons", "LF_Everon1989", "Prefabs", "Buildings", "Bank");
const CHECK = process.argv.includes("--check");
const id = (s) => createHash("sha1").update(`bank-kit|${s}`).digest("hex").slice(0, 16).toUpperCase();
const n = (v) => String(+(+v).toFixed(3));
const guidOf = (rel) => /Name "\{([0-9A-F]{16})\}/.exec(readFileSync(join(DIR, `${rel}.meta`), "utf8"))[1];

const metaText = (guid, path) => `MetaFileClass {
 Name "{${guid}}${path}"
 Configurations {
  EntityTemplateResourceClass PC {
  }
  EntityTemplateResourceClass XBOX_ONE : PC {
  }
  EntityTemplateResourceClass XBOX_SERIES : PC {
  }
  EntityTemplateResourceClass PS4 : PC {
  }
  EntityTemplateResourceClass PS5 : PC {
  }
  EntityTemplateResourceClass HEADLESS : PC {
  }
 }
}
`;

const ROOM_DOOR = "{B17A72DCB10A840C}Prefabs/Props/Roleplay/BankVault/LF_BankVault_RoomDoor.et";
const BAG = "{65DE48DCC1D4DD01}Prefabs/Items/Roleplay/LF_MoneyBag.et";
const CASH = "{5439738849229352}Prefabs/Items/Currencies/MoneyStack.et";
const ALARM = "{CDE4035B661F9555}Prefabs/Structures/BuildingParts/Eletrical/ElectricityBoxes/ElectricityBox_small_01.et";
const FASCIA = "{4697D9464B596C24}Prefabs/Props/Signs/LF_Sign_Fascia.et";
const LAMP = "{34F74A7DBAD46974}Prefabs/Props/Furniture/LightWall_01.et";
const LAMP_LIT = "{71C4DB9B2591501B}Assets/Props/Furniture/LightWall_01/data/LightWall_01_Glass_lit.emat";

const at = (o, yaw) => [` coords ${n(o[0])} ${n(o[1])} ${n(o[2])}`, yaw ? ` angleY ${n(yaw)}` : null].filter(Boolean);
const child = (cls, ref, key, body, pos, yaw, scale, name = "") =>
  [`  ${cls}${name ? ` ${name}` : ""} : "${ref}" {`, `   ID "${id(key)}"`, ...body.map((l) => `   ${l}`), ...(scale ? [`   scale ${scale}`] : []), ...at(pos, yaw).map((l) => `  ${l}`), "  }"].join("\n");

const rows = [
  // the vault room's door (safe type bank_room supplies the haul, tools and timers); the id is explicit, the swing is
  // cut to 55 degrees so the leaf clears the corridor's far wall
  child("GenericEntity", ROOM_DOOR, "vault-door", ["components {", ` LF_BreachTargetComponent "{A7D41E0C3B985208}" {`, `  m_fSwingYaw ${vault.swing}`, " }", ` LF_BankVaultComponent "{A7D41E0C3B985209}" {`, `  m_sVaultId "${vault.id}"`, " }", "}"], [vault.x, vault.y, vault.z], vault.yaw, null, "BankVault"),
  child("GenericEntity", BAG, "bag", ["components {", ` LF_PlaceComponent "{24218E86E931E2E7}" {`, '  m_sKind "BankBag"', `  m_sId "${vault.id}"`, " }", "}", "Flags 0x403 0"], kit.bag, 0)
    .replace(/\n  }$/, `\n   {\n    GenericEntity : "${CASH}" {\n     ID "${id("bag-cash")}"\n     coords 0 0.15 0\n    }\n   }\n  }`),
  child("GenericEntity", ALARM, "alarm", ["components {", " LF_AlarmMountComponent \"{9ACA0BDAD2FE2665}\" {", "  m_vOffset 0 0.167 0.065", "  m_fReach 12", " }", "}"], [kit.alarmBox.x, kit.alarmBox.y, kit.alarmBox.z], kit.alarmBox.yaw),
  child("GenericEntity", FASCIA, "sign", ["components {", ` LF_SignComponent "{6E3FF3980D87CACE}" {`, '  m_sSignId "bank"', "  m_vRimColor 0.85 0.68 0.3", "  m_vPaintColor 0.42 0.07 0.07", "  m_vTextColor 0.92 0.78 0.42", " }", "}"], [kit.sign.x, kit.sign.y, kit.sign.z], kit.sign.yaw),
  // the door lamp: the vanilla glass wall lamp with its lit glass (a plain static mesh), and one warm point light in
  // front of it, CastShadow 0
  child("GenericEntity", LAMP, "door-lamp", ["components {", ' MeshObject "{5872F0EB7FA40561}" {', "  Materials {", `   MaterialAssignClass "{${id("door-lamp-lit")}}" {`, '    SourceMaterial "LightWall_01_Glass"', `    AssignedMaterial "${LAMP_LIT}"`, "   }", "  }", " }", "}"], [kit.lamp.x, kit.lamp.y, kit.lamp.z], kit.lamp.yaw),
  ["  LightEntity {", `   ID "${id("facade-glow")}"`, `   coords ${n(kit.lamp.x)} ${n(kit.lamp.y - 0.1)} ${n(kit.lamp.z + 0.6)}`, "   Color 1 0.8 0.55 1", "   LV 4.5", "   ClipEV -10", "   CastShadow 0", "   Radius 8", "   NearPlane 0.3", "   LensFlareType Disabled", "  }"].join("\n"),
];

const buildingRef = `{${guidOf("Bank.et")}}Prefabs/Buildings/Bank/Bank.et`;
const text = `SCR_DestructibleBuildingEntity : "${buildingRef}" {\n ID "${id("root")}"\n {\n${rows.join("\n")}\n }\n}\n`;
const file = join(DIR, "Bank_Kit.et");
const meta = `${file}.meta`;
if (!existsSync(meta)) {
  if (CHECK) throw new Error("Bank_Kit.et.meta is missing");
  writeFileSync(meta, metaText(randomBytes(8).toString("hex").toUpperCase(), "Prefabs/Buildings/Bank/Bank_Kit.et"));
}
const cur = existsSync(file) ? readFileSync(file, "utf8") : null;
if (CHECK) {
  console.log(cur === text ? "bank kit: current" : "bank kit: DRIFT (run node tools/buildings/bank/kit.mjs)");
  process.exit(cur === text ? 0 : 1);
}
writeFileSync(file, text);
console.log(`bank kit: ${rows.length} child(ren) written to Bank_Kit.et`);
