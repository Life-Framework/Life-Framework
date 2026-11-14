// red-proof: add a FileIO.OpenFile("$profile:.save/lf-red-proof.json", FileMode.WRITE) write to
// LF_ConfigMigrator.MigrateFile (after the backup) and config/migration-no-persist fails "the engine
// save folder is unchanged by a migration (expected '$profile:.save/settings;...', got
// '$profile:.save/lf-red-proof.json;$profile:.save/settings;...')". Observed 2026-11-14.

//------------------------------------------------------------------------------------------------
//! Test-only identity step v1 to v2: the document passes through unchanged. Main ships schema 1
//! and an empty LF_ConfigMigrator.BuildShipped, so this is the only way to run a migration with
//! a v1 file today; LoadOwnerFile takes the steps and the target as arguments.
class LF_Test_MigrationIdentityV1ToV2 : LF_ConfigMigration
{
	override int FromVersion()
	{
		return 1;
	}

	override string GetName()
	{
		return "test identity v1 to v2";
	}

	override string Apply(string json)
	{
		return json;
	}
};

//------------------------------------------------------------------------------------------------
//! Lists every file and folder under a profile folder. FileIO.FindFiles is already recursive and
//! returns full "$profile:..." paths, so one call is the whole tree.
class LF_Test_FolderListing
{
	protected ref array<string> m_aEntries = {};
	protected string m_sSkip;

	//! FindFiles callback: records one entry.
	void Add(string fileName, FileAttribute attributes = 0, string filesystem = string.Empty)
	{
		if (!m_sSkip.IsEmpty() && fileName.Contains(m_sSkip))
			return;
		m_aEntries.Insert(fileName);
	}

	//! \param[in] root The folder, e.g. "$profile:.save".
	//! \param[in] skip Entries whose path contains this are left out (the test's own scratch folder); may be empty.
	//! \return The sorted paths, empty when the folder does not exist.
	static array<string> Snapshot(string root, string skip = string.Empty)
	{
		LF_Test_FolderListing listing = new LF_Test_FolderListing();
		listing.m_sSkip = skip;
		FileIO.FindFiles(listing.Add, root, string.Empty);
		array<string> result = {};
		result.Copy(listing.m_aEntries);
		result.Sort();
		return result;
	}

	//! \return The entries joined with ";" for an assertion message.
	static string Describe(array<string> entries)
	{
		string text;
		foreach (string entry : entries)
		{
			text += entry + ";";
		}
		return text;
	}
};

//------------------------------------------------------------------------------------------------
//! A config migration opens no persistence (save) file (#303). Runs a real v1 to v2 migration
//! through LoadOwnerFile with a test-only identity step, and lists the engine's save folder
//! ($profile:.save) and the LifeFramework profile folder (outside the test's own scratch folder)
//! before and after: nothing may appear or vanish there. Inside the scratch folder the migration
//! may leave only config.json and its .bak. Uses a scratch folder under $profile, never the
//! server's own config.json.
// tier: LOGIC
class LF_Test_ConfigMigrationNoPersist : LF_Test
{
	static const string SCRATCH = "test-config-migrate-nopersist";
	static const string DIR = "$profile:LifeFramework/test-config-migrate-nopersist";
	static const string OWNER = "$profile:LifeFramework/test-config-migrate-nopersist/config.json";
	static const string EXAMPLE = "$profile:LifeFramework/test-config-migrate-nopersist/config.example.json";
	static const string BACKUP = "$profile:LifeFramework/test-config-migrate-nopersist/config.json.bak-v1-stamp";
	static const string STAMP = "stamp";
	static const string V1 = "{\"schemaVersion\":1,\"shops\":{\"defaultBuyPrice\":61}}";

	override string GetName()
	{
		return "config/migration-no-persist";
	}

	protected static void Clean()
	{
		FileIO.DeleteFile(OWNER);
		FileIO.DeleteFile(EXAMPLE);
		FileIO.DeleteFile(BACKUP);
		FileIO.DeleteFile(DIR);
	}

	protected static string ReadText(string path)
	{
		FileHandle handle = FileIO.OpenFile(path, FileMode.READ);
		if (!handle)
			return string.Empty;
		string text;
		string line;
		while (handle.ReadLine(line) >= 0)
		{
			text += line;
		}
		handle.Close();
		return text;
	}

	override void Run(LF_TestContext ctx)
	{
		Clean();
		LF_ConfigService.Reset();
		FileIO.MakeDirectory(DIR);
		FileHandle handle = FileIO.OpenFile(OWNER, FileMode.WRITE);
		if (!handle)
		{
			ctx.True(false, "the scratch config.json can be written");
			return;
		}
		handle.Write(V1);
		handle.Close();

		array<ref LF_ConfigMigration> steps = {};
		steps.Insert(new LF_Test_MigrationIdentityV1ToV2());

		array<string> saveBefore = LF_Test_FolderListing.Snapshot("$profile:.save");
		array<string> frameworkBefore = LF_Test_FolderListing.Snapshot("$profile:LifeFramework", SCRATCH);

		ctx.True(LF_ConfigService.LoadOwnerFile(OWNER, EXAMPLE, steps, 2, STAMP), "the v1 file migrates to v2 and loads");

		array<string> saveAfter = LF_Test_FolderListing.Snapshot("$profile:.save");
		array<string> frameworkAfter = LF_Test_FolderListing.Snapshot("$profile:LifeFramework", SCRATCH);
		array<string> scratch = LF_Test_FolderListing.Snapshot(DIR);

		// The migration really ran: the file is v2 and a backup holds the v1 original.
		ctx.Equal(2, LF_ConfigMigrator.PeekVersion(ReadText(OWNER)), "the file on disk is now v2");
		ctx.EqualStr(V1, ReadText(BACKUP), "the backup holds the v1 original");

		// Only config.json and its .bak exist in the scratch folder afterwards.
		ctx.Equal(2, scratch.Count(), "the migration leaves exactly config.json and its backup: " + LF_Test_FolderListing.Describe(scratch));
		ctx.True(scratch.Find(OWNER) >= 0, "config.json is in the scratch folder");
		ctx.True(scratch.Find(BACKUP) >= 0, "the backup is in the scratch folder");

		// No persistence file was opened or written: the save folder and the rest of the framework folder are the same.
		ctx.EqualStr(LF_Test_FolderListing.Describe(saveBefore), LF_Test_FolderListing.Describe(saveAfter), "the engine save folder is unchanged by a migration");
		ctx.EqualStr(LF_Test_FolderListing.Describe(frameworkBefore), LF_Test_FolderListing.Describe(frameworkAfter), "the LifeFramework profile folder is unchanged by a migration");

		Clean();
		LF_ConfigService.Reset();
	}
};
