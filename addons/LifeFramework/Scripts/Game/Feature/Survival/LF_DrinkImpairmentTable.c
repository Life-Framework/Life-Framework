//------------------------------------------------------------------------------------------------
//! The impairment a drink gives, by prefab (#2002). `LF_ConsumableEffectImpair` carries default
//! numbers of its own; a row here for the drink's prefab wins over them. The core file
//! (Configs/Survival/LF_DrinkImpairment.conf) ships no rows: what a drink does is a server's
//! decision, so a pack overlays rows by prefab (add, replace, patch or remove through the override
//! model, LF_ConfigOverlay) and a drink with no row keeps the component's values.
//! The id of a row is its prefab's GUID, so `{GUID}path` casing cannot split two spellings.

//------------------------------------------------------------------------------------------------
//! One drink's dose.
[BaseContainerProps()]
class LF_DrinkImpairmentRow : LF_ConfigRecord
{
	[Attribute("", UIWidgets.ResourcePickerThumbnail, "The drink's prefab; its GUID is the row's id", "et")]
	ResourceName m_Prefab;

	[Attribute("20", UIWidgets.Slider, "Impairment points one use adds (0-100)", params: "0 100 1")]
	float m_fStrength;

	[Attribute("600", UIWidgets.Auto, "Seconds until the level has worn off")]
	float m_fDurationSeconds;

	[Attribute("0", UIWidgets.ComboBox, "How the level falls", enums: ParamEnumArray.FromEnum(LF_EImpairmentCurve))]
	LF_EImpairmentCurve m_eCurve;

	[Attribute("60", UIWidgets.Slider, "Most impairment drinking this can stack up to (0-100)", params: "0 100 1")]
	float m_fCap;

	//------------------------------------------------------------------------------------------------
	//! Test and fallback seam: a row built in script has no attributes to fill.
	static LF_DrinkImpairmentRow Make(ResourceName prefab, float strength, float durationSeconds, LF_EImpairmentCurve curve, float cap)
	{
		LF_DrinkImpairmentRow row = new LF_DrinkImpairmentRow();
		row.m_Prefab = prefab;
		row.m_fStrength = strength;
		row.m_fDurationSeconds = durationSeconds;
		row.m_eCurve = curve;
		row.m_fCap = cap;
		return row;
	}
}

//------------------------------------------------------------------------------------------------
//! The table. Loaded from Configs/Survival/LF_DrinkImpairment.conf; pack overlays layer over it.
[BaseContainerProps(configRoot: true)]
class LF_DrinkImpairmentConfig
{
	static const ResourceName CONFIG_PATH = "{B51DE038F39E5336}Configs/Survival/LF_DrinkImpairment.conf";
	static const string OVERLAY_NAME = "drinkImpairment";

	[Attribute("", UIWidgets.Object, "One row per drink prefab")]
	ref array<ref LF_DrinkImpairmentRow> m_aDrinks;

