// red-proof: set m_bLF_DevToolsOff 0 in Missions/LF_PlaytestTown.conf and ran
// `tools\cli test --no-build --tier fast` (2026-11-08). Observed:
//   [ELTEST]   - playtest mission closes dev tools
//   [ELTEST]   - dev tools closed on the playtest mission despite the DebugWorld opt-in
//   [ELTEST]   - a non-admin is refused on the playtest mission
//   [ELTEST] SUMMARY tier=fast passed=91 failed=1 total=92
// Restored m_bLF_DevToolsOff 1 and the suite went green (92/92).

// tier: LOGIC
//! The hosted playtest mission reuses DebugWorld, whose game mode opts every player into dev
//! tools. The mission header must close them, or any joining player gets the debug menu
//! (free cash) and the agent-harness RPCs.
class EL_Test_PlaytestMission : EL_Test
{
	static const string PLAYTEST_MISSION = "{1054CC8646357708}Missions/LF_PlaytestTown.conf";

	override string GetName()
	{
		return "host/playtest-mission";
	}

	override void Run(EL_TestContext ctx)
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
		ctx.False(EL_DevGate.IsAllowed(devTools, false), "a non-admin is refused on the playtest mission");
		ctx.True(EL_DevGate.IsAllowed(devTools, true), "an admin still passes on the playtest mission");

		// The bare DebugWorld (Workbench play, tools\cli test) keeps its dev tools.
		ctx.True(LF_MissionHeader.ResolveDevToolsEnabled(true, null), "no mission header keeps the game mode opt-in");
	}
}
