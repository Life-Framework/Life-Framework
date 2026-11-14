//------------------------------------------------------------------------------------------------
//! Sound slots (#323): the sound twin of prefab slots (LF_PrefabSlots). A prop or system that plays
//! a looped or one-shot sound asks for it by slot id instead of naming a sound project, so a pack
//! swaps the sound by overriding the slot. One core file (Configs/SoundSlots/LF_SoundSlots.conf)
//! lists each slot id and may give it a default sound project (.acp); a pack overlay of the same class
//! patches a slot's m_Project by id through the override model (LF_ConfigOverlay,
//! docs/config/override-model.md). A slot with no core default is filled by a pack or stays silent:
//! the town ambience (atmo_*) has no core sound since #323 batch 4; the flagship pack fills it.
//!
//! A pack project that does not load is rejected at first use with a named reason and the slot
//! falls back to the core default. A slot nobody can resolve logs LF_Debug.Error once and Get
//! returns empty: the caller plays nothing and that one feature stays silent, never a crash.
//!
//! Layers: core, then pack overlays. No owner (config.json) layer: client and server load the same
//! mission header. A slot is resolved once per world; a changed pack needs a restart.
[BaseContainerProps()]
class LF_SoundSlot : LF_ConfigRecord
{
	[Attribute("", UIWidgets.EditBox, "Stable slot id a prop or system asks for (atmo_pub_radio). Never rename one")]
	string m_sSlot;

	[Attribute("", UIWidgets.ResourcePickerThumbnail, "The sound project the slot plays. Empty in core: a pack fills it, or the slot is silent", "acp")]
	ResourceName m_Project;

	//------------------------------------------------------------------------------------------------
	static LF_SoundSlot Make(string slot, ResourceName project)
	{
		LF_SoundSlot record = new LF_SoundSlot();
		record.m_sSlot = slot;
		record.m_Project = project;
		return record;
	}
}

//------------------------------------------------------------------------------------------------
[BaseContainerProps(configRoot: true)]
class LF_SoundSlotsConfig
{
	static const ResourceName CONFIG_PATH = "{458C3EFC96FFEF15}Configs/SoundSlots/LF_SoundSlots.conf";

	//! The name log lines and the overlay loader use. Not an owner section: config.json has no slots.
	static const string LIST_NAME = "sounds";

	[Attribute("", UIWidgets.Object, "Sound slots. The core file sets every slot's default; a pack overlay patches m_Project by slot id")]
	ref array<ref LF_SoundSlot> m_aSlots;

	//------------------------------------------------------------------------------------------------
	//! The core file, not layered, or null (logged).
	static LF_SoundSlotsConfig LoadCore()
	{
		Resource container = BaseContainerTools.LoadContainer(CONFIG_PATH);
		if (!container || !container.IsValid() || !container.GetResource())
		{
			LF_Debug.Error(LF_SoundSlots.FEATURE, "sound slots config does not resolve, every slot is empty: " + CONFIG_PATH);
			return null;
		}

		LF_SoundSlotsConfig config = LF_SoundSlotsConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(container.GetResource().ToBaseContainer()));
		if (!config)
			LF_Debug.Error(LF_SoundSlots.FEATURE, "sound slots config has an unexpected root class: " + CONFIG_PATH);
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

		LF_SoundSlotList list = new LF_SoundSlotList(this);
		array<ref Resource> resources = {};
		array<ResourceName> names = {};
		LF_ConfigOverlay.LoadPackOverlays(CONFIG_PATH, LF_SoundSlotsConfig, LIST_NAME, resources, names);
		LF_ConfigClaims claims = new LF_ConfigClaims();
		foreach (int i, Resource resource : resources)
		{
			int changed = LF_ConfigOverlay.ApplyContainerList(list, resource.GetResource().ToBaseContainer(), "m_aSlots", names[i], claims);
			LF_Debug.Info(LF_SoundSlots.FEATURE, string.Format("pack overlay %1 changed %2 slots", names[i], changed));
		}
	}

	//------------------------------------------------------------------------------------------------
	LF_SoundSlot Find(string slot)
	{
		foreach (LF_SoundSlot record : m_aSlots)
		{
			if (record && record.m_sSlot == slot)
				return record;
		}

		return null;
	}
}

//------------------------------------------------------------------------------------------------
//! The slots list for the shared merge (LF_ConfigOverlay), keyed by slot id. A pack patches only m_Project.
class LF_SoundSlotList : LF_ConfigList
{
	protected LF_SoundSlotsConfig m_Config;

	void LF_SoundSlotList(LF_SoundSlotsConfig config)
	{
		m_Config = config;
	}

	override string GetName()
	{
		return "sounds.slots";
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
		return LF_SoundSlot.Cast(record).m_sSlot;
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
		return LF_SoundSlot.Make(id, ResourceName.Empty);
	}

