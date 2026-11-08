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

	//! Reentrancy guard: a turn-in inside SettleChain settles again at its end.
	protected bool m_bSettling;
	static const int MAX_CHAIN_PASSES = 16;
	static const string HANDOVER_SOUND = "SOUND_PICK_UP";

	//! How long a chain step's errand or the giver's turn-in line stays up: long enough to read.
	static const float STORY_SECONDS = 12.0;
	static const int GREET_DELAY_MS = 5000;

	//! How close (meters) a reader must stand to a sign for its read to count. The Read action
	//! shows within a few meters; the extra room covers stepping back while the card is open.
	static const float SIGN_READ_RADIUS = 6.0;

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
	//! \param chained True for a chain step accepted on its own: the player hears the next
	//! errand (the step's description) instead of the generic "accepted" hint.
	bool AcceptQuest(string questId, bool chained = false)
	{
		if (!Replication.IsServer())
			return false;

		LF_QuestManager manager = LF_QuestManager.GetInstance();
		LF_QuestDefinition def = manager.GetDefinition(questId);
		if (!def)
		{
			EL_Debug.Warn("Quests", string.Format("accept rejected quest=%1 (unknown id)", questId));
			Notify("#LF-Quest_Rejected", "#LF-Quest_Title");
			return false;
		}

		// Fail-safe: a quest with no objectives is a misconfiguration and must not be accepted.
		if (!def.HasObjectives())
		{
			EL_Debug.Error("Quests", string.Format("accept rejected quest=%1 (no objectives, misconfigured)", questId));
			return false;
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
			return false;
		}

		if (chained)
		{
			EL_Debug.Log("Quests", string.Format("accepted quest=%1 (chain step)", questId));
			Notify(def.m_sDescriptionKey, def.m_sTitleKey, STORY_SECONDS);
		}
		else
		{
			EL_Debug.Log("Quests", string.Format("accepted quest=%1", questId));
			Notify("#LF-Quest_Accepted", "#LF-Quest_Title");
		}

		PushLog();
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server, at spawn: a player who has not been handed their ID card yet hears where to get it
	//! (the card quest's description), once the HUD is up. Everyone else hears nothing.
	void GreetNewcomer()
	{
		if (!Replication.IsServer())
			return;

		GetGame().GetCallqueue().CallLater(DoGreetNewcomer, GREET_DELAY_MS);
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		if (GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(DoGreetNewcomer);

		super.OnDelete(owner);
	}

	//------------------------------------------------------------------------------------------------
	protected void DoGreetNewcomer()
	{
		foreach (string questId, LF_QuestDefinition def : LF_QuestManager.GetInstance().GetDefinitions())
		{
			if (!def || !def.m_bRewardIdCard)
				continue;
			if (m_Tracker.GetStatus(questId) != LF_EQuestStatus.AVAILABLE)
				continue;

			EL_Debug.Log("Quests", string.Format("newcomer pointed at quest=%1", questId));
			Notify(def.m_sDescriptionKey, def.m_sTitleKey, STORY_SECONDS);
			return;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Server: moves a chain along. Completed auto turn-in steps are turned in where the player
	//! stands, and auto-accept steps whose previous quest is turned in start. Runs after every
	//! event, turn-in, and restore; a restored mid-chain player gets the next step back.
	//! \return How many steps this call turned in or accepted.
	int SettleChain()
	{
		if (!Replication.IsServer() || m_bSettling)
			return 0;

		LF_QuestManager manager = LF_QuestManager.GetInstance();
		map<string, ref LF_QuestDefinition> defs = manager.GetDefinitions();
		m_bSettling = true;

		int moved = 0;
		array<string> questIds = {};
		// Each pass can open the next step; a chain longer than this is a misconfiguration loop.
		for (int pass = 0; pass < MAX_CHAIN_PASSES; pass++)
		{
			int before = moved;

			m_Tracker.CollectAutoTurnIns(defs, questIds);
			foreach (string turnInId : questIds)
			{
				if (TurnInQuest(turnInId))
					moved++;
			}

			m_Tracker.CollectAutoAccepts(defs, questIds);
			foreach (string acceptId : questIds)
			{
				if (AcceptQuest(acceptId, true))
					moved++;
			}

			if (moved == before)
				break;
		}

		m_bSettling = false;
		return moved;
	}

	//------------------------------------------------------------------------------------------------
	//! Server-side turn-in: authoritative collect recount, then reward.
	bool TurnInQuest(string questId)
	{
		if (!Replication.IsServer())
			return false;

		LF_QuestManager manager = LF_QuestManager.GetInstance();
		LF_QuestDefinition def = manager.GetDefinition(questId);
		if (!def)
			return false;

		LF_QuestState state = m_Tracker.GetState(questId);
		if (!state || state.GetStatus() != LF_EQuestStatus.COMPLETED)
		{
			EL_Debug.Log("Quests", string.Format("turn-in rejected quest=%1 (not completable)", questId));
			Notify("#LF-Quest_NotComplete", "#LF-Quest_Title");
			return false;
		}

		// Authoritative recount: the completion must hold against the live inventory.
		manager.ReportCollect(GetOwner());
		state = m_Tracker.GetState(questId);
		if (!state || state.GetStatus() != LF_EQuestStatus.COMPLETED)
		{
			EL_Debug.Log("Quests", string.Format("turn-in rejected quest=%1 (collect items no longer held)", questId));
			Notify("#LF-Quest_MissingItems", "#LF-Quest_Title");
			return false;
		}

		// Collect objectives hand the items over: take them before paying, all or nothing.
		if (!TakeCollectedItems(def))
		{
			Notify("#LF-Quest_MissingItems", "#LF-Quest_Title");
			return false;
		}

		GrantReward(def);
		m_Tracker.TurnIn(questId, GetGame().GetWorld().GetWorldTime() / 1000.0);
		LF_Progression.NoteTurnedIn(EL_Utils.GetPlayerUID(GetOwner()), questId);

		int rewardItems = 0;
		if (def.m_aRewardItems)
			rewardItems = def.m_aRewardItems.Count();
		EL_Debug.Log("Quests", string.Format("turned in quest=%1 reward=money:%2 xp:%3 items:%4 idcard:%5", questId, def.m_iRewardMoney, def.m_fRewardXp, rewardItems, def.m_bRewardIdCard));

		// A chain step turned in on its own stays quiet; the next step's errand is the news.
		if (!def.m_sTurnInKey.IsEmpty())
			Notify(def.m_sTurnInKey, def.m_sTitleKey, STORY_SECONDS);
		else if (!def.m_bAutoTurnIn)
			Notify("#LF-Quest_TurnedIn", "#LF-Quest_Title");

		PushLog();
		SettleChain();
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Removes every COLLECT_REPORT objective's target amount from the player. Checks all
	//! amounts first, then removes; a short removal puts back what was taken and refuses.
	//! \return True when every collected item was handed over (or the quest collects nothing).
	protected bool TakeCollectedItems(LF_QuestDefinition def)
	{
		map<string, int> needed = new map<string, int>();
		for (int i = 0; i < def.GetObjectiveCount(); i++)
		{
			LF_QuestObjective objective = def.GetObjective(i);
			if (!objective || objective.m_eType != LF_EQuestEventType.COLLECT_REPORT)
				continue;
			if (objective.m_sFilter.IsEmpty() || objective.m_iTarget <= 0)
				continue;

			needed.Set(objective.m_sFilter, needed.Get(objective.m_sFilter) + objective.m_iTarget);
		}

		if (needed.IsEmpty())
			return true;

		IEntity owner = GetOwner();
		if (!owner)
		{
			EL_Debug.Error("Quests", string.Format("turn-in refused quest=%1 (no owner to take items from)", def.m_sQuestId));
			return false;
		}

		foreach (string prefab, int amount : needed)
		{
			int held = EL_InventoryUtils.GetAmount(owner, prefab);
			if (held < amount)
			{
				EL_Debug.Warn("Quests", string.Format("turn-in refused quest=%1 (holds %2 of %3 %4)", def.m_sQuestId, held, amount, prefab));
				return false;
			}
		}

		map<string, int> taken = new map<string, int>();
		foreach (string prefab, int amount : needed)
		{
			int removed = EL_InventoryUtils.RemoveAmount(owner, prefab, amount);
			taken.Set(prefab, removed);
			if (removed >= amount)
				continue;

			foreach (string takenPrefab, int takenAmount : taken)
			{
				if (takenAmount > 0)
					EL_InventoryUtils.AddAmount(owner, takenPrefab, takenAmount, true);
			}

			EL_Debug.Error("Quests", string.Format("turn-in refused quest=%1 (removed %2 of %3 %4, items returned)", def.m_sQuestId, removed, amount, prefab));
			return false;
		}

		EL_Debug.Log("Quests", string.Format("turn-in took collected items for quest=%1", def.m_sQuestId));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected void GrantReward(LF_QuestDefinition def)
	{
		IEntity owner = GetOwner();
		if (!owner)
			return;

		if (def.m_iRewardMoney > 0)
			EL_MoneyUtils.GiveCash(owner, def.m_iRewardMoney);

		if (def.m_bRewardIdCard)
			LF_QuestRewards.HandIdCard(owner, LF_QuestRewards.AccountOf(owner));

		// A giver hands the reward over: the player hears it land in their pockets.
		if (def.m_iRewardMoney > 0 || def.m_bRewardIdCard || (def.m_aRewardItems && !def.m_aRewardItems.IsEmpty()))
			LF_PlayerInteraction.PlayHandoverSound(owner, HANDOVER_SOUND);

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
	//! \return Whether this character has turned the quest in.
	bool HasTurnedIn(string questId)
	{
		return m_Tracker.HasTurnedIn(questId);
	}

	//------------------------------------------------------------------------------------------------
	//! \return Quest ids this character has turned in.
	array<string> GetTurnedInQuestIds()
	{
		return m_Tracker.GetCompletedQuestIds();
	}

	//------------------------------------------------------------------------------------------------
	//! Persistence seam (LF_Progression): marks saved turn-ins on a fresh character. A quest id
	//! no giver defines any more is kept (its giver may come back) and logged.
	//! \return How many quests this call marked turned in.
	int RestoreTurnedIn(notnull array<string> questIds)
	{
		LF_QuestManager manager = LF_QuestManager.GetInstance();
		int restored = 0;
		foreach (string questId : questIds)
		{
			if (!m_Tracker.RestoreTurnedIn(questId))
				continue;

			restored++;
			if (manager && !manager.GetDefinition(questId))
				EL_Debug.Warn("Quests", string.Format("restored turn-in for quest=%1 that no giver defines", questId));
		}

		if (restored > 0)
		{
			EL_Debug.Log("Quests", string.Format("restored %1 turned-in quest(s)", restored));
			PushLog();
			// Only turn-ins persist: a player who left mid-chain gets the next step back.
			SettleChain();
		}

		return restored;
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
		if (Replication.IsServer())
		{
			RequestOpenGiver(giverId);
			return;
		}

		Rpc(RpcAsk_OpenGiver, giverId);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_OpenGiver(string giverId)
	{
		RequestOpenGiver(giverId);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a client opened a giver menu. The giver must be registered and within
	//! LF_QuestManager.GIVER_RADIUS, or a client could complete TALK objectives anywhere.
	//! \return True when the talk was reported.
	bool RequestOpenGiver(string giverId)
	{
		if (!Replication.IsServer())
			return false;

		LF_QuestManager manager = LF_QuestManager.GetInstance();
		if (!manager.IsNearGiver(GetOwner(), giverId))
		{
			EL_Debug.Warn("Quests", string.Format("open giver refused giver=%1 (unknown or not within %2 m)", giverId, LF_QuestManager.GIVER_RADIUS));
			return false;
		}

		manager.ReportTalk(GetOwner(), giverId);
		manager.ReportCollect(GetOwner());
		PushLog();
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: true when the caller stands at the giver that offers questId. Logs the refusal.
	protected bool IsAtQuestGiver(string questId, string verb)
	{
		LF_QuestManager manager = LF_QuestManager.GetInstance();
		string giverId = manager.GetGiverOfQuest(questId);
		if (manager.IsNearGiver(GetOwner(), giverId))
			return true;

		EL_Debug.Warn("Quests", string.Format("%1 refused quest=%2 (giver '%3' unknown or not within %4 m)", verb, questId, giverId, LF_QuestManager.GIVER_RADIUS));
		return false;
	}

	//------------------------------------------------------------------------------------------------
	void AskAccept(string questId)
	{
		if (Replication.IsServer())
		{
			RequestAccept(questId);
			return;
		}

		Rpc(RpcAsk_Accept, questId);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_Accept(string questId)
	{
		RequestAccept(questId);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: client-requested accept. Requires the quest's giver within reach.
	//! \return True when the quest is ACTIVE afterwards.
	bool RequestAccept(string questId)
	{
		if (!Replication.IsServer())
			return false;

		if (!IsAtQuestGiver(questId, "accept"))
			return false;

		if (!AcceptQuest(questId))
			return false;

		// The player is talking to the giver right now: a "talk to me" objective counts at once,
		// instead of asking them to close the menu and open it again.
		LF_QuestManager.GetInstance().ReportTalk(GetOwner(), LF_QuestManager.GetInstance().GetGiverOfQuest(questId));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Client: the local player read a sign. The server checks the sign stands beside them and
	//! reports the SIGNAL "sign.read:<id>".
	void AskSignRead(string signId)
	{
		if (Replication.IsServer())
		{
			RequestSignRead(signId);
			return;
		}

		Rpc(RpcAsk_SignRead, signId);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_SignRead(string signId)
	{
		RequestSignRead(signId);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a client says its player read signId. Counted only when a sign with that id
	//! stands within SIGN_READ_RADIUS, or a client could read the town board from anywhere.
	//! \return True when the read was reported.
	bool RequestSignRead(string signId)
	{
		if (!Replication.IsServer())
			return false;

		if (!LF_QuestSignals.IsSignNear(GetOwner(), signId, SIGN_READ_RADIUS))
		{
			EL_Debug.Warn("Quests", string.Format("sign read refused sign=%1 (no such sign within %2 m)", signId, SIGN_READ_RADIUS));
			return false;
		}

		LF_QuestManager.GetInstance().ReportSignal(GetOwner(), LF_QuestSignals.SignRead(signId));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	void AskTurnIn(string questId)
	{
		if (Replication.IsServer())
		{
			RequestTurnIn(questId);
			return;
		}

		Rpc(RpcAsk_TurnIn, questId);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_TurnIn(string questId)
	{
		RequestTurnIn(questId);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: client-requested turn-in. Requires the quest's giver within reach.
	//! \return True when the quest is TURNED_IN afterwards.
	bool RequestTurnIn(string questId)
	{
		if (!Replication.IsServer())
			return false;

		if (!IsAtQuestGiver(questId, "turn-in"))
			return false;

		return TurnInQuest(questId);
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
	protected void RpcDo_Notify(string message, string title, float duration)
	{
		EL_Utils.Notify(message, title, duration);
	}

	//------------------------------------------------------------------------------------------------
	protected void Notify(string message, string title, float duration = 4.0)
	{
		if (!Replication.IsServer())
			return;
		Rpc(RpcDo_Notify, message, title, duration);
	}
}
