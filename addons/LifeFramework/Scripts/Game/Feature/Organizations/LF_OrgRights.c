//------------------------------------------------------------------------------------------------
//! Organization rights as a bitmask (#750a). A rank carries one int; bit N is
//! LF_EOrganizationPermission value N. Bits 0-3 are the four bools a rank saved before the mask
//! (withdraw, invite, kick, buy upgrade), in that order, so a version 1 organization record
//! back-fills the mask from them. Pure rules: no entity, no world.
//------------------------------------------------------------------------------------------------
class LF_OrgRights
{
	//! Number of defined rights; every bit at or above it is dropped on load.
	static const int COUNT = 9;
	//! Every defined right.
	static const int ALL = 511;

	//------------------------------------------------------------------------------------------------
	static int Bit(LF_EOrganizationPermission permission)
	{
		if (permission < 0 || permission >= COUNT)
			return 0;

		return 1 << permission;
	}

	//------------------------------------------------------------------------------------------------
	static bool Has(int rights, LF_EOrganizationPermission permission)
	{
		int bit = Bit(permission);
		if (bit == 0)
			return false;

		return (rights & bit) == bit;
	}

	//------------------------------------------------------------------------------------------------
	//! Drops bits no right is defined for (a hand-edited or corrupt save).
	static int Clean(int rights)
	{
		return rights & ALL;
	}

	//------------------------------------------------------------------------------------------------
	//! The four rights a rank record saved as bools before the mask.
	static int FromLegacy(bool canWithdraw, bool canInvite, bool canKick, bool canBuyUpgrade)
	{
		int rights = 0;
		if (canWithdraw)
			rights = rights | Bit(LF_EOrganizationPermission.WITHDRAW);
		if (canInvite)
			rights = rights | Bit(LF_EOrganizationPermission.INVITE);
		if (canKick)
			rights = rights | Bit(LF_EOrganizationPermission.KICK);
		if (canBuyUpgrade)
			rights = rights | Bit(LF_EOrganizationPermission.BUY_UPGRADE);

		return rights;
	}

	//------------------------------------------------------------------------------------------------
	//! What a rank saved before the mask could do, as a mask. The founder could do everything, so it
	//! gets every right, the new ones included. Any other rank keeps its four bools and gets the
	//! stash, which every member could open before ranks gated it.
	static int MigrateLegacy(string rankId, bool canWithdraw, bool canInvite, bool canKick, bool canBuyUpgrade)
	{
		if (rankId == LF_OrganizationConfig.RANK_FOUNDER)
			return ALL;

		return FromLegacy(canWithdraw, canInvite, canKick, canBuyUpgrade) | Bit(LF_EOrganizationPermission.USE_STASH);
	}

	//------------------------------------------------------------------------------------------------
	//! "invite,kick" for log lines.
	static string Describe(int rights)
	{
		string text;
		for (int bit = 0; bit < COUNT; bit++)
		{
			if ((rights & (1 << bit)) == 0)
				continue;

			if (!text.IsEmpty())
				text += ",";

			text += typename.EnumToString(LF_EOrganizationPermission, bit);
		}

		if (text.IsEmpty())
			return "none";

		return text;
	}
}
