// LF_QuestManager.c - world-facing quest hub: definition registry + event routing.
//
// A lazy singleton (like EL_JobManager). Quest givers register their definitions
// on server init; the gather/process/kill hook seams and the giver action route
// world events here, which resolves the acting player's LF_QuestTrackerComponent
// and pushes typed LF_QuestEvents into the pure tracker. Every accepted state
// change is logged through EL_Debug.Log("Quests", ...).

class LF_QuestManager
{
	protected static ref LF_QuestManager s_Instance;
	protected ref map<string, ref LF_QuestDefinition> m_mDefinitions = new map<string, ref LF_QuestDefinition>();

	//------------------------------------------------------------------------------------------------
	static LF_QuestManager GetInstance()
	{
		if (!s_Instance)
			s_Instance = new LF_QuestManager();
		return s_Instance;
	}

	//------------------------------------------------------------------------------------------------
	//! Merge one definition into the registry. Duplicate ids are overwritten
	//! (last registered giver wins) so a re-register is idempotent.
	void RegisterDefinition(LF_QuestDefinition def)
	{
		if (!def || def.m_sQuestId.IsEmpty())
		{
			EL_Debug.Warn("Quests", "definition skipped: missing quest id");
			return;
		}
		m_mDefinitions.Set(def.m_sQuestId, def);
	}

	//------------------------------------------------------------------------------------------------
	//! Register every quest offered by a giver component.
	void RegisterGiver(LF_QuestGiverComponent giver)
	{
		if (!giver)
			return;

		array<ref LF_QuestDefinition> quests = giver.GetQuests();
		if (!quests)
		{
			EL_Debug.Error("Quests", string.Format("giver '%1' has no quest list", giver.GetGiverId()));
			return;
		}

		foreach (LF_QuestDefinition def : quests)
		{
			if (!def)
				continue;
			RegisterDefinition(def);
			EL_Debug.Log("Quests", string.Format("registered quest '%1' from giver '%2'", def.m_sQuestId, giver.GetGiverId()));
		}
	}

	//------------------------------------------------------------------------------------------------
	LF_QuestDefinition GetDefinition(string questId)
	{
		return m_mDefinitions.Get(questId);
	}

	//------------------------------------------------------------------------------------------------
	map<string, ref LF_QuestDefinition> GetDefinitions()
	{
		return m_mDefinitions;
	}

	//------------------------------------------------------------------------------------------------
	//! Faction key of a player character, via the account manager (the mod's own
	//! faction enum, consistent with spawn points and loadouts).
	string GetCharacterFaction(IEntity character)
	{
		if (!character)
			return "";

		string uid = EL_Utils.GetCharacterId(character);
		if (uid.IsEmpty())
			return "";

		EL_PlayerAccount account = EL_PlayerAccountManager.GetInstance().GetAccount(uid);
		if (!account)
			return "";

		return typename.EnumToString(EL_Faction, account.GetFaction());
	}

	//------------------------------------------------------------------------------------------------
	//---------------------------- Event routing (server only) ----------------------------
	//------------------------------------------------------------------------------------------------

	void ReportGather(IEntity user, ResourceName item)
	{
		Route(user, MakeEvent(LF_EQuestEventType.GATHER, item, 1));
	}

	void ReportProcess(IEntity user, ResourceName item)
	{
		Route(user, MakeEvent(LF_EQuestEventType.PROCESS, item, 1));
	}

	void ReportTalk(IEntity user, string giverId)
	{
		Route(user, MakeEvent(LF_EQuestEventType.TALK, giverId, 1));
	}

	void ReportArea(IEntity user, string areaId)
	{
		Route(user, MakeEvent(LF_EQuestEventType.AREA_ENTER, areaId, 1));
	}

