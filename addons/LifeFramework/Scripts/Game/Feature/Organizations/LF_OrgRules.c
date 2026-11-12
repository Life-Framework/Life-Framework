//------------------------------------------------------------------------------------------------
//! Pure rules for the organization registry (M1-17): the founding fee, what a name may be, the
//! rights a rank carries as replicated bits, and which entry the `org_invite` slot of the player
//! interaction menu offers. No entity, no world: the server and the offer side share these.
//------------------------------------------------------------------------------------------------
enum LF_EOrgRoleMode
{
	NONE,
	//! A leader faces someone with no organization.
	INVITE,
	//! A leader faces a member of their own organization who is not the founder.
	REMOVE,
	//! Someone with no organization faces the leader whose offer they hold.
	JOIN
}

//------------------------------------------------------------------------------------------------
//! The `org_promote` and `org_demote` entries (#750a): one rung up or down for a member below you.
enum LF_EOrgRankMode
{
	NONE,
	PROMOTE,
	DEMOTE
}

//------------------------------------------------------------------------------------------------
//! What one character's organization looks like from the offer side. The server fills it from
//! LF_OrganizationManager; a client reads the copy replicated on the character.
class LF_OrgView : Managed
{
	string m_sOrgId;
	string m_sOrgName;
	int m_iRights;
	//! Name of the organization whose offer this character holds, or empty.
	string m_sOfferOrgName;
	//! Player id of whoever made that offer, or 0.
	int m_iOfferFromPlayerId;
	//! Ladder position of this character's rank (0 is the top), or -1 without one.
	int m_iRankPosition = -1;
	//! Number of rungs on this character's organization's ladder.
	int m_iLadderSize;

	//------------------------------------------------------------------------------------------------
	static LF_OrgView Create(string orgId, string orgName, int rights, string offerOrgName, int offerFromPlayerId)
	{
		LF_OrgView view = new LF_OrgView();
		view.m_sOrgId = orgId;
		view.m_sOrgName = orgName;
		view.m_iRights = rights;
		view.m_sOfferOrgName = offerOrgName;
		view.m_iOfferFromPlayerId = offerFromPlayerId;
		return view;
	}

	//------------------------------------------------------------------------------------------------
	//! Sets the ladder fields. \return this, so a test can chain it on Create.
	LF_OrgView SetLadder(int position, int size)
	{
		m_iRankPosition = position;
		m_iLadderSize = size;
		return this;
	}

	//------------------------------------------------------------------------------------------------
	//! The ladder fields as one replicated int: size in bits 8-15, position + 1 in bits 0-7.
	int PackLadder()
	{
		return (Math.ClampInt(m_iLadderSize, 0, 255) << 8) | Math.ClampInt(m_iRankPosition + 1, 0, 255);
	}

	//------------------------------------------------------------------------------------------------
	void UnpackLadder(int packed)
	{
		m_iRankPosition = (packed & 255) - 1;
		m_iLadderSize = (packed >> 8) & 255;
	}

	//------------------------------------------------------------------------------------------------
	bool HasOrg()
	{
		return !m_sOrgId.IsEmpty();
	}

	//------------------------------------------------------------------------------------------------
	//! Same organization and strictly higher on its ladder. A member with no rung (a rank missing from
	//! the table) sits below every rung, as on the server (LF_Organization.Outranks).
	bool Outranks(LF_OrgView other)
	{
		if (!other || !HasOrg() || other.m_sOrgId != m_sOrgId || m_iRankPosition < 0)
			return false;
		if (other.m_iRankPosition < 0)
			return true;

		return m_iRankPosition < other.m_iRankPosition;
	}

	//------------------------------------------------------------------------------------------------
	bool Can(int right)
	{
		return (m_iRights & right) != 0;
	}
}

//------------------------------------------------------------------------------------------------
class LF_OrgRules
{
	//! Cash the founder hands over at the registry desk. Carried MoneyStack, not the bank.
	static const int FOUNDING_FEE = 250;
	static const int NAME_MIN = 3;
	static const int NAME_MAX = 32;
	//! An offer stands this long; then the invitee has to be asked again.
	static const int OFFER_MS = 120000;
	protected static const string NAME_CHARS = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789 -'&.";

