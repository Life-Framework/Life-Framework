// red-proof: 2026-11-17 `tools/cli test --no-build --red LF_Test_WhitelistLegacyFailurePaths,LF_Test_WhitelistLegacyIgnoredCount`: RED-OK whitelist/legacy-ignored-count (fails with the hook on, 10 assertions); green with --only.
// tier: WORLD
//! whitelist/legacy-ignored-count (#2607): after the move, a uid added to the legacy file is counted
//! as ignored (Migrate warns with that count once per start) and never reaches the roster.
class LF_Test_WhitelistLegacyIgnoredCount : LF_Test
{
	protected static const string ROSTER = "$profile:TestResults/whitelist-ignored-roster.txt";
	protected static const string LEGACY = "$profile:TestResults/whitelist-ignored-police.txt";
	protected static const string LEGACY_CONNECT = "$profile:TestResults/whitelist-ignored-connect.txt";
	protected static const string MARKER = "$profile:TestResults/whitelist-ignored-marker.txt";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "whitelist/legacy-ignored-count";
	}

	//------------------------------------------------------------------------------------------------
	protected void WriteLines(string path, array<string> lines)
	{
		FileIO.MakeDirectory("$profile:TestResults");
		FileHandle file = FileIO.OpenFile(path, FileMode.WRITE);
		if (!file)
			return;

		foreach (string line : lines)
		{
			file.WriteLine(line);
		}
		file.Close();
	}

	//------------------------------------------------------------------------------------------------
	protected void Cleanup()
	{
		FileIO.DeleteFile(ROSTER);
		FileIO.DeleteFile(LEGACY);
		FileIO.DeleteFile(LEGACY_CONNECT);
		FileIO.DeleteFile(MARKER);
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		string uidA = "legacy-ignored-a";
		string uidB = "legacy-ignored-b";
		string late1 = "legacy-ignored-late-1";
		string late2 = "legacy-ignored-late-2";

		Cleanup();
		LF_WhitelistManager.SetSavePath(ROSTER);
		LF_WhitelistManager.Reset();
		LF_WhitelistManager.Load();

		array<string> legacyIn = {uidA, uidB};
		WriteLines(LEGACY, legacyIn);
		ctx.Equal(2, LF_WhitelistLegacyMigration.Migrate(LF_WhitelistType.POLICE, LEGACY, MARKER), "the first start migrates both uids");

		int ignoredNone = -1;
		if (!RedProof())
			ignoredNone = LF_WhitelistLegacyMigration.CountIgnored(LF_WhitelistType.POLICE, LEGACY);
		ctx.Equal(0, ignoredNone, "a legacy file that matches the roster has no ignored uids");

		// An admin who still edits the legacy file adds two uids.
		array<string> legacyAfter = {uidA, uidB, late1, " " + late2, late1};
		WriteLines(LEGACY, legacyAfter);

		int ignored = 0;
		if (!RedProof())
			ignored = LF_WhitelistLegacyMigration.CountIgnored(LF_WhitelistType.POLICE, LEGACY);
		ctx.Equal(2, ignored, "two uids added after the move are counted once each");

		ctx.Equal(0, LF_WhitelistLegacyMigration.Migrate(LF_WhitelistType.POLICE, LEGACY, MARKER), "the next start adds nothing");
		ctx.False(LF_WhitelistManager.IsWhitelistedForFaction(late1, LF_EFactionType.POLICE), "a uid added to the legacy file after the move stays off the roster");
		ctx.Equal(2, LF_WhitelistManager.CountFaction(LF_EFactionType.POLICE), "the roster is unchanged by the ignored uids");

		ctx.Equal(0, LF_WhitelistLegacyMigration.CountIgnored(LF_WhitelistType.POLICE, "$profile:TestResults/whitelist-ignored-missing.txt"), "a missing legacy file has no ignored uids");

		array<string> connectLines = {"legacy-ignored-connect"};
		WriteLines(LEGACY_CONNECT, connectLines);
		ctx.Equal(0, LF_WhitelistLegacyMigration.CountIgnored(LF_WhitelistType.CONNECT, LEGACY_CONNECT), "a type with no roster counts nothing");

		Cleanup();
		LF_WhitelistManager.SetSavePath("");
		LF_WhitelistManager.Reset();
	}
}
