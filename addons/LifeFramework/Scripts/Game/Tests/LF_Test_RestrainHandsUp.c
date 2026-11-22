// red-proof: 2026-11-21 `tools\cli test --no-build --red LF_Test_RestrainHandsUp`; RedProof() passes hands down, the
// unfixed rule (only a recent hold-up made a suspect compliant), so "hands up and unarmed: may be cuffed" fails.

//! rc 0.2.0: a suspect who raised their hands to an officer's aim (LF_HandsUp) could be searched but not cuffed: the
//! cuffing rule read only a recent gunpoint hold-up. Hands up and unarmed now complies; armed never does, and a
//! compliant window of 0 still limits cuffing to downed players.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_RestrainHandsUp : LF_Test
{
	override string GetName()
	{
		return "police/restrain-hands-up";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		int now = 100000;
		bool handsUp = !RedProof();

		ctx.True(LF_Restraint.Complies(-1, now, 60, handsUp, false), "hands up and unarmed: may be cuffed");
		ctx.False(LF_Restraint.Complies(-1, now, 60, true, true), "hands up but armed: may not be cuffed");
		ctx.False(LF_Restraint.Complies(-1, now, 0, true, false), "a window of 0 keeps cuffing to downed players");
		ctx.False(LF_Restraint.Complies(-1, now, 60, false, false), "hands down, never held up: may not be cuffed");
		ctx.True(LF_Restraint.Complies(now - 30, now, 60, false, false), "a recent hold-up still complies");
		ctx.False(LF_Restraint.Complies(now - 90, now, 60, false, false), "an old hold-up does not");
		ctx.Equal(LF_ERestrain.OK, LF_Restraint.DecideRestrain(true, true, false, LF_Restraint.Complies(-1, now, 60, handsUp, false), false), "an officer with cuffs cuffs a suspect with hands up");
	}
}
