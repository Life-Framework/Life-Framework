//------------------------------------------------------------------------------------------------
//! One crop of the catalogue (#2663, spec #809): the seed that plants it, how long it grows, the
//! window in which it must be watered, how long a ripe crop keeps, what it yields and the look of each
//! growth stage. Core ships one neutral example row (Configs/Farming/LF_Crops.conf); cabbage and
//! potato are the pack's (#810).
[BaseContainerProps()]
class LF_CropRow
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
			LF_Debug.Error(FEATURE, string.Format("crop catalogue has an unexpected root class: %1", CONFIG_PATH));

		return s_Catalogue;
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
