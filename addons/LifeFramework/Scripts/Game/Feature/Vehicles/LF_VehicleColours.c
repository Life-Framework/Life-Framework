// LF_VehicleColours.c — the colours a lot car can be bought in (epic #1826).
//
// A colour is a thin child of our locked car prefab that overrides only the body materials, so the
// colour of a bought car IS its spawn prefab (`m_sSpawnPrefab` in the frozen `vehicles` record). No
// new save field, no RplProp: the server spawns a replicated prefab and every client and late
// joiner loads the same paint. Prices, chop shop, impound and garage still key on the catalog
// prefab. Design: docs/research/car-colours.md (PR #1824).
//
// Rows are keyed by the catalog (vanilla) prefab, not by lot, so every lot sells the same paints
// and a pack fills the list through its overlay (Configs/Vehicles/LF_VehicleColours.conf is empty
// in core). Colour 0 is the colour sold today: its spawn prefab is the offer's own, its delta 0.
// A model with no row sells as before (one colour, index 0).

//------------------------------------------------------------------------------------------------
//! One colour of one model.
[BaseContainerProps()]
class LF_VehicleColour
{
	[Attribute("", UIWidgets.EditBox, "Localization key of the colour's name (a colour name key)")]
	string m_sNameKey;

	[Attribute("0", UIWidgets.Auto, "Swatch the sheet paints, 0xRRGGBB as a decimal number", params: "0 16777215")]
	int m_iSwatch;

	[Attribute("", UIWidgets.ResourcePickerThumbnail, "Locked child that spawns in this colour", "et")]
	ResourceName m_SpawnPrefab;

	[Attribute("0", UIWidgets.Auto, "Added to the model's price", params: "0 1000000")]
	int m_iPriceDelta;

	//------------------------------------------------------------------------------------------------
	//! Test and setup seam: a colour built in script has no attributes to fill.
	static LF_VehicleColour Make(string nameKey, int swatch, ResourceName spawnPrefab, int priceDelta)
	{
		LF_VehicleColour colour = new LF_VehicleColour();
		colour.m_sNameKey = nameKey;
		colour.m_iSwatch = swatch;
		colour.m_SpawnPrefab = spawnPrefab;
		colour.m_iPriceDelta = priceDelta;
		return colour;
	}
}

//------------------------------------------------------------------------------------------------
//! One model's colours, in the order the sheet lists them. The id is the catalog prefab's GUID.
[BaseContainerProps()]
class LF_VehicleColourSet : LF_ConfigRecord
{
	[Attribute("", UIWidgets.ResourcePickerThumbnail, "Vanilla catalog prefab the lot sells", "et")]
	ResourceName m_Prefab;

	[Attribute("", UIWidgets.Object, "The colours, first is the default and the colour sold today")]
	ref array<ref LF_VehicleColour> m_aColours;

	//------------------------------------------------------------------------------------------------
	static LF_VehicleColourSet Make(ResourceName prefab)
	{
		LF_VehicleColourSet row = new LF_VehicleColourSet();
		row.m_Prefab = prefab;
		row.m_aColours = {};
		return row;
	}
}

//------------------------------------------------------------------------------------------------
//! The colours config. Layers: the core file (empty), then each pack overlay of this class in load
//! order. There is no owner (config.json) layer and nothing streams to the client: the server
//! sends the sheet its colour names, swatches and deltas.
[BaseContainerProps(configRoot: true)]
class LF_VehicleColoursConfig
{
	static const ResourceName CONFIG_PATH = "{2D5759B91EDDC134}Configs/Vehicles/LF_VehicleColours.conf";
	static const string LIST_NAME = "vehicle-colours";

	[Attribute("", UIWidgets.Object, "Models that sell in colours. A model with no row sells as one colour")]
	ref array<ref LF_VehicleColourSet> m_aModels;

	protected static ref LF_VehicleColoursConfig s_Config;
	protected static bool s_bLoadAttempted;

