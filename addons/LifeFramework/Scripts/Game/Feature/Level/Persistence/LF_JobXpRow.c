//------------------------------------------------------------------------------------------------
//! Persisted XP of one character in one job (#749): the "joblevels" payload (save version 28) of
//! LF_PersistenceComponentSerializer. One row per (character, job), keyed by the character record
//! key (LF_Utils.GetPlayerUID) and the job id of an LF_JobTrackDef (CIVILIAN, DELIVERY, FARMER, ...).
//! The level is never saved: LF_JobsConfig's thresholds derive it, so a rebalance reaches old saves.
//!
//! Positional in a binary save: never add, remove or reorder a field. A new field is a new payload.
class LF_JobXpRow
{
	string m_sPlayerUid;
	string m_sJob;
	int m_iXp;

	//------------------------------------------------------------------------------------------------
	static LF_JobXpRow Create(string playerUid, string job, int xp)
	{
		LF_JobXpRow row = new LF_JobXpRow();
		row.m_sPlayerUid = playerUid;
		row.m_sJob = job;
		row.m_iXp = xp;
		return row;
	}

	//------------------------------------------------------------------------------------------------
	LF_JobXpRow Copy()
	{
		return Create(m_sPlayerUid, m_sJob, m_iXp);
	}
}
