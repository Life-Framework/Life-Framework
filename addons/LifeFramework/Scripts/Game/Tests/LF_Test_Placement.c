// red-proof: observed 2026-11-14 with `tools\cli test --no-build --only LF_Test_Placement`: Judge checking the blocker before the step failed `too steep wins over a blocker (expected 2, got 3)`; restored, green.
// tier: LOGIC
//! The placement foundation's verdict (#927, #928): a probe maps to valid or to one invalid reason,
//! in a fixed order (no ground, then too steep, then blocked), and every invalid verdict carries a
//! reason for the log and the refusal while a valid one carries none.
class LF_Test_Placement : LF_Test
{
	override string GetName()
	{
		return "placement/validity";
	}

	override void Run(LF_TestContext ctx)
	{
		float maxStep = 0.25;

		ctx.Equal(LF_EPlacementVerdict.NO_GROUND, LF_PlacementRules.Judge(null, maxStep), "no probe is no ground");

		LF_PlacementProbe flat = Probe(true, 10.0, 10.1, "");
		ctx.Equal(LF_EPlacementVerdict.VALID, LF_PlacementRules.Judge(flat, maxStep), "flat clear ground is valid");
		ctx.EqualStr("", LF_PlacementRules.Reason(LF_EPlacementVerdict.VALID, flat, maxStep), "a valid spot has no reason");

		LF_PlacementProbe edge = Probe(true, 10.0, 10.25, "");
		ctx.Equal(LF_EPlacementVerdict.VALID, LF_PlacementRules.Judge(edge, maxStep), "a rise of exactly the step is still valid");

		LF_PlacementProbe kerb = Probe(true, 10.0, 10.3, "");
		ctx.Equal(LF_EPlacementVerdict.TOO_STEEP, LF_PlacementRules.Judge(kerb, maxStep), "a rise past the step (a kerb) is too steep");
		ctx.True(LF_PlacementRules.Reason(LF_EPlacementVerdict.TOO_STEEP, kerb, maxStep).IndexOf("rises") != -1, "the steep reason says the ground rises");

		LF_PlacementProbe car = Probe(true, 10.0, 10.1, "S105_beige");
		ctx.Equal(LF_EPlacementVerdict.BLOCKED, LF_PlacementRules.Judge(car, maxStep), "something in the footprint blocks it");
		ctx.EqualStr("S105_beige is in the way", LF_PlacementRules.Reason(LF_EPlacementVerdict.BLOCKED, car, maxStep), "the blocked reason names what is in the way");

		LF_PlacementProbe air = Probe(false, 0, 0, "S105_beige");
		ctx.Equal(LF_EPlacementVerdict.NO_GROUND, LF_PlacementRules.Judge(air, maxStep), "no ground wins over a blocker");

		LF_PlacementProbe steepAndBlocked = Probe(true, 10.0, 11.0, "S105_beige");
		ctx.Equal(LF_EPlacementVerdict.TOO_STEEP, LF_PlacementRules.Judge(steepAndBlocked, maxStep), "too steep wins over a blocker");

		ctx.EqualStr("blocked", LF_PlacementRules.Name(LF_EPlacementVerdict.BLOCKED), "verdict names are lower case for logs and the harness");
		ctx.EqualStr("valid", LF_PlacementRules.Name(LF_EPlacementVerdict.VALID), "the valid name");
	}

	//------------------------------------------------------------------------------------------------
	protected LF_PlacementProbe Probe(bool ground, float lowest, float highest, string blocker)
	{
		LF_PlacementProbe probe = new LF_PlacementProbe();
		probe.m_bGround = ground;
		probe.m_fLowest = lowest;
		probe.m_fHighest = highest;
		probe.m_sBlocker = blocker;
		return probe;
	}
}
