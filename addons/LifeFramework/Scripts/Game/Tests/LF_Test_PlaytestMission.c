// red-proof: set m_bLF_DevToolsOff 0 in the playtest header (then in the core) and ran
// `tools\cli test --no-build --tier fast` (2026-11-08). Observed:
//   [LFTEST]   - playtest mission closes dev tools
//   [LFTEST]   - dev tools closed on the playtest mission despite the DebugWorld opt-in
//   [LFTEST]   - a non-admin is refused on the playtest mission
//   [LFTEST] SUMMARY tier=fast passed=91 failed=1 total=92
// Restored m_bLF_DevToolsOff 1 and the suite went green (92/92).
// The playtest headers moved to the pack in #323 batch 1; the Workshop text checks (plain name,
// description, town picture, listed in Scenarios) moved with them to
// tools/validation/validate-pack-missions.mjs. This test now reads the core fixture
// Missions/LF_HostedTest.conf, which carries the same two switches on DebugWorld.

// tier: LOGIC
//! A hosted mission reuses DebugWorld, whose game mode opts every player into dev tools. A
//! header with m_bLF_DevToolsOff must close them, or any joining player gets the debug menu
//! (free cash) and the agent-harness RPCs.
class LF_Test_PlaytestMission : LF_Test
{
	static const string HOSTED_MISSION = "{43902474F5601DAA}Missions/LF_HostedTest.conf";

	override string GetName()
	{
		return "host/playtest-mission";
	}

	override void Run(LF_TestContext ctx)
	{
		MissionHeader header = MissionHeader.ReadMissionHeader(HOSTED_MISSION);
		ctx.True(header != null, "hosted mission header resolves");
		if (!header)
			return;

		LF_MissionHeader lfHeader = LF_MissionHeader.Cast(header);
		ctx.True(lfHeader != null, "hosted mission is an LF_MissionHeader");
		if (!lfHeader)
			return;

		ctx.True(lfHeader.m_bLF_DevToolsOff, "hosted mission closes dev tools");
		ctx.True(lfHeader.m_bLF_HostStopFile, "hosted mission watches the host stop file");
		string world = header.GetWorldResourceName();
		ctx.True(world.Contains("DebugWorld"), "hosted mission loads DebugWorld");

		// DebugWorld's game mode opts in; the header must win for non-admins.
		bool devTools = LF_MissionHeader.ResolveDevToolsEnabled(true, header);
		ctx.False(devTools, "dev tools closed on the hosted mission despite the DebugWorld opt-in");
		ctx.False(LF_DevGate.IsAllowed(devTools, false), "a non-admin is refused on the hosted mission");
		ctx.True(LF_DevGate.IsAllowed(devTools, true), "an admin still passes on the hosted mission");

		// The core's missions stay out of a player's Scenarios list.
		array<string> devMissions = {"{5A1E5A1E5A1E0002}Missions/EL_DebugTest.conf", HOSTED_MISSION};
		foreach (string hidden : devMissions)
		{
			SCR_MissionHeader dev = SCR_MissionHeader.Cast(MissionHeader.ReadMissionHeader(hidden));
			ctx.True(dev && !dev.m_bShowInScenarioMenu, "hidden from Scenarios: " + hidden);
		}

		// The bare DebugWorld (Workbench play, tools\cli test) keeps its dev tools.
		ctx.True(LF_MissionHeader.ResolveDevToolsEnabled(true, null), "no mission header keeps the game mode opt-in");
	}
}
