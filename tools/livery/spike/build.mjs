// tools/livery/spike/build.mjs - livery spike #4226: write the test repaints into this checkout's core
// addon so `look item` and a session can render them, or remove them again.
//
//   node tools/livery/spike/build.mjs          write the spike materials and prefabs
//   node tools/livery/spike/build.mjs --mask <spec.json>  also build our UAZ469 mask from the vanilla
//        CIV mask (mask.py in Blender) and import it through Workbench (tools\cli ui-image --batch)
//   node tools/livery/spike/build.mjs --clean  remove them (run before ship: they are never committed)
//
// Method (docs/architecture/livery-spike.md): vanilla vehicle bodies and garments are MatPBRMulti
// materials. A repaint is a new .emat that inherits the vanilla variant and overrides its colour
// layers; the prefab points the vanilla mesh slot at it. No new texture, no new .xob, so no bake.
import { existsSync, mkdirSync, rmSync, statSync, writeFileSync } from "node:fs";
import { spawnSync } from "node:child_process";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..", "..");
const ADDON = join(ROOT, "addons", "LifeFramework");
const ASSETS = "Assets/LiverySpike";
const PREFABS = "Prefabs/LiverySpike";

// Fixed spike GUIDs (prefix 4226): stable across runs so a rebuild does not shuffle them.
const G = {
  uazPoliceEmat: "42260000A1000001",
  s1203PoliceEmat: "42260000A1000002",
  jacketTintEmat: "42260000A1000003",
  uazPolice: "42260000B1000001",
  s1203Police: "42260000B1000002",
  jacketTint: "42260000B1000003",
  uazMask: "42260000D1000001",
  uazOwnEmat: "42260000A1000005",
  uazOwn: "42260000B1000005",
  jacketItemEmat: "42260000A1000006",
};

// Police colours: white body, navy secondary layer (the vanilla mask's second paint area).
const WHITE = "0.62 0.62 0.6 1";
const NAVY = "0.012 0.03 0.09 1";

const emats = {
  [`${ASSETS}/LF_Spike_UAZ469_Police.emat`]: [G.uazPoliceEmat,
    `MatPBRMulti : "{D1F1DD9AE3DC919D}Assets/Vehicles/Wheeled/UAZ469/Data/UAZ469_Body_CIV_BLUE.emat" {
 Color_2 ${NAVY}
 Color_1 ${WHITE}
}
`],
  [`${ASSETS}/LF_Spike_S1203_Police.emat`]: [G.s1203PoliceEmat,
    `MatPBRMulti : "{0078643DF969CA01}Assets/Vehicles/Wheeled/S1203/Data/S1203_body.emat" {
 Enabled_2 0
 Color_1 ${WHITE}
}
`],
  // Our own mask: the second colour (navy) wherever mask.py drew in R ("POLICE" on the doors).
  [`${ASSETS}/LF_Spike_UAZ469_OwnMask.emat`]: [G.uazOwnEmat,
    `MatPBRMulti : "{D1F1DD9AE3DC919D}Assets/Vehicles/Wheeled/UAZ469/Data/UAZ469_Body_CIV_BLUE.emat" {
 MaskMap "{${G.uazMask}}${ASSETS}/LF_Spike_UAZ469_Police_MASK.edds"
 Enabled_2 1
 Color_2 ${NAVY}
 Mask_2 ""
 MaskOffset_2 0.95
 MaskSharpness_2 10
 Color_1 ${WHITE}
}
`],
  // The ground model is MatPBRBasic: Color multiplies its BCR.
  [`${ASSETS}/LF_Spike_Jacket_Denim_Orange_item.emat`]: [G.jacketItemEmat,
    `MatPBRBasic : "{1F2252814775E921}Assets/Characters/Uniforms/Jacket_Denim_01/Data/Jacket_Denim_01_item.emat" {
 Color 1 0.45 0.1 1
}
`],
  // Tint: Color_1 multiplies the garment's own BCR texture (work orange).
  [`${ASSETS}/LF_Spike_Jacket_Denim_Orange.emat`]: [G.jacketTintEmat,
    `MatPBRMulti : "{099DB33E51EB0AE1}Assets/Characters/Uniforms/Jacket_Denim_01/Data/Jacket_Denim_01_Brown.emat" {
 Color_1 1 0.45 0.1 1
}
`],
};

