//------------------------------------------------------------------------------------------------
//! The war clock (M7 gang war W4, #2669). A server sweep every 30 s calls `LF_OrgWars.Tick(now)`, so a
//! declaration nobody answers lapses and a war past `endsAt` ends on time, whether or not anyone touches
//! the war desk. A subscriber on `LF_OnOrganizationEmptied` (#2592) ends every open war of that
//! organization with reason `forfeit` (a declaration it made or received becomes LAPSED with that
//! reason, an accepted war becomes ENDED and starts the pair cooldown). The announce side is
//! `LF_OrgWarAnnounce`: each end publishes `LF_OnOrgWarEnded`, so each closing is told exactly once.
//! `Start` runs from the game mode; `SweepAt(now)` takes the clock so a test drives time itself.
//------------------------------------------------------------------------------------------------
class LF_OrgWarSweep
{
	static const string FEATURE = "OrgWars";
	static const int SWEEP_MS = 30000;	// lf-fixed: sweep period of the war clock, not a balance knob

	protected static bool s_bScheduled;
	protected static ref LF_OrgWarForfeitListener s_Forfeit;

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		Cancel();
		if (s_Forfeit)
			LF_EventBus.Unsubscribe(LF_OnOrganizationEmptied, s_Forfeit);

		s_Forfeit = null;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: wires the forfeit subscriber and the announce subscriber, and starts the 30 s sweep.
	//! Idempotent. A client or a world with no call queue gets the subscribers only.
	static void Start()
	{
		Install();
		if (s_bScheduled || !Replication.IsServer() || !GetGame() || !GetGame().GetCallqueue())
			return;

		s_bScheduled = true;
		GetGame().GetCallqueue().CallLater(Tick, SWEEP_MS, true);
		LF_Debug.Info(FEATURE, string.Format("war clock sweeps every %1 ms", SWEEP_MS));
	}

	//------------------------------------------------------------------------------------------------
	//! The subscribers alone (a test installs them after it reset the bus).
	static void Install()
	{
		if (!s_Forfeit)
			s_Forfeit = new LF_OrgWarForfeitListener();

		LF_EventBus.Subscribe(LF_OnOrganizationEmptied, s_Forfeit);
		LF_OrgWarAnnounce.Install();
	}

	//------------------------------------------------------------------------------------------------
	protected static void Cancel()
	{
		if (s_bScheduled && GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(Tick);

		s_bScheduled = false;
	}

	//------------------------------------------------------------------------------------------------
	protected static void Tick()
	{
		SweepAt(System.GetUnixTime());
	}

	//------------------------------------------------------------------------------------------------
	//! Server: one pass of the clock at unix second `now`.
	//! \return How many wars the pass closed (lapsed or ended).
	static int SweepAt(int now)
	{
		int before = OpenCount();
		LF_OrgWars.Tick(now);
		return before - OpenCount();
	}

	//------------------------------------------------------------------------------------------------
	//! Server: ends every open war of `orgId` with reason forfeit at `now`.
	//! \return How many wars were closed.
	static int ForfeitOf(string orgId, int now)
	{
		array<string> open = {};
		foreach (LF_OrgWarRecord war : LF_OrgWars.ExportAll())
		{
			if (!war || !war.Involves(orgId))
				continue;

			if (war.m_eState == LF_EOrgWarState.DECLARED || war.m_eState == LF_EOrgWarState.ACCEPTED)
				open.Insert(war.m_sWarId);
		}

		foreach (string warId : open)
		{
			LF_OrgWars.End(warId, LF_OrgWars.REASON_FORFEIT, now);
		}

		if (!open.IsEmpty())
			LF_Debug.Info(FEATURE, string.Format("org=%1 emptied: %2 war(s) forfeited", orgId, open.Count()));

		return open.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Wars still waiting for an answer or running.
	protected static int OpenCount()
	{
		int count;
		foreach (LF_OrgWarRecord war : LF_OrgWars.ExportAll())
		{
			if (war && (war.m_eState == LF_EOrgWarState.DECLARED || war.m_eState == LF_EOrgWarState.ACCEPTED))
				count++;
		}

		return count;
	}
}

//------------------------------------------------------------------------------------------------
//! Bus subscriber (#2592): an organization that lost its last member forfeits its wars.
class LF_OrgWarForfeitListener : LF_EventSubscriber
{
	override bool Handle(notnull LF_Event busEvent)
	{
		LF_OnOrganizationEmptied emptied = LF_OnOrganizationEmptied.Cast(busEvent);
		if (!emptied)
			return false;

		LF_OrgWarSweep.ForfeitOf(emptied.m_sOrgId, System.GetUnixTime());
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_OrgWarSweep's statics (found by reflection; Core/LF_StaticReset.c).
class LF_OrgWarSweep_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_OrgWarSweep";
	}

	override void Apply()
	{
		LF_OrgWarSweep.ResetStatics();
	}
}
