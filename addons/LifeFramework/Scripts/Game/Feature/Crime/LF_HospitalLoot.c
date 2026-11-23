// LF_HospitalLoot.c - the pharmacy cabinet's loot as chain goods (#4447, 0.3.0 Crime D3).
// Server side. A placed cabinet lists its own prefabs (LF_HospitalCabinetComponent.m_aLootPrefabs, wins
// when set); with none the pack's LF_HospitalLootConfig supplies the haul; with no pack conf the core
// default (LF_Hospital.FillDefaultLoot) stays. A config line may be narcotic: its prefab is registered
// with LF_VehicleSearchGoods as NARCOTICS, so a stop search lists it, and the units of a haul map to an
// amount class through LF_GoodsClass (A up to 2, B up to 10, C above) for the charge sheet. The law book
// (Police) decides the charge; this only makes and classes the goods.

//------------------------------------------------------------------------------------------------
//! One line of the cabinet haul: a prefab, how many, and whether it counts as narcotics.
[BaseContainerProps()]
class LF_HospitalLootLine
{
	[Attribute("", UIWidgets.ResourceNamePicker, "Item the cabinet yields", "et")]
	ResourceName m_Prefab;

	[Attribute("1", UIWidgets.EditBox, "How many of the item the cabinet yields", params: "1 20")]
	int m_iCount;

	[Attribute("0", UIWidgets.CheckBox, "The item is narcotics: a stop search lists it, and its units set the amount class")]
	bool m_bNarcotic;
}

//------------------------------------------------------------------------------------------------
//! The pack's cabinet haul. A pack lists a whole LF_HospitalLootConfig in its mission header overlays
//! (the last one wins). Core has no default lines here: with none, LF_Hospital.FillDefaultLoot is used.
[BaseContainerProps(configRoot: true)]
class LF_HospitalLootConfig
{
	[Attribute("", UIWidgets.Auto, "The haul, one line per item")]
	ref array<ref LF_HospitalLootLine> m_aLines;

	protected static ref LF_HospitalLootConfig s_Config;
	protected static bool s_bLoaded;

	//------------------------------------------------------------------------------------------------
	//! The live haul: the last pack overlay of this class, or null when no pack lists one.
	static LF_HospitalLootConfig Get()
	{
		if (s_bLoaded)
			return s_Config;

		s_bLoaded = true;
		array<ref Resource> resources = {};
		array<ResourceName> names = {};
		LF_ConfigOverlay.LoadPackOverlays(ResourceName.Empty, LF_HospitalLootConfig, "hospitalLoot", resources, names);
		foreach (int i, Resource resource : resources)
		{
			LF_HospitalLootConfig overlay = LF_HospitalLootConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(resource.GetResource().ToBaseContainer()));
			if (!overlay)
			{
				LF_Debug.Error("Hospital", "cabinet loot overlay has an unexpected root class: " + names[i]);
				continue;
			}

			s_Config = overlay;
			LF_Debug.Info("Hospital", "cabinet loot from " + names[i]);
		}

		return s_Config;
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_Config = null;
		s_bLoaded = false;
	}
}

//------------------------------------------------------------------------------------------------
class LF_HospitalLoot
{
	static const int MAX_LINE_COUNT = 20; // lf-fixed: safety cap on one config line, so a typo never floods a cabinet with items

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		LF_HospitalLootConfig.ResetStatics();
	}

	//------------------------------------------------------------------------------------------------
	//! Whether a pack lists at least one usable haul line.
	static bool HasConfig()
	{
		LF_HospitalLootConfig config = LF_HospitalLootConfig.Get();
		return config && config.m_aLines && !config.m_aLines.IsEmpty();
	}

	//------------------------------------------------------------------------------------------------
	//! The haul a cabinet yields: its own list when set, else the pack conf (each line repeated by its
	//! count), else the core default.
	//! \param placed The cabinet's own list; null or empty falls through.
	//! \param[out] loot Receives the prefabs, one entry per unit.
	static void Choose(array<ResourceName> placed, notnull array<ResourceName> loot)
	{
		loot.Clear();
		if (placed && placed.Count() > 0)
		{
			foreach (ResourceName prefab : placed)
				loot.Insert(prefab);
			return;
		}

		if (FromConfig(loot))
			return;

		LF_Hospital.FillDefaultLoot(loot);
	}

	//------------------------------------------------------------------------------------------------
	//! The pack conf haul, one entry per unit. False (and loot untouched) when no pack lists one.
	static bool FromConfig(notnull array<ResourceName> loot)
	{
		if (!HasConfig())
			return false;

		foreach (LF_HospitalLootLine line : LF_HospitalLootConfig.Get().m_aLines)
		{
			if (!line || line.m_Prefab.IsEmpty())
			{
				LF_Debug.Error("Hospital", "cabinet loot line skipped: empty prefab");
				continue;
			}

			int count = LF_Utils.ClampInt(line.m_iCount, 1, MAX_LINE_COUNT);
			for (int i = 0; i < count; i++)
				loot.Insert(line.m_Prefab);
		}

		return !loot.IsEmpty();
	}

	//------------------------------------------------------------------------------------------------
	//! Whether the pack conf marks this prefab as narcotics.
	static bool IsNarcotic(ResourceName prefab)
	{
		if (!HasConfig() || prefab.IsEmpty())
			return false;

		foreach (LF_HospitalLootLine line : LF_HospitalLootConfig.Get().m_aLines)
		{
			if (line && line.m_bNarcotic && LF_Utils.SamePrefab(line.m_Prefab, prefab))
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Narcotic units in a haul (one per entry).
	static int NarcoticUnits(notnull array<ResourceName> loot)
	{
		int units = 0;
		foreach (ResourceName prefab : loot)
		{
			if (IsNarcotic(prefab))
				units++;
		}

		return units;
	}

	//------------------------------------------------------------------------------------------------
	//! The amount class of the narcotic part of a haul, for the charge sheet.
	static LF_EGoodsClass HaulClass(notnull array<ResourceName> loot)
	{
		return LF_GoodsClass.FromUnits(NarcoticUnits(loot));
	}

	//------------------------------------------------------------------------------------------------
	//! Registers every narcotic line with the stop search, so a search lists the vanilla item by class
	//! (a vanilla prefab cannot carry the marker). Server only; run again after a world-end reset.
	static void RegisterGoods()
	{
		if (!HasConfig())
			return;

		foreach (LF_HospitalLootLine line : LF_HospitalLootConfig.Get().m_aLines)
		{
			if (line && line.m_bNarcotic && !line.m_Prefab.IsEmpty())
				LF_VehicleSearchGoods.Register(line.m_Prefab, LF_EVehicleSearchGoodsClass.NARCOTICS);
		}
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of the loot config cache (LF_StaticReset finds it by reflection).
class LF_HospitalLoot_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_HospitalLoot";
	}

	override void Apply()
	{
		LF_HospitalLoot.ResetStatics();
	}
}
