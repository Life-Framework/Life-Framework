//------------------------------------------------------------------------------------------------
//! One crop of the catalogue (#2663, spec #809): the seed that plants it, how long it grows, the
//! window in which it must be watered, how long a ripe crop keeps, what it yields and the look of each
//! growth stage. Core ships one neutral example row (Configs/Farming/LF_Crops.conf); cabbage and
//! potato are the pack's (#810, #3176): a pack overlay adds or patches a crop by id.
[BaseContainerProps()]
class LF_CropRow : LF_ConfigRecord
{
	[Attribute("", UIWidgets.EditBox, "Stable crop id. Saved plants are keyed by it, so never rename one")]
	string m_sCropId;

	[Attribute("", UIWidgets.ResourcePickerThumbnail, "Seed item the player plants", "et")]
	ResourceName m_SeedPrefab;

	[Attribute("600", UIWidgets.Auto, "Seconds from planting to ripe", params: "1 604800")]
	int m_iGrowthSeconds;

	[Attribute("60", UIWidgets.Auto, "Earliest a watering counts, in seconds since the last one (or planting). Earlier is refused", params: "0 604800")]
	int m_iWaterMinSeconds;

	[Attribute("240", UIWidgets.Auto, "Latest a watering counts, in seconds since the last one. Past it the plant dies. 0 means the crop needs no water", params: "0 604800")]
	int m_iWaterMaxSeconds;

	[Attribute("1800", UIWidgets.Auto, "Seconds a ripe crop stays harvestable before it rots. 0 means it never rots", params: "0 604800")]
	int m_iLifetimeAfterRipe;

	[Attribute("", UIWidgets.ResourcePickerThumbnail, "Item a ripe harvest gives (#2664)", "et")]
	ResourceName m_HarvestPrefab;

	[Attribute("3", UIWidgets.Auto, "Items one harvest gives", params: "1 100")]
	int m_iYield;

	[Attribute("1", UIWidgets.CheckBox, "Legal to grow. The shape keeps the flag; no illicit crop ships in M8")]
	bool m_bLegal;

	[Attribute("", UIWidgets.ResourcePickerThumbnail, "Prefab of each growth stage, first to ripe. Two or more; the last is the ripe crop", "et")]
	ref array<ResourceName> m_aStagePrefabs;
}

//------------------------------------------------------------------------------------------------
//! The crop catalogue config: Configs/Farming/LF_Crops.conf. Server only.
[BaseContainerProps(configRoot: true)]
class LF_CropCatalogue
{
	static const ResourceName CONFIG_PATH = "{9B4C2E71D3A85F06}Configs/Farming/LF_Crops.conf";
	static const string FEATURE = "Farming";

	[Attribute("", UIWidgets.Object, "Crops a player can plant")]
	ref array<ref LF_CropRow> m_aCrops;

	protected static ref LF_CropCatalogue s_Catalogue;
	protected static bool s_bLoadAttempted;

	//------------------------------------------------------------------------------------------------
	//! The loaded catalogue, or null (logged once) when the file does not resolve.
	static LF_CropCatalogue Get()
	{
		if (s_bLoadAttempted)
			return s_Catalogue;

		s_bLoadAttempted = true;
		Resource container = BaseContainerTools.LoadContainer(CONFIG_PATH);
		if (!container || !container.GetResource())
		{
			LF_Debug.Error(FEATURE, string.Format("crop catalogue does not resolve, nothing can be planted: %1", CONFIG_PATH));
			return null;
		}

		s_Catalogue = LF_CropCatalogue.Cast(BaseContainerTools.CreateInstanceFromContainer(container.GetResource().ToBaseContainer()));
		if (!s_Catalogue)
		{
			LF_Debug.Error(FEATURE, string.Format("crop catalogue has an unexpected root class: %1", CONFIG_PATH));
			return null;
		}

		s_Catalogue.ApplyLayers();
		return s_Catalogue;
	}

