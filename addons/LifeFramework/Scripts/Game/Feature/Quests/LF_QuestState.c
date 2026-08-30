// LF_QuestState.c - runtime state of a single accepted quest and its lifecycle
// transitions. Pure logic (no world): the tracker drives it from server events,
// and its owner (the character component) handles replication and persistence.

class LF_QuestState
{
	protected LF_EQuestStatus m_eStatus = LF_EQuestStatus.AVAILABLE;
	protected string m_sQuestId;
	protected ref map<int, int> m_mObjectiveProgress = new map<int, int>();
	protected float m_fAcceptedAt;
	protected float m_fDeadline = -1;
	protected float m_fLastTurnedInAt;

	//------------------------------------------------------------------------------------------------
	//! Begin (or restart) the quest at the given game time.
	void Accept(string questId, float timeLimitSeconds, float now)
	{
		m_sQuestId = questId;
		m_eStatus = LF_EQuestStatus.ACTIVE;
		m_fAcceptedAt = now;
		m_fLastTurnedInAt = 0;
		m_mObjectiveProgress.Clear();
		if (timeLimitSeconds > 0)
			m_fDeadline = now + timeLimitSeconds;
		else
			m_fDeadline = -1;
	}

	//------------------------------------------------------------------------------------------------
	//! Apply a world event to one objective. Returns the resulting status so the
	//! caller can log the transition (e.g. an objective just completed the quest).
	//! COMPLETED quests keep accepting events so a SET report can regress a
	//! collect quest back to ACTIVE when the player drops the items.
	LF_EQuestStatus ApplyEvent(int objectiveIndex, LF_QuestEvent questEvent, LF_QuestDefinition def)
	{
		if (m_eStatus != LF_EQuestStatus.ACTIVE && m_eStatus != LF_EQuestStatus.COMPLETED)
			return m_eStatus;
		if (!def)
			return m_eStatus;

		LF_QuestObjective objective = def.GetObjective(objectiveIndex);
		if (!objective)
			return m_eStatus;

		int progress = GetObjectiveProgress(objectiveIndex);
		int newProgress = objective.ApplyEvent(progress, questEvent);
		if (newProgress == progress)
			return m_eStatus;

		m_mObjectiveProgress.Set(objectiveIndex, newProgress);
		RecheckCompletion(def);
		return m_eStatus;
	}

	//------------------------------------------------------------------------------------------------
	//! Two-way completion check. ACTIVE -> COMPLETED when every objective is done;
	//! COMPLETED -> ACTIVE when a SET objective regresses (player dropped the items).
	//! A quest with no objectives can never complete: misconfiguration degrades to
	//! an uncompletable quest rather than an auto-complete.
	void RecheckCompletion(LF_QuestDefinition def)
	{
		if (!def || !def.HasObjectives())
			return;

		bool allDone = true;
		for (int i = 0; i < def.GetObjectiveCount(); i++)
		{
			LF_QuestObjective objective = def.GetObjective(i);
			if (!objective || !objective.IsComplete(GetObjectiveProgress(i)))
			{
				allDone = false;
				break;
			}
		}

		if (allDone && m_eStatus == LF_EQuestStatus.ACTIVE)
			m_eStatus = LF_EQuestStatus.COMPLETED;
		else if (!allDone && m_eStatus == LF_EQuestStatus.COMPLETED)
			m_eStatus = LF_EQuestStatus.ACTIVE;
	}

	//------------------------------------------------------------------------------------------------
	//! Expire a timed quest once its deadline passes.
	void Tick(float now)
	{
		if (m_eStatus == LF_EQuestStatus.ACTIVE && m_fDeadline > 0 && now >= m_fDeadline)
			m_eStatus = LF_EQuestStatus.FAILED;
	}

	//------------------------------------------------------------------------------------------------
	//! Claim the reward, recording the time as the cooldown basis.
	void TurnIn(float now)
	{
		if (m_eStatus != LF_EQuestStatus.COMPLETED)
			return;
		m_fLastTurnedInAt = now;
		m_eStatus = LF_EQuestStatus.TURNED_IN;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether this state allows re-accepting the quest right now.
	bool CanReaccept(bool repeatable, float cooldownSeconds, float now)
	{
		if (m_eStatus == LF_EQuestStatus.ACTIVE || m_eStatus == LF_EQuestStatus.COMPLETED)
			return false;
		if (!repeatable)
			return false;
		if (cooldownSeconds > 0 && m_fLastTurnedInAt > 0 && now < m_fLastTurnedInAt + cooldownSeconds)
			return false;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	int GetObjectiveProgress(int objectiveIndex)
	{
		int progress;
		if (m_mObjectiveProgress.Find(objectiveIndex, progress))
			return progress;
		return 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Numeric status value (log wire format + persistence).
	int GetStatusValue()
	{
		return m_eStatus;
	}

	//------------------------------------------------------------------------------------------------
	//! Live objective progress map (read-only use).
	map<int, int> GetProgressMap()
	{
		return m_mObjectiveProgress;
	}

	LF_EQuestStatus GetStatus() { return m_eStatus; }
	string GetQuestId() { return m_sQuestId; }
	float GetDeadline() { return m_fDeadline; }
	float GetLastTurnedInAt() { return m_fLastTurnedInAt; }
}