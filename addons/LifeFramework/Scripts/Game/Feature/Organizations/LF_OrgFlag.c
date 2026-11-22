//------------------------------------------------------------------------------------------------
//! One saved gang flag (#4213, "orgflags" save section): the organization id and the preset flag it
//! flies, 1 to LF_OrgFlags.COUNT. Field order is the save order; append only.
class LF_OrgFlagRow : Managed
{
	static const int ROW_VERSION = 1;

	//! Row format, written first. 1 is this shape.
	int m_iVersion;
	string m_sOrgId;
	int m_iFlag;

	//------------------------------------------------------------------------------------------------
	static LF_OrgFlagRow Create(string orgId, int flag)
	{
		LF_OrgFlagRow row = new LF_OrgFlagRow();
		row.m_iVersion = ROW_VERSION;
		row.m_sOrgId = orgId;
		row.m_iFlag = flag;
		return row;
	}
}

//------------------------------------------------------------------------------------------------
//! Each organization's flag (#4213, gangs C2): one of the COUNT preset flags, numbered 1 to COUNT;
//! NONE (0) is no flag. A side table keyed by organization id, so the positional "orgs" record does
//! not move. Every organization has a flag after a load: one with no row takes the first free flag
//! (EnsureAll). "Free" means no other organization on the register holds it. Two organizations may
//! hold the same flag (a saved pair, or a later pick under the online lock rule); nothing here
//! refuses that, the choosing rule decides. Server-side state; nothing replicates here.
class LF_OrgFlags
{
	static const string FEATURE = "Organizations";
	//! No flag.
	static const int NONE = 0;
	//! The preset flags, numbered 1 to COUNT (decision N20: 20 preset flags).
	static const int COUNT = 20;

	protected static ref map<string, int> s_mFlags;

	//------------------------------------------------------------------------------------------------
	protected static map<string, int> Flags()
	{
		if (!s_mFlags)
			s_mFlags = new map<string, int>();

		return s_mFlags;
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_mFlags = null;
	}

