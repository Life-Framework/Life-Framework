// red-proof: RED-OK 2026-11-16 `tools\cli test --no-build --red LF_Test_UIBackPath`: with the hook on the Back press is skipped and "plain sheet closes on Back", the form and the ATM sheet fail; the green run passes 12 assertions.
// tier: WORLD
//! Back closes every kit sheet through one path: LF_UIScreen.Back(), reached by the MenuBack action
//! listener and by the per-frame poll, with LF_UIBack refusing the second report of one press.
//! The sheets open here are the kit's three shapes: a plain sheet with Close, a form with a text
//! field (focus on a field must not swallow Back), and the ATM terminal skin; every other screen is
//! the same LF_UIScreen.Open call (validate-ui-back-path keeps it that way).
//! Backspace is one of MenuBack's sources; LF_UIBack.IsTyping keeps it for a focused text field
//! (validate-ui-back-path also fails a kit sheet with no Close part).
//! LF_DialogueScene is not a kit sheet: it shares LF_UIBack, so a closed scene refuses Back.
class LF_Test_UIBackPath : LF_Test
{
	override string GetName()
	{
		return "ui-kit/back-path";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		array<ref LF_UIPart> plain = {};
		plain.Insert(LF_UIPart.Create(LF_EUIPartKind.TITLE, "BackTitle", "Back path"));
		plain.Insert(LF_UIPart.Create(LF_EUIPartKind.CLOSE, "BackClose", "Close"));
		CheckSheet(ctx, "plain sheet", LF_UIScreen.Open(plain));

		array<ref LF_UIPart> form = {};
		form.Insert(LF_UIPart.Create(LF_EUIPartKind.FIELD, "BackName", "Given name"));
		form.Insert(LF_UIPart.Create(LF_EUIPartKind.BUTTON, "BackSign", "Sign"));
		CheckSheet(ctx, "form with a field", LF_UIScreen.Open(form));

		CheckSheet(ctx, "ATM terminal", LF_UIScreen.Open(LF_ATMTerminal.BuildParts(), LF_UISkin.Terminal()));

		ctx.False(LF_UIBack.IsTyping(false), "Backspace is Back when no text field has focus");
		LF_UIBack.Reset();
		ctx.False(LF_DialogueScene.Back(), "a closed dialogue scene has nothing to leave");
		ctx.False(LF_DialogueScene.HasBackHandler(), "a closed dialogue scene holds no Back listener");
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckSheet(LF_TestContext ctx, string what, LF_UIScreen screen)
	{
		ctx.NotNull(screen, what + " opens");
		if (!screen)
			return;

		ctx.True(screen.HasBackHandler(), what + " registers the MenuBack handler");
		ctx.True(screen.IsOpen(), what + " is open before Back");
		ctx.False(screen.IsFieldFocused(), what + ": focus starts on a button, so Back is never mistaken for typing");

		LF_UIBack.Reset();
		if (!RedProof())
			screen.Back();

		ctx.False(screen.IsOpen(), what + " closes on Back");
		ctx.False(screen.Back(), what + ": a second Back report for the same press does nothing");
		screen.Close();
	}
}
