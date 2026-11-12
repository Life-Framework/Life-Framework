// red-proof: replace the `fileVersion > buildVersion` test in LF_ConfigService.ClassifySchema with
// false and config/schema-version fails "a newer file is NEWER (expected 3, got 1)"; replace the
// `schemaVersion < 1` branch in LoadOwnerString and it fails "version 0 is invalid (expected 4,
// got 2)". Observed 2026-11-12.

//------------------------------------------------------------------------------------------------
//! The owner file's schemaVersion (#302): missing reads as 1, a newer file logs an error and still
//! applies the fields this build knows, an older file reads as-is, and a bad value reads as 1.
//! No migrations; those are #303.
// tier: LOGIC
class LF_Test_ConfigSchemaVersion : LF_Test
{
	override string GetName()
	{
		return "config/schema-version";
	}

	override void Run(LF_TestContext ctx)
	{
		ctx.Equal(LF_EConfigSchemaStatus.CURRENT, LF_ConfigService.ClassifySchema(1, 1), "the same version is current");
		ctx.Equal(LF_EConfigSchemaStatus.OLDER, LF_ConfigService.ClassifySchema(1, 3), "an older file is classified OLDER");
		ctx.Equal(LF_EConfigSchemaStatus.NEWER, LF_ConfigService.ClassifySchema(5, 3), "a newer file is classified NEWER");

		ctx.Equal(LF_EConfigSchemaStatus.NONE, LF_ConfigService.GetSchemaStatus(), "no owner file, no status");

		LF_ConfigService.Reset();
		LF_ConfigService.LoadOwnerString("{\"shops\":{}}", "test");
		ctx.Equal(LF_EConfigSchemaStatus.CURRENT, LF_ConfigService.GetSchemaStatus(), "a missing schemaVersion is version 1, current");
		ctx.Equal(1, LF_ConfigService.GetOwnerSchemaVersion(), "a missing schemaVersion reads as 1");

		LF_ConfigService.LoadOwnerString("{\"schemaVersion\":1,\"shops\":{}}", "test");
		ctx.Equal(LF_EConfigSchemaStatus.CURRENT, LF_ConfigService.GetSchemaStatus(), "schemaVersion 1 is current");

		// A newer file: an error is logged (LF_Debug.Error) and the fields this build knows apply.
		LF_ShopPricesConfig config = LF_Test_ConfigMergeFixture.Merge("{\"schemaVersion\":9,\"shops\":{\"defaultBuyPrice\":61,\"newThing\":1}}");
		ctx.Equal(LF_EConfigSchemaStatus.NEWER, LF_ConfigService.GetSchemaStatus(), "a newer file is NEWER");
		ctx.Equal(9, LF_ConfigService.GetOwnerSchemaVersion(), "the file's version is kept");
		ctx.Equal(61, config.m_iDefaultBuyPrice, "a newer file still applies the fields this build knows");

		// An older file: read as-is. The build's version is passed because this build reads 1.
		LF_ConfigService.Reset();
		LF_ConfigService.LoadOwnerString("{\"schemaVersion\":1,\"shops\":{}}", "test", 3);
		ctx.Equal(LF_EConfigSchemaStatus.OLDER, LF_ConfigService.GetSchemaStatus(), "an older file is OLDER");
		ctx.True(LF_ConfigService.HasOwnerLayer(), "an older file is still an owner layer");

		// A bad value reads as version 1 and says so.
		LF_ConfigService.LoadOwnerString("{\"schemaVersion\":0,\"shops\":{}}", "test");
		ctx.Equal(LF_EConfigSchemaStatus.INVALID, LF_ConfigService.GetSchemaStatus(), "version 0 is invalid");
		ctx.Equal(1, LF_ConfigService.GetOwnerSchemaVersion(), "version 0 reads as 1");
		LF_ConfigService.LoadOwnerString("{\"schemaVersion\":\"2\",\"shops\":{}}", "test");
		ctx.Equal(LF_EConfigSchemaStatus.INVALID, LF_ConfigService.GetSchemaStatus(), "a string is invalid");
		LF_ConfigService.LoadOwnerString("{\"schemaVersion\":{\"a\":1},\"shops\":{}}", "test");
		ctx.Equal(LF_EConfigSchemaStatus.INVALID, LF_ConfigService.GetSchemaStatus(), "an object is invalid");
		LF_ConfigService.LoadOwnerString("{\"schemaVersion\":-4,\"shops\":{}}", "test");
		ctx.Equal(LF_EConfigSchemaStatus.INVALID, LF_ConfigService.GetSchemaStatus(), "a negative version is invalid");

		LF_ConfigService.Reset();
	}
};
