//------------------------------------------------------------------------------------------------
//! Level events on the server bus (docs/architecture/events.md). Fields are plain values: uids,
//! job ids and ints, never IEntity or EntityID. Published on the server only, after the state
//! change happened. A new event of this feature is appended to this file, not to a central list.

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
