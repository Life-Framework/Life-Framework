//------------------------------------------------------------------------------------------------
//! Persisted delivery-job sign-up of one player, keyed by the player uid (the account key). Saved
//! as the "jobsignups" payload (save version 9) by EL_PersistenceComponentSerializer.
//!
//! The job is stored by EL_EJobType name, not by enum value, so a reordered enum drops an unknown
//! name instead of signing the player up for another job. An active run is not part of it.
//------------------------------------------------------------------------------------------------
class LF_JobSignUpRecord : Managed
{
	string m_sPlayerUid;
	string m_sJob;

	//------------------------------------------------------------------------------------------------
	static LF_JobSignUpRecord Create(string playerUid, string job)
	{
		LF_JobSignUpRecord record();
		record.m_sPlayerUid = playerUid;
		record.m_sJob = job;
		return record;
	}
}
