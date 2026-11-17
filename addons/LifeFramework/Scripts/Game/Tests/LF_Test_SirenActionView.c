// red-proof: observed 2026-11-16 with `tools\cli test --no-build --red LF_Test_SirenActionView`: RED-OK sirenlights/action-view (fails with the hook on, 5 assertions).
// tier: LOGIC
//! The siren action's menu row (the ambulance bug "I can't see the options for the siren"): a seated
//! driver who is off duty sees the row greyed with a reason, never an empty menu; a seated on-duty
//! driver gets it enabled; nobody sees the row for a mode that already runs or from outside the seat.
class LF_Test_SirenActionView : LF_Test
{
	override string GetName()
	{
		return "sirenlights/action-view";
	}

	override void Run(LF_TestContext ctx)
	{
		ctx.Equal(LF_ESirenActionView.SHOWN, LF_SirenActionRules.View(true, true, false), "a seated on-duty driver gets the siren action");
		LF_ESirenActionView offDuty = LF_ESirenActionView.HIDDEN;
		if (!RedProof())
			offDuty = LF_SirenActionRules.View(true, false, false);

		ctx.Equal(LF_ESirenActionView.NEEDS_DUTY, offDuty, "a seated off-duty driver sees it greyed, not nothing");
		ctx.Equal(LF_ESirenActionView.HIDDEN, LF_SirenActionRules.View(false, true, false), "outside the driver seat there is no row");
		ctx.Equal(LF_ESirenActionView.HIDDEN, LF_SirenActionRules.View(true, true, true), "the mode that already runs is not offered again");
	}
}
