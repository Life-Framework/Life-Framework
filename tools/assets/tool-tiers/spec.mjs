// Tool tier finishes (#4188): three tools x three tiers. Each tier prefab inherits the tool's
// existing prefab and swaps the one material for a tinted copy of it, so the mesh and the
// textures are the shipped ones and only the finish changes.
export const OUT_DIR = "addons/LF_Everon1989/Prefabs/Tools/Tiers";
export const OUT_RES = "Prefabs/Tools/Tiers";

// parent: the tool prefab the tier prefab inherits. meshComp: its MeshObject component id.
// source: the .xob's source material slot. bcr/nmo: the tool's own maps (nmo may be absent).
export const TOOLS = [
  {
    key: "Pickaxe",
    parent: "{23341E2840F5ACB4}Prefabs/Tools/Pickaxe.et",
    meshComp: "{59908096775F151D}",
    source: "Pickaxe",
    bcr: "{81838C52E7772E41}Assets/Tools/Pickaxe/Data/T_Pickaxe_BCR.edds",
    nmo: "{B95B37C03E007CDF}Assets/Tools/Pickaxe/Data/T_Pickaxe_NMO.edds",
  },
  {
    key: "Axe",
    parent: "{9CAC68BA551417A4}Prefabs/Tools/Axe/Axe.et",
    meshComp: "{59908096775F151D}",
    source: "None",
    gain: 2.4, // the axe's colour map is dark, so a plain tint barely separates the tiers
    bcr: "{DCEEC5CF4B5CB8DC}Assets/Tools/Axe/Data/Axe_BCR.edds",
    nmo: null,
  },
  {
    key: "Rod",
    parent: "{0327968D17D249FD}Prefabs/Items/Fishing/LF_FishingRod.et",
    meshComp: "{DB9AEA85B40D5D8E}",
    source: "FishingRod",
    bcr: "{A31BDC2E23C8BA4A}Assets/Items/Fishing/FishingRod/Data/FishingRod_BCR.edds",
    nmo: "{B1B2F7B02D5EDA26}Assets/Items/Fishing/FishingRod/Data/FishingRod_NMO.edds",
  },
];

// color multiplies the tool's base colour map; metal scales the metalness the NMO map gives.
export const TIERS = [
  { key: "Worn", color: "0.50 0.34 0.24 1", metal: 0.1 },
  { key: "Standard", color: "0.85 0.85 0.82 1", metal: 0.5 },
  { key: "Fine", color: "0.55 0.75 1 1", metal: 1 },
];

export const name = (tool, tier) => `LF_Tool_${tool.key}_${tier.key}`;
