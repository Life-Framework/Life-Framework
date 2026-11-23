// red-proof: new test, no failing state on main (the sale never read a perk). `tools\cli test --no-build --red LF_Test_PerkPrice` (2026-11-23): RED-OK perks/price, 16 assertions, the hook skips both good_word picks so the miner share (expected 110, got 100) and the large-total check fail. Green with `--only`.
//! perks/price (WK-W11_2, #4409): the `good_word` perk adds its share to what a trader pays for that trade's goods.
//! A miner with the perk gets the share on top of the iron-ore price, a miner without it gets the plain price, a farmer
//! with a mining perk gets nothing for ore, the share stays inside the +15% cap and a perk holder is never paid less.
//! The same function (LF_PerkPrice.Adjust) prices a trader sale (LF_TraderSale.Sell and SellToBuyer) and the sell rows.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_PerkPrice : LF_Test
{
	static const string MINER_PERK = "test-perk-price-miner";
	static const string MINER_PLAIN = "test-perk-price-plain";
	static const string FARMER_PERK = "test-perk-price-farmer";

	protected static const ResourceName IRON_ORE = "{2C571D6F7C05E095}Prefabs/Resources/Mining/IronOre.et";
	protected static const ResourceName PLANK = "{45D1B36F99DD2FC3}Prefabs/Resources/WoodCutting/WoodPlank.et";
	protected static const ResourceName APPLE = "{C9D661E5B0714711}Prefabs/Items/Food/Apple.et";

	override string GetName()
	{
		return "perks/price";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_TradePerks.Forget(MINER_PERK);
		LF_TradePerks.Forget(MINER_PLAIN);
		LF_TradePerks.Forget(FARMER_PERK);

		if (!RedProof())
		{
			ctx.True(LF_TradePerks.Pick(MINER_PERK, "MINER", 10, LF_PerkPrice.PERK_GOOD_WORD), "the miner picks good word at 10");
			ctx.True(LF_TradePerks.Pick(FARMER_PERK, "FARMER", 10, LF_PerkPrice.PERK_GOOD_WORD), "the farmer picks good word at 10");
		}

		// The good names its trade by its folder.
		ctx.EqualStr("MINER", LF_PerkPrice.TradeOfGood(IRON_ORE), "iron ore is a mining good");
		ctx.EqualStr("LUMBERJACK", LF_PerkPrice.TradeOfGood(PLANK), "a plank is a logging good");
		ctx.EqualStr("", LF_PerkPrice.TradeOfGood(APPLE), "an apple names no work trade here");
		ctx.EqualStr("", LF_PerkPrice.TradeOfGood(""), "no good, no trade");

		// A sale of iron ore: 100 cash at the trader, the perk's share more with the perk, the same without.
		int plainTotal = 100;
		int perked = LF_PerkPrice.Adjust(MINER_PERK, IRON_ORE, plainTotal);
		ctx.Equal(plainTotal + LF_PerkPrice.GOOD_WORD_PERCENT, perked, "a miner with good word is paid the share more for iron ore");
		ctx.Equal(plainTotal, LF_PerkPrice.Adjust(MINER_PLAIN, IRON_ORE, plainTotal), "a miner without the perk is paid the plain price");
		ctx.Equal(plainTotal, LF_PerkPrice.Adjust(FARMER_PERK, IRON_ORE, plainTotal), "a farmer's good word does not lift an ore price");
		ctx.Equal(plainTotal, LF_PerkPrice.Adjust("", IRON_ORE, plainTotal), "no character, no perk");
		ctx.Equal(plainTotal, LF_PerkPrice.Adjust(MINER_PERK, APPLE, plainTotal), "a good outside every work trade is paid plain");
		ctx.Equal(0, LF_PerkPrice.Adjust(MINER_PERK, IRON_ORE, 0), "a zero price stays zero");
		ctx.Equal(1, LF_PerkPrice.Adjust(MINER_PERK, IRON_ORE, 1), "one cash rounds back to one, never under the plain price");
		ctx.Equal(1800000480 + 180000048, LF_PerkPrice.Adjust(MINER_PERK, IRON_ORE, 1800000480), "a large total keeps its int precision");

		// The share is one number inside the plan's cap and the displayed rows use the same adjustment.
		ctx.True(LF_PerkPrice.GOOD_WORD_PERCENT <= 15, "the buyer price share stays inside the +15% cap");
		ctx.Equal(perked, LF_PerkPrice.Adjust(MINER_PERK, IRON_ORE, plainTotal), "the same sale prices the same twice");

		LF_TradePerks.Forget(MINER_PERK);
		LF_TradePerks.Forget(MINER_PLAIN);
		LF_TradePerks.Forget(FARMER_PERK);
	}
}
