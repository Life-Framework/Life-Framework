//------------------------------------------------------------------------------------------------
//! One service call that is on the map, as the server remembers it to find it again when it is taken.
class LF_ServiceCallMapMark : Managed
{
	//! The share key, "dispatch:<n>".
	string m_sKey;
	string m_sService;
	string m_sCallerUid;
	string m_sPlace;
	string m_sSummary;
	int m_iX;
	int m_iZ;
	float m_fExpires;
	bool m_bTaken;
}

//------------------------------------------------------------------------------------------------
//! Dispatch marks (card M10, #2238; everon-map.md section 3 "Shared marks", D-M10). A raised police or
//! medic call becomes a ROLE shared mark for that service's on-duty players (LF_MapShares); a taken
//! call turns solid (the deep colour of its service); the mark leaves when the call leaves the board
//! (the same expiry) or the world reset clears the calls. Server only; nothing here is saved, a
//! restart clears calls and marks alike. A call with no position (zero) or for another service gets
//! no mark and says so in the log.
class LF_ServiceCallMapMarks
{
	static const string FEATURE = "ServiceCalls";
	static const string KEY_PREFIX = "dispatch:";
	//! Indexes into the vanilla placed-marker config (Configs/Map/MapMarkerConfig.conf): icon 0 is the
	//! first general icon; colours 8 cyan and 7 blue (police), 1 orange and 4 red (medic). Light while
	//! open, deep once taken.
	static const int ICON = 0; // lf-fixed: vanilla config index, not a server tunable
	static const int COLOUR_POLICE_OPEN = 8; // lf-fixed: vanilla config index
	static const int COLOUR_POLICE_TAKEN = 7; // lf-fixed: vanilla config index
	static const int COLOUR_MEDIC_OPEN = 1; // lf-fixed: vanilla config index
	static const int COLOUR_MEDIC_TAKEN = 4; // lf-fixed: vanilla config index

