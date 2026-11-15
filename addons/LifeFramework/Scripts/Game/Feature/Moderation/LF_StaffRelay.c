//------------------------------------------------------------------------------------------------
//! Bus subscriber: an admin action goes to the staff relay.
class LF_StaffRelayAdminSubscriber : LF_EventSubscriber
{
	override bool Handle(notnull LF_Event busEvent)
	{
		LF_OnAdminAction action = LF_OnAdminAction.Cast(busEvent);
		if (!action)
			return false;

		LF_StaffRelay.OfferAdmin(action.m_sAdminUid, action.m_sTargetUid, action.m_sAction, System.GetUnixTime());
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Bus subscriber: a report that flagged incidents goes to the staff relay.
class LF_StaffRelayFlagSubscriber : LF_EventSubscriber
{
	override bool Handle(notnull LF_Event busEvent)
	{
		LF_OnIncidentFlagged flagged = LF_OnIncidentFlagged.Cast(busEvent);
		if (!flagged)
			return false;

		LF_StaffRelay.OfferFlag(flagged.m_sReporterUid, flagged.m_iFlagged, System.GetUnixTime());
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Staff relay (#1284, epic #1006): admin actions (LF_OnAdminAction) and new report flags
//! (LF_OnIncidentFlagged) go to the staff channel through LF_StaffWebhook.Send, one short line each.
//! Throttled per kind: events collect for WINDOW_SECONDS from the first one, then ONE line goes out
//! (the event's own line when there was one, a count when there were more). Server only, never
//! replicated, nothing persists. Time is passed in so a test controls it.
//------------------------------------------------------------------------------------------------
class LF_StaffRelay
{
	static const string FEATURE = "Moderation";
	static const int WINDOW_SECONDS = 5; // lf-fixed: staff-channel batching window, not a gameplay rule

	protected static ref LF_StaffRelayAdminSubscriber s_Admin;
	protected static ref LF_StaffRelayFlagSubscriber s_Flag;
	protected static bool s_bTicking;
	protected static int s_iInstalls;

	//! Admin actions waiting in the current window, and when the first arrived (0 = none waiting).
	protected static int s_iAdminCount;
	protected static int s_iAdminSince;
	protected static string s_sAdminLast;

	//! Report flags waiting: reports, incidents flagged in them, first arrival.
	protected static int s_iFlagReports;
	protected static int s_iFlagIncidents;
	protected static int s_iFlagSince;
	protected static string s_sFlagLast;

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_Admin = null;
		s_Flag = null;
		s_bTicking = false;
		s_iInstalls = 0;
		s_iAdminCount = 0;
		s_iAdminSince = 0;
		s_sAdminLast = string.Empty;
		s_iFlagReports = 0;
		s_iFlagIncidents = 0;
		s_iFlagSince = 0;
		s_sFlagLast = string.Empty;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: subscribes once. Safe to call again. \return true when subscribed.
	static bool EnsureSubscribed()
	{
		if (!Replication.IsServer())
			return false;

		if (s_Admin && s_Flag && LF_EventBus.SubscriberCount(LF_OnAdminAction) > 0 && LF_EventBus.SubscriberCount(LF_OnIncidentFlagged) > 0)
			return true;

		s_Admin = new LF_StaffRelayAdminSubscriber();
		s_Flag = new LF_StaffRelayFlagSubscriber();
		bool ok = LF_EventBus.Subscribe(LF_OnAdminAction, s_Admin);
		if (!LF_EventBus.Subscribe(LF_OnIncidentFlagged, s_Flag))
			ok = false;

		LF_Debug.Info(FEATURE, "staff relay subscribed to admin actions and report flags");
		return ok;
	}

	//------------------------------------------------------------------------------------------------
	//! Drops both subscriptions, the flush tick and whatever waits (tests restore the bus with it).
	//! LF_StaticReset.ResetAll calls it when a world ends. Safe to call when nothing is installed.
	static void Uninstall()
	{
		if (s_Admin)
			LF_EventBus.Unsubscribe(LF_OnAdminAction, s_Admin);

		if (s_Flag)
			LF_EventBus.Unsubscribe(LF_OnIncidentFlagged, s_Flag);

		if (s_bTicking && GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(Tick);

		ResetStatics();
	}

	//------------------------------------------------------------------------------------------------
	//! Server: subscribes, and keeps a 1 s tick that sends what is waiting once its window ends.
	//! Called from the game mode's OnGameStart (the modded class below); safe to call again.
	static void Install()
	{
		InstallAs(Replication.IsServer());
	}

	//------------------------------------------------------------------------------------------------
	//! Install with the role passed in, so a test can be a client. A client does nothing: no
	//! subscription, no tick. \return true when the relay is installed afterwards.
	static bool InstallAs(bool isServer)
	{
		if (!isServer)
			return false;

		if (s_bTicking)
			return true;

		if (!EnsureSubscribed())
			return false;

		s_bTicking = true;
		s_iInstalls++;
		GetGame().GetCallqueue().CallLater(Tick, 1000, true);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! How many times an install took effect since the last Uninstall: 1 on a started server, 0 on a client.
	static int GetInstallCount()
	{
		return s_iInstalls;
	}

	//------------------------------------------------------------------------------------------------
	static bool IsInstalled()
	{
		return s_bTicking && s_Admin && s_Flag;
	}

	//------------------------------------------------------------------------------------------------
	protected static void Tick()
	{
		Flush(System.GetUnixTime());
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: the line for admin action(s). One action reads as itself; more read as a count.
	static string AdminLine(int count, string last)
	{
		if (count <= 1)
			return "Admin: " + last;

		return string.Format("Admin: %1 actions, last: %2", count, last);
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: the line for report flag(s). One report reads as itself; more read as counts.
	static string FlagLine(int reports, int incidents, string lastReporter)
	{
		if (reports <= 1)
			return string.Format("Report: %1 flagged %2 incident(s)", lastReporter, incidents);

		return string.Format("Report: %1 reports flagged %2 incidents, last by %3", reports, incidents, lastReporter);
	}

	//------------------------------------------------------------------------------------------------
	//! An admin action happened. Starts the window when none is open.
	static void OfferAdmin(string adminUid, string targetUid, string action, int now)
	{
		string last = adminUid + " " + action;
		if (!targetUid.IsEmpty())
			last = last + " -> " + targetUid;

		if (s_iAdminCount == 0)
			s_iAdminSince = now;

		s_iAdminCount++;
		s_sAdminLast = last;
		Flush(now);
	}

	//------------------------------------------------------------------------------------------------
	//! A report flagged incidents. Starts the window when none is open.
	static void OfferFlag(string reporterUid, int flagged, int now)
	{
		if (s_iFlagReports == 0)
			s_iFlagSince = now;

		s_iFlagReports++;
		s_iFlagIncidents += flagged;
		s_sFlagLast = reporterUid;
		Flush(now);
	}

	//------------------------------------------------------------------------------------------------
	//! Sends each kind whose window has run its course. \return How many lines were sent.
	static int Flush(int now)
	{
		int sent;
		if (s_iAdminCount > 0 && now - s_iAdminSince >= WINDOW_SECONDS)
		{
			LF_StaffWebhook.Send(AdminLine(s_iAdminCount, s_sAdminLast));
			s_iAdminCount = 0;
			sent++;
		}

		if (s_iFlagReports > 0 && now - s_iFlagSince >= WINDOW_SECONDS)
		{
			LF_StaffWebhook.Send(FlagLine(s_iFlagReports, s_iFlagIncidents, s_sFlagLast));
			s_iFlagReports = 0;
			s_iFlagIncidents = 0;
			sent++;
		}

		return sent;
	}
}

//------------------------------------------------------------------------------------------------
//! Self-start: the staff relay comes up with the world, on the server only.
modded class LF_GameModeRoleplay
{
	//------------------------------------------------------------------------------------------------
	protected override void OnGameStart()
	{
		super.OnGameStart();
		LF_StaffRelay.Install();
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of this file's statics (Core/LF_StaticReset.c runs every feature's hook).
modded class LF_StaticResetHooks
{
	override void Run(notnull array<string> done, bool apply)
	{
		super.Run(done, apply);
		done.Insert("LF_StaffRelay");
		if (apply)
			LF_StaffRelay.Uninstall();
	}
}
