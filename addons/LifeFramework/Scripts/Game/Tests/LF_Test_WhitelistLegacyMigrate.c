// red-proof: 2026-11-17 `tools/cli test --no-build --red LF_Test_WhitelistLegacyMigrate`: RED-OK whitelist/legacy-migrate (fails with the hook on, 26 assertions); green with --only.
// tier: WORLD
//! whitelist/legacy-migrate (#2575): a legacy LF_Whitelist file moves into the roster once.
//! Repeats and uids already on the roster are skipped, a second start adds nothing, a missing
//! file changes nothing, CONNECT is counted and not migrated, and the legacy file is never written.
class LF_Test_WhitelistLegacyMigrate : LF_Test
{
	protected static const string ROSTER = "$profile:TestResults/whitelist-legacy-roster.txt";
	protected static const string LEGACY = "$profile:TestResults/whitelist-legacy-police.txt";
	protected static const string LEGACY_MEDIC = "$profile:TestResults/whitelist-legacy-medic.txt";
	protected static const string LEGACY_CONNECT = "$profile:TestResults/whitelist-legacy-connect.txt";
	protected static const string MISSING = "$profile:TestResults/whitelist-legacy-missing.txt";
	protected static const string MARKER = "$profile:TestResults/whitelist-legacy-marker.txt";
	protected static const string MARKER_FRESH = "$profile:TestResults/whitelist-legacy-marker-fresh.txt";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "whitelist/legacy-migrate";
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
	protected int CountLines(string path)
	{
		int count = 0;
		FileHandle file = FileIO.OpenFile(path, FileMode.READ);
		if (!file)
			return -1;

		string line;
		while (file.ReadLine(line) > 0)
		{
			count++;
		}
		file.Close();
		return count;
	}

	//------------------------------------------------------------------------------------------------
	protected void Cleanup()
	{
		FileIO.DeleteFile(ROSTER);
		FileIO.DeleteFile(LEGACY);
		FileIO.DeleteFile(LEGACY_MEDIC);
		FileIO.DeleteFile(LEGACY_CONNECT);
		FileIO.DeleteFile(MISSING);
		FileIO.DeleteFile(MARKER);
		FileIO.DeleteFile(MARKER_FRESH);
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		string uidA = "legacy-migrate-a";
		string uidB = "legacy-migrate-b";
		string uidOnRoster = "legacy-migrate-on-roster";

		Cleanup();
		LF_WhitelistManager.SetSavePath(ROSTER);
		LF_WhitelistManager.Reset();
		LF_WhitelistManager.Load();
		LF_WhitelistManager.AddFactionWhitelist(uidOnRoster, LF_EFactionType.POLICE);

		array<string> legacyLinesIn = {};
		legacyLinesIn.Insert(uidA);
		legacyLinesIn.Insert(uidA);
		legacyLinesIn.Insert(" " + uidB);
		legacyLinesIn.Insert("");
		legacyLinesIn.Insert(uidOnRoster);
		WriteLines(LEGACY, legacyLinesIn);
		int legacyLines = CountLines(LEGACY);

		int migrated = 0;
		if (!RedProof())
			migrated = LF_WhitelistLegacyMigration.Migrate(LF_WhitelistType.POLICE, LEGACY, MARKER);

		ctx.Equal(2, migrated, "two new police uids migrate (the repeat and the rostered uid are skipped)");
		ctx.Equal(3, LF_WhitelistManager.CountFaction(LF_EFactionType.POLICE), "the roster holds the two migrated uids beside the one already there");
		ctx.True(LF_WhitelistManager.IsWhitelistedForFaction(uidA, LF_EFactionType.POLICE), "uid A is on the police roster");
		ctx.True(LF_WhitelistManager.IsWhitelistedForFaction(uidB, LF_EFactionType.POLICE), "uid B (padded line) is on the police roster");
		ctx.Equal(1, LF_WhitelistManager.GetWhitelistedFactions(uidA).Count(), "the repeated uid has one police entry");
		ctx.Equal(legacyLines, CountLines(LEGACY), "the legacy file is not rewritten");

		ctx.Equal(0, LF_WhitelistLegacyMigration.Migrate(LF_WhitelistType.POLICE, LEGACY, MARKER), "a second start adds nothing");
		ctx.Equal(0, LF_WhitelistLegacyMigration.Migrate(LF_WhitelistType.POLICE, LEGACY, MARKER_FRESH), "a lost marker still adds nothing (repeats skipped)");
		ctx.Equal(3, LF_WhitelistManager.CountFaction(LF_EFactionType.POLICE), "the roster is unchanged after the repeat runs");

		LF_WhitelistManager.RemoveFactionWhitelist(uidA, LF_EFactionType.POLICE);
		ctx.Equal(0, LF_WhitelistLegacyMigration.Migrate(LF_WhitelistType.POLICE, LEGACY, MARKER), "a uid an admin removed is not put back by the next start");
		ctx.False(LF_WhitelistManager.IsWhitelistedForFaction(uidA, LF_EFactionType.POLICE), "the removed uid stays off the roster");

		ctx.Equal(0, LF_WhitelistLegacyMigration.Migrate(LF_WhitelistType.POLICE, MISSING, MARKER), "a missing legacy file migrates nothing");
		ctx.Equal(2, LF_WhitelistManager.CountFaction(LF_EFactionType.POLICE), "a missing legacy file changes nothing");
		ctx.False(FileIO.FileExists(MISSING), "a missing legacy file is not created");

		array<string> connectLines = {"legacy-migrate-connect"};
		WriteLines(LEGACY_CONNECT, connectLines);
		ctx.Equal(0, LF_WhitelistLegacyMigration.Migrate(LF_WhitelistType.CONNECT, LEGACY_CONNECT, MARKER), "CONNECT uids are not migrated");
		ctx.Equal(2, LF_WhitelistManager.CountFaction(LF_EFactionType.POLICE), "CONNECT changes no roster");

		array<string> medicLines = {"legacy-migrate-medic"};
		WriteLines(LEGACY_MEDIC, medicLines);
		ctx.Equal(0, LF_WhitelistLegacyMigration.Migrate(LF_WhitelistType.MEDIC, LEGACY_MEDIC, MARKER), "MEDIC uids are not migrated (no medic roster)");
		ctx.Equal(0, LF_WhitelistManager.GetWhitelistedJobs("legacy-migrate-medic").Count(), "MEDIC writes no job entry");
		ctx.Equal(1, CountLines(LEGACY_MEDIC), "the MEDIC legacy file stays on disk");

		LF_WhitelistManager.Reset();
		LF_WhitelistManager.Load();
		ctx.True(LF_WhitelistManager.IsWhitelistedForFaction(uidB, LF_EFactionType.POLICE), "a migrated uid survives a roster reload");
		ctx.False(LF_WhitelistManager.IsWhitelistedForFaction(uidA, LF_EFactionType.POLICE), "the admin removal survives a roster reload");

		Cleanup();
		LF_WhitelistManager.SetSavePath("");
		LF_WhitelistManager.Reset();
	}
}
