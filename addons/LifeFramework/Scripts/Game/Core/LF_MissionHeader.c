//------------------------------------------------------------------------------------------------
//! LF_MissionHeader - a mission header that can close dev tools and listen for a host stop request.
//! Used by Missions/LF_PlaytestTown.conf. That mission reuses DebugWorld, whose game mode prefab
//! opts every player into dev tools, so the header overrides that. tools\cli host stop asks for a
//! graceful close through a stop file. The engine writes its SHUTDOWN save point on that close.
class LF_MissionHeader : SCR_MissionHeader
{
	[Attribute("0", desc: "Close dev tools (debug menu, agent harness) for non-administrators, even when the world's game mode opts in.")]
	bool m_bLF_DevToolsOff;

	[Attribute("0", desc: "Server: close gracefully when $profile:lf-host-stop appears (tools\\cli host stop).")]
	bool m_bLF_HostStopFile;

	[Attribute("0", desc: "The first player to join while the police roster is empty is added to it (the host on a playtest). Off for real servers.")]
	bool m_bLF_FirstPlayerPolice;

	[Attribute("0", desc: "A police application is approved the moment it is filed, so a solo tester can try the form (the desk). Off for real servers: an administrator approves.")]
	bool m_bLF_InstantPoliceApproval;

	[Attribute("", UIWidgets.ResourceNamePicker, desc: "Config overlays (.conf) this mission layers over the shipped defaults, lowest precedence first: a later file wins over an earlier one, the server owner's config.json wins over all. Each overlay is a .conf of the same class as the core file it changes and sets only the fields it names. Read by LF_MissionConfigPackSource (D4 option b, #278).", params: "conf")]
	ref array<ResourceName> m_aLF_ConfigOverlays;

	//------------------------------------------------------------------------------------------------
	//! \return the running mission's header when it is an LF_MissionHeader, else null.
	static LF_MissionHeader GetCurrent()
	{
		if (!GetGame())
			return null;

		return LF_MissionHeader.Cast(GetGame().GetMissionHeader());
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: whether dev tools are open, given the game mode's opt-in and the mission header.
	//! \param gameModeOptIn The world's game mode opts every player in (LF_GameModeRoleplay).
	//! \param header The mission header; null when the server was started on a bare world.
	static bool ResolveDevToolsEnabled(bool gameModeOptIn, MissionHeader header)
	{
		LF_MissionHeader lfHeader = LF_MissionHeader.Cast(header);
		if (lfHeader && lfHeader.m_bLF_DevToolsOff)
			return false;

		return gameModeOptIn;
	}
}

//------------------------------------------------------------------------------------------------
//! Server-side watcher for tools\cli host stop. Started by the game mode when the mission header
//! asks for it. The CLI writes the stop file into the server profile; the watcher deletes it and
//! requests a normal close, so the engine runs the SHUTDOWN save before the process exits.
class LF_HostStop
{
	static const string STOP_FILE = "$profile:lf-host-stop";
	static const int POLL_MS = 2000;

	protected static bool s_bRunning;

	//------------------------------------------------------------------------------------------------
	static void StartIfRequested()
	{
		if (s_bRunning || !Replication.IsServer())
			return;

		LF_MissionHeader header = LF_MissionHeader.GetCurrent();
		if (!header || !header.m_bLF_HostStopFile)
			return;

		// A stop file left from a crashed run would close this boot at once.
		if (FileIO.FileExists(STOP_FILE))
		{
			FileIO.DeleteFile(STOP_FILE);
			LF_Debug.Warn("Host", "removed a stale stop file from an earlier run");
		}

		s_bRunning = true;
		GetGame().GetCallqueue().CallLater(Poll, POLL_MS, true);
		LF_Debug.Info("Host", string.Format("watching %1 for tools\\cli host stop", STOP_FILE));
	}

	//------------------------------------------------------------------------------------------------
	static void Poll()
	{
		if (!FileIO.FileExists(STOP_FILE))
			return;

		FileIO.DeleteFile(STOP_FILE);
		GetGame().GetCallqueue().Remove(Poll);
		s_bRunning = false;
		LF_Debug.Info("Host", "stop requested, closing the game (engine shutdown save follows)");
		GetGame().RequestClose();
	}
}
