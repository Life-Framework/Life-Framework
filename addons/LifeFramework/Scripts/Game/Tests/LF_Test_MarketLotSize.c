// red-proof: observed 2026-11-19 `tools/cli test --no-build --red LF_Test_MarketLotSize`: RED PROOF OK (the one listed test fails with the hook on); RedProof() makes every lot size 1, so the part-lot, remainder, seconds-to-lot and curve-lot cases fail. Green with `--only`: passed.
//! A price per lot (#3622): a market curve row and a premium buyer row may carry `m_iLotSize` units, so a
//! good counted in kg can fetch a whole-cash price per 200 kg or per 5 kg. A sale pays whole lots only
//! (units / lot, rounded down) and the rest stays with the seller; the curve falls per lot sold; a stack
//! below one lot books nothing; a buyer's purse and refill stay in units. Lot size 1 changes nothing.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_MarketLotSize : LF_Test
{
	static const ResourceName GRAIN = "{577A665F1662E7AD}Prefabs/Resources/Mining/CopperBar.et";
	static const ResourceName PLAIN = "{BCD1E8CD7F975074}Prefabs/Resources/Mining/IronBar.et";
	static const ResourceName FLOUR = "{ACC9A10F0F20F5C4}Prefabs/Resources/Mining/GoldBar.et";
	static const string BUYER = "test-bakery";
	static const int T0 = 1800000480;	// lf-fixed: a unix time past 2^24, where float maths loses seconds

	override string GetName()
	{
		return "market/lot-size";
	}

	//------------------------------------------------------------------------------------------------
	//! The lot size the cases use, or 1 under the red-proof hook (`--red`).
	protected int Lot(int lot)
	{
		if (RedProof())
			return 1;

		return lot;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		RunCurveRow(ctx);
		RunBoard(ctx);
		RunBook(ctx);
		RunBuyerSale(ctx);
		RunBuyerPurse(ctx);
		RunBuyerProblems(ctx);
		RunCurveMatch(ctx);
		LF_MarketConfig.Reload();
		LF_MarketBook.ResetStatics();
	}

	//------------------------------------------------------------------------------------------------
	//! Grain: $9 a 200 kg lot (base and ceiling), floor $5, 4 percent a lot off the price, one plain good beside it.
	protected LF_MarketConfig MakeConfig()
	{
		LF_MarketConfig config = new LF_MarketConfig();
		config.m_bEnabled = true;
		config.m_iMaxRecoveryHours = 24;
		config.m_aGoods = {};
		config.m_aGoods.Insert(LF_MarketGoodDef.Make(GRAIN, 9, 5, 9, 4, 26, Lot(200)));
		config.m_aGoods.Insert(LF_MarketGoodDef.Make(PLAIN, 10, 5, 10, 0, 0));
		return config;
	}

	//------------------------------------------------------------------------------------------------
	protected void RunCurveRow(LF_TestContext ctx)
	{
		LF_MarketConfig config = MakeConfig();
		LF_MarketGoodDef grain = config.FindGood(GRAIN);
		ctx.EqualStr("", grain.Problem(), "the grain row is valid");
		ctx.Equal(Lot(200), grain.Lot(), "the lot is 200 kg");
		ctx.Equal(2, grain.Lots(450), "450 kg is two whole lots");
		ctx.Equal(0, grain.Lots(199), "199 kg fills no lot");
		ctx.Equal(0, grain.SellableUnits(199), "199 kg sells nothing");
		ctx.Equal(400, grain.SellableUnits(450), "450 kg sells 400 and keeps 50");
		ctx.Equal(1, config.FindGood(PLAIN).Lot(), "a row that sets no lot is priced per unit");
		ctx.Equal(7, config.FindGood(PLAIN).SellableUnits(7), "so every unit sells");

		LF_MarketGoodDef zero = LF_MarketGoodDef.Make(GRAIN, 9, 5, 9, 4, 26, 0);
		ctx.EqualStr("", zero.Problem(), "an unset lot counts as 1");
		ctx.Equal(1, zero.Lot(), "an unset lot reads as 1");
		LF_MarketGoodDef negative = LF_MarketGoodDef.Make(GRAIN, 9, 5, 9, 4, 26, -5);
		ctx.True(!negative.Problem().IsEmpty(), "a negative lot is refused");
		LF_MarketGoodDef huge = LF_MarketGoodDef.Make(GRAIN, 9, 5, 9, 4, 26, LF_PriceCurve.MAX_BULK_UNITS + 1);
		ctx.True(!huge.Problem().IsEmpty(), "a lot above the largest bulk is refused");
	}

	//------------------------------------------------------------------------------------------------
	//! The book quotes and books per lot: two lots of 9 then 8.64 pay 17.64, which is 18; the price ends at 8.28.
	protected void RunBook(LF_TestContext ctx)
	{
		LF_MarketBook.ResetStatics();
		LF_MarketConfig.UseForTest(MakeConfig());

		ctx.Equal(0, LF_MarketBook.QuoteAt(GRAIN, 199, T0), "199 kg quotes nothing");
		ctx.Equal(18, LF_MarketBook.QuoteAt(GRAIN, 450, T0), "450 kg quotes two lots: 9 + 8.64 rounds to 18");
		ctx.Equal(400, LF_MarketBook.SellableUnits(GRAIN, 450), "the book takes 400 of 450 kg");
		ctx.Equal(450, LF_MarketBook.SellableUnits(PLAIN, 450), "a good with no lot sells every unit");
		ctx.Equal(Lot(200), LF_MarketBook.LotOf(GRAIN), "the book reports the lot");

		int oldPrice;
		ctx.Equal(0, LF_MarketBook.ApplySaleAt(GRAIN, 199, T0, oldPrice), "a part lot moves no price");
		ctx.True(LF_MarketBook.Snapshot(GRAIN) == null, "and books nothing");

		int newPrice = LF_MarketBook.ApplySaleAt(GRAIN, 450, T0, oldPrice);
		ctx.Equal(9, oldPrice, "the price stood at 9 a lot");
		ctx.Equal(8, newPrice, "two lots take 0.72 off: 8.28 shows as 8");
		ctx.Equal(828, LF_MarketBook.CurrentFineAt(GRAIN, T0), "the book keeps 8.28 in hundredths");
		ctx.Equal(8, LF_MarketBook.CurrentPriceAt(GRAIN, T0), "the board price is per lot");
	}

	//------------------------------------------------------------------------------------------------
	//! The price board names the lot of a good priced per lot, so a price of 9 is not read as 9 a kg.
	protected void RunBoard(LF_TestContext ctx)
	{
		LF_MarketBook.ResetStatics();
		LF_MarketConfig.UseForTest(MakeConfig());
		LF_BoardAnswer answer = LF_PriceBoard.BuildAnswerAt(T0);
		ctx.Equal(2, answer.m_aTexts.Count(), "the board lists both goods");
		if (answer.m_aTexts.Count() < 2)
			return;

		ctx.True(answer.m_aTexts[0].EndsWith(" x200"), "the grain line carries its lot: " + answer.m_aTexts[0]);
		ctx.True(!answer.m_aTexts[1].Contains(" x"), "a good with no lot shows no suffix: " + answer.m_aTexts[1]);
	}

	//------------------------------------------------------------------------------------------------
	protected LF_BuyerDemandRow FlourRow(int cap)
	{
		return LF_BuyerDemandRow.Make(BUYER, FLOUR, cap, 500, 1, "", Lot(5));
	}

	//------------------------------------------------------------------------------------------------
	//! Flour: $1 a 5 kg lot, a purse of 1000 kg refilling 500 kg an hour. 52 kg sells 50 and keeps 2.
	protected void RunBuyerSale(LF_TestContext ctx)
	{
		LF_BuyerDemandRow row = FlourRow(1000);
		ctx.EqualStr("", row.Problem(), "the flour row is valid");
		LF_BuyerPurse purse = LF_BuyerPurse.Full(row, T0);

		LF_BuyerDemandSale sale = LF_BuyerDemand.Sell(row, purse, 52, T0);
		ctx.Equal(50, sale.m_iAccepted, "52 kg sells 50 kg, ten whole lots");
		ctx.Equal(2, sale.m_iRefused, "2 kg stay with the seller");
		ctx.Equal(2, sale.m_iUnderLot, "and they are the part lot, not a full purse");
		ctx.Equal(10, sale.m_iPayout, "ten lots at $1 pay 10");
		ctx.Equal(950, purse.m_iRemaining, "the purse counts kg");

		LF_BuyerDemandSale small = LF_BuyerDemand.Sell(row, purse, 4, T0);
		ctx.Equal(0, small.m_iAccepted, "4 kg is below one lot");
		ctx.Equal(4, small.m_iUnderLot, "all of it is the part lot");
		ctx.Equal(0, small.m_iPayout, "and pays nothing");
		ctx.Equal(950, purse.m_iRemaining, "the purse is untouched");

		LF_BuyerDemandSale big = LF_BuyerDemand.Sell(row, purse, 3000, T0);
		ctx.Equal(950, big.m_iAccepted, "a big stack takes what the purse holds");
		ctx.Equal(2050, big.m_iRefused, "and refuses the rest");
		ctx.Equal(0, big.m_iUnderLot, "none of it is a part lot");
		ctx.Equal(190, big.m_iPayout, "190 lots at $1 pay 190");
		ctx.Equal(0, purse.m_iRemaining, "the purse is empty");
		ctx.Equal(36, big.m_iSecondsToNext, "one 5 kg lot is 36 s away at 500 kg an hour");
	}

	//------------------------------------------------------------------------------------------------
	//! A purse that is not a whole number of lots strands the part lot until it refills.
	protected void RunBuyerPurse(LF_TestContext ctx)
	{
		LF_BuyerDemandRow row = FlourRow(12);
		LF_BuyerPurse purse = LF_BuyerPurse.Full(row, T0);
		LF_BuyerDemandSale sale = LF_BuyerDemand.Sell(row, purse, 20, T0);
		ctx.Equal(10, sale.m_iAccepted, "a purse of 12 kg takes two 5 kg lots");
		ctx.Equal(10, sale.m_iRefused, "and refuses the other 10");
		ctx.Equal(0, sale.m_iUnderLot, "20 kg is whole lots");
		ctx.Equal(2, purse.m_iRemaining, "2 kg are left in the purse");
		ctx.Equal(22, LF_BuyerDemand.SecondsToNextUnit(row, purse, T0), "3 kg more make a lot: 3 / 500 of an hour is 22 s");
	}

	//------------------------------------------------------------------------------------------------
	protected void RunBuyerProblems(LF_TestContext ctx)
	{
		LF_BuyerDemandRow tooSmall = LF_BuyerDemandRow.Make(BUYER, FLOUR, 3, 0, 1, "", 5);
		ctx.True(!tooSmall.Problem().IsEmpty(), "a purse below one lot is refused");
		LF_BuyerDemandRow negative = LF_BuyerDemandRow.Make(BUYER, FLOUR, 100, 0, 1, "", -1);
		ctx.True(!negative.Problem().IsEmpty(), "a negative lot is refused");
		LF_BuyerDemandRow plain = LF_BuyerDemandRow.Make(BUYER, FLOUR, 100, 0, 1);
		ctx.EqualStr("", plain.Problem(), "a row with no lot is valid");
		ctx.Equal(1, plain.Lot(), "and is priced per unit");
	}

	//------------------------------------------------------------------------------------------------
	//! A row names its curve good only when the lot sizes match: the curve's floor is cash per ITS lot.
	protected void RunCurveMatch(LF_TestContext ctx)
	{
		LF_MarketConfig config = new LF_MarketConfig();
		config.m_bEnabled = true;
		config.m_aGoods = {};
		config.m_aGoods.Insert(LF_MarketGoodDef.Make(GRAIN, 9, 5, 9, 4, 26, Lot(200)));
		LF_MarketConfig.UseForTest(config);

		LF_BuyerDemandRow same = LF_BuyerDemandRow.Make(BUYER, GRAIN, 2000, 500, 9, GRAIN, Lot(200));
		ctx.True(LF_BuyerDemandConfig.CurveFor(same) != null, "a curve with the row's own lot prices it");
		LF_BuyerDemandRow other = LF_BuyerDemandRow.Make(BUYER, GRAIN, 2000, 500, 9, GRAIN, Lot(100));
		ctx.True(LF_BuyerDemandConfig.CurveFor(other) == null, "a curve with another lot is refused and the row prices flat");
	}
}
