//------------------------------------------------------------------------------------------------
//! The welcome gift (#1781): what the clerk hands over at the end of the arrival chain, by quest id.
//! The core file (Configs/Quests/LF_WelcomeGift.conf) ships no rows: what a newcomer is given is a
//! server's decision (the 1989 Everon pack gives a map, food and a drink), so a pack overlays rows
//! by quest id through the override model (LF_ConfigOverlay: add, replace, patch, remove).
//! `LF_QuestTrackerComponent.GrantReward` spawns a quest's rows after its own `m_aRewardItems`.
//! No row, no gift; the chain itself never pays cash.

//------------------------------------------------------------------------------------------------
//! One quest's gift.
[BaseContainerProps()]
class LF_WelcomeGiftRow : LF_ConfigRecord
{
	[Attribute("", UIWidgets.EditBox, "The quest whose turn-in hands the gift over; its id is the row's id")]
	string m_sQuestId;

	[Attribute("", UIWidgets.ResourceNamePicker, "The items spawned into the player's inventory on turn-in", "et")]
	ref array<ResourceName> m_aItems;

	//------------------------------------------------------------------------------------------------
	//! Test and fallback seam: a row built in script has no attributes to fill.
	static LF_WelcomeGiftRow Make(string questId, notnull array<ResourceName> items)
	{
		LF_WelcomeGiftRow row = new LF_WelcomeGiftRow();
		row.m_sQuestId = questId;
		row.m_aItems = {};
		row.m_aItems.Copy(items);
		return row;
	}
}

//------------------------------------------------------------------------------------------------
//! The table. Loaded from Configs/Quests/LF_WelcomeGift.conf; pack overlays layer over it.
[BaseContainerProps(configRoot: true)]
class LF_WelcomeGiftConfig
{
	static const ResourceName CONFIG_PATH = "{7B3E1F04C2A95D68}Configs/Quests/LF_WelcomeGift.conf";
	static const string OVERLAY_NAME = "welcomeGift";
	static const string LOG = "Quests";

	[Attribute("", UIWidgets.Object, "One row per quest that hands a gift over")]
	ref array<ref LF_WelcomeGiftRow> m_aGifts;