	//! Replicated rights: the rank's mask (bit = LF_EOrganizationPermission, LF_OrgRights) plus
	//! RIGHT_FOUNDER, which is view only and never saved.
	static const int RIGHT_WITHDRAW = 1;
	static const int RIGHT_INVITE = 2;
	static const int RIGHT_KICK = 4;
	static const int RIGHT_BUY = 8;
	static const int RIGHT_MANAGE_RANKS = 16;
	static const int RIGHT_FOUNDER = 65536;

	//------------------------------------------------------------------------------------------------
	//! \return Empty when the typed name may be registered (uniqueness is the manager's check),
	//! else the localization key of the reason.
	static string RefuseName(string typed)
	{
		string name = typed.Trim();
		if (name.IsEmpty())
			return "#LF-Org_NameEmpty";

		if (name.Length() < NAME_MIN)
			return "#LF-Org_NameShort";

		if (name.Length() > NAME_MAX)
			return "#LF-Org_NameLong";

		for (int i = 0; i < name.Length(); i++)
		{
			if (!NAME_CHARS.Contains(name.Get(i)))
				return "#LF-Org_NameChars";
		}

		if (name.Contains("  "))
			return "#LF-Org_NameChars";

		return string.Empty;
	}

	//------------------------------------------------------------------------------------------------
	//! The comparison key for "is this name taken": trimmed, lower case.
	static string NameKey(string name)
	{
		string key = name.Trim();
		key.ToLower();
		return key;
	}

	//------------------------------------------------------------------------------------------------
	static int RightsOf(LF_Organization org, string characterId)
	{
		if (!org || characterId.IsEmpty() || !org.IsMember(characterId))
			return 0;

		int rights = org.RightsOf(characterId);
		if (org.GetRankId(characterId) == LF_OrganizationConfig.RANK_FOUNDER)
			rights = rights | RIGHT_FOUNDER;

		return rights;
	}

	//------------------------------------------------------------------------------------------------
	//! Which `org_invite` entry user sees on target. Exactly one or none.
	static LF_EOrgRoleMode ModeFor(LF_OrgView user, LF_OrgView target, int targetPlayerId)
	{
		if (!user || !target)
			return LF_EOrgRoleMode.NONE;

		if (!user.HasOrg())
		{
			if (!user.m_sOfferOrgName.IsEmpty() && targetPlayerId > 0 && user.m_iOfferFromPlayerId == targetPlayerId)
				return LF_EOrgRoleMode.JOIN;

			return LF_EOrgRoleMode.NONE;
		}

		if (!target.HasOrg())
		{
			if (user.Can(RIGHT_INVITE))
				return LF_EOrgRoleMode.INVITE;

			return LF_EOrgRoleMode.NONE;
		}

		if (target.m_sOrgId != user.m_sOrgId)
			return LF_EOrgRoleMode.NONE;

		if (user.Can(RIGHT_KICK) && !target.Can(RIGHT_FOUNDER) && user.Outranks(target))
			return LF_EOrgRoleMode.REMOVE;

		return LF_EOrgRoleMode.NONE;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether user may move target one rung (promote: up, else down). The same rule the server
	//! applies in LF_OrganizationManager.Promote/Demote: the manage-ranks right, target strictly
	//! below user, and the new rung still below user and on the ladder.
	static LF_EOrgRankMode RankModeFor(LF_OrgView user, LF_OrgView target, bool promote)
	{
		if (!user || !target || !user.Can(RIGHT_MANAGE_RANKS) || !user.Outranks(target) || target.m_iRankPosition < 0)
			return LF_EOrgRankMode.NONE;

		if (promote)
		{
			if (target.m_iRankPosition - 1 > user.m_iRankPosition)
				return LF_EOrgRankMode.PROMOTE;

			return LF_EOrgRankMode.NONE;
		}

		if (target.m_iRankPosition + 1 < user.m_iLadderSize)
			return LF_EOrgRankMode.DEMOTE;

		return LF_EOrgRankMode.NONE;
	}

	//------------------------------------------------------------------------------------------------
	static string RankKey(int rights)
	{
		if ((rights & RIGHT_FOUNDER) != 0)
			return "#LF-Org_RankFounder";

		return "#LF-Org_RankMember";
	}
}
