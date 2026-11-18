//------------------------------------------------------------------------------------------------
//! Stall market tables (#2924, framework #786, #759). Loaded from Configs/Market/LF_Stalls.conf. The core
//! ships it off (`m_bEnabled` 0), so no server changes behaviour until it opts in. All money is whole cash
//! units. Market day is a window of the in-game week: day index `m_iMarketDay` between `m_iOpenHour` and
//! `m_iCloseHour` (0..24, open before close). `m_iMarketDay` 0 means every day; 1..6 is that day of the week
//! (`dayNumber % 7`). A pack overlay (the mission header's `m_aLF_ConfigOverlays`) patches the root fields it
//! sets (#3182), the way `LF_MarketConfig` does.
[BaseContainerProps(configRoot: true)]
class LF_StallConfig
{
	static const string FEATURE = "Market";
	static const ResourceName CONFIG_PATH = "{C8B9FD75014D9A62}Configs/Market/LF_Stalls.conf";

	//! Largest price or fee the tables accept: keeps every sum inside an int.
	static const int MAX_CASH = 10000000;	// lf-fixed: int overflow guard for money sums

	[Attribute("0", UIWidgets.CheckBox, "Whether stalls can be rented. Off refuses every rent")]
	bool m_bEnabled;

	[Attribute("5", UIWidgets.Auto, "Day of the in-game week the market runs, 1 to 6; 0 runs it every day", params: "0 6")]
	int m_iMarketDay;

	[Attribute("8", UIWidgets.Auto, "In-game hour the market opens, 0 to 23", params: "0 23")]
	int m_iOpenHour;

	[Attribute("18", UIWidgets.Auto, "In-game hour the market closes, 1 to 24, after the opening hour", params: "1 24")]
	int m_iCloseHour;

	[Attribute("50", UIWidgets.Auto, "Cash a holder pays to rent a pitch for one market day", params: "0 10000000")]
	int m_iPitchFee;

	[Attribute("6", UIWidgets.Auto, "Most items one stall lists at a time", params: "0 1000")]
	int m_iMaxListings;

	[Attribute("1", UIWidgets.Auto, "Lowest price a listing may ask", params: "0 10000000")]
	int m_iMinPrice;

	[Attribute("1000", UIWidgets.Auto, "Highest price a listing may ask", params: "0 10000000")]
	int m_iMaxPrice;

	[Attribute("5", UIWidgets.Auto, "Percent of a sale the market keeps; the seller gets the rest", params: "0 100")]
	int m_iCutPct;

	protected static ref LF_StallConfig s_Config;
	protected static bool s_bLoadAttempted;

	//------------------------------------------------------------------------------------------------
	static LF_StallConfig Make(bool enabled, int marketDay, int openHour, int closeHour, int pitchFee, int maxListings, int minPrice, int maxPrice, int cutPct)
	{
		LF_StallConfig config = new LF_StallConfig();
		config.m_bEnabled = enabled;
		config.m_iMarketDay = marketDay;
		config.m_iOpenHour = openHour;
		config.m_iCloseHour = closeHour;
		config.m_iPitchFee = pitchFee;
		config.m_iMaxListings = maxListings;
		config.m_iMinPrice = minPrice;
		config.m_iMaxPrice = maxPrice;
		config.m_iCutPct = cutPct;
		return config;
	}

	//------------------------------------------------------------------------------------------------
	//! The loaded config, or null (logged once) when the file does not resolve. An unusable table
	//! is switched off at load (`Sanitize`), so a bad file never opens a stall.
	static LF_StallConfig Get()
	{
		if (s_bLoadAttempted)
			return s_Config;

		s_bLoadAttempted = true;
		Resource container = BaseContainerTools.LoadContainer(CONFIG_PATH);
		if (!container || !container.GetResource())
		{
			LF_Debug.Error(FEATURE, string.Format("stall config does not resolve: %1", CONFIG_PATH));
			return null;
		}

		s_Config = LF_StallConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(container.GetResource().ToBaseContainer()));
		if (!s_Config)
		{
			LF_Debug.Error(FEATURE, string.Format("stall config has an unexpected root class: %1", CONFIG_PATH));
			return null;
		}