	protected static ref array<ref LF_ServiceCallMapMark> s_aMarks;
	protected static int s_iSequence;

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		LF_ServiceCallMapMarkSubscriber.ResetStatics();
		s_aMarks = null;
		s_iSequence = 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: listen to raised and taken calls (idempotent).
	static void Install()
	{
		if (!Replication.IsServer())
			return;

		LF_ServiceCallMapMarkSubscriber.Install();
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: the colour of a service's mark, open or taken; -1 for a service that gets no mark.
	static int ColourFor(string service, bool taken)
	{
		if (service == LF_MapShares.ROLE_POLICE)
		{
			if (taken)
				return COLOUR_POLICE_TAKEN;

			return COLOUR_POLICE_OPEN;
		}

		if (service == LF_MapShares.ROLE_MEDIC)
		{
			if (taken)
				return COLOUR_MEDIC_TAKEN;

			return COLOUR_MEDIC_OPEN;
		}

		return -1;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a call was raised. \return The share key, or "" when the call gets no mark.
	static string OnCall(notnull LF_OnServiceCall call)
	{
		if (ColourFor(call.m_sService, false) < 0)
		{
			LF_Debug.Info(FEATURE, string.Format("no map mark for the %1 call: only police and medic calls are marked", call.m_sService));
			return "";
		}

		if (call.m_vPosition == vector.Zero)
		{
			LF_Debug.Warn(FEATURE, string.Format("no map mark for the %1 call from '%2': the call has no position", call.m_sService, call.m_sPlace));
			return "";
		}

		Prune();
		s_iSequence++;
		LF_ServiceCallMapMark mark = new LF_ServiceCallMapMark();
		mark.m_sKey = KEY_PREFIX + s_iSequence.ToString();
		mark.m_sService = call.m_sService;
		mark.m_sCallerUid = call.m_sCallerUid;
		mark.m_sPlace = call.m_sPlace;
		mark.m_sSummary = call.m_sSummary;
		mark.m_iX = Math.Round(call.m_vPosition[0]);
		mark.m_iZ = Math.Round(call.m_vPosition[2]);
		mark.m_fExpires = LF_MapShares.Now() + LF_ServiceCallsConfig.BoardExpirySeconds();
		if (!Send(mark))
			return "";

		Marks().Insert(mark);
		return mark.m_sKey;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a call was taken. The event carries no call id, so the mark is the oldest one with the
	//! same caller, service, place and summary that is not taken yet (the board takes the oldest call
	//! with room too). \return The share key that turned taken, or "".
	static string OnTaken(notnull LF_OnServiceCallTaken taken)
	{
		Prune();
		// A first taker takes an open call; a later taker joins one already taken (police allow 2).
		bool wantTaken = taken.m_iTakers > 1;
		LF_ServiceCallMapMark found;
		LF_ServiceCallMapMark fallback;
		foreach (LF_ServiceCallMapMark mark : Marks())
		{
			if (mark.m_sService != taken.m_sService || mark.m_sCallerUid != taken.m_sCallerUid)
				continue;

			if (mark.m_sPlace != taken.m_sPlace || mark.m_sSummary != taken.m_sSummary)
				continue;

			if (!fallback)
				fallback = mark;

			if (mark.m_bTaken == wantTaken)
			{
				found = mark;
				break;
			}
		}

		if (!found)
			found = fallback;

		if (!found)
		{
			LF_Debug.Info(FEATURE, string.Format("taken %1 call from '%2' has no map mark", taken.m_sService, taken.m_sPlace));
			return "";
		}

		if (found.m_bTaken)
			return found.m_sKey;

		found.m_bTaken = true;
		Send(found);
		return found.m_sKey;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the calls closed (world reset). Removes every dispatch mark. \return How many went.
	static int CloseAll()
	{
		if (!s_aMarks)
			return 0;

		int closed;
		foreach (LF_ServiceCallMapMark mark : s_aMarks)
		{
			if (LF_MapShares.Remove(mark.m_sKey))
				closed++;
		}

		s_aMarks.Clear();
		if (closed > 0)
			LF_Debug.Info(FEATURE, string.Format("%1 dispatch map mark(s) closed", closed));

		return closed;
	}

	//------------------------------------------------------------------------------------------------
	//! Marks the server still tracks (an expired one leaves at the next call or take).
	static int Count()
	{
		Prune();
		return Marks().Count();
	}

	//------------------------------------------------------------------------------------------------
	protected static bool Send(notnull LF_ServiceCallMapMark mark)
	{
		LF_MapShareRow row = LF_MapShareRow.Make(mark.m_sKey, LF_EMapShareAudience.ROLE, mark.m_sService, mark.m_iX, mark.m_iZ, ICON, ColourFor(mark.m_sService, mark.m_bTaken), mark.m_sSummary, mark.m_fExpires);
		bool sent = LF_MapShares.Put(row);
		if (sent)
			LF_Debug.Info(FEATURE, string.Format("%1 call '%2' is map mark %3 (taken=%4)", mark.m_sService, mark.m_sPlace, mark.m_sKey, mark.m_bTaken));

		return sent;
	}

	//------------------------------------------------------------------------------------------------
	//! Forget marks the share table no longer holds (expired with the board).
	protected static void Prune()
	{
		if (!s_aMarks)
			return;

		for (int i = s_aMarks.Count() - 1; i >= 0; i--)
		{
			if (!LF_MapShares.Has(s_aMarks[i].m_sKey))
				s_aMarks.Remove(i);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected static array<ref LF_ServiceCallMapMark> Marks()
	{
		if (!s_aMarks)
			s_aMarks = {};

		return s_aMarks;
	}
}

//------------------------------------------------------------------------------------------------
//! Bus subscriber for raised and taken calls.
class LF_ServiceCallMapMarkSubscriber : LF_EventSubscriber
{
	protected static ref LF_ServiceCallMapMarkSubscriber s_Subscriber;

	//------------------------------------------------------------------------------------------------
	override bool Handle(notnull LF_Event busEvent)
	{
		LF_OnServiceCall call = LF_OnServiceCall.Cast(busEvent);
		if (call)
		{
			LF_ServiceCallMapMarks.OnCall(call);
			return true;
		}

		LF_OnServiceCallTaken taken = LF_OnServiceCallTaken.Cast(busEvent);
		if (taken)
		{
			LF_ServiceCallMapMarks.OnTaken(taken);
			return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	static void Install()
	{
		if (s_Subscriber)
		{
			LF_EventBus.Unsubscribe(LF_OnServiceCall, s_Subscriber);
			LF_EventBus.Unsubscribe(LF_OnServiceCallTaken, s_Subscriber);
		}

		s_Subscriber = new LF_ServiceCallMapMarkSubscriber();
		LF_EventBus.Subscribe(LF_OnServiceCall, s_Subscriber);
		LF_EventBus.Subscribe(LF_OnServiceCallTaken, s_Subscriber);
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		if (s_Subscriber)
		{
			LF_EventBus.Unsubscribe(LF_OnServiceCall, s_Subscriber);
			LF_EventBus.Unsubscribe(LF_OnServiceCallTaken, s_Subscriber);
		}

		s_Subscriber = null;
	}
}

//------------------------------------------------------------------------------------------------
//! Self-start: the server listens from the world's start.
modded class LF_GameModeRoleplay
{
	//------------------------------------------------------------------------------------------------
	protected override void OnGameStart()
	{
		super.OnGameStart();
		LF_ServiceCallMapMarks.Install();
	}
}

//------------------------------------------------------------------------------------------------
//! The world reset clears the calls (LF_WorldReset_Calls), so their marks close too.
modded class LF_WorldResetRegistry
{
	override void Collect(notnull array<ref LF_WorldResetStep> steps)
	{
		super.Collect(steps);
		steps.Insert(new LF_WorldReset_DispatchMarks());
	}
}

//------------------------------------------------------------------------------------------------
class LF_WorldReset_DispatchMarks : LF_WorldResetStep
{
	override string GetName()
	{
		return "dispatchmarks";
	}

	override bool IsCrimeState()
	{
		return true;
	}

	override int Run()
	{
		return LF_ServiceCallMapMarks.CloseAll();
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_ServiceCallMapMarks' statics (registered at script load; Core/LF_StaticReset.c).
class LF_ServiceCallMapMarks_StaticResetHook : LF_StaticResetHook
{
	protected static bool s_bRegistered = LF_StaticReset.Register(new LF_ServiceCallMapMarks_StaticResetHook());

	override string Owner()
	{
		return "LF_ServiceCallMapMarks";
	}

	override void Apply()
	{
		LF_ServiceCallMapMarks.ResetStatics();
	}
}
