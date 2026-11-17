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
	//! Where the crime was filed (#2578), when the report knew it: the waypoint uses it before the site
	//! text, so a site named only by a map label still gets one. Session only, like the whole list.
	vector m_vPosition;
	bool m_bHasPosition;
	//! The lowest police rank that may take the call.
	int m_iMinRank;
	//! The officer who took it; empty while the call is open.
	string m_sOfficerUid;
	bool m_bClosed;
}

//------------------------------------------------------------------------------------------------
//! The server's list of police calls (#2477, scope card police-missions). A player's first wanted
//! report posts a FINE call carrying the report's place. A crime raises the wanted level before it
//! files its report (#2516), so a level rising from 0 with no report yet waits: the call is posted when
//! LF_OnWantedReportFiled arrives. A level back at 0 closes the suspect's open call (served time,
//! expiry, an arrest by another officer). An officer takes a call only on duty and at or above its
//! rank; one officer per call; Complete closes it once. Session only: the list is not saved. Nothing
//! here pays or marks a target; LF_PoliceCallPay does.
class LF_PoliceCalls
{
	static const string FEATURE = "PoliceCalls";
	static const string KIND_FINE = "FINE";
	//! Calls kept (open and closed). A full list drops its oldest closed call; with none, a new call is refused.
	static const int MAX_CALLS = 64;	// lf-fixed: a list bound, not gameplay

	protected static ref array<ref LF_PoliceCall> s_aCalls;
	//! Suspects whose wanted level rose before any report: their call is posted with the first report.
	protected static ref array<string> s_aWaiting;
	protected static int s_iNextId;
	protected static ref LF_PoliceCallsSubscriber s_Subscriber;

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_aCalls = null;
		s_aWaiting = null;
		s_iNextId = 0;
		if (s_Subscriber)
		{
			LF_EventBus.Unsubscribe(LF_OnWantedChanged, s_Subscriber);
			LF_EventBus.Unsubscribe(LF_OnWantedReportFiled, s_Subscriber);
		}

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
	protected static array<string> Waiting()
	{
		if (!s_aWaiting)
			s_aWaiting = new array<string>();

		return s_aWaiting;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: listens for wanted level changes and filed reports. Called when the game mode starts.
	static bool Install()
	{
		if (!Replication.IsServer())
			return false;

		if (s_Subscriber && LF_EventBus.IsSubscribed(LF_OnWantedChanged, s_Subscriber))
			return true;

		s_Subscriber = new LF_PoliceCallsSubscriber();
		bool wanted = LF_EventBus.Subscribe(LF_OnWantedChanged, s_Subscriber);
		bool reports = LF_EventBus.Subscribe(LF_OnWantedReportFiled, s_Subscriber);
		return wanted && reports;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a wanted level changed. A level back at 0 closes the suspect's open call. A rise from 0
	//! posts the call with the newest report's place, or waits for the report when none is filed yet.
	static void OnWantedChanged(LF_OnWantedChanged busEvent)
	{
		if (!busEvent)
			return;

		if (busEvent.m_iNewLevel <= 0)
		{
			Waiting().RemoveItem(busEvent.m_sPlayerUid);
			CloseFor(busEvent.m_sPlayerUid, "no longer wanted");
			return;
		}

		if (busEvent.m_iOldLevel > 0)
			return;

		array<ref LF_WantedReportRecord> reports = LF_WantedReports.GetFor(busEvent.m_sPlayerUid);
		if (!reports || reports.IsEmpty())
		{
			if (Waiting().Find(busEvent.m_sPlayerUid) < 0)
				Waiting().Insert(busEvent.m_sPlayerUid);

			LF_Debug.Info(FEATURE, string.Format("call for %1 waits for the crime report", busEvent.m_sPlayerUid));
			return;
		}

		LF_WantedReportRecord newest = reports[reports.Count() - 1];
		Post(KIND_FINE, busEvent.m_sPlayerUid, newest.m_sPlace, LF_PoliceRanks.PATROL, newest.m_bHasPosition, newest.m_vPosition);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a crime report was filed. Posts the call of a suspect whose wanted level rose first.
	static void OnReportFiled(LF_OnWantedReportFiled busEvent)
	{
		if (!busEvent || Waiting().Find(busEvent.m_sPlayerUid) < 0)
			return;

		Waiting().RemoveItem(busEvent.m_sPlayerUid);
		Post(KIND_FINE, busEvent.m_sPlayerUid, busEvent.m_sPlace, LF_PoliceRanks.PATROL, busEvent.m_bHasPosition, busEvent.m_vPosition);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: closes the suspect's open call without an officer completing it (the suspect is no longer
	//! wanted). Pays nothing; LF_PoliceCallPay pays only through Complete.
	//! eturn True when a call was open.
	static bool CloseFor(string suspectUid, string reason)
	{
		LF_PoliceCall call = FindOpenFor(suspectUid);
		if (!call)
			return false;

		call.m_bClosed = true;
		LF_Debug.Info(FEATURE, string.Format("call #%1 closed for %2: %3 (officer '%4')", call.m_iId, suspectUid, reason, call.m_sOfficerUid));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: posts a call. One open call per suspect: a second post for the same suspect returns the open one.
	//! \return The call, or null when the suspect is empty, the rank is off the ladder or the list is full.
	static LF_PoliceCall Post(string kind, string suspectUid, string site, int minRank, bool hasPosition = false, vector position = vector.Zero)
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
		call.m_bHasPosition = hasPosition;
		call.m_vPosition = position;
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
		LF_OnWantedReportFiled filed = LF_OnWantedReportFiled.Cast(busEvent);
		if (filed)
		{
			LF_PoliceCalls.OnReportFiled(filed);
			return true;
		}

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
