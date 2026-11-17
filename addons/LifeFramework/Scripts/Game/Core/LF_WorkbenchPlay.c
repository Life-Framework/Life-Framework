//------------------------------------------------------------------------------------------------
//! Workbench play only (docs/tooling/fast-play.md). Three things a World Editor Play needs that a
//! real launch never does, each a pure decision (LOGIC-tier tested, LF_Test_WorkbenchPlay) behind a
//! WORKBENCH build check, so a dedicated server or a published build never takes them:
//! - Fast play: the "LF Fast Play" plugin (Scripts/WorkbenchGame/LFFastPlay) writes REQUEST_FILE and
//!   starts play; the creation flow then skips the rules quiz and form R-89 and spawns a dev character
//!   with the starting bank and an ID card (LF_CharacterCreationManager.FastPlay).
//! - Pack overlays: Workbench play loads the world with no mission header, so no pack overlay (signs,
//!   dialogue, quiz, sound slots) would load. The fallback is the mission header whose World is the
//!   open world (LF_MissionConfigPackSource).
//! - Persistence: with no mission header the engine's default systems config has no
//!   SCR_PersistenceSystem, and the World Editor cannot name one; that is expected there, not an error.
class LF_WorkbenchPlay
{
	static const string FEATURE = "WorkbenchPlay";
	//! Written by the plugin before it starts play; the game marks it used on read.
	static const string REQUEST_FILE = "$profile:lf-fastplay/request.txt";
	static const string REQUEST_FAST = "fast";
	static const string REQUEST_USED = "used";
	// lf-fixed: the Workbench dev character's name and age, not a server choice
	static const string DEV_FIRST_NAME = "Dev";
	// lf-fixed: see above
	static const string DEV_LAST_NAME = "Tester";
	// lf-fixed: see above
	static const int DEV_AGE = 30;

