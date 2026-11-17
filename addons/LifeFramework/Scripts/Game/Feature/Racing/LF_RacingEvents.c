//------------------------------------------------------------------------------------------------
//! Racing events on the server bus (docs/architecture/events.md). Fields are plain values: uids,
//! ids and ints, never IEntity or EntityID. Published on the server only, after the state change.
//! A new event of this feature is appended to this file, not to a central list.

//! A race finished and its pot was paid (#1923). The results board reads it (R4).
class LF_OnRaceFinished : LF_Event
{
	string m_sCourseId;
	string m_sWinnerUid;
	//! The whole pot that was paid out, in dollars.
	int m_iPot;
	//! Racers who started, leavers included.
	int m_iEntrants;
	//! The winner's time from the start, in milliseconds.
	int m_iWinnerMs;
	//! Every racer who finished, winner included, in finish order (#2160): uid and time from the start in
	//! milliseconds at the same index. A racer who left is not in it. The record board offers each pair.
	ref array<string> m_aFinisherUids = {};
	ref array<int> m_aFinisherMs = {};

	static LF_OnRaceFinished Create(string courseId, string winnerUid, int pot, int entrants, int winnerMs)
	{
		LF_OnRaceFinished e = new LF_OnRaceFinished();
		e.m_sCourseId = courseId;
		e.m_sWinnerUid = winnerUid;
		e.m_iPot = pot;
		e.m_iEntrants = entrants;
		e.m_iWinnerMs = winnerMs;
		return e;
	}

	//------------------------------------------------------------------------------------------------
	//! Adds one finisher (a uid and a time above 0). A blank uid or a time of 0 or less is skipped.
	void AddFinisher(string uid, int ms)
	{
		if (uid.IsEmpty() || ms <= 0)
		{
			LF_Debug.Warn("Racing", string.Format("finisher not added to the finish event: uid '%1' ms %2", uid, ms));
			return;
		}

		m_aFinisherUids.Insert(uid);
		m_aFinisherMs.Insert(ms);
	}
}
