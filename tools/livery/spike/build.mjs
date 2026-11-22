// tools/livery/spike/build.mjs - livery spike #4226: write the test repaints into this checkout's core
// addon so `look item` and a session can render them, or remove them again.
//
//   node tools/livery/spike/build.mjs          write the spike materials and prefabs
//   node tools/livery/spike/build.mjs --clean  remove them (run before ship: they are never committed)
//
// Method (docs/architecture/livery-spike.md): vanilla vehicle bodies and garments are MatPBRMulti
// materials. A repaint is a new .emat that inherits the vanilla variant and overrides its colour
// layers; the prefab points the vanilla mesh slot at it. No new texture, no new .xob, so no bake.
import { existsSync, mkdirSync, rmSync, writeFileSync } from "node:fs";
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
  uazLetteringEmat: "42260000A1000004",
  uazPolice: "42260000B1000001",
  s1203Police: "42260000B1000002",
  jacketTint: "42260000B1000003",
  uazLettering: "42260000B1000004",
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
  // Overlay test: the vanilla lettered mask (PTS) under our colours. A mask is the overlay channel.
  [`${ASSETS}/LF_Spike_UAZ469_Lettering.emat`]: [G.uazLetteringEmat,
    `MatPBRMulti : "{D1F1DD9AE3DC919D}Assets/Vehicles/Wheeled/UAZ469/Data/UAZ469_Body_CIV_BLUE.emat" {
 MaskMap "{7089570FD0ECC263}Assets/Vehicles/Wheeled/UAZ469/Data/UAZ469_Body_PTS_GLOBAL_MASK.edds"
 Enabled_2 1
 Color_2 ${NAVY}
 Color_1 ${WHITE}
}
`],
  [`${ASSETS}/LF_Spike_S1203_Police.emat`]: [G.s1203PoliceEmat,
    `MatPBRMulti : "{0078643DF969CA01}Assets/Vehicles/Wheeled/S1203/Data/S1203_body.emat" {
 Enabled_2 1
 Color_2 ${NAVY}
 Color_1 ${WHITE}
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

const prefabs = {
  [`${PREFABS}/LF_Spike_UAZ469_Police.et`]: [G.uazPolice,
    `Vehicle : "{EFEB161F9BC2031D}Prefabs/Vehicles/Wheeled/UAZ469/LF_UAZ469_Police.et" {
 ID "42260000C1000001"
 components {
${uazMesh(`{${G.uazPoliceEmat}}${ASSETS}/LF_Spike_UAZ469_Police.emat`)} }
}
`],
  [`${PREFABS}/LF_Spike_UAZ469_Lettering.et`]: [G.uazLettering,
    `Vehicle : "{EFEB161F9BC2031D}Prefabs/Vehicles/Wheeled/UAZ469/LF_UAZ469_Police.et" {
 ID "42260000C1000004"
 components {
${uazMesh(`{${G.uazLetteringEmat}}${ASSETS}/LF_Spike_UAZ469_Lettering.emat`)} }
}
`],
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
  [`${PREFABS}/LF_Spike_Jacket_Denim_Orange.et`]: [G.jacketTint,
    `GameEntity : "{43D84EA05C66258C}Prefabs/Characters/Uniforms/Jacket_Denim_01/Jacket_Denim_01_base.et" {
 ID "42260000C1000003"
 components {
  BaseLoadoutClothComponent "{5104869D194C627A}" {
   WornMaterialsOverride {
    "{${G.jacketTintEmat}}${ASSETS}/LF_Spike_Jacket_Denim_Orange.emat"
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

if (process.argv.includes("--clean")) {
  for (const d of [ASSETS, PREFABS]) {
    const abs = join(ADDON, d);
    if (existsSync(abs)) { rmSync(abs, { recursive: true }); console.log(`removed ${d}`); }
  }
} else {
  for (const [rel, [guid, text]] of Object.entries(emats)) write(rel, guid, text, "EMATResourceClass");
  for (const [rel, [guid, text]] of Object.entries(prefabs)) write(rel, guid, text, "EntityTemplateResourceClass");
}
