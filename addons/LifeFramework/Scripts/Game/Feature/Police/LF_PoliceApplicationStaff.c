//------------------------------------------------------------------------------------------------
//! Staff side of police applications: list the open ones, approve or reject one by account uid.
//! One server API over LF_PoliceApplications (its gates and the cooldown included). Who is staff is
//! decided here, on the server: the account uid is on the staff roster file, or the engine says the
//! player is an administrator (at opening that is the maintainer only). A caller that is neither is
//! refused and logged, and no client field is trusted: the target uid is validated, and the sender is
//! resolved by the caller from the player controller, never read from a payload.
//! Roster file: $profile:LifeFramework/staff.txt, one account uid per line, `#` starts a comment.
//! A line may carry an optional level after the uid (`<uid> moderator|operator|administrator`) and a
//! trailing `# display name`; the level is for tools\cli staff roster check|admins (#1320), a listed
//! uid is staff whatever the level. It is read on every call, so an edit takes effect without a restart.
//------------------------------------------------------------------------------------------------
class LF_PoliceApplicationStaff : LF_PoliceApplications
{
	static const int UID_MAX_LENGTH = 64;
	protected static const string UID_CHARS = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ-_";

	protected static string s_sStaffPath = "";

	//------------------------------------------------------------------------------------------------
	//! Tests point this at a private file. An empty path uses the profile file.
	static void SetStaffPath(string path)
	{
		s_sStaffPath = path;
	}

	//------------------------------------------------------------------------------------------------
	protected static string GetStaffPath()
	{
		if (!s_sStaffPath.IsEmpty())
			return s_sStaffPath;

		return "$profile:LifeFramework/staff.txt";
	}

	//------------------------------------------------------------------------------------------------
	//! One roster line: `<uid> [level] [# comment]`. False for a blank or comment-only line.
	static bool ParseRosterLine(string line, out string uid, out string level)
	{
		uid = "";
		level = "";
		int hash = line.IndexOf("#");
		if (hash >= 0)
			line = line.Substring(0, hash);

		line = line.Trim();
		if (line.IsEmpty())
			return false;

		int cut = line.Length();
		int space = line.IndexOf(" ");
		if (space >= 0 && space < cut)
			cut = space;

		int tab = line.IndexOf("\t");
		if (tab >= 0 && tab < cut)
			cut = tab;

		uid = line.Substring(0, cut);
		level = line.Substring(cut, line.Length() - cut).Trim();
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Every account uid on the staff roster file. A missing file is an empty roster.
	static void ReadRoster(out array<string> uids)
	{
		uids = new array<string>();
		string path = GetStaffPath();
		if (!FileIO.FileExists(path))
			return;

		FileHandle handle = FileIO.OpenFile(path, FileMode.READ);
		if (!handle)
		{
			LF_Debug.Error("Police", string.Format("cannot open the staff roster: %1", path));
			return;
		}

		string line;
		while (handle.ReadLine(line) >= 0)
		{
			string uid;
			string level;
			if (ParseRosterLine(line, uid, level))
				uids.Insert(uid);
		}
		handle.Close();
	}

	//------------------------------------------------------------------------------------------------
	//! Server: whether the account may use the staff tools.
	//! \param engineAdmin The engine's ADMINISTRATOR role for the sender (AdminChatChannel.IsAdminPlayer).
	static bool IsStaff(string callerUid, bool engineAdmin)
	{
		if (engineAdmin)
			return true;

		if (callerUid.IsEmpty())
			return false;

		array<string> roster;
		ReadRoster(roster);
		return roster.Find(callerUid) != -1;
	}

	//------------------------------------------------------------------------------------------------
	//! A target uid the file format and the roster can carry: 1..UID_MAX_LENGTH of letters, digits, - and _.
	static bool IsValidUid(string uid)
	{
		int length = uid.Length();
		if (length < 1 || length > UID_MAX_LENGTH)
			return false;

		for (int i = 0; i < length; i++)
		{
			if (UID_CHARS.IndexOf(uid.Get(i)) < 0)
				return false;
		}
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the common refusal for a staff call, "" when the caller is staff.
	protected static string RefuseCaller(string verb, string callerUid, bool engineAdmin)
	{
		if (IsStaff(callerUid, engineAdmin))
			return "";

		LF_Debug.Warn("Police", string.Format("staff %1 refused: %2 is not staff", verb, callerUid));
		return "#LF-PoliceApp_NotAdmin";
	}

	//------------------------------------------------------------------------------------------------
	//! Server: copies of the open applications, oldest first. A non-staff caller gets an empty list
	//! and the refusal key.
	//! \param[out] refusal "" when listed, else the localization key.
	static array<ref LF_PoliceApplication> ListOpen(string callerUid, bool engineAdmin, out string refusal)
	{
		array<ref LF_PoliceApplication> rows = new array<ref LF_PoliceApplication>();
		refusal = RefuseCaller("list", callerUid, engineAdmin);
		if (!refusal.IsEmpty())
			return rows;

		foreach (LF_PoliceApplication app : All())
		{
			if (app.m_sStatus != STATUS_OPEN)
				continue;

			LF_PoliceApplication copy = new LF_PoliceApplication();
			copy.m_sStatus = app.m_sStatus;
			copy.m_sUid = app.m_sUid;
			copy.m_sName = app.m_sName;
			copy.m_sWhy = app.m_sWhy;
			copy.m_sAvailability = app.m_sAvailability;
			copy.m_iTime = app.m_iTime;
			copy.m_iRejectedAt = app.m_iRejectedAt;
			copy.m_iTrialEndsAt = app.m_iTrialEndsAt;
			rows.Insert(copy);
		}
		return rows;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: shared checks of approve and reject. "" when the call may go on.
	protected static string RefuseDecision(string verb, string callerUid, bool engineAdmin, string targetUid)
	{
		string refusal = RefuseCaller(verb, callerUid, engineAdmin);
		if (!refusal.IsEmpty())
			return refusal;

		if (!IsValidUid(targetUid))
		{
			LF_Debug.Warn("Police", string.Format("staff %1 by %2 refused: the target uid is not valid", verb, callerUid));
			return "#LF-PoliceApp_Error_NoAccount";
		}

		if (!HasOpen(targetUid))
		{
			LF_Debug.Warn("Police", string.Format("staff %1 by %2 refused: %3 has no open application", verb, callerUid, targetUid));
			return "#LF-PoliceApp_NoneOpen";
		}

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Server: approves the open application of targetUid and puts them on the police roster.
	//! \return "" when approved, else the localization key of the refusal.
	static string ApproveByUid(string callerUid, bool engineAdmin, string targetUid)
	{
		string refusal = RefuseDecision("approve", callerUid, engineAdmin, targetUid);
		if (!refusal.IsEmpty())
			return refusal;

		if (!Approve(targetUid, true))
			return "#LF-PoliceApp_NoneOpen";

		LF_Debug.Info("Police", string.Format("staff %1 approved the application of %2", callerUid, targetUid));
		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Server: rejects the open application of targetUid; the reapply cooldown starts at now.
	//! \return "" when rejected, else the localization key of the refusal.
	static string RejectByUid(string callerUid, bool engineAdmin, string targetUid, int now)
	{
		string refusal = RefuseDecision("reject", callerUid, engineAdmin, targetUid);
		if (!refusal.IsEmpty())
			return refusal;

		if (!Reject(targetUid, true, now))
			return "#LF-PoliceApp_NoneOpen";

		LF_Debug.Info("Police", string.Format("staff %1 rejected the application of %2", callerUid, targetUid));
		return "";
	}
}
