// red-proof: 2026-11-18 tools/cli test --no-build --red LF_Test_FishingFloat RED-OK (RedProof() skips the Show call, so no float is out and the dip, the rest height and the hide assertions fail).
//! fishing/float: the owner's float (#2995). It is a local entity at the water: `Show` puts it out at the
//! cast point, `Dip` starts the bobbing and a bob step pulls it under its resting height, `Hide` takes it away
//! and stops the bobbing. A dip with no float out does nothing. The dip's steps go under, nearly up, under,
//! up. The server never sees the float: this test touches no session state.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_FishingFloat : LF_Test
{
	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "fishing/float";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_FishingFloat.Hide();

		// The dip pattern: pulled under, nearly up, under again, up; then it repeats.
		ctx.True(LF_FishingFloat.DepthAt(0) > LF_FishingFloat.DepthAt(1), "the first step is deeper than the second");
		ctx.True(LF_FishingFloat.DepthAt(2) > LF_FishingFloat.DepthAt(1), "it goes under again after nearly surfacing");
		ctx.True(LF_FishingFloat.DepthAt(3) == 0, "the fourth step is back at rest");
		ctx.True(LF_FishingFloat.DepthAt(0) == LF_FishingFloat.DepthAt(4), "the pattern repeats");

		// No float out: a dip does nothing, a hide does nothing.
		LF_FishingFloat.Dip();
		ctx.False(LF_FishingFloat.IsDipping(), "a dip with no float out does not start");
		LF_FishingFloat.Hide();

		vector at = "1000 0 1000";
		if (!RedProof())
			LF_FishingFloat.Show(at);

		ctx.True(LF_FishingFloat.IsOut(), "a shown float is out");
		ctx.False(LF_FishingFloat.IsDipping(), "a new float rests");

		LF_FishingFloat.Dip();
		ctx.True(LF_FishingFloat.IsDipping(), "the bite starts the dip");

		LF_FishingFloat.Hide();
		ctx.False(LF_FishingFloat.IsOut(), "a hidden float is gone");
		ctx.False(LF_FishingFloat.IsDipping(), "hiding stops the bobbing");
	}
}
