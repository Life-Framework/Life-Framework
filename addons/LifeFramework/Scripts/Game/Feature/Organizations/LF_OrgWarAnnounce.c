//------------------------------------------------------------------------------------------------
//! Tells the island about organization wars (M7 gang war W4, #2669). A bus subscriber on the three war
//! events sends one `LF_NotificationManagerComponent.NotifyAll` toast per event (declared, accepted,
//! ended or lapsed, both organization names), and the online members of the two sides get a longer
//! notice of their own. Texts are core-neutral keys (`LF-OrgWarAnnounce_*`); a pack words them.
//! Server only: the events are published on the server, the toast goes out through the notification
//! manager's own RPC. A toast has one argument, so the two names ride as "Declarer / Target".
//------------------------------------------------------------------------------------------------
class LF_OrgWarAnnounce
{
	static const string FEATURE = "OrgWars";
	static const string TITLE = "#LF-OrgWarAnnounce_Title";
	static const string KEY_DECLARED = "#LF-OrgWarAnnounce_Declared";
	static const string KEY_ACCEPTED = "#LF-OrgWarAnnounce_Accepted";
	static const string KEY_TIME = "#LF-OrgWarAnnounce_Time";
	static const string KEY_LAPSED = "#LF-OrgWarAnnounce_Lapsed";
	static const string KEY_FORFEIT = "#LF-OrgWarAnnounce_Forfeit";
	static const string MEMBER_DECLARED_OUT = "#LF-OrgWarAnnounce_MemberDeclaredOut";
	static const string MEMBER_DECLARED_IN = "#LF-OrgWarAnnounce_MemberDeclaredIn";
	static const string MEMBER_ACCEPTED = "#LF-OrgWarAnnounce_MemberAccepted";
	static const string MEMBER_TIME = "#LF-OrgWarAnnounce_MemberTime";
	static const string MEMBER_LAPSED = "#LF-OrgWarAnnounce_MemberLapsed";
	static const string MEMBER_FORFEIT = "#LF-OrgWarAnnounce_MemberForfeit";

	static const float ISLAND_SECONDS = 6.0;	// lf-fixed: toast time on screen, not a balance knob
	static const float MEMBER_SECONDS = 12.0;	// lf-fixed: members read their own notice longer

	protected static int s_iAnnounced;
	protected static int s_iMemberNotices;
	protected static string s_sLastKey;
	protected static string s_sLastArg;
	protected static ref LF_OrgWarAnnounceListener s_Listener;

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		if (s_Listener)
		{
			LF_EventBus.Unsubscribe(LF_OnOrgWarDeclared, s_Listener);
			LF_EventBus.Unsubscribe(LF_OnOrgWarAccepted, s_Listener);
			LF_EventBus.Unsubscribe(LF_OnOrgWarEnded, s_Listener);
		}

