// red-proof: Observed 2026-11-10/11 with `tools\cli test --no-build --only LF_Test_NpcWearsClothes`.
// Before Npc_Base.et had a storage: `[LFTEST]   - {850859F356F5058E}Prefabs/Characters/Npcs/Npc_Default_01.et
// holds every worn item in its equipped-loadout storage, which replicates the slot to clients (expected 3, got 0)`,
// the same for 2 more prefabs and all 22 placed NPCs. With vanilla SCR_EquipedLoadoutStorageComponent:
// `- ... Npc_Default_01.et is hidden from a player's inventory Vicinity`. Green with LF_NpcLoadoutStorageComponent.
// tier: WORLD
//! Every NPC wears its loadout: jacket, pants and boots are attached to the NPC (children carrying
//! BaseLoadoutClothComponent), each sits in a slot of the NPC's equipped-loadout storage (the
//! inventory state a client receives, so it wears the item instead of dropping it at the NPC's
//! feet, #328), and no loose cloth item lies near it. Checks a fresh spawn of each NPC prefab and
//! every dressed NPC the DebugWorld town places.
class LF_Test_NpcWearsClothes : LF_Test
{
	protected ref array<IEntity> m_aFound = {};

	override string GetName()
	{
		return "npc/wears-clothes";
	}

	protected bool InsertFound(IEntity entity)
	{
		m_aFound.Insert(entity);
		return true;
	}

	protected bool FilterCloth(IEntity entity)
	{
		return entity && entity.FindComponent(BaseLoadoutClothComponent) != null && entity.GetParent() == null;
	}

	//! Cloth items lying free (no parent) within 3 m of the NPC.
	protected int CountLoose(IEntity npc)
	{
		m_aFound = new array<IEntity>();
		GetGame().GetWorld().QueryEntitiesBySphere(npc.GetOrigin(), 3, InsertFound, FilterCloth);
		return m_aFound.Count();
	}

	//! True when the cloth sits in a slot of the NPC's own equipped-loadout storage.
	protected bool InNpcStorage(IEntity npc, IEntity cloth)
	{
		InventoryItemComponent item = InventoryItemComponent.Cast(cloth.FindComponent(InventoryItemComponent));
		if (!item)
			return false;
		InventoryStorageSlot slot = item.GetParentSlot();
		if (!slot)
			return false;
		EquipedLoadoutStorageComponent storage = EquipedLoadoutStorageComponent.Cast(slot.GetStorage());
		return storage && storage.GetOwner() == npc;
	}

	protected void Check(LF_TestContext ctx, IEntity npc, string label)
	{
		int worn = 0;
		int stored = 0;
		string names;
		IEntity child = npc.GetChildren();
		while (child)
		{
			if (child.FindComponent(BaseLoadoutClothComponent))
			{
				worn++;
				names += LF_Utils.GetPrefabName(child) + " ";
				if (InNpcStorage(npc, child))
					stored++;
			}
			child = child.GetSibling();
		}
		ctx.True(worn >= 3, label + " wears jacket, pants and boots (wears " + worn + ": " + names + ")");
		ctx.Equal(worn, stored, label + " holds every worn item in its equipped-loadout storage, which replicates the slot to clients");
		ctx.Equal(0, CountLoose(npc), label + " has no loose clothing lying nearby");

		// The storage is an inventory item too: a player's Vicinity must not list the NPC, or they
		// could open it and take the clothes.
		EquipedLoadoutStorageComponent storage = EquipedLoadoutStorageComponent.Cast(npc.FindComponent(EquipedLoadoutStorageComponent));
		ctx.True(storage && storage.IsHiddenInVicnity(), label + " is hidden from a player's inventory Vicinity");
	}

	override void Run(LF_TestContext ctx)
	{
		array<ResourceName> prefabs = {
			"{850859F356F5058E}Prefabs/Characters/Npcs/Npc_Default_01.et",
			"{E8989175335080B8}Prefabs/Characters/Quests/QuestGiver_Start.et",
			"{DE27E685D6FE93D7}Prefabs/Characters/Quests/QuestGiver_Clerk.et"
		};
		foreach (ResourceName prefab : prefabs)
		{
			IEntity npc = LF_Utils.SpawnEntityPrefab(prefab, "0 -50 0");
			ctx.True(npc != null, "the NPC spawns: " + prefab);
			if (!npc)
				continue;

			Check(ctx, npc, prefab);
			SCR_EntityHelper.DeleteEntityAndChildren(npc);
		}

		// Every dressed NPC placed in the DebugWorld town. A bare LF_NpcBaseEntity with no loadout
		// (TraderChain.layer's PoliceDutyNPC groups the tomato trader) has nothing to wear.
		m_aFound = new array<IEntity>();
		GetGame().GetWorld().QueryEntitiesBySphere("200 0 250", 400, InsertFound, FilterNpc);
		array<IEntity> placed = {};
		placed.Copy(m_aFound);
		ctx.True(placed.Count() > 0, "the town places NPCs");
		foreach (IEntity npc : placed)
			Check(ctx, npc, "placed NPC " + npc.GetName() + " at " + npc.GetOrigin());
	}

	protected bool FilterNpc(IEntity entity)
	{
		return entity && LF_NpcBaseEntity.Cast(entity) != null && entity.FindComponent(BaseLoadoutManagerComponent) != null;
	}
}
