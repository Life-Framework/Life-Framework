//------------------------------------------------------------------------------------------------
//! One open or closed police call (#2477, parent #234). Plain values only: uids, a place text, ints.
class LF_PoliceCall : Managed
{
	int m_iId;
	string m_sKind;
	//! The character key the wanted reports use.
	string m_sSuspectUid;
	//! Where the suspect was reported: the report's place text (a sign title key or "grid X-Z"),
	//! read at runtime, never a coded coordinate.
	string m_sSite;
	//! The lowest police rank that may take the call.
	int m_iMinRank;
	//! The officer who took it; empty while the call is open.
	string m_sOfficerUid;
	bool m_bClosed;
}

//------------------------------------------------------------------------------------------------
//! The server's list of police calls (#2477, scope card police-missions). A player's first wanted
//! report (LF_OnWantedChanged from 0 to above 0) posts a FINE call. An officer takes a call only on
//! duty and at or above its rank; one officer per call; Complete closes it once. Session only: the
//! list is not saved. Nothing here pays or marks a target; later slices read the list.
class LF_PoliceCalls
{
	static const string FEATURE = "PoliceCalls";
	static const string KIND_FINE = "FINE";
	//! Calls kept (open and closed). A full list drops its oldest closed call; with none, a new call is refused.
	static const int MAX_CALLS = 64;	// lf-fixed: a list bound, not gameplay

	protected static ref array<ref LF_PoliceCall> s_aCalls;
	protected static int s_iNextId;
	protected static ref LF_PoliceCallsSubscriber s_Subscriber;

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_aCalls = null;
		s_iNextId = 0;
		if (s_Subscriber)
			LF_EventBus.Unsubscribe(LF_OnWantedChanged, s_Subscriber);

