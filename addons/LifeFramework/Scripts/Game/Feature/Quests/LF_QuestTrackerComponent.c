// LF_QuestTrackerComponent.c - per-player quest state on the character.
//
// Wraps the pure LF_QuestTracker and owns everything world-facing:
//   - server-side accept/turn-in with prerequisite and reward logic,
//   - an authoritative collect recount at turn-in (never trusts a client count),
//   - per-character FRAME tick for timed expiry and area-entry detection,
//   - quest-log pushes to the owning client (RpcDo_Log) and result hints.
// All state changes flow through EL_Debug.Log("Quests", ...).

[ComponentEditorProps(category: "LifeFramework/Feature/Quests", description: "Per-player quest tracker attached to the character")]
class LF_QuestTrackerComponentClass : ScriptComponentClass
{
}

class LF_QuestTrackerComponent : ScriptComponent
{
	protected ref LF_QuestTracker m_Tracker = new LF_QuestTracker();
	protected ref map<string, bool> m_mInsideAreas = new map<string, bool>();
	protected float m_fTickAccumulator;
	protected string m_sClientLogCache;

	protected ref ScriptInvoker m_OnLogReceived;

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		if (!Replication.IsServer())
			return;

		SetEventMask(owner, EntityEvent.INIT | EntityEvent.FRAME);
	}

	//------------------------------------------------------------------------------------------------
	override void EOnFrame(IEntity owner, float timeSlice)
	{
		if (!Replication.IsServer())
			return;

		m_fTickAccumulator += timeSlice;
		if (m_fTickAccumulator < 0.5)
			return;
		m_fTickAccumulator = 0;

		LF_QuestManager manager = LF_QuestManager.GetInstance();
		float now = GetGame().GetWorld().GetWorldTime() / 1000.0;

		TickTimedQuests(now);
		CheckAreas(manager, owner);
	}

	//------------------------------------------------------------------------------------------------
	//! Expire timed quests, notifying and pushing the log once per expiry.
	protected void TickTimedQuests(float now)
	{
		map<string, int> before = new map<string, int>();
		foreach (string questId, LF_QuestState state : m_Tracker.GetStates())
			before.Set(questId, state.GetStatusValue());

		m_Tracker.Tick(now);

		bool anyExpired = false;
		foreach (string questId, LF_QuestState state : m_Tracker.GetStates())
		{
			int prior;
			if (!before.Find(questId, prior))
				continue;
			if (prior == LF_EQuestStatus.ACTIVE && state.GetStatus() == LF_EQuestStatus.FAILED)
			{
				anyExpired = true;
				EL_Debug.Log("Quests", string.Format("quest=%1 expired", questId));
				Notify("#LF-Quest_Expired", "#LF-Quest_Title");
			}
		}
		if (anyExpired)
			PushLog();
	}

	//------------------------------------------------------------------------------------------------
	//! Fires AREA_ENTER once per entry (edge detection): progress must not grow
	//! while the player simply stands inside the area.
	protected void CheckAreas(LF_QuestManager manager, IEntity owner)
	{
		if (!manager || !owner)
			return;

		map<string, ref LF_QuestDefinition> defs = manager.GetDefinitions();
		vector playerPos = owner.GetOrigin();

		foreach (string questId, LF_QuestState state : m_Tracker.GetStates())
		{
			if (state.GetStatus() != LF_EQuestStatus.ACTIVE && state.GetStatus() != LF_EQuestStatus.COMPLETED)
				continue;

			LF_QuestDefinition def = defs.Get(questId);
			if (!def)
				continue;

			for (int i = 0; i < def.GetObjectiveCount(); i++)
			{
				LF_QuestObjective objective = def.GetObjective(i);
				if (!objective || objective.m_eType != LF_EQuestEventType.AREA_ENTER || objective.m_fRadius <= 0)
					continue;

				string key = questId + "#" + i.ToString();
				bool inside = vector.Distance(playerPos, objective.m_vPosition) <= objective.m_fRadius;

				bool wasInside = false;
				if (!m_mInsideAreas.Find(key, wasInside))
				{
					m_mInsideAreas.Set(key, inside);
					if (inside)
						manager.ReportArea(owner, objective.m_sFilter);
					continue;
				}

				if (inside && !wasInside)
				{
					m_mInsideAreas.Set(key, true);
					manager.ReportArea(owner, objective.m_sFilter);
				}
				else if (!inside)
				{
					m_mInsideAreas.Set(key, false);
				}
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Recompute every collect objective from the player's actual inventory
	//! (server-generated, authoritative) and push the log if anything changed.
	//! Called on menu open (live display) and before turn-in (completion gate).
	void RecountCollect()
	{
		if (!Replication.IsServer())
			return;
		LF_QuestManager.GetInstance().ReportCollect(GetOwner());
	}

	//------------------------------------------------------------------------------------------------
	//! Server-side accept: prerequisite snapshot, fail-safe guards, notification.
	void AcceptQuest(string questId)
	{
		if (!Replication.IsServer())
			return;

		LF_QuestManager manager = LF_QuestManager.GetInstance();
		LF_QuestDefinition def = manager.GetDefinition(questId);
		if (!def)
		{
			EL_Debug.Warn("Quests", string.Format("accept rejected quest=%1 (unknown id)", questId));
			Notify("#LF-Quest_Rejected", "#LF-Quest_Title");
			return;
		}

		// Fail-safe: a quest with no objectives is a misconfiguration and must not be accepted.
		if (!def.HasObjectives())
		{
			EL_Debug.Error("Quests", string.Format("accept rejected quest=%1 (no objectives, misconfigured)", questId));
			return;
		}

		IEntity owner = GetOwner();
		int level = 1;
		EL_PlayerLevelComponent levelComp = EL_Component<EL_PlayerLevelComponent>.Find(owner);
		if (levelComp)
			level = levelComp.GetPlayerLevel();

		EL_EJobType job = EL_EJobType.UNEMPLOYED;
		EL_PlayerJobComponent jobComp = EL_Component<EL_PlayerJobComponent>.Find(owner);
		if (jobComp)
			job = jobComp.GetJob();

		string faction = manager.GetCharacterFaction(owner);
		float now = GetGame().GetWorld().GetWorldTime() / 1000.0;

		if (!m_Tracker.Accept(def, level, job, faction, now, m_Tracker.GetCompletedQuestIds()))
		{
			EL_Debug.Log("Quests", string.Format("accept rejected quest=%1 (prerequisites or cooldown)", questId));
			Notify("#LF-Quest_Rejected", "#LF-Quest_Title");
			return;
		}

		EL_Debug.Log("Quests", string.Format("accepted quest=%1", questId));
		Notify("#LF-Quest_Accepted", "#LF-Quest_Title");
		PushLog();
	}

	//------------------------------------------------------------------------------------------------
	//! Server-side turn-in: authoritative collect recount, then reward.
	void TurnInQuest(string questId)
	{
		if (!Replication.IsServer())
			return;

		LF_QuestManager manager = LF_QuestManager.GetInstance();
		LF_QuestDefinition def = manager.GetDefinition(questId);
		if (!def)
			return;

		LF_QuestState state = m_Tracker.GetState(questId);
		if (!state || state.GetStatus() != LF_EQuestStatus.COMPLETED)
		{
			EL_Debug.Log("Quests", string.Format("turn-in rejected quest=%1 (not completable)", questId));
			Notify("#LF-Quest_NotComplete", "#LF-Quest_Title");
			return;
		}

		// Authoritative recount: the completion must hold against the live inventory.
		manager.ReportCollect(GetOwner());
		state = m_Tracker.GetState(questId);
		if (!state || state.GetStatus() != LF_EQuestStatus.COMPLETED)
		{
			EL_Debug.Log("Quests", string.Format("turn-in rejected quest=%1 (collect items no longer held)", questId));
			Notify("#LF-Quest_MissingItems", "#LF-Quest_Title");
			return;
		}

		GrantReward(def);
		m_Tracker.TurnIn(questId, GetGame().GetWorld().GetWorldTime() / 1000.0);

		int rewardItems = 0;
		if (def.m_aRewardItems)
			rewardItems = def.m_aRewardItems.Count();
		EL_Debug.Log("Quests", string.Format("turned in quest=%1 reward=money:%2 xp:%3 items:%4", questId, def.m_iRewardMoney, def.m_fRewardXp, rewardItems));
		Notify("#LF-Quest_TurnedIn", "#LF-Quest_Title");
		PushLog();
	}

	//------------------------------------------------------------------------------------------------
	protected void GrantReward(LF_QuestDefinition def)
	{
		IEntity owner = GetOwner();
		if (!owner)
			return;

		if (def.m_iRewardMoney > 0)
		{
			EL_BankAccountComponent bankAccount = EL_Component<EL_BankAccountComponent>.Find(owner);
			if (bankAccount)
				bankAccount.Deposit(def.m_iRewardMoney, "Quest reward");
			else
				EL_MoneyUtils.GiveCash(owner, def.m_iRewardMoney);
		}

		if (def.m_fRewardXp > 0)
		{
			EL_PlayerLevelComponent levelComp = EL_Component<EL_PlayerLevelComponent>.Find(owner);
			if (levelComp)
				levelComp.AddExperience(def.m_fRewardXp, "Quest");
		}

		if (def.m_aRewardItems)
		{
			SCR_InventoryStorageManagerComponent inventory = EL_Component<SCR_InventoryStorageManagerComponent>.Find(owner);
			if (!inventory)
				return;

			EntitySpawnParams spawnParams();
			spawnParams.Transform[3] = owner.GetOrigin();

			foreach (ResourceName prefab : def.m_aRewardItems)
			{
				if (prefab.IsEmpty())
					continue;

				IEntity item = GetGame().SpawnEntityPrefabEx(prefab, false, null, spawnParams);
				if (!item)
				{
					EL_Debug.Error("Quests", string.Format("reward item failed to spawn: %1", prefab));
					continue;
				}

				if (!inventory.TryInsertItem(item))
				{
					EL_Debug.Warn("Quests", string.Format("reward item dropped near player (inventory full): %1", prefab));
				}
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Push the current quest log to the owning client (server only).
	void PushLog()
	{
		if (!Replication.IsServer())
			return;
		Rpc(RpcDo_Log, LF_QuestLogFormat.Encode(m_Tracker.GetStates()));
	}

	//------------------------------------------------------------------------------------------------
	//! Client cache read by the quest menu.
	string GetClientLogCache()
	{
		return m_sClientLogCache;
	}

	//------------------------------------------------------------------------------------------------
	ScriptInvoker GetOnLogReceived()
	{
		if (!m_OnLogReceived)
			m_OnLogReceived = new ScriptInvoker();
		return m_OnLogReceived;
	}

	//------------------------------------------------------------------------------------------------
	LF_QuestTracker GetTracker()
	{
		return m_Tracker;
	}

	//------------------------------------------------------------------------------------------------
	//! Client asks the server to (re)report the giver interaction, recount
	//! collect objectives and push the fresh log - the open of a giver menu is
	//! what completes TALK objectives.
	void AskOpenGiver(string giverId)
	{
		Rpc(RpcAsk_OpenGiver, giverId);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_OpenGiver(string giverId)
	{
		LF_QuestManager manager = LF_QuestManager.GetInstance();
		manager.ReportTalk(GetOwner(), giverId);
		manager.ReportCollect(GetOwner());
		PushLog();
	}

	//------------------------------------------------------------------------------------------------
	void AskAccept(string questId)
	{
		Rpc(RpcAsk_Accept, questId);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_Accept(string questId)
	{
		AcceptQuest(questId);
	}

	//------------------------------------------------------------------------------------------------
	void AskTurnIn(string questId)
	{
		Rpc(RpcAsk_TurnIn, questId);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_TurnIn(string questId)
	{
		TurnInQuest(questId);
	}

	//------------------------------------------------------------------------------------------------
	void AskRequestLog()
	{
		Rpc(RpcAsk_RequestLog);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_RequestLog()
	{
		PushLog();
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_Log(string serialized)
	{
		m_sClientLogCache = serialized;
		if (m_OnLogReceived)
			m_OnLogReceived.Invoke(serialized);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_Notify(string message, string title)
	{
		EL_Utils.Notify(message, title, 4.0);
	}

	//------------------------------------------------------------------------------------------------
	protected void Notify(string message, string title)
	{
		if (!Replication.IsServer())
			return;
		Rpc(RpcDo_Notify, message, title);
	}
}