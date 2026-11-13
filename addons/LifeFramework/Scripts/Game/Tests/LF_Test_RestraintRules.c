// red-proof: observed 2026-11-13 with `tools\cli test --no-build --only LF_Test_RestraintRules`: IsCompliant with `<` in place of `<=` failed `the window includes its last second`; restored, green.
// tier: LOGIC
//! police/restraint-rules: who may cuff whom. An on-duty officer holding handcuffs may cuff a downed
//! player, or a held-up one who holds no weapon while the hold-up is recent; never someone already
//! cuffed. Any on-duty officer may uncuff a cuffed player. A hold-up records when its target was
//! held up, which is what makes them compliant.
class LF_Test_RestraintRules : LF_Test
{
	override string GetName()
	{
		return "police/restraint-rules";
	}

	override void Run(LF_TestContext ctx)
	{
		// DecideRestrain(onDutyPolice, holdsCuffs, targetDowned, targetCompliant, targetRestrained)
		ctx.Equal(LF_ERestrain.OK, LF_Restraint.DecideRestrain(true, true, true, false, false), "an officer with cuffs may cuff a downed player");
		ctx.Equal(LF_ERestrain.OK, LF_Restraint.DecideRestrain(true, true, false, true, false), "an officer with cuffs may cuff a compliant player");
		ctx.Equal(LF_ERestrain.NOT_COMPLIANT, LF_Restraint.DecideRestrain(true, true, false, false, false), "a standing, non-compliant player cannot be cuffed");
		ctx.Equal(LF_ERestrain.NO_CUFFS, LF_Restraint.DecideRestrain(true, false, true, false, false), "no cuffs in hand, no cuffing");
		ctx.Equal(LF_ERestrain.NOT_POLICE, LF_Restraint.DecideRestrain(false, true, true, false, false), "only an on-duty officer cuffs");
		ctx.Equal(LF_ERestrain.ALREADY_RESTRAINED, LF_Restraint.DecideRestrain(true, true, true, false, true), "a cuffed player is not cuffed twice");

		ctx.Equal(LF_ERestrain.OK, LF_Restraint.DecideRelease(true, true), "an on-duty officer uncuffs a cuffed player");
		ctx.Equal(LF_ERestrain.NOT_RESTRAINED, LF_Restraint.DecideRelease(true, false), "there is nothing to uncuff on a free player");
		ctx.Equal(LF_ERestrain.NOT_POLICE, LF_Restraint.DecideRelease(false, true), "a civilian cannot uncuff");

		// IsCompliant(heldUpAt, now, windowSeconds, armed)
		ctx.True(LF_Restraint.IsCompliant(1000, 1030, 60, false), "held up 30 s ago and unarmed is compliant");
		ctx.True(LF_Restraint.IsCompliant(1000, 1060, 60, false), "the window includes its last second");
		ctx.False(LF_Restraint.IsCompliant(1000, 1061, 60, false), "a hold-up older than the window has run out");
		ctx.False(LF_Restraint.IsCompliant(1000, 1030, 60, true), "a held-up player with a weapon in hand is not compliant");
		ctx.False(LF_Restraint.IsCompliant(-1, 1030, 60, false), "a player never held up is not compliant");
		ctx.False(LF_Restraint.IsCompliant(1000, 1000, 0, false), "a zero window allows downed players only");

		ctx.EqualStr("#LF-Restrain_NoCuffs", LF_Restraint.RefusalKey(LF_ERestrain.NO_CUFFS), "a refusal tells the officer why");

		// The hold-up records its target's time.
		LF_HoldUp.ResetStatics();
		ctx.Equal(-1, LF_HoldUp.HeldUpAt("lf-test-target"), "nobody has been held up yet");
		LF_EHoldUp result = LF_HoldUp.Attempt("lf-test-actor", "lf-test-target", "0 0 0", 1, true, 5000);
		ctx.Equal(LF_EHoldUp.OK, result, "an armed hold-up in reach goes ahead");
		ctx.Equal(5000, LF_HoldUp.HeldUpAt("lf-test-target"), "the hold-up records when its target was held up");
		ctx.Equal(-1, LF_HoldUp.HeldUpAt("lf-test-actor"), "the one holding up is not recorded as held up");
		LF_HoldUp.ResetStatics();
	}
}
