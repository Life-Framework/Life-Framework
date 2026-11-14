// The robber's 1989 civilian look, the same in every shot: a denim jacket over a brown shirt, dark grey trousers,
// a black knitted cap pulled low. The base game has no civilian shoes, so the boots stay (every civilian NPC in this
// repo wears the same Soviet boots). No vest, helmet, backpack or uniform part. `shoot.mjs setup` puts these on with
// the harness `itemwear` verb and warns when anything military is left.
export const COSTUME = [
  "{27162F1F6EE6581A}Prefabs/Characters/Uniforms/Jacket_Denim_01/Jacket_Denim_01_brownShirt.et",
  "{5A8C9A020BD7ABA6}Prefabs/Characters/Uniforms/Pants_Trousers_01/Pants_Trousers_01_DarkGrey.et",
  "{BE19A15F1073E7BD}Prefabs/Characters/HeadGear/Hat_Knitted_01/Hat_Knitted_01_black.et",
];
// Taken off when the spawn loadout carries them.
export const STRIP = ["{06B68C58B72EAAC6}Prefabs/Items/Equipment/Backpacks/Backpack_ALICE_Medium.et"];
