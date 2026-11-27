//------------------------------------------------------------------------------------------------
//! LF_SiteAlerts.c: tells an organisation its ground is under threat (#2686, gang war #1022).
//! Server only. It listens for LF_OnSiteContested and LF_OnSiteCaptured (LF_ContestedSites) and
//! toasts every online member of the organisation concerned, wherever they stand:
//! - a contest alerts the holder's members, once per contest and never again for the same site
//!   within CONTEST_REPEAT_SECONDS (a fight that flickers on and off the sweep does not spam);
//! - a capture alerts the old holder ("lost") and the new one ("captured"); no repeat rule, a
//!   capture is rare and every one matters.
//! Who is a member and how a toast reaches a player is behind LF_SiteAlertNotifier, so the
//! rules are tested with no world and no players (LF_Test_SiteAlerts).

//! The two seams of an alert: who the organisation's members are, and how one of them is told.
//! The default asks LF_OrganizationManager and shows an LF_NotificationManagerComponent toast to
//! the member's player when they are online (an offline member is skipped, nothing is queued).
class LF_SiteAlertNotifier : Managed
{
	//------------------------------------------------------------------------------------------------
	//! Character ids of the organisation's members into `into` (empty for an unknown organisation).
	void MembersOf(string orgId, notnull array<string> into)
	{
		into.Clear();
		LF_Organization org = LF_OrganizationManager.Get(orgId);
		if (org)
			org.GetMemberIds(into);
	}

	//------------------------------------------------------------------------------------------------
	//! Tells one member, by character id, with a string-table key. True when a player got it.
	bool Alert(string characterId, string messageKey)
	{
		if (characterId.IsEmpty() || !GetGame() || !GetGame().GetPlayerManager())
			return false;

		array<int> playerIds = {};
		GetGame().GetPlayerManager().GetPlayers(playerIds);
		foreach (int playerId : playerIds)
		{
			if (LF_OrgMapMarks.CharacterIdOfPlayer(playerId) != characterId)
				continue;

			LF_NotificationManagerComponent.NotifyPlayer(playerId, "#LF-SiteAlert_Title", messageKey, LF_SiteAlerts.TOAST_SECONDS, LF_ENotificationType.WARNING);
			return true;
		}

		return false;
	}
}

//------------------------------------------------------------------------------------------------
class LF_SiteAlerts
{
	static const int CONTEST_REPEAT_SECONDS = 60;	// lf-fixed: one alert per contest, not per flicker
	static const float TOAST_SECONDS = 8.0;	// lf-fixed: how long an alert toast stays up

	static const string KEY_CONTESTED = "#LF-SiteAlert_Contested";
	static const string KEY_CAPTURED = "#LF-SiteAlert_Captured";
	static const string KEY_LOST = "#LF-SiteAlert_Lost";

	protected static ref LF_SiteAlertListener s_Listener;
	protected static ref LF_SiteAlertNotifier s_Notifier;
	//! Hideout id to the unix time its last contest alert went out.
	protected static ref map<string, int> s_mLastContest;
	protected static int s_iSent;

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		if (s_Listener)
		{
			LF_EventBus.Unsubscribe(LF_OnSiteContested, s_Listener);
			LF_EventBus.Unsubscribe(LF_OnSiteCaptured, s_Listener);
		}

		s_Listener = null;
		s_Notifier = null;
		s_mLastContest = null;
		s_iSent = 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Subscribes to the site events on the server (idempotent); the sweep calls it.
	static void Start()
	{
		if (s_Listener || !Replication.IsServer())
			return;

		s_Listener = new LF_SiteAlertListener();
		LF_EventBus.Subscribe(LF_OnSiteContested, s_Listener);
		LF_EventBus.Subscribe(LF_OnSiteCaptured, s_Listener);
	}

	//------------------------------------------------------------------------------------------------
	//! Replaces how members are found and told (a test passes a fake); null restores the default.
	static void SetNotifier(LF_SiteAlertNotifier notifier)
	{
		s_Notifier = notifier;
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_SiteAlertNotifier Notifier()
	{
		if (!s_Notifier)
			s_Notifier = new LF_SiteAlertNotifier();

		return s_Notifier;
	}

	//------------------------------------------------------------------------------------------------
	//! A site became contested: alert the holder's members once, unless this site alerted within
	//! CONTEST_REPEAT_SECONDS. Returns how many members were told.
	static int OnContested(notnull LF_OnSiteContested contested, int nowUnix)
	{
		if (contested.m_sHolderOrgId.IsEmpty())
			return 0;

		if (!s_mLastContest)
			s_mLastContest = new map<string, int>();

		int last;
		if (s_mLastContest.Find(contested.m_sHideoutId, last) && nowUnix - last < CONTEST_REPEAT_SECONDS)
		{
			LF_Debug.Info("Sites", string.Format("contest alert for %1 skipped: one went out %2 s ago", contested.m_sHideoutId, nowUnix - last));
			return 0;
		}

		s_mLastContest.Set(contested.m_sHideoutId, nowUnix);
		int told = Tell(contested.m_sHolderOrgId, KEY_CONTESTED);
		LF_Debug.Info("Sites", string.Format("contest alert for %1: %2 member(s) of %3 told", contested.m_sHideoutId, told, contested.m_sHolderOrgId));
		return told;
	}

	//------------------------------------------------------------------------------------------------
	//! A site changed hands: the old holder (when there was one) hears it was lost, the new one
	//! that it was captured. Returns how many members were told.
	static int OnCaptured(notnull LF_OnSiteCaptured captured)
	{
		int told;
		if (!captured.m_sFromOrgId.IsEmpty())
			told += Tell(captured.m_sFromOrgId, KEY_LOST);

		told += Tell(captured.m_sToOrgId, KEY_CAPTURED);
		LF_Debug.Info("Sites", string.Format("capture alert for %1: %2 member(s) told", captured.m_sHideoutId, told));
		return told;
	}

	//------------------------------------------------------------------------------------------------
	//! Every member of the organisation, once each. Returns how many a player received.
	protected static int Tell(string orgId, string messageKey)
	{
		if (orgId.IsEmpty())
			return 0;

		LF_SiteAlertNotifier notifier = Notifier();
		array<string> members = {};
		notifier.MembersOf(orgId, members);
		int told;
		foreach (string characterId : members)
		{
			if (notifier.Alert(characterId, messageKey))
				told++;
		}

		s_iSent += told;
		return told;
	}

}

//------------------------------------------------------------------------------------------------
//! Hands each site event to LF_SiteAlerts.
class LF_SiteAlertListener : LF_EventSubscriber
{
	override bool Handle(notnull LF_Event busEvent)
	{
		LF_OnSiteContested contested = LF_OnSiteContested.Cast(busEvent);
		if (contested)
		{
			LF_SiteAlerts.OnContested(contested, System.GetUnixTime());
			return true;
		}

		LF_OnSiteCaptured captured = LF_OnSiteCaptured.Cast(busEvent);
		if (captured)
			LF_SiteAlerts.OnCaptured(captured);

		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_SiteAlerts's statics (LF_StaticReset finds it by reflection).
class LF_SiteAlerts_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_SiteAlerts";
	}

	override void Apply()
	{
		LF_SiteAlerts.ResetStatics();
	}
}