	//------------------------------------------------------------------------------------------------
	//! \return The row for this quest id, or null when the table has none.
	LF_WelcomeGiftRow Find(string questId)
	{
		if (!m_aGifts || questId.IsEmpty())
			return null;

		foreach (LF_WelcomeGiftRow row : m_aGifts)
		{
			if (row && row.m_sQuestId == questId)
				return row;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Why a row cannot stand (no quest id, no items, or an item that does not load), or empty.
	static string Problem(notnull LF_WelcomeGiftRow row)
	{
		if (row.m_sQuestId.IsEmpty())
			return "the row names no quest";

		if (!row.m_aItems || row.m_aItems.IsEmpty())
			return string.Format("the row for %1 lists no items", row.m_sQuestId);

		foreach (ResourceName item : row.m_aItems)
		{
			if (item.IsEmpty())
				return string.Format("the row for %1 has an empty item", row.m_sQuestId);

			Resource prefab = Resource.Load(item);
			if (!prefab || !prefab.IsValid())
				return string.Format("the row for %1 names an item that does not load: %2", row.m_sQuestId, item);
		}

		return string.Empty;
	}

	//------------------------------------------------------------------------------------------------
	//! Layers the pack overlays of the core file, in load order, over this instance.
	void ApplyLayers(ResourceName coreConf = CONFIG_PATH)
	{
		array<ref Resource> resources = {};
		array<ResourceName> names = {};
		LF_ConfigOverlay.LoadPackOverlays(coreConf, LF_WelcomeGiftConfig, OVERLAY_NAME, resources, names);
		LF_ConfigClaims claims = new LF_ConfigClaims();
		foreach (int i, Resource resource : resources)
		{
			int changed = ApplyContainer(resource.GetResource().ToBaseContainer(), names[i], claims);
			LF_Debug.Info(LOG, string.Format("welcome gift: pack overlay %1 changed %2 row(s)", names[i], changed));
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Applies one pack `.conf` of this class: its `m_aGifts` rows by the override verbs (`m_sOp`).
	//! \return Rows added, replaced, patched or removed.
	int ApplyContainer(notnull BaseContainer container, string source = "pack", LF_ConfigClaims claims = null)
	{
		if (!m_aGifts)
			m_aGifts = new array<ref LF_WelcomeGiftRow>();

		return LF_ConfigOverlay.ApplyContainerList(new LF_WelcomeGiftList(this), container, "m_aGifts", source, claims);
	}
}

//------------------------------------------------------------------------------------------------
//! The row list as the override model sees it. The id is the quest id.
class LF_WelcomeGiftList : LF_ConfigList
{
	protected LF_WelcomeGiftConfig m_Config;

	void LF_WelcomeGiftList(LF_WelcomeGiftConfig config)
	{
		m_Config = config;
	}

	override string GetName()
	{
		return "welcomeGift.gifts";
	}

	override int Count()
	{
		return m_Config.m_aGifts.Count();
	}

	override LF_ConfigRecord Get(int index)
	{
		return m_Config.m_aGifts[index];
	}

	override string IdOf(notnull LF_ConfigRecord record)
	{
		return LF_WelcomeGiftRow.Cast(record).m_sQuestId;
	}

	override string IdOfContainer(notnull BaseContainer container)
	{
		string questId;
		container.Get("m_sQuestId", questId);
		return questId;
	}

	override string IdHint()
	{
		return "a quest id";
	}

	override LF_ConfigRecord NewRecord(string id)
	{
		return LF_WelcomeGiftRow.Make(id, {});
	}

	override void Set(int index, notnull LF_ConfigRecord record)
	{
		LF_WelcomeGiftRow row = LF_WelcomeGiftRow.Cast(record);
		if (index < m_Config.m_aGifts.Count())
			m_Config.m_aGifts.Set(index, row);
		else
			m_Config.m_aGifts.Insert(row);
	}

	override void RemoveAt(int index)
	{
		m_Config.m_aGifts.RemoveOrdered(index);
	}

	override int PatchFromContainer(notnull LF_ConfigRecord record, notnull BaseContainer container)
	{
		LF_WelcomeGiftRow row = LF_WelcomeGiftRow.Cast(record);
		if (!container.IsVariableSetDirectly("m_aItems"))
			return 0;

		array<ResourceName> items = {};
		if (!container.Get("m_aItems", items))
			return 0;

		row.m_aItems = {};
		row.m_aItems.Copy(items);
		return 1;
	}

	//! A row that names no quest, no items, or an item that does not load cannot stand.
	override string Incomplete(notnull LF_ConfigRecord record)
	{
		return LF_WelcomeGiftConfig.Problem(LF_WelcomeGiftRow.Cast(record));
	}
}

//------------------------------------------------------------------------------------------------
//! The loaded table: core file, then pack overlays.
class LF_WelcomeGiftTable
{
	protected static ref LF_WelcomeGiftConfig s_Config;
	protected static bool s_bLoadAttempted;

	//------------------------------------------------------------------------------------------------
	//! The loaded table, or null (logged once) when the file does not resolve; no quest then hands a
	//! gift over.
	static LF_WelcomeGiftConfig Get()
	{
		if (s_bLoadAttempted)
			return s_Config;

		s_bLoadAttempted = true;
		Resource container = BaseContainerTools.LoadContainer(LF_WelcomeGiftConfig.CONFIG_PATH);
		if (!container || !container.GetResource())
		{
			LF_Debug.Error(LF_WelcomeGiftConfig.LOG, string.Format("welcome gift table does not resolve: %1", LF_WelcomeGiftConfig.CONFIG_PATH));
			return null;
		}

		s_Config = LF_WelcomeGiftConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(container.GetResource().ToBaseContainer()));
		if (!s_Config)
		{
			LF_Debug.Error(LF_WelcomeGiftConfig.LOG, string.Format("welcome gift table has an unexpected root class: %1", LF_WelcomeGiftConfig.CONFIG_PATH));
			return null;
		}

		if (!s_Config.m_aGifts)
			s_Config.m_aGifts = new array<ref LF_WelcomeGiftRow>();

		s_Config.ApplyLayers();
		return s_Config;
	}

	//------------------------------------------------------------------------------------------------
	//! Adds the items a quest's turn-in hands over from the table to `items` (none when it has no row).
	static void ItemsOf(string questId, notnull array<ResourceName> items)
	{
		LF_WelcomeGiftConfig config = Get();
		if (!config)
			return;

		LF_WelcomeGiftRow row = config.Find(questId);
		if (!row || !row.m_aItems)
			return;

		foreach (ResourceName item : row.m_aItems)
		{
			items.Insert(item);
		}
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
//! World-end reset of LF_WelcomeGiftTable's statics (registered at script load; Core/LF_StaticReset.c).
class LF_WelcomeGiftTable_StaticResetHook : LF_StaticResetHook
{
	protected static bool s_bRegistered = LF_StaticReset.Register(new LF_WelcomeGiftTable_StaticResetHook());

	override string Owner()
	{
		return "LF_WelcomeGiftTable";
	}

	override void Apply()
	{
		LF_WelcomeGiftTable.ResetStatics();
	}
}
