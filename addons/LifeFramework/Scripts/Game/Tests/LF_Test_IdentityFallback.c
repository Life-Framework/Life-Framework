// red-proof: Observed 2026-11-08 with `tools\cli test --no-build --only LF_Test_IdentityFallback`: making the
// -noBackend branch unreachable in LF_Utils.ChooseIdentityFallback failed it with
// `[LFTEST] FAIL spawn/identity-fallback`, `- a -noBackend dedicated server keys a player by name`. Restored; green.

// tier: LOGIC
class LF_Test_IdentityFallback : LF_Test
{
	override string GetName()
	{
		return "spawn/identity-fallback";
	}

	override void Run(LF_TestContext ctx)
	{
		// tools\cli host start runs a -noBackend dedicated server: no backend ever answers, so a
		// friend who joins must get an identity from their name, or the faction pick is refused
		// with #EL-CC_Error_NoAccount and the player never spawns.
		ctx.True(LF_Utils.ChooseIdentityFallback(true, false, true) == LF_EIdentityFallback.PLAYER_NAME, "a -noBackend dedicated server keys a player by name");
		ctx.True(LF_Utils.ChooseIdentityFallback(true, false, false) == LF_EIdentityFallback.WAIT, "a dedicated server with a backend waits for the real identity");
		ctx.True(LF_Utils.ChooseIdentityFallback(true, true, true) == LF_EIdentityFallback.SESSION_SLOT, "the multi-client session keys a player by player id");
		ctx.True(LF_Utils.ChooseIdentityFallback(false, false, false) == LF_EIdentityFallback.PLAYER_NAME, "Workbench play and a listen host key a player by name");
	}
}
