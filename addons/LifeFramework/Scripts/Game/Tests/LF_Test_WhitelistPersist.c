// red-proof: Observed 2026-11-08 with `tools\cli test --no-build --tier all`: inverting "a restart still refuses a uid
// that was not saved" to ctx.True failed it with `[LFTEST] FAIL whitelist/persist`,
// `- a restart still refuses a uid that was not saved`. Restored; green.

// tier: LOGIC
class LF_Test_WhitelistPersist : LF_Test
{
	override string GetName()
	{
		return "whitelist/persist";
	}

	override void Run(LF_TestContext ctx)
	{
		string savedUid = "whitelist-persist-saved";
		string unsavedUid = "whitelist-persist-unsaved";
		string path = "$profile:TestResults/whitelist-persist.txt";

		LF_WhitelistManager.SetSavePath(path);
		LF_WhitelistManager.Reset();
		if (FileIO.FileExists(path))
			FileIO.DeleteFile(path);

		ctx.Equal(1, LF_WhitelistManager.VERSION, "whitelist save version is 1");

		int missingBefore = LF_WhitelistManager.GetMissingFileCount();
		LF_WhitelistManager.Load();
		ctx.Equal(missingBefore + 1, LF_WhitelistManager.GetMissingFileCount(), "a missing whitelist file is logged");
		ctx.False(LF_WhitelistManager.IsWhitelistedForFaction(savedUid, LF_EFactionType.POLICE), "a missing file leaves the police faction closed");
		ctx.True(LF_WhitelistManager.IsWhitelistedForFaction(unsavedUid, LF_EFactionType.CIVILIAN), "civilian stays open when the file is missing");

		int refusedBefore = LF_WhitelistManager.GetFactionRefusalCount();
		ctx.False(LF_WhitelistManager.MayJoinFaction(unsavedUid, LF_EFactionType.POLICE), "choosing police is refused when the uid is not on the list");
		ctx.Equal(refusedBefore + 1, LF_WhitelistManager.GetFactionRefusalCount(), "the police refusal is logged");
		ctx.True(LF_WhitelistManager.MayJoinFaction(unsavedUid, LF_EFactionType.CIVILIAN), "choosing civilian stays open");
		ctx.Equal(refusedBefore + 1, LF_WhitelistManager.GetFactionRefusalCount(), "civilian is not a refusal");

		ctx.True(LF_WhitelistManager.AddFactionWhitelist(savedUid, LF_EFactionType.POLICE), "adding a police uid writes the list");

		string versionLine = "";
		FileHandle versionFile = FileIO.OpenFile(path, FileMode.READ);
		if (versionFile)
		{
			versionFile.ReadLine(versionLine);
			versionFile.Close();
		}
		versionLine.TrimInPlace();
		ctx.EqualStr(LF_WhitelistManager.VERSION.ToString(), versionLine, "the save starts with the version");

		LF_WhitelistManager.Reset();
		LF_WhitelistManager.Load();
		ctx.True(LF_WhitelistManager.IsWhitelistedForFaction(savedUid, LF_EFactionType.POLICE), "the saved uid survives a restart");
		ctx.False(LF_WhitelistManager.IsWhitelistedForFaction(unsavedUid, LF_EFactionType.POLICE), "a restart still refuses a uid that was not saved");
		ctx.True(LF_WhitelistManager.MayJoinFaction(savedUid, LF_EFactionType.POLICE), "a saved uid may choose police after restart");
		int refusedAfterRestart = LF_WhitelistManager.GetFactionRefusalCount();
		ctx.False(LF_WhitelistManager.MayJoinFaction(unsavedUid, LF_EFactionType.POLICE), "choosing police is still refused for a uid that was not saved");
		ctx.Equal(refusedAfterRestart + 1, LF_WhitelistManager.GetFactionRefusalCount(), "the restart refusal is logged");
		ctx.True(LF_WhitelistManager.MayJoinFaction(unsavedUid, LF_EFactionType.CIVILIAN), "civilian stays open after restart");

		ctx.Equal(1, LF_WhitelistManager.GetWhitelistedFactions(savedUid).Count(), "the saved uid has one police entry");
		int missingAfterSave = LF_WhitelistManager.GetMissingFileCount();
		LF_WhitelistManager.Load();
		ctx.Equal(1, LF_WhitelistManager.GetWhitelistedFactions(savedUid).Count(), "a second load does not duplicate the saved uid");
		ctx.True(LF_WhitelistManager.IsWhitelistedForFaction(savedUid, LF_EFactionType.POLICE), "the saved uid stays on the list");
		ctx.False(LF_WhitelistManager.IsWhitelistedForFaction(unsavedUid, LF_EFactionType.POLICE), "a second load still refuses a uid that was not saved");
		ctx.Equal(missingAfterSave, LF_WhitelistManager.GetMissingFileCount(), "loading a present file is not a missing-file error");

		if (FileIO.FileExists(path))
			FileIO.DeleteFile(path);
		LF_WhitelistManager.SetSavePath("");
		LF_WhitelistManager.Reset();
	}
}
