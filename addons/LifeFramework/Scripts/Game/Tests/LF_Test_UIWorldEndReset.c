// red-proof: RED-OK 2026-11-18 `tools\cli test --no-build --red LF_Test_KeypadBackspace,LF_Test_UIWorldEndReset`: with the hook on the prompt hold survives and the stale death screen stays (8 assertions).
//! A world can end with a UI open: a failed quiz kicks the player with the result page up, a player
//! dies and disconnects before pressing Respawn. Before the fix nothing reset the kit's prompt-hold
//! count or the death screen's instance, so the next session in the same game process hid every
//! interaction prompt (LF_ActionPromptGate) and returned the stale death screen. Each owner's
//! LF_StaticResetHook now drops them; this applies only those two owners' resets.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_UIWorldEndReset : LF_Test
{
	override string GetName()
	{
		return "ui-kit/world-end-reset";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		array<ref LF_UIPart> parts = {};
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.TITLE, "ResetTitle", "World end"));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.CLOSE, "ResetClose", "Close"));
		LF_UIScreen sheet = LF_UIScreen.Open(parts);
		ctx.NotNull(sheet, "a kit sheet opens");
		ctx.True(LF_UIScreen.IsActionPromptSuppressed(), "an open sheet holds the interaction prompt");

		if (!RedProof())
			LF_UIScreen.ResetStatics();

		ctx.False(LF_UIScreen.IsActionPromptSuppressed(), "after the world-end reset no sheet of the old world holds the prompt");
		if (sheet)
			sheet.Close();

		ctx.False(LF_UIScreen.IsActionPromptSuppressed(), "closing the old sheet afterwards does not leave a negative hold");

		LF_DeathScreen death = LF_DeathScreen.Open(null);
		ctx.NotNull(death, "the death screen opens");
		if (!RedProof())
			LF_DeathScreen.ResetStatics();

		LF_DeathScreen stale = LF_DeathScreen.GetInstance();
		ctx.True(stale == null, "after the world-end reset the next death opens a fresh death screen");
		if (stale)
			stale.Close();
	}
}
