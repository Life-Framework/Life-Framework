//------------------------------------------------------------------------------------------------
//! The quest giver table (#3124): quests a pack adds to a giver that already stands in the world,
//! without a prefab variant. The core file (Configs/Quests/LF_QuestGiverTable.conf) ships no rows:
//! which errands a giver offers beyond its prefab's own is a server's decision, so a pack overlays
//! rows by giver id through the override model (LF_ConfigOverlay: add, replace, patch, remove), the
//! way LF_WelcomeGiftTable does for the gift. `LF_QuestGiverComponent.GetQuests()` returns the
//! prefab's quests followed by the row's. A quest id the giver already offers is refused (the prefab
//! quest wins: quest ids are persistence keys), and a row for a giver id nothing registers is
//! logged by `Audit` and never read.

//------------------------------------------------------------------------------------------------
//! One giver's added quests. The id is the giver id.
[BaseContainerProps()]
class LF_QuestGiverRow : LF_ConfigRecord
{
	[Attribute("", UIWidgets.EditBox, "The giver id (LF_QuestGiverComponent.m_sGiverId) these quests are added to; the row's id")]
	string m_sGiverId;

	[Attribute("", UIWidgets.Object, "Quests the giver offers after its prefab's own")]
	ref array<ref LF_QuestDefinition> m_aQuests;

	//------------------------------------------------------------------------------------------------
	//! Test and fallback seam: a row built in script has no attributes to fill.
	static LF_QuestGiverRow Make(string giverId)
	{
		LF_QuestGiverRow row = new LF_QuestGiverRow();
		row.m_sGiverId = giverId;
		row.m_aQuests = {};
		return row;
	}
}

//------------------------------------------------------------------------------------------------
//! The table. Loaded from Configs/Quests/LF_QuestGiverTable.conf; pack overlays layer over it.
[BaseContainerProps(configRoot: true)]
class LF_QuestGiverConfig
{
	static const ResourceName CONFIG_PATH = "{3C5A7E19B4D02F86}Configs/Quests/LF_QuestGiverTable.conf";
	static const string OVERLAY_NAME = "questGivers";
	static const string LOG = "Quests";

	[Attribute("", UIWidgets.Object, "One row per giver that is offered more quests")]
	ref array<ref LF_QuestGiverRow> m_aGivers;

