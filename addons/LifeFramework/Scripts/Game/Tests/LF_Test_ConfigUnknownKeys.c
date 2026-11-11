// red-proof: replace the `known.Contains(key)` test in LF_ConfigReader.WarnUnknownKeys with false and
// config/unknown-keys fails "the typo in the section and the extra key in the record (expected 2,
// got 5)"; make LF_ConfigService.IsKnownTopLevelKey stop accepting schemaVersion and it fails
// "schemaVersion and shops are known top-level keys (expected 0, got 1)". Observed 2026-11-12.

//------------------------------------------------------------------------------------------------
//! An unknown key warns and is ignored: in a section, in a record, and at the top level. The
//! known keys around it still apply.
// tier: LOGIC
class LF_Test_ConfigUnknownKeys : LF_Test
{
	override string GetName()
	{
		return "config/unknown-keys";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_ConfigService.Reset();
		LF_ConfigService.LoadOwnerString("{\"schemaVersion\":1,\"shops\":{\"defaultBuyPrice\":61,\"defaultBuyPrise\":9,\"priceRules\":{\"Prefabs/Items/Food/\":{\"buyPrice\":12,\"discount\":3}}}}", "test");
		LF_ConfigReader reader = LF_ConfigService.OpenOwnerSection(LF_ShopPricesConfig.SECTION);
		ctx.NotNull(reader, "the section opens");
		if (!reader)
			return;

		LF_ShopPricesConfig config = LF_Test_ConfigMergeFixture.MakeCore();
		config.ApplyOwner(reader);
		ctx.Equal(2, reader.GetUnknownCount(), "the typo in the section and the extra key in the record");
		ctx.Equal(0, reader.GetRejectedCount(), "an unknown key is a warning, not a rejection");
		ctx.Equal(61, config.m_iDefaultBuyPrice, "the known key beside the typo applies");
		LF_ShopPriceRule food = LF_Test_ConfigMergeFixture.Find(config, LF_Test_ConfigMergeFixture.FOOD);
		ctx.True(food && food.m_iBuyPrice == 12, "the known key beside the extra record key applies");

		// Only unknown keys warn, listed in file order.
		LF_ConfigService.LoadOwnerString("{\"shops\":{\"defaultBuyPrice\":61,\"a\":1,\"b\":2,\"defaultSellPrice\":3,\"c\":{}}}", "test");
		reader = LF_ConfigService.OpenOwnerSection(LF_ShopPricesConfig.SECTION);
		if (reader)
		{
			array<string> known = {"defaultBuyPrice", "defaultSellPrice"};
			ctx.Equal(3, reader.WarnUnknownKeys(known), "only the typo in the section is unknown");
		}

		// Top level.
		ctx.Equal(2, LF_ConfigService.WarnUnknownSections("{\"schemaVersion\":1,\"shop\":{},\"shops\":{},\"jobz\":[1]}", "test"), "a mistyped section and a stray key are unknown at the top");
		ctx.Equal(0, LF_ConfigService.WarnUnknownSections("{\"schemaVersion\":1,\"shops\":{}}", "test"), "schemaVersion and shops are known top-level keys");
		ctx.Equal(0, LF_ConfigService.WarnUnknownSections("[1,2]", "test"), "a document that is not an object lists nothing and does not crash");
		ctx.Equal(0, LF_ConfigService.WarnUnknownSections("5", "test"), "a document that is a number lists nothing and does not crash");
		ctx.Equal(0, LF_ConfigService.WarnUnknownSections("\"x\"", "test"), "a document that is a string lists nothing");

		LF_ConfigService.Reset();
	}
};
