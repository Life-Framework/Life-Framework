// red-proof: RED-OK 2026-11-17 `tools/cli test --no-build --red LF_Test_CharacterSheetsReset`: with the reset skipped the form and pick stay current and open (3 assertions fail); green with the hooks (--only, 3 passed with LF_Test_UiSheetStaticReset and LF_Test_UIWorldEndReset).
//! #2512: the registration form and the character pick keep their open screen in a static. A world
//! that ends with one up (a disconnect before signing, a pick still open at the kick) left the
//! sheet drawn and holding the interaction prompt, because the reset only dropped the pointer.
//! Both resets now take the screen down first. Open needs a player controller, so the test hands
//! each class an already open kit sheet through its seam; the sheet's public state shows the hide.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_CharacterSheetsReset : LF_Test
{
	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "character-creation/sheets-world-end-reset";
	}

	//------------------------------------------------------------------------------------------------
	protected LF_UIScreen OpenSheet(LF_TestContext ctx, string title)
	{
		array<ref LF_UIPart> parts = {};
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.TITLE, title, "World end"));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.CLOSE, title + "Close", "Close"));
		LF_UIScreen sheet = LF_UIScreen.Open(parts);
		ctx.NotNull(sheet, title + ": a kit sheet opens");
		return sheet;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_UIScreen form = OpenSheet(ctx, "SheetResetForm");
		if (!form)
			return;

		LF_RegistrationForm.AdoptForTest(form);
		ctx.True(LF_RegistrationForm.Current() == form, "the form is the current one before the world ends");
		if (!RedProof())
			LF_RegistrationForm.ResetStatics();

		ctx.True(LF_RegistrationForm.Current() == null, "no registration form survives a world end");
		ctx.False(form.IsOpen(), "the open registration form is hidden at world end");
		form.Close();

		LF_UIScreen pick = OpenSheet(ctx, "SheetResetPick");
		if (!pick)
			return;

		LF_CharacterSelectSheet.AdoptForTest(pick);
		ctx.True(LF_CharacterSelectSheet.Current() == pick, "the pick is the current one before the world ends");
		if (!RedProof())
			LF_CharacterSelectSheet.ResetStatics();

		ctx.True(LF_CharacterSelectSheet.Current() == null, "no character pick survives a world end");
		ctx.False(pick.IsOpen(), "the open character pick is hidden at world end");
		pick.Close();
	}
}