	override void Set(int index, notnull LF_ConfigRecord record)
	{
		LF_SoundSlot slot = LF_SoundSlot.Cast(record);
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
		LF_SoundSlot slot = LF_SoundSlot.Cast(record);
		if (container.IsVariableSetDirectly("m_Project") && container.Get("m_Project", slot.m_Project))
			return 1;

		return 0;
	}

	override string Incomplete(notnull LF_ConfigRecord record)
	{
		if (LF_SoundSlot.Cast(record).m_Project.IsEmpty())
			return "a slot needs m_Project";

		return string.Empty;
	}
}

//------------------------------------------------------------------------------------------------
//! What props and systems call: LF_SoundSlots.Get("atmo_pub_radio"). Resolves every slot once, on first use.
class LF_SoundSlots
{
	static const string FEATURE = "SoundSlots";

	protected static ref map<string, ResourceName> s_mResolved;
	protected static ref map<string, bool> s_mWarned;

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_mResolved = null;
		s_mWarned = null;
	}

	//------------------------------------------------------------------------------------------------
	//! The sound project for a slot, or empty when none can be used (logged once per slot). The
	//! caller plays nothing on empty.
	static ResourceName Get(string slot)
	{
		if (!s_mResolved)
			Resolve();

		ResourceName project;
		if (s_mResolved.Find(slot, project))
			return project;

		if (!s_mWarned)
			s_mWarned = new map<string, bool>();
		if (!s_mWarned.Contains(slot))
		{
			s_mWarned.Set(slot, true);
			LF_Debug.Error(FEATURE, string.Format("slot '%1' has no usable sound project; whatever asks for it plays nothing", slot));
		}

		return ResourceName.Empty;
	}

	//------------------------------------------------------------------------------------------------
	//! Why `project` cannot fill a slot, or empty when it can.
	static string Unusable(ResourceName project)
	{
		if (project.IsEmpty())
			return "no sound project";

		Resource resource = Resource.Load(project);
		if (!resource || !resource.IsValid())
			return "does not load";

		return string.Empty;
	}

	//------------------------------------------------------------------------------------------------
	//! Loads the core file, layers the packs over it and checks every result. A rejected pack
	//! project falls back to the core default.
	static void Resolve()
	{
		s_mResolved = new map<string, ResourceName>();
		LF_SoundSlotsConfig core = LF_SoundSlotsConfig.LoadCore();
		if (!core)
			return;

		LF_SoundSlotsConfig layered = LF_SoundSlotsConfig.LoadCore();
		layered.ApplyLayers();

		foreach (LF_SoundSlot coreSlot : core.m_aSlots)
		{
			if (!coreSlot || coreSlot.m_sSlot.IsEmpty())
				continue;

			string slot = coreSlot.m_sSlot;
			ResourceName project = coreSlot.m_Project;
			bool fromPack;
			LF_SoundSlot packSlot = layered.Find(slot);
			if (!packSlot)
			{
				LF_Debug.Error(FEATURE, string.Format("slot '%1': a pack removed it; it keeps the core default %2", slot, coreSlot.m_Project));
			}
			else if (!LF_Utils.SamePrefab(packSlot.m_Project, coreSlot.m_Project))
			{
				string packWhy = Unusable(packSlot.m_Project);
				if (packWhy.IsEmpty())
				{
					project = packSlot.m_Project;
					fromPack = true;
					LF_Debug.Info(FEATURE, string.Format("slot '%1' -> %2 (pack)", slot, project));
				}
				else
				{
					LF_Debug.Error(FEATURE, string.Format("slot '%1': pack project %2 rejected, %3; it falls back to the core default %4", slot, packSlot.m_Project, packWhy, coreSlot.m_Project));
				}
			}

			if (!fromPack && coreSlot.m_Project.IsEmpty())
			{
				LF_Debug.Info(FEATURE, string.Format("slot '%1' has no core sound and no pack fills it; it plays nothing", slot));
				s_mResolved.Set(slot, ResourceName.Empty);
				continue;
			}

			string coreWhy;
			if (!fromPack)
				coreWhy = Unusable(coreSlot.m_Project);
			if (!coreWhy.IsEmpty())
			{
				LF_Debug.Error(FEATURE, string.Format("slot '%1': core default %2 is unusable, %3; the slot is empty", slot, coreSlot.m_Project, coreWhy));
				continue;
			}

			s_mResolved.Set(slot, project);
		}

		foreach (LF_SoundSlot extra : layered.m_aSlots)
		{
			if (extra && !core.Find(extra.m_sSlot))
				LF_Debug.Warn(FEATURE, string.Format("slot '%1' is not a core slot; nothing asks for it, ignored", extra.m_sSlot));
		}
	}
}