	//------------------------------------------------------------------------------------------------
	//! \return The row for this giver id, or null when the table has none.
	LF_QuestGiverRow Find(string giverId)
	{
		if (!m_aGivers || giverId.IsEmpty())
			return null;

		foreach (LF_QuestGiverRow row : m_aGivers)
		{
			if (row && row.m_sGiverId == giverId)
				return row;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Why a row cannot stand (no giver id, no quests, a quest with no id), or empty.
	static string Problem(notnull LF_QuestGiverRow row)
	{
		if (row.m_sGiverId.IsEmpty())
			return "the row names no giver";

		if (!row.m_aQuests || row.m_aQuests.IsEmpty())
			return string.Format("the row for %1 lists no quests", row.m_sGiverId);

		foreach (LF_QuestDefinition quest : row.m_aQuests)
		{
			if (!quest || quest.m_sQuestId.IsEmpty())
				return string.Format("the row for %1 has a quest with no id", row.m_sGiverId);
		}

		return string.Empty;
	}

	//------------------------------------------------------------------------------------------------
	//! Layers the pack overlays of the core file, in load order, over this instance.
	void ApplyLayers(ResourceName coreConf = CONFIG_PATH)
	{
		array<ref Resource> resources = {};
		array<ResourceName> names = {};
		LF_ConfigOverlay.LoadPackOverlays(coreConf, LF_QuestGiverConfig, OVERLAY_NAME, resources, names);
		LF_ConfigClaims claims = new LF_ConfigClaims();
		foreach (int i, Resource resource : resources)
		{
			int changed = ApplyContainer(resource.GetResource().ToBaseContainer(), names[i], claims);
			LF_Debug.Info(LOG, string.Format("quest givers: pack overlay %1 changed %2 row(s)", names[i], changed));
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Applies one pack `.conf` of this class: its `m_aGivers` rows by the override verbs (`m_sOp`).
	//! \return Rows added, replaced, patched or removed.
	int ApplyContainer(notnull BaseContainer container, string source = "pack", LF_ConfigClaims claims = null)
	{
		if (!m_aGivers)
			m_aGivers = new array<ref LF_QuestGiverRow>();

		return LF_ConfigOverlay.ApplyContainerList(new LF_QuestGiverList(this), container, "m_aGivers", source, claims);
	}
}

//------------------------------------------------------------------------------------------------
//! The row list as the override model sees it. The id is the giver id.
class LF_QuestGiverList : LF_ConfigList
{
	protected LF_QuestGiverConfig m_Config;

	void LF_QuestGiverList(LF_QuestGiverConfig config)
	{
		m_Config = config;
	}

	override string GetName()
	{
		return "questGivers.givers";
	}

	override int Count()
	{
		return m_Config.m_aGivers.Count();
	}

	override LF_ConfigRecord Get(int index)
	{
		return m_Config.m_aGivers[index];
	}

	override string IdOf(notnull LF_ConfigRecord record)
	{
		return LF_QuestGiverRow.Cast(record).m_sGiverId;
	}

	override string IdOfContainer(notnull BaseContainer container)
	{
		string giverId;
		container.Get("m_sGiverId", giverId);
		return giverId;
	}

	override string IdHint()
	{
		return "a quest giver id";
	}

	override LF_ConfigRecord NewRecord(string id)
	{
		return LF_QuestGiverRow.Make(id);
	}

	override void Set(int index, notnull LF_ConfigRecord record)
	{
		LF_QuestGiverRow row = LF_QuestGiverRow.Cast(record);
		if (index < m_Config.m_aGivers.Count())
			m_Config.m_aGivers.Set(index, row);
		else
			m_Config.m_aGivers.Insert(row);
	}

	override void RemoveAt(int index)
	{
		m_Config.m_aGivers.RemoveOrdered(index);
	}

	//! A patch gives the giver's whole quest list again; the quests it leaves out are dropped.
	override int PatchFromContainer(notnull LF_ConfigRecord record, notnull BaseContainer container)
	{
		LF_QuestGiverRow row = LF_QuestGiverRow.Cast(record);
		if (!container.IsVariableSetDirectly("m_aQuests"))
			return 0;

		row.m_aQuests = new array<ref LF_QuestDefinition>();
		BaseContainerList quests = container.GetObjectArray("m_aQuests");
		if (quests)
		{
			for (int i = 0; i < quests.Count(); i++)
			{
				BaseContainer entry = quests.Get(i);
				if (!entry)
					continue;

				LF_QuestDefinition quest = LF_QuestDefinition.Cast(BaseContainerTools.CreateInstanceFromContainer(entry));
				if (quest)
					row.m_aQuests.Insert(quest);
			}
		}

		return 1;
	}

	//! A row that names no giver, no quests, or a quest with no id cannot stand.
	override string Incomplete(notnull LF_ConfigRecord record)
	{
		return LF_QuestGiverConfig.Problem(LF_QuestGiverRow.Cast(record));
	}
}

//------------------------------------------------------------------------------------------------
//! The loaded table: core file, then pack overlays.
class LF_QuestGiverTable
{
	protected static ref LF_QuestGiverConfig s_Config;
	protected static bool s_bLoadAttempted;
	protected static bool s_bAuditQueued;
	protected static ref array<string> s_aRefused = {};

	//! Delay before the unknown-giver audit: every placed giver has registered by then.
	static const int AUDIT_DELAY_MS = 30000; // lf-fixed: an internal settle delay, not a server setting

	//------------------------------------------------------------------------------------------------
	//! The loaded table, or null (logged once) when the file does not resolve; no giver then gains a
	//! quest.
	static LF_QuestGiverConfig Get()
	{
		if (s_bLoadAttempted)
			return s_Config;

		s_bLoadAttempted = true;
		Resource container = BaseContainerTools.LoadContainer(LF_QuestGiverConfig.CONFIG_PATH);
		if (!container || !container.GetResource())
		{
			LF_Debug.Error(LF_QuestGiverConfig.LOG, string.Format("quest giver table does not resolve: %1", LF_QuestGiverConfig.CONFIG_PATH));
			return null;
		}

		s_Config = LF_QuestGiverConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(container.GetResource().ToBaseContainer()));
		if (!s_Config)
		{
			LF_Debug.Error(LF_QuestGiverConfig.LOG, string.Format("quest giver table has an unexpected root class: %1", LF_QuestGiverConfig.CONFIG_PATH));
			return null;
		}

		if (!s_Config.m_aGivers)
			s_Config.m_aGivers = new array<ref LF_QuestGiverRow>();

		s_Config.ApplyLayers();
		return s_Config;
	}

	//------------------------------------------------------------------------------------------------
	//! The quests a giver offers: its prefab's own, then the table's row for its id. A table quest
	//! whose id the giver already offers is refused (logged once) and the earlier one wins. With no
	//! row the prefab's own list is returned as it is.
	static array<ref LF_QuestDefinition> QuestsFor(string giverId, array<ref LF_QuestDefinition> prefabQuests)
	{
		LF_QuestGiverConfig config = Get();
		if (!config)
			return prefabQuests;

		LF_QuestGiverRow row = config.Find(giverId);
		if (!row || !row.m_aQuests || row.m_aQuests.IsEmpty())
			return prefabQuests;

		array<ref LF_QuestDefinition> merged = {};
		array<string> ids = {};
		if (prefabQuests)
		{
			foreach (LF_QuestDefinition own : prefabQuests)
			{
				merged.Insert(own);
				if (own)
					ids.Insert(own.m_sQuestId);
			}
		}

		foreach (LF_QuestDefinition added : row.m_aQuests)
		{
			if (!added || added.m_sQuestId.IsEmpty())
				continue;

			if (ids.Find(added.m_sQuestId) != -1)
			{
				Refuse(giverId, added.m_sQuestId);
				continue;
			}

			ids.Insert(added.m_sQuestId);
			merged.Insert(added);
		}

		return merged;
	}

	//------------------------------------------------------------------------------------------------
	protected static void Refuse(string giverId, string questId)
	{
		string key = giverId + "/" + questId;
		if (s_aRefused.Find(key) != -1)
			return;

		s_aRefused.Insert(key);
		LF_Debug.Warn(LF_QuestGiverConfig.LOG, string.Format("quest giver table: giver '%1' already offers quest '%2'; the table's copy is refused", giverId, questId));
	}

	//------------------------------------------------------------------------------------------------
	//! \return How many table quests were refused as a duplicate id (each counted once).
	static int RefusedCount()
	{
		return s_aRefused.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Logs each row whose giver id no registered giver has; such a row is never read.
	//! \return How many rows are for an unknown giver.
	static int Audit()
	{
		LF_QuestGiverConfig config = Get();
		if (!config || !config.m_aGivers)
			return 0;

		LF_QuestManager manager = LF_QuestManager.GetInstance();
		int unknown;
		foreach (LF_QuestGiverRow row : config.m_aGivers)
		{
			if (!row || manager.GetGiver(row.m_sGiverId))
				continue;

			unknown++;
			LF_Debug.Warn(LF_QuestGiverConfig.LOG, string.Format("quest giver table: no giver '%1' stands in this world; its row is ignored", row.m_sGiverId));
		}

		return unknown;
	}

	//------------------------------------------------------------------------------------------------
	//! Queues the audit once per world, after the placed givers have registered.
	static void QueueAudit()
	{
		if (s_bAuditQueued)
			return;

		s_bAuditQueued = true;
		if (GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().CallLater(Audit, AUDIT_DELAY_MS, false);
	}

	//------------------------------------------------------------------------------------------------
	//! World-end reset: the next Get() loads the core file and layers the overlays again.
	static void ResetStatics()
	{
		s_Config = null;
		s_bLoadAttempted = false;
		s_bAuditQueued = false;
		s_aRefused = {};
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_QuestGiverTable's statics (LF_StaticReset finds it by reflection).
class LF_QuestGiverTable_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_QuestGiverTable";
	}

	override void Apply()
	{
		LF_QuestGiverTable.ResetStatics();
	}
}
