//------------------------------------------------------------------------------------------------
//! Prefab slots (#964): a system that spawns or hands out an item asks for it by slot id instead of
//! naming a prefab, so a pack swaps the look by overriding the slot. One core file
//! (Configs/PrefabSlots/LF_PrefabSlots.conf) maps each slot id to its default prefab and the
//! components the system needs on it; a pack overlay of the same class patches a slot's prefab by
//! id through the override model (LF_ConfigOverlay, docs/config/override-model.md).
//!
//! The required components are read from the core file only, so a pack can change the look and
//! never loosen what the system needs. A pack prefab that does not load or lacks a required
//! component is rejected at load with a named reason and the slot falls back to the core default.
//! A slot nobody can resolve logs LF_Debug.Error and Get returns empty: that one feature degrades.
//!
//! Layers: core, then pack overlays. There is no owner (config.json) layer: client and server load
//! the same mission header, so both resolve the same prefab without streaming anything. A slot is
//! resolved once per world; a changed pack needs a restart.
[BaseContainerProps()]
class LF_PrefabSlot : LF_ConfigRecord
{
	[Attribute("", UIWidgets.EditBox, "Stable slot id a system asks for (cash, id_card). Never rename one")]
	string m_sSlot;

	[Attribute("", UIWidgets.ResourcePickerThumbnail, "The prefab the slot hands out", "et")]
	ResourceName m_Prefab;

	[Attribute("", UIWidgets.Auto, "Component classes the system needs on the prefab (a subclass counts). Read from the core file only; a pack cannot change them")]
	ref array<string> m_aRequires;

	//------------------------------------------------------------------------------------------------
	static LF_PrefabSlot Make(string slot, ResourceName prefab)
	{
		LF_PrefabSlot record = new LF_PrefabSlot();
		record.m_sSlot = slot;
		record.m_Prefab = prefab;
		record.m_aRequires = {};
		return record;
	}
}

//------------------------------------------------------------------------------------------------
[BaseContainerProps(configRoot: true)]
class LF_PrefabSlotsConfig
{
	static const ResourceName CONFIG_PATH = "{585C8B3D2EDA00E3}Configs/PrefabSlots/LF_PrefabSlots.conf";

	//! The name log lines and the overlay loader use. Not an owner section: config.json has no slots.
	static const string LIST_NAME = "prefabs";

	[Attribute("", UIWidgets.Object, "Prefab slots. The core file sets every slot's default and required components; a pack overlay patches m_Prefab by slot id")]
	ref array<ref LF_PrefabSlot> m_aSlots;

	//------------------------------------------------------------------------------------------------
	//! The core file, not layered, or null (logged).
	static LF_PrefabSlotsConfig LoadCore()
	{
		Resource container = BaseContainerTools.LoadContainer(CONFIG_PATH);
		if (!container || !container.IsValid() || !container.GetResource())
		{
			LF_Debug.Error(LF_PrefabSlots.FEATURE, "prefab slots config does not resolve, every slot is empty: " + CONFIG_PATH);
			return null;
		}

		LF_PrefabSlotsConfig config = LF_PrefabSlotsConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(container.GetResource().ToBaseContainer()));
		if (!config)
			LF_Debug.Error(LF_PrefabSlots.FEATURE, "prefab slots config has an unexpected root class: " + CONFIG_PATH);
		else if (!config.m_aSlots)
			config.m_aSlots = {};

		return config;
	}

	//------------------------------------------------------------------------------------------------
	//! Merges each pack overlay of this class over this instance, in load order.
	void ApplyLayers()
	{
		if (!m_aSlots)
			m_aSlots = {};

		LF_PrefabSlotList list = new LF_PrefabSlotList(this);
		array<ref Resource> resources = {};
		array<ResourceName> names = {};
		LF_ConfigOverlay.LoadPackOverlays(CONFIG_PATH, LF_PrefabSlotsConfig, LIST_NAME, resources, names);
		LF_ConfigClaims claims = new LF_ConfigClaims();
		foreach (int i, Resource resource : resources)
		{
			int changed = LF_ConfigOverlay.ApplyContainerList(list, resource.GetResource().ToBaseContainer(), "m_aSlots", names[i], claims);
			LF_Debug.Info(LF_PrefabSlots.FEATURE, string.Format("pack overlay %1 changed %2 slots", names[i], changed));
		}
	}

	//------------------------------------------------------------------------------------------------
	LF_PrefabSlot Find(string slot)
	{
		foreach (LF_PrefabSlot record : m_aSlots)
		{
			if (record && record.m_sSlot == slot)
				return record;
		}

		return null;
	}
}