		s_Listener = null;
		s_iAnnounced = 0;
		s_iMemberNotices = 0;
		s_sLastKey = "";
		s_sLastArg = "";
	}

	//------------------------------------------------------------------------------------------------
	//! Subscribes to the three war events. Idempotent: a second call finds the subscriber on the bus.
	static void Install()
	{
		if (!s_Listener)
			s_Listener = new LF_OrgWarAnnounceListener();

		LF_EventBus.Subscribe(LF_OnOrgWarDeclared, s_Listener);
		LF_EventBus.Subscribe(LF_OnOrgWarAccepted, s_Listener);
		LF_EventBus.Subscribe(LF_OnOrgWarEnded, s_Listener);
	}

	//------------------------------------------------------------------------------------------------
	//! Island toasts sent since the world started (test seam: one per war event).
	static int GetAnnouncedCount()
	{
		return s_iAnnounced;
	}

	//------------------------------------------------------------------------------------------------
	static string GetLastKey()
	{
		return s_sLastKey;
	}

	//------------------------------------------------------------------------------------------------
	static string GetLastArg()
	{
		return s_sLastArg;
	}

	//------------------------------------------------------------------------------------------------
	static void OnDeclared(notnull LF_OnOrgWarDeclared declared)
	{
		string declarer = NameOf(declared.m_sDeclarerOrgId);
		string target = NameOf(declared.m_sTargetOrgId);
		Island(KEY_DECLARED, declarer + " / " + target, LF_ENotificationType.WARNING);
		Members(declared.m_sDeclarerOrgId, MEMBER_DECLARED_OUT, target, LF_ENotificationType.INFO);
		Members(declared.m_sTargetOrgId, MEMBER_DECLARED_IN, declarer, LF_ENotificationType.WARNING);
	}

	//------------------------------------------------------------------------------------------------
	static void OnAccepted(notnull LF_OnOrgWarAccepted accepted)
	{
		string declarer = NameOf(accepted.m_sDeclarerOrgId);
		string target = NameOf(accepted.m_sTargetOrgId);
		Island(KEY_ACCEPTED, declarer + " / " + target, LF_ENotificationType.WARNING);
		Members(accepted.m_sDeclarerOrgId, MEMBER_ACCEPTED, target, LF_ENotificationType.WARNING);
		Members(accepted.m_sTargetOrgId, MEMBER_ACCEPTED, declarer, LF_ENotificationType.WARNING);
	}

	//------------------------------------------------------------------------------------------------
	static void OnEnded(notnull LF_OnOrgWarEnded ended)
	{
		string declarer = NameOf(ended.m_sDeclarerOrgId);
		string target = NameOf(ended.m_sTargetOrgId);
		string islandKey = KEY_TIME;
		string memberKey = MEMBER_TIME;
		if (ended.m_sReason == LF_OrgWars.REASON_LAPSED)
		{
			islandKey = KEY_LAPSED;
			memberKey = MEMBER_LAPSED;
		}
		else if (ended.m_sReason == LF_OrgWars.REASON_FORFEIT)
		{
			islandKey = KEY_FORFEIT;
			memberKey = MEMBER_FORFEIT;
		}

		Island(islandKey, declarer + " / " + target, LF_ENotificationType.INFO);
		Members(ended.m_sDeclarerOrgId, memberKey, target, LF_ENotificationType.INFO);
		Members(ended.m_sTargetOrgId, memberKey, declarer, LF_ENotificationType.INFO);
	}

	//------------------------------------------------------------------------------------------------
	//! One toast to everyone connected. The count moves even when no manager is on the game mode.
	protected static void Island(string key, string arg, LF_ENotificationType type)
	{
		s_iAnnounced++;
		s_sLastKey = key;
		s_sLastArg = arg;
		LF_Debug.Info(FEATURE, string.Format("announce %1: %2", key, arg));
		LF_NotificationManagerComponent.NotifyAll(TITLE, key, ISLAND_SECONDS, type, arg);
	}

	//------------------------------------------------------------------------------------------------
	//! A longer notice to each connected member of the organization; `arg` names the other side.
	protected static void Members(string orgId, string key, string arg, LF_ENotificationType type)
	{
		LF_Organization org = LF_OrganizationManager.Get(orgId);
		if (!org || !GetGame())
			return;

		PlayerManager players = GetGame().GetPlayerManager();
		if (!players)
			return;

		array<int> playerIds = {};
		players.GetPlayers(playerIds);
		foreach (int playerId : playerIds)
		{
			string characterId = LF_Hideout.CharacterIdOf(players.GetPlayerControlledEntity(playerId));
			if (characterId.IsEmpty() || !org.IsMember(characterId))
				continue;

			s_iMemberNotices++;
			LF_NotificationManagerComponent.NotifyPlayer(playerId, TITLE, key, MEMBER_SECONDS, type, arg);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected static string NameOf(string orgId)
	{
		LF_Organization org = LF_OrganizationManager.Get(orgId);
		if (org)
			return org.GetName();

		return orgId;
	}
}

//------------------------------------------------------------------------------------------------
//! Bus subscriber: the three war events become toasts.
class LF_OrgWarAnnounceListener : LF_EventSubscriber
{
	override bool Handle(notnull LF_Event busEvent)
	{
		LF_OnOrgWarDeclared declared = LF_OnOrgWarDeclared.Cast(busEvent);
		if (declared)
		{
			LF_OrgWarAnnounce.OnDeclared(declared);
			return true;
		}

		LF_OnOrgWarAccepted accepted = LF_OnOrgWarAccepted.Cast(busEvent);
		if (accepted)
		{
			LF_OrgWarAnnounce.OnAccepted(accepted);
			return true;
		}

		LF_OnOrgWarEnded ended = LF_OnOrgWarEnded.Cast(busEvent);
		if (ended)
		{
			LF_OrgWarAnnounce.OnEnded(ended);
			return true;
		}

		return false;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_OrgWarAnnounce's statics (found by reflection; Core/LF_StaticReset.c).
class LF_OrgWarAnnounce_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_OrgWarAnnounce";
	}

	override void Apply()
	{
		LF_OrgWarAnnounce.ResetStatics();
	}
}
