//------------------------------------------------------------------------------------------------
//! Ban record (#1285, epic #1006). Every vanilla `#ban create` and `#ban remove` (the unban) that the
//! server accepts appends one row to $profile:LifeFramework/ban_record.txt:
//!   V1|ACTION|targetUid|unix|staffUid|durationSeconds|incidentIds|reason
//! ACTION is BAN or UNBAN; incidentIds is a comma list of the target's recent LF_IncidentLog numbers
//! (empty for none); reason is last and has its separators cleaned. The version leads every row, so a
//! later layout adds V2 beside V1; a row the reader does not know is skipped with a warning.
//! The file is append-only and not part of the save, so it needs no persistence version.
//! Enforcement stays vanilla; this is the staff memory of why. `#banrecord <uid>` lists the rows.
//! Server only.
//------------------------------------------------------------------------------------------------
class LF_BanRow : Managed
{
	string m_sAction;
	string m_sTargetUid;
	int m_iTime;
	string m_sStaffUid;
	int m_iDuration;
	ref array<int> m_aIncidentIds = {};
	string m_sReason;
}

class LF_BanRecord
{
	static const string FEATURE = "BanRecord";
	static const string ROW_VERSION = "V1";
	static const int ROW_FIELDS = 8;
	static const string ACTION_BAN = "BAN";
	static const string ACTION_UNBAN = "UNBAN";
	//! The staff uid of a ban sent over RCON (no player behind it).
	static const string STAFF_RCON = "rcon";
	//! How many of the target's incidents a row names (the newest). How far back is the incident
	//! log's own retention (LF_IncidentConfig).
	static const int INCIDENT_MAX = 10; // lf-fixed: bounds one text row, not a gameplay rule

	protected static string s_sPath = "";

	//------------------------------------------------------------------------------------------------
	//! Tests point this at a private file. An empty path uses the profile file.
	static void SetSavePath(string path)
	{
		s_sPath = path;
	}

