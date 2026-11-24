//------------------------------------------------------------------------------------------------
//! One kind of export order a book can open (#4410): which goods, how many, the wholesale price per
//! unit the pay is figured from (the market row's own price when the template leaves it at 0, #4844),
//! the order bonus on top, and how many game minutes the hauler has.
[BaseContainerProps()]
class LF_ExportOrderTemplate
{
	[Attribute("", UIWidgets.EditBox, "Export order: a stable id for this kind of order")]
	string m_sId;

	[Attribute("", UIWidgets.ResourcePickerThumbnail, "Export order: the goods the crane takes", params: "et")]
	ResourceName m_Goods;

	[Attribute("", UIWidgets.EditBox, "Export order: the goods' name on the board (a #key or plain US English text)")]
	string m_sGoodsName;

	[Attribute("", UIWidgets.EditBox, "Export order: where the goods go, as the board names it (a #key or plain US English text)")]
	string m_sDestination;

	[Attribute("20", UIWidgets.Auto, "Export order: how many units make the order", params: "1 1000")]
	int m_iCount;

	[Attribute("0", UIWidgets.Auto, "Export order: the wholesale price of one unit; the order pays the premium over it. 0 reads the market row of the goods (its base price / lot size)", params: "0 100000")]
	int m_iWholesale;

	[Attribute("0", UIWidgets.Auto, "Export order: cash paid on top of wholesale plus the premium, once per order handed in", params: "0 100000")]
	int m_iBonus;

	[Attribute("180", UIWidgets.Auto, "Export order: game minutes from opening to the deadline", params: "10 2880")]
	int m_iGameMinutes;
}

//------------------------------------------------------------------------------------------------
//! Export order numbers (#4410). The core ships no orders (the goods are pack data); a pack overlay of
//! this class, listed in its mission header, sets the rule numbers and its order kinds.
[BaseContainerProps(configRoot: true)]
class LF_ExportOrderConfig
{
	//! No core file: the overlays are matched by class, so there is no core path to name.
	static const ResourceName CONFIG_PATH = "";
	//! The name the overlay log lines carry. There is no owner config.json section: a server tunes orders through a pack overlay.
	static const string LOG_LABEL = "exportOrders";

	static const int DEFAULT_OPEN_SLOTS = 3;	// lf-fixed: fallback only, the card's rule
	static const int DEFAULT_REFILL_MINUTES = 30;	// lf-fixed: fallback only, the card's rule
	static const int DEFAULT_MIN_LEVEL = 10;	// lf-fixed: fallback only, the card's rule
	static const int DEFAULT_PREMIUM_PCT = 15;	// lf-fixed: fallback only, the card's rule
	static const int DEFAULT_LATE_STANDING = -10;	// lf-fixed: fallback only
	static const int DEFAULT_XP = 120;	// lf-fixed: fallback only

	[Attribute("3", UIWidgets.Auto, "Export orders: how many orders are open server-wide at once", params: "1 12")]
	int m_iOpenSlots;

	[Attribute("30", UIWidgets.Auto, "Export orders: game minutes between two openings while a slot is free", params: "1 1440")]
	int m_iRefillMinutes;

	[Attribute("10", UIWidgets.Auto, "Export orders: the job level a hauler needs to take one", params: "0 100")]
	int m_iMinLevel;

	[Attribute("DELIVERY", UIWidgets.EditBox, "Export orders: the job track whose level gates the board and earns the XP")]
	string m_sJob;

	[Attribute("15", UIWidgets.Auto, "Export orders: percent paid over wholesale", params: "0 200")]
	int m_iPremiumPct;

	[Attribute("120", UIWidgets.Auto, "Export orders: job XP for an order handed in", params: "0 100000")]
	int m_iXp;

	[Attribute("TOWN", UIWidgets.EditBox, "Export orders: the standing faction a late order costs")]
	string m_sStandingFaction;

	[Attribute("-10", UIWidgets.Auto, "Export orders: standing change for a taken order that misses its deadline (negative)", params: "-1000 0")]
	int m_iLateStanding;

	[Attribute(desc: "Export orders: the kinds of order the book opens, in turn")]
	ref array<ref LF_ExportOrderTemplate> m_aTemplates;

	protected static ref LF_ExportOrderConfig s_Config;
	protected static bool s_bLoadAttempted;

	//------------------------------------------------------------------------------------------------
	static LF_ExportOrderConfig CreateDefault()
	{
		LF_ExportOrderConfig config = new LF_ExportOrderConfig();
		config.m_iOpenSlots = DEFAULT_OPEN_SLOTS;
		config.m_iRefillMinutes = DEFAULT_REFILL_MINUTES;
		config.m_iMinLevel = DEFAULT_MIN_LEVEL;
		config.m_sJob = "DELIVERY";
		config.m_iPremiumPct = DEFAULT_PREMIUM_PCT;
		config.m_iXp = DEFAULT_XP;
		config.m_sStandingFaction = "TOWN";
		config.m_iLateStanding = DEFAULT_LATE_STANDING;
		config.m_aTemplates = {};
		return config;
	}

