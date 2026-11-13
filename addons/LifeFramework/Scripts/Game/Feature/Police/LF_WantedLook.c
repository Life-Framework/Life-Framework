//------------------------------------------------------------------------------------------------
//! What a suspect looked like at the crime, for the portrait on the wanted board's face
//! (docs/architecture/visual-boards-spike.md, issue 4). Captured on the server when the report is
//! made: the character prefab and what each slot of its EquipedLoadoutStorageComponent held, as
//! resource GUIDs (16 hex digits, no path): "CHARGUID|slot=ITEMGUID|slot=ITEMGUID". A GUID is
//! the engine's own id for a prefab, so the wire never carries a path. The client spawns that
//! character in the preview world and dresses it the same way (LF_WantedPortrait).
class LF_WantedLook
{
	static const string PART = "|";
	static const string SLOT = "=";
	//! "{" + 16 hex digits + "}".
	static const int GUID_LENGTH = 16;	// lf-fixed: the engine's GUID format

	//------------------------------------------------------------------------------------------------
	//! The 16 hex digits of a "{GUID}path" resource name; empty when it has none.
	static string GuidOf(ResourceName name)
	{
		string text = name;
		if (text.Length() < GUID_LENGTH + 2 || !text.StartsWith("{") || text.Substring(GUID_LENGTH + 1, 1) != "}")
			return string.Empty;

		return text.Substring(1, GUID_LENGTH);
	}

	//------------------------------------------------------------------------------------------------
	//! A GUID back into a resource name the engine loads ("{GUID}").
	static ResourceName ResourceOf(string guid)
	{
		if (guid.Length() != GUID_LENGTH)
			return ResourceName.Empty;

		return "{" + guid + "}";
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the suspect's look now; empty for a missing body or one with no prefab.
	static string Capture(IEntity suspect)
	{
		if (!suspect || !suspect.GetPrefabData())
			return string.Empty;

		string look = GuidOf(suspect.GetPrefabData().GetPrefabName());
		if (look.IsEmpty())
			return string.Empty;

		EquipedLoadoutStorageComponent storage = EquipedLoadoutStorageComponent.Cast(suspect.FindComponent(EquipedLoadoutStorageComponent));
		if (!storage)
			return look;

		for (int slot = 0; slot < storage.GetSlotsCount(); slot++)
		{
			IEntity item = storage.Get(slot);
			if (!item || !item.GetPrefabData())
				continue;

			string guid = GuidOf(item.GetPrefabData().GetPrefabName());
			if (!guid.IsEmpty())
				look += PART + slot.ToString() + SLOT + guid;
		}

		return look;
	}

	//------------------------------------------------------------------------------------------------
	//! Client: a look back into the character prefab and the slot -> item prefabs.
	//! \return False when there is no character to draw.
	static bool Parse(string look, out ResourceName character, notnull array<int> outSlots, notnull array<ResourceName> outItems)
	{
		outSlots.Clear();
		outItems.Clear();
		array<string> parts = {};
		look.Split(PART, parts, true);
		if (parts.IsEmpty())
			return false;

		character = ResourceOf(parts[0]);
		if (character.IsEmpty())
			return false;

		for (int i = 1; i < parts.Count(); i++)
		{
			int at = parts[i].IndexOf(SLOT);
			if (at <= 0)
				continue;

			ResourceName item = ResourceOf(parts[i].Substring(at + 1, parts[i].Length() - at - 1));
			string slotText = parts[i].Substring(0, at);
			int slot = slotText.ToInt();
			if (item.IsEmpty() || slot < 0 || slot.ToString() != slotText)
				continue;

			outSlots.Insert(slot);
			outItems.Insert(item);
		}

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Client: one poster's portrait. A dressed dummy of the suspect in the preview world, drawn by the
//! poster's ItemPreviewWidget as a head-and-shoulders mugshot: the widget is three times the photo
//! frame and the frame clips it, because preview attributes made in script drew a black frame in
//! the spike. Every portrait is framed the same way, named or not.
class LF_WantedPortrait : Managed
{
	static const ResourceName PREVIEW_MANAGER = "{9F18C476AB860F3B}Prefabs/World/Game/ItemPreviewManager.et";

	protected IEntity m_Dummy;

	//------------------------------------------------------------------------------------------------
	//! Draw `look` in `photo`. \return Items dressed, or -1 when no picture could be made (the
	//! poster keeps its empty photo frame).
	int Show(ItemPreviewWidget photo, string look)
	{
		Clear();
		ResourceName character;
		array<int> slots = {};
		array<ResourceName> items = {};
		if (!photo || look.IsEmpty() || !LF_WantedLook.Parse(look, character, slots, items))
			return -1;

		ItemPreviewManagerEntity manager = Manager();
		if (!manager)
			return -1;

		IEntity shared = manager.ResolvePreviewEntityForPrefab(character);
		Resource characterResource = Resource.Load(character);
		if (!shared || !characterResource || !characterResource.IsValid())
		{
			LF_Debug.Error(LF_WantedPosters.FEATURE, "portrait: no preview entity for " + character);
			return -1;
		}

		// The vanilla arsenal-loadout preview (SCR_LoadoutPreviewComponent) dresses a local entity in
		// the preview world the same way.
		m_Dummy = GetGame().SpawnEntityPrefabLocal(characterResource, shared.GetWorld());
		if (!m_Dummy)
		{
			LF_Debug.Warn(LF_WantedPosters.FEATURE, "portrait: the dummy did not spawn; the plain character is drawn");
			manager.SetPreviewItemFromPrefab(photo, character, null, true);
			return 0;
		}

		int dressed;
		EquipedLoadoutStorageComponent storage = EquipedLoadoutStorageComponent.Cast(m_Dummy.FindComponent(EquipedLoadoutStorageComponent));
		for (int i = 0; storage && i < items.Count(); i++)
		{
			if (slots[i] >= storage.GetSlotsCount())
				continue;

			InventoryStorageSlot slot = storage.GetSlot(slots[i]);
			Resource itemResource = Resource.Load(items[i]);
			if (!slot || !itemResource || !itemResource.IsValid())
				continue;

			IEntity old = slot.GetAttachedEntity();
			if (old)
				SCR_EntityHelper.DeleteEntityAndChildren(old);

			IEntity item = GetGame().SpawnEntityPrefabLocal(itemResource, m_Dummy.GetWorld());
			if (!item)
				continue;

			slot.AttachEntity(item);
			dressed++;
		}

		manager.SetPreviewItem(photo, m_Dummy, null, true);
		return dressed;
	}

	//------------------------------------------------------------------------------------------------
	protected static ItemPreviewManagerEntity Manager()
	{
		ChimeraWorld world = ChimeraWorld.CastFrom(GetGame().GetWorld());
		if (!world)
			return null;

		ItemPreviewManagerEntity manager = world.GetItemPreviewManager();
		if (manager)
			return manager;

		Resource resource = Resource.Load(PREVIEW_MANAGER);
		if (resource && resource.IsValid())
			GetGame().SpawnEntityPrefabLocal(resource, world);

		manager = world.GetItemPreviewManager();
		if (!manager)
			LF_Debug.Error(LF_WantedPosters.FEATURE, "portrait: no ItemPreviewManagerEntity; posters show an empty photo frame");

		return manager;
	}

	//------------------------------------------------------------------------------------------------
	//! Delete the dummy. The frozen face keeps the picture it last drew.
	void Clear()
	{
		if (m_Dummy && !m_Dummy.IsDeleted())
			SCR_EntityHelper.DeleteEntityAndChildren(m_Dummy);

		m_Dummy = null;
	}
}
