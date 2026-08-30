// red-proof: perturb the enum bounds in EL_DebugMenu.c (e.g. start COUNT at a value that makes
// a real id out of range, or make IsValidAction accept ids >= COUNT), then run the fast tier;
// the range assertions below go red.

// tier: LOGIC
class EL_Test_DebugActionValidation : EL_Test
{
	override string GetName()
	{
		return "debug-menu/action-validation";
	}

	override void Run(EL_TestContext ctx)
	{
		ctx.True(EL_DebugActionUtils.IsValidAction(EL_EDebugAction.GIVE_CASH), "first action id is valid");
		ctx.True(EL_DebugActionUtils.IsValidAction(EL_EDebugAction.SET_JOB), "last real action id is valid");
		ctx.False(EL_DebugActionUtils.IsValidAction(EL_EDebugAction.COUNT), "the COUNT sentinel is not a valid action");
		ctx.False(EL_DebugActionUtils.IsValidAction(-1), "negative ids are invalid");
		ctx.False(EL_DebugActionUtils.IsValidAction(999), "out-of-range ids are invalid");
	}
}
