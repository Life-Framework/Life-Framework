// red-proof: 2026-11-17 on main (before the fix) `tools/cli test --no-build --only LF_Test_JobWhitelistRoster` failed: the account on the police roster was refused the police job.
// tier: WORLD
//! whitelist/job-roster (#2577): the police job and licence checks read the faction roster by account
//! uid. An account on the POLICE roster passes with its second character active (the character key is
//! not the roster key); an account off it fails; the old per-job list still passes, read only.
class LF_Test_JobWhitelistRoster : LF_Test
{
	protected static const string ROSTER = "$profile:TestResults/job-whitelist-roster.txt";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "whitelist/job-roster";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		const int playerId = 9577;
		string accountUid = LF_Utils.GetAccountUID(playerId);
		ctx.False(accountUid.IsEmpty(), "the test player id has an account uid");
		if (accountUid.IsEmpty())
			return;

		FileIO.DeleteFile(ROSTER);
		LF_WhitelistManager.SetSavePath(ROSTER);
		LF_WhitelistManager.Reset();
		LF_WhitelistManager.Load();
		LF_JobGates.UseForTest(LF_JobGates.OVERRIDE_OFF);

		LF_PlayerAccountManager accounts = LF_PlayerAccountManager.GetInstance();
		LF_PlayerAccount account = LF_PlayerAccount.Create(accountUid);
		accounts.AddToCache(account);
		account.AddCharacter(LF_PlayerCharacter.Create(LF_TestCharacterFixture.CHARACTER_PREFAB, "Una", "First", 30), true);
		account.AddCharacter(LF_PlayerCharacter.Create(LF_TestCharacterFixture.CHARACTER_PREFAB, "Duo", "Second", 30), true);
		string characterKey = LF_Utils.GetPlayerUID(playerId);
		ctx.True(characterKey != accountUid, "the second character plays under its own key, not the account uid");

		ctx.False(LF_WhitelistManager.IsWhitelistedForJob(accountUid, LF_EJobType.POLICE), "off the roster: the police job is refused");

		LF_WhitelistManager.AddFactionWhitelist(accountUid, LF_EFactionType.POLICE);
		ctx.True(LF_WhitelistManager.IsWhitelistedForJob(accountUid, LF_EJobType.POLICE), "on the police roster: the account passes the police job with its second character active");
		ctx.False(LF_WhitelistManager.IsWhitelistedForJob(characterKey, LF_EJobType.POLICE), "the character key is not a roster key and is refused");
		ctx.True(LF_WhitelistManager.IsWhitelistedForFaction(accountUid, LF_EFactionType.POLICE), "the faction roster agrees");

		LF_WhitelistManager.RemoveFactionWhitelist(accountUid, LF_EFactionType.POLICE);
		ctx.False(LF_WhitelistManager.IsWhitelistedForJob(accountUid, LF_EJobType.POLICE), "removed from the roster: the police job is refused again");

		// The old per-job list is read for back-compat.
		string legacyUid = "job-roster-legacy-uid";
		LF_WhitelistManager.AddJobWhitelist(legacyUid, LF_EJobType.POLICE);
		ctx.True(LF_WhitelistManager.IsWhitelistedForJob(legacyUid, LF_EJobType.POLICE), "an old per-job entry still passes");

		LF_JobGates.UseForTest(LF_JobGates.OVERRIDE_NONE);
		LF_WhitelistManager.SetSavePath("");
		LF_WhitelistManager.Reset();
		FileIO.DeleteFile(ROSTER);
		accounts.RemoveFromCache(accountUid);
	}
}
