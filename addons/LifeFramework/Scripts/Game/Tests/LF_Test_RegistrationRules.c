// red-proof: Observed 2026-11-08 with `tools\cli test --no-build --tier all`: made
// LF_RegistrationRules.IsMark return true; got `[LFTEST] FAIL creation/registration-rules`,
// `- a bad surname is refused (expected '#LF-Reg_Error_NameChars', got '')`. Restored; green.
// tier: LOGIC
class LF_Test_RegistrationRules : LF_Test
{
	override string GetName()
	{
		return "creation/registration-rules";
	}

	override void Run(LF_TestContext ctx)
	{
		ctx.EqualStr("", LF_RegistrationRules.RefuseName("Ada"), "a plain name is accepted");
		ctx.EqualStr("", LF_RegistrationRules.RefuseName("Anne-Marie"), "a hyphenated name is accepted");
		ctx.EqualStr("", LF_RegistrationRules.RefuseName("O'Neill"), "an apostrophe inside a name is accepted");
		ctx.EqualStr("", LF_RegistrationRules.RefuseName("Van Der Berg"), "single inner spaces are accepted");
		ctx.EqualStr("#EL-CC_Error_Fields", LF_RegistrationRules.RefuseName(""), "an empty name is refused");
		ctx.EqualStr("#EL-CC_Error_NameLength", LF_RegistrationRules.RefuseName("Abcdefghijklmnopqrstuvwxyzabcdefg"), "a 33 letter name is refused");
		ctx.EqualStr("#LF-Reg_Error_NameChars", LF_RegistrationRules.RefuseName("Ada1"), "a digit is refused");
		ctx.EqualStr("#LF-Reg_Error_NameChars", LF_RegistrationRules.RefuseName("#EL-ID_Information"), "a localization key is refused");
		ctx.EqualStr("#LF-Reg_Error_NameChars", LF_RegistrationRules.RefuseName("Ada  Lovelace"), "a double space is refused");
		ctx.EqualStr("#LF-Reg_Error_NameChars", LF_RegistrationRules.RefuseName("-Ada"), "a leading hyphen is refused");
		ctx.EqualStr("#LF-Reg_Error_NameChars", LF_RegistrationRules.RefuseName("Ada'"), "a trailing apostrophe is refused");
		ctx.EqualStr("#LF-Reg_Error_NameChars", LF_RegistrationRules.RefuseName("Ada<b>"), "markup is refused");

		ctx.Equal(36, LF_RegistrationRules.ParseAge(" 36 "), "a typed age parses");
		ctx.Equal(-1, LF_RegistrationRules.ParseAge("3x"), "a non-digit age is -1");
		ctx.Equal(-1, LF_RegistrationRules.ParseAge("-20"), "a signed age is -1");
		ctx.Equal(-1, LF_RegistrationRules.ParseAge("1000"), "four digits is -1");
		ctx.Equal(-1, LF_RegistrationRules.ParseAge(""), "an empty age is -1");

		ctx.EqualStr("", LF_RegistrationRules.RefuseForm(" Ada ", "Lovelace", "36"), "a valid form is accepted");
		ctx.EqualStr("#EL-CC_Error_Fields", LF_RegistrationRules.RefuseForm("Ada", "Lovelace", " "), "a blank age is refused");
		ctx.EqualStr("#EL-CC_Error_Age", LF_RegistrationRules.RefuseForm("Ada", "Lovelace", "17"), "age 17 is refused");
		ctx.EqualStr("#EL-CC_Error_Age", LF_RegistrationRules.RefuseForm("Ada", "Lovelace", "abc"), "a word for an age is refused");
		ctx.EqualStr("#LF-Reg_Error_NameChars", LF_RegistrationRules.RefuseForm("Ada", "Love1ace", "36"), "a bad surname is refused");

		// The server path: the RPC handler's check rejects what the client form would.
		LF_PlayerAccount account = LF_PlayerAccount.Create("creation-registration-rules");
		account.SetFaction(LF_Faction.CIVILIAN);
		ctx.EqualStr("#LF-Reg_Error_NameChars", LF_CharacterCreationRules.RefuseCharacter(account, "Ada", "Love<lace>", 36), "the server refuses a bad surname sent over RPC");
		ctx.EqualStr("", LF_CharacterCreationRules.RefuseCharacter(account, " Ada ", "Lovelace", 36), "the server trims and accepts a valid form");
	}
}
