// Bank_Kit.et: the bank as one placeable prefab. It inherits Bank.et (the Office_E_01 override with its furniture)
// and adds the interactables as children: the vault annex (LF_BankVault, drill and charge) with its hood and the
// cash bag, the bell box and the fascia sign on the front, and a warm glow by the door. Everything is existing
// systems; nothing here is a new component. Tellers stand in the layer (stage.mjs, kit rows of `town fit`).
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

const VAULT = "{6177818988438527}Prefabs/Props/Roleplay/BankVault/LF_BankVault.et";
const HOOD = "{7A3C5E9D1B4F6082}Prefabs/Props/Roleplay/BankVault/LF_BankVault_SmallRoof.et";
const BAG = "{65DE48DCC1D4DD01}Prefabs/Items/Roleplay/LF_MoneyBag.et";
const CASH = "{5439738849229352}Prefabs/Items/Currencies/MoneyStack.et";
const ALARM = "{CDE4035B661F9555}Prefabs/Structures/BuildingParts/Eletrical/ElectricityBoxes/ElectricityBox_small_01.et";
const FASCIA = "{4697D9464B596C24}Prefabs/Props/Signs/LF_Sign_Fascia.et";

const at = (o, yaw) => [` coords ${n(o[0])} ${n(o[1])} ${n(o[2])}`, yaw ? ` angleY ${n(yaw)}` : null].filter(Boolean);
const child = (cls, ref, key, body, pos, yaw, scale) =>
  [`  ${cls} : "${ref}" {`, `   ID "${id(key)}"`, ...body.map((l) => `   ${l}`), ...(scale ? [`   scale ${scale}`] : []), ...at(pos, yaw).map((l) => `  ${l}`), "  }"].join("\n");
// the vault's own frame (yaw 0 here) to building space
const inVault = (o) => [vault.x + o[0], vault.y + o[1], vault.z + o[2]];

const rows = [
  child("GenericEntity", VAULT, "vault", ["components {", ` LF_BankVaultComponent "{3B1F6C0A7D2E4858}" {`, `  m_sVaultId "${vault.id}"`, " }", "}"], [vault.x, vault.y, vault.z], vault.yaw),
  child("GenericEntity", HOOD, "hood", [], inVault(kit.hood), 0, 1.2),
  child("GenericEntity", BAG, "bag", ["components {", ` LF_PlaceComponent "{24218E86E931E2E7}" {`, '  m_sKind "BankBag"', `  m_sId "${vault.id}"`, " }", "}", "Flags 0x403 0"], inVault(kit.bag), 0)
    .replace(/\n  }$/, `\n   {\n    GenericEntity : "${CASH}" {\n     ID "${id("bag-cash")}"\n     coords 0 0.15 0\n    }\n   }\n  }`),
  child("GenericEntity", ALARM, "alarm", ["components {", " LF_AlarmMountComponent \"{9ACA0BDAD2FE2665}\" {", "  m_vOffset 0 0.167 0.065", "  m_fReach 12", " }", "}"], [kit.alarmBox.x, kit.alarmBox.y, kit.alarmBox.z], kit.alarmBox.yaw),
  child("GenericEntity", FASCIA, "sign", ["components {", ` LF_SignComponent "{6E3FF3980D87CACE}" {`, '  m_sSignId "bank"', "  m_vRimColor 0.85 0.68 0.3", "  m_vPaintColor 0.42 0.07 0.07", "  m_vTextColor 0.92 0.78 0.42", " }", "}"], [kit.sign.x, kit.sign.y, kit.sign.z], kit.sign.yaw),
  // the facade glow: one warm point light by the door, no shadow (the vanilla `_on` wall lamps carry three lights with CastShadow unset, so a fixture mesh is left out)
  ["  LightEntity {", `   ID "${id("facade-glow")}"`, `   coords ${n(kit.lamp.x)} ${n(kit.lamp.y - 0.4)} ${n(kit.lamp.z + 1.0)}`, "   Color 1 0.78 0.5 1", "   LV 9", "   ClipEV -10", "   CastShadow 0", "   Radius 9", "   NearPlane 0.3", "   LensFlareType Disabled", "  }"].join("\n"),
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
