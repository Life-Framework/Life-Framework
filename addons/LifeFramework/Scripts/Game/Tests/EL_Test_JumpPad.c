// red-proof: expected 0 for the first enter and the fast suite reported
// [ELTEST]   - enter applies upward velocity (expected 0, got 12)

// tier: LOGIC
class EL_Test_JumpPad : EL_Test
{
	override string GetName()
	{
		return "toys/jump-pad";
	}

	override void Run(EL_TestContext ctx)
	{
		LF_JumpPad pad = new LF_JumpPad();
		pad.SetEnabled(true);
		pad.SetUpwardSpeed(LF_JumpPad.DEFAULT_UPWARD_SPEED);

		vector entered = pad.AcceptEnter(1);
		vector repeat = pad.AcceptEnter(1);
		pad.AcceptExit(1);
		vector reenter = pad.AcceptEnter(1);

		pad.SetEnabled(false);
		vector disabled = pad.AcceptEnter(2);

		ctx.EqualFloat(LF_JumpPad.DEFAULT_UPWARD_SPEED, entered[1], 0.001, "enter applies upward velocity");
		ctx.EqualFloat(0, entered[0], 0.001, "enter leaves lateral x at zero");
		ctx.EqualFloat(0, entered[2], 0.001, "enter leaves lateral z at zero");

		ctx.EqualFloat(0, repeat[1], 0.001, "the same enter applies upward velocity once");
		ctx.EqualFloat(0, repeat[0], 0.001, "repeat enter x is zero");
		ctx.EqualFloat(0, repeat[2], 0.001, "repeat enter z is zero");

		ctx.EqualFloat(LF_JumpPad.DEFAULT_UPWARD_SPEED, reenter[1], 0.001, "a new enter applies upward velocity again");

		ctx.EqualFloat(0, disabled[0], 0.001, "disabled pad x is zero");
		ctx.EqualFloat(0, disabled[1], 0.001, "disabled pad y is zero");
		ctx.EqualFloat(0, disabled[2], 0.001, "disabled pad z is zero");
	}
}
