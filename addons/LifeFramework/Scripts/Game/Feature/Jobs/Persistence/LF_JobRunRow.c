//------------------------------------------------------------------------------------------------
//! A cargo run in progress with its deposit, keyed by the player uid (the character record key).
//! Saved as the "jobruns" payload (save version 50, #2481) by LF_PersistenceComponentSerializer.
//! The job names the cargo the run handed out, so the load can take that cargo out of the player's
//! carried snapshot before it refunds the deposit (LF_CivilianJob.ApplyDeposits): a restart pays
//! the deposit back or leaves the cargo, never both. m_iJob is an LF_EJobType as an int;
//! LF_EJobType.UNEMPLOYED means "unknown" (a version 49 row, which did not name the job).
//------------------------------------------------------------------------------------------------
class LF_JobRunRow : Managed
{
	string m_sPlayerUid;
	int m_iAmount;
	int m_iJob;

	//------------------------------------------------------------------------------------------------
	static LF_JobRunRow Create(string playerUid, int amount, int job)
	{
		LF_JobRunRow row();
		row.m_sPlayerUid = playerUid;
		row.m_iAmount = amount;
		row.m_iJob = job;
		return row;
	}
}
