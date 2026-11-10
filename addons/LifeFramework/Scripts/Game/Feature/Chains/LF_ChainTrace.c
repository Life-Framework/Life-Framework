//------------------------------------------------------------------------------------------------
//! One player who could smell a site: a server-side snapshot taken for a sweep.
class LF_ChainTraceListener : Managed
{
	string m_sUid;
	vector m_vPosition;
	bool m_bOfficer;
	IEntity m_Body;

	static LF_ChainTraceListener Create(string uid, vector position, bool officer, IEntity body)
	{
		LF_ChainTraceListener l = new LF_ChainTraceListener();
		l.m_sUid = uid;
		l.m_vPosition = position;
		l.m_bOfficer = officer;
		l.m_Body = body;
		return l;
	}
}

//------------------------------------------------------------------------------------------------
//! The smell and smoke trace of a running chain site (#418). Server only.
//!
//! Work scans nothing. LF_OnChainWork starts one timer (SWEEP_MS) that runs only while a site is
//! warm or a station still smokes. Each sweep (1) sets the smoke level of every station from its
//! site heat (a station replicates a small int, so it changes only at a threshold) and (2) lets
//! every on-duty officer and every other player inside the heat-scaled range notice the smell,
//! once per listener and site per CUE_COOLDOWN_S. A notice is a toast and an LF_OnChainSmell on
//! the bus; LF_ChainSmellLead turns that into a police service call.
class LF_ChainTrace
{
	static const string FEATURE = "Chains";
	static const int SWEEP_MS = 5000;
	static const float CUE_COOLDOWN_S = 60;
	static const float LEAD_COOLDOWN_S = 300;
	static const int MAX_SMOKE = 3;

