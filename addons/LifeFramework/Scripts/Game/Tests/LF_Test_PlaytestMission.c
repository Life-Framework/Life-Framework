// red-proof: set m_bLF_DevToolsOff 0 in Missions/LF_PlaytestTown.conf and ran
// `tools\cli test --no-build --tier fast` (2026-11-08). Observed:
//   [LFTEST]   - playtest mission closes dev tools
//   [LFTEST]   - dev tools closed on the playtest mission despite the DebugWorld opt-in
//   [LFTEST]   - a non-admin is refused on the playtest mission
//   [LFTEST] SUMMARY tier=fast passed=91 failed=1 total=92
// Restored m_bLF_DevToolsOff 1 and the suite went green (92/92).
// red-proof (Workshop header, 2026-11-10): set m_sName to a string-table key and ran
// `tools\cli test --no-build --only LF_Test_PlaytestMission`. Observed:
//   [LFTEST]   - playtest mission name is plain text: (the key)
//   [LFTEST] SUMMARY tier=only passed=0 failed=1 total=1
// Restored the plain name and it passed (1/1).

// tier: LOGIC
//! The hosted playtest mission reuses DebugWorld, whose game mode opts every player into dev
//! tools. The mission header must close them, or any joining player gets the debug menu
//! (free cash) and the agent-harness RPCs.
class LF_Test_PlaytestMission : LF_Test
{
	static const string PLAYTEST_MISSION = "{1054CC8646357708}Missions/LF_PlaytestTown.conf";
	static const string PREVIEW_IMAGE = "{BA0EB87AB780FD56}Missions/LF_PlaytestTown_Preview.edds";

	override string GetName()
	{
		return "host/playtest-mission";
	}

	override void Run(LF_TestContext ctx)
	{
		MissionHeader header = MissionHeader.ReadMissionHeader(PLAYTEST_MISSION);
		ctx.True(header != null, "playtest mission header resolves");
		if (!header)
			return;

		LF_MissionHeader lfHeader = LF_MissionHeader.Cast(header);
		ctx.True(lfHeader != null, "playtest mission is an LF_MissionHeader");
		if (!lfHeader)
			return;

		ctx.True(lfHeader.m_bLF_DevToolsOff, "playtest mission closes dev tools");
		ctx.True(lfHeader.m_bLF_HostStopFile, "playtest mission watches the host stop file");
		string world = header.GetWorldResourceName();
		ctx.True(world.Contains("DebugWorld"), "playtest mission loads DebugWorld");

		// DebugWorld's game mode opts in; the playtest header must win for non-admins.
		bool devTools = LF_MissionHeader.ResolveDevToolsEnabled(true, header);
		ctx.False(devTools, "dev tools closed on the playtest mission despite the DebugWorld opt-in");
		ctx.False(LF_DevGate.IsAllowed(devTools, false), "a non-admin is refused on the playtest mission");
		ctx.True(LF_DevGate.IsAllowed(devTools, true), "an admin still passes on the playtest mission");

		// The Workshop backend copies the name and description as text and lists the scenario
		// only when the header says so; the scenario image must be the town picture.
		SCR_MissionHeader scr = SCR_MissionHeader.Cast(header);
		ctx.True(scr.m_bShowInScenarioMenu, "playtest mission is listed in Scenarios");
		ctx.True(scr.m_sName == "Life Framework: Playtest Town", "playtest mission name is plain text: " + scr.m_sName);
		ctx.False(scr.m_sDescription.IsEmpty() || scr.m_sDescription.StartsWith("#"), "playtest mission description is plain text");
		ctx.True(scr.m_sPreviewImage == PREVIEW_IMAGE && scr.m_sLoadingScreen == PREVIEW_IMAGE && scr.m_sIcon == PREVIEW_IMAGE, "playtest mission uses the town picture");
		ctx.True(scr.m_iPlayerCount > 1, "playtest mission is multiplayer");

		// The developer missions stay out of a player's Scenarios list.
		array<string> devMissions = {"{5A1E5A1E5A1E0002}Missions/EL_DebugTest.conf", "{FA38849EE56C68EB}Missions/EveronLifeGameMode.conf"};
		foreach (string hidden : devMissions)
		{
			SCR_MissionHeader dev = SCR_MissionHeader.Cast(MissionHeader.ReadMissionHeader(hidden));
			ctx.True(dev && !dev.m_bShowInScenarioMenu, "hidden from Scenarios: " + hidden);
		}

		// The bare DebugWorld (Workbench play, tools\cli test) keeps its dev tools.
		ctx.True(LF_MissionHeader.ResolveDevToolsEnabled(true, null), "no mission header keeps the game mode opt-in");
	}
}
