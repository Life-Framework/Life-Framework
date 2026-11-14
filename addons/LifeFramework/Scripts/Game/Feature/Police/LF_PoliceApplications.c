//------------------------------------------------------------------------------------------------
//! Server record of police applications, a text file under the profile
//! ($profile:LifeFramework/police_applications.txt, one `STATUS|uid|name|why|availability|unix|rejectedAt|trialEndsAt`
//! line each; STATUS is OPEN, APPROVED, REJECTED or EXPIRED). It is not part of the save, so it needs
//! no persistence version; the reader still takes the older six-field rows (both new fields 0), and
//! the next write upgrades them. Approval writes the police roster through LF_WhitelistManager.
//! Gates (#803): playtime and rejection cooldown at Submit, a trial that ends the roster entry.
//------------------------------------------------------------------------------------------------
class LF_PoliceApplication
{
	string m_sStatus;
	string m_sUid;
	string m_sName;
	string m_sWhy;
	string m_sAvailability;
	int m_iTime;
	int m_iRejectedAt;	// unix time of the rejection, 0 for none
	int m_iTrialEndsAt;	// unix time the trial ends, 0 for no trial (or already promoted)
}

class LF_PoliceApplications
{
	static const string STATUS_OPEN = "OPEN";
	static const string STATUS_APPROVED = "APPROVED";
	static const string STATUS_REJECTED = "REJECTED";
	static const string STATUS_EXPIRED = "EXPIRED";
	static const int ROW_FIELDS_V1 = 6;
	static const int ROW_FIELDS = 8;

	protected static ref array<ref LF_PoliceApplication> s_aApplications;
	protected static string s_sPath = "";
	protected static int s_iLastSecondsLeft;

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
	//! The key an application is filed and approved under. The police roster is a whitelist, and
	//! whitelists are per account (#331): LF_AccountStateBits.FromAccount reads the roster by the
	//! account uid, so an application filed under a later character's key would never put anyone on it.
	static string ApplicantUid(int playerId)
	{
		return LF_Utils.GetAccountUID(playerId);
	}

	//------------------------------------------------------------------------------------------------
	//! Wait (seconds) the latest refusal named: the playtime or cooldown left. 0 for any other refusal.
	static int GetLastSecondsLeft()
	{
		return s_iLastSecondsLeft;
	}