//------------------------------------------------------------------------------------------------
//! The slots list for the shared merge (LF_ConfigOverlay), keyed by slot id. A pack patches only
//! m_Prefab; m_aRequires stays the core file's.
class LF_PrefabSlotList : LF_ConfigList
{
	protected LF_PrefabSlotsConfig m_Config;

	void LF_PrefabSlotList(LF_PrefabSlotsConfig config)
	{
		m_Config = config;
	}

	override string GetName()
	{
		return "prefabs.slots";
	}

	override int Count()
	{
		return m_Config.m_aSlots.Count();
	}

	override LF_ConfigRecord Get(int index)
	{
		return m_Config.m_aSlots[index];
	}

	override string IdOf(notnull LF_ConfigRecord record)
	{
		return LF_PrefabSlot.Cast(record).m_sSlot;
	}

	override string IdOfContainer(notnull BaseContainer container)
	{
		string id;
		container.Get("m_sSlot", id);
		return id;
	}

	override string IdHint()
	{
		return "a slot id";
	}

	override LF_ConfigRecord NewRecord(string id)
	{
		return LF_PrefabSlot.Make(id, ResourceName.Empty);
	}

	override void Set(int index, notnull LF_ConfigRecord record)
	{
		LF_PrefabSlot slot = LF_PrefabSlot.Cast(record);
		if (index < m_Config.m_aSlots.Count())
			m_Config.m_aSlots.Set(index, slot);
		else
			m_Config.m_aSlots.Insert(slot);
	}

	override void RemoveAt(int index)
	{
		m_Config.m_aSlots.RemoveOrdered(index);
	}

	override int PatchFromContainer(notnull LF_ConfigRecord record, notnull BaseContainer container)
	{
		LF_PrefabSlot slot = LF_PrefabSlot.Cast(record);
		if (container.IsVariableSetDirectly("m_Prefab") && container.Get("m_Prefab", slot.m_Prefab))
			return 1;

		return 0;
	}

	override string Incomplete(notnull LF_ConfigRecord record)
	{
		if (LF_PrefabSlot.Cast(record).m_Prefab.IsEmpty())
			return "a slot needs m_Prefab";

		return string.Empty;
	}
}

//------------------------------------------------------------------------------------------------
//! What systems call: LF_PrefabSlots.Get("cash"). Resolves every slot once, on first use.
class LF_PrefabSlots
{
	static const string FEATURE = "PrefabSlots";

