// red-proof: change the `while (FileIO.FileExists(candidate))` in LF_ConfigMigrator.BackupPath to
// `while (false)` and config/migration-file fails "the existing backup is not overwritten (expected
// 'sentinel', got ...)"; point CopyFile at another destination and it fails "the backup holds the
// original text"; change `version >= target` to `version > target` in MigrateFile and it fails "a v1
// file at build v1 is not backed up". Observed 2026-11-12.

//------------------------------------------------------------------------------------------------
//! Migration at boot against a real file (#303). An older owner file is backed up first and the
//! backup keeps the original bytes, a backup that already exists is never overwritten, a failed
//! migration leaves the original and boots on shipped values, and a migrated file is not migrated
//! again. Uses a scratch folder under $profile, never the server's own config.json.
// tier: LOGIC
class LF_Test_ConfigMigrationFile : LF_Test
{
	static const string DIR = "$profile:LifeFramework/test-config-migrate";
	static const string OWNER = "$profile:LifeFramework/test-config-migrate/config.json";
	static const string EXAMPLE = "$profile:LifeFramework/test-config-migrate/config.example.json";
	static const string STAMP = "stamp";
	static const string BACKUP = "$profile:LifeFramework/test-config-migrate/config.json.bak-v0-stamp";
	static const string BACKUP2 = "$profile:LifeFramework/test-config-migrate/config.json.bak-v0-stamp-2";
	static const string V0 = "{\"schemaVersion\":0,\"shop\":{\"defaultBuyPrice\":61}}";

	override string GetName()
	{
		return "config/migration-file";
	}

	protected static void WriteText(string path, string text)
	{
		FileIO.MakeDirectory(DIR);
		FileHandle handle = FileIO.OpenFile(path, FileMode.WRITE);
		if (!handle)
			return;
		handle.Write(text);
		handle.Close();
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

	protected static void Clean()
	{
		FileIO.DeleteFile(OWNER);
		FileIO.DeleteFile(EXAMPLE);
		FileIO.DeleteFile(BACKUP);
		FileIO.DeleteFile(BACKUP2);
		FileIO.DeleteFile(OWNER + ".bak-v1-" + STAMP);
	}

	override void Run(LF_TestContext ctx)
	{
		Clean();
		LF_ConfigService.Reset();
		array<ref LF_ConfigMigration> steps = {};
		steps.Insert(new LF_Test_MigrationV0ToV1());

		// An older file: backed up with its original bytes, migrated, written, and applied.
		WriteText(OWNER, V0);
		ctx.True(LF_ConfigService.LoadOwnerFile(OWNER, EXAMPLE, steps, 1, STAMP), "a migratable file loads");
		ctx.True(FileIO.FileExists(BACKUP), "the backup is named for the old version");
		ctx.EqualStr(V0, ReadText(BACKUP), "the backup holds the original text");
		ctx.Equal(1, LF_ConfigMigrator.PeekVersion(ReadText(OWNER)), "the file on disk is now v1");
		ctx.True(ReadText(OWNER).Contains("\"shops\""), "the file on disk has the migrated shape");
		LF_ShopPricesConfig config = LF_Test_ConfigMergeFixture.MakeCore();
		config.ApplyLayers(string.Empty);
		ctx.Equal(61, config.m_iDefaultBuyPrice, "the migrated value applies at boot");

		// Idempotence: loading the migrated file again changes nothing and adds no backup.
		string after = ReadText(OWNER);
		LF_ConfigService.Reset();
		ctx.True(LF_ConfigService.LoadOwnerFile(OWNER, EXAMPLE, steps, 1, STAMP), "the migrated file loads");
		ctx.False(FileIO.FileExists(BACKUP2), "a migrated file is not backed up again");
		ctx.False(FileIO.FileExists(OWNER + ".bak-v1-" + STAMP), "a v1 file at build v1 is not backed up");
		ctx.EqualStr(V0, ReadText(BACKUP), "the first backup is intact");
		ctx.EqualStr(after, ReadText(OWNER), "a migrated file is not rewritten");

		// A backup that already exists is never overwritten: the next one gets a suffix.
		Clean();
		LF_ConfigService.Reset();
		WriteText(BACKUP, "sentinel");
		WriteText(OWNER, V0);
		ctx.True(LF_ConfigService.LoadOwnerFile(OWNER, EXAMPLE, steps, 1, STAMP), "a migration beside an existing backup loads");
		ctx.EqualStr("sentinel", ReadText(BACKUP), "the existing backup is not overwritten");
		ctx.EqualStr(V0, ReadText(BACKUP2), "the new backup takes the next name");

		// A failing migration keeps the original file and runs on shipped values.
		Clean();
		LF_ConfigService.Reset();
		string broken = "{\"schemaVersion\":0,\"cost\":{}}";	// the v0 to v1 step wants "shop"
		WriteText(OWNER, broken);
		ctx.False(LF_ConfigService.LoadOwnerFile(OWNER, EXAMPLE, steps, 1, STAMP), "a failed migration gives no owner layer");
		ctx.EqualStr(broken, ReadText(OWNER), "a failed migration leaves the original file");
		ctx.False(FileIO.FileExists(BACKUP), "a failed migration writes no backup");
		ctx.False(LF_ConfigService.HasOwnerLayer(), "no owner layer after a failed migration");
		LF_ShopPricesConfig shipped = LF_Test_ConfigMergeFixture.MakeCore();
		int shippedBuy = shipped.m_iDefaultBuyPrice;
		shipped.ApplyLayers(string.Empty);
		ctx.Equal(shippedBuy, shipped.m_iDefaultBuyPrice, "shops run on the shipped value");

		// An older file with no step to reach it also keeps the file and boots.
		LF_ConfigService.Reset();
		string v1 = "{\"schemaVersion\":1,\"shops\":{}}";
		WriteText(OWNER, v1);
		ctx.False(LF_ConfigService.LoadOwnerFile(OWNER, EXAMPLE, steps, 2, STAMP), "no step from v1 gives no owner layer");
		ctx.EqualStr(v1, ReadText(OWNER), "the file is unchanged");

		Clean();
		LF_ConfigService.Reset();
	}
};
