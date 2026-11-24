// red-proof: made StandSpotOnFloor use the prop rule for a standing target (`if (false && targetStands)`) and ran
// `tools\cli test --no-build --only LF_Test_HarnessStandRaisedFloor`. The raised-floor line fails: a teller 1.1 m above the pavement
// lost every inside spot again.
// The front-side line fails under `--red` (inverted).

//! #4180 (round 5, E4): `stand <Teller> n2` landed on the outer wall of the bank. The lobby floor is 1.1 m above the
//! street, so the ring's lowest ground was the pavement and every inside spot read as a counter top. A character
//! target stands on the floor: its own height is the floor to match, not the lowest ground around it. And a
//! character is stood in front of (its customer side), not on the first clear spot around it, which was the street.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_HarnessStandRaisedFloor : LF_Test
{
	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "harness/stand-raised-floor";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		bool targetStands = true;
		if (RedProof())
			targetStands = false;

		// The bank teller: feet 13.05, lobby floor 13.10, the pavement outside 11.99 (the ring's lowest ground).
		ctx.True(LF_AgentHarness.StandSpotOnFloor(13.1, 11.99, 13.053, targetStands), "a teller's lobby floor is a spot");
		ctx.True(!LF_AgentHarness.StandSpotOnFloor(11.99, 11.99, 13.053, targetStands), "the pavement a floor below the teller is not");
		// A prop keeps the old rule: the top of a counter is higher than the floor of its ring.
		ctx.True(LF_AgentHarness.StandSpotOnFloor(0.1, 0, 0.9, false), "a prop's floor spot is a spot");
		ctx.True(!LF_AgentHarness.StandSpotOnFloor(1.0, 0, 0.9, false), "a counter top beside a prop is not");

		// The customer side: the teller faces -7 degrees (+Z), so a spot 2 m north is in front and one 2 m south is behind.
		vector teller = "4367.72 13.05 10650.73";
		bool inFront = LF_AgentHarness.StandSpotInFront(teller, -7, "4367.72 13.1 10652.73");
		if (RedProof())
			inFront = !inFront;

		ctx.True(inFront, "a spot north of a teller facing north is in front");
		ctx.True(!LF_AgentHarness.StandSpotInFront(teller, -7, "4367.72 12 10648.7"), "the street behind the teller is not");
	}
}
