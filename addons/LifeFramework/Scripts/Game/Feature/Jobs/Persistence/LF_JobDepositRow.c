//------------------------------------------------------------------------------------------------
//! Persisted cargo deposit held against one player's run in progress, keyed by the player uid (the
//! account key). Saved as the "jobdeposits" payload (save version 49) by
//! LF_PersistenceComponentSerializer. A run is session state and a restart ends it, so the load
//! gives every saved deposit back to the player's bank account (LF_CivilianJob.ApplyDeposits).
//------------------------------------------------------------------------------------------------
class LF_JobDepositRow : Managed
{
	string m_sPlayerUid;
	int m_iAmount;

	//------------------------------------------------------------------------------------------------
	static LF_JobDepositRow Create(string playerUid, int amount)
	{
		LF_JobDepositRow row();
		row.m_sPlayerUid = playerUid;
		row.m_iAmount = amount;
		return row;
	}
}