	protected static ref map<string, ResourceName> s_mResolved;
	//! Slot -> the core file's default prefab (what an older save holds when a pack swapped the slot).
	protected static ref map<string, ResourceName> s_mCoreDefault;
	protected static ref map<string, bool> s_mWarned;

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_mResolved = null;
		s_mCoreDefault = null;
		s_mWarned = null;
	}

	//------------------------------------------------------------------------------------------------
	//! The prefab for a slot, or empty when no prefab for it can be used (logged once per slot).
	static ResourceName Get(string slot)
	{
		if (!s_mResolved)
			Resolve();

		ResourceName prefab;
		if (s_mResolved.Find(slot, prefab))
			return prefab;

		if (!s_mWarned)
			s_mWarned = new map<string, bool>();
		if (!s_mWarned.Contains(slot))
		{
			s_mWarned.Set(slot, true);
			LF_Debug.Error(FEATURE, string.Format("slot '%1' has no usable prefab; the feature that asks for it is off", slot));
		}

		return ResourceName.Empty;
	}

	//------------------------------------------------------------------------------------------------
	//! Older prefabs of a slot whose items may still exist (#1130): the core default when a pack
	//! swapped the slot, and every prefab the save alias table (#965) leads to the slot's prefab.
	//! A system that counts a slot's items counts these too, so a swap after a first save loses
	//! nothing. Only prefabs that load are listed. Nothing is added for an unswapped slot.
	static void LegacyPrefabs(string slot, notnull array<ResourceName> legacy)
	{
		ResourceName current = Get(slot);
		if (current.IsEmpty())
			return;

		ResourceName coreDefault;
		if (s_mCoreDefault && s_mCoreDefault.Find(slot, coreDefault) && !LF_Utils.SamePrefab(coreDefault, current) && LF_SaveIds.PrefabLoads(coreDefault))
			legacy.Insert(coreDefault);

		array<ResourceName> aliased = {};
		LF_SaveIds.PrefabSourcesOf(current, aliased);
		foreach (ResourceName source : aliased)
		{
			bool known;
			foreach (ResourceName have : legacy)
			{
				if (LF_Utils.SamePrefab(have, source))
					known = true;
			}

			if (!known && LF_SaveIds.PrefabLoads(source))
				legacy.Insert(source);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Why `prefab` cannot fill a slot that needs `requires`, or empty when it can.
	static string Unusable(ResourceName prefab, array<string> requires)
	{
		if (prefab.IsEmpty())
			return "no prefab";

		Resource resource = Resource.Load(prefab);
		if (!resource || !resource.IsValid())
			return "does not load";

		if (!requires)
			return string.Empty;

		foreach (string className : requires)
		{
			typename type = className.ToType();
			if (!type)
				return "requires unknown class " + className;

			if (!SCR_BaseContainerTools.FindComponentSource(resource, type))
				return "has no " + className;
		}

		return string.Empty;
	}

	//------------------------------------------------------------------------------------------------
	//! Loads the core file, keeps each slot's default and requirements, layers the packs over it and
	//! checks every result. A rejected pack prefab falls back to the core default.
	static void Resolve()
	{
		s_mResolved = new map<string, ResourceName>();
		s_mCoreDefault = new map<string, ResourceName>();
		LF_PrefabSlotsConfig core = LF_PrefabSlotsConfig.LoadCore();
		if (!core)
			return;

		LF_PrefabSlotsConfig layered = LF_PrefabSlotsConfig.LoadCore();
		layered.ApplyLayers();

		foreach (LF_PrefabSlot coreSlot : core.m_aSlots)
		{
			if (!coreSlot || coreSlot.m_sSlot.IsEmpty())
				continue;

			string slot = coreSlot.m_sSlot;
			ResourceName prefab = coreSlot.m_Prefab;
			s_mCoreDefault.Set(slot, coreSlot.m_Prefab);
			bool fromPack;
			LF_PrefabSlot packSlot = layered.Find(slot);
			if (!packSlot)
			{
				LF_Debug.Error(FEATURE, string.Format("slot '%1': a pack removed it; it keeps the core default %2", slot, coreSlot.m_Prefab));
			}
			else if (!LF_Utils.SamePrefab(packSlot.m_Prefab, coreSlot.m_Prefab))
			{
				string packWhy = Unusable(packSlot.m_Prefab, coreSlot.m_aRequires);
				if (packWhy.IsEmpty())
				{
					prefab = packSlot.m_Prefab;
					fromPack = true;
					LF_Debug.Info(FEATURE, string.Format("slot '%1' -> %2 (pack)", slot, prefab));
				}
				else
				{
					LF_Debug.Error(FEATURE, string.Format("slot '%1': pack prefab %2 rejected, %3; it falls back to the core default %4", slot, packSlot.m_Prefab, packWhy, coreSlot.m_Prefab));
				}
			}

			string coreWhy;
			if (!fromPack)
				coreWhy = Unusable(coreSlot.m_Prefab, coreSlot.m_aRequires);
			if (!coreWhy.IsEmpty())
			{
				LF_Debug.Error(FEATURE, string.Format("slot '%1': core default %2 is unusable, %3; the slot is empty", slot, coreSlot.m_Prefab, coreWhy));
				continue;
			}

			s_mResolved.Set(slot, prefab);
		}

		foreach (LF_PrefabSlot extra : layered.m_aSlots)
		{
			if (extra && !core.Find(extra.m_sSlot))
				LF_Debug.Warn(FEATURE, string.Format("slot '%1' is not a core slot; no system asks for it, ignored", extra.m_sSlot));
		}
	}
}
