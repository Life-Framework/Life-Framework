//------------------------------------------------------------------------------------------------
//! Level events on the server bus (docs/architecture/events.md). Fields are plain values: uids,
//! job ids and ints, never IEntity or EntityID. Published on the server only, after the state
//! change happened. A new event of this feature is appended to this file, not to a central list.

//! XP was added to a character's job (LF_JobLevels.AddXp, after the first-shift doubling and the daily
//! cap). m_iAmount is what was actually granted; m_sReason is what earned it ("quest ...", an action id).
class LF_OnJobXpAwarded : LF_Event
{
	string m_sPlayerUid;
	string m_sJob;
	int m_iAmount;
	string m_sReason;

	static LF_OnJobXpAwarded Create(string playerUid, string job, int amount, string reason)
	{
		LF_OnJobXpAwarded e = new LF_OnJobXpAwarded();
		e.m_sPlayerUid = playerUid;
		e.m_sJob = job;
		e.m_iAmount = amount;
		e.m_sReason = reason;
		return e;
	}
}

//! A character's level in one job rose (LF_JobLevels.AddXp, whatever granted the XP: an in-world
//! action, a quest, a debug grant). LF_JobLevelSync refreshes the replicated DELIVERY level.
class LF_OnJobLevelChanged : LF_Event
{
	string m_sPlayerUid;
	string m_sJob;
	int m_iLevelBefore;
	int m_iLevel;

	static LF_OnJobLevelChanged Create(string playerUid, string job, int levelBefore, int level)
	{
		LF_OnJobLevelChanged e = new LF_OnJobLevelChanged();
		e.m_sPlayerUid = playerUid;
		e.m_sJob = job;
		e.m_iLevelBefore = levelBefore;
		e.m_iLevel = level;
		return e;
	}
}