	//------------------------------------------------------------------------------------------------
	//! A kill counts toward the KILLER's quests, filtered by the victim's faction.
	void ReportKill(IEntity victim, IEntity killer)
	{
		LF_QuestTrackerComponent killerTracker = ResolveTracker(killer);
		if (!killerTracker)
			return;
		Route(killer, MakeEvent(LF_EQuestEventType.KILL, GetCharacterFaction(victim), 1));
	}

	//------------------------------------------------------------------------------------------------
	//! Recompute every collect objective of the player's active/completed quests
	//! from their live inventory. Server-generated reports are authoritative.
	void ReportCollect(IEntity user)
	{
		LF_QuestTrackerComponent tracker = ResolveTracker(user);
		if (!tracker)
			return;

		bool any = false;
		foreach (string questId, LF_QuestState state : tracker.GetTracker().GetStates())
		{
			if (state.GetStatus() != LF_EQuestStatus.ACTIVE && state.GetStatus() != LF_EQuestStatus.COMPLETED)
				continue;

			LF_QuestDefinition def = m_mDefinitions.Get(questId);
			if (!def)
				continue;

			for (int i = 0; i < def.GetObjectiveCount(); i++)
			{
				LF_QuestObjective objective = def.GetObjective(i);
				if (!objective || objective.m_eType != LF_EQuestEventType.COLLECT_REPORT)
					continue;

				int held = CountItem(user, objective.m_sFilter);
				LF_QuestEvent ev = MakeEvent(LF_EQuestEventType.COLLECT_REPORT, objective.m_sFilter, held);
				if (tracker.GetTracker().ApplyEvent(ev, m_mDefinitions))
					any = true;
			}
		}

		if (any)
			tracker.PushLog();
	}

	//------------------------------------------------------------------------------------------------
	//! How many of a prefab the player holds (root items + stack quantities).
	int CountItem(IEntity user, string prefab)
	{
		if (!user || prefab.IsEmpty())
			return 0;

		SCR_InventoryStorageManagerComponent storage = EL_Component<SCR_InventoryStorageManagerComponent>.Find(user);
		if (!storage)
			return 0;

		array<IEntity> rootItems();
		storage.GetAllRootItems(rootItems);

		int total = 0;
		foreach (IEntity item : rootItems)
		{
			if (!item || EL_Utils.GetPrefabName(item) != prefab)
				continue;

			int quantity = 1;
			EL_QuantityComponent quantityComponent = EL_Component<EL_QuantityComponent>.Find(item);
			if (quantityComponent)
				quantity = quantityComponent.GetQuantity();
			total += quantity;
		}
		return total;
	}

	//------------------------------------------------------------------------------------------------
	protected bool Route(IEntity user, LF_QuestEvent ev)
	{
		LF_QuestTrackerComponent tracker = ResolveTracker(user);
		if (!tracker)
			return false;

		bool changed = tracker.GetTracker().ApplyEvent(ev, m_mDefinitions);
		if (changed)
		{
			tracker.PushLog();
			LogEvent(ev);
		}
		return changed;
	}

	//------------------------------------------------------------------------------------------------
	protected LF_QuestTrackerComponent ResolveTracker(IEntity user)
	{
		if (!user)
			return null;
		return LF_QuestTrackerComponent.Cast(user.FindComponent(LF_QuestTrackerComponent));
	}

	//------------------------------------------------------------------------------------------------
	protected LF_QuestEvent MakeEvent(LF_EQuestEventType type, string filter, int value)
	{
		LF_QuestEvent ev = new LF_QuestEvent();
		ev.m_eType = type;
		ev.m_sFilter = filter;
		ev.m_iValue = value;
		return ev;
	}

	//------------------------------------------------------------------------------------------------
	protected void LogEvent(LF_QuestEvent ev)
	{
		EL_Debug.Log("Quests", string.Format("event type=%1 filter='%2' value=%3", typename.EnumToString(LF_EQuestEventType, ev.m_eType), ev.m_sFilter, ev.m_iValue));
	}
}
