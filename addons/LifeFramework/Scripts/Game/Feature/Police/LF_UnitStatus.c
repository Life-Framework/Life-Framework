//------------------------------------------------------------------------------------------------
//! A unit's radio status on the dispatch board (PO-D3, #4204).
enum LF_EUnitStatus
{
	NONE,
	AVAILABLE,
	BUSY
}

//------------------------------------------------------------------------------------------------
//! Unit status (PO-D3, #4204): an officer on duty radios "available" or "busy" from the police car
//! (LF_UnitStatusAction). The server keeps one status per officer UID; every officer's dispatch board
//! read carries the rows (the modded LF_WantedBoard below), and clocking out clears the officer's row.
//! Session state only: a restart starts every unit with no status.
class LF_UnitStatus
{
	static const string FEATURE = "UnitStatus";
	static const string ENTRY_PREFIX = "unit";
	static const string FIELD_SEPARATOR = "|";

	protected static ref map<string, int> s_mStatus;
	protected static ref map<string, string> s_mName;

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_mStatus = null;
		s_mName = null;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: sets an officer's status. \param onDutyOfficer The server's own duty check of the account.
	//! \return false when refused (off duty, no UID, or NONE); each refusal is logged.
	static bool Set(string uid, string name, LF_EUnitStatus status, bool onDutyOfficer)
	{
		if (uid.IsEmpty())
		{
			LF_Debug.Warn(FEATURE, "status refused: no officer UID");
			return false;
		}

		if (!onDutyOfficer)
		{
			LF_Debug.Warn(FEATURE, string.Format("status refused for %1: not an officer on duty", uid));
			return false;
		}

		if (status != LF_EUnitStatus.AVAILABLE && status != LF_EUnitStatus.BUSY)
		{
			LF_Debug.Warn(FEATURE, string.Format("status refused for %1: %2 is not a status to set", uid, status));
			return false;
		}

		if (!s_mStatus)
			s_mStatus = new map<string, int>();
		if (!s_mName)
			s_mName = new map<string, string>();

		s_mStatus.Set(uid, status);
		s_mName.Set(uid, name);
		LF_Debug.Info(FEATURE, string.Format("unit %1 (%2) is now %3", uid, name, typename.EnumToString(LF_EUnitStatus, status)));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	static LF_EUnitStatus Get(string uid)
	{
		if (!s_mStatus || !s_mStatus.Contains(uid))
			return LF_EUnitStatus.NONE;

		return s_mStatus.Get(uid);
	}

	//------------------------------------------------------------------------------------------------
	//! The name the unit radioed under; empty when unknown.
	static string NameOf(string uid)
	{
		if (!s_mName || !s_mName.Contains(uid))
			return string.Empty;

		return s_mName.Get(uid);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: drops an officer's status (clock out). \return true when there was one.
	static bool Clear(string uid)
	{
		if (!s_mStatus || !s_mStatus.Contains(uid))
			return false;

		s_mStatus.Remove(uid);
		if (s_mName)
			s_mName.Remove(uid);

		LF_Debug.Info(FEATURE, string.Format("unit %1 status cleared", uid));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! "unit|<status>|<name>": the name is last, so a name holding the separator stays whole.
	static string BoardEntry(string name, LF_EUnitStatus status)
	{
		return ENTRY_PREFIX + FIELD_SEPARATOR + status.ToString() + FIELD_SEPARATOR + name;
	}

	//------------------------------------------------------------------------------------------------
	static bool IsBoardEntry(string entry)
	{
		return entry.StartsWith(ENTRY_PREFIX + FIELD_SEPARATOR);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: one row per unit with a status, by name. A unit whose account is no longer an officer
	//! on duty (a clock-out that skipped OnDutyChanged) is dropped here and logged.
	static void BuildBoardEntries(notnull array<string> outEntries)
	{
		outEntries.Clear();
		if (!s_mStatus || s_mStatus.IsEmpty())
			return;

		array<string> stale = {};
		array<string> rows = {};
		foreach (string uid, int status : s_mStatus)
		{
			if (!IsOnDuty(uid))
			{
				stale.Insert(uid);
				continue;
			}

			rows.Insert(BoardEntry(NameOf(uid), status));
		}

		foreach (string uid : stale)
		{
			LF_Debug.Info(FEATURE, string.Format("unit %1 is off duty; its row is dropped", uid));
			Clear(uid);
		}

		rows.Sort();
		outEntries.InsertAll(rows);
	}

	//------------------------------------------------------------------------------------------------
	protected static bool IsOnDuty(string uid)
	{
		LF_PlayerAccountManager accounts = LF_PlayerAccountManager.GetInstance();
		LF_PlayerAccount account;
		if (accounts)
			account = accounts.GetFromCache(uid);

		return LF_PoliceUtils.IsOnDutyOfficer(account);
	}

	//------------------------------------------------------------------------------------------------
	//! Client: one unit entry as a board line, "Unit Dupont: busy". A malformed entry reads as empty.
	static string BoardLine(string entry)
	{
		array<string> fields = {};
		entry.Split(FIELD_SEPARATOR, fields, false);
		if (fields.Count() < 3 || fields[0] != ENTRY_PREFIX)
		{
			LF_Debug.Error(FEATURE, string.Format("unit board entry dropped: '%1'", entry));
			return string.Empty;
		}

		string name = fields[2];
		for (int i = 3; i < fields.Count(); i++)
		{
			name += FIELD_SEPARATOR + fields[i];
		}

		if (name.IsEmpty())
			name = WidgetManager.Translate("#LF-UnitStatus_NoName");

		string state;
		if (fields[1].ToInt() == LF_EUnitStatus.BUSY)
			state = WidgetManager.Translate("#LF-UnitStatus_Busy");
		else
			state = WidgetManager.Translate("#LF-UnitStatus_Available");

		return WidgetManager.Translate("#LF-UnitStatus_BoardLine", name, state);
	}
}

//------------------------------------------------------------------------------------------------
class LF_UnitStatusStaticReset : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_UnitStatus";
	}

	override void Apply()
	{
		LF_UnitStatus.ResetStatics();
	}
}

//------------------------------------------------------------------------------------------------
//! Clocking out clears the unit's status. Both entry points: a static call inside the base class
//! binds to the base method, so OnDutyChanged does not reach the OnDutyChangedAt override.
modded class LF_PoliceRanks
{
	override static void OnDutyChanged(LF_PlayerAccount account, bool nowOnDuty)
	{
		super.OnDutyChanged(account, nowOnDuty);
		if (!nowOnDuty && account && Replication.IsServer())
			LF_UnitStatus.Clear(account.GetPersistentId());
	}

	//! The officer stays on duty while away (LF_PoliceRanks.OnPlayerLeft), but their radio row goes.
	override static void OnPlayerLeft(int playerId)
	{
		super.OnPlayerLeft(playerId);
		if (!Replication.IsServer() || !LF_PlayerAccountManager.GetInstance())
			return;

		LF_PlayerAccount account = LF_PlayerAccountManager.GetInstance().GetFromCache(LF_Utils.GetPlayerUID(playerId));
		if (account)
			LF_UnitStatus.Clear(account.GetPersistentId());
	}

	override static void OnDutyChangedAt(LF_PlayerAccount account, bool nowOnDuty, int nowMs)
	{
		super.OnDutyChangedAt(account, nowOnDuty, nowMs);
		if (!nowOnDuty && account && Replication.IsServer())
			LF_UnitStatus.Clear(account.GetPersistentId());
	}
}

//------------------------------------------------------------------------------------------------
//! The dispatch board carries the unit rows for officers: appended to the answer after the dispatch
//! log, read back into their own section above the close row.
modded class LF_WantedBoard
{
	override static LF_BoardAnswer BuildAnswer(int playerId)
	{
		LF_BoardAnswer answer = super.BuildAnswer(playerId);
		if (!answer || !answer.m_bPrivileged)
			return answer;

		array<string> units = {};
		LF_UnitStatus.BuildBoardEntries(units);
		answer.m_aTexts.InsertAll(units);
		return answer;
	}

	override static array<ref LF_UIPart> BuildParts(bool police, array<string> names, array<string> offences, array<string> clothing, array<string> vehicles, array<string> places, array<int> levels, array<string> dispatches = null)
	{
		array<string> units = {};
		array<string> others;
		if (dispatches)
		{
			others = {};
			foreach (string entry : dispatches)
			{
				if (LF_UnitStatus.IsBoardEntry(entry))
					units.Insert(entry);
				else
					others.Insert(entry);
			}
		}

		array<ref LF_UIPart> parts = super.BuildParts(police, names, offences, clothing, vehicles, places, levels, others);
		if (!police || units.IsEmpty() || !parts)
			return parts;

		// Above the close row, which BuildParts always puts last.
		int at = parts.Count();
		if (at > 0)
			at--;

		parts.InsertAt(LF_UIPart.Create(LF_EUIPartKind.LINE, "UnitHeading", WidgetManager.Translate("#LF-UnitStatus_BoardHeading")), at);
		at++;
		foreach (int index, string unit : units)
		{
			string line = LF_UnitStatus.BoardLine(unit);
			if (line.IsEmpty())
				continue;

			parts.InsertAt(LF_UIPart.Create(LF_EUIPartKind.LINE, "Unit" + index, line), at);
			at++;
		}

		return parts;
	}
}