	//------------------------------------------------------------------------------------------------
	//! The defaults with every pack overlay of this class laid over them, in load order.
	static LF_ExportOrderConfig Get()
	{
		if (s_bLoadAttempted)
			return s_Config;

		s_bLoadAttempted = true;
		s_Config = CreateDefault();
		array<ref Resource> resources = {};
		array<ResourceName> names = {};
		LF_ConfigOverlay.LoadPackOverlays(CONFIG_PATH, LF_ExportOrderConfig, LOG_LABEL, resources, names);
		foreach (int i, Resource resource : resources)
		{
			int changed = s_Config.ApplyContainer(resource.GetResource().ToBaseContainer());
			LF_Debug.Info(LF_ConfigService.FEATURE, string.Format("exportOrders: pack overlay %1 changed %2 fields", names[i], changed));
		}

		if (s_Config.m_aTemplates.IsEmpty())
			LF_Debug.Info("ExportOrder", "no export order kinds configured: the board stays empty");

		return s_Config;
	}

	//------------------------------------------------------------------------------------------------
	static void Reload()
	{
		s_Config = null;
		s_bLoadAttempted = false;
	}

	//------------------------------------------------------------------------------------------------
	//! Tests: makes `config` the one Get returns until Reload.
	static void UseForTest(LF_ExportOrderConfig config)
	{
		s_Config = config;
		s_bLoadAttempted = true;
	}

	//------------------------------------------------------------------------------------------------
	//! Applies one pack `.conf`: the fields it sets directly; a template list it sets replaces ours.
	//! \return Fields changed.
	int ApplyContainer(BaseContainer container)
	{
		if (!container)
			return 0;

		int changed;
		if (container.IsVariableSetDirectly("m_iOpenSlots") && container.Get("m_iOpenSlots", m_iOpenSlots))
			changed++;
		if (container.IsVariableSetDirectly("m_iRefillMinutes") && container.Get("m_iRefillMinutes", m_iRefillMinutes))
			changed++;
		if (container.IsVariableSetDirectly("m_iMinLevel") && container.Get("m_iMinLevel", m_iMinLevel))
			changed++;
		if (container.IsVariableSetDirectly("m_sJob") && container.Get("m_sJob", m_sJob))
			changed++;
		if (container.IsVariableSetDirectly("m_iPremiumPct") && container.Get("m_iPremiumPct", m_iPremiumPct))
			changed++;
		if (container.IsVariableSetDirectly("m_iXp") && container.Get("m_iXp", m_iXp))
			changed++;
		if (container.IsVariableSetDirectly("m_sStandingFaction") && container.Get("m_sStandingFaction", m_sStandingFaction))
			changed++;
		if (container.IsVariableSetDirectly("m_iLateStanding") && container.Get("m_iLateStanding", m_iLateStanding))
			changed++;

		if (container.IsVariableSetDirectly("m_aTemplates"))
		{
			BaseContainerList list = container.GetObjectArray("m_aTemplates");
			if (list)
			{
				m_aTemplates = {};
				for (int i = 0; i < list.Count(); i++)
				{
					LF_ExportOrderTemplate template = LF_ExportOrderTemplate.Cast(BaseContainerTools.CreateInstanceFromContainer(list.Get(i)));
					if (template && template.m_Goods && template.m_iCount > 0 && template.m_iWholesale >= 0 && template.m_iBonus >= 0)
						m_aTemplates.Insert(template);
					else
						LF_Debug.Error("ExportOrder", string.Format("export order kind %1 skipped: it needs goods and a count", i));
				}

				changed++;
			}
		}

		return changed;
	}

	//------------------------------------------------------------------------------------------------
	//! \return What an order of `count` units at `wholesale` pays: the premium over wholesale, rounded half up.
	int PayFor(int count, int wholesale)
	{
		int base = count * wholesale;
		return (base * (100 + m_iPremiumPct) + 50) / 100;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The wholesale price of one unit of the template's goods: its own `m_iWholesale` when set,
	//! else the market row's base price per unit (base / lot size, rounded down). 0 when neither prices it.
	static int WholesaleFor(notnull LF_ExportOrderTemplate template)
	{
		if (template.m_iWholesale > 0)
			return template.m_iWholesale;

		LF_MarketConfig market = LF_MarketConfig.Get();
		if (!market)
			return 0;

		LF_MarketGoodDef def = market.FindGood(template.m_Goods);
		if (!def)
			return 0;

		return def.m_iBase / def.Lot();
	}

	//------------------------------------------------------------------------------------------------
	//! \return What one order of the template pays: count x wholesale plus the premium, plus the order
	//! bonus. 0 when nothing prices the goods (the book then skips the kind).
	int PayForTemplate(notnull LF_ExportOrderTemplate template)
	{
		int wholesale = WholesaleFor(template);
		if (wholesale <= 0)
			return 0;

		return PayFor(template.m_iCount, wholesale) + template.m_iBonus;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_ExportOrderConfig's statics (LF_StaticReset finds it by reflection).
class LF_ExportOrderConfig_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_ExportOrderConfig";
	}

	override void Apply()
	{
		LF_ExportOrderConfig.Reload();
	}
}