		s_Subscriber = null;
	}

	//------------------------------------------------------------------------------------------------
	protected static array<ref LF_PoliceCall> Calls()
	{
		if (!s_aCalls)
			s_aCalls = new array<ref LF_PoliceCall>();

		return s_aCalls;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: listens for a wanted level change. Called when the game mode starts.
	static bool Install()
	{
		if (!Replication.IsServer())
			return false;

		if (s_Subscriber && LF_EventBus.IsSubscribed(LF_OnWantedChanged, s_Subscriber))
			return true;

		s_Subscriber = new LF_PoliceCallsSubscriber();
		return LF_EventBus.Subscribe(LF_OnWantedChanged, s_Subscriber);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a wanted level changed. Only the first report (0 to above 0) posts a call.
	static void OnWantedChanged(LF_OnWantedChanged busEvent)
	{
		if (!busEvent || busEvent.m_iOldLevel > 0 || busEvent.m_iNewLevel <= 0)
			return;

		string site;
		array<ref LF_WantedReportRecord> reports = LF_WantedReports.GetFor(busEvent.m_sPlayerUid);
		if (reports && reports.Count() > 0)
			site = reports[reports.Count() - 1].m_sPlace;

		Post(KIND_FINE, busEvent.m_sPlayerUid, site, LF_PoliceRanks.PATROL);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: posts a call. One open call per suspect: a second post for the same suspect returns the open one.
	//! \return The call, or null when the suspect is empty, the rank is off the ladder or the list is full.
	static LF_PoliceCall Post(string kind, string suspectUid, string site, int minRank)
	{
		if (suspectUid.IsEmpty() || kind.IsEmpty())
		{
			LF_Debug.Warn(FEATURE, "call refused: no suspect or kind");
			return null;
		}

		if (!LF_PoliceRanks.IsRankUsable(minRank))
		{
			LF_Debug.Warn(FEATURE, string.Format("call for %1 refused: rank %2 is not on the ladder", suspectUid, minRank));
			return null;
		}

		LF_PoliceCall open = FindOpenFor(suspectUid);
		if (open)
			return open;

		array<ref LF_PoliceCall> calls = Calls();
		if (calls.Count() >= MAX_CALLS && !DropOldestClosed())
		{
			LF_Debug.Warn(FEATURE, string.Format("call for %1 refused: %2 calls are open", suspectUid, MAX_CALLS));
			return null;
		}

		s_iNextId++;
		LF_PoliceCall call = new LF_PoliceCall();
		call.m_iId = s_iNextId;
		call.m_sKind = kind;
		call.m_sSuspectUid = suspectUid;
		call.m_sSite = site;
		call.m_iMinRank = minRank;
		calls.Insert(call);
		LF_Debug.Info(FEATURE, string.Format("call #%1 %2 posted for %3 at '%4', rank %5", call.m_iId, kind, suspectUid, site, minRank));
		return call;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool DropOldestClosed()
	{
		array<ref LF_PoliceCall> calls = Calls();
		for (int i = 0; i < calls.Count(); i++)
		{
			if (calls[i].m_bClosed)
			{
				calls.Remove(i);
				return true;
			}
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	static LF_PoliceCall Get(int callId)
	{
		array<ref LF_PoliceCall> calls = Calls();
		for (int i = 0; i < calls.Count(); i++)
		{
			if (calls[i].m_iId == callId)
				return calls[i];
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! The open call (taken or not) for this suspect, or null.
	static LF_PoliceCall FindOpenFor(string suspectUid)
	{
		array<ref LF_PoliceCall> calls = Calls();
		for (int i = 0; i < calls.Count(); i++)
		{
			if (!calls[i].m_bClosed && calls[i].m_sSuspectUid == suspectUid)
				return calls[i];
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Calls not closed yet, taken or not.
	static int OpenCount()
	{
		int count;
		array<ref LF_PoliceCall> calls = Calls();
		for (int i = 0; i < calls.Count(); i++)
		{
			if (!calls[i].m_bClosed)
				count++;
		}

		return count;
	}

	//------------------------------------------------------------------------------------------------
	//! The oldest call nobody holds yet whose minimum rank this rank meets (#2479: the board's take), or null.
	static LF_PoliceCall OldestOpenFor(int rank)
	{
		array<ref LF_PoliceCall> calls = Calls();
		for (int i = 0; i < calls.Count(); i++)
		{
			if (!calls[i].m_bClosed && calls[i].m_sOfficerUid.IsEmpty() && rank >= calls[i].m_iMinRank)
				return calls[i];
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: an officer takes a call. The account state is a parameter (LF_AccountStateBits), so no
	//! character is needed. Refused when off duty, under the call's rank, the call is unknown or closed,
	//! or another officer has it.
	static bool Take(int callId, string officerUid, int accountState)
	{
		if (officerUid.IsEmpty())
		{
			LF_Debug.Warn(FEATURE, string.Format("take of call #%1 refused: no officer uid", callId));
			return false;
		}

		LF_PoliceCall call = Get(callId);
		if (!call || call.m_bClosed)
		{
			LF_Debug.Warn(FEATURE, string.Format("take of call #%1 by %2 refused: no open call", callId, officerUid));
			return false;
		}

		if (!LF_AccountStateBits.IsOnDutyPolice(accountState))
		{
			LF_Debug.Warn(FEATURE, string.Format("take of call #%1 by %2 refused: not on duty", callId, officerUid));
			return false;
		}

		int rank = LF_AccountStateBits.GetPoliceRank(accountState);
		if (rank < call.m_iMinRank)
		{
			LF_Debug.Warn(FEATURE, string.Format("take of call #%1 by %2 refused: rank %3 is under %4", callId, officerUid, rank, call.m_iMinRank));
			return false;
		}

		if (!call.m_sOfficerUid.IsEmpty())
		{
			LF_Debug.Warn(FEATURE, string.Format("take of call #%1 by %2 refused: %3 has it", callId, officerUid, call.m_sOfficerUid));
			return false;
		}

		call.m_sOfficerUid = officerUid;
		LF_Debug.Info(FEATURE, string.Format("call #%1 taken by %2", callId, officerUid));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the officer who took the call closes it. True once; false for a second close, an unknown
	//! call or an officer who does not hold it.
	static bool Complete(int callId, string officerUid)
	{
		LF_PoliceCall call = Get(callId);
		if (!call || call.m_bClosed)
		{
			LF_Debug.Warn(FEATURE, string.Format("complete of call #%1 by %2 refused: no open call", callId, officerUid));
			return false;
		}

		if (officerUid.IsEmpty() || call.m_sOfficerUid != officerUid)
		{
			LF_Debug.Warn(FEATURE, string.Format("complete of call #%1 by %2 refused: held by '%3'", callId, officerUid, call.m_sOfficerUid));
			return false;
		}

		call.m_bClosed = true;
		LF_Debug.Info(FEATURE, string.Format("call #%1 completed by %2", callId, officerUid));
		return true;
	}
}

//------------------------------------------------------------------------------------------------
class LF_PoliceCallsSubscriber : LF_EventSubscriber
{
	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "PoliceCalls";
	}

	//------------------------------------------------------------------------------------------------
	override bool Handle(notnull LF_Event busEvent)
	{
		LF_OnWantedChanged changed = LF_OnWantedChanged.Cast(busEvent);
		if (!changed)
			return false;

		LF_PoliceCalls.OnWantedChanged(changed);
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_PoliceCalls's statics (registered at script load; Core/LF_StaticReset.c).
class LF_PoliceCalls_StaticResetHook : LF_StaticResetHook
{
	protected static bool s_bRegistered = LF_StaticReset.Register(new LF_PoliceCalls_StaticResetHook());

	override string Owner()
	{
		return "LF_PoliceCalls";
	}

	override void Apply()
	{
		LF_PoliceCalls.ResetStatics();
	}
}