	//------------------------------------------------------------------------------------------------
	//! Unix time of the latest rejection of uid, 0 for none.
	static int LastRejectedAt(string uid)
	{
		int latest = 0;
		foreach (LF_PoliceApplication app : All())
		{
			if (app.m_sUid == uid && app.m_iRejectedAt > latest)
				latest = app.m_iRejectedAt;
		}
		return latest;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: records an application with no gates (playtest missions, tests).
	//! \return "" when filed, else the refusal key.
	static string Submit(string uid, string name, string why, string availability, bool onRoster)
	{
		return SubmitGated(uid, name, why, availability, onRoster, 0, 0, 0, 0);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: records an application, refusing it under the playtime and cooldown gates (#803).
	//! A refusal names its reason; GetLastSecondsLeft is the time left. A gate at 0 hours is off.
	//! The cooldown reads the account's latest rejection from this file.
	//! \return "" when filed, else the refusal key.
	static string SubmitGated(string uid, string name, string why, string availability, bool onRoster, int playtimeSeconds, int minPlaytimeHours, int cooldownHours, int now)
	{
		s_iLastSecondsLeft = 0;
		if (uid.IsEmpty())
			return "#LF-PoliceApp_Error_NoAccount";

		name = LF_PoliceApplicationRules.Clean(name);
		why = LF_PoliceApplicationRules.Clean(why);
		availability = LF_PoliceApplicationRules.Clean(availability);

		string refusal = LF_PoliceApplicationRules.RefuseForm(name, why, availability);
		int secondsLeft;
		if (refusal.IsEmpty())
			refusal = LF_PoliceApplicationRules.RefuseSubmitGated(onRoster, HasOpen(uid), playtimeSeconds, minPlaytimeHours, LastRejectedAt(uid), cooldownHours, now, secondsLeft);
		if (!refusal.IsEmpty())
		{
			s_iLastSecondsLeft = secondsLeft;
			LF_Debug.Info("Police", string.Format("police application from %1 refused: %2 (%3 left)", uid, refusal, LF_PoliceApplicationRules.FormatDuration(secondsLeft)));
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

		return ApproveAt(uid, System.GetUnixTime(), LF_PoliceConfig.TrialHours());
	}

	//------------------------------------------------------------------------------------------------
	//! Server, playtest missions only: approves with no admin present.
	static bool ApproveInstant(string uid)
	{
		return ApproveAt(uid, System.GetUnixTime(), 0);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: approves the open application of uid at now. A trial of trialHours (0 for none)
	//! ends the roster entry unless the officer is promoted first.
	static bool ApproveAt(string uid, int now, int trialHours)
	{
		LF_PoliceApplication app = FindOpen(uid);
		if (!app)
		{
			LF_Debug.Warn("Police", string.Format("approval refused: %1 has no open application", uid));
			return false;
		}

		LF_WhitelistManager.AddFactionWhitelist(app.m_sUid, LF_EFactionType.POLICE);
		app.m_sStatus = STATUS_APPROVED;
		app.m_iTrialEndsAt = LF_PoliceApplicationRules.TrialEndsAt(now, trialHours);
		Save();
		LF_Debug.Info("Police", string.Format("police application of %1 (%2) approved, added to the police roster, trial ends at %3", app.m_sUid, app.m_sName, app.m_iTrialEndsAt));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: rejects the open application of uid and starts the reapply cooldown.
	//! \return True when an open application was rejected.
	static bool Reject(string uid, bool approverIsAdmin, int now)
	{
		if (!LF_PoliceApplicationRules.CanApprove(approverIsAdmin))
		{
			LF_Debug.Warn("Police", string.Format("rejection of %1 refused: the sender is not an administrator", uid));
			return false;
		}

		LF_PoliceApplication app = FindOpen(uid);
		if (!app)
		{
			LF_Debug.Warn("Police", string.Format("rejection refused: %1 has no open application", uid));
			return false;
		}

		app.m_sStatus = STATUS_REJECTED;
		app.m_iRejectedAt = now;
		Save();
		LF_Debug.Info("Police", string.Format("police application of %1 (%2) rejected at %3", app.m_sUid, app.m_sName, now));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a promotion ends the trial for good.
	//! \return True when a running trial was ended.
	static bool EndTrial(string uid)
	{
		bool ended = false;
		foreach (LF_PoliceApplication app : All())
		{
			if (app.m_sStatus != STATUS_APPROVED || app.m_sUid != uid || app.m_iTrialEndsAt <= 0)
				continue;

			app.m_iTrialEndsAt = 0;
			ended = true;
		}

		if (ended)
		{
			Save();
			LF_Debug.Info("Police", string.Format("trial of %1 ended by promotion", uid));
		}
		return ended;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: removes from the police roster every officer whose trial ran out at now unpromoted.
	//! \return How many left the roster.
	static int ExpireTrials(int now)
	{
		int expired = 0;
		foreach (LF_PoliceApplication app : All())
		{
			if (app.m_sStatus != STATUS_APPROVED || !LF_PoliceApplicationRules.TrialExpired(app.m_iTrialEndsAt, now))
				continue;

			LF_WhitelistManager.RemoveFactionWhitelist(app.m_sUid, LF_EFactionType.POLICE);
			app.m_sStatus = STATUS_EXPIRED;
			app.m_iTrialEndsAt = 0;
			expired++;
			LF_Debug.Info("Police", string.Format("trial of %1 (%2) expired unpromoted, removed from the police roster", app.m_sUid, app.m_sName));
		}

		if (expired > 0)
			Save();
		return expired;
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
			int fields = parts.Count();
			if (fields != ROW_FIELDS_V1 && fields != ROW_FIELDS)
				continue;

			LF_PoliceApplication app = new LF_PoliceApplication();
			app.m_sStatus = parts[0];
			app.m_sUid = parts[1];
			app.m_sName = parts[2];
			app.m_sWhy = parts[3];
			app.m_sAvailability = parts[4];
			app.m_iTime = parts[5].ToInt();
			if (fields == ROW_FIELDS)
			{
				app.m_iRejectedAt = parts[6].ToInt();
				app.m_iTrialEndsAt = parts[7].ToInt();
			}
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
			handle.WriteLine(string.Format("%1|%2|%3|%4|%5|%6|%7|%8", app.m_sStatus, app.m_sUid, app.m_sName, app.m_sWhy, app.m_sAvailability, app.m_iTime, app.m_iRejectedAt, app.m_iTrialEndsAt));
		}
		handle.Close();
	}
}
