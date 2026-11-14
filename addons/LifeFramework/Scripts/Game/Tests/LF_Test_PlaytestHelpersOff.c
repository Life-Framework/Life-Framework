// red-proof: 2026-11-15, `tools/cli test --no-build --red LF_Test_PlaytestHelpersOff`: RED-OK host/playtest-helpers-off (fails with the hook on, 7 assertions); green without it.

// tier: LOGIC
//! The flagship runs the DebugWorld game mode prefab, which switches the playtest helpers on (test cars,
//! bank and cash top-up, jail-me board, spawn hub, test dispatch, near convoy drop). A mission header with
//! m_bLF_PlaytestHelpersOff closes them all; any other header, and the bare world, keep the game mode's flag.
class LF_Test_PlaytestHelpersOff : LF_Test
{
	override string GetName()
	{
		return "host/playtest-helpers-off";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_MissionHeader flagship = new LF_MissionHeader();
		flagship.m_bLF_PlaytestHelpersOff = true;
		LF_MissionHeader playtest = new LF_MissionHeader();

		bool onFlagship = true;
		if (!RedProof())
			onFlagship = LF_MissionHeader.ResolvePlaytestHelper(true, flagship);

		ctx.False(onFlagship, "a helper the game mode switches on is closed by a header with m_bLF_PlaytestHelpersOff");
		ctx.False(LF_MissionHeader.ResolvePlaytestHelper(false, flagship), "a helper the game mode leaves off stays off");
		ctx.True(LF_MissionHeader.ResolvePlaytestHelper(true, playtest), "the playtest header leaves the game mode's helpers on");
		ctx.False(LF_MissionHeader.ResolvePlaytestHelper(false, playtest), "the playtest header does not switch a helper on");
		ctx.True(LF_MissionHeader.ResolvePlaytestHelper(true, null), "no mission header (bare DebugWorld, tests) keeps the game mode's helpers");
		ctx.False(LF_MissionHeader.ResolvePlaytestHelper(false, null), "no mission header leaves an off helper off");
	}
}
