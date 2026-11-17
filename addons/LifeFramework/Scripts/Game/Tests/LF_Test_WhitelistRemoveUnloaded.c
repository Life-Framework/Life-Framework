// red-proof: observed 2026-11-17 (bug fix: the test run with the fix reverted failed) on the unfixed LF_WhitelistManager (remove returned false on an empty cache): "a remove before any load still removes", "after the next restart the officer is off the roster". Green with the fix.
// tier: LOGIC
//! whitelist/remove-before-load (review 2026-11-17): a roster removal that comes before anything
//! loaded the whitelist file (LF_PoliceApplications.ExpireTrials on the first duty desk use after a
//! restart) loads the file and removes the entry. It used to return false on the empty cache, so the
//! trial was marked expired while the officer stayed on the roster for good.
class LF_Test_WhitelistRemoveUnloaded : LF_Test
{
	protected static const string PATH = "$profile:LifeFramework/test_whitelist_remove_unloaded.txt";
	protected static const string UID = "wl-remove-unloaded-uid";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "whitelist/remove-before-load";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		FileIO.DeleteFile(PATH);
		LF_WhitelistManager.SetSavePath(PATH);
		LF_WhitelistManager.Reset();

		ctx.True(LF_WhitelistManager.AddFactionWhitelist(UID, LF_EFactionType.POLICE), "the officer is put on the police roster");
		ctx.True(LF_WhitelistManager.GetWhitelistedFactions(UID).Contains(LF_EFactionType.POLICE), "the roster lists the officer");

		// A restart: the cache is gone, the file stays.
		LF_WhitelistManager.Reset();
		ctx.True(LF_WhitelistManager.RemoveFactionWhitelist(UID, LF_EFactionType.POLICE), "a remove before any load still removes");

		LF_WhitelistManager.Reset();
		LF_WhitelistManager.Load();
		ctx.False(LF_WhitelistManager.GetWhitelistedFactions(UID).Contains(LF_EFactionType.POLICE), "after the next restart the officer is off the roster");

		FileIO.DeleteFile(PATH);
		LF_WhitelistManager.SetSavePath("");
		LF_WhitelistManager.Reset();
	}
}
