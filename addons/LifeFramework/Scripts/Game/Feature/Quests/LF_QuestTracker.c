// LF_QuestTracker.c - per-player quest state holder. Pure logic: accepts quests,
// routes world events to objective progress, ticks deadlines. The world-facing
// LF_QuestTrackerComponent (script component on the character) wraps this and
// owns replication, persistence and reward application. Keeping the logic pure
// here is what makes the whole lifecycle testable in the fast tier.

class LF_QuestTracker
{
	protected ref map<string, ref LF_QuestState> m_mStates = new map<string, ref LF_QuestState>();

	//------------------------------------------------------------------------------------------------
	//! Whether a quest can be accepted right now.
	//! \param def The quest definition.
	//! \param level Player level.
	//! \param job Player job.
	//! \param faction Player faction key.
	//! \param now Current game time (cooldown/deadline basis).
	//! \param completedQuestIds Quest ids the player has turned in (previous-quest prerequisite).
	bool CanAccept(LF_QuestDefinition def, int level, EL_EJobType job, string faction, float now, array<string> completedQuestIds)
	{
		if (!def || def.m_sQuestId.IsEmpty())
			return false;

		if (def.m_iMinLevel > 0 && level < def.m_iMinLevel)
			return false;
		if (def.m_eRequiredJob != EL_EJobType.UNEMPLOYED && job != def.m_eRequiredJob)
			return false;
		if (!def.m_sRequiredFaction.IsEmpty() && faction != def.m_sRequiredFaction)
			return false;
		if (!def.m_sRequiredPreviousQuestId.IsEmpty() && !CompletedListContains(def.m_sRequiredPreviousQuestId, completedQuestIds))
			return false;

		LF_QuestState existing = m_mStates.Get(def.m_sQuestId);
		if (existing)
		{
			if (existing.GetStatus() == LF_EQuestStatus.ACTIVE || existing.GetStatus() == LF_EQuestStatus.COMPLETED)
				return false;
			return existing.CanReaccept(def.m_bRepeatable, def.m_fCooldownSeconds, now);
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Accept a quest. False when prerequisites fail or the cooldown is still running.
	bool Accept(LF_QuestDefinition def, int level, EL_EJobType job, string faction, float now, array<string> completedQuestIds)
	{
		if (!CanAccept(def, level, job, faction, now, completedQuestIds))
			return false;

		LF_QuestState state = m_mStates.Get(def.m_sQuestId);
		if (!state)
		{
			state = new LF_QuestState();
			m_mStates.Set(def.m_sQuestId, state);
		}
		state.Accept(def.m_sQuestId, def.m_fTimeLimitSeconds, now);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Route a world event to every active quest's matching objective. COMPLETED
	//! quests keep listening too: a SET report can regress a collect quest back to
	//! ACTIVE when the player no longer holds the items. The two-way recheck in
	//! LF_QuestState handles the transition; ADD objectives past the target are
	//! harmless and stay COMPLETED.
	//! \param defs Definitions by quest id, as collected from all quest givers.
	//! \return true when any quest's progress changed (caller logs/syncs the change).
	bool ApplyEvent(LF_QuestEvent questEvent, map<string, ref LF_QuestDefinition> defs)
	{
		if (!questEvent || !defs)
			return false;

		bool changed = false;
		foreach (string questId, LF_QuestState state : m_mStates)
		{
			if (state.GetStatus() != LF_EQuestStatus.ACTIVE && state.GetStatus() != LF_EQuestStatus.COMPLETED)
				continue;

			LF_QuestDefinition def = defs.Get(questId);
			if (!def)
				continue;

			for (int i = 0; i < def.GetObjectiveCount(); i++)
			{
				LF_QuestObjective objective = def.GetObjective(i);
				if (!objective || !objective.Matches(questEvent))
					continue;

				int before = state.GetObjectiveProgress(i);
				state.ApplyEvent(i, questEvent, def);
				if (state.GetObjectiveProgress(i) != before)
					changed = true;
			}
		}
		return changed;
	}

	//------------------------------------------------------------------------------------------------
	//! Advance game time: expire timed quests.
	void Tick(float now)
	{
		foreach (string questId, LF_QuestState state : m_mStates)
		{
			state.Tick(now);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Turn in a completed quest, recording the time. False when not completable.
	bool TurnIn(string questId, float now)
	{
		LF_QuestState state = m_mStates.Get(questId);
		if (!state)
			return false;
		if (state.GetStatus() != LF_EQuestStatus.COMPLETED)
			return false;
		state.TurnIn(now);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Chain steps ready to turn in on their own: COMPLETED quests marked m_bAutoTurnIn.
	void CollectAutoTurnIns(map<string, ref LF_QuestDefinition> defs, notnull array<string> outQuestIds)
	{
		outQuestIds.Clear();
		if (!defs)
			return;

		foreach (string questId, LF_QuestState state : m_mStates)
		{
			if (state.GetStatus() != LF_EQuestStatus.COMPLETED)
				continue;

			LF_QuestDefinition def = defs.Get(questId);
			if (def && def.m_bAutoTurnIn)
				outQuestIds.Insert(questId);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Chain steps to accept on their own: m_bAutoAccept quests this player has never started
	//! whose previous quest is turned in. A step already turned in (a returning player) or
	//! running is left alone.
	void CollectAutoAccepts(map<string, ref LF_QuestDefinition> defs, notnull array<string> outQuestIds)
	{
		outQuestIds.Clear();
		if (!defs)
			return;

		foreach (string questId, LF_QuestDefinition def : defs)
		{
			if (!def || !def.m_bAutoAccept || def.m_sRequiredPreviousQuestId.IsEmpty())
				continue;
			if (m_mStates.Contains(questId))
				continue;
			if (!HasTurnedIn(def.m_sRequiredPreviousQuestId))
				continue;

			outQuestIds.Insert(questId);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Persistence seam: marks a quest turned in when this tracker has no state for it. A live
	//! state (re-accepted, active) is kept, so applying twice changes nothing.
	//! \return true when this call added the turned-in state.
	bool RestoreTurnedIn(string questId)
	{
		if (questId.IsEmpty() || m_mStates.Contains(questId))
			return false;

		LF_QuestState state = new LF_QuestState();
		state.RestoreTurnedIn(questId);
		m_mStates.Set(questId, state);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	LF_QuestState GetState(string questId)
	{
		return m_mStates.Get(questId);
	}

	//------------------------------------------------------------------------------------------------
	//! Live state map (read-only use).
	map<string, ref LF_QuestState> GetStates()
	{
		return m_mStates;
	}

	//------------------------------------------------------------------------------------------------
	LF_EQuestStatus GetStatus(string questId)
	{
		LF_QuestState state = m_mStates.Get(questId);
		if (!state)
			return LF_EQuestStatus.AVAILABLE;
		return state.GetStatus();
	}

	//------------------------------------------------------------------------------------------------
	bool HasTurnedIn(string questId)
	{
		return GetStatus(questId) == LF_EQuestStatus.TURNED_IN;
	}

	//------------------------------------------------------------------------------------------------
	//! Quest ids the player has turned in (previous-quest prerequisite basis).
	array<string> GetCompletedQuestIds()
	{
		array<string> ids = {};
		foreach (string questId, LF_QuestState state : m_mStates)
		{
			if (state.GetStatus() == LF_EQuestStatus.TURNED_IN)
				ids.Insert(questId);
		}
		return ids;
	}

	//------------------------------------------------------------------------------------------------
	protected bool CompletedListContains(string questId, array<string> completedQuestIds)
	{
		if (!completedQuestIds)
			return false;
		foreach (string id : completedQuestIds)
		{
			if (id == questId)
				return true;
		}
		return false;
	}
}