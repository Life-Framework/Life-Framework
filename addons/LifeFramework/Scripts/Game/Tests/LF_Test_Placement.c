// red-proof: observed 2026-11-14 with `tools\cli test --no-build --only LF_Test_Placement`: Judge checking the blocker before the step failed `too steep wins over a blocker (expected 2, got 3)`; restored, green. Observed 2026-11-14 for the #1026 review: ProposalProblem without its horizontal check failed `a forward straight up is refused`; restored, green.
// tier: LOGIC
//! The placement foundation's verdict (#927, #928): a probe maps to valid or to one invalid reason,
//! in a fixed order (no ground, then too steep, then blocked), and every invalid verdict carries a
//! reason for the log and the refusal while a valid one carries none. The confirm (#1026) takes a
//! proposal within reach of where the player stands now, measures its heading against straight
//! across a road, and refuses one that is not finite, not in the world, or has no horizontal forward.
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

		// The confirm's reach (#1026): within the limit of where the player stands now, not of a re-plan.
		vector ahead[4];
		LF_PlacementGeometry.Transform("0 0 1.75", "0 0 3.75", ahead);
		vector sideways[4];
		LF_PlacementGeometry.Transform("3 0 1.75", "3 0 3.75", sideways);
		vector far[4];
		LF_PlacementGeometry.Transform("0 0 4", "0 0 6", far);
		vector high[4];
		LF_PlacementGeometry.Transform("0 3 1.75", "0 3 3.75", high);
		string offBy;
		ctx.True(LF_PlacementGeometry.WithinReach(vector.Zero, ahead, 4.25, 2, offBy), "the spot 2.75 m ahead is within reach");
		ctx.True(LF_PlacementGeometry.WithinReach(vector.Zero, sideways, 4.25, 2, offBy), "the same spot after the player turned or stepped 3 m sideways is within reach");
		ctx.False(LF_PlacementGeometry.WithinReach(vector.Zero, far, 4.25, 2, offBy), "5 m away is not");
		ctx.True(offBy.IndexOf("from the player") != -1, "the reach reason says how far: " + offBy);
		ctx.False(LF_PlacementGeometry.WithinReach(vector.Zero, high, 4.25, 2, offBy), "3 m above the player's feet is not");

		// Across a road running along x: either way across is 0 degrees, along it is 90.
		ctx.InRange(LF_PlacementGeometry.AcrossAngle("0 0 1", "1 0 0"), 0, 0.1, "straight across the road");
		ctx.InRange(LF_PlacementGeometry.AcrossAngle("0 0 -1", "-1 0 0"), 0, 0.1, "across the other way");
		ctx.InRange(LF_PlacementGeometry.AcrossAngle("0.5 0 0.866", "1 0 0"), 29.5, 30.5, "30 degrees off across");
		ctx.InRange(LF_PlacementGeometry.AcrossAngle("1 0 0", "1 0 0"), 89.5, 90.5, "along the road");

		// A proposal off the wire: a usable one, then garbage the server must refuse.
		ctx.EqualStr("", LF_PlacementGeometry.ProposalProblem(ahead), "a proposal from a ghost is usable");
		vector wild[4];
		LF_PlacementGeometry.Transform("0 0 1000000000", "0 0 1000000001", wild);
		ctx.True(LF_PlacementGeometry.ProposalProblem(wild).IndexOf("out of the world") != -1, "an origin a million km out is refused");
		vector zero[4];
		Copy(ahead, zero);
		zero[2] = vector.Zero;
		ctx.True(LF_PlacementGeometry.ProposalProblem(zero).IndexOf("not a unit vector") != -1, "a zero forward is refused");
		vector stretched[4];
		Copy(ahead, stretched);
		stretched[2] = "0 0 5";
		ctx.True(LF_PlacementGeometry.ProposalProblem(stretched).IndexOf("not a unit vector") != -1, "a forward 5 long is refused");
		vector up[4];
		Copy(ahead, up);
		up[2] = "0 1 0";
		ctx.True(LF_PlacementGeometry.ProposalProblem(up).IndexOf("not horizontal") != -1, "a forward straight up is refused");
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

	//------------------------------------------------------------------------------------------------
	protected void Copy(vector from[4], out vector to[4])
	{
		for (int i = 0; i < 4; i++)
			to[i] = from[i];
	}
}
