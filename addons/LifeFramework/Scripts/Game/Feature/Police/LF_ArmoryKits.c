//------------------------------------------------------------------------------------------------
//! One officer's armory book: when the last kit was issued (System.GetUnixTime) and the serials of
//! that kit not yet turned in or reported lost. Plain values, persisted as is ("armorykits").
class LF_ArmoryKitRow
{
	int m_iVersion = 1;
	string m_sOfficerUid;
	int m_iLastIssueUnix;
	ref array<string> m_aSerials = {};

	//------------------------------------------------------------------------------------------------
	LF_ArmoryKitRow Copy()
	{
		LF_ArmoryKitRow copy = new LF_ArmoryKitRow();
		copy.m_iVersion = m_iVersion;
		copy.m_sOfficerUid = m_sOfficerUid;
		copy.m_iLastIssueUnix = m_iLastIssueUnix;
		if (m_aSerials)
			copy.m_aSerials.Copy(m_aSerials);

		return copy;
	}
}

//------------------------------------------------------------------------------------------------
//! Station armory book (#2416). An officer holds at most one issued kit: its serialled lines stay
//! outstanding until they are turned in at the armory or reported lost, and a new kit waits for
//! that. A kit is issued at most once per CooldownSeconds() of real time per officer. Both survive
//! clock-out and restart (the "armorykits" payload), so dropping the kit for a friend and clocking
//! out and in no longer draws a second one. Server only, never replicated.
class LF_ArmoryKits
{
	static const string FEATURE = "Armory";
	//! A lost report is a second armory use within this many seconds of the "kit still out" warning.
	static const int LOST_CONFIRM_SECONDS = 15;	// lf-fixed: the confirm window of one interaction, not a balance value

