// red-proof: observed 2026-11-14 with `tools\cli test --no-build --only LF_Test_CuffingRules`: LF_Cuffing.Step judging the rule before reach failed `reach is judged before the rule (expected 1, got 2)`; restored, green.
// tier: LOGIC
//! police/cuffing-rules: the rules of the Cuff and Uncuff hold (LF_Cuffing). A hold goes on while the pair
//! stays in reach and the cuffing rule holds; reach is judged first; a hold the server never hears the
//! end of times out; and the server accepts the end of a hold only after most of the action's duration,
//! so a client cannot cuff in an instant.
class LF_Test_CuffingRules : LF_Test
{
	override string GetName()
	{
		return "police/cuffing-rules";
	}

	override void Run(LF_TestContext ctx)
	{
		// Step(reach, rule, elapsedSeconds, maxSeconds)
		ctx.Equal(LF_ECuffingStep.KEEP, LF_Cuffing.Step(LF_EInteractionRefusal.NONE, LF_ERestrain.OK, 0.5, 8), "a pair in reach under the rule keeps cuffing");
		ctx.Equal(LF_ECuffingStep.STOP_REACH, LF_Cuffing.Step(LF_EInteractionRefusal.TOO_FAR, LF_ERestrain.OK, 0.5, 8), "walking out of reach stops the hold");
		ctx.Equal(LF_ECuffingStep.STOP_REACH, LF_Cuffing.Step(LF_EInteractionRefusal.DEAD, LF_ERestrain.OK, 0.5, 8), "someone dying stops the hold");
		ctx.Equal(LF_ECuffingStep.STOP_REFUSED, LF_Cuffing.Step(LF_EInteractionRefusal.NONE, LF_ERestrain.NO_CUFFS, 0.5, 8), "cuffs out of the hand stop the hold");
		ctx.Equal(LF_ECuffingStep.STOP_REFUSED, LF_Cuffing.Step(LF_EInteractionRefusal.NONE, LF_ERestrain.NOT_COMPLIANT, 0.5, 8), "a suspect who is no longer held up or downed stops the hold");
		ctx.Equal(LF_ECuffingStep.STOP_REACH, LF_Cuffing.Step(LF_EInteractionRefusal.TOO_FAR, LF_ERestrain.NO_CUFFS, 0.5, 8), "reach is judged before the rule");
		ctx.Equal(LF_ECuffingStep.STOP_TIMEOUT, LF_Cuffing.Step(LF_EInteractionRefusal.NONE, LF_ERestrain.OK, 8.5, 8), "a hold whose end never arrives is dropped");
		ctx.Equal(LF_ECuffingStep.KEEP, LF_Cuffing.Step(LF_EInteractionRefusal.NONE, LF_ERestrain.OK, 8, 8), "the watchdog is not tripped at its limit");

		// HoldLongEnough(elapsed, duration)
		ctx.True(LF_Cuffing.HoldLongEnough(1.8, 1.8), "a full hold is accepted");
		ctx.True(LF_Cuffing.HoldLongEnough(1.5, 1.8), "a hold a little short of the bar is accepted: the start report can lag");
		ctx.False(LF_Cuffing.HoldLongEnough(0.2, 1.8), "an instant completion is refused");
		ctx.False(LF_Cuffing.HoldLongEnough(1.0, 1.8), "half the duration is refused");
	}
}
