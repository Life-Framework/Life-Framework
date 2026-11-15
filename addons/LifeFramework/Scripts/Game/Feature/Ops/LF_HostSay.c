//------------------------------------------------------------------------------------------------
//! Operator message to every screen (Ops, M4, #1453). `tools\cli host say "<text>"` writes
//! $profile:LifeFramework/host_say.txt; the server polls it every 10 seconds, toasts every player
//! once through the notifications feature, logs the text and deletes the file, so the operator can
//! speak to the town without joining. Server only. Same file-drop pattern as LF_HostStop.
class LF_HostSay
{
	static const string FEATURE = "Ops";
	static const string SAY_FILE = "$profile:LifeFramework/host_say.txt";
	static const int POLL_MS = 10000;
	static const int MAX_LENGTH = 200;
	static const float TOAST_SECONDS = 12.0;
	static const string TOAST_TITLE = "#LF-HostSay_Title";
	static const string TOAST_MESSAGE = "#LF-HostSay_Message";

	protected static bool s_bRunning;
	protected static int s_iSent;
	protected static string s_sLastLog;

	//------------------------------------------------------------------------------------------------
	//! Server: starts the 10 second poll for the say file. A file left from before this boot is
	//! dropped, so a stale message is not announced to whoever joins first.
	static void StartIfServer()
	{
		if (s_bRunning || !Replication.IsServer())
			return;

		if (FileIO.FileExists(SAY_FILE))
		{
			FileIO.DeleteFile(SAY_FILE);
			Log(false, "removed a stale say file from an earlier run");
		}

		s_bRunning = true;
		GetGame().GetCallqueue().CallLater(Poll, POLL_MS, true);
		Log(true, string.Format("watching %1 for tools\\cli host say", SAY_FILE));
	}

	//------------------------------------------------------------------------------------------------
	static void Poll()
	{
		ProcessFile(SAY_FILE);
	}

	//------------------------------------------------------------------------------------------------
	//! Reads one say file, toasts its first non-empty line to every player and deletes the file.
	//! An empty file sends nothing, warns and is deleted too, so it does not warn every poll.
	//! \return the text that was sent, or "" when nothing was
	static string ProcessFile(string path)
	{
		if (!FileIO.FileExists(path))
			return "";

		string text = ReadFirstLine(path);
		FileIO.DeleteFile(path);

		text = LF_HostSay.Clean(text);
		if (text.IsEmpty())
		{
			Log(false, "host say: the say file was empty, nothing sent");
			return "";
		}

		Send(text);
		Log(true, string.Format("host say: %1", text));
		return text;
	}

	//------------------------------------------------------------------------------------------------
	//! Trims the text and cuts it to MAX_LENGTH characters.
	static string Clean(string raw)
	{
		string text = raw.Trim();
		if (text.Length() > MAX_LENGTH)
			text = text.Substring(0, MAX_LENGTH);
		return text;
	}

	//------------------------------------------------------------------------------------------------
	//! One toast to every connected player; the text rides as the message argument.
	protected static void Send(string text)
	{
		s_iSent++;
		LF_NotificationManagerComponent.NotifyAll(TOAST_TITLE, TOAST_MESSAGE, TOAST_SECONDS, LF_ENotificationType.INFO, text);
	}

	//------------------------------------------------------------------------------------------------
	protected static string ReadFirstLine(string path)
	{
		FileHandle handle = FileIO.OpenFile(path, FileMode.READ);
		if (!handle)
		{
			Log(false, string.Format("host say: cannot open %1", path));
			return "";
		}

		string result = "";
		string line;
		while (handle.ReadLine(line) >= 0)
		{
			if (!line.Trim().IsEmpty())
			{
				result = line;
				break;
			}
		}

		handle.Close();
		return result;
	}

	//------------------------------------------------------------------------------------------------
	protected static void Log(bool info, string message)
	{
		s_sLastLog = message;
		if (info)
			LF_Debug.Info(FEATURE, message);
		else
			LF_Debug.Warn(FEATURE, message);
	}

	//------------------------------------------------------------------------------------------------
	//! How many say toasts this run has sent; tests read it.
	static int GetSentCount()
	{
		return s_iSent;
	}

	//------------------------------------------------------------------------------------------------
	//! The last line logged; tests read it.
	static string GetLastLog()
	{
		return s_sLastLog;
	}
}

//------------------------------------------------------------------------------------------------
//! Starts the say poll from the game mode without editing LF_GameModeRoleplay.c (M4 rule).
modded class LF_GameModeRoleplay
{
	protected override void OnGameStart()
	{
		super.OnGameStart();
		LF_HostSay.StartIfServer();
	}
}