	//! -1 not read yet, 0 normal play, 1 fast play. Read once per world (reset in LF_StaticReset).
	protected static int s_iFastPlay = -1;
	protected static bool s_bHeaderSearched;
	protected static ref LF_MissionHeader s_FallbackHeader;

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_iFastPlay = -1;
		s_bHeaderSearched = false;
		s_FallbackHeader = null;
	}

	//------------------------------------------------------------------------------------------------
	//! True only in a Workbench build.
	static bool IsWorkbench()
	{
#ifdef WORKBENCH
		return true;
#else
		return false;
#endif
	}

	//------------------------------------------------------------------------------------------------
	//! Fast play runs only inside Workbench and only for a fresh "fast" request from the plugin.
	static bool FastPlayDecision(bool workbench, string request)
	{
		return workbench && request.Trim() == REQUEST_FAST;
	}

	//------------------------------------------------------------------------------------------------
	//! A missing SCR_PersistenceSystem is expected only in Workbench play with no mission header.
	static bool PersistenceMissingExpected(bool workbench, bool hasMissionHeader)
	{
		return workbench && !hasMissionHeader;
	}

	//------------------------------------------------------------------------------------------------
	//! Which header stands in for the missing one: of the headers whose world is the open world, the
	//! one listing the most overlays (the full server header over a trimmed playtest one); the first
	//! on a tie. When no header for the open world lists any overlay (DebugWorld: the village is a
	//! SubScene of it, and only the pack's headers carry the overlays), the header with the most
	//! overlays of all: Workbench loads the pack beside the core, so its dialogue voices, signs,
	//! quiz and sounds belong to any world opened there. -1 when no header lists an overlay.
	//! \param worlds World path of each candidate header, in path order.
	//! \param overlayCounts How many config overlays each lists.
	//! \param openWorld The open world's path.
	static int PickFallbackHeader(notnull array<string> worlds, notnull array<int> overlayCounts, string openWorld)
	{
		string open = NormalizeWorld(openWorld);
		if (open.IsEmpty())
			return -1;

		int best = -1;
		int bestAny = -1;
		for (int i = 0; i < worlds.Count() && i < overlayCounts.Count(); i++)
		{
			if (overlayCounts[i] <= 0)
				continue;

			if (bestAny < 0 || overlayCounts[i] > overlayCounts[bestAny])
				bestAny = i;

			if (NormalizeWorld(worlds[i]) != open)
				continue;

			if (best < 0 || overlayCounts[i] > overlayCounts[best])
				best = i;
		}

		if (best >= 0)
			return best;

		return bestAny;
	}

	//------------------------------------------------------------------------------------------------
	//! "{GUID}Worlds/X/X.ent", "$addon:Worlds/X/X.ent" and "worlds/x/x.ent" compare equal.
	static string NormalizeWorld(string world)
	{
		string path = world;
		int close = path.IndexOf("}");
		if (path.StartsWith("{") && close > 0)
			path = path.Substring(close + 1, path.Length() - close - 1);

		int colon = path.IndexOf(":");
		if (colon >= 0)
			path = path.Substring(colon + 1, path.Length() - colon - 1);

		path.Replace("\\", "/");
		path.ToLower();
		return path.Trim();
	}

	//------------------------------------------------------------------------------------------------
	//! This play session was started with LF Fast Play. Reads the plugin's request once and marks it
	//! used, so the next plain Play is the full onboarding again.
	static bool IsFastPlay()
	{
		if (s_iFastPlay >= 0)
			return s_iFastPlay == 1;

		string request = ReadRequest();
		s_iFastPlay = 0;
		if (FastPlayDecision(IsWorkbench(), request))
		{
			s_iFastPlay = 1;
			MarkRequestUsed();
			LF_Debug.Info(FEATURE, "LF Fast Play: the dev character skips the rules quiz and form R-89");
		}

		return s_iFastPlay == 1;
	}

	//------------------------------------------------------------------------------------------------
	protected static string ReadRequest()
	{
		string line;
#ifdef WORKBENCH
		if (!FileIO.FileExists(REQUEST_FILE))
			return line;

		FileHandle file = FileIO.OpenFile(REQUEST_FILE, FileMode.READ);
		if (!file)
			return line;

		file.ReadLine(line);
		file.Close();
#endif
		return line;
	}

	//------------------------------------------------------------------------------------------------
	//! Overwritten rather than deleted: FileIO.DeleteFile has been seen to leave a Workbench file in
	//! place (tools/visual/wb/LF_VisualShotsPlugin.c), and a stale "fast" would make every Play fast.
	protected static void MarkRequestUsed()
	{
#ifdef WORKBENCH
		FileHandle file = FileIO.OpenFile(REQUEST_FILE, FileMode.WRITE);
		if (!file)
		{
			LF_Debug.Error(FEATURE, "could not mark the fast-play request used: " + REQUEST_FILE);
			return;
		}

		file.WriteLine(REQUEST_USED);
		file.Close();
#endif
	}

	//------------------------------------------------------------------------------------------------
	//! Workbench play with no mission header: the header of the open world, found once among the
	//! loaded addons' Missions/*.conf. Null outside Workbench, or when no header names this world.
	static LF_MissionHeader FallbackHeader()
	{
		if (!IsWorkbench() || s_bHeaderSearched)
			return s_FallbackHeader;

		// Asked before the world has loaded (no world file yet): try again on the next call.
		if (!GetGame() || GetGame().GetWorldFile().IsEmpty())
			return null;

		s_bHeaderSearched = true;

		LF_WorkbenchHeaderSearch search = new LF_WorkbenchHeaderSearch();
		search.Run();

		array<string> worlds = {};
		array<int> overlayCounts = {};
		array<ref LF_MissionHeader> headers = {};
		foreach (ResourceName path : search.m_aFound)
		{
			LF_MissionHeader header = LF_MissionHeader.Cast(MissionHeader.ReadMissionHeader(path));
			if (!header)
				continue;

			headers.Insert(header);
			worlds.Insert(header.GetWorldPath());
			overlayCounts.Insert(header.m_aLF_ConfigOverlays.Count());
		}

		int pick = PickFallbackHeader(worlds, overlayCounts, GetGame().GetWorldFile());
		if (pick < 0)
		{
			LF_Debug.Warn(FEATURE, string.Format("Workbench play with no mission header, and none of %1 mission header(s) (%2 LF) lists a pack overlay; no pack overlay loads, so the dialogue voices, signs and quiz are the core's (world %3)", search.m_aFound.Count(), headers.Count(), GetGame().GetWorldFile()));
			return null;
		}

		s_FallbackHeader = headers[pick];
		string forWorld = "for this world";
		if (NormalizeWorld(worlds[pick]) != NormalizeWorld(GetGame().GetWorldFile()))
			forWorld = "of the pack (no header for this world lists overlays: " + GetGame().GetWorldFile() + ")";

		LF_Debug.Info(FEATURE, string.Format("Workbench play with no mission header: pack overlays come from the header %1 (%2 overlays)", forWorld, overlayCounts[pick]));
		return s_FallbackHeader;
	}
}

//------------------------------------------------------------------------------------------------
//! Collects every mission header .conf in the loaded addons (sorted), for FallbackHeader.
class LF_WorkbenchHeaderSearch : Managed
{
	ref array<ResourceName> m_aFound = {};

	//------------------------------------------------------------------------------------------------
	void Run()
	{
		SearchResourcesFilter filter = new SearchResourcesFilter();
		filter.fileExtensions = {"conf"};
		ResourceDatabase.SearchResources(filter, OnFound);
		m_aFound.Sort();
	}

	//------------------------------------------------------------------------------------------------
	protected void OnFound(ResourceName resourceName, string exactPath = "")
	{
		string path = resourceName;
		path.ToLower();
		if (path.Contains("missions/"))
			m_aFound.Insert(resourceName);
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_WorkbenchPlay's statics (registered at script load; Core/LF_StaticReset.c).
class LF_WorkbenchPlay_StaticResetHook : LF_StaticResetHook
{
	protected static bool s_bRegistered = LF_StaticReset.Register(new LF_WorkbenchPlay_StaticResetHook());

	override string Owner()
	{
		return "LF_WorkbenchPlay";
	}

	override void Apply()
	{
		LF_WorkbenchPlay.ResetStatics();
	}
}
