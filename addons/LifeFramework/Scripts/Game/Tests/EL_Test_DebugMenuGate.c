// red-proof: PENDING - suite not runnable until the base game is restored (game data project
// missing). To prove: make EL_DevGate.IsAllowed return true, run the fast tier, expect the
// "refused when not debug and not admin" assertion to FAIL; restore and record the line here.

// tier: LOGIC
class EL_Test_DebugMenuGate : EL_Test
{
	override string GetName()
	{
		return "debug-menu/gate";
	}

	override void Run(EL_TestContext ctx)
	{
		ctx.False(EL_DevGate.IsAllowed(false, false), "refused when not debug and not admin");
		ctx.True(EL_DevGate.IsAllowed(true, false), "allowed when the game mode enables dev tools");
		ctx.True(EL_DevGate.IsAllowed(false, true), "allowed for an administrator");
		ctx.True(EL_DevGate.IsAllowed(true, true), "allowed for an administrator in a debug game mode");
	}
}
