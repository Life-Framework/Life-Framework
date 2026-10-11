// red-proof: the assertion "a restart still refuses a uid that was not saved" was
// inverted to ctx.True (expecting the unsaved uid to be listed) and then restored
// in this same change. The game suite was not booted, so no [ELTEST] FAIL line
// was observed.

// tier: LOGIC
class EL_Test_WhitelistPersist : EL_Test
{
	override string GetName()
	{
		return "whitelist/persist";
	}

	override void Run(EL_TestContext ctx)
	{
		string savedUid = "whitelist-persist-saved";
		string unsavedUid = "whitelist-persist-unsaved";
		string path = "$profile:TestResults/whitelist-persist.txt";

		EL_WhitelistManager.SetSavePath(path);
		EL_WhitelistManager.Reset();
		if (FileIO.FileExists(path))
			FileIO.DeleteFile(path);

		ctx.Equal(1, EL_WhitelistManager.VERSION, "whitelist save version is 1");

		int missingBefore = EL_WhitelistManager.GetMissingFileCount();
		EL_WhitelistManager.Load();
		ctx.Equal(missingBefore + 1, EL_WhitelistManager.GetMissingFileCount(), "a missing whitelist file is logged");
		ctx.False(EL_WhitelistManager.IsWhitelistedForFaction(savedUid, EL_EFactionType.POLICE), "a missing file leaves the police faction closed");
		ctx.True(EL_WhitelistManager.IsWhitelistedForFaction(unsavedUid, EL_EFactionType.CIVILIAN), "civilian stays open when the file is missing");

		int refusedBefore = EL_WhitelistManager.GetFactionRefusalCount();
		ctx.False(EL_WhitelistManager.MayJoinFaction(unsavedUid, EL_EFactionType.POLICE), "choosing police is refused when the uid is not on the list");
		ctx.Equal(refusedBefore + 1, EL_WhitelistManager.GetFactionRefusalCount(), "the police refusal is logged");
		ctx.True(EL_WhitelistManager.MayJoinFaction(unsavedUid, EL_EFactionType.CIVILIAN), "choosing civilian stays open");
		ctx.Equal(refusedBefore + 1, EL_WhitelistManager.GetFactionRefusalCount(), "civilian is not a refusal");

		ctx.True(EL_WhitelistManager.AddFactionWhitelist(savedUid, EL_EFactionType.POLICE), "adding a police uid writes the list");

		string versionLine = "";
		FileHandle versionFile = FileIO.OpenFile(path, FileMode.READ);
		if (versionFile)
		{
			versionFile.ReadLine(versionLine);
			versionFile.Close();
		}
		versionLine.TrimInPlace();
		ctx.EqualStr(EL_WhitelistManager.VERSION.ToString(), versionLine, "the save starts with the version");

		EL_WhitelistManager.Reset();
		EL_WhitelistManager.Load();
		ctx.True(EL_WhitelistManager.IsWhitelistedForFaction(savedUid, EL_EFactionType.POLICE), "the saved uid survives a restart");
		ctx.False(EL_WhitelistManager.IsWhitelistedForFaction(unsavedUid, EL_EFactionType.POLICE), "a restart still refuses a uid that was not saved");
		ctx.True(EL_WhitelistManager.MayJoinFaction(savedUid, EL_EFactionType.POLICE), "a saved uid may choose police after restart");
		int refusedAfterRestart = EL_WhitelistManager.GetFactionRefusalCount();
		ctx.False(EL_WhitelistManager.MayJoinFaction(unsavedUid, EL_EFactionType.POLICE), "choosing police is still refused for a uid that was not saved");
		ctx.Equal(refusedAfterRestart + 1, EL_WhitelistManager.GetFactionRefusalCount(), "the restart refusal is logged");
		ctx.True(EL_WhitelistManager.MayJoinFaction(unsavedUid, EL_EFactionType.CIVILIAN), "civilian stays open after restart");

		ctx.Equal(1, EL_WhitelistManager.GetWhitelistedFactions(savedUid).Count(), "the saved uid has one police entry");
		int missingAfterSave = EL_WhitelistManager.GetMissingFileCount();
		EL_WhitelistManager.Load();
		ctx.Equal(1, EL_WhitelistManager.GetWhitelistedFactions(savedUid).Count(), "a second load does not duplicate the saved uid");
		ctx.True(EL_WhitelistManager.IsWhitelistedForFaction(savedUid, EL_EFactionType.POLICE), "the saved uid stays on the list");
		ctx.False(EL_WhitelistManager.IsWhitelistedForFaction(unsavedUid, EL_EFactionType.POLICE), "a second load still refuses a uid that was not saved");
		ctx.Equal(missingAfterSave, EL_WhitelistManager.GetMissingFileCount(), "loading a present file is not a missing-file error");

		if (FileIO.FileExists(path))
			FileIO.DeleteFile(path);
		EL_WhitelistManager.SetSavePath("");
		EL_WhitelistManager.Reset();
	}
}
