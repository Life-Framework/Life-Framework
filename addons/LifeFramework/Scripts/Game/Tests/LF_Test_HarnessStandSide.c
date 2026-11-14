// red-proof: made StandDistance ignore the token's metres (`if (false && metres > 0)`) and ran
// `tools\cli test --no-build --only LF_Test_HarnessStandSide --remote lf-testbox` (2026-11-14). Observed:
//   [LFTEST]   - s9 stands 9 m out (expected 9, got 1.2)
//   [LFTEST] SUMMARY tier=only passed=0 failed=1 total=1
// Restored it.

// tier: LOGIC
//! The harness stand side token: a compass letter with an optional distance in whole metres, so a
//! proof can stand on a building's street side ("s9") instead of 1.2 m from its centre (inside).
class LF_Test_HarnessStandSide : LF_Test
{
	override string GetName()
	{
		return "harness/stand-side-distance";
	}

	override void Run(LF_TestContext ctx)
	{
		ctx.EqualFloat(1.2, LF_AgentHarness.StandDistance("e"), 0.001, "a bare letter stands 1.2 m out");
		ctx.EqualFloat(2, LF_AgentHarness.StandDistance("n2"), 0.001, "n2 still stands 2 m out (NPC talk proofs)");
		ctx.EqualFloat(9, LF_AgentHarness.StandDistance("s9"), 0.001, "s9 stands 9 m out");
		ctx.EqualFloat(30, LF_AgentHarness.StandDistance("w30"), 0.001, "w30 stands 30 m out");
		ctx.EqualFloat(-9, LF_AgentHarness.StandOffset("s9")[2], 0.001, "s9 is south: -z");

		ctx.EqualStr("s", LF_AgentHarness.SideCompass("s9"), "s9 is a side");
		ctx.EqualStr("", LF_AgentHarness.SideCompass("s31"), "past 30 m is refused");
		ctx.EqualStr("", LF_AgentHarness.SideCompass("s0"), "0 m is refused");
		ctx.EqualStr("", LF_AgentHarness.SideCompass("sx"), "a non-number distance is refused");
		ctx.EqualStr("", LF_AgentHarness.SideCompass("q"), "an unknown letter is refused");

		LF_AgentCommand cmd = new LF_AgentCommand();
		ctx.True(LF_AgentCommand.Parse("11 stand Annex_Supermarket s9", cmd), "stand with s9 parses");
		ctx.EqualStr("s9", cmd.Arg2, "the side keeps its distance");
		LF_AgentCommand bad = new LF_AgentCommand();
		ctx.True(!LF_AgentCommand.Parse("12 stand Annex_Supermarket s99", bad), "stand with s99 is refused");
	}
}
