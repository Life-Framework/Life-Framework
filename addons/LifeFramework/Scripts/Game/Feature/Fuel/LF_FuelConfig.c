//------------------------------------------------------------------------------------------------
//! Fuel station stock table (#3327, framework #788). Loaded from Configs/Fuel/LF_Fuel.conf. The core ships it
//! off (`m_bEnabled` 0), so no server changes behaviour until it opts in. Prices are in hundredths of a cash
//! unit per litre (`LF_FuelStockRules.PRICE_SCALE`); price 0 is the kill switch. One table for every station
//! in this card; per-station rows come with the station component.
[BaseContainerProps(configRoot: true)]
class LF_FuelConfig
{
	static const string FEATURE = "Fuel";
	static const ResourceName CONFIG_PATH = "{E66F86EF400CD08A}Configs/Fuel/LF_Fuel.conf";

	[Attribute("0", UIWidgets.CheckBox, "Whether fuel stations sell from stock. Off refuses every sale")]
	bool m_bEnabled;

	[Attribute("200", UIWidgets.Auto, "Price of one litre in hundredths of a cash unit; 0 turns fuel sales off", params: "0 10000")]
	int m_iPricePerLitre;

	[Attribute("100", UIWidgets.Auto, "What a delivery pays per litre that landed, in hundredths of a cash unit", params: "0 10000")]
	int m_iDeliveryPayPerLitre;

	[Attribute("2000", UIWidgets.Auto, "Litres a station holds at most", params: "1 100000")]
	int m_iCapacity;

	[Attribute("500", UIWidgets.Auto, "Litres a station holds when it is first made", params: "0 100000")]
	int m_iStartLitres;

	[Attribute("200", UIWidgets.Auto, "Litres one delivery run carries", params: "1 100000")]
	int m_iLitresPerDelivery;

	[Attribute("1", UIWidgets.Auto, "A station holding fewer litres than this refuses to sell", params: "1 100000")]
	int m_iMinStockToSell;

	[Attribute("60", UIWidgets.Auto, "What the depot charges per litre loaded into the tanker, in hundredths of a cash unit (#3331)", params: "0 10000")]
	int m_iWholesalePerLitre;

	[Attribute("", UIWidgets.ResourcePickerThumbnail, "The tanker prefab the haul job hands out; empty until a pack names one", "et")]
	ResourceName m_TankerPrefab;

	[Attribute("", UIWidgets.Object, "Per-station capacity and starting litres by station id; a station with no row takes the table values")]
	ref array<ref LF_FuelStationRow> m_aStations;

	protected static ref LF_FuelConfig s_Config;
	protected static bool s_bLoadAttempted;

	//------------------------------------------------------------------------------------------------
	static LF_FuelConfig Make(bool enabled, int pricePerLitre, int deliveryPay, int capacity, int startLitres, int litresPerDelivery, int minStockToSell)
	{
		LF_FuelConfig config = new LF_FuelConfig();
		config.m_bEnabled = enabled;
		config.m_iPricePerLitre = pricePerLitre;
		config.m_iDeliveryPayPerLitre = deliveryPay;
		config.m_iCapacity = capacity;
		config.m_iStartLitres = startLitres;
		config.m_iLitresPerDelivery = litresPerDelivery;
		config.m_iMinStockToSell = minStockToSell;
		return config;
	}

	//------------------------------------------------------------------------------------------------
	//! The loaded config, or null (logged once) when the file does not resolve. An unusable table is
	//! switched off at load (`Sanitize`), so a bad file never sells fuel.
	static LF_FuelConfig Get()
	{
		if (s_bLoadAttempted)
			return s_Config;

		s_bLoadAttempted = true;
		Resource container = BaseContainerTools.LoadContainer(CONFIG_PATH);
		if (!container || !container.GetResource())
		{
			LF_Debug.Error(FEATURE, string.Format("fuel config does not resolve: %1", CONFIG_PATH));
			return null;
		}

		s_Config = LF_FuelConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(container.GetResource().ToBaseContainer()));
		if (!s_Config)
		{
			LF_Debug.Error(FEATURE, string.Format("fuel config has an unexpected root class: %1", CONFIG_PATH));
			return null;
		}

