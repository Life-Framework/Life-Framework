//------------------------------------------------------------------------------------------------
//! Persisted playtime of one account in one job or service (#692): the "playtime" payload (save
//! version 26) of LF_PersistenceComponentSerializer. Plain values, keyed by the account uid
//! (LF_Utils.GetAccountUID, not a character key) and the job name LF_Playtime.JobFor gives
//! (CIVILIAN, POLICE, MEDIC, PARCEL, ...). An account's total is the sum of its rows, so no total
//! row is stored.
//!
//! Positional in a binary save: never add, remove or reorder a field. A new field is a new payload.
class LF_PlaytimeRecord
{
	string m_sAccountUid;
	string m_sJob;
	int m_iSeconds;

	//------------------------------------------------------------------------------------------------
	static LF_PlaytimeRecord Create(string accountUid, string job, int seconds)
	{
		LF_PlaytimeRecord record = new LF_PlaytimeRecord();
		record.m_sAccountUid = accountUid;
		record.m_sJob = job;
		record.m_iSeconds = seconds;
		return record;
	}

	//------------------------------------------------------------------------------------------------
	LF_PlaytimeRecord Copy()
	{
		return Create(m_sAccountUid, m_sJob, m_iSeconds);
	}
}
