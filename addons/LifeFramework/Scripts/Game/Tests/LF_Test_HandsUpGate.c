// red-proof: Observed 2026-11-19 with `tools/cli test --no-build --red LF_Test_HandsUpGate`: RED-OK crime/hands-up-gate (fails with the hook on, 11 assertions; the gate ignores the restraint, so "restrained allows" fails); hook off, green.
//! The hands-up gate (#3399, LF_HandsUpGate): hands up allows, restrained allows, neither refuses
//! with "They haven't given up.". The hands-up flag counts only from a target that is alive,
//! player-controlled and not downed.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_HandsUpGate : LF_Test
{
	override string GetName()
	{
		return "crime/hands-up-gate";
	}

	override void Run(LF_TestContext ctx)
	{
		// allows(handsUp, restrained, alive, playerControlled, downed)
		ctx.True(LF_HandsUpGate.Allows(true, false, true, true, false), "hands up allows");

		bool restrained = true;
		if (RedProof())
			restrained = false;
		ctx.True(LF_HandsUpGate.Allows(false, restrained, true, true, false), "restrained allows");
		ctx.True(LF_HandsUpGate.Allows(true, true, true, true, false), "both allows");

		ctx.False(LF_HandsUpGate.Allows(false, false, true, true, false), "neither refuses");

		// The hands-up flag is read only from a live, player-controlled target that is not downed.
		ctx.False(LF_HandsUpGate.Allows(true, false, false, true, false), "a dead target's stale hands-up refuses");
		ctx.False(LF_HandsUpGate.Allows(true, false, true, false, false), "an AI's hands-up refuses");
		ctx.False(LF_HandsUpGate.Allows(true, false, true, true, true), "a downed target's hands-up refuses");
		ctx.True(LF_HandsUpGate.Allows(false, true, false, false, true), "restraint stands on its own");

		// No target at all has not given up, and the refusal is the localization key.
		ctx.False(LF_HandsUpGate.GivenUp(null), "no target has not given up");
		ctx.EqualStr(LF_HandsUpGate.NOT_GIVEN_UP, LF_HandsUpGate.Refusal(null), "the refusal is the not-given-up key");
	}
}
