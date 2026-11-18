//------------------------------------------------------------------------------------------------
//! War kills (M7 gang war W5, #2670). A kill between two members of organizations at war
//! (`LF_OrgWars.IsAtWar`) is a war kill, not a murder, unless the rules say so
//! (`LF_OrgWarRules.m_bWarKillsAreMurder`, default off): `LF_Murder` skips it with `SKIP_ORG_WAR`,
//! so it raises no wanted level and files no crime report. When `m_bTagWarKills` is on (default)
//! the kill is also appended to the incident log as its own kind, `war_kill`, whether or not it
//! counted as murder, so staff and the war ledger can tell it from an ordinary death. The
//! ordinary `death` record is unchanged. A war never lifts a safe zone: damage there is refused
//! before any kill, and this file does not touch that rule.
//!
//! Membership is by character: the killer's and the victim's account uid resolve to the active
//! character, which the organization register knows. A kill with a side that has no account,
//! no character or no organization is never a war kill. Server only; the bus is the server's.
//------------------------------------------------------------------------------------------------
class LF_OrgWarKills
{
	static const string FEATURE = "OrgWars";
	//! The incident kind (LF_IncidentLog). `incidents.kinds` must list it when the owner narrows the kinds.
	static const string KIND_WAR_KILL = "war_kill";

	protected static ref LF_OrgWarKillsSubscriber s_Subscriber;

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		if (s_Subscriber)
			LF_EventBus.Unsubscribe(LF_OnPlayerKilled, s_Subscriber);

		s_Subscriber = null;
	}

	//------------------------------------------------------------------------------------------------
	//! Subscribes the tagger to the bus. Idempotent: the bus refuses a second subscription of the same
	//! subscriber, which lives for the world's life. LF_Murder calls it before it publishes a kill,
	//! because the bus table is cleared at world end.
	static void Watch()
	{
		if (!s_Subscriber)
			s_Subscriber = new LF_OrgWarKillsSubscriber();

		LF_EventBus.Subscribe(LF_OnPlayerKilled, s_Subscriber);
	}

	//------------------------------------------------------------------------------------------------
	//! The organization id an account's active character belongs to, or "" when the account is not
	//! resident, has no character, or the character has no organization.
	static string OrgIdOfUid(string uid)
	{
		if (uid.IsEmpty())
			return "";

		LF_PlayerAccountManager accounts = LF_PlayerAccountManager.GetInstance();
		if (!accounts)
			return "";

		LF_PlayerAccount account = accounts.GetAccount(uid);
		if (!account)
			return "";

		LF_PlayerCharacter character = account.GetActiveCharacter();
		if (!character)
			return "";

		LF_Organization org = LF_OrganizationManager.GetByCharacter(character.GetId());
		if (!org)
			return "";

		return org.GetId();
	}

	//------------------------------------------------------------------------------------------------
	//! True when the killer and the victim belong to two organizations that are at war right now.
	//! Moves the war clock first, so a war that ran out a second ago does not count.
	static bool IsWarKill(string killerUid, string victimUid)
	{
		if (killerUid.IsEmpty() || victimUid.IsEmpty() || killerUid == victimUid)
			return false;

		string killerOrg = OrgIdOfUid(killerUid);
		string victimOrg = OrgIdOfUid(victimUid);
		if (killerOrg.IsEmpty() || victimOrg.IsEmpty() || killerOrg == victimOrg)
			return false;

		LF_OrgWars.Tick(System.GetUnixTime());
		return LF_OrgWars.IsAtWar(killerOrg, victimOrg);
	}

	//------------------------------------------------------------------------------------------------
	//! The rule `LF_Murder` applies after its own: a war kill is not murder unless the rules say so.
	//! \return LF_Murder.SKIP_ORG_WAR or "".
	static string SkipReasonFor(string killerUid, string victimUid, notnull LF_OrgWarRules rules)
	{
		if (rules.m_bWarKillsAreMurder)
			return "";

		if (!IsWarKill(killerUid, victimUid))
			return "";

		return LF_Murder.SKIP_ORG_WAR;
	}

	//------------------------------------------------------------------------------------------------
	//! Appends the `war_kill` incident for a kill event when the pair is at war and the rules tag war
	//! kills. The incident has the death's shape: victim as subject, killer as the other party, the
	//! weapon as `how`, and `murder` as LF_Murder decided it.
	//! \return True when an incident was appended.
	static bool Tag(notnull LF_OnPlayerKilled killed, notnull LF_OrgWarRules rules)
	{
		if (!rules.m_bTagWarKills)
			return false;

		if (!IsWarKill(killed.m_sKillerUid, killed.m_sVictimUid))
			return false;

		LF_IncidentRecord record = LF_IncidentLog.Append(KIND_WAR_KILL, System.GetUnixTime(), killed.m_sVictimUid, killed.m_sKillerUid, killed.m_vPosition, 0, killed.m_sWeapon, killed.m_bMurder);
		if (!record)
		{
			LF_Debug.Info(FEATURE, string.Format("war_kill of %1 by %2 not logged: the incident kind is switched off", killed.m_sVictimUid, killed.m_sKillerUid));
			return false;
		}

		LF_Debug.Info(FEATURE, string.Format("war_kill: %1 killed %2 (murder %3)", killed.m_sKillerUid, killed.m_sVictimUid, killed.m_bMurder));
		return true;
	}
}

//------------------------------------------------------------------------------------------------
class LF_OrgWarKillsSubscriber : LF_EventSubscriber
{
	override bool Handle(notnull LF_Event busEvent)
	{
		LF_OnPlayerKilled killed = LF_OnPlayerKilled.Cast(busEvent);
		if (!killed)
			return false;

		LF_OrgWarKills.Tag(killed, LF_OrgWarConfig.Rules());
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_OrgWarKills' statics (found by reflection; Core/LF_StaticReset.c).
class LF_OrgWarKills_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_OrgWarKills";
	}

	override void Apply()
	{
		LF_OrgWarKills.ResetStatics();
	}
}
