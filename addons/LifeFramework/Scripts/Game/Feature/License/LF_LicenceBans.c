//------------------------------------------------------------------------------------------------
//! One licence ban (#3430, the "licencebans" save section): a character's record key, the licence
//! name (as LF_ELicenseType spells it) and the in-game minute the ban ends (minutes since
//! 1970-01-01 of the game clock's date, LF_LicenceDesk.NowMinutes; the date survives a restart).
//! Field order is the binary save order; append only.
class LF_LicenceBanRow : Managed
{
	static const int ROW_VERSION = 1;

	//! Row format, written first. 1 is this shape.
	int m_iVersion;
	string m_sPlayerUid;
	string m_sLicence;
	int m_iEndMinute;

	//------------------------------------------------------------------------------------------------
	static LF_LicenceBanRow Create(string playerUid, string licence, int endMinute)
	{
		LF_LicenceBanRow row = new LF_LicenceBanRow();
		row.m_iVersion = ROW_VERSION;
		row.m_sPlayerUid = playerUid;
		row.m_sLicence = licence;
		row.m_iEndMinute = endMinute;
		return row;
	}

	//------------------------------------------------------------------------------------------------
	LF_LicenceBanRow Copy()
	{
		return Create(m_sPlayerUid, m_sLicence, m_iEndMinute);
	}
}

//------------------------------------------------------------------------------------------------
//! Licence bans by character (#3430). Server only, never replicated. A ban takes the licence from
//! the holder (the progression record that the save and every respawn read, and the live body when
//! given) and refuses getting it back until the in-game minute it ends:
//! LF_LicenseManagerComponent.CanUnlockLicense asks IsBanned, so the licence desk, a shop counter and
//! PurchaseLicense all refuse. A ban that has ended is lifted the first time anything reads it, and on
//! load. The arrest that hands a ban out (#3432) calls Ban with LF_LicenceSeizureRules.BansFor.
class LF_LicenceBans
{
	static const string FEATURE = "License";

	protected static ref array<ref LF_LicenceBanRow> s_aRows;
	//! Tests only: the in-game minute Now() returns when 0 or more.
	protected static int s_iNowOverride = -1;

	//------------------------------------------------------------------------------------------------
	protected static array<ref LF_LicenceBanRow> Rows()
	{
		if (!s_aRows)
			s_aRows = new array<ref LF_LicenceBanRow>();

		return s_aRows;
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_aRows = null;
		s_iNowOverride = -1;
	}

