//------------------------------------------------------------------------------------------------
//! Jobs events on the server bus (docs/architecture/events.md). Fields are plain values: uids and
//! ints, never IEntity or EntityID. Published on the server only, after the state change happened.
//! A new event of this feature is appended to this file, not to a central list.

//! A job run was completed and paid.
class LF_OnJobCompleted : LF_Event
{
	string m_sPlayerUid;
	int m_iJob;
	int m_iPay;

	static LF_OnJobCompleted Create(string playerUid, int job, int pay)
	{
		LF_OnJobCompleted e = new LF_OnJobCompleted();
		e.m_sPlayerUid = playerUid;
		e.m_iJob = job;
		e.m_iPay = pay;
		return e;
	}
}
