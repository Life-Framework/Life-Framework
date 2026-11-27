// red-proof: RED-OK 2026-11-27 `tools/cli test --no-build --red LF_Test_DutyLockerCash,LF_Test_SearchSuspectName` (log test-20261127-183949): with the introduction skipped it fails "after the introduction the officer reads the character name (expected Ivan Petrov, got an unknown person)". Green without the hook; before the fix the toast took LF_Utils.GetPlayerName (the account name).
//! catalogue/search-suspect-name (#5145): the officer's "Seized from ..." toast names the suspect
//! only after the suspect introduced themselves to that officer (LF_Introductions), and then by the
//! character name. A stranger is "an unknown person"; the account name never shows.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_SearchSuspectName : LF_Test
{
	protected static const string OFFICER_UID = "test-search-name-officer";
	protected static const string SUSPECT_UID = "test-search-name-suspect";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "catalogue/search-suspect-name";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		array<ref LF_IntroductionRecord> introductions = LF_Introductions.ExportAll();
		LF_Introductions.ApplyAll(new array<ref LF_IntroductionRecord>());

		LF_PlayerAccount suspect = LF_PlayerAccountManager.GetOrCreate(SUSPECT_UID);
		if (!suspect.GetActiveCharacter())
			suspect.AddCharacter(LF_PlayerCharacter.Create(LF_TestCharacterFixture.CHARACTER_PREFAB, "Ivan", "Petrov", 30), true);

		string characterName = LF_PlayerInteraction.DisplayName(SUSPECT_UID);
		ctx.False(characterName.IsEmpty(), "the suspect has a character name");

		string unknown = WidgetManager.Translate("#LF-Search_UnknownSuspect");
		ctx.EqualStr(unknown, LF_ItemSearch.SuspectNameFor(OFFICER_UID, SUSPECT_UID), "a stranger is an unknown person to the officer");
		ctx.EqualStr(unknown, LF_ItemSearch.SuspectNameFor("", SUSPECT_UID), "an officer with no account id knows nobody");

		array<string> lines = {"M9"};
		string strangerText = LF_ItemSearch.SeizedText(LF_ItemSearch.SuspectNameFor(OFFICER_UID, SUSPECT_UID), lines);
		ctx.False(strangerText.Contains(characterName), "the stranger's toast does not name them");
		ctx.True(strangerText.Contains(unknown), "the stranger's toast says an unknown person");

		// The suspect introduced themselves to someone else: still a stranger to this officer.
		LF_Introductions.Introduce("test-search-name-other", SUSPECT_UID);
		ctx.EqualStr(unknown, LF_ItemSearch.SuspectNameFor(OFFICER_UID, SUSPECT_UID), "an introduction to someone else tells the officer nothing");

		if (!RedProof())
			LF_Introductions.Introduce(OFFICER_UID, SUSPECT_UID);
		ctx.EqualStr(characterName, LF_ItemSearch.SuspectNameFor(OFFICER_UID, SUSPECT_UID), "after the introduction the officer reads the character name");

		LF_Introductions.ApplyAll(introductions);
	}
}