	//------------------------------------------------------------------------------------------------
	//! Tests only: pin the in-game clock (minutes since 1970-01-01); -1 releases it.
	static void SetNowForTest(int minutes)
	{
		s_iNowOverride = minutes;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The in-game minute now, or -1 when the world has no clock (then no ban ever ends).
	static int Now()
	{
		if (s_iNowOverride >= 0)
			return s_iNowOverride;

		return LF_LicenceDesk.NowMinutes();
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_LicenceBanRow Find(string playerUid, string licence)
	{
		foreach (LF_LicenceBanRow row : Rows())
		{
			if (row.m_sPlayerUid == playerUid && row.m_sLicence == licence)
				return row;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool HasEnded(notnull LF_LicenceBanRow row, int now)
	{
		return now >= 0 && row.m_iEndMinute <= now;
	}

	//------------------------------------------------------------------------------------------------
	//! Bans a licence: takes it from the character's saved licences and from the live holder, and
	//! refuses it for `days` in-game days. A second ban on the same licence keeps the later end.
	//! 0 days takes the licence with no ban.
	//! \param[in] playerUid The character's record key (LF_Utils.GetPlayerUID).
	//! \param[in] holder The character's live body, or null when offline.
	//! \return The in-game minute the ban ends, or -1 when nothing was banned.
	static int Ban(string playerUid, LF_ELicenseType licence, int days, IEntity holder = null)
	{
		if (!Replication.IsServer())
			return -1;

		if (playerUid.IsEmpty())
		{
			LF_Debug.Warn(FEATURE, "licence ban refused: no character key");
			return -1;
		}

		string name = typename.EnumToString(LF_ELicenseType, licence);
		TakeLicence(playerUid, licence, holder);
		if (days <= 0)
		{
			LF_Debug.Info(FEATURE, string.Format("%1 lost %2 with no ban", playerUid, name));
			return -1;
		}

		int now = Now();
		if (now < 0)
		{
			LF_Debug.Error(FEATURE, string.Format("licence ban of %1 on %2 has no game clock; the licence is taken with no ban", playerUid, name));
			return -1;
		}

		int banDays = Math.ClampInt(days, 1, LF_LicenceSeizureRules.BAN_DAYS_MAX);
		int end = now + banDays * LF_LicenceDesk.MINUTES_PER_DAY;
		LF_LicenceBanRow row = Find(playerUid, name);
		if (!row)
		{
			row = LF_LicenceBanRow.Create(playerUid, name, end);
			Rows().Insert(row);
		}
		else if (end > row.m_iEndMinute || HasEnded(row, now))
		{
			row.m_iEndMinute = end;
		}

		LF_Debug.Info(FEATURE, string.Format("%1 banned from %2 for %3 in-game day(s), until minute %4", playerUid, name, banDays, row.m_iEndMinute));
		return row.m_iEndMinute;
	}

	//------------------------------------------------------------------------------------------------
	//! Takes a licence from the saved record and the live body. Idempotent.
	protected static void TakeLicence(string playerUid, LF_ELicenseType licence, IEntity holder)
	{
		array<LF_ELicenseType> saved = {};
		LF_Progression.GetLicenses(playerUid, saved);
		int at = saved.Find(licence);
		if (at >= 0)
		{
			saved.RemoveOrdered(at);
			LF_Progression.NoteLicenses(playerUid, saved);
		}

		if (!holder)
			return;

		LF_LicenseManagerComponent manager = LF_LicenseManagerComponent.Cast(holder.FindComponent(LF_LicenseManagerComponent));
		if (!manager || !manager.HasLicense(licence))
			return;

		array<LF_ELicenseType> live = {};
		live.Copy(manager.GetUnlockedLicenses());
		live.RemoveItemOrdered(licence);
		manager.SetUnlockedLicenses(live);
	}

	//------------------------------------------------------------------------------------------------
	//! \return True while the character may not get the licence back. An ended ban is lifted here.
	static bool IsBanned(string playerUid, LF_ELicenseType licence)
	{
		if (playerUid.IsEmpty() || !s_aRows)
			return false;

		LF_LicenceBanRow row = Find(playerUid, typename.EnumToString(LF_ELicenseType, licence));
		if (!row)
			return false;

		if (!HasEnded(row, Now()))
			return true;

		Lift(row);
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The in-game minute the ban ends, or -1 with no ban in force.
	static int EndOf(string playerUid, LF_ELicenseType licence)
	{
		if (!IsBanned(playerUid, licence))
			return -1;

		LF_LicenceBanRow row = Find(playerUid, typename.EnumToString(LF_ELicenseType, licence));
		if (!row)
			return -1;

		return row.m_iEndMinute;
	}

	//------------------------------------------------------------------------------------------------
	protected static void Lift(notnull LF_LicenceBanRow row)
	{
		LF_Debug.Info(FEATURE, string.Format("licence ban of %1 on %2 ended at minute %3; lifted", row.m_sPlayerUid, row.m_sLicence, row.m_iEndMinute));
		Rows().RemoveItem(row);
	}

	//------------------------------------------------------------------------------------------------
	//! Persistence seam: every ban still in force at `now`.
	static array<ref LF_LicenceBanRow> ExportAll(int now)
	{
		array<ref LF_LicenceBanRow> rows = new array<ref LF_LicenceBanRow>();
		foreach (LF_LicenceBanRow row : Rows())
		{
			if (!HasEnded(row, now))
				rows.Insert(row.Copy());
		}

		return rows;
	}

	//------------------------------------------------------------------------------------------------
	//! Persistence seam: the saved bans replace the live ones. A ban that ended while the server was
	//! down is lifted; every ban still in force takes its licence from the saved record again (the
	//! progression payload loads before any section). Idempotent.
	//! \return How many bans are in force after the load.
	static int ApplyAll(notnull array<ref LF_LicenceBanRow> rows, int now)
	{
		s_aRows = new array<ref LF_LicenceBanRow>();
		foreach (LF_LicenceBanRow row : rows)
		{
			if (!row || row.m_sPlayerUid.IsEmpty())
			{
				LF_Debug.Warn(FEATURE, "saved licence ban with no character key skipped");
				continue;
			}

			LF_ELicenseType licence = typename.StringToEnum(LF_ELicenseType, row.m_sLicence);
			if (typename.EnumToString(LF_ELicenseType, licence) != row.m_sLicence)
			{
				LF_Debug.Warn(FEATURE, string.Format("saved licence ban of %1 names '%2', which is not a licence; skipped", row.m_sPlayerUid, row.m_sLicence));
				continue;
			}

			if (HasEnded(row, now))
			{
				LF_Debug.Info(FEATURE, string.Format("licence ban of %1 on %2 ended while the server was down (minute %3); lifted on load", row.m_sPlayerUid, row.m_sLicence, row.m_iEndMinute));
				continue;
			}

			if (Find(row.m_sPlayerUid, row.m_sLicence))
				continue;

			s_aRows.Insert(row.Copy());
			TakeLicence(row.m_sPlayerUid, licence, null);
		}

		return s_aRows.Count();
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_LicenceBans' statics (LF_StaticReset finds it by reflection).
class LF_LicenceBans_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_LicenceBans";
	}

	override void Apply()
	{
		LF_LicenceBans.ResetStatics();
	}
}
