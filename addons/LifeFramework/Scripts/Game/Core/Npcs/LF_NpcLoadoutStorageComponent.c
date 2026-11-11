[ComponentEditorProps(category: "EveronLife/Core/Npcs", description: "Holds a station NPC's worn clothing in its loadout slots")]
class LF_NpcLoadoutStorageComponentClass : SCR_EquipedLoadoutStorageComponentClass
{
}

//! The station NPC's equipped-loadout storage (Npc_Base.et). BaseLoadoutManagerComponent puts the
//! NPC's clothing into its slots, and the storage replicates which slot holds which item, so every
//! client wears the clothing instead of leaving it at the NPC's feet (#328), as a vanilla
//! character's storage does. A storage is also an inventory item, so the NPC is hidden from a
//! player's Vicinity: nobody can open it and take the clothes, or pick up the NPC itself.
class LF_NpcLoadoutStorageComponent : SCR_EquipedLoadoutStorageComponent
{
	override protected bool ShouldHideInVicinity()
	{
		return true;
	}
}
