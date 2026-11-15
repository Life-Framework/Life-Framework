//------------------------------------------------------------------------------------------------
//! Read access to the roster rows for the notice, from this file: LF_PoliceApplications keeps its
//! row list protected and its file format is not ours to change (S9 #1288 and S6 #1285 edit it).
modded class LF_PoliceApplications
{
	//------------------------------------------------------------------------------------------------
	//! The latest row of uid that put them on the roster: APPROVED (on it now) or EXPIRED (a trial
	//! that ran out unpromoted). Null when there is none. Open and rejected rows say nothing yet.
	static LF_PoliceApplication FindRosterRow(string uid)
	{
		LF_PoliceApplication found = null;
		foreach (LF_PoliceApplication app : All())
		{
			if (app.m_sUid != uid)
				continue;

			if (app.m_sStatus == STATUS_APPROVED || app.m_sStatus == STATUS_EXPIRED)
				found = app;
		}
		return found;
	}
}

//------------------------------------------------------------------------------------------------
//! Bus subscriber: a player spawned.
class LF_PoliceRosterNoticeSubscriber : LF_EventSubscriber
{
	override bool Handle(notnull LF_Event busEvent)
	{
		LF_OnPlayerSpawnedEvent spawned = LF_OnPlayerSpawnedEvent.Cast(busEvent);
		if (!spawned)
			return false;

		LF_PoliceRosterNotice.OnSpawned(spawned.m_iPlayerId, spawned.m_sAccountUid, spawned.m_bFirstThisSession);
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Police roster notice (#1447, chapter #1006 beat 4). A player whose application was approved while
//! they were away is told at their first spawn of the session that they are on the roster; an officer
//! whose trial ran out unpromoted is told they are off it. Nobody else sees anything. Each row is
//! told once: the told rows are kept in a text file beside the applications
//! ($profile:LifeFramework/police_roster_told.txt, `STATUS|uid|applicationTime` per line), so a
//! promoted officer is not greeted every session and a restart does not repeat a notice.
//! Server only; nothing replicates. The toast is LF_NotificationManagerComponent.NotifyPlayer.
//------------------------------------------------------------------------------------------------
class LF_PoliceRosterNotice
{
	static const string FEATURE = "Police";
	static const string KEY_APPROVED = "#LF-PoliceRoster_Approved";
	static const string KEY_EXPIRED = "#LF-PoliceRoster_Expired";

	protected static ref LF_PoliceRosterNoticeSubscriber s_Subscriber;
	protected static ref set<string> s_mTold;
	protected static ref array<string> s_aSent;
	protected static string s_sPath = "";
	protected static bool s_bQuiet;

	//------------------------------------------------------------------------------------------------
	//! Tests point this at a private file. An empty path uses the profile file.
	static void SetToldPath(string path)
	{
		s_sPath = path;
		s_mTold = null;
	}

	//------------------------------------------------------------------------------------------------
	//! Tests turn the toast off (no notification manager in a logic test); the notice is still recorded.
	static void SetQuiet(bool quiet)
	{
		s_bQuiet = quiet;
	}

	//------------------------------------------------------------------------------------------------
	//! Notices given since the last reset, each `playerId|key`.
	static int SentCount()
	{
		if (!s_aSent)
			return 0;

		return s_aSent.Count();
	}

	//------------------------------------------------------------------------------------------------
	static string SentAt(int index)
	{
		if (!s_aSent || index < 0 || index >= s_aSent.Count())
			return "";

		return s_aSent[index];
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		Uninstall();
		s_mTold = null;
		s_aSent = null;
		s_sPath = "";
		s_bQuiet = false;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: subscribes to the spawn event once. Safe to call again. \return true when subscribed.
	static bool EnsureSubscribed()
	{
		if (!Replication.IsServer())
			return false;

		if (s_Subscriber && LF_EventBus.SubscriberCount(LF_OnPlayerSpawnedEvent) > 0)
			return true;

		s_Subscriber = new LF_PoliceRosterNoticeSubscriber();
		return LF_EventBus.Subscribe(LF_OnPlayerSpawnedEvent, s_Subscriber);
	}

	//------------------------------------------------------------------------------------------------
	static void Uninstall()
	{
		if (s_Subscriber)
			LF_EventBus.Unsubscribe(LF_OnPlayerSpawnedEvent, s_Subscriber);

		s_Subscriber = null;
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: the notice key for a roster row status, "" for any other.
	static string KeyForStatus(string status)
	{
		if (status == LF_PoliceApplications.STATUS_APPROVED)
			return KEY_APPROVED;

		if (status == LF_PoliceApplications.STATUS_EXPIRED)
			return KEY_EXPIRED;

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a player spawned. On the first spawn of the session, tells an approved recruit or an
	//! expired trial once. \return The key told, or "" when nothing was.
	static string OnSpawned(int playerId, string uid, bool firstThisSession)
	{
		if (!firstThisSession || uid.IsEmpty() || playerId <= 0)
			return "";

		LF_PoliceApplication row = LF_PoliceApplications.FindRosterRow(uid);
		if (!row)
			return "";

		string key = KeyForStatus(row.m_sStatus);
		string told = string.Format("%1|%2|%3", row.m_sStatus, row.m_sUid, row.m_iTime);
		if (key.IsEmpty() || Told().Find(told) >= 0)
			return "";

		Told().Insert(told);
		SaveTold();
		if (!s_aSent)
			s_aSent = new array<string>();

		s_aSent.Insert(string.Format("%1|%2", playerId, key));
		LF_Debug.Info(FEATURE, string.Format("roster notice %1 for %2 (player %3)", key, uid, playerId));
		if (!s_bQuiet)
			LF_NotificationManagerComponent.NotifyPlayer(playerId, "#LF-PoliceApp_Title", key, 8.0);

		return key;
	}

	//------------------------------------------------------------------------------------------------
	protected static string GetPath()
	{
		if (!s_sPath.IsEmpty())
			return s_sPath;

		return "$profile:LifeFramework/police_roster_told.txt";
	}

	//------------------------------------------------------------------------------------------------
	protected static set<string> Told()
	{
		if (!s_mTold)
			LoadTold();

		return s_mTold;
	}

	//------------------------------------------------------------------------------------------------
	protected static void LoadTold()
	{
		s_mTold = new set<string>();
		string path = GetPath();
		if (!FileIO.FileExists(path))
			return;

		FileHandle handle = FileIO.OpenFile(path, FileMode.READ);
		if (!handle)
		{
			LF_Debug.Error(FEATURE, string.Format("cannot open the police roster notice file: %1", path));
			return;
		}

		string line;
		while (handle.ReadLine(line) >= 0)
		{
			if (!line.IsEmpty())
				s_mTold.Insert(line);
		}
		handle.Close();
	}

	//------------------------------------------------------------------------------------------------
	protected static void SaveTold()
	{
		string path = GetPath();
		string parent = FilePath.StripFileName(path);
		if (!parent.IsEmpty())
			FileIO.MakeDirectory(parent);

		FileHandle handle = FileIO.OpenFile(path, FileMode.WRITE);
		if (!handle)
		{
			LF_Debug.Error(FEATURE, string.Format("cannot write the police roster notice file: %1", path));
			return;
		}

		foreach (string line : Told())
		{
			handle.WriteLine(line);
		}
		handle.Close();
	}
}

//------------------------------------------------------------------------------------------------
//! Self-start: the notice listens from the world's start, on the server only.
modded class LF_GameModeRoleplay
{
	//------------------------------------------------------------------------------------------------
	protected override void OnGameStart()
	{
		super.OnGameStart();
		LF_PoliceRosterNotice.EnsureSubscribed();
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of this file's statics (Core/LF_StaticReset.c runs every feature's hook).
modded class LF_StaticResetHooks
{
	override void Run(notnull array<string> done, bool apply)
	{
		super.Run(done, apply);
		done.Insert("LF_PoliceRosterNotice");
		if (apply)
			LF_PoliceRosterNotice.ResetStatics();
	}
}
