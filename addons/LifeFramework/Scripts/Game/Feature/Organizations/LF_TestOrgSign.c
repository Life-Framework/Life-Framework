// LF_TestOrgSign.c - the playtest "Join test organisation" / "Leave organisation" sign (#934).
// A solo tester has nobody to invite them, so the sign puts a character in a shared organisation
// named "Test Organisation" (the first one in founds it and takes the hideout lease; later ones
// join at the entry rank) and takes them out again. The server decides; the client only asks.
// Playtest only, like the travel globe: the game mode must have the playtest hub on
// (LF_SpawnLogic.m_bPlaytestHub) and the dev gate must pass (LF_DevGate).

enum LF_ETestOrgRefusal
{
	NONE,
	NOT_PLAYTEST,
	NOT_ALLOWED,
	NO_CHARACTER,
	TOO_FAR,
	ALREADY_IN,
	NOT_IN
}

//------------------------------------------------------------------------------------------------
//! Pure rules, so a LOGIC test can drive every side of them.
class LF_TestOrgRules
{
	static const string ORG_NAME = "Test Organisation";

	//------------------------------------------------------------------------------------------------
	//! \param joining true for Join, false for Leave.
	static LF_ETestOrgRefusal Check(bool hubEnabled, bool devAllowed, bool hasCharacter, bool near, bool inOrganisation, bool joining)
	{
		if (!hubEnabled)
			return LF_ETestOrgRefusal.NOT_PLAYTEST;

		if (!devAllowed)
			return LF_ETestOrgRefusal.NOT_ALLOWED;

		if (!hasCharacter)
			return LF_ETestOrgRefusal.NO_CHARACTER;

		if (!near)
			return LF_ETestOrgRefusal.TOO_FAR;

		if (joining && inOrganisation)
			return LF_ETestOrgRefusal.ALREADY_IN;

		if (!joining && !inOrganisation)
			return LF_ETestOrgRefusal.NOT_IN;

		return LF_ETestOrgRefusal.NONE;
	}

	//------------------------------------------------------------------------------------------------
	//! The first one in (or the first one back into an empty test organisation) is its founder,
	//! so a lone tester can try ranks and the treasury; everyone after joins at the entry rank.
	static string RankFor(bool organisationHasFounder, string entryRankId)
	{
		if (organisationHasFounder)
			return entryRankId;

		return LF_OrganizationConfig.RANK_FOUNDER;
	}

	//------------------------------------------------------------------------------------------------
	static string RefusalKey(LF_ETestOrgRefusal reason)
	{
		if (reason == LF_ETestOrgRefusal.NOT_PLAYTEST || reason == LF_ETestOrgRefusal.NOT_ALLOWED)
			return "#LF-TestOrgSign_Denied";

		if (reason == LF_ETestOrgRefusal.NO_CHARACTER)
			return "#LF-TestOrgSign_NoCharacter";

		if (reason == LF_ETestOrgRefusal.TOO_FAR)
			return "#LF-TestOrgSign_TooFar";

		if (reason == LF_ETestOrgRefusal.ALREADY_IN)
			return "#LF-TestOrgSign_AlreadyIn";

		if (reason == LF_ETestOrgRefusal.NOT_IN)
			return "#LF-TestOrgSign_NotIn";

		return string.Empty;
	}
}

//------------------------------------------------------------------------------------------------
class LF_TestOrgSign
{
	static const string FEATURE = "TestOrgSign";
	//! Server reach to the sign: the action shows within about 2 m; the rest is slack for movement.
	static const float REACH = 5.0; // lf-fixed: anti-cheat reach tied to the action's context radius, not an owner knob

	//------------------------------------------------------------------------------------------------
	protected static bool DevAllowed(IEntity user)
	{
		PlayerManager playerManager = GetGame().GetPlayerManager();
		if (!playerManager || !user)
			return false;

		int playerId = playerManager.GetPlayerIdFromControlledEntity(user);
		return LF_DevGate.IsAllowedForPlayer(playerId);
	}

	//------------------------------------------------------------------------------------------------
	//! Server. Puts the user in the Test Organisation. \return the refusal, NONE when they are in.
	static LF_ETestOrgRefusal Join(IEntity user, IEntity sign)
	{
		return Run(user, sign, true);
	}