		s_Config.ApplyLayers();
		s_Config.Sanitize();
		return s_Config;
	}

	//------------------------------------------------------------------------------------------------
	//! Merges each pack overlay (the mission header's m_aLF_ConfigOverlays, in load order) over this
	//! instance, which holds the core `.conf`: every root field an overlay sets directly replaces the
	//! core value, a field it leaves alone stays. Sanitize runs after, so a bad merged table is refused
	//! like a core one.
	void ApplyLayers()
	{
		array<ref Resource> resources = {};
		array<ResourceName> names = {};
		LF_ConfigOverlay.LoadPackOverlays(CONFIG_PATH, LF_StallConfig, "stalls", resources, names);
		foreach (int i, Resource resource : resources)
		{
			int changed = ApplyPackContainer(resource.GetResource().ToBaseContainer(), names[i]);
			LF_Debug.Info(FEATURE, string.Format("stalls: pack overlay %1 changed %2 fields", names[i], changed));
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Applies one pack overlay's root fields. \return Fields changed.
	//! \param[in] root The overlay's root container.
	//! \param[in] source The overlay file, for log lines.
	int ApplyPackContainer(notnull BaseContainer root, string source)
	{
		int changed;
		bool enabled;
		if (root.IsVariableSetDirectly("m_bEnabled") && root.Get("m_bEnabled", enabled))
		{
			m_bEnabled = enabled;
			changed++;
		}

		int value;
		if (root.IsVariableSetDirectly("m_iMarketDay") && root.Get("m_iMarketDay", value))
		{
			m_iMarketDay = value;
			changed++;
		}

		if (root.IsVariableSetDirectly("m_iOpenHour") && root.Get("m_iOpenHour", value))
		{
			m_iOpenHour = value;
			changed++;
		}

		if (root.IsVariableSetDirectly("m_iCloseHour") && root.Get("m_iCloseHour", value))
		{
			m_iCloseHour = value;
			changed++;
		}

		if (root.IsVariableSetDirectly("m_iPitchFee") && root.Get("m_iPitchFee", value))
		{
			m_iPitchFee = value;
			changed++;
		}

		if (root.IsVariableSetDirectly("m_iMaxListings") && root.Get("m_iMaxListings", value))
		{
			m_iMaxListings = value;
			changed++;
		}

		if (root.IsVariableSetDirectly("m_iMinPrice") && root.Get("m_iMinPrice", value))
		{
			m_iMinPrice = value;
			changed++;
		}

		if (root.IsVariableSetDirectly("m_iMaxPrice") && root.Get("m_iMaxPrice", value))
		{
			m_iMaxPrice = value;
			changed++;
		}

		if (root.IsVariableSetDirectly("m_iCutPct") && root.Get("m_iCutPct", value))
		{
			m_iCutPct = value;
			changed++;
		}

		return changed;
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
	static void UseForTest(LF_StallConfig config)
	{
		s_Config = config;
		s_bLoadAttempted = true;
	}

	//------------------------------------------------------------------------------------------------
	//! \return Empty when the table is usable, else why not: a market day outside 0..6, hours that do not
	//! give an open window, a negative or oversized fee or price, a min price above the max, or a cut outside 0..100.
	string Problem()
	{
		if (m_iMarketDay < 0 || m_iMarketDay > 6)
			return string.Format("market day %1 is outside 0 to 6", m_iMarketDay);
		if (m_iOpenHour < 0 || m_iCloseHour > 24 || m_iOpenHour >= m_iCloseHour)
			return string.Format("market hours %1 to %2 are not an open window inside 0 to 24", m_iOpenHour, m_iCloseHour);
		if (m_iPitchFee < 0 || m_iPitchFee > MAX_CASH)
			return string.Format("pitch fee %1 is outside 0 to %2", m_iPitchFee, MAX_CASH);
		if (m_iMaxListings < 0)
			return string.Format("max listings %1 is negative", m_iMaxListings);
		if (m_iMinPrice < 0 || m_iMaxPrice > MAX_CASH)
			return string.Format("price range %1 to %2 is outside 0 to %3", m_iMinPrice, m_iMaxPrice, MAX_CASH);
		if (m_iMinPrice > m_iMaxPrice)
			return string.Format("min price %1 is above max price %2", m_iMinPrice, m_iMaxPrice);
		if (m_iCutPct < 0 || m_iCutPct > 100)
			return string.Format("cut %1 percent is outside 0 to 100", m_iCutPct);

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Switches an unusable table off with `LF_Debug.Error("Market", ...)` naming the problem.
	//! \return true when the table was usable.
	bool Sanitize()
	{
		string problem = Problem();
		if (problem.IsEmpty())
			return true;

		LF_Debug.Error(FEATURE, string.Format("stall config refused at load, stalls stay off: %1", problem));
		m_bEnabled = false;
		return false;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_StallConfig's statics.
class LF_StallConfig_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_StallConfig";
	}

	override void Apply()
	{
		LF_StallConfig.Reload();
	}
}
