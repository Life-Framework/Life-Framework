//------------------------------------------------------------------------------------------------
//! Puts an outfit preset (Configs/NpcOutfits/*.conf, LF_NpcOutfitPreset) on a character at run time
//! (#4863). The World Editor dresser writes a preset into a placed NPC's layer; this does the same job
//! for a character the server spawns: each slot's garment is spawned and inserted into the loadout, the
//! way the starter kit dresses a new player (LF_SpawnLogic.GiveRoleKit). Server only.
class LF_NpcOutfitDresser
{
	static const string FEATURE = "NpcOutfit";

	//------------------------------------------------------------------------------------------------
	//! The preset fields in dress order (a hat last, so it sits over the head already shown).
	static array<string> SlotFields()
	{
		return { "m_Torso", "m_Legs", "m_Feet", "m_Vest", "m_Backpack", "m_Head" };
	}

	//------------------------------------------------------------------------------------------------
	//! The garments a preset names, in dress order. Empty when the preset is not an LF_NpcOutfitPreset.
	static array<ResourceName> Garments(ResourceName preset)
	{
		array<ResourceName> garments = {};
		if (preset.IsEmpty())
			return garments;

		Resource resource = BaseContainerTools.LoadContainer(preset);
		if (!resource || !resource.IsValid() || !resource.GetResource())
		{
			LF_Debug.Error(FEATURE, "outfit preset does not load: " + preset);
			return garments;
		}

		BaseContainer conf = resource.GetResource().ToBaseContainer();
		if (!conf || conf.GetClassName() != "LF_NpcOutfitPreset")
		{
			LF_Debug.Error(FEATURE, "outfit is not an LF_NpcOutfitPreset: " + preset);
			return garments;
		}

		foreach (string field : SlotFields())
		{
			ResourceName garment;
			conf.Get(field, garment);
			if (!garment.IsEmpty())
				garments.Insert(garment);
		}

		return garments;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: dress `character` in `preset`. A piece that cannot be worn is dropped and logged; the rest
	//! still go on, and an empty preset leaves the character as it is.
	//! \return How many garments were put on.
	static int Dress(IEntity character, ResourceName preset)
	{
		if (!character || preset.IsEmpty() || !Replication.IsServer())
			return 0;

		InventoryStorageManagerComponent storage = LF_Component<InventoryStorageManagerComponent>.Find(character);
		if (!storage)
		{
			LF_Debug.Error(FEATURE, "outfit not worn: the character has no inventory storage manager");
			return 0;
		}

		int worn;
		foreach (ResourceName garment : Garments(preset))
		{
			Resource resource = Resource.Load(garment);
			if (!resource || !resource.IsValid())
			{
				LF_Debug.Error(FEATURE, "garment does not load: " + garment);
				continue;
			}

			EntitySpawnParams params();
			params.Transform[3] = character.GetOrigin();
			IEntity item = GetGame().SpawnEntityPrefabEx(garment, false, null, params);
			if (!item)
			{
				LF_Debug.Error(FEATURE, "garment did not spawn: " + garment);
				continue;
			}

			if (!storage.TryInsertItem(item, EStoragePurpose.PURPOSE_LOADOUT_PROXY))
			{
				LF_Debug.Warn(FEATURE, "garment did not go on: " + garment);
				SCR_EntityHelper.DeleteEntityAndChildren(item);
				continue;
			}

			worn++;
		}

		return worn;
	}

	//------------------------------------------------------------------------------------------------
	//! Is a garment of this prefab worn: a child of the character that carries a loadout cloth component.
	static bool IsWearing(IEntity character, ResourceName garment)
	{
		if (!character || garment.IsEmpty())
			return false;

		IEntity child = character.GetChildren();
		while (child)
		{
			if (child.FindComponent(BaseLoadoutClothComponent))
			{
				EntityPrefabData data = child.GetPrefabData();
				if (data && LF_Utils.SamePrefab(data.GetPrefabName(), garment))
					return true;
			}

			child = child.GetSibling();
		}

		return false;
	}
}