	//------------------------------------------------------------------------------------------------
	//! The layered config, or null (logged once) when the core file does not resolve.
	static LF_VehicleColoursConfig Get()
	{
		if (s_bLoadAttempted)
			return s_Config;

		s_bLoadAttempted = true;
		Resource container = BaseContainerTools.LoadContainer(CONFIG_PATH);
		if (!container || !container.IsValid() || !container.GetResource())
		{
			LF_Debug.Error("Vehicle", string.Format("vehicle colours config does not resolve: %1", CONFIG_PATH));
			return null;
		}

		s_Config = LF_VehicleColoursConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(container.GetResource().ToBaseContainer()));
		if (!s_Config)
		{
			LF_Debug.Error("Vehicle", string.Format("vehicle colours config has an unexpected root class: %1", CONFIG_PATH));
			return null;
		}

		s_Config.ApplyLayers();
		return s_Config;
	}

	//------------------------------------------------------------------------------------------------
	//! Drops the cached instance so the next Get() loads it again.
	static void Reload()
	{
		s_Config = null;
		s_bLoadAttempted = false;
	}

	//------------------------------------------------------------------------------------------------
	//! Test seam: makes `config` what Get() returns. Reload() undoes it.
	static void Use(LF_VehicleColoursConfig config)
	{
		s_Config = config;
		s_bLoadAttempted = true;
	}

	//------------------------------------------------------------------------------------------------
	//! Merges each pack overlay of this class over this instance, in load order.
	void ApplyLayers()
	{
		if (!m_aModels)
			m_aModels = {};

		LF_VehicleColourList list = new LF_VehicleColourList(this);
		array<ref Resource> resources = {};
		array<ResourceName> names = {};
		LF_ConfigOverlay.LoadPackOverlays(CONFIG_PATH, LF_VehicleColoursConfig, LIST_NAME, resources, names);
		LF_ConfigClaims claims = new LF_ConfigClaims();
		foreach (int i, Resource resource : resources)
		{
			int changed = LF_ConfigOverlay.ApplyContainerList(list, resource.GetResource().ToBaseContainer(), "m_aModels", names[i], claims);
			LF_Debug.Info("Vehicle", string.Format("vehicle colours: pack overlay %1 changed %2 models", names[i], changed));
		}
	}

	//------------------------------------------------------------------------------------------------
	//! The colour row of a catalog prefab, by GUID. Null when the model sells in one colour.
	LF_VehicleColourSet SetFor(ResourceName catalogPrefab)
	{
		if (!m_aModels || catalogPrefab.IsEmpty())
			return null;

		foreach (LF_VehicleColourSet row : m_aModels)
		{
			if (row && row.m_aColours && row.m_aColours.Count() > 0 && LF_VehiclePriceModel.SameResource(row.m_Prefab, catalogPrefab))
				return row;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! How many colours a model sells in; 1 when it has no row (the one colour sold today).
	int CountFor(ResourceName catalogPrefab)
	{
		LF_VehicleColourSet row = SetFor(catalogPrefab);
		if (!row)
			return 1;

		return row.m_aColours.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! The colour at index of a model, or null for a model with no row or an index out of range.
	LF_VehicleColour ColourAt(ResourceName catalogPrefab, int colour)
	{
		LF_VehicleColourSet row = SetFor(catalogPrefab);
		if (!row || colour < 0 || colour >= row.m_aColours.Count())
			return null;

		return row.m_aColours[colour];
	}

	//------------------------------------------------------------------------------------------------
	//! The base colour's spawn prefab for a spawn prefab that is one of this config's colours,
	//! so a colour car is priced, chopped and described as its model. Any other prefab comes back as is.
	ResourceName BaseSpawnOf(ResourceName spawnPrefab)
	{
		if (!m_aModels || spawnPrefab.IsEmpty())
			return spawnPrefab;

		foreach (LF_VehicleColourSet row : m_aModels)
		{
			if (!row || !row.m_aColours || row.m_aColours.Count() == 0)
				continue;

			foreach (LF_VehicleColour colour : row.m_aColours)
			{
				if (colour && LF_VehiclePriceModel.SameResource(colour.m_SpawnPrefab, spawnPrefab))
				{
					ResourceName baseSpawn = row.m_aColours[0].m_SpawnPrefab;
					if (baseSpawn.IsEmpty())
						return spawnPrefab;

					return baseSpawn;
				}
			}
		}

		return spawnPrefab;
	}

	//------------------------------------------------------------------------------------------------
	//! The shipped config's `BaseSpawnOf`; the prefab itself when the config is missing.
	static ResourceName ResolveBase(ResourceName spawnPrefab)
	{
		LF_VehicleColoursConfig config = Get();
		if (!config)
			return spawnPrefab;

		return config.BaseSpawnOf(spawnPrefab);
	}

	//------------------------------------------------------------------------------------------------
	//! Where a respawn goes when the saved spawn prefab no longer loads (a pack dropped the colour):
	//! the model's base colour, else the locked child the prices config lists, else empty.
	ResourceName RespawnFallback(ResourceName catalogPrefab, ResourceName pricesSpawnPrefab)
	{
		LF_VehicleColour first = ColourAt(catalogPrefab, 0);
		if (first && !first.m_SpawnPrefab.IsEmpty())
			return first.m_SpawnPrefab;

		return pricesSpawnPrefab;
	}
}

//------------------------------------------------------------------------------------------------
//! The colour rows for the shared merge (LF_ConfigOverlay), keyed by the catalog prefab's GUID. A
//! pack's patch replaces a model's whole colour list; its replace and add take a whole row.
class LF_VehicleColourList : LF_ConfigList
{
	protected LF_VehicleColoursConfig m_Config;

	void LF_VehicleColourList(LF_VehicleColoursConfig config)
	{
		m_Config = config;
	}

	override string GetName()
	{
		return "vehicle-colours.models";
	}

	override int Count()
	{
		return m_Config.m_aModels.Count();
	}

	override LF_ConfigRecord Get(int index)
	{
		return m_Config.m_aModels[index];
	}

	override string IdOf(notnull LF_ConfigRecord record)
	{
		LF_VehicleColourSet row = LF_VehicleColourSet.Cast(record);
		if (row.m_Prefab.IsEmpty())
			return string.Empty;

		return LF_VehiclePriceModel.GuidOf(row.m_Prefab);
	}

	override string IdOfContainer(notnull BaseContainer container)
	{
		string prefab;
		container.Get("m_Prefab", prefab);
		if (prefab.IsEmpty())
			return string.Empty;

		return LF_VehiclePriceModel.GuidOf(prefab);
	}

	override string IdHint()
	{
		return "a catalog prefab with its GUID";
	}

	override LF_ConfigRecord NewRecord(string id)
	{
		return LF_VehicleColourSet.Make("{" + id + "}");
	}

	override void Set(int index, notnull LF_ConfigRecord record)
	{
		LF_VehicleColourSet row = LF_VehicleColourSet.Cast(record);
		if (index < m_Config.m_aModels.Count())
			m_Config.m_aModels.Set(index, row);
		else
			m_Config.m_aModels.Insert(row);
	}

	override void RemoveAt(int index)
	{
		m_Config.m_aModels.RemoveOrdered(index);
	}

	//------------------------------------------------------------------------------------------------
	override int PatchFromContainer(notnull LF_ConfigRecord record, notnull BaseContainer container)
	{
		LF_VehicleColourSet row = LF_VehicleColourSet.Cast(record);
		if (!container.IsVariableSetDirectly("m_aColours"))
			return 0;

		BaseContainerList entries = container.GetObjectArray("m_aColours");
		if (!entries)
			return 0;

		array<ref LF_VehicleColour> colours = {};
		for (int i = 0; i < entries.Count(); i++)
		{
			BaseContainer entry = entries.Get(i);
			if (!entry)
				continue;

			LF_VehicleColour colour = LF_VehicleColour.Cast(BaseContainerTools.CreateInstanceFromContainer(entry));
			if (colour)
				colours.Insert(colour);
		}

		row.m_aColours = colours;
		return 1;
	}

	//------------------------------------------------------------------------------------------------
	override string Incomplete(notnull LF_ConfigRecord record)
	{
		LF_VehicleColourSet row = LF_VehicleColourSet.Cast(record);
		if (row.m_Prefab.IsEmpty())
			return "a colour row has no catalog prefab";

		return string.Empty;
	}
}
