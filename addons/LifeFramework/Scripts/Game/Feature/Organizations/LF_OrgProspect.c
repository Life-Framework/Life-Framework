//------------------------------------------------------------------------------------------------
//! One prospect: who joined which gang off the street, and when (unix seconds).
class LF_OrgProspectRow : Managed
{
	static const int ROW_VERSION = 1;

	int m_iVersion;
	string m_sCharacterId;
	string m_sOrgId;
	int m_iJoinedAt;

	//------------------------------------------------------------------------------------------------
	static LF_OrgProspectRow Create(string characterId, string orgId, int joinedAt)
	{
		LF_OrgProspectRow row = new LF_OrgProspectRow();
		row.m_iVersion = ROW_VERSION;
		row.m_sCharacterId = characterId;
		row.m_sOrgId = orgId;
		row.m_iJoinedAt = joinedAt;
		return row;
	}
}

//------------------------------------------------------------------------------------------------
//! Prospect rank (#4455, gangs A2). A player who joins a gang through an invite
//! (LF_OrgInvites.Accept, the face-to-face "Join <org>") is a prospect for 48 hours: they
//! keep every other right of their rank, but not the fund or the stash (LF_OrgRights.ForProspect). A
//! leader's promote ends it early. The join time is unix seconds and is saved in its own section
//! (LF_OrgProspectSavePayload), so the 48 hours run across restarts. Server state only: every rights
//! check (LF_Organization.Allows, RightsOf) runs on the server.
//------------------------------------------------------------------------------------------------
class LF_OrgProspect
{
	static const string FEATURE = "Organizations";
	// lf-fixed: the 0.3.0 plan sets the 48-hour prospect period (GA-A2); an owner key needs LF_OrgConfig, outside #4455.
	static const int PERIOD_SECONDS = 172800;
	//! Shown in place of the rank name while a member is a prospect. Words, never a countdown.
	static const string RANK_KEY = "#LF-OrgProspect_Rank";
	//! What a prospect is told when the fund or the stash refuses them.
	static const string REFUSED_KEY = "#LF-OrgProspect_Refused";

	//! characterId -> row.
	protected static ref map<string, ref LF_OrgProspectRow> s_mRows = new map<string, ref LF_OrgProspectRow>();
	//! Test clock in unix seconds; 0 uses the real clock.
	protected static int s_iClock;

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_mRows = new map<string, ref LF_OrgProspectRow>();
		s_iClock = 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Tests: pin the clock (unix seconds); 0 goes back to the real clock.
	static void SetClock(int unixSeconds)
	{
		s_iClock = unixSeconds;
	}

	//------------------------------------------------------------------------------------------------
	static int Now()
	{
		if (s_iClock > 0)
			return s_iClock;

		return System.GetUnixTime();
	}

	//------------------------------------------------------------------------------------------------
	//! A character just joined orgId off the street.
	static void Mark(string characterId, string orgId)
	{
		if (characterId.IsEmpty() || orgId.IsEmpty())
			return;

		s_mRows.Set(characterId, LF_OrgProspectRow.Create(characterId, orgId, Now()));
		LF_Debug.Info(FEATURE, string.Format("prospect org=%1 member=%2 joinedAt=%3", orgId, characterId, Now()));
	}

	//------------------------------------------------------------------------------------------------
	//! The character is a prospect no longer (promoted, or joined some other way).
	static void Clear(string characterId, string reason)
	{
		if (!s_mRows.Contains(characterId))
			return;

		s_mRows.Remove(characterId);
		LF_Debug.Info(FEATURE, string.Format("prospect ended member=%1 reason=%2", characterId, reason));
	}

	//------------------------------------------------------------------------------------------------
	//! True while the character is a prospect of org: joined it by invite under 48 hours ago, still in it,
	//! and still on its entry rung (a promote by any path ends it).
	static bool IsProspect(LF_Organization org, string characterId)
	{
		if (!org || characterId.IsEmpty())
			return false;

		LF_OrgProspectRow row = s_mRows.Get(characterId);
		if (!row || row.m_sOrgId != org.GetId())
			return false;
		if (Now() - row.m_iJoinedAt >= PERIOD_SECONDS)
			return false;
		if (!org.IsMember(characterId))
			return false;

		return org.GetRankId(characterId) == org.EntryRankId();
	}

	//------------------------------------------------------------------------------------------------
	//! The words a prospect gets when \permission is held back, or empty when it is not.
	static string RefusalFor(LF_Organization org, string characterId, LF_EOrganizationPermission permission)
	{
		if (!LF_OrgRights.BarsProspect(permission) || !IsProspect(org, characterId))
			return string.Empty;

		return REFUSED_KEY;
	}

