// red-proof: drop the `next = StampVersion(next, version + 1)` line in LF_ConfigMigrator.RunChain and
// config/migration-chain fails "the migrated document is stamped v1 (expected 1, got -1)"; make the
// missing-step branch `version++; continue;` and it fails "a failed chain leaves the output
// unchanged". Observed 2026-11-12.

//------------------------------------------------------------------------------------------------
//! Test-only migration steps. V0ToV1 is the framework's proof fixture: a v0 file called the
//! section "shop", v1 calls it "shops". V1ToV2 and V2ToV3 exist to prove ordering: each refuses a
//! document the step before it did not leave in its shape, so a chain run out of order fails.
class LF_Test_MigrationV0ToV1 : LF_ConfigMigration
{
	override int FromVersion()
	{
		return 0;
	}

	override string GetName()
	{
		return "test v0 to v1";
	}

	override string Apply(string json)
	{
		if (!json.Contains("\"shop\""))
			return string.Empty;
		string result = json;
		result.Replace("\"shop\"", "\"shops\"");
		return result;
	}
};

class LF_Test_MigrationV1ToV2 : LF_ConfigMigration
{
	override int FromVersion()
	{
		return 1;
	}

	override string GetName()
	{
		return "test v1 to v2";
	}

	override string Apply(string json)
	{
		if (!json.Contains("\"shops\"") || !json.Contains("\"cost\""))
			return string.Empty;
		string result = json;
		result.Replace("\"cost\"", "\"defaultBuyPrice\"");
		return result;
	}
};

class LF_Test_MigrationV2ToV3 : LF_ConfigMigration
{
	override int FromVersion()
	{
		return 2;
	}

	override string GetName()
	{
		return "test v2 to v3";
	}

	override string Apply(string json)
	{
		if (!json.Contains("\"defaultBuyPrice\""))
			return string.Empty;
		string result = json;
		result.Replace("\"defaultBuyPrice\"", "\"defaultSellPrice\"");
		return result;
	}
};

//! A step that fails, as a real one does on a document it cannot read.
class LF_Test_MigrationFails : LF_ConfigMigration
{
	override int FromVersion()
	{
		return 1;
	}

	override string GetName()
	{
		return "test failing step";
	}

	override string Apply(string json)
	{
		return string.Empty;
	}
};

//------------------------------------------------------------------------------------------------
//! The in-memory migration chain (#303): steps run in version order with the version stamped
//! after each, a missing or failing step stops the chain, and the v0 to v1 fixture produces a
//! document the shops section reads.
// tier: LOGIC
class LF_Test_ConfigMigrationChain : LF_Test
{
	override string GetName()
	{
		return "config/migration-chain";
	}

	static array<ref LF_ConfigMigration> Steps()
	{
		array<ref LF_ConfigMigration> steps = {};
		steps.Insert(new LF_Test_MigrationV2ToV3());	// out of order on purpose
		steps.Insert(new LF_Test_MigrationV0ToV1());
		steps.Insert(new LF_Test_MigrationV1ToV2());
		return steps;
	}

	override void Run(LF_TestContext ctx)
	{
		ctx.Equal(0, LF_ConfigMigrator.PeekVersion("{\"schemaVersion\":0}"), "PeekVersion reads 0");
		ctx.Equal(-1, LF_ConfigMigrator.PeekVersion("{\"shops\":{}}"), "no schemaVersion peeks -1");
		ctx.Equal(-1, LF_ConfigMigrator.PeekVersion("{\"schemaVersion\":\"x\"}"), "a string schemaVersion peeks -1");
		ctx.Equal(-1, LF_ConfigMigrator.PeekVersion("not json"), "invalid JSON peeks -1");

		ctx.Equal(7, LF_ConfigMigrator.PeekVersion(LF_ConfigMigrator.StampVersion("{ \"schemaVersion\" :  2 ,\"a\":1}", 7)), "StampVersion replaces the number through whitespace");
		ctx.Equal(4, LF_ConfigMigrator.PeekVersion(LF_ConfigMigrator.StampVersion("{\"a\":1}", 4)), "StampVersion adds a missing key");
		ctx.Equal(4, LF_ConfigMigrator.PeekVersion(LF_ConfigMigrator.StampVersion("{}", 4)), "StampVersion adds a key to an empty object");
		ctx.True(LF_ConfigMigrator.StampVersion("nope", 4).IsEmpty(), "StampVersion refuses a document that is not an object");

		// The v0 to v1 fixture: the shops section reads after it.
		string migrated;
		array<ref LF_ConfigMigration> steps = Steps();
		ctx.True(LF_ConfigMigrator.RunChain("{\"schemaVersion\":0,\"shop\":{\"defaultBuyPrice\":61}}", steps, 1, migrated), "v0 to v1 runs");
		ctx.Equal(1, LF_ConfigMigrator.PeekVersion(migrated), "the migrated document is stamped v1");
		LF_ShopPricesConfig config = LF_Test_ConfigMergeFixture.Merge(migrated);
		ctx.Equal(61, config.m_iDefaultBuyPrice, "the migrated shops section applies");

		// Ordered multi-step: v0 to v3 runs 0->1, 1->2, 2->3 whatever order the list holds.
		string full;
		ctx.True(LF_ConfigMigrator.RunChain("{\"schemaVersion\":0,\"shop\":{\"cost\":61}}", steps, 3, full), "v0 to v3 runs in order");
		ctx.Equal(3, LF_ConfigMigrator.PeekVersion(full), "the chain ends stamped v3");
		ctx.True(full.Contains("\"defaultSellPrice\":61") && full.Contains("\"shops\""), "every step's change is in the result");

		// A gap stops the chain and leaves the output alone.
		string kept = "untouched";
		array<ref LF_ConfigMigration> gap = {};
		gap.Insert(new LF_Test_MigrationV0ToV1());
		ctx.False(LF_ConfigMigrator.RunChain("{\"schemaVersion\":0,\"shop\":{}}", gap, 3, kept), "a missing step v1 to v2 fails the chain");
		ctx.EqualStr("untouched", kept, "a failed chain leaves the output unchanged");

		// A failing step stops the chain.
		array<ref LF_ConfigMigration> failing = {};
		failing.Insert(new LF_Test_MigrationFails());
		ctx.False(LF_ConfigMigrator.RunChain("{\"schemaVersion\":1}", failing, 2, kept), "a failing step fails the chain");

		// A step that does not recognise the document fails rather than passing it on.
		ctx.False(LF_ConfigMigrator.RunChain("{\"schemaVersion\":1,\"other\":{}}", steps, 2, kept), "a step that cannot read the document fails");
	}
};
