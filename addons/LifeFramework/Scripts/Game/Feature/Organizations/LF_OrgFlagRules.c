//------------------------------------------------------------------------------------------------
//! Flag lock (#4220, gangs C3, decision N21). The first gang keeps its flag. A second gang may not
//! pick a flag another organization holds while a member of that holder is online. When two
//! organizations fly the same flag and both have a member online, the later one's poles fly a plain
//! cloth in the flag's colour (the pole reads FliesPlainCloth; the colour is the flag number's own).
//!
//! "Online" is a list of character ids, so every rule is pure and the tests need no players; the
//! overloads without the list read the live players (server only). "First" is the order the flags
//! were claimed through Pick in this session; an organization with no claim yet (loaded from a save)
//! counts as earlier than any claim made since, and two such organizations rank by organization id
//! so every machine agrees. The claim order is not saved: after a restart, a pair that shares a
//! flag ranks by id until someone picks again. The flag table itself is LF_OrgFlags (#4213).
//! Server-side state; nothing replicates here.
class LF_OrgFlagRules
{
	static const string FEATURE = "Organizations";

	//! organization id -> claim number (1, 2, ...) of its latest flag change through Pick.
	protected static ref map<string, int> s_mClaim;
	protected static int s_iClaimSeq;

	//------------------------------------------------------------------------------------------------
	protected static map<string, int> Claims()
	{
		if (!s_mClaim)
			s_mClaim = new map<string, int>();

		return s_mClaim;
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_mClaim = null;
		s_iClaimSeq = 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the character ids of every player with an active character, right now.
	static void OnlineCharacterIds(notnull array<string> outIds)
	{
		outIds.Clear();
		if (!GetGame() || !GetGame().GetPlayerManager())
			return;

		LF_PlayerAccountManager accounts = LF_PlayerAccountManager.GetInstance();
		if (!accounts)
			return;

		array<int> playerIds = {};
		GetGame().GetPlayerManager().GetPlayers(playerIds);
		foreach (int playerId : playerIds)
		{
			string characterId = LF_OrgMapMarks.CharacterIdOfAccount(accounts.GetAccount(playerId));
			if (!characterId.IsEmpty())
				outIds.Insert(characterId);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when a member of the organization is in the online list.
	static bool HasMemberOnline(string orgId, notnull array<string> online)
	{
		LF_Organization org = LF_OrganizationManager.Get(orgId);
		if (!org)
			return false;

		foreach (string characterId : online)
		{
			if (org.IsMember(characterId))
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The id of another organization that holds the flag and has a member online (the one
	//! that keeps it), or an empty string when the flag is open to orgId.
	static string BlockingHolder(string orgId, int flag, notnull array<string> online)
	{
		array<ref LF_OrganizationRecord> records = LF_OrganizationManager.ExportAll();
		foreach (LF_OrganizationRecord record : records)
		{
			if (!record || record.m_sId.IsEmpty() || record.m_sId == orgId)
				continue;
			if (LF_OrgFlags.FlagOf(record.m_sId) != flag)
				continue;
			if (HasMemberOnline(record.m_sId, online))
				return record.m_sId;
		}

		return string.Empty;
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when the organization may fly this flag: it is registered, the flag is a preset,
	//! and no other holder has a member online. Its own flag is always open to it.
	static bool CanPick(string orgId, int flag, notnull array<string> online)
	{
		if (!LF_OrganizationManager.Get(orgId) || !LF_OrgFlags.IsValid(flag))
			return false;

		return BlockingHolder(orgId, flag, online).IsEmpty();
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the organization takes the flag when CanPick says so. Re-picking its own flag changes
	//! nothing, so it keeps its place in the claim order. A refusal is logged and returns false.
	static bool Pick(string orgId, int flag, notnull array<string> online)
	{
		if (!LF_OrganizationManager.Get(orgId) || !LF_OrgFlags.IsValid(flag))
		{
			LF_Debug.Error(FEATURE, string.Format("flag pick refused: org=%1 flag=%2 is not a registered organization and a preset", orgId, flag));
			return false;
		}

		string holder = BlockingHolder(orgId, flag, online);
		if (!holder.IsEmpty())
		{
			LF_Debug.Info(FEATURE, string.Format("flag pick refused: org=%1 flag=%2 is held by org=%3 with a member online", orgId, flag, holder));
			return false;
		}

		if (LF_OrgFlags.FlagOf(orgId) == flag)
			return true;

		if (!LF_OrgFlags.Set(orgId, flag))
			return false;

		s_iClaimSeq++;
		Claims().Set(orgId, s_iClaimSeq);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when organization a claimed its flag before organization b. No claim counts as
	//! before any claim; a tie ranks by organization id.
	static bool ClaimedBefore(string a, string b)
	{
		int seqA = Claims().Get(a);
		int seqB = Claims().Get(b);
		if (seqA != seqB)
			return seqA < seqB;

		array<string> ids = {a, b};
		ids.Sort();
		return ids.Get(0) == a;
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when the organization's poles fly a plain cloth in its colour: another organization
	//! flies the same flag, claimed it first, and both have a member online.
	static bool FliesPlainCloth(string orgId, notnull array<string> online)
	{
		int flag = LF_OrgFlags.FlagOf(orgId);
		if (!LF_OrgFlags.IsValid(flag) || !HasMemberOnline(orgId, online))
			return false;

		string holder = BlockingHolder(orgId, flag, online);
		if (holder.IsEmpty())
			return false;

		return ClaimedBefore(holder, orgId);
	}

}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_OrgFlagRules's statics (LF_StaticReset finds it by reflection).
class LF_OrgFlagRules_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_OrgFlagRules";
	}

	override void Apply()
	{
		LF_OrgFlagRules.ResetStatics();
	}
}
