// red-proof: RED-OK 2026-11-18 `tools\cli test --no-build --red LF_Test_KeypadBackspace,LF_Test_UIWorldEndReset`: with the hook on "a sheet with the keypad Backspace hotkey keeps Backspace" and "Backspace on the ATM deletes a digit" fail (9 assertions).
// tier: WORLD
//! Backspace is one of vanilla MenuBack's sources (keyboard:KC_BACK) and also the ATM keypad's
//! delete-a-digit key (LF_KeypadBack). Before the fix a Backspace on the ATM closed the terminal
//! instead of deleting a digit: LF_UIScreen.Back() kept Backspace only for a focused text field.
//! A sheet that binds LF_KeypadBack as a hotkey now keeps Backspace too; Escape and gamepad B
//! still close it, and a sheet without the keypad still closes on Backspace.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_KeypadBackspace : LF_Test
{
	override string GetName()
	{
		return "ui-kit/keypad-backspace";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_UIScreen atm = LF_UIScreen.Open(LF_ATMTerminal.BuildParts(), LF_UISkin.Terminal());
		ctx.NotNull(atm, "the ATM terminal sheet opens");
		if (!atm)
			return;

		if (!RedProof())
			atm.BindHotkey(LF_UIBack.KEYPAD_BACK, LF_ATMTerminal.BACK);

		bool keeps = atm.KeepsBackspace();
		atm.Close();

		array<ref LF_UIPart> plain = {};
		plain.Insert(LF_UIPart.Create(LF_EUIPartKind.TITLE, "KeypadTitle", "Plain"));
		plain.Insert(LF_UIPart.Create(LF_EUIPartKind.CLOSE, "KeypadClose", "Close"));
		LF_UIScreen sheet = LF_UIScreen.Open(plain);
		ctx.NotNull(sheet, "a plain sheet opens");
		bool plainKeeps = true;
		if (sheet)
		{
			plainKeeps = sheet.KeepsBackspace();
			sheet.Close();
		}

		ctx.True(keeps, "a sheet with the keypad Backspace hotkey keeps Backspace");
		ctx.True(LF_UIBack.Keeps(keeps, true), "Backspace on the ATM deletes a digit and does not close the terminal");
		ctx.False(LF_UIBack.Keeps(keeps, false), "Escape or gamepad B on the ATM still goes Back");
		ctx.False(plainKeeps, "a sheet without the keypad does not keep Backspace");
		ctx.False(LF_UIBack.Keeps(plainKeeps, true), "Backspace still closes a sheet without the keypad");
	}
}