	//------------------------------------------------------------------------------------------------
	//! Merges each pack overlay (the mission header's m_aLF_ConfigOverlays, in load order) over this
	//! instance, which holds the core `.conf`: the crops by crop id (add, replace, patch, remove;
	//! docs/config/override-model.md).
	void ApplyLayers()
	{
		array<ref Resource> resources = {};
		array<ResourceName> names = {};
		LF_ConfigOverlay.LoadPackOverlays(CONFIG_PATH, LF_CropCatalogue, "farming", resources, names);
		LF_ConfigClaims claims = new LF_ConfigClaims();
		foreach (int i, Resource resource : resources)
		{
			int changed = ApplyPackContainer(resource.GetResource().ToBaseContainer(), names[i], claims);
			LF_Debug.Info(FEATURE, string.Format("crops: pack overlay %1 changed %2 crops", names[i], changed));
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Applies one pack overlay's `m_aCrops` by crop id. \return Crops changed.
	//! \param[in] root The overlay's root container.
	//! \param[in] source The overlay file, for log lines.
	int ApplyPackContainer(notnull BaseContainer root, string source, LF_ConfigClaims claims = null)
	{
		if (!m_aCrops)
			m_aCrops = new array<ref LF_CropRow>();

		return LF_ConfigOverlay.ApplyContainerList(new LF_CropList(this), root, "m_aCrops", source, claims);
	}

	//------------------------------------------------------------------------------------------------
	//! Drops the cached catalogue; the next Get loads the `.conf` and its pack overlays again (world end, tests).
	static void Reload()
	{
		s_Catalogue = null;
		s_bLoadAttempted = false;
	}

	//------------------------------------------------------------------------------------------------
	//! The row with this crop id, or null.
	static LF_CropRow Find(string cropId)
	{
		LF_CropCatalogue catalogue = Get();
		if (!catalogue)
			return null;

		return catalogue.FindIn(cropId);
	}

	//------------------------------------------------------------------------------------------------
	//! The row with this crop id in this catalogue, or null.
	LF_CropRow FindIn(string cropId)
	{
		if (!m_aCrops || cropId.IsEmpty())
			return null;

		foreach (LF_CropRow row : m_aCrops)
		{
			if (row && row.m_sCropId == cropId)
				return row;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! The crop whose seed is this prefab, or null. The planting action finds the crop from the seed in hand.
	LF_CropRow FindBySeed(ResourceName seedPrefab)
	{
		if (!m_aCrops || seedPrefab.IsEmpty())
			return null;

		foreach (LF_CropRow row : m_aCrops)
		{
			if (row && LF_Utils.SamePrefab(row.m_SeedPrefab, seedPrefab))
				return row;
		}

		return null;
	}
}

//------------------------------------------------------------------------------------------------
//! The crop catalogue for the shared merge (LF_ConfigOverlay), keyed by crop id. A new or replaced
//! crop needs a seed and two or more stage prefabs, none of them empty: a crop with a blank stage
//! would plant, grow and draw nothing, so the row is dropped with an error instead.
class LF_CropList : LF_ConfigList
{
	protected LF_CropCatalogue m_Catalogue;

	void LF_CropList(LF_CropCatalogue catalogue)
	{
		m_Catalogue = catalogue;
		if (!m_Catalogue.m_aCrops)
			m_Catalogue.m_aCrops = new array<ref LF_CropRow>();
	}

	override string GetName()
	{
		return "farming.crops";
	}

	override int Count()
	{
		return m_Catalogue.m_aCrops.Count();
	}

	override LF_ConfigRecord Get(int index)
	{
		return m_Catalogue.m_aCrops[index];
	}

	override string IdOf(notnull LF_ConfigRecord record)
	{
		return LF_CropRow.Cast(record).m_sCropId;
	}

	override string IdOfContainer(notnull BaseContainer container)
	{
		string id;
		container.Get("m_sCropId", id);
		return id;
	}

	override string IdHint()
	{
		return "a crop id";
	}

	override LF_ConfigRecord NewRecord(string id)
	{
		LF_CropRow crop = new LF_CropRow();
		crop.m_sCropId = id;
		return crop;
	}

	override void Set(int index, notnull LF_ConfigRecord record)
	{
		LF_CropRow crop = LF_CropRow.Cast(record);
		if (index < m_Catalogue.m_aCrops.Count())
			m_Catalogue.m_aCrops.Set(index, crop);
		else
			m_Catalogue.m_aCrops.Insert(crop);
	}

	override void RemoveAt(int index)
	{
		m_Catalogue.m_aCrops.RemoveOrdered(index);
	}

	override string Incomplete(notnull LF_ConfigRecord record)
	{
		LF_CropRow crop = LF_CropRow.Cast(record);
		if (crop.m_sCropId.IsEmpty())
			return "the entry has no crop id";
		if (crop.m_SeedPrefab.IsEmpty())
			return "a crop needs a seed prefab";

		return StagesProblem(crop.m_aStagePrefabs);
	}

	//------------------------------------------------------------------------------------------------
	//! Why this stage list cannot stand (fewer than two, or one blank), or empty.
	static string StagesProblem(array<ResourceName> stages)
	{
		if (!stages || stages.Count() < 2)
			return "a crop needs two or more stage prefabs";

		foreach (int i, ResourceName stage : stages)
		{
			if (stage.IsEmpty())
				return string.Format("stage %1 has an empty prefab", i + 1);
		}

		return string.Empty;
	}

	override int PatchFromContainer(notnull LF_ConfigRecord record, notnull BaseContainer container)
	{
		LF_CropRow crop = LF_CropRow.Cast(record);
		int changed;
		ResourceName prefab;
		if (container.IsVariableSetDirectly("m_SeedPrefab") && container.Get("m_SeedPrefab", prefab) && !prefab.IsEmpty())
		{
			crop.m_SeedPrefab = prefab;
			changed++;
		}

		if (container.IsVariableSetDirectly("m_HarvestPrefab") && container.Get("m_HarvestPrefab", prefab) && !prefab.IsEmpty())
		{
			crop.m_HarvestPrefab = prefab;
			changed++;
		}

		int number;
		if (container.IsVariableSetDirectly("m_iGrowthSeconds") && container.Get("m_iGrowthSeconds", number) && number > 0)
		{
			crop.m_iGrowthSeconds = number;
			changed++;
		}

		if (container.IsVariableSetDirectly("m_iWaterMinSeconds") && container.Get("m_iWaterMinSeconds", number) && number >= 0)
		{
			crop.m_iWaterMinSeconds = number;
			changed++;
		}

		if (container.IsVariableSetDirectly("m_iWaterMaxSeconds") && container.Get("m_iWaterMaxSeconds", number) && number >= 0)
		{
			crop.m_iWaterMaxSeconds = number;
			changed++;
		}

		if (container.IsVariableSetDirectly("m_iLifetimeAfterRipe") && container.Get("m_iLifetimeAfterRipe", number) && number >= 0)
		{
			crop.m_iLifetimeAfterRipe = number;
			changed++;
		}

		if (container.IsVariableSetDirectly("m_iYield") && container.Get("m_iYield", number) && number >= 1)
		{
			crop.m_iYield = number;
			changed++;
		}

		bool legal;
		if (container.IsVariableSetDirectly("m_bLegal") && container.Get("m_bLegal", legal))
		{
			crop.m_bLegal = legal;
			changed++;
		}

		// The stage list is replaced whole, and only when it is usable.
		if (container.IsVariableSetDirectly("m_aStagePrefabs"))
		{
			array<ResourceName> stages = {};
			string problem = "the stage list could not be read";
			if (container.Get("m_aStagePrefabs", stages))
				problem = StagesProblem(stages);

			if (problem.IsEmpty())
			{
				crop.m_aStagePrefabs = new array<ResourceName>();
				crop.m_aStagePrefabs.Copy(stages);
				changed++;
			}
			else
			{
				LF_Debug.Error(LF_CropCatalogue.FEATURE, string.Format("farming.crops %1: pack stage list refused (%2); keeping the shipped stages", crop.m_sCropId, problem));
			}
		}

		return changed;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_CropCatalogue's statics (LF_StaticReset finds it by reflection).
class LF_CropCatalogue_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_CropCatalogue";
	}

	override void Apply()
	{
		LF_CropCatalogue.Reload();
	}
}
