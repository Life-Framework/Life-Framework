// What each asset kind needs. Positions are Enfusion model space in metres: x right, y up, z forward.
//
// The weapon skeleton is the vanilla M9's, read from Assets/Weapons/Handguns/M9/M9_body.xob with
// xob.mjs on 2026-11-12 (game 1.9). A weapon built on these node names and rest positions is driven
// by the vanilla pistol animation graph without a new animation: the bones must keep these exact
// positions, because the M9 clips write bone-local transforms, and the mesh is fitted around them.
// `node` nodes are sockets (memory points); `bone` nodes are skinned bones.

const M9 = {
  xob: "Assets/Weapons/Handguns/M9/M9_body.xob",
  bounds: { min: [-0.0185, -0.0837, -0.0708], max: [0.0167, 0.0569, 0.1485] },
  nodes: [
    { name: "w_root", parent: null, pos: [0, 0, 0], bone: true },
    { name: "w_hammer", parent: "w_root", pos: [0, 0.02694, -0.04329], bone: true },
    { name: "w_trigger", parent: "w_root", pos: [0, 0.0159, 0.02292], bone: true },
    { name: "w_slide", parent: "w_root", pos: [0, 0.04321, 0.05125], bone: true },
    { name: "w_fire_mode", parent: "w_slide", pos: [-0.00755, 0.00318, -0.08435], bone: true },
    { name: "w_slide_release", parent: "w_root", pos: [-0.01186, 0.02396, 0.0215], bone: true },
    { name: "eye", parent: "w_root", pos: [0, 0.05588, -0.10698] },
    { name: "barrel_muzzle", parent: "w_root", pos: [0.00015, 0.04728, 0.15073], fit: "muzzle" },
    { name: "barrel_chamber", parent: "w_root", pos: [0.00015, 0.04728, 0.01796] },
    { name: "slot_magazine", parent: "w_root", pos: [0.00015, -0.07526, -0.05794] },
  ],
};

import { BUDGETS } from "./budgets.mjs";

export const KINDS = {
  weapon: {
    proven: true,
    assetRoot: "Assets/Weapons/Handguns",
    prefabRoot: "Prefabs/Weapons/Handguns",
    defaultSize: { length: 0.2, height: 0.14, width: 0.036 },
    defaultTris: BUDGETS.weapon.lod0,
    maxTris: BUDGETS.weapon.max,
    reference: M9,
    skeleton: M9.nodes,
    // The hand pose and the reload clips assume the M9 grip, so the mesh is scaled to the M9's
    // height and its rear-bottom corner is put on the M9's (docs/asset-pipeline/socket-checks.md).
    fit: "pistol",
    collider: { prefix: "UCX_", layerPreset: "WeaponFire", gamemat: "{3B2D2687F56BB4EF}Common/Materials/Game/WeaponParts/weapon_metal.gamemat" },
    metaFlags: { ExportSceneHierarchy: true, ExportSkinning: true },
    // Sockets the prefab chain names (PivotID); the import check fails when the .xob lacks one.
    requiredSockets: ["eye", "barrel_muzzle", "barrel_chamber", "slot_magazine"],
    requiredBones: ["w_root", "w_trigger", "w_slide", "w_hammer", "w_fire_mode", "w_slide_release"],
  },
  // A held item. Vanilla held gadgets (Gauze_01_packed.xob) have no bones or sockets: the hand
  // attachment holds the model at its origin, which the "centre" fit puts in the middle of the mesh.
  // The scaffold is prefab-gadget.mjs. Proven with the handcuffs (#664, 2026-11-13).
  gadget: {
    proven: true,
    assetRoot: "Assets/Items/Roleplay",
    prefabRoot: "Prefabs/Items/Roleplay",
    defaultSize: { length: 0.15, height: 0.08, width: 0.03 },
    defaultTris: BUDGETS.gadget.lod0,
    maxTris: BUDGETS.gadget.max,
    fit: "centre",
    collider: { prefix: "UBX_", layerPreset: "ItemFireView", gamemat: "{13581C35484F3000}Common/Materials/Game/fabric.gamemat" },
    requiredSockets: [],
    requiredBones: [],
  },
  // Proven with the police spike strip (tools/assets/spike-strip): one generated link, repeated
  // along the length by fit.repeat, stacked into a folded model by fit.fold, a world prefab and
  // a carried item scaffolded by prefab.mjs.
  deployable: {
    proven: true,
    assetRoot: "Assets/Props/Roleplay",
    prefabRoot: "Prefabs/Props/Roleplay",
    defaultSize: { length: 3.0, height: 0.05, width: 0.3 },
    defaultTris: BUDGETS.deployable.lod0,
    maxTris: BUDGETS.deployable.max,
    fit: "ground",
    // maxHeight: the box covers the base only, so a wheel rolls over the strip instead of climbing it.
    collider: { prefix: "UBX_", layerPreset: "ItemFireView", gamemat: "{3B2D2687F56BB4EF}Common/Materials/Game/WeaponParts/weapon_metal.gamemat", maxHeight: 0.03 },
    requiredSockets: [],
    requiredBones: [],
  },
  // Texture sets with no model: tiling materials for meshes that already exist (the ore veins on the
  // vanilla quarry rock, tools/assets/ore-veins). Every map is a spec variant; no clean, no .xob,
  // no prefab. Proven with the ore veins (2026-11-14).
  material: {
    proven: true,
    assetRoot: "Assets/Materials",
    prefabRoot: "Prefabs",
    defaultSize: { length: 1, height: 1, width: 1 },
    defaultTris: 100,
    maxTris: 100,
    fit: "none",
    collider: null,
    requiredSockets: [],
    requiredBones: [],
  },
};