	//------------------------------------------------------------------------------------------------
	static bool IsValid(int flag)
	{
		return flag >= 1 && flag <= COUNT;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The organization's flag, or NONE (no row, or the organization is not on the register).
	static int FlagOf(string orgId)
	{
		if (orgId.IsEmpty() || !LF_OrganizationManager.Get(orgId))
			return NONE;

		return Flags().Get(orgId);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the organization flies this flag. Refused (logged) off the server, for an organization
	//! that is not registered, or for a flag outside 1 to COUNT.
	static bool Set(string orgId, int flag)
	{
		if (!Replication.IsServer())
		{
			LF_Debug.Error(FEATURE, "flag set rejected: not the server");
			return false;
		}
		if (!LF_OrganizationManager.Get(orgId))
		{
			LF_Debug.Error(FEATURE, string.Format("flag set refused: organization '%1' is not registered", orgId));
			return false;
		}
		if (!IsValid(flag))
		{
			LF_Debug.Error(FEATURE, string.Format("flag set refused for org=%1: flag %2 is not 1 to %3", orgId, flag, COUNT));
			return false;
		}

		Flags().Set(orgId, flag);
		LF_Debug.Info(FEATURE, string.Format("flag org=%1 flag=%2", orgId, flag));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when an organization on the register other than exceptOrgId holds the flag.
	static bool IsHeld(int flag, string exceptOrgId = string.Empty)
	{
		foreach (string orgId, int held : Flags())
		{
			if (held == flag && orgId != exceptOrgId && LF_OrganizationManager.Get(orgId))
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The lowest flag no other registered organization holds, or NONE when all COUNT are held.
	static int FirstFree(string exceptOrgId = string.Empty)
	{
		for (int flag = 1; flag <= COUNT; flag++)
		{
			if (!IsHeld(flag, exceptOrgId))
				return flag;
		}

		return NONE;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: gives a registered organization with no flag the first free one. When all COUNT are
	//! held it takes flag 1 (shared) and warns, so every organization still flies a flag.
	//! \return The organization's flag, or NONE when it is not registered.
	static int Ensure(string orgId)
	{
		if (!LF_OrganizationManager.Get(orgId))
			return NONE;

		int flag = Flags().Get(orgId);
		if (IsValid(flag))
			return flag;

		flag = FirstFree(orgId);
		if (flag == NONE)
		{
			flag = 1;
			LF_Debug.Warn(FEATURE, string.Format("all %1 flags are held; org=%2 shares flag 1", COUNT, orgId));
		}

		Flags().Set(orgId, flag);
		LF_Debug.Info(FEATURE, string.Format("flag org=%1 flag=%2 (first free)", orgId, flag));
		return flag;
	}

	//------------------------------------------------------------------------------------------------
	//! Every registered organization with no flag takes the first free one, in organization id order
	//! so a second load hands out the same flags.
	//! \return How many organizations were given a flag.
	static int EnsureAll()
	{
		array<string> ids = {};
		array<ref LF_OrganizationRecord> records = LF_OrganizationManager.ExportAll();
		foreach (LF_OrganizationRecord record : records)
		{
			if (record && !record.m_sId.IsEmpty())
				ids.Insert(record.m_sId);
		}

		ids.Sort();
		int given;
		foreach (string orgId : ids)
		{
			if (IsValid(Flags().Get(orgId)))
				continue;

			Ensure(orgId);
			given++;
		}

		return given;
	}

	//------------------------------------------------------------------------------------------------
	//! Persistence seam: one row per organization with a flag, sorted by organization id. A row whose
	//! organization is not on the register right now is written too: the register can be replaced
	//! and put back (a load, a test) while the flag table stays, and FlagOf ignores such a row.
	static array<ref LF_OrgFlagRow> ExportAll()
	{
		array<string> ids = {};
		foreach (string orgId, int flag : Flags())
		{
			if (IsValid(flag) && !orgId.IsEmpty())
				ids.Insert(orgId);
		}

		ids.Sort();
		array<ref LF_OrgFlagRow> rows = new array<ref LF_OrgFlagRow>();
		foreach (string id : ids)
		{
			rows.Insert(LF_OrgFlagRow.Create(id, Flags().Get(id)));
		}

		return rows;
	}

	//------------------------------------------------------------------------------------------------
	//! Persistence seam: the saved flags replace the live table, then every organization with no row
	//! takes the first free flag (EnsureAll). A row with no version, no organization id, a flag outside
	//! 1 to COUNT is skipped with a warning. A row whose organization is not on the register is kept
	//! (logged): it counts for nothing until that organization is back. Idempotent.
	//! \return How many rows were applied.
	static int ApplyAll(notnull array<ref LF_OrgFlagRow> rows)
	{
		s_mFlags = new map<string, int>();
		int applied;
		foreach (LF_OrgFlagRow row : rows)
		{
			if (!row || row.m_iVersion < 1 || row.m_sOrgId.IsEmpty())
			{
				LF_Debug.Warn(FEATURE, "flag load skipped a row with no version or organization");
				continue;
			}
			if (!IsValid(row.m_iFlag))
			{
				LF_Debug.Warn(FEATURE, string.Format("flag load skipped org=%1: flag %2 is not 1 to %3", row.m_sOrgId, row.m_iFlag, COUNT));
				continue;
			}
			if (!LF_OrganizationManager.Get(row.m_sOrgId))
				LF_Debug.Info(FEATURE, string.Format("flag load kept org=%1 flag=%2: the organization is not on the register", row.m_sOrgId, row.m_iFlag));

			s_mFlags.Set(row.m_sOrgId, row.m_iFlag);
			applied++;
		}

		int given = EnsureAll();
		LF_Debug.Info(FEATURE, string.Format("flag load applied %1 of %2 row(s); %3 organization(s) took the first free flag", applied, rows.Count(), given));
		return applied;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_OrgFlags's statics (LF_StaticReset finds it by reflection).
class LF_OrgFlags_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_OrgFlags";
	}

	override void Apply()
	{
		LF_OrgFlags.ResetStatics();
	}
}
