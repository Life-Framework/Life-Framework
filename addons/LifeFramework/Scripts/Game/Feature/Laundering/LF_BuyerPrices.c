//------------------------------------------------------------------------------------------------
//! One line of the illicit buyer's price list (#2920): the price he pays for one unit of a chain good.
[BaseContainerProps()]
class LF_BuyerPriceRow
{
	[Attribute(ResourceName.Empty, UIWidgets.ResourcePickerThumbnail, "Chain good the buyer takes (an output of a chain step flagged m_bGoods)", "et")]
	ResourceName m_Prefab;

	[Attribute("0", UIWidgets.EditBox, "Price in cash value paid for one unit (at least 1)")]
	int m_iPrice;
}

//------------------------------------------------------------------------------------------------
//! The illicit buyer's price list and sale limits (#2920, M7 beat 4 of #1025). Loaded from
//! Configs/Crime/LF_BuyerPrices.conf; the 1989 pack overrides the file with its own prices (B4). A bad
//! list (an empty or repeated prefab, a price below 1) refuses the whole config: the buyer then buys
//! nothing, and every refusal is logged.
[BaseContainerProps(configRoot: true)]
class LF_BuyerPrices
{
	static const ResourceName CONFIG_PATH = "{6B3D1A8F20C94E57}Configs/Crime/LF_BuyerPrices.conf";

	[Attribute("1000", UIWidgets.EditBox, "Fastest one character's sales repeat, in milliseconds; sooner is refused")]
	int m_iMinActionMs;

	[Attribute("4", UIWidgets.EditBox, "Metres from the buyer a character must stand")]
	float m_fReach;

	[Attribute("", UIWidgets.Object, "What the buyer pays, per chain good")]
	ref array<ref LF_BuyerPriceRow> m_aPrices;

	protected static ref LF_BuyerPrices s_Config;
	protected static bool s_bLoadAttempted;

	//------------------------------------------------------------------------------------------------
	//! The loaded config, or null when it is missing or refused (logged).
	static LF_BuyerPrices Get()
	{
		if (s_bLoadAttempted)
			return s_Config;

		s_bLoadAttempted = true;
		Resource container = BaseContainerTools.LoadContainer(CONFIG_PATH);
		if (!container || !container.GetResource())
		{
			LF_Debug.Error("Laundering", string.Format("buyer price config does not resolve: %1", CONFIG_PATH));
			return null;
		}

		LF_BuyerPrices loaded = LF_BuyerPrices.Cast(BaseContainerTools.CreateInstanceFromContainer(container.GetResource().ToBaseContainer()));
		if (!loaded)
		{
			LF_Debug.Error("Laundering", string.Format("buyer price config has an unexpected root class: %1", CONFIG_PATH));
			return null;
		}

		loaded.ApplyLayers();
		Install(loaded);
		return s_Config;
	}

	//------------------------------------------------------------------------------------------------
	//! Puts each pack overlay of this class (the mission header's m_aLF_ConfigOverlays, in load order) over
	//! this instance, which holds the core `.conf` (#2922). The reach and repeat gap an overlay writes
	//! replace the core's; each price row of an overlay replaces the row of the same prefab or adds a new one.
	void ApplyLayers()
	{
		array<ref Resource> resources = {};
		array<ResourceName> names = {};
		LF_ConfigOverlay.LoadPackOverlays(CONFIG_PATH, LF_BuyerPrices, "buyer prices", resources, names);
		foreach (int i, Resource resource : resources)
		{
			int changed = ApplyPackContainer(resource.GetResource().ToBaseContainer());
			LF_Debug.Info("Laundering", string.Format("buyer prices: pack overlay %1 changed %2 entries", names[i], changed));
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Applies one overlay container. \return Fields and price rows changed.
	int ApplyPackContainer(notnull BaseContainer root)
	{
		int changed;
		if (root.IsVariableSetDirectly("m_iMinActionMs") && root.Get("m_iMinActionMs", m_iMinActionMs))
			changed++;
		if (root.IsVariableSetDirectly("m_fReach") && root.Get("m_fReach", m_fReach))
			changed++;

		if (!m_aPrices)
			m_aPrices = new array<ref LF_BuyerPriceRow>();

		BaseContainerList rows = root.GetObjectArray("m_aPrices");
		if (!rows)
			return changed;

		for (int i = 0; i < rows.Count(); i++)
		{
			BaseContainer entry = rows.Get(i);
			if (!entry)
				continue;

			LF_BuyerPriceRow row = LF_BuyerPriceRow.Cast(BaseContainerTools.CreateInstanceFromContainer(entry));
			if (!row)
				continue;

			int at = -1;
			foreach (int j, LF_BuyerPriceRow existing : m_aPrices)
			{
				if (existing && LF_Utils.SamePrefab(existing.m_Prefab, row.m_Prefab))
				{
					at = j;
					break;
				}
			}

			if (at >= 0)
				m_aPrices.Set(at, row);
			else
				m_aPrices.Insert(row);

			changed++;
		}

		return changed;
	}

	//------------------------------------------------------------------------------------------------
	//! Take `config` as the loaded config (Get calls it; tests call it with a built one). A config that
	//! fails Validate is dropped, so the buyer buys nothing.
	static void Install(LF_BuyerPrices config)
	{
		s_bLoadAttempted = true;
		s_Config = null;
		if (Validate(config))
			s_Config = config;
	}

	//------------------------------------------------------------------------------------------------
	//! A usable list: at least one row, every prefab set and unique, every price at least 1, a
	//! non-negative repeat gap and a positive reach. Logs the first fault and returns false.
	static bool Validate(LF_BuyerPrices config)
	{
		if (!config || !config.m_aPrices || config.m_aPrices.IsEmpty())
		{
			LF_Debug.Error("Laundering", "buyer price config refused: the price list is empty");
			return false;
		}

		if (config.m_iMinActionMs < 0 || config.m_fReach <= 0)
		{
			LF_Debug.Error("Laundering", string.Format("buyer price config refused: m_iMinActionMs %1 or m_fReach %2 is out of range", config.m_iMinActionMs, config.m_fReach));
			return false;
		}

		foreach (int i, LF_BuyerPriceRow row : config.m_aPrices)
		{
			if (!row || row.m_Prefab.IsEmpty() || row.m_iPrice < 1)
			{
				LF_Debug.Error("Laundering", string.Format("buyer price config refused: row %1 has no prefab or a price below 1", i));
				return false;
			}

			for (int j = 0; j < i; j++)
			{
				LF_BuyerPriceRow earlier = config.m_aPrices[j];
				if (earlier && LF_Utils.SamePrefab(earlier.m_Prefab, row.m_Prefab))
				{
					LF_Debug.Error("Laundering", string.Format("buyer price config refused: rows %1 and %2 price the same prefab %3", j, i, row.m_Prefab));
					return false;
				}
			}
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The price of one unit of `prefab`, or 0 when the buyer does not name it (or there is no config).
	static int PriceOf(ResourceName prefab)
	{
		LF_BuyerPrices config = Get();
		if (!config || prefab.IsEmpty())
			return 0;

		foreach (LF_BuyerPriceRow row : config.m_aPrices)
		{
			if (row && LF_Utils.SamePrefab(row.m_Prefab, prefab))
				return row.m_iPrice;
		}

		return 0;
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_Config = null;
		s_bLoadAttempted = false;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_BuyerPrices's statics (LF_StaticReset finds it by reflection).
class LF_BuyerPrices_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_BuyerPrices";
	}

	override void Apply()
	{
		LF_BuyerPrices.ResetStatics();
	}
}