const uazMesh = (emat) => `  MeshObject "{51DAA09FEFBFC0E7}" {
   Materials {
    MaterialAssignClass "{64F09BE9246E252A}" {
     SourceMaterial "UAZ469_Body"
     AssignedMaterial "${emat}"
    }
   }
  }
`;

// The UAZ469's doors and door tops are separate slot entities with their own MeshObject on the same
// "UAZ469_Body" material (and so the same mask atlas). A livery that only overrides the body leaves
// the vanilla doors (blue on the CIV_blue base). Each variant gets its own door and top prefabs, and
// the vehicle's door slots point at them. Component and assign GUIDs are read from the vanilla files.
const DOORS = ["L01", "R01", "L02", "R02"];
const VANILLA_DOOR = (d) => `Prefabs/Vehicles/Wheeled/UAZ469/VehParts/Doors/Blue/UAZ469_door_${d}_blue.et`;
const VANILLA_TOP = (d) => `Prefabs/Vehicles/Wheeled/UAZ469/VehParts/Doors/Blue/UAZ469_door_${d}_top_blue.et`;

function bodyOverride(text, emat) {
  const mesh = /MeshObject "\{([0-9A-F]{16})\}"/.exec(text);
  const assign = /MaterialAssignClass "\{([0-9A-F]{16})\}" \{\s*SourceMaterial "UAZ469_Body"\s/.exec(text);
  if (!mesh || !assign) throw new Error("no UAZ469_Body material slot in a vanilla door prefab");
  return `  MeshObject "{${mesh[1]}}" {
   Materials {
    MaterialAssignClass "{${assign[1]}}" {
     SourceMaterial "UAZ469_Body"
     AssignedMaterial "${emat}"
    }
   }
  }
`;
}

async function uazVariant(vfs, tag, idx, vehGuid, emat) {
  const guidOf = (kind, d) => `4226${kind}${idx}${String(d).padStart(2, "0")}`.padEnd(16, "0");
  const slots = [];
  for (const [i, d] of DOORS.entries()) {
    const vDoor = vfs.readFile(VANILLA_DOOR(d)).toString("utf8");
    const vTop = vfs.readFile(VANILLA_TOP(d)).toString("utf8");
    const veh = vfs.readFile("Prefabs/Vehicles/Wheeled/UAZ469/UAZ469_covered_CIV_blue.et").toString("utf8");
    const doorGuid = veh.match(new RegExp(`Prefab "\\{([0-9A-F]{16})\\}${VANILLA_DOOR(d)}"`))[1];
    const topGuid = /Prefab "\{([0-9A-F]{16})\}/.exec(vDoor)[1];
    const topRel = `${PREFABS}/UAZ469_${tag}_door_${d}_top.et`;
    const doorRel = `${PREFABS}/UAZ469_${tag}_door_${d}.et`;
    write(topRel, guidOf("F1", i), `GenericEntity : "{${topGuid}}${VANILLA_TOP(d)}" {
 ID "${guidOf("F3", i)}"
 components {
${bodyOverride(vTop, emat)} }
}
`, "EntityTemplateResourceClass");
    const slotMgr = /SlotManagerComponent "\{([0-9A-F]{16})\}"/.exec(vDoor)[1];
    write(doorRel, guidOf("F2", i), `GenericEntity : "{${doorGuid}}${VANILLA_DOOR(d)}" {
 ID "${guidOf("F4", i)}"
 components {
${bodyOverride(vDoor, emat)}  SlotManagerComponent "{${slotMgr}}" {
   Slots {
    RegisteringComponentSlotInfo DoorTop {
     Prefab "{${guidOf("F1", i)}}${topRel}"
    }
   }
  }
 }
}
`, "EntityTemplateResourceClass");
    slots.push(`    RegisteringComponentSlotInfo Door_${d} {
     Prefab "{${guidOf("F2", i)}}${doorRel}"
    }
`);
  }
  write(`${PREFABS}/LF_Spike_UAZ469_${tag}.et`, vehGuid, `Vehicle : "{EFEB161F9BC2031D}Prefabs/Vehicles/Wheeled/UAZ469/LF_UAZ469_Police.et" {
 ID "${guidOf("F5", 0)}"
 components {
${uazMesh(emat)}  SlotManagerComponent "{55BCE45E438E4CFF}" {
   Slots {
${slots.join("")}   }
  }
 }
}
`, "EntityTemplateResourceClass");
}

const prefabs = {
  [`${PREFABS}/LF_Spike_S1203_Police.et`]: [G.s1203Police,
    `Vehicle : "{543799AC5C52989C}Prefabs/Vehicles/Wheeled/S1203/S1203_transport_beige.et" {
 ID "42260000C1000002"
 components {
  MeshObject "{51DAA09FEFBFC0E7}" {
   Materials {
    MaterialAssignClass "{64DA70D3831058D9}" {
     SourceMaterial "S1203_body"
     AssignedMaterial "{${G.s1203PoliceEmat}}${ASSETS}/LF_Spike_S1203_Police.emat"
    }
   }
  }
 }
}
`],
  // The worn jacket model with the tint on a shop mannequin (the place tool cannot measure a character,
  // so a session or a test spawns the worker below; the mannequin carries the same worn material).
  [`${PREFABS}/LF_Spike_Mannequin_Orange.et`]: ["42260000B1000007",
    `GenericEntity : "{DAEBE5DD34859027}Prefabs/Props/Commercial/LF_Mannequin.et" {
 ID "42260000C1000007"
 {
  GenericEntity : "{8733B60DA6679BF0}Prefabs/Props/Commercial/LF_MannequinGarment.et" {
   ID "42260000C1000008"
   components {
    MeshObject "{9F2610C81BC2FC77}" {
     Object "{6F223F9465D881DF}Assets/Characters/Uniforms/Jacket_Denim_01/Jacket_Denim_01.xob"
     Materials {
      MaterialAssignClass "{42260000E1000002}" {
       SourceMaterial "Jacket_Denim_01"
       AssignedMaterial "{${G.jacketTintEmat}}${ASSETS}/LF_Spike_Jacket_Denim_Orange.emat"
      }
     }
    }
   }
  }
 }
}
`],
  // A vanilla construction worker wearing the tinted jacket: the worn path (loadout slot) for pictures.
  [`${PREFABS}/LF_Spike_Worker_Orange.et`]: ["42260000B1000006",
    `SCR_ChimeraCharacter : "{6F5A71376479B353}Prefabs/Characters/Factions/CIV/ConstructionWorker/Character_CIV_ConstructionWorker_1.et" {
 ID "42260000C1000006"
 components {
  BaseLoadoutManagerComponent "{520EA1D2DB11821E}" {
   Slots {
    LoadoutSlotInfo Jacket {
     Prefab "{${G.jacketTint}}${PREFABS}/LF_Spike_Jacket_Denim_Orange.et"
    }
   }
  }
 }
}
`],
  [`${PREFABS}/LF_Spike_Jacket_Denim_Orange.et`]: [G.jacketTint,
    `GameEntity : "{43D84EA05C66258C}Prefabs/Characters/Uniforms/Jacket_Denim_01/Jacket_Denim_01_base.et" {
 ID "42260000C1000003"
 components {
  BaseLoadoutClothComponent "{5104869D194C627A}" {
   WornMaterialsOverride {
    "{${G.jacketTintEmat}}${ASSETS}/LF_Spike_Jacket_Denim_Orange.emat"
   }
   ItemMaterialsOverride {
    "{${G.jacketItemEmat}}${ASSETS}/LF_Spike_Jacket_Denim_Orange_item.emat"
   }
  }
 }
}
`],
};

function meta(path, guid, cls) {
  const plats = ["XBOX_ONE", "XBOX_SERIES", "PS4", "PS5", "HEADLESS"];
  return `MetaFileClass {
 Name "{${guid}}${path}"
 Configurations {
  ${cls} PC {
  }
${plats.map((p) => `  ${cls} ${p} : PC {\n  }\n`).join("")} }
}
`;
}

function write(rel, guid, text, cls) {
  const abs = join(ADDON, rel);
  mkdirSync(dirname(abs), { recursive: true });
  writeFileSync(abs, text);
  writeFileSync(abs + ".meta", meta(rel, guid, cls));
  console.log(`wrote {${guid}}${rel}`);
}

// The mask texture: TextureMask preset (BC7, linear), as Book_GLOBAL_MASK uses.
const MASK_CONF = { PC: "2231C67ACC4BB0AC", XBOX_ONE: "3B78A666261DD8B4", XBOX_SERIES: "9A79E1F659B378E7", PS4: "5CB64ED6EEBDE368", HEADLESS: "0B025DFAF55852CD" };
function maskMeta(rel, guid) {
  const row = (p) => `  TIFFResourceClass ${p} : "{${MASK_CONF[p]}}Configs/System/ResourceTypes/${p}/TextureMask.conf" {
  }
`;
  return `MetaFileClass {
 Name "{${guid}}${rel}"
 Configurations {
${Object.keys(MASK_CONF).map(row).join("")}  TIFFResourceClass PS5 : PC {
  }
 }
}
`;
}

const BLENDER = process.env.LF_BLENDER || "C:/Program Files/Blender Foundation/Blender 5.3/blender.exe";

async function buildMask(spec) {
  const { gameVfs } = await import("../../lib/game-prefab.mjs");
  const { eddsToDds } = await import("./edds.mjs");
  const work = join(ROOT, "tmp", "livery");
  mkdirSync(work, { recursive: true });
  const dds = join(work, "uaz_CIV.dds");
  writeFileSync(dds, eddsToDds((await gameVfs()).readFile("Assets/Vehicles/Wheeled/UAZ469/Data/UAZ469_Body_CIV_GLOBAL_MASK.edds")).dds);
  const rel = `${ASSETS}/LF_Spike_UAZ469_Police_MASK`;
  const tif = join(ADDON, rel + ".tif");
  mkdirSync(dirname(tif), { recursive: true });
  const r = spawnSync(BLENDER, ["-b", "--factory-startup", "--python", join(ROOT, "tools", "livery", "spike", "mask.py"), "--", dds, tif, resolve(spec)], { encoding: "utf8", windowsHide: true, timeout: 180000 });
  if (!/LF_MASK wrote/.test(r.stdout || "")) throw new Error("mask.py failed: " + (r.stdout || "") + (r.stderr || ""));
  writeFileSync(join(ADDON, rel + ".edds.meta"), maskMeta(rel + ".edds", G.uazMask));
  const edds = join(ADDON, rel + ".edds");
  if (existsSync(edds) && statSync(edds).mtimeMs < statSync(tif).mtimeMs) rmSync(edds);
  const list = join(work, "mask-batch.txt");
  writeFileSync(list, `addons/LifeFramework/${rel}.tif
`);
  const imp = spawnSync(process.execPath, [join(ROOT, "tools", "cli.mjs"), "ui-image", "--batch", "tmp/livery/mask-batch.txt"], { cwd: ROOT, encoding: "utf8", windowsHide: true, timeout: 600000 });
  console.log((imp.stdout || "").trim().split("\n").slice(-2).join("\n"));
  if (!existsSync(edds)) throw new Error("the import built no " + rel + ".edds");
  console.log(`mask {${G.uazMask}}${rel}.edds (preview ${tif.replace(/\.tif$/, "_preview.png")})`);
}

if (process.argv.includes("--clean")) {
  for (const d of [ASSETS, PREFABS]) {
    const abs = join(ADDON, d);
    if (existsSync(abs)) { rmSync(abs, { recursive: true }); console.log(`removed ${d}`); }
  }
} else {
  for (const [rel, [guid, text]] of Object.entries(emats)) write(rel, guid, text, "EMATResourceClass");
  for (const [rel, [guid, text]] of Object.entries(prefabs)) write(rel, guid, text, "EntityTemplateResourceClass");
  const { gameVfs } = await import("../../lib/game-prefab.mjs");
  const vfs = await gameVfs();
  await uazVariant(vfs, "Police", 1, G.uazPolice, `{${G.uazPoliceEmat}}${ASSETS}/LF_Spike_UAZ469_Police.emat`);
  await uazVariant(vfs, "OwnMask", 2, G.uazOwn, `{${G.uazOwnEmat}}${ASSETS}/LF_Spike_UAZ469_OwnMask.emat`);
  const mi = process.argv.indexOf("--mask");
  if (mi > 0) await buildMask(process.argv[mi + 1]);
}
