//------------------------------------------------------------------------------------------------
//! A taken police call completes when its officer fines the call's suspect at the call's site, and
//! pays the officer the call reward in cash once (#2478, parent #234). Server only.
//!
//! The site: LF_PoliceCall keeps the report's place as text, so this keeps a position per suspect.
//! A crime raises the wanted level before its report is filed, so the position comes from the police
//! service call the report publishes (the alarm site, or the suspect's body when there is none). When
//! the report came first, the first wanted report keeps the body's position if it stands where the
//! report's place text names. A wanted level dropping to 0, or rising from 0, forgets the kept site.
//! A call with no kept position falls back to the centre of its "grid X-Z" square; any other place
//! text cannot be resolved and the fine completes nothing.
//! Exactly once: the cash goes in first and is taken back when it did not all fit; the call is then
//! closed by LF_PoliceCalls.Complete, which closes it once.
class LF_PoliceCallPay
{
	static const string FEATURE = "PoliceCallPay";
	//! Ledger reason of the reward (LF_MoneyUtils).
	static const string REASON = "police-call";
	static const int DEFAULT_REWARD = 150;	// lf-fixed: fallback when the police config is missing; mirrors the m_iCallReward default
	static const float DEFAULT_RADIUS = 50;	// lf-fixed: fallback when the police config is missing; mirrors the m_fCallRadius default
	//! Metres of one "grid X-Z" square (LF_WantedReports.GridPlace).
	static const float GRID_SQUARE = 100;	// lf-fixed: mirrors LF_WantedReports.GridPlace
	//! Half the diagonal of a grid square: a grid site reaches its corners.
	static const float GRID_REACH = 71;	// lf-fixed: geometry of GRID_SQUARE

	protected static ref map<string, vector> s_mSites;
	protected static ref LF_PoliceCallPaySubscriber s_Subscriber;

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_mSites = null;
		if (s_Subscriber)
		{
			LF_EventBus.Unsubscribe(LF_OnFineIssued, s_Subscriber);
			LF_EventBus.Unsubscribe(LF_OnWantedChanged, s_Subscriber);
			LF_EventBus.Unsubscribe(LF_OnServiceCall, s_Subscriber);
		}

