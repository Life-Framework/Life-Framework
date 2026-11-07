// red-proof: made EL_DevGate.IsAllowed return true and ran `tools\cli test --no-build --tier fast`
// (2026-11-07). Observed:
//   [ELTEST] FAIL debug-menu/gate
//   [ELTEST]   - refused when not debug and not admin
//   [ELTEST] SUMMARY tier=fast passed=79 failed=1 total=80
// Restored `return devToolsEnabled || isAdmin;` and the suite went green.

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