	protected static ref map<string, float> s_mCued;
	protected static bool s_bTimer;
	protected static ref LF_ChainTraceStart s_Starter;

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_mCued = null;
		s_bTimer = false;
		s_Starter = null;
		LF_ChainSmellLead.ResetStatics();
	}

	//------------------------------------------------------------------------------------------------
	//! Inside the range is noticed, outside is not. A cold site (range 0) is never noticed.
	static bool Noticed(float distance, float rangeM)
	{
		return rangeM > 0 && distance <= rangeM;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether a cue sent at lastSeconds (negative for never) may repeat at nowSeconds.
	static bool CueDue(float lastSeconds, float nowSeconds, float cooldownSeconds)
	{
		return lastSeconds < 0 || nowSeconds - lastSeconds >= cooldownSeconds;
	}

	//------------------------------------------------------------------------------------------------
	//! Smoke level 0..MAX_SMOKE from the site heat: none when cold, then thirds of the maximum.
	static int SmokeLevel(LF_ChainHeatDef def, float heat)
	{
		if (!def || def.m_fMax <= 0 || heat <= 0)
			return 0;

		int level = Math.Ceil(Math.Min(heat / def.m_fMax, 1) * MAX_SMOKE);
		return Math.ClampInt(level, 1, MAX_SMOKE);
	}

	//------------------------------------------------------------------------------------------------
	protected static map<string, float> Cued()
	{
		if (!s_mCued)
			s_mCued = new map<string, float>();

		return s_mCued;
	}

	//------------------------------------------------------------------------------------------------
	//! When key was last cued, or -1 for never. A map miss leaves the out value at 0, which would
	//! read as "cued at world second 0", so the miss is checked first.
	static float LastSeconds(notnull map<string, float> table, string key)
	{
		if (!table.Contains(key))
			return -1;

		return table.Get(key);
	}

	//------------------------------------------------------------------------------------------------
	//! Notices for one sweep. \return How many listeners noticed a site this sweep.
	static int Sweep(float nowSeconds, notnull array<ref LF_ChainTraceListener> listeners)
	{
		array<LF_ChainSiteState> sites = {};
		LF_ChainSites.GetWarmSites(nowSeconds, sites);

		int noticed;
		foreach (LF_ChainSiteState site : sites)
		{
			float range = LF_ChainSites.GetRange(site.m_sSiteId, nowSeconds);
			if (range <= 0)
				continue;

			float heat = LF_ChainSites.GetHeat(site.m_sSiteId, nowSeconds);
			foreach (LF_ChainTraceListener listener : listeners)
			{
				float distance = vector.Distance(listener.m_vPosition, site.m_vOrigin);
				if (!Noticed(distance, range))
					continue;

				string key = listener.m_sUid + "|" + site.m_sSiteId;
				if (!CueDue(LastSeconds(Cued(), key), nowSeconds, CUE_COOLDOWN_S))
					continue;

				Cued().Set(key, nowSeconds);
				Cue(listener, site, distance, heat);
				noticed++;
			}
		}

		return noticed;
	}

	//------------------------------------------------------------------------------------------------
	protected static void Cue(LF_ChainTraceListener listener, LF_ChainSiteState site, float distance, float heat)
	{
		string message = "#LF-Chain_Smell_Witness";
		if (listener.m_bOfficer)
			message = "#LF-Chain_Smell_Officer";

		LF_Utils.NotifyEntity(listener.m_Body, message, "#LF-Chain_Smell_Title", 6.0);
		LF_Debug.Info(FEATURE, string.Format("%1 smelled site %2 from %3 m (heat %4, officer=%5)", listener.m_sUid, site.m_sSiteId, Math.Round(distance), heat, listener.m_bOfficer));
		LF_EventBus.Publish(LF_OnChainSmell.Create(listener.m_sUid, listener.m_bOfficer, site.m_sSiteId, site.m_sChainId, distance, heat, site.m_vOrigin));
	}

	//------------------------------------------------------------------------------------------------
	//! Sets every station's smoke level from its site heat. \return Stations still smoking.
	static int UpdateSmoke(float nowSeconds)
	{
		int smoking;
		foreach (string key, LF_ChainStationComponent station : LF_ChainStationComponent.Live())
		{
			if (!station)
				continue;

			int level;
			LF_ChainDef chain = LF_ChainCatalog.FindChain(station.GetChainId());
			if (chain)
				level = SmokeLevel(chain.m_Heat, LF_ChainSites.GetHeat(station.GetSiteId(), nowSeconds));

			station.SetSmoke(level);
			if (level > 0)
				smoking++;
		}

		return smoking;
	}

	//------------------------------------------------------------------------------------------------
	//! The live players as listeners. Officers are on-duty police; everyone else is a witness.
	protected static void GatherListeners(notnull array<ref LF_ChainTraceListener> outListeners)
	{
		if (!GetGame() || !GetGame().GetPlayerManager())
			return;

		LF_PlayerAccountManager accounts = LF_PlayerAccountManager.GetInstance();
		array<int> playerIds = {};
		GetGame().GetPlayerManager().GetPlayers(playerIds);
		foreach (int playerId : playerIds)
		{
			IEntity body = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
			if (!body)
				continue;

			string uid = LF_Utils.GetPlayerUID(playerId);
			bool officer;
			if (accounts)
				officer = LF_AccountStateBits.IsOnDutyPolice(LF_AccountStateBits.FromAccount(accounts.GetAccount(uid)));

			outListeners.Insert(LF_ChainTraceListener.Create(uid, body.GetOrigin(), officer, body));
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Wakes the sweep timer if it is not already running.
	static void Start()
	{
		if (s_bTimer || !Replication.IsServer() || !GetGame())
			return;

		s_bTimer = true;
		Tick();
	}

	//------------------------------------------------------------------------------------------------
	protected static void Tick()
	{
		float now = GetGame().GetWorld().GetWorldTime() / 1000;
		int smoking = UpdateSmoke(now);

		array<LF_ChainSiteState> warm = {};
		LF_ChainSites.GetWarmSites(now, warm);
		if (!warm.IsEmpty())
		{
			array<ref LF_ChainTraceListener> listeners = {};
			GatherListeners(listeners);
			Sweep(now, listeners);
		}

		if (smoking > 0 || !warm.IsEmpty())
		{
			GetGame().GetCallqueue().CallLater(Tick, SWEEP_MS, false);
			return;
		}

		s_bTimer = false;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: subscribe once per world (the bus table is cleared at world end).
	static void Install()
	{
		if (!Replication.IsServer())
			return;

		if (s_Starter)
			LF_EventBus.Unsubscribe(LF_OnChainWork, s_Starter);

		s_Starter = new LF_ChainTraceStart();
		LF_EventBus.Subscribe(LF_OnChainWork, s_Starter);
		LF_ChainSmellLead.Install();
	}
}

//------------------------------------------------------------------------------------------------
//! Bus subscriber: accepted work wakes the trace sweep.
class LF_ChainTraceStart : LF_EventSubscriber
{
	override bool Handle(notnull LF_Event busEvent)
	{
		if (!LF_OnChainWork.Cast(busEvent))
			return false;

		LF_ChainTrace.Start();
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Bus subscriber: a noticed smell becomes a police lead (#418). One call per site per
//! LEAD_COOLDOWN_S, so a crowd walking past a still is one line on the board, not twenty. The call
//! goes through LF_ServiceCalls like a payphone call: dispatch logs and radios it, the service
//! board pins it, and nothing happens when nobody is on duty.
class LF_ChainSmellLead : LF_EventSubscriber
{
	protected static ref map<string, float> s_mLast;
	static ref LF_ChainSmellLead s_Subscriber;

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_mLast = null;
		s_Subscriber = null;
	}

	//------------------------------------------------------------------------------------------------
	//! Marks a lead for the site if one is due. \return True when this notice should raise a call.
	static bool TakeLead(string siteId, float nowSeconds)
	{
		if (!s_mLast)
			s_mLast = new map<string, float>();

		if (!LF_ChainTrace.CueDue(LF_ChainTrace.LastSeconds(s_mLast, siteId), nowSeconds, LF_ChainTrace.LEAD_COOLDOWN_S))
			return false;

		s_mLast.Set(siteId, nowSeconds);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override bool Handle(notnull LF_Event busEvent)
	{
		LF_OnChainSmell smell = LF_OnChainSmell.Cast(busEvent);
		if (!smell)
			return false;

		float now = GetGame().GetWorld().GetWorldTime() / 1000;
		if (!TakeLead(smell.m_sSiteId, now))
			return true;

		string place = LF_WantedReports.DescribePlace(smell.m_vPosition);
		LF_ServiceCalls.Place(smell.m_sListenerUid, "police", place, "#LF-Chain_SmellLead", smell.m_vPosition);
		LF_Debug.Info(LF_ChainTrace.FEATURE, string.Format("smell lead for site %1 at '%2'", smell.m_sSiteId, place));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	static void Install()
	{
		if (!Replication.IsServer())
			return;

		if (s_Subscriber)
			LF_EventBus.Unsubscribe(LF_OnChainSmell, s_Subscriber);

		s_Subscriber = new LF_ChainSmellLead();
		LF_EventBus.Subscribe(LF_OnChainSmell, s_Subscriber);
	}
}