		s_Config.ApplyLayers();
		s_Config.Sanitize();
		return s_Config;
	}

	//------------------------------------------------------------------------------------------------
	//! Puts each pack overlay of this class (the mission header's m_aLF_ConfigOverlays, in load order) over
	//! this instance, which holds the core `.conf` (#3331). An overlay sets only the fields it writes; a
	//! field it leaves out keeps the core (or an earlier pack's) value. No pack: the core file alone.
	void ApplyLayers()
	{
		array<ref Resource> resources = {};
		array<ResourceName> names = {};
		LF_ConfigOverlay.LoadPackOverlays(CONFIG_PATH, LF_FuelConfig, "fuel", resources, names);
		foreach (int i, Resource resource : resources)
		{
			int changed = ApplyPackContainer(resource.GetResource().ToBaseContainer());
			LF_Debug.Info(FEATURE, string.Format("fuel: pack overlay %1 set %2 fields", names[i], changed));
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Applies the fields one overlay container writes directly. A station list replaces the earlier one
	//! whole. \return How many fields it set.
	int ApplyPackContainer(notnull BaseContainer root)
	{
		int changed;
		if (root.IsVariableSetDirectly("m_bEnabled") && root.Get("m_bEnabled", m_bEnabled))
			changed++;
		if (root.IsVariableSetDirectly("m_iPricePerLitre") && root.Get("m_iPricePerLitre", m_iPricePerLitre))
			changed++;
		if (root.IsVariableSetDirectly("m_iDeliveryPayPerLitre") && root.Get("m_iDeliveryPayPerLitre", m_iDeliveryPayPerLitre))
			changed++;
		if (root.IsVariableSetDirectly("m_iCapacity") && root.Get("m_iCapacity", m_iCapacity))
			changed++;
		if (root.IsVariableSetDirectly("m_iStartLitres") && root.Get("m_iStartLitres", m_iStartLitres))
			changed++;
		if (root.IsVariableSetDirectly("m_iLitresPerDelivery") && root.Get("m_iLitresPerDelivery", m_iLitresPerDelivery))
			changed++;
		if (root.IsVariableSetDirectly("m_iMinStockToSell") && root.Get("m_iMinStockToSell", m_iMinStockToSell))
			changed++;
		if (root.IsVariableSetDirectly("m_iWholesalePerLitre") && root.Get("m_iWholesalePerLitre", m_iWholesalePerLitre))
			changed++;
		if (root.IsVariableSetDirectly("m_TankerPrefab") && root.Get("m_TankerPrefab", m_TankerPrefab))
			changed++;

		if (root.IsVariableSetDirectly("m_aStations"))
		{
			m_aStations = new array<ref LF_FuelStationRow>();
			BaseContainerList rows = root.GetObjectArray("m_aStations");
			if (rows)
			{
				for (int i = 0; i < rows.Count(); i++)
				{
					LF_FuelStationRow row = LF_FuelStationRow.Cast(BaseContainerTools.CreateInstanceFromContainer(rows.Get(i)));
					if (row)
						m_aStations.Insert(row);
				}
			}

			changed++;
		}

		return changed;
	}

	//------------------------------------------------------------------------------------------------
	//! The row for this station id, or null (the station then takes the table values).
	LF_FuelStationRow FindStation(string id)
	{
		if (!m_aStations)
			return null;

		foreach (LF_FuelStationRow row : m_aStations)
		{
			if (row && row.m_sId == id)
				return row;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Drops the cached instance; the next Get loads the `.conf` again (world end, tests).
	static void Reload()
	{
		s_Config = null;
		s_bLoadAttempted = false;
	}

	//------------------------------------------------------------------------------------------------
	//! Tests: makes `config` the one Get returns until Reload.
	static void UseForTest(LF_FuelConfig config)
	{
		s_Config = config;
		s_bLoadAttempted = true;
	}

	//------------------------------------------------------------------------------------------------
	//! A new station at the configured capacity and starting litres.
	LF_FuelStock NewStation()
	{
		return LF_FuelStock.Make(m_iCapacity, m_iStartLitres);
	}

	//------------------------------------------------------------------------------------------------
	//! A new station by id: its own row's capacity and starting litres, else the table values.
	LF_FuelStock NewStationFor(string id)
	{
		LF_FuelStationRow row = FindStation(id);
		if (!row)
			return NewStation();

		return LF_FuelStock.Make(row.m_iCapacity, row.m_iStartLitres);
	}

	//------------------------------------------------------------------------------------------------
	//! \return Empty when the table is usable, else why not: a price or pay outside 0..MAX_PRICE, a capacity
	//! outside 1..MAX_LITRES, start litres or a delivery outside the capacity, or a minimum stock above it.
	string Problem()
	{
		int maxPrice = LF_FuelStockRules.MAX_PRICE;
		if (m_iPricePerLitre < 0 || m_iPricePerLitre > maxPrice)
			return string.Format("price %1 is outside 0 to %2", m_iPricePerLitre, maxPrice);
		if (m_iDeliveryPayPerLitre < 0 || m_iDeliveryPayPerLitre > maxPrice)
			return string.Format("delivery pay %1 is outside 0 to %2", m_iDeliveryPayPerLitre, maxPrice);
		if (m_iWholesalePerLitre < 0 || m_iWholesalePerLitre > maxPrice)
			return string.Format("wholesale price %1 is outside 0 to %2", m_iWholesalePerLitre, maxPrice);
		if (m_iCapacity < 1 || m_iCapacity > LF_FuelStockRules.MAX_LITRES)
			return string.Format("capacity %1 is outside 1 to %2", m_iCapacity, LF_FuelStockRules.MAX_LITRES);
		if (m_iStartLitres < 0 || m_iStartLitres > m_iCapacity)
			return string.Format("start litres %1 is outside 0 to the capacity %2", m_iStartLitres, m_iCapacity);
		if (m_iLitresPerDelivery < 1 || m_iLitresPerDelivery > m_iCapacity)
			return string.Format("litres per delivery %1 is outside 1 to the capacity %2", m_iLitresPerDelivery, m_iCapacity);
		if (m_iMinStockToSell < 1 || m_iMinStockToSell > m_iCapacity)
			return string.Format("minimum stock to sell %1 is outside 1 to the capacity %2", m_iMinStockToSell, m_iCapacity);
		if (m_aStations)
		{
			foreach (LF_FuelStationRow row : m_aStations)
			{
				if (!row)
					continue;
				if (row.m_sId.IsEmpty())
					return "a station row has no id";
				if (row.m_iCapacity < 1 || row.m_iCapacity > LF_FuelStockRules.MAX_LITRES)
					return string.Format("station %1 capacity %2 is outside 1 to %3", row.m_sId, row.m_iCapacity, LF_FuelStockRules.MAX_LITRES);
				if (row.m_iStartLitres < 0 || row.m_iStartLitres > row.m_iCapacity)
					return string.Format("station %1 start litres %2 is outside 0 to its capacity %3", row.m_sId, row.m_iStartLitres, row.m_iCapacity);
			}
		}

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Switches an unusable table off with `LF_Debug.Error("Fuel", ...)` naming the problem.
	//! \return true when the table was usable.
	bool Sanitize()
	{
		string problem = Problem();
		if (problem.IsEmpty())
			return true;

		LF_Debug.Error(FEATURE, string.Format("fuel config refused at load, fuel stock stays off: %1", problem));
		m_bEnabled = false;
		return false;
	}
}

//------------------------------------------------------------------------------------------------
//! One station's own capacity and starting litres, keyed by the station's id (#3331).
[BaseContainerProps()]
class LF_FuelStationRow
{
	[Attribute("", UIWidgets.EditBox, "Station id the station component names")]
	string m_sId;

	[Attribute("2000", UIWidgets.Auto, "Litres this station holds at most", params: "1 100000")]
	int m_iCapacity;

	[Attribute("500", UIWidgets.Auto, "Litres this station holds when it is first made", params: "0 100000")]
	int m_iStartLitres;
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_FuelConfig's statics.
class LF_FuelConfig_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_FuelConfig";
	}

	override void Apply()
	{
		LF_FuelConfig.Reload();
	}
}
