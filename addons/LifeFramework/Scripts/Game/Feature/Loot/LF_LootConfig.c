//------------------------------------------------------------------------------------------------
//! The loot config (#3341, slice of #1020): Configs/Loot/LF_Loot.conf holds one loot table per
//! container kind: weighted rows (prefab, count range), how many rolls fill the container and how long
//! until it refills. The core ships the structure and a neutral example; a pack adds or tunes tables by
//! overlay (add, replace, patch, remove; docs/config/override-model.md). LF_LootRules rolls a table.
//------------------------------------------------------------------------------------------------

//! One thing a container can hold: a prefab, how likely it is next to its siblings and how many.
[BaseContainerProps()]
class LF_LootRowDef
{
	[Attribute(ResourceName.Empty, UIWidgets.ResourcePickerThumbnail, "Item that can be found", "et")]
	ResourceName m_Prefab;

	[Attribute("1", UIWidgets.Auto, "Weight against the other rows: 2 is twice as likely as 1. 0 means never picked")]
	int m_iWeight;

	[Attribute("1", UIWidgets.Auto, "Fewest items of this row one pick gives")]
	int m_iCountMin;

	[Attribute("1", UIWidgets.Auto, "Most items of this row one pick gives (not below the fewest)")]
	int m_iCountMax;
}

//------------------------------------------------------------------------------------------------
//! The loot table of one container kind.
[BaseContainerProps()]
class LF_LootTableDef : LF_ConfigRecord
{
	[Attribute("", UIWidgets.EditBox, "Container kind, unique across the config (crate, locker, toolbox)")]
	string m_sKind;

	[Attribute("", UIWidgets.Object, "Weighted rows; at least one needs a weight above 0")]
	ref array<ref LF_LootRowDef> m_aRows;

	[Attribute("3", UIWidgets.Auto, "How many rolls fill the container (its fill size). Each roll picks one row")]
	int m_iRolls;

	[Attribute("1800", UIWidgets.Auto, "Seconds after a fill before the container may refill. 0 refills at once")]
	int m_iRefillSeconds;
}

//------------------------------------------------------------------------------------------------
[BaseContainerProps(configRoot: true)]
class LF_LootConfig
{
	static const ResourceName CONFIG_PATH = "{01A03D5C220AD64B}Configs/Loot/LF_Loot.conf";

	[Attribute("", UIWidgets.Object, "The loot tables, one per container kind")]
	ref array<ref LF_LootTableDef> m_aTables;

	protected static ref LF_LootConfig s_Config;
	protected static bool s_bLoadAttempted;
	protected static ResourceName s_PathOverride;

	//------------------------------------------------------------------------------------------------
	//! The loaded config, or null (logged once) when the file does not resolve.
	static LF_LootConfig Get()
	{
		if (s_bLoadAttempted)
			return s_Config;

		s_bLoadAttempted = true;
		ResourceName path = CONFIG_PATH;
		if (!s_PathOverride.IsEmpty())
			path = s_PathOverride;

		s_Config = LoadFrom(path);
		// A path override (tests) replaces the shipped file whole; only the shipped file takes pack layers.
		if (s_Config && s_PathOverride.IsEmpty())
			s_Config.ApplyLayers();

		return s_Config;
	}