	protected static ref array<ref LF_ArmoryKitRow> s_aRows;
	//! uid -> unix time of the "kit still out" warning that a second use confirms. Not saved.
	protected static ref map<string, int> s_mLostAsked;

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_aRows = null;
		s_mLostAsked = null;
	}

	//------------------------------------------------------------------------------------------------
	protected static array<ref LF_ArmoryKitRow> Rows()
	{
		if (!s_aRows)
			s_aRows = new array<ref LF_ArmoryKitRow>();

		return s_aRows;
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_ArmoryKitRow Find(string uid)
	{
		if (uid.IsEmpty())
			return null;

		foreach (LF_ArmoryKitRow row : Rows())
		{
			if (row.m_sOfficerUid == uid)
				return row;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_ArmoryKitRow FindOrAdd(string uid)
	{
		LF_ArmoryKitRow row = Find(uid);
		if (row)
			return row;

		row = new LF_ArmoryKitRow();
		row.m_sOfficerUid = uid;
		Rows().Insert(row);
		return row;
	}

	//------------------------------------------------------------------------------------------------
	//! Serials of this officer's issued kit still out.
	static int Outstanding(string uid)
	{
		LF_ArmoryKitRow row = Find(uid);
		if (!row)
			return 0;

		return row.m_aSerials.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! System.GetUnixTime() of this officer's last issue, 0 when never issued.
	static int LastIssueUnix(string uid)
	{
		LF_ArmoryKitRow row = Find(uid);
		if (!row)
			return 0;

		return row.m_iLastIssueUnix;
	}

	//------------------------------------------------------------------------------------------------
	static bool IsOutstanding(string uid, string serial)
	{
		LF_ArmoryKitRow row = Find(uid);
		if (!row || serial.IsEmpty())
			return false;

		return row.m_aSerials.Find(serial) != -1;
	}

	//------------------------------------------------------------------------------------------------
	//! Seconds of real time between two kits for one officer (LF_PoliceConfig armoryCooldownMinutes,
	//! two hours shipped).
	static int CooldownSeconds()
	{
		return LF_PoliceConfig.ArmoryCooldownMinutes() * 60;
	}

	//------------------------------------------------------------------------------------------------
	//! Seconds until this officer may be issued a kit again; 0 when the cooldown has run out. A clock
	//! that went backwards (nowUnix before the last issue) moves the issue to nowUnix, so the officer
	//! waits one full cooldown, not until the clock catches up.
	static int CooldownLeft(string uid, int nowUnix)
	{
		LF_ArmoryKitRow row = Find(uid);
		if (!row || row.m_iLastIssueUnix <= 0)
			return 0;

		int cooldown = CooldownSeconds();
		int elapsed = nowUnix - row.m_iLastIssueUnix;
		if (elapsed < 0)
		{
			LF_Debug.Warn(FEATURE, string.Format("the clock is before %1's last issue (%2 < %3): the cooldown restarts now", uid, nowUnix, row.m_iLastIssueUnix));
			row.m_iLastIssueUnix = nowUnix;
			return cooldown;
		}

		return Math.Max(cooldown - elapsed, 0);
	}

	//------------------------------------------------------------------------------------------------
	//! A kit was issued at nowUnix with these serialled lines (may be empty: a kit with no
	//! serialled line still starts the cooldown).
	static void RecordIssue(string uid, int nowUnix, array<string> serials)
	{
		if (uid.IsEmpty())
		{
			LF_Debug.Error(FEATURE, "issue not booked: no officer id");
			return;
		}

		LF_ArmoryKitRow row = FindOrAdd(uid);
		row.m_iLastIssueUnix = nowUnix;
		if (serials)
		{
			foreach (string serial : serials)
			{
				if (!serial.IsEmpty() && row.m_aSerials.Find(serial) == -1)
					row.m_aSerials.Insert(serial);
			}
		}

		ClearLostAsk(uid);
		LF_Debug.Info(FEATURE, string.Format("kit issued to %1 at %2, %3 serial(s) out", uid, nowUnix, row.m_aSerials.Count()));
	}

	//------------------------------------------------------------------------------------------------
	//! These serials came back to the armory. Serials not on this officer's book are ignored.
	//! \return Serials cleared.
	static int TurnIn(string uid, notnull array<string> serials)
	{
		LF_ArmoryKitRow row = Find(uid);
		if (!row)
			return 0;

		int cleared;
		foreach (string serial : serials)
		{
			int index = row.m_aSerials.Find(serial);
			if (index == -1)
				continue;

			row.m_aSerials.Remove(index);
			cleared++;
		}

		if (cleared > 0)
		{
			ClearLostAsk(uid);
			LF_Debug.Info(FEATURE, string.Format("%1 turned in %2 serial(s), %3 still out", uid, cleared, row.m_aSerials.Count()));
		}

		return cleared;
	}

	//------------------------------------------------------------------------------------------------
	//! The first use with kit out and none of it carried warns and returns false; a second use
	//! within LOST_CONFIRM_SECONDS returns true (the officer confirms the report).
	static bool ConfirmLost(string uid, int nowUnix)
	{
		if (!s_mLostAsked)
			s_mLostAsked = new map<string, int>();

		int asked;
		if (s_mLostAsked.Find(uid, asked) && nowUnix >= asked && nowUnix - asked <= LOST_CONFIRM_SECONDS)
		{
			s_mLostAsked.Remove(uid);
			return true;
		}

		s_mLostAsked.Set(uid, nowUnix);
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Every serial of this officer's kit is reported lost: the book clears them and logs each one,
	//! so staff can follow a lost gun. The cooldown still runs from the last issue.
	//! \return Serials reported.
	static int ReportLost(string uid)
	{
		LF_ArmoryKitRow row = Find(uid);
		if (!row || row.m_aSerials.IsEmpty())
			return 0;

		int reported = row.m_aSerials.Count();
		foreach (string serial : row.m_aSerials)
		{
			LF_Debug.Warn(FEATURE, string.Format("%1 reported serial %2 lost", uid, serial));
		}

		row.m_aSerials.Clear();
		ClearLostAsk(uid);
		return reported;
	}

	//------------------------------------------------------------------------------------------------
	protected static void ClearLostAsk(string uid)
	{
		if (s_mLostAsked)
			s_mLostAsked.Remove(uid);
	}

	//------------------------------------------------------------------------------------------------
	static array<ref LF_ArmoryKitRow> ExportAll()
	{
		array<ref LF_ArmoryKitRow> records = new array<ref LF_ArmoryKitRow>();
		foreach (LF_ArmoryKitRow row : Rows())
		{
			records.Insert(row.Copy());
		}

		return records;
	}

	//------------------------------------------------------------------------------------------------
	//! Replaces the book (idempotent). A row with no officer or a repeated officer is skipped.
	//! \return Rows applied.
	static int ApplyAll(array<ref LF_ArmoryKitRow> records)
	{
		s_aRows = new array<ref LF_ArmoryKitRow>();
		s_mLostAsked = null;
		if (!records)
			return 0;

		foreach (LF_ArmoryKitRow record : records)
		{
			if (!record || record.m_sOfficerUid.IsEmpty() || Find(record.m_sOfficerUid))
			{
				LF_Debug.Warn(FEATURE, "load skipped an armory row with no officer or a repeated officer");
				continue;
			}

			s_aRows.Insert(record.Copy());
		}

		return s_aRows.Count();
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_ArmoryKits's statics (registered at script load; Core/LF_StaticReset.c).
class LF_ArmoryKits_StaticResetHook : LF_StaticResetHook
{
	protected static bool s_bRegistered = LF_StaticReset.Register(new LF_ArmoryKits_StaticResetHook());

	override string Owner()
	{
		return "LF_ArmoryKits";
	}

	override void Apply()
	{
		LF_ArmoryKits.ResetStatics();
	}
}