	//------------------------------------------------------------------------------------------------
	//! "{GUID}path" to "GUID" in upper case; a bare path comes back upper-cased whole.
	static string GuidOf(ResourceName resource)
	{
		string text = resource;
		if (text.StartsWith("{"))
		{
			int close = text.IndexOf("}");
			if (close > 1)
				text = text.Substring(1, close - 1);
		}

		text.ToUpper();
		return text;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The row for this drink prefab, or null when the table has none.
	LF_DrinkImpairmentRow Find(ResourceName prefab)
	{
		if (!m_aDrinks || prefab.IsEmpty())
			return null;

		string guid = GuidOf(prefab);
		foreach (LF_DrinkImpairmentRow row : m_aDrinks)
		{
			if (row && GuidOf(row.m_Prefab) == guid)
				return row;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! The row that applies to a drink: the table's when it has one, else the fallback (the
	//! component's own values).
	LF_DrinkImpairmentRow Resolve(ResourceName prefab, notnull LF_DrinkImpairmentRow fallback)
	{
		LF_DrinkImpairmentRow row = Find(prefab);
		if (row)
			return row;

		return fallback;
	}

	//------------------------------------------------------------------------------------------------
	//! Why a row cannot stand (no prefab, or a prefab that does not load), or empty.
	static string Problem(notnull LF_DrinkImpairmentRow row)
	{
		if (row.m_Prefab.IsEmpty())
			return "the row names no prefab";

		Resource prefab = Resource.Load(row.m_Prefab);
		if (!prefab || !prefab.IsValid())
			return string.Format("the prefab does not load: %1", row.m_Prefab);

		return string.Empty;
	}

	//------------------------------------------------------------------------------------------------
	//! Drops the rows of this instance that name a missing prefab (logged, one line each). The core
	//! file is checked this way; overlay rows are checked as they merge.
	//! \return Rows dropped.
	int DropBrokenRows(string source)
	{
		if (!m_aDrinks)
			return 0;

		int dropped;
		for (int i = m_aDrinks.Count() - 1; i >= 0; i--)
		{
			string why;
			if (!m_aDrinks[i])
				why = "empty row";
			else
				why = Problem(m_aDrinks[i]);

			if (why.IsEmpty())
				continue;

			LF_Debug.Error(LF_Impairment.LOG, string.Format("%1: drink row skipped: %2", source, why));
			m_aDrinks.RemoveOrdered(i);
			dropped++;
		}

		return dropped;
	}

	//------------------------------------------------------------------------------------------------
	//! Layers the pack overlays of the core file, in load order, over this instance.
	void ApplyLayers(ResourceName coreConf = CONFIG_PATH)
	{
		array<ref Resource> resources = {};
		array<ResourceName> names = {};
		LF_ConfigOverlay.LoadPackOverlays(coreConf, LF_DrinkImpairmentConfig, OVERLAY_NAME, resources, names);
		LF_ConfigClaims claims = new LF_ConfigClaims();
		foreach (int i, Resource resource : resources)
		{
			int changed = ApplyContainer(resource.GetResource().ToBaseContainer(), names[i], claims);
			LF_Debug.Info(LF_Impairment.LOG, string.Format("drink table: pack overlay %1 changed %2 row(s)", names[i], changed));
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Applies one pack `.conf` of this class: its `m_aDrinks` rows by the override verbs (`m_sOp`).
	//! \return Rows added, replaced, patched or removed.
	int ApplyContainer(notnull BaseContainer container, string source = "pack", LF_ConfigClaims claims = null)
	{
		if (!m_aDrinks)
			m_aDrinks = new array<ref LF_DrinkImpairmentRow>();

		return LF_ConfigOverlay.ApplyContainerList(new LF_DrinkImpairmentList(this), container, "m_aDrinks", source, claims);
	}
}

//------------------------------------------------------------------------------------------------
//! The row list as the override model sees it. The id is the prefab's GUID.
class LF_DrinkImpairmentList : LF_ConfigList
{
	protected LF_DrinkImpairmentConfig m_Config;

	void LF_DrinkImpairmentList(LF_DrinkImpairmentConfig config)
	{
		m_Config = config;
	}

	override string GetName()
	{
		return "drinkImpairment.drinks";
	}

	override int Count()
	{
		return m_Config.m_aDrinks.Count();
	}

	override LF_ConfigRecord Get(int index)
	{
		return m_Config.m_aDrinks[index];
	}

	override string IdOf(notnull LF_ConfigRecord record)
	{
		LF_DrinkImpairmentRow row = LF_DrinkImpairmentRow.Cast(record);
		if (row.m_Prefab.IsEmpty())
			return string.Empty;

		return LF_DrinkImpairmentConfig.GuidOf(row.m_Prefab);
	}

	override string IdOfContainer(notnull BaseContainer container)
	{
		string prefab;
		container.Get("m_Prefab", prefab);
		if (prefab.IsEmpty())
			return string.Empty;

		return LF_DrinkImpairmentConfig.GuidOf(prefab);
	}

	override string IdHint()
	{
		return "a drink prefab GUID";
	}

	override LF_ConfigRecord NewRecord(string id)
	{
		return LF_DrinkImpairmentRow.Make("{" + id + "}", 20, 600, LF_EImpairmentCurve.LINEAR, 60);
	}

	override void Set(int index, notnull LF_ConfigRecord record)
	{
		LF_DrinkImpairmentRow row = LF_DrinkImpairmentRow.Cast(record);
		if (index < m_Config.m_aDrinks.Count())
			m_Config.m_aDrinks.Set(index, row);
		else
			m_Config.m_aDrinks.Insert(row);
	}

	override void RemoveAt(int index)
	{
		m_Config.m_aDrinks.RemoveOrdered(index);
	}

	override int PatchFromContainer(notnull LF_ConfigRecord record, notnull BaseContainer container)
	{
		LF_DrinkImpairmentRow row = LF_DrinkImpairmentRow.Cast(record);
		int changed;
		if (container.IsVariableSetDirectly("m_fStrength") && container.Get("m_fStrength", row.m_fStrength))
			changed++;
		if (container.IsVariableSetDirectly("m_fDurationSeconds") && container.Get("m_fDurationSeconds", row.m_fDurationSeconds))
			changed++;
		if (container.IsVariableSetDirectly("m_fCap") && container.Get("m_fCap", row.m_fCap))
			changed++;
		if (container.IsVariableSetDirectly("m_eCurve"))
		{
			int curve;
			if (container.Get("m_eCurve", curve))
			{
				row.m_eCurve = curve;
				changed++;
			}
		}
		return changed;
	}

	//! A row that names no prefab, or one that does not load, cannot stand.
	override string Incomplete(notnull LF_ConfigRecord record)
	{
		return LF_DrinkImpairmentConfig.Problem(LF_DrinkImpairmentRow.Cast(record));
	}
}

//------------------------------------------------------------------------------------------------
//! The loaded table: core file, then pack overlays.
class LF_DrinkImpairmentTable
{
	protected static ref LF_DrinkImpairmentConfig s_Config;
	protected static bool s_bLoadAttempted;

	//------------------------------------------------------------------------------------------------
	//! The loaded table, or null (logged once) when the file does not resolve; the effect then keeps
	//! its own values.
	static LF_DrinkImpairmentConfig Get()
	{
		if (s_bLoadAttempted)
			return s_Config;

		s_bLoadAttempted = true;
		Resource container = BaseContainerTools.LoadContainer(LF_DrinkImpairmentConfig.CONFIG_PATH);
		if (!container || !container.GetResource())
		{
			LF_Debug.Error(LF_Impairment.LOG, string.Format("drink impairment table does not resolve: %1", LF_DrinkImpairmentConfig.CONFIG_PATH));
			return null;
		}

		s_Config = LF_DrinkImpairmentConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(container.GetResource().ToBaseContainer()));
		if (!s_Config)
		{
			LF_Debug.Error(LF_Impairment.LOG, string.Format("drink impairment table has an unexpected root class: %1", LF_DrinkImpairmentConfig.CONFIG_PATH));
			return null;
		}

		if (!s_Config.m_aDrinks)
			s_Config.m_aDrinks = new array<ref LF_DrinkImpairmentRow>();

		s_Config.DropBrokenRows("core");
		s_Config.ApplyLayers();
		return s_Config;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The row for a drink prefab, or null when no row names it (or the table is missing).
	static LF_DrinkImpairmentRow Find(ResourceName prefab)
	{
		LF_DrinkImpairmentConfig config = Get();
		if (!config)
			return null;

		return config.Find(prefab);
	}

	//------------------------------------------------------------------------------------------------
	//! World-end reset: the next Get() loads the core file and layers the overlays again.
	static void ResetStatics()
	{
		s_Config = null;
		s_bLoadAttempted = false;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_DrinkImpairmentTable's statics (registered at script load; Core/LF_StaticReset.c).
class LF_DrinkImpairmentTable_StaticResetHook : LF_StaticResetHook
{
	protected static bool s_bRegistered = LF_StaticReset.Register(new LF_DrinkImpairmentTable_StaticResetHook());

	override string Owner()
	{
		return "LF_DrinkImpairmentTable";
	}

	override void Apply()
	{
		LF_DrinkImpairmentTable.ResetStatics();
	}
}