	//------------------------------------------------------------------------------------------------
	//! Server. Takes the user out of their organisation, founder included. \return the refusal.
	static LF_ETestOrgRefusal Leave(IEntity user, IEntity sign)
	{
		return Run(user, sign, false);
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_ETestOrgRefusal Run(IEntity user, IEntity sign, bool joining)
	{
		string characterId;
		if (user)
			characterId = LF_OrganizationPlayer.CharacterId(user);

		bool near = user && sign && vector.Distance(user.GetOrigin(), sign.GetOrigin()) <= REACH;
		bool inOrganisation = LF_OrganizationManager.GetByCharacter(characterId) != null;
		LF_ETestOrgRefusal refusal = LF_TestOrgRules.Check(LF_SpawnHub.IsEnabled(), DevAllowed(user), !characterId.IsEmpty(), near, inOrganisation, joining);
		if (refusal != LF_ETestOrgRefusal.NONE)
		{
			LF_Debug.Warn(FEATURE, string.Format("refused %1: %2", characterId, typename.EnumToString(LF_ETestOrgRefusal, refusal)));
			Tell(user, LF_TestOrgRules.RefusalKey(refusal));
			return refusal;
		}

		if (joining)
			Enter(user, characterId);
		else
			Exit(user, characterId);

		return LF_ETestOrgRefusal.NONE;
	}

	//------------------------------------------------------------------------------------------------
	protected static void Enter(IEntity user, string characterId)
	{
		LF_Organization org = LF_OrganizationManager.FindByName(LF_TestOrgRules.ORG_NAME);
		bool founder;
		if (org)
		{
			founder = !HasFounder(org);
			string rank = LF_TestOrgRules.RankFor(!founder, org.EntryRankId());
			if (!LF_OrganizationManager.JoinDirect(org.GetId(), characterId, rank))
			{
				LF_Debug.Error(FEATURE, string.Format("join failed: %1 could not enter %2", characterId, org.GetId()));
				return;
			}
		}
		else
		{
			org = LF_OrganizationManager.Create(characterId, LF_TestOrgRules.ORG_NAME);
			if (!org)
			{
				LF_Debug.Error(FEATURE, "join failed: the test organisation could not be founded");
				return;
			}

			founder = true;
		}

		string message = "#LF-TestOrgSign_Joined";
		if (founder)
		{
			// A founder holds the right to lease; the first one in takes the yard if it is free.
			if (LF_OrgHideoutLease.StateFor(org.GetId(), LF_Hideout.OrgHideoutId()) == LF_EOrgLease.FREE)
				LF_OrgHideoutLease.Lease(characterId, LF_Hideout.OrgHideoutId());

			if (LF_OrgHideoutLease.StateFor(org.GetId(), LF_Hideout.OrgHideoutId()) == LF_EOrgLease.TAKEN)
				message = "#LF-TestOrgSign_YardTaken";
		}

		LF_Debug.Info(FEATURE, string.Format("%1 joined %2 as %3", characterId, org.GetId(), org.GetRankId(characterId)));
		LF_OrgReplication.Publish(user);
		Tell(user, message);
	}

	//------------------------------------------------------------------------------------------------
	protected static void Exit(IEntity user, string characterId)
	{
		if (!LF_OrganizationManager.LeaveDirect(characterId))
		{
			LF_Debug.Error(FEATURE, string.Format("leave failed: %1", characterId));
			return;
		}

		LF_Debug.Info(FEATURE, string.Format("%1 left their organisation", characterId));
		LF_OrgReplication.Publish(user);
		Tell(user, "#LF-TestOrgSign_Left");
	}

	//------------------------------------------------------------------------------------------------
	protected static bool HasFounder(LF_Organization org)
	{
		array<string> ids = {};
		org.GetMemberIds(ids);
		foreach (string memberId : ids)
		{
			if (org.GetRankId(memberId) == LF_OrganizationConfig.RANK_FOUNDER)
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected static void Tell(IEntity user, string key)
	{
		if (user && !key.IsEmpty())
			LF_Utils.NotifyEntity(user, key, "#LF-TestOrgSign_Title", 4.0);
	}
}
