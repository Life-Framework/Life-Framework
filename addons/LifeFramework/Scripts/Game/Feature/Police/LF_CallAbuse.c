//------------------------------------------------------------------------------------------------
//! The low-priority mark of a call (#4421, PO-D2). Set when the call is posted for a caller who was
//! charged for false reports today; the dispatch board shows it. Session only, like the call list.
modded class LF_PoliceCall
{
	bool m_bLowPriority;
}

//------------------------------------------------------------------------------------------------
//! 911 abuse (#4421, PO-D2; plan N16). An officer who closes a call as unfounded (LF_CallOutcome,
//! #4203) counts one against the call's caller. The third unfounded close in one in-game day files
//! one false-report charge against the caller (the law book's emergency line abuse code, #4348;
//! it appears on the charge sheet, #4533) and marks the caller's next calls low priority until the
//! day rolls. Server only. The count lives in memory for the session and is not saved: a restart
//! starts the day clean. "The caller" of a police call is the uid the call carries
//! (LF_PoliceCall.m_sSuspectUid, the LF_OnServiceCall caller).
class LF_CallAbuse
{
	static const string FEATURE = "Police";
	//! The law book code the charge is filed under (pack law book, LF_CrimeCatalog_Everon1989.conf).
	static const string OFFENCE_FALSE_REPORT = "#LF-Ticket_OffenceEmergencyLineAbuse";
	//! The board's text for a marked call.
	static const string LOW_PRIORITY_KEY = "#LF-CallAbuse_LowPriority";
	//! Unfounded closes in one day that file the charge.
	static const int UNFOUNDED_LIMIT = 3;	// lf-fixed: the plan's three a day (N16), not a per-server tunable

	//! Unfounded closes today per caller uid.
	protected static ref map<string, int> s_mCounts;
	//! Callers charged today (marked low priority until the day rolls).
	protected static ref array<string> s_aCharged;
	//! The in-game day the counts belong to; -1 before the first count.
	protected static int s_iDay = -1;

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_mCounts = null;
		s_aCharged = null;
		s_iDay = -1;
	}

	//------------------------------------------------------------------------------------------------
	//! Today's lists. A new in-game day drops yesterday's counts and marks.
	protected static void Roll()
	{
		int today = LF_ConvictionRecord.Today();
		if (!s_mCounts)
			s_mCounts = new map<string, int>();

		if (!s_aCharged)
			s_aCharged = new array<string>();

		if (s_iDay != today)
		{
			if (s_iDay >= 0)
				LF_Debug.Info(FEATURE, string.Format("911 abuse counts reset: day %1 to %2", s_iDay, today));

			s_mCounts.Clear();
			s_aCharged.Clear();
			s_iDay = today;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Unfounded closes counted against the caller today.
	static int CountFor(string callerUid)
	{
		Roll();
		int count;
		if (s_mCounts.Find(callerUid, count))
			return count;

		return 0;
	}

	//------------------------------------------------------------------------------------------------
	//! True when the caller was charged today: their calls are posted low priority.
	static bool IsMarked(string callerUid)
	{
		Roll();
		return !callerUid.IsEmpty() && s_aCharged.Find(callerUid) >= 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: an officer closed this call as unfounded. Counts it against the caller; the third one
	//! today files the charge once and marks the caller. Closing your own call counts for nothing.
	//! \return True when this close filed the charge.
	static bool OnUnfounded(LF_PoliceCall call, string officerUid)
	{
		if (!Replication.IsServer() || !call)
			return false;

		string callerUid = call.m_sSuspectUid;
		if (callerUid.IsEmpty())
			return false;

		if (LF_DutyBonus.SameAccount(officerUid, callerUid))
		{
			LF_Debug.Info(FEATURE, string.Format("unfounded call #%1 not counted: %2 closed their own call", call.m_iId, callerUid));
			return false;
		}

		Roll();
		int count = CountFor(callerUid) + 1;
		s_mCounts.Set(callerUid, count);
		LF_Debug.Info(FEATURE, string.Format("911 abuse: call #%1 of %2 closed unfounded, %3 of %4 today", call.m_iId, callerUid, count, UNFOUNDED_LIMIT));
		if (count != UNFOUNDED_LIMIT)
			return false;

		return Charge(callerUid, call.m_sSite);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: files the false-report charge against the caller and marks them low priority.
	protected static bool Charge(string callerUid, string place)
	{
		LF_WantedReportRecord report = LF_WantedReports.Add(callerUid, OFFENCE_FALSE_REPORT, "", "", place, true);
		if (!report)
		{
			LF_Debug.Error(FEATURE, string.Format("911 abuse: the false-report charge against %1 was not filed", callerUid));
			return false;
		}

		s_aCharged.Insert(callerUid);
		LF_Debug.Info(FEATURE, string.Format("911 abuse: %1 charged (report #%2 %3); their calls are low priority until the day rolls", callerUid, report.m_iSequence, OFFENCE_FALSE_REPORT));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: marks a freshly posted call low priority when its caller was charged today.
	static void MarkIfAbuser(LF_PoliceCall call)
	{
		if (!call || call.m_bLowPriority || !IsMarked(call.m_sSuspectUid))
			return;

		call.m_bLowPriority = true;
		LF_Debug.Info(FEATURE, string.Format("call #%1 of %2 posted low priority: charged for false reports today", call.m_iId, call.m_sSuspectUid));
	}

	//------------------------------------------------------------------------------------------------
	//! The board's line for a call: the low-priority tag, or empty for a normal call.
	static string BoardTag(LF_PoliceCall call)
	{
		if (call && call.m_bLowPriority)
			return LOW_PRIORITY_KEY;

		return string.Empty;
	}
}

//------------------------------------------------------------------------------------------------
//! A call posted for a charged caller carries the low-priority mark from the start.
modded class LF_PoliceCalls
{
	override static LF_PoliceCall Post(string kind, string suspectUid, string site, int minRank, bool hasPosition = false, vector position = vector.Zero)
	{
		LF_PoliceCall call = super.Post(kind, suspectUid, site, minRank, hasPosition, position);
		LF_CallAbuse.MarkIfAbuser(call);
		return call;
	}
}

//------------------------------------------------------------------------------------------------
//! An unfounded close counts against the caller (the outcome is recorded by LF_CallOutcome.Close).
modded class LF_CallOutcome
{
	override static bool Close(int callId, string officerUid, LF_CallOutcomeKind outcome, vector officerPosition, out string messageKey, out string messageArg)
	{
		bool closed = super.Close(callId, officerUid, outcome, officerPosition, messageKey, messageArg);
		if (closed && outcome == LF_CallOutcomeKind.UNFOUNDED)
			LF_CallAbuse.OnUnfounded(LF_PoliceCalls.Get(callId), officerUid);

		return closed;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_CallAbuse's statics (LF_StaticReset finds it by reflection).
class LF_CallAbuse_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_CallAbuse";
	}

	override void Apply()
	{
		LF_CallAbuse.ResetStatics();
	}
}
