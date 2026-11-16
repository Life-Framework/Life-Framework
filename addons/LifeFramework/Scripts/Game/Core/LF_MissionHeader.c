//------------------------------------------------------------------------------------------------
//! LF_MissionHeader - a mission header that can close dev tools and listen for a host stop request.
//! Used by the pack's playtest headers and the core fixture Missions/LF_HostedTest.conf. They reuse DebugWorld, whose game mode prefab
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

	[Attribute("0", desc: "Testing bypass: every whitelist-gated job (police roster, playtime, application) opens to every player. Only the playtest mission sets it; the flagship header never does (validate-flagship-job-gates). The owner config jobs.testUnlockAll asks for the same switch.")]
	bool m_bLF_TestUnlockAllJobs;

	[Attribute("0", desc: "Lets the job-gate bypass (m_bLF_TestUnlockAllJobs or jobs.testUnlockAll) run on a dedicated server. Without it a dedicated server refuses the bypass and logs an error.")]
	bool m_bLF_TestUnlockOnDedicated;

	[Attribute("0", desc: "Close the playtest helpers the world's game mode switches on (test cars, bank and cash top-up, the jail-me board, the spawn hub travel globe, test dispatch calls, the near convoy drop), even when the game mode opts in. On for a mission a real server runs; off for the playtest mission and DebugWorld.")]
	bool m_bLF_PlaytestHelpersOff;

	[Attribute("", UIWidgets.ResourceNamePicker, desc: "Config overlays (.conf) this mission layers over the shipped defaults, lowest precedence first: a later file wins over an earlier one, the server owner's config.json wins over all. Each overlay is a .conf of the same class as the core file it changes and sets only the fields it names. Read by LF_MissionConfigPackSource (D4 option b, #278).", params: "conf")]
	ref array<ResourceName> m_aLF_ConfigOverlays;

	[Attribute("", UIWidgets.ResourceNamePicker, desc: "The pack's manifest (.conf of class LF_PackManifest): its id and the core versions it supports. When the running core is outside that range the core logs one error and loads none of this mission's config overlays. Empty: no check (#963).", params: "conf")]
	ResourceName m_LF_PackManifest;

	[Attribute("", UIWidgets.Auto, desc: "Map states that are live (D14, #2208). An LF_PlaceComponent with a map state outside this list stays dark: it does not register in LF_Places, so its systems have no place there. Empty: every state is live (the playtest town and the test worlds). The pack says what each number means.")]
	ref array<int> m_aLF_LiveStates;

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

	//------------------------------------------------------------------------------------------------
	//! Pure: whether a playtest helper (LF_SpawnLogic test cars, money top-up, jail-me board, spawn hub,
	//! test dispatch; LF_GameModeRoleplay near convoy drop) is on, given the game mode's flag and the
	//! mission header. A header with m_bLF_PlaytestHelpersOff wins: the flagship runs the DebugWorld
	//! game mode prefab, which has them all on.
	//! \param gameModeFlag The helper's own flag on the game mode prefab.
	//! \param header The mission header; null when the server was started on a bare world.
	static bool ResolvePlaytestHelper(bool gameModeFlag, MissionHeader header)
	{
		LF_MissionHeader lfHeader = LF_MissionHeader.Cast(header);
		if (lfHeader && lfHeader.m_bLF_PlaytestHelpersOff)
			return false;

		return gameModeFlag;
	}

	//------------------------------------------------------------------------------------------------
	//! \return the playtest-helper flag resolved against the running mission's header.
	static bool PlaytestHelper(bool gameModeFlag)
	{
		if (!gameModeFlag || !GetGame())
			return gameModeFlag;

		return ResolvePlaytestHelper(gameModeFlag, GetGame().GetMissionHeader());
	}
}

//------------------------------------------------------------------------------------------------
//! Server-side watcher for tools\cli host stop. Started by the game mode when the mission header
//! asks for it. The CLI writes the stop file into the server profile; the watcher deletes it and
//! requests a normal close, so the engine runs the SHUTDOWN save before the process exits.
class LF_HostStop
{
	static const string STOP_FILE = "$profile:lf-host-stop";
	//! agent-box/session.mjs roster writes the police roster, then this file: the server reloads it.
	static const string RELOAD_ROSTER_FILE = "$profile:lf-reload-roster";
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
		if (FileIO.FileExists(RELOAD_ROSTER_FILE))
		{
			FileIO.DeleteFile(RELOAD_ROSTER_FILE);
			LF_WhitelistManager.Reset();
			LF_WhitelistManager.Load();
			LF_Debug.Info("Host", "roster reloaded from the profile (session roster verb)");
		}

		if (!FileIO.FileExists(STOP_FILE))
			return;

		FileIO.DeleteFile(STOP_FILE);
		GetGame().GetCallqueue().Remove(Poll);
		s_bRunning = false;
		LF_Debug.Info("Host", "stop requested, closing the game (engine shutdown save follows)");
		GetGame().RequestClose();
	}
}
