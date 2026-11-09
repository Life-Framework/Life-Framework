// red-proof: made LF_DevGate.IsAllowed return true and ran `tools\cli test --no-build --tier fast`
// (2026-11-07). Observed:
//   [LFTEST] FAIL debug-menu/gate
//   [LFTEST]   - refused when not debug and not admin
//   [LFTEST] SUMMARY tier=fast passed=79 failed=1 total=80
// Restored `return devToolsEnabled || isAdmin;` and the suite went green.

// tier: LOGIC
class LF_Test_DebugMenuGate : LF_Test
{
	override string GetName()
	{
		return "debug-menu/gate";
	}

	override void Run(LF_TestContext ctx)
	{
		ctx.False(LF_DevGate.IsAllowed(false, false), "refused when not debug and not admin");
		ctx.True(LF_DevGate.IsAllowed(true, false), "allowed when the game mode enables dev tools");
		ctx.True(LF_DevGate.IsAllowed(false, true), "allowed for an administrator");
		ctx.True(LF_DevGate.IsAllowed(true, true), "allowed for an administrator in a debug game mode");
	}
}
