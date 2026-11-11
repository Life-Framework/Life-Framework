//------------------------------------------------------------------------------------------------
//! Server record of police applications, a text file under the profile
//! ($profile:LifeFramework/police_applications.txt, one `STATUS|uid|name|why|availability|unix`
//! line each; STATUS is OPEN or APPROVED). It is not part of the save, so it needs no persistence
//! version. Approval writes the police roster through LF_WhitelistManager.
//------------------------------------------------------------------------------------------------
class LF_PoliceApplication
{
	string m_sStatus;
	string m_sUid;
	string m_sName;
	string m_sWhy;
	string m_sAvailability;
	int m_iTime;
}

class LF_PoliceApplications
{
	static const string STATUS_OPEN = "OPEN";
	static const string STATUS_APPROVED = "APPROVED";

	protected static ref array<ref LF_PoliceApplication> s_aApplications;
	protected static string s_sPath = "";

	//------------------------------------------------------------------------------------------------
	//! Tests point this at a private file. An empty path uses the profile file.
	static void SetSavePath(string path)
	{
		s_sPath = path;
		s_aApplications = null;
	}

	//------------------------------------------------------------------------------------------------
	protected static string GetPath()
	{
		if (!s_sPath.IsEmpty())
			return s_sPath;

		return "$profile:LifeFramework/police_applications.txt";
	}

	//------------------------------------------------------------------------------------------------
	protected static array<ref LF_PoliceApplication> All()
	{
		if (!s_aApplications)
			Load();

		return s_aApplications;
	}

	//------------------------------------------------------------------------------------------------
	static bool HasOpen(string uid)
	{
		return FindOpen(uid) != null;
	}

	//------------------------------------------------------------------------------------------------
	static LF_PoliceApplication FindOpen(string uid)
	{
		foreach (LF_PoliceApplication app : All())
		{
			if (app.m_sStatus == STATUS_OPEN && app.m_sUid == uid)
				return app;
		}
		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! The application waiting longest, or null.
	static LF_PoliceApplication FindOldestOpen()
	{
		foreach (LF_PoliceApplication app : All())
		{
			if (app.m_sStatus == STATUS_OPEN)
				return app;
		}
		return null;
	}

	//------------------------------------------------------------------------------------------------
	static int CountOpen()
	{
		int count = 0;
		foreach (LF_PoliceApplication app : All())
		{
			if (app.m_sStatus == STATUS_OPEN)
				count++;
		}
		return count;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: records an application. \return "" when filed, else the refusal key.
	static string Submit(string uid, string name, string why, string availability, bool onRoster)
	{
		if (uid.IsEmpty())
			return "#LF-PoliceApp_Error_NoAccount";

		name = LF_PoliceApplicationRules.Clean(name);
		why = LF_PoliceApplicationRules.Clean(why);
		availability = LF_PoliceApplicationRules.Clean(availability);

		string refusal = LF_PoliceApplicationRules.RefuseForm(name, why, availability);
		if (refusal.IsEmpty())
			refusal = LF_PoliceApplicationRules.RefuseSubmit(onRoster, HasOpen(uid));
		if (!refusal.IsEmpty())
		{
			LF_Debug.Info("Police", string.Format("police application from %1 refused: %2", uid, refusal));
			return refusal;
		}

		LF_PoliceApplication app = new LF_PoliceApplication();
		app.m_sStatus = STATUS_OPEN;
		app.m_sUid = uid;
		app.m_sName = name;
		app.m_sWhy = why;
		app.m_sAvailability = availability;
		app.m_iTime = System.GetUnixTime();
		All().Insert(app);
		Save();
		LF_Debug.Info("Police", string.Format("police application filed by %1 (%2): %3 / %4", uid, name, why, availability));
		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Server: approves the open application of uid and puts them on the police roster.
	//! \return True when an open application was approved.
	static bool Approve(string uid, bool approverIsAdmin)
	{
		if (!LF_PoliceApplicationRules.CanApprove(approverIsAdmin))
		{
			LF_Debug.Warn("Police", string.Format("approval of %1 refused: approver is not an administrator", uid));
			return false;
		}

		return ApproveInstant(uid);
	}

	//------------------------------------------------------------------------------------------------
	//! Server, playtest missions only: approves with no admin present.
	static bool ApproveInstant(string uid)
	{
		LF_PoliceApplication app = FindOpen(uid);
		if (!app)
		{
			LF_Debug.Warn("Police", string.Format("approval refused: %1 has no open application", uid));
			return false;
		}

		LF_WhitelistManager.AddFactionWhitelist(app.m_sUid, LF_EFactionType.POLICE);
		app.m_sStatus = STATUS_APPROVED;
		Save();
		LF_Debug.Info("Police", string.Format("police application of %1 (%2) approved, added to the police roster", app.m_sUid, app.m_sName));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected static void Load()
	{
		s_aApplications = new array<ref LF_PoliceApplication>();
		string path = GetPath();
		if (!FileIO.FileExists(path))
			return;

		FileHandle handle = FileIO.OpenFile(path, FileMode.READ);
		if (!handle)
		{
			LF_Debug.Error("Police", string.Format("cannot open the police applications file: %1", path));
			return;
		}

		string line;
		while (handle.ReadLine(line) >= 0)
		{
			array<string> parts = {};
			line.Split("|", parts, false);
			if (parts.Count() != 6)
				continue;

			LF_PoliceApplication app = new LF_PoliceApplication();
			app.m_sStatus = parts[0];
			app.m_sUid = parts[1];
			app.m_sName = parts[2];
			app.m_sWhy = parts[3];
			app.m_sAvailability = parts[4];
			app.m_iTime = parts[5].ToInt();
			s_aApplications.Insert(app);
		}
		handle.Close();
	}

	//------------------------------------------------------------------------------------------------
	protected static void Save()
	{
		string path = GetPath();
		string parent = FilePath.StripFileName(path);
		if (!parent.IsEmpty())
			FileIO.MakeDirectory(parent);

		FileHandle handle = FileIO.OpenFile(path, FileMode.WRITE);
		if (!handle)
		{
			LF_Debug.Error("Police", string.Format("cannot write the police applications file: %1", path));
			return;
		}

		foreach (LF_PoliceApplication app : All())
		{
			handle.WriteLine(string.Format("%1|%2|%3|%4|%5|%6", app.m_sStatus, app.m_sUid, app.m_sName, app.m_sWhy, app.m_sAvailability, app.m_iTime));
		}
		handle.Close();
	}
}
