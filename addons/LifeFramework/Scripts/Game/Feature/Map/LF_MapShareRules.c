//------------------------------------------------------------------------------------------------
//! Who a shared map mark is for (docs/architecture/everon-map.md section 3, "Shared marks").
enum LF_EMapShareAudience
{
	//! One character, by id (taxi driver, job runner).
	PLAYER,
	//! The on-duty players of one service: police or medic (D-M10). Off duty sees nothing.
	ROLE,
	//! The members of one organization.
	ORG,
	//! Every player (event areas, story places).
	EVERYONE,
	//! An unknown word from a table: reaches nobody.
	NONE
}

//------------------------------------------------------------------------------------------------
//! What the server knows of one player for audience resolution. Server only; never sent.
class LF_MapShareMember : Managed
{
	int m_iPlayerId;
	//! The active character id (the key orgs and PLAYER rows use).
	string m_sUid;
	//! On-duty services only: "police", "medic". Empty off duty.
	ref array<string> m_aRoles = {};
	//! Organization id, "" when in none.
	string m_sOrgId;

	//------------------------------------------------------------------------------------------------
	static LF_MapShareMember Make(int playerId, string uid, array<string> roles, string orgId)
	{
		LF_MapShareMember member = new LF_MapShareMember();
		member.m_iPlayerId = playerId;
		member.m_sUid = uid;
		member.m_sOrgId = orgId;
		if (roles)
			member.m_aRoles.Copy(roles);

		return member;
	}
}

//------------------------------------------------------------------------------------------------
//! The shared-mark rules as pure functions (card M9). The node twin is tools/map/rules/shares.mjs; both
//! run Configs/Map/Rules/shares.cases.json (LF_Test_MapShares), so they cannot drift.
class LF_MapShareRules
{
	//! The widest area a share row may carry, in metres (Everon is 12.8 km across).
	static const int AREA_RADIUS_MAX = 5000; // lf-fixed: wire limit of a shared-mark area (everon-map.md section 3)

	//------------------------------------------------------------------------------------------------
	//! Is this a drawable area for a row: a radius of 1 to AREA_RADIUS_MAX metres and a style id that
	//! LF_MapAreaStyle holds (ids are appended only: a new last style is added here and to AREA_STYLE_IDS
	//! in tools/map/rules/shares.mjs, which the selftest compares). Radius 0 is "no area", not an error.
	static bool AreaValid(int radius, int style)
	{
		if (radius <= 0 || radius > AREA_RADIUS_MAX)
			return false;

		return style >= 0 && style <= LF_MapAreaStyle.EVENT;
	}

	//------------------------------------------------------------------------------------------------
	//! Does this player receive a row for this audience? An empty id matches nobody (fail safe).
	static bool InAudience(LF_EMapShareAudience audience, string audienceId, LF_MapShareMember member)
	{
		if (audience == LF_EMapShareAudience.EVERYONE)
			return member != null;

		if (!member || audienceId.IsEmpty())
			return false;

		if (audience == LF_EMapShareAudience.PLAYER)
			return audienceId == member.m_sUid;

		if (audience == LF_EMapShareAudience.ROLE)
			return member.m_aRoles && member.m_aRoles.Find(audienceId) != -1;

		if (audience == LF_EMapShareAudience.ORG)
			return audienceId == member.m_sOrgId;

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! A row with expires 0 never expires; otherwise it is gone at and after its time (world seconds).
	static bool Expired(float expires, float now)
	{
		return expires > 0 && now >= expires;
	}

	//------------------------------------------------------------------------------------------------
	//! What a membership change sends: keys wanted but not delivered go in adds, keys delivered but no
	//! longer wanted go in removes, each in its list's order.
	static void Diff(array<string> delivered, array<string> wanted, notnull array<string> adds, notnull array<string> removes)
	{
		adds.Clear();
		removes.Clear();
		if (wanted)
		{
			foreach (string key : wanted)
			{
				if (!delivered || delivered.Find(key) == -1)
					adds.Insert(key);
			}
		}

		if (delivered)
		{
			foreach (string key : delivered)
			{
				if (!wanted || wanted.Find(key) == -1)
					removes.Insert(key);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Case-table spelling of an audience: PLAYER, ROLE, ORG, EVERYONE. Anything else is NONE.
	static LF_EMapShareAudience ParseAudience(string word)
	{
		if (word == "PLAYER")
			return LF_EMapShareAudience.PLAYER;

		if (word == "ROLE")
			return LF_EMapShareAudience.ROLE;

		if (word == "ORG")
			return LF_EMapShareAudience.ORG;

		if (word == "EVERYONE")
			return LF_EMapShareAudience.EVERYONE;

		return LF_EMapShareAudience.NONE;
	}

	//------------------------------------------------------------------------------------------------
	//! The case-table spelling of a diff: "add:a,b|remove:c".
	static string DiffText(array<string> adds, array<string> removes)
	{
		return "add:" + Join(adds) + "|remove:" + Join(removes);
	}

	//------------------------------------------------------------------------------------------------
	protected static string Join(array<string> keys)
	{
		string text;
		foreach (int index, string key : keys)
		{
			if (index > 0)
				text += ",";

			text += key;
		}
		return text;
	}
}
