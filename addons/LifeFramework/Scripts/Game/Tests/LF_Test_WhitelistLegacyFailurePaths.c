// red-proof: 2026-11-17 `tools/cli test --no-build --red LF_Test_WhitelistLegacyFailurePaths,LF_Test_WhitelistLegacyIgnoredCount`: RED-OK whitelist/legacy-failure-paths (fails with the hook on, 18 assertions); green with --only.
// tier: WORLD
//! whitelist/legacy-failure-paths (#2607): the two refusals of the legacy migration lose nothing.
//! A roster this build cannot read stops the migration before it adds anything, and a roster save
//! that fails leaves the file unmarked. Each is followed by the next start (a fresh load of a
//! readable roster), which must migrate every uid and write the marker.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_WhitelistLegacyFailurePaths : LF_Test
{
	protected static const string ROSTER = "$profile:TestResults/whitelist-fail-roster.txt";
	protected static const string ROSTER_DIR = "$profile:TestResults/whitelist-fail-roster-dir.txt";
	protected static const string LEGACY = "$profile:TestResults/whitelist-fail-police.txt";
	protected static const string MARKER = "$profile:TestResults/whitelist-fail-marker.txt";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "whitelist/legacy-failure-paths";
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
	protected string FirstLine(string path)
	{
		string line = "";
		FileHandle file = FileIO.OpenFile(path, FileMode.READ);
		if (!file)
			return line;

		file.ReadLine(line);
		file.Close();
		line.TrimInPlace();
		return line;
	}

	//------------------------------------------------------------------------------------------------
	protected void Cleanup()
	{
		FileIO.DeleteFile(ROSTER);
		FileIO.DeleteFile(LEGACY);
		FileIO.DeleteFile(MARKER);
		FileIO.DeleteFile(ROSTER_DIR);
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		string uidA = "legacy-fail-a";
		string uidB = "legacy-fail-b";

		Cleanup();
		array<string> legacyIn = {uidA, uidB};
		WriteLines(LEGACY, legacyIn);

		// 1) Unreadable roster (a future version): nothing is added, nothing is marked, the file is kept.
		array<string> futureRoster = {"99", "1", "legacy-fail-existing", "POLICE", "0"};
		WriteLines(ROSTER, futureRoster);
		LF_WhitelistManager.SetSavePath(ROSTER);
		LF_WhitelistManager.Reset();
		LF_WhitelistManager.Load();
		ctx.True(LF_WhitelistManager.IsLoadFailed(), "the fixture roster is unreadable");

		int migrated = 0;
		if (!RedProof())
			migrated = LF_WhitelistLegacyMigration.Migrate(LF_WhitelistType.POLICE, LEGACY, MARKER);

		ctx.Equal(0, migrated, "an unreadable roster migrates nothing");
		ctx.Equal(0, LF_WhitelistManager.CountFaction(LF_EFactionType.POLICE), "an unreadable roster gets no police uid added");
		ctx.False(LF_WhitelistManager.IsWhitelistedForFaction(uidA, LF_EFactionType.POLICE), "uid A is not on the unreadable roster");
		ctx.False(FileIO.FileExists(MARKER), "an unreadable roster writes no marker");
		ctx.EqualStr("99", FirstLine(ROSTER), "the unreadable roster file is left as it was");

		// The owner fixes the roster and restarts: the same legacy file now migrates in full.
		FileIO.DeleteFile(ROSTER);
		LF_WhitelistManager.Reset();
		LF_WhitelistManager.Load();
		ctx.False(LF_WhitelistManager.IsLoadFailed(), "the repaired roster loads");
		ctx.Equal(2, LF_WhitelistLegacyMigration.Migrate(LF_WhitelistType.POLICE, LEGACY, MARKER), "the next start migrates both uids");
		ctx.True(FileIO.FileExists(MARKER), "the next start writes the marker");
		ctx.True(LF_WhitelistManager.IsWhitelistedForFaction(uidA, LF_EFactionType.POLICE), "uid A is on the roster after the retry");
		ctx.True(LF_WhitelistManager.IsWhitelistedForFaction(uidB, LF_EFactionType.POLICE), "uid B is on the roster after the retry");

		// 2) Save failed: the roster loaded fine, then its save path turns out to be a directory. No marker; the next start retries.
		FileIO.DeleteFile(ROSTER);
		FileIO.DeleteFile(MARKER);
		FileIO.MakeDirectory(ROSTER_DIR);
		LF_WhitelistManager.SetSavePath(ROSTER);
		LF_WhitelistManager.Reset();
		LF_WhitelistManager.Load();
		ctx.False(LF_WhitelistManager.IsLoadFailed(), "a missing roster is a first run, not a failed load");
		LF_WhitelistManager.SetSavePath(ROSTER_DIR);

		int attempted = 0;
		if (!RedProof())
			attempted = LF_WhitelistLegacyMigration.Migrate(LF_WhitelistType.POLICE, LEGACY, MARKER);

		ctx.Equal(2, attempted, "a failed save still reports the uids it tried to add");
		ctx.False(FileIO.FileExists(MARKER), "a failed roster save writes no marker");

		// Next start: the roster path works and the roster loads from disk (the failed run saved nothing).
		FileIO.DeleteFile(ROSTER_DIR);
		LF_WhitelistManager.SetSavePath(ROSTER);
		LF_WhitelistManager.Reset();
		LF_WhitelistManager.Load();
		ctx.Equal(0, LF_WhitelistManager.CountFaction(LF_EFactionType.POLICE), "the failed run left nothing on the roster");
		ctx.Equal(2, LF_WhitelistLegacyMigration.Migrate(LF_WhitelistType.POLICE, LEGACY, MARKER), "the start after a failed save migrates both uids");
		ctx.True(FileIO.FileExists(MARKER), "the start after a failed save writes the marker");

		Cleanup();
		LF_WhitelistManager.SetSavePath("");
		LF_WhitelistManager.Reset();
	}
}
