// red-proof: in LF_ConfigReader.ReadInt replace the `read > max` check with false and config/reject
// fails "an over-maximum default keeps the shipped value (expected 50, got 5000000)"; replace the
// `read < min` check and it fails "a negative price keeps the shipped value (expected 8, got -3)".
// Both observed 2026-11-12.

//------------------------------------------------------------------------------------------------
//! A numeric field outside the range its config attribute declares is refused on its own: the
//! lower layer's value stays, the rest of the section and the record still apply, and the reader
//! counts it.
// tier: LOGIC
class LF_Test_ConfigReject : LF_Test
{
	override string GetName()
	{
		return "config/reject";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_ShopPricesConfig config = LF_Test_ConfigMergeFixture.Merge("{\"shops\":{\"defaultBuyPrice\":5000000,\"defaultSellPrice\":30,\"priceRules\":{\"Prefabs/Items/Food/\":{\"buyPrice\":-3,\"sellPrice\":6}}}}");
		ctx.Equal(50, config.m_iDefaultBuyPrice, "an over-maximum default keeps the shipped value");
		ctx.Equal(30, config.m_iDefaultSellPrice, "the next field still applies");
		LF_ShopPriceRule food = LF_Test_ConfigMergeFixture.Find(config, LF_Test_ConfigMergeFixture.FOOD);
		ctx.NotNull(food, "the food rule is still there");
		if (food)
		{
			ctx.Equal(8, food.m_iBuyPrice, "a negative price keeps the shipped value");
			ctx.Equal(6, food.m_iSellPrice, "the rest of that record still applies");
		}

		// The limits themselves are valid.
		config = LF_Test_ConfigMergeFixture.Merge("{\"shops\":{\"defaultBuyPrice\":1000000,\"defaultSellPrice\":0}}");
		ctx.Equal(1000000, config.m_iDefaultBuyPrice, "the maximum is accepted");
		ctx.Equal(0, config.m_iDefaultSellPrice, "the minimum is accepted");

		// The reader counts each refusal and takes nothing from it.
		LF_ConfigService.Reset();
		LF_ConfigService.LoadOwnerString("{\"shops\":{\"defaultBuyPrice\":-1,\"defaultSellPrice\":1000001}}", "test");
		LF_ConfigReader reader = LF_ConfigService.OpenOwnerSection(LF_ShopPricesConfig.SECTION);
		ctx.NotNull(reader, "the section opens");
		if (reader)
		{
			int buy = 7;
			ctx.False(reader.ReadInt("defaultBuyPrice", buy, 0, 100), "below the minimum is refused");
			ctx.False(reader.ReadInt("defaultSellPrice", buy, 0, 100), "above the maximum is refused");
			ctx.Equal(7, buy, "a refused value never lands");
			ctx.Equal(2, reader.GetRejectedCount(), "each refusal is counted");
			ctx.Equal(0, reader.GetAppliedCount(), "nothing was applied");
		}

		LF_ConfigService.Reset();
	}
};