	//------------------------------------------------------------------------------------------------
	//! Live rows only: a lapsed prospect is not written, so the save shrinks by itself.
	static array<ref LF_OrgProspectRow> ExportAll()
	{
		array<ref LF_OrgProspectRow> rows = new array<ref LF_OrgProspectRow>();
		int now = Now();
		foreach (string characterId, LF_OrgProspectRow row : s_mRows)
		{
			if (row && now - row.m_iJoinedAt < PERIOD_SECONDS)
				rows.Insert(LF_OrgProspectRow.Create(row.m_sCharacterId, row.m_sOrgId, row.m_iJoinedAt));
		}

		return rows;
	}

	//------------------------------------------------------------------------------------------------
	//! Replaces the table with the saved rows (a second load ends in the same state). Bad rows are skipped.
	//! \return Rows kept.
	static int ApplyAll(notnull array<ref LF_OrgProspectRow> rows)
	{
		s_mRows = new map<string, ref LF_OrgProspectRow>();
		int kept;
		foreach (LF_OrgProspectRow row : rows)
		{
			if (!row || row.m_iVersion < 1 || row.m_sCharacterId.IsEmpty() || row.m_sOrgId.IsEmpty() || row.m_iJoinedAt <= 0)
			{
				LF_Debug.Warn(FEATURE, "prospect row skipped: bad row in the save");
				continue;
			}

			s_mRows.Set(row.m_sCharacterId, LF_OrgProspectRow.Create(row.m_sCharacterId, row.m_sOrgId, row.m_iJoinedAt));
			kept++;
		}

		return kept;
	}
}

//------------------------------------------------------------------------------------------------
//! The fund and stash rights drop for a prospect wherever a member's rights are read (#4455). The
//! fund withdrawal, the strongbox, the stash and the catalogue all ask Allows or RightsOf on the server,
//! so none of their call sites changes.
modded class LF_Organization
{
	//------------------------------------------------------------------------------------------------
	override bool Allows(string characterId, LF_EOrganizationPermission permission)
	{
		if (!super.Allows(characterId, permission))
			return false;

		if (LF_OrgProspect.RefusalFor(this, characterId, permission).IsEmpty())
			return true;

		LF_Debug.Info(LF_OrgProspect.FEATURE, string.Format("refused: %1 is a prospect of %2 and cannot use %3 yet", characterId, GetId(), typename.EnumToString(LF_EOrganizationPermission, permission)));
		return false;
	}

	//------------------------------------------------------------------------------------------------
	override int RightsOf(string characterId)
	{
		int rights = super.RightsOf(characterId);
		if (LF_OrgProspect.IsProspect(this, characterId))
			return LF_OrgRights.ForProspect(rights);

		return rights;
	}

	//------------------------------------------------------------------------------------------------
	override string GetRankName(string characterId)
	{
		if (LF_OrgProspect.IsProspect(this, characterId))
			return LF_OrgProspect.RANK_KEY;

		return super.GetRankName(characterId);
	}
}

//------------------------------------------------------------------------------------------------
//! Ends the prospect period on a promote (#4455). A direct add (LF_OrganizationManager.Invite with no
//! offer, used by fixtures and tools) makes a full member; only the street join below makes a prospect.
modded class LF_OrganizationManager
{
	//------------------------------------------------------------------------------------------------
	override static bool Promote(string orgId, string actorCharacterId, string targetCharacterId)
	{
		bool moved = super.Promote(orgId, actorCharacterId, targetCharacterId);
		if (moved)
			LF_OrgProspect.Clear(targetCharacterId, "promoted");

		return moved;
	}

	//------------------------------------------------------------------------------------------------
	//! A playtest join is a full member at once.
	override static bool JoinDirect(string orgId, string characterId, string rankId)
	{
		bool joined = super.JoinDirect(orgId, characterId, rankId);
		if (joined)
			LF_OrgProspect.Clear(characterId, "direct join");

		return joined;
	}
}

//------------------------------------------------------------------------------------------------
//! The street join (#4455): a player who takes an offer face to face (LF_OrgInviteRole "Join <org>") starts
//! as a prospect.
modded class LF_OrgInvites
{
	//------------------------------------------------------------------------------------------------
	override static LF_EOrgOfferResult Accept(string targetCharacterId, string inviterCharacterId, float nowMs)
	{
		LF_EOrgOfferResult result = super.Accept(targetCharacterId, inviterCharacterId, nowMs);
		if (result != LF_EOrgOfferResult.JOINED)
			return result;

		LF_Organization org = LF_OrganizationManager.GetByCharacter(targetCharacterId);
		if (org)
			LF_OrgProspect.Mark(targetCharacterId, org.GetId());

		return result;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_OrgProspect's statics (LF_StaticReset finds it by reflection).
class LF_OrgProspect_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_OrgProspect";
	}

	override void Apply()
	{
		LF_OrgProspect.ResetStatics();
	}
}