	//------------------------------------------------------------------------------------------------
	static string GetPath()
	{
		if (!s_sPath.IsEmpty())
			return s_sPath;

		return "$profile:LifeFramework/ban_record.txt";
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: a field safe for the row (no separator, no line break, trimmed).
	static string Clean(string text)
	{
		text.Replace("|", "/");
		text.Replace("\r", " ");
		text.Replace("\n", " ");
		text.TrimInPlace();
		return text;
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: incident numbers as "3,7,9" ("" for none).
	static string JoinIds(notnull array<int> ids)
	{
		string text = "";
		foreach (int i, int id : ids)
		{
			if (i > 0)
				text = text + ",";
			text = text + id.ToString();
		}
		return text;
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: one row as a line.
	static string FormatRow(notnull LF_BanRow row)
	{
		string ids = JoinIds(row.m_aIncidentIds);
		return string.Format("%1|%2|%3|%4|%5|%6|%7|%8", ROW_VERSION, Clean(row.m_sAction), Clean(row.m_sTargetUid), row.m_iTime, Clean(row.m_sStaffUid), row.m_iDuration, ids, Clean(row.m_sReason));
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: a line as a row, or null for a legacy or malformed line.
	static LF_BanRow ParseRow(string line)
	{
		array<string> parts = {};
		line.Split("|", parts, false);
		if (parts.Count() != ROW_FIELDS || parts[0] != ROW_VERSION)
			return null;

		if (parts[1] != ACTION_BAN && parts[1] != ACTION_UNBAN)
			return null;

		if (parts[2].IsEmpty())
			return null;

		LF_BanRow row = new LF_BanRow();
		row.m_sAction = parts[1];
		row.m_sTargetUid = parts[2];
		row.m_iTime = parts[3].ToInt();
		row.m_sStaffUid = parts[4];
		row.m_iDuration = parts[5].ToInt();
		array<string> ids = {};
		parts[6].Split(",", ids, true);
		foreach (string id : ids)
		{
			int number = id.ToInt();
			if (number > 0)
				row.m_aIncidentIds.Insert(number);
		}
		row.m_sReason = parts[7];
		return row;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the numbers of the target's incidents (as either party) the log still holds, the newest
	//! INCIDENT_MAX, oldest first.
	static array<int> RecentIncidentIds(string uid)
	{
		array<int> ids = {};
		if (uid.IsEmpty())
			return ids;

		array<ref LF_IncidentRecord> records = LF_IncidentLog.Query(vector.Zero, -1, 0, -1, "");
		foreach (LF_IncidentRecord record : records)
		{
			if (record.m_sSubjectUid == uid || record.m_sOtherUid == uid)
				ids.Insert(record.m_iNumber);
		}

		while (ids.Count() > INCIDENT_MAX)
			ids.RemoveOrdered(0);

		return ids;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: appends one row. Refuses (logged) a row with no target or an unknown action.
	//! \return True when the row was written.
	static bool Record(string action, string targetUid, string staffUid, string reason, int duration, int now)
	{
		if (!Replication.IsServer())
			return false;

		if (targetUid.IsEmpty() || (action != ACTION_BAN && action != ACTION_UNBAN))
		{
			LF_Debug.Warn(FEATURE, string.Format("ban record refused: action '%1' target '%2'", action, targetUid));
			return false;
		}

		LF_BanRow row = new LF_BanRow();
		row.m_sAction = action;
		row.m_sTargetUid = targetUid;
		row.m_iTime = now;
		row.m_sStaffUid = staffUid;
		row.m_iDuration = duration;
		row.m_sReason = reason;
		if (action == ACTION_BAN)
			row.m_aIncidentIds = RecentIncidentIds(targetUid);

		string path = GetPath();
		string parent = FilePath.StripFileName(path);
		if (!parent.IsEmpty())
			FileIO.MakeDirectory(parent);

		FileMode mode = FileMode.APPEND;
		if (!FileIO.FileExists(path))
			mode = FileMode.WRITE;

		FileHandle handle = FileIO.OpenFile(path, mode);
		if (!handle)
		{
			LF_Debug.Error(FEATURE, string.Format("cannot write the ban record: %1", path));
			return false;
		}

		string line = FormatRow(row);
		handle.WriteLine(line);
		handle.Close();
		LF_Debug.Info(FEATURE, "recorded " + line);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: every row about uid, oldest first (all rows for an empty uid). Legacy and malformed
	//! lines are skipped with one warning each.
	static array<ref LF_BanRow> ReadFor(string uid)
	{
		array<ref LF_BanRow> rows = new array<ref LF_BanRow>();
		string path = GetPath();
		if (!FileIO.FileExists(path))
			return rows;

		FileHandle handle = FileIO.OpenFile(path, FileMode.READ);
		if (!handle)
		{
			LF_Debug.Error(FEATURE, string.Format("cannot open the ban record: %1", path));
			return rows;
		}

		string line;
		int lineNumber = 0;
		while (handle.ReadLine(line) >= 0)
		{
			lineNumber++;
			if (line.Trim().IsEmpty())
				continue;

			LF_BanRow row = ParseRow(line);
			if (!row)
			{
				LF_Debug.Warn(FEATURE, string.Format("skipped ban record line %1 (legacy or malformed): %2", lineNumber, line));
				continue;
			}

			if (uid.IsEmpty() || row.m_sTargetUid == uid)
				rows.Insert(row);
		}
		handle.Close();
		return rows;
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: the rows as the text an admin reads.
	static string Describe(string uid, notnull array<ref LF_BanRow> rows)
	{
		if (rows.IsEmpty())
			return string.Format("No ban record for %1", uid);

		string text = string.Format("Ban record for %1 (%2):", uid, rows.Count());
		foreach (LF_BanRow row : rows)
		{
			string ids = JoinIds(row.m_aIncidentIds);
			if (ids.IsEmpty())
				ids = "-";

			text = text + string.Format("\nunix %1 %2 by %3, %4 s, incidents %5: %6", row.m_iTime, row.m_sAction, row.m_sStaffUid, row.m_iDuration, ids, row.m_sReason);
		}
		return text;
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: the account uid a vanilla ban id names. A connected player id or name becomes that
	//! player's account uid; an identity id is already one; anything else is kept as typed.
	static string TargetUidOf(string id)
	{
		if (id.IsEmpty())
			return "";

		PlayerManager playerManager = GetGame().GetPlayerManager();
		if (!playerManager)
			return id;

		if (id.Length() != 36 && SCR_StringHelper.IsFormat(SCR_EStringFormat.DIGITS_ONLY, id))
		{
			string uid = LF_Utils.GetAccountUID(id.ToInt());
			if (!uid.IsEmpty())
				return uid;
			return id;
		}

		array<int> playerIds = {};
		playerManager.GetPlayers(playerIds);
		foreach (int playerId : playerIds)
		{
			if (playerManager.GetPlayerName(playerId) == id)
			{
				string named = LF_Utils.GetAccountUID(playerId);
				if (!named.IsEmpty())
					return named;
			}
		}
		return id;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a vanilla #ban the server accepted (result PENDING). argv is the command after vanilla
	//! parsed it: [ban, create|remove, id, duration, reason...]. Help, list and refused commands write
	//! nothing. Call it before the command's admin incident so the ban's own row is not among its ids.
	static void RecordCommand(array<string> argv, string staffUid, ScrServerCmdResult result)
	{
		if (!argv || argv.Count() < 3 || !result || result.m_eResultType != EServerCmdResultType.PENDING)
			return;

		string action;
		if (argv[1] == "create")
			action = ACTION_BAN;
		else if (argv[1] == "remove")
			action = ACTION_UNBAN;
		else
			return;

		int duration = 0;
		string reason = "";
		if (action == ACTION_BAN && argv.Count() > 3)
		{
			duration = argv[3].ToInt();
			for (int i = 4; i < argv.Count(); i++)
			{
				if (!reason.IsEmpty())
					reason = reason + " ";
				reason = reason + argv[i];
			}
		}

		Record(action, TargetUidOf(argv[2]), staffUid, reason, duration, System.GetUnixTime());
	}
}

//------------------------------------------------------------------------------------------------
//! `#banrecord <uid>`: the ban record rows of one account, answered to the admin who asked (chat) or
//! the RCON console. Administrators only, as vanilla #ban.
class LF_BanRecordCommand : ScrServerCommand
{
	//------------------------------------------------------------------------------------------------
	override string GetKeyword()
	{
		return "banrecord";
	}

	//------------------------------------------------------------------------------------------------
	override bool IsServerSide()
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override int RequiredRCONPermission()
	{
		return ERCONPermissions.PERMISSIONS_ADMIN;
	}

	//------------------------------------------------------------------------------------------------
	override int RequiredChatPermission()
	{
		return EPlayerRole.ADMINISTRATOR;
	}

	//------------------------------------------------------------------------------------------------
	protected ScrServerCmdResult List(array<string> argv)
	{
		if (!argv || argv.Count() < 2 || argv[1].IsEmpty())
			return ScrServerCmdResult("#banrecord <account uid>", EServerCmdResultType.PARAMETERS);

		string uid = LF_BanRecord.TargetUidOf(argv[1]);
		return ScrServerCmdResult(LF_BanRecord.Describe(uid, LF_BanRecord.ReadFor(uid)), EServerCmdResultType.OK);
	}

	//------------------------------------------------------------------------------------------------
	override ref ScrServerCmdResult OnChatServerExecution(array<string> argv, int playerId)
	{
		return List(argv);
	}

	//------------------------------------------------------------------------------------------------
	override ref ScrServerCmdResult OnChatClientExecution(array<string> argv, int playerId)
	{
		return ScrServerCmdResult(string.Empty, EServerCmdResultType.OK);
	}

	//------------------------------------------------------------------------------------------------
	override ref ScrServerCmdResult OnRCONExecution(array<string> argv)
	{
		return List(argv);
	}

	//------------------------------------------------------------------------------------------------
	override ref ScrServerCmdResult OnUpdate()
	{
		return ScrServerCmdResult(string.Empty, EServerCmdResultType.OK);
	}
}