		s_Subscriber = null;
	}

	//------------------------------------------------------------------------------------------------
	protected static map<string, vector> Sites()
	{
		if (!s_mSites)
			s_mSites = new map<string, vector>();

		return s_mSites;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: listens for fines, wanted changes and police service calls. Called when the game mode starts.
	static bool Install()
	{
		if (!Replication.IsServer())
			return false;

		if (s_Subscriber && LF_EventBus.IsSubscribed(LF_OnFineIssued, s_Subscriber))
			return true;

		s_Subscriber = new LF_PoliceCallPaySubscriber();
		bool fines = LF_EventBus.Subscribe(LF_OnFineIssued, s_Subscriber);
		bool wanted = LF_EventBus.Subscribe(LF_OnWantedChanged, s_Subscriber);
		bool reports = LF_EventBus.Subscribe(LF_OnServiceCall, s_Subscriber);
		return fines && wanted && reports;
	}

	//------------------------------------------------------------------------------------------------
	//! Cash the officer gets for a completed call (police config callReward).
	static int Reward()
	{
		LF_PoliceConfig config = LF_PoliceConfig.Get();
		if (!config)
			return DEFAULT_REWARD;

		return Math.Max(config.m_iCallReward, 0);
	}

	//------------------------------------------------------------------------------------------------
	//! Metres from the call's site within which the fine completes it (police config callRadius).
	static float Radius()
	{
		LF_PoliceConfig config = LF_PoliceConfig.Get();
		if (!config)
			return DEFAULT_RADIUS;

		return Math.Max(config.m_fCallRadius, 0);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: keeps position as the site of this suspect's calls until one completes or the
	//! wanted level clears.
	static void RecordSite(string suspectUid, vector position)
	{
		if (suspectUid.IsEmpty())
			return;

		Sites().Set(suspectUid, position);
	}

	//------------------------------------------------------------------------------------------------
	static bool HasSite(string suspectUid)
	{
		return Sites().Contains(suspectUid);
	}

	//------------------------------------------------------------------------------------------------
	//! The site position of call and the radius around it, from the kept position or its grid square.
	//! \return False when the site text names no position this can resolve.
	static bool SiteOf(notnull LF_PoliceCall call, out vector position, out float radius)
	{
		radius = Radius();
		if (Sites().Find(call.m_sSuspectUid, position))
			return true;

		if (!GridCentre(call.m_sSite, position))
			return false;

		radius = Math.Max(radius, GRID_REACH);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: the centre of a "grid X-Z" place (LF_WantedReports.GridPlace); y is 0.
	static bool GridCentre(string place, out vector position)
	{
		string prefix = LF_WantedReports.PLACE_GRID_KEY + " ";
		if (!place.StartsWith(prefix))
			return false;

		array<string> parts = {};
		place.Substring(prefix.Length(), place.Length() - prefix.Length()).Split("-", parts, false);
		if (parts.Count() != 2 || parts[0].IsEmpty() || parts[1].IsEmpty())
			return false;

		position = Vector((parts[0].ToInt() + 0.5) * GRID_SQUARE, 0, (parts[1].ToInt() + 0.5) * GRID_SQUARE);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a wanted level that clears or starts forgets the kept site. When the report was filed
	//! before the level rose, the body's position is kept if it stands where the report's place names.
	static void OnWantedChanged(LF_OnWantedChanged busEvent)
	{
		if (!busEvent)
			return;

		if (busEvent.m_iNewLevel <= 0)
		{
			Sites().Remove(busEvent.m_sPlayerUid);
			return;
		}

		if (busEvent.m_iOldLevel > 0)
			return;

		Sites().Remove(busEvent.m_sPlayerUid);
		array<ref LF_WantedReportRecord> reports = LF_WantedReports.GetFor(busEvent.m_sPlayerUid);
		if (!reports || reports.IsEmpty())
			return;

		LF_PlayerAccount account;
		IEntity body = LF_PoliceUtils.FindPlayerEntityByUid(busEvent.m_sPlayerUid, account);
		if (!body)
			return;

		vector origin = body.GetOrigin();
		string place = reports[reports.Count() - 1].m_sPlace;
		if (LF_WantedReports.DescribePlace(origin) != place)
		{
			LF_Debug.Info(FEATURE, string.Format("site of %1 not kept: the body is not at '%2'", busEvent.m_sPlayerUid, place));
			return;
		}

		RecordSite(busEvent.m_sPlayerUid, origin);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a crime report went out as a police service call. The first one for a reported suspect
	//! keeps the alarm site, or the suspect's body when the report sounded no alarm.
	static void OnServiceCall(LF_OnServiceCall busEvent)
	{
		if (!busEvent || busEvent.m_sService != "police" || busEvent.m_sCallerUid.IsEmpty())
			return;

		string suspectUid = busEvent.m_sCallerUid;
		if (HasSite(suspectUid) || LF_WantedReports.CountFor(suspectUid) <= 0)
			return;

		vector site = busEvent.m_vPosition;
		if (site == vector.Zero)
		{
			LF_PlayerAccount account;
			IEntity body = LF_PoliceUtils.FindPlayerEntityByUid(suspectUid, account);
			if (!body)
				return;

			site = body.GetOrigin();
		}

		RecordSite(suspectUid, site);
		LF_Debug.Info(FEATURE, string.Format("site of %1 kept at %2 ('%3')", suspectUid, site, busEvent.m_sPlace));
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a fine ticket was written. Pays the officer's cash wallet when it completes a call.
	static void OnFineIssued(LF_OnFineIssued busEvent)
	{
		if (!busEvent || !Replication.IsServer())
			return;

		LF_PlayerAccount account;
		IEntity officer = LF_PoliceUtils.FindPlayerEntityByUid(busEvent.m_sOfficerUid, account);
		LF_CashWallet wallet;
		if (officer)
			wallet = new LF_EntityCashWallet(officer, REASON, busEvent.m_sHolderUid);

		int paid = Settle(busEvent.m_sOfficerUid, busEvent.m_sHolderUid, busEvent.m_vOfficerPosition, busEvent.m_iAmount, wallet);
		if (paid > 0 && officer)
			LF_Utils.NotifyEntity(officer, "#LF-PoliceCall_Paid", "#LF-PoliceCall_Title", 4.0, paid.ToString());
	}

	//------------------------------------------------------------------------------------------------
	//! Server: officerUid fined suspectUid fineAmount standing at officerPosition. Completes the
	//! suspect's open call and pays the reward (at most the fine) into wallet when the suspect is still
	//! reported, the officer holds that call and stands within the radius of its site. A refusal, or
	//! cash that does not all fit, leaves the call open and the wallet as it was.
	//! \return Cash paid; 0 when nothing completed.
	static int Settle(string officerUid, string suspectUid, vector officerPosition, int fineAmount, LF_CashWallet wallet)
	{
		LF_PoliceCall call = LF_PoliceCalls.FindOpenFor(suspectUid);
		if (!call)
			return 0;

		if (officerUid.IsEmpty() || call.m_sOfficerUid != officerUid)
		{
			LF_Debug.Info(FEATURE, string.Format("fine of %1 by %2 completes nothing: call #%3 is held by '%4'", suspectUid, officerUid, call.m_iId, call.m_sOfficerUid));
			return 0;
		}

		if (LF_WantedReports.CountFor(suspectUid) <= 0)
		{
			LF_Debug.Info(FEATURE, string.Format("fine by %1 completes nothing: %2 has no open report for call #%3", officerUid, suspectUid, call.m_iId));
			return 0;
		}

		vector site;
		float radius;
		if (!SiteOf(call, site, radius))
		{
			LF_Debug.Warn(FEATURE, string.Format("fine by %1 completes nothing: call #%2 site '%3' has no position", officerUid, call.m_iId, call.m_sSite));
			return 0;
		}

		float distance = vector.DistanceXZ(officerPosition, site);
		if (distance > radius)
		{
			LF_Debug.Info(FEATURE, string.Format("fine by %1 completes nothing: %2 m from call #%3 site, radius %4", officerUid, Math.Round(distance), call.m_iId, radius));
			return 0;
		}

		if (!wallet)
		{
			LF_Debug.Error(FEATURE, string.Format("call #%1 left open: no cash wallet for officer %2", call.m_iId, officerUid));
			return 0;
		}

		// The reward never exceeds the fine, so a suspect and an officer working together cannot mint cash.
		int reward = Math.Min(Reward(), Math.Max(fineAmount, 0));
		int paid;
		if (reward > 0)
		{
			paid = wallet.Put(reward);
			if (paid != reward)
			{
				if (paid > 0)
					wallet.Take(paid);

				LF_Debug.Warn(FEATURE, string.Format("call #%1 left open: only %2 of %3 cash fits on %4", call.m_iId, paid, reward, officerUid));
				return 0;
			}
		}

		if (!LF_PoliceCalls.Complete(call.m_iId, officerUid))
		{
			if (paid > 0)
				wallet.Take(paid);

			return 0;
		}

		Sites().Remove(suspectUid);
		LF_Debug.Info(FEATURE, string.Format("call #%1 completed by %2, +%3 cash", call.m_iId, officerUid, paid));
		return paid;
	}
}

//------------------------------------------------------------------------------------------------
class LF_PoliceCallPaySubscriber : LF_EventSubscriber
{
	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "PoliceCallPay";
	}

	//------------------------------------------------------------------------------------------------
	override bool Handle(notnull LF_Event busEvent)
	{
		LF_OnFineIssued fine = LF_OnFineIssued.Cast(busEvent);
		if (fine)
		{
			LF_PoliceCallPay.OnFineIssued(fine);
			return true;
		}

		LF_OnServiceCall service = LF_OnServiceCall.Cast(busEvent);
		if (service)
		{
			LF_PoliceCallPay.OnServiceCall(service);
			return true;
		}

		LF_OnWantedChanged changed = LF_OnWantedChanged.Cast(busEvent);
		if (!changed)
			return false;

		LF_PoliceCallPay.OnWantedChanged(changed);
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_PoliceCallPay's statics (LF_StaticReset finds it by reflection).
class LF_PoliceCallPay_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_PoliceCallPay";
	}

	override void Apply()
	{
		LF_PoliceCallPay.ResetStatics();
	}
}
