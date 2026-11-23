//------------------------------------------------------------------------------------------------
//! The live member-mark rules as pure functions (card C11, #4222, decision N25): a downed or jailed
//! member's mark is grey, a viewer in custody sees no marks, and a member who leaves loses their mark
//! at once. LF_OrgLiveMarks (publishing) and LF_MapShares (who receives) ask these; nothing here reads
//! the game, so a test covers every rule without a world.
class LF_OrgLiveMarkRules
{
	//! The live key layout of LF_OrgLiveMarks: `orglive:<orgId>:<characterId>`.
	static const string KEY_PREFIX = "orglive:";
	static const string KEY_SEPARATOR = ":";

	//------------------------------------------------------------------------------------------------
	//! A member whose mark is grey: downed, or in custody (jailed or arrested).
	static bool IsGrey(bool downed, bool inCustody)
	{
		return downed || inCustody;
	}

	//------------------------------------------------------------------------------------------------
	//! The placed-marker colour index for a mark: greyIndex when grey, else liveIndex.
	static int ColourFor(bool grey, int liveIndex, int greyIndex)
	{
		if (grey)
			return greyIndex;

		return liveIndex;
	}

	//------------------------------------------------------------------------------------------------
	//! Does this viewer receive a live mark of organization markOrgId? Only that organization's own
	//! members, and none while the viewer is in custody (a jailed member must not read their crew's
	//! positions off the map).
	static bool ViewerSees(string markOrgId, string viewerOrgId, bool viewerInCustody)
	{
		if (viewerInCustody)
			return false;

		if (markOrgId.IsEmpty() || viewerOrgId.IsEmpty())
			return false;

		return markOrgId == viewerOrgId;
	}

	//------------------------------------------------------------------------------------------------
	//! Does a mark keep its place? It stays only while its character is still in the organization the
	//! key names. A member who left (no organization now) or moved to another one loses it.
	static bool MarkStays(string markOrgId, string characterOrgNow)
	{
		if (markOrgId.IsEmpty() || characterOrgNow.IsEmpty())
			return false;

		return markOrgId == characterOrgNow;
	}

	//------------------------------------------------------------------------------------------------
	//! The organization id a live key names, "" when the key is not a live key.
	static string OrgIdOfKey(string key)
	{
		if (!key.StartsWith(KEY_PREFIX))
			return "";

		string rest = key.Substring(KEY_PREFIX.Length(), key.Length() - KEY_PREFIX.Length());
		int cut = rest.IndexOf(KEY_SEPARATOR);
		if (cut <= 0)
			return "";

		return rest.Substring(0, cut);
	}

	//------------------------------------------------------------------------------------------------
	//! The character id a live key names, "" when the key is not a live key.
	static string CharacterIdOfKey(string key)
	{
		if (!key.StartsWith(KEY_PREFIX))
			return "";

		string rest = key.Substring(KEY_PREFIX.Length(), key.Length() - KEY_PREFIX.Length());
		int cut = rest.IndexOf(KEY_SEPARATOR);
		if (cut <= 0 || cut >= rest.Length() - 1)
			return "";

		return rest.Substring(cut + 1, rest.Length() - cut - 1);
	}

	//------------------------------------------------------------------------------------------------
	//! Must this live key be taken off the map now, given the organization its character is in now
	//! ("" when none)? True for a key that is not a live key at all, so it is never kept by mistake.
	static bool DropKey(string key, string characterOrgNow)
	{
		return !MarkStays(OrgIdOfKey(key), characterOrgNow);
	}
}
