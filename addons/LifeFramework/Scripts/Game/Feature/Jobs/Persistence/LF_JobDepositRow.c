//------------------------------------------------------------------------------------------------
//! Version 49 shape of a held cargo deposit (the "jobdeposits" payload, #2408), keyed by the player uid.
//! Kept only so a version 49 save still reads: from version 50 the payload is written empty and the
//! rows, with the job, are LF_JobRunRow in "jobruns" (#2481). LF_CivilianJob.RowsFromLegacy converts them.
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
