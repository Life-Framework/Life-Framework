// Stage "prefab": scaffold the prefabs that put the imported model in the game. Each file is written
// once and never overwritten: after the scaffold it is an ordinary prefab an agent or a human edits.
// A weapon inherits the vanilla M9 (its skeleton, animation graph and hand pose are what the mesh
// was fitted to) and gets its own magazine, ammo and magazine config.
import { existsSync, mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { freshGuid, readGuid, usedGuids } from "./meta.mjs";

const PLATFORMS = ["PC", "XBOX_ONE", "XBOX_SERIES", "PS4", "HEADLESS"];

export function resourceMeta(resPath, guid, cls) {
  return `MetaFileClass {\n Name "{${guid}}${resPath}"\n Configurations {\n${PLATFORMS.map((p, i) => `  ${cls} ${p}${i ? " : PC" : ""} {\n  }\n`).join("")} }\n}\n`;
}

// Vanilla resources a pistol-skeleton weapon builds on (game 1.9; read with game_read 2026-11-12).
const VANILLA = {
  pistolBase: "{809488C4984AC087}Prefabs/Weapons/Handguns/M9/Handgun_M9_base.et",
  magazineBase: "{DFFBF5F097FCF797}Prefabs/Weapons/Magazines/Magazine_9x19_M9_15rnd_Base.et",
  bullet: "{9D290AA75ADA4618}Prefabs/Weapons/Ammo/Ammo_9x19_Ball_M882.et",
};

const fmt = (n) => String(+Number(n).toFixed(4));

function componentBlock(c, guid) {
  const props = Object.entries(c.props || {}).map(([k, v]) => `   ${k} ${typeof v === "string" ? JSON.stringify(v) : v}\n`).join("");
  return `  ${c.class} "{${guid}}" {\n${props}  }\n`;
}

export function weaponPrefab({ id, xob, magazine, w, report, guids }) {
  const { min, max } = report.bounds;
  const top = max[1];
  const extra = (w.components || []).map((c, i) => componentBlock(c, guids.components[i])).join("");
  const well = w.magazine_well ? `     MagazineWell ${w.magazine_well} "{${guids.well}}" {\n     }\n` : "";
  return `GenericEntity : "${VANILLA.pistolBase}" {
 ID "${id}"
 components {
  MeshObject "{CFBAA4B701F95D69}" {
   Object "${xob}"
  }
  SCR_WeaponAttachmentsStorageComponent "{51F080D5CE45A1A2}" {
   Attributes SCR_ItemAttributeCollection "{51F080D5C64F12C5}" {
    ItemDisplayName WeaponUIInfo "{5222CB0701437228}" {
     Name "${w.name_key}"
     Description "${w.description_key}"
    }
    ItemPhysAttributes ItemPhysicalAttributes "{5534C5023A57E47F}" {
     Weight ${fmt(w.weight ?? 0.9)}
    }
   }
  }
  WeaponComponent "{CFBAA4B706BA66E8}" {
   components {
    SightsComponent "{BB23A637957CFFF8}" {
     SightsPointFront PointInfo "{5F28F96E9EAABF60}" {
      Offset 0 ${fmt(top)} ${fmt(max[2] - 0.02)}
     }
     SightsPointRear PointInfo "{5F28F96EEC0F2DB0}" {
      Offset 0 ${fmt(top)} ${fmt(min[2] + 0.03)}
     }
    }
    MuzzleComponent "{50F64C45E7271D47}" {
${well}     MagazineTemplate "${magazine}"
    }
   }
   UIInfo WeaponUIInfo "{CC3BA6A2C42F09F4}" {
    Name "${w.name_key}"
    Description "${w.description_key}"
   }
  }
${extra} }
}
`;
}

export function magazinePrefab({ id, ammoConf, w, guids }) {
  const well = w.magazine_well ? `   MagazineWell ${w.magazine_well} "{${guids.well}}" {\n   }\n` : "";
  const rounds = w.max_ammo ?? 15;
  return `GenericEntity : "${VANILLA.magazineBase}" {
 ID "${id}"
 components {
  InventoryMagazineComponent "{51EED12B803954CC}" {
   Attributes SCR_ItemAttributeCollection "{51EED12B86960275}" {
    ItemDisplayName UIInfo "{51FEE38527BFEA5B}" {
     Name "${w.magazine_name_key}"
     Description "${w.magazine_description_key}"
    }
   }
  }
  SCR_MagazineComponent "{CA6BE4D6B4DAFD69}" {
${well}   UIInfo MagazineUIInfo "{57126733AA401586}" {
    Name "${w.magazine_name_key}"
    Description "${w.magazine_description_key}"
   }
   MaxAmmo ${rounds}
   AmmoConfig "${ammoConf}"
   AmmoMapping {
    ${Array(rounds).fill(0).join(" ")}
   }
  }
 }
}
`;
}

export function ammoPrefab({ id, w }) {
  return `Projectile : "${VANILLA.bullet}" {
 ID "${id}"
 components {
  ShellMoveComponent "{851AA4A2AE0A5691}" {
   InitSpeed ${fmt(w.init_speed ?? 380)}
   InitSpeedVariation 0
   ProjectileEffects {
    ProjectileDamage "{5168BBF3DBD79134}" {
     DamageValue ${fmt(w.damage ?? 50)}
    }
   }
  }
 }
}
`;
}

export const ammoConf = (ammo) => `MagazineConfig {\n AmmoResourceArray {\n  "${ammo}"\n }\n}\n`;

// Every projectile a magazine can hold has to be in the game's ammunition list, or the magazine's
// class raises a VME at load ("unable to find ... in the SCR_MagazineRepackingSystem ammunition
// list"). A mod adds to that list with a same-GUID override of the vanilla config; `+ {` appends
// to the inherited array (the same shape as Configs/System/chimeraInputCommon.conf).
const AMMO_LIST = { rel: "Configs/Weapons/Ammo/AmmoList.conf", guid: "DAA31F8A78F30455" };

export function ammoListWith(text, ammo) {
  if (!text) return `SCR_AmmoList {\n m_aAmmoList + {\n  "${ammo}"\n }\n}\n`;
  if (text.includes(`"${ammo}"`)) return text;
  const at = text.indexOf(" }\n}");
  if (!/m_aAmmoList \+ \{/.test(text) || at < 0) throw new Error(`${AMMO_LIST.rel} is not the append-only override this tool writes; add "${ammo}" by hand`);
  return `${text.slice(0, at)}  "${ammo}"\n${text.slice(at)}`;
}

function listAmmo(addon, ammo) {
  const abs = join(addon, AMMO_LIST.rel);
  mkdirSync(dirname(abs), { recursive: true });
  const before = existsSync(abs) ? readFileSync(abs, "utf8") : "";
  const after = ammoListWith(before, ammo);
  if (after !== before) writeFileSync(abs, after);
  if (!existsSync(abs + ".meta")) writeFileSync(abs + ".meta", resourceMeta(AMMO_LIST.rel, AMMO_LIST.guid, "CONFResourceClass"));
  return after !== before;
}

export async function writePrefabs(ctx) {
  const { root, spec, dir } = ctx;
  if (spec.kind === "deployable") return (await import("./prefab-deployable.mjs")).writeDeployablePrefabs(ctx);
  if (spec.kind !== "weapon") {
    // Every other kind's scaffold is its own file, prefab-<kind>.mjs, exporting writePrefabs(ctx).
    const own = new URL(`./prefab-${spec.kind}.mjs`, import.meta.url);
    if (existsSync(fileURLToPath(own))) return (await import(own.href)).writePrefabs(ctx);
    console.log(`prefab: no scaffold for kind ${spec.kind} yet; write the prefab by hand (docs/asset-pipeline/README.md "Kinds")`);
    return { skipped: true };
  }
  const w = spec.weapon;
  if (!w?.name_key || !w?.description_key || !w?.magazine_name_key || !w?.magazine_description_key) {
    throw new Error("spec.weapon needs name_key, description_key, magazine_name_key and magazine_description_key (string table keys, with the #)");
  }
  const addon = join(root, "addons", "LifeFramework");
  const report = JSON.parse(readFileSync(join(dir, "clean", "report.json"), "utf8"));
  const xobRes = `${spec.asset_dir}/${spec.name}.xob`;
  const xobGuid = readGuid(join(addon, xobRes + ".meta"));
  if (!xobGuid || !existsSync(join(addon, xobRes))) throw new Error(`no imported model at ${xobRes}; run the import stage first`);

  const used = usedGuids(addon);
  const files = {
    ammo: `${spec.prefab_dir}/Ammo_${spec.name}.et`,
    conf: `Configs/Weapons/Ammo/LF_Ammo_${spec.name}.conf`,
    magazine: `${spec.prefab_dir}/Magazine_${spec.name}.et`,
    weapon: `${spec.prefab_dir}/Handgun_${spec.name}.et`,
  };
  const ref = {};
  for (const [k, rel] of Object.entries(files)) ref[k] = `{${readGuid(join(addon, rel + ".meta")) || freshGuid(used)}}${rel}`;
  const guids = { well: freshGuid(used), components: (w.components || []).map(() => freshGuid(used)) };
  const magGuids = { well: freshGuid(used) };
  const text = {
    ammo: ammoPrefab({ id: freshGuid(used), w }),
    conf: ammoConf(ref.ammo),
    magazine: magazinePrefab({ id: freshGuid(used), ammoConf: ref.conf, w, guids: magGuids }),
    weapon: weaponPrefab({ id: freshGuid(used), xob: `{${xobGuid}}${xobRes}`, magazine: ref.magazine, w, report, guids }),
  };
  const written = [];
  for (const [k, rel] of Object.entries(files)) {
    const abs = join(addon, rel);
    mkdirSync(dirname(abs), { recursive: true });
    if (!existsSync(abs)) { writeFileSync(abs, text[k]); written.push(rel); }
    if (!existsSync(abs + ".meta")) writeFileSync(abs + ".meta", resourceMeta(rel, ref[k].slice(1, 17), rel.endsWith(".conf") ? "CONFResourceClass" : "EntityTemplateResourceClass"));
  }
  if (listAmmo(addon, ref.ammo)) written.push(AMMO_LIST.rel);
  // A scaffold that exists is not rewritten, so check the one thing a re-import can break.
  const weaponText = readFileSync(join(addon, files.weapon), "utf8");
  if (!weaponText.includes(`{${xobGuid}}${xobRes}`)) throw new Error(`${files.weapon} does not reference {${xobGuid}}${xobRes}`);
  console.log(`prefab: ${written.length ? "wrote " + written.join(", ") : "all four files exist, left as they are"}`);
  console.log(`  weapon ${ref.weapon}`);
  console.log(`  add the string table keys ${[w.name_key, w.description_key, w.magazine_name_key, w.magazine_description_key].join(", ")} and run tools\\cli regen-localization`);
  return ref;
}