	//------------------------------------------------------------------------------------------------
	//! Merges each pack overlay (the mission header's m_aLF_ConfigOverlays, in load order) over this
	//! instance by container kind (add, replace, patch, remove).
	void ApplyLayers()
	{
		if (!m_aTables)
			m_aTables = new array<ref LF_LootTableDef>();

		array<ref Resource> resources = {};
		array<ResourceName> names = {};
		LF_ConfigOverlay.LoadPackOverlays(CONFIG_PATH, LF_LootConfig, "loot", resources, names);
		LF_ConfigClaims claims = new LF_ConfigClaims();
		foreach (int i, Resource resource : resources)
		{
			int changed = ApplyPackContainer(resource.GetResource().ToBaseContainer(), names[i], claims);
			LF_Debug.Info("Loot", string.Format("loot: pack overlay %1 changed %2 tables", names[i], changed));
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Applies one pack overlay's `m_aTables` by container kind. \return Entries changed.
	int ApplyPackContainer(notnull BaseContainer root, string source, LF_ConfigClaims claims = null)
	{
		if (!m_aTables)
			m_aTables = new array<ref LF_LootTableDef>();

		return LF_ConfigOverlay.ApplyContainerList(new LF_LootTableList(this), root, "m_aTables", source, claims);
	}

	//------------------------------------------------------------------------------------------------
	//! Loads one config file. A broken file logs LF_Debug.Error and returns null; nothing else is affected.
	static LF_LootConfig LoadFrom(ResourceName path)
	{
		Resource container = BaseContainerTools.LoadContainer(path);
		if (!container || !container.GetResource())
		{
			LF_Debug.Error("Loot", string.Format("loot config does not resolve, no loot tables: %1", path));
			return null;
		}

		LF_LootConfig config = LF_LootConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(container.GetResource().ToBaseContainer()));
		if (!config)
			LF_Debug.Error("Loot", string.Format("loot config has an unexpected root class, no loot tables: %1", path));

		return config;
	}

	//------------------------------------------------------------------------------------------------
	//! Drops the cached instance so the next Get() loads again. A path reads that file instead of the
	//! shipped one (tests); no path restores the shipped config.
	static void Reload(ResourceName path = ResourceName.Empty)
	{
		s_Config = null;
		s_bLoadAttempted = false;
		s_PathOverride = path;
	}

	//------------------------------------------------------------------------------------------------
	LF_LootTableDef FindTable(string kind)
	{
		if (!m_aTables || kind.IsEmpty())
			return null;

		foreach (LF_LootTableDef table : m_aTables)
		{
			if (table && table.m_sKind == kind)
				return table;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! The table of a container kind from the loaded config; null for an unknown kind or a missing config.
	static LF_LootTableDef TableOf(string kind)
	{
		LF_LootConfig config = Get();
		if (!config)
			return null;

		return config.FindTable(kind);
	}

}

//------------------------------------------------------------------------------------------------
//! The loot tables for the shared merge (LF_ConfigOverlay), keyed by container kind. A new or
//! replaced table must pass LF_LootRules.Validate or it is refused with the reason. A patch changes
//! the fill size and refill time only; rows change by replace.
class LF_LootTableList : LF_ConfigList
{
	protected LF_LootConfig m_Config;

	void LF_LootTableList(LF_LootConfig config)
	{
		m_Config = config;
		if (!m_Config.m_aTables)
			m_Config.m_aTables = new array<ref LF_LootTableDef>();
	}

	override string GetName()
	{
		return "loot.tables";
	}

	override int Count()
	{
		return m_Config.m_aTables.Count();
	}

	override LF_ConfigRecord Get(int index)
	{
		return m_Config.m_aTables[index];
	}

	override string IdOf(notnull LF_ConfigRecord record)
	{
		return LF_LootTableDef.Cast(record).m_sKind;
	}

	override string IdOfContainer(notnull BaseContainer container)
	{
		string id;
		container.Get("m_sKind", id);
		return id;
	}

	override string IdHint()
	{
		return "a container kind";
	}

	override LF_ConfigRecord NewRecord(string id)
	{
		LF_LootTableDef table = new LF_LootTableDef();
		table.m_sKind = id;
		return table;
	}

	override void Set(int index, notnull LF_ConfigRecord record)
	{
		LF_LootTableDef table = LF_LootTableDef.Cast(record);
		if (index < m_Config.m_aTables.Count())
			m_Config.m_aTables.Set(index, table);
		else
			m_Config.m_aTables.Insert(table);
	}

	override void RemoveAt(int index)
	{
		m_Config.m_aTables.RemoveOrdered(index);
	}

	override string Incomplete(notnull LF_ConfigRecord record)
	{
		return LF_LootRules.Validate(LF_LootTableDef.Cast(record));
	}

	override int PatchFromContainer(notnull LF_ConfigRecord record, notnull BaseContainer container)
	{
		LF_LootTableDef table = LF_LootTableDef.Cast(record);
		int changed;
		if (container.IsVariableSetDirectly("m_iRolls"))
		{
			int rolls;
			if (container.Get("m_iRolls", rolls) && rolls > 0 && rolls <= LF_LootRules.MAX_ROLLS)
			{
				table.m_iRolls = rolls;
				changed++;
			}
			else
			{
				LF_Debug.Error("Loot", string.Format("loot.tables %1: pack fill size is not between 1 and %2; keeping %3", table.m_sKind, LF_LootRules.MAX_ROLLS, table.m_iRolls));
			}
		}

		if (container.IsVariableSetDirectly("m_iRefillSeconds"))
		{
			int refill;
			if (container.Get("m_iRefillSeconds", refill) && refill >= 0)
			{
				table.m_iRefillSeconds = refill;
				changed++;
			}
			else
			{
				LF_Debug.Error("Loot", string.Format("loot.tables %1: pack refill time is below 0; keeping %2", table.m_sKind, table.m_iRefillSeconds));
			}
		}

		return changed;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_LootConfig's statics (LF_StaticReset finds it by reflection).
class LF_LootConfig_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_LootConfig";
	}

	override void Apply()
	{
		LF_LootConfig.Reload();
	}
}
