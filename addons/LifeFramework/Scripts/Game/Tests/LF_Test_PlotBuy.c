// red-proof: observed 2026-11-18 `tools\cli test --no-build --red LF_Test_PlotBuy` printed `RED-OK property/plot-buy (fails with the hook on, 30 assertions)` (the hook skips every Buy call); `--only` then passed=4 failed=0.
// tier: WORLD
//! property/plot-buy (#2649): a player at a for-sale plot sign buys it with carried cash. The server
//! picks the plot from the buyer's position (no id from the client). Enough cash makes the buyer the
//! owner in LF_Properties and takes exactly the price; short cash is refused and nothing is taken; an
//! owned plot is refused for a second buyer; a buyer out of reach is refused.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_PlotBuy : LF_Test
{
	protected static const ResourceName SIGN = "{DA9D4FEB9BD67EA5}Prefabs/Props/Property/LF_PlotSign.et";
	protected static const string PLOT = "plot-a";
	protected static const string BUYER_UID = "lf-test-plot-buyer";
	protected static const string SECOND_UID = "lf-test-plot-second";
	protected static const string FAR_UID = "lf-test-plot-far";

	override string GetName()
	{
		return "property/plot-buy";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_PropertyConfig.Reload();
		LF_Properties.ResetStatics();
		int price = LF_PropertyConfig.PlotPrice(PLOT);
		ctx.True(price > 0, "the example plot has a price");

		IEntity buyer = LF_TestCharacterFixture.Spawn(ctx);
		IEntity second = LF_TestCharacterFixture.Spawn(ctx);
		IEntity far = LF_TestCharacterFixture.Spawn(ctx);
		IEntity sign;
		if (buyer)
			sign = LF_Utils.SpawnEntityPrefab(SIGN, buyer.GetOrigin() + "1 0 0");

		ctx.NotNull(sign, "the plot sign prefab spawns");
		LF_PlotComponent plot;
		if (sign)
			plot = LF_Component<LF_PlotComponent>.Find(sign);

		ctx.NotNull(plot, "the plot sign carries LF_PlotComponent");
		if (price <= 0 || !buyer || !second || !far || !plot)
		{
			Cleanup(buyer, second, far, sign);
			return;
		}

		plot.SetPlotKey(PLOT);
		string plotKey;

		// Short cash: refused, nothing taken, nobody owns it.
		SetCash(ctx, buyer, price - 1);
		ctx.Equal(LF_EPlotBuy.REFUSED_NO_CASH, Buy(buyer, BUYER_UID, plotKey), "a buyer one short of the price is refused");
		ctx.Equal(price - 1, LF_MoneyUtils.GetCash(buyer), "and nothing is taken");
		ctx.EqualStr("", LF_Properties.GetOwnerKey(PLOT), "and nobody owns the plot");

		// Enough cash: owner, exactly the price taken.
		SetCash(ctx, buyer, price + 50);
		ctx.Equal(LF_EPlotBuy.BOUGHT, Buy(buyer, BUYER_UID, plotKey), "a buyer with the price buys the plot");
		ctx.EqualStr(PLOT, plotKey, "the server picked the plot from the buyer's feet");
		ctx.EqualStr(BUYER_UID, LF_Properties.GetOwnerKey(PLOT), "the buyer owns the plot in LF_Properties");
		ctx.Equal(50, LF_MoneyUtils.GetCash(buyer), "exactly the price was taken");
		LF_PropertyRecord row = LF_Properties.Get(PLOT);
		ctx.True(row && row.m_iPurchasedAt > 0, "the row records when it was bought");

		// An owned plot: a second buyer at the sign is refused and keeps their cash.
		sign.SetOrigin(second.GetOrigin() + "1 0 0");
		SetCash(ctx, second, price * 2);
		ctx.Equal(LF_EPlotBuy.REFUSED_OWNED, Buy(second, SECOND_UID, plotKey), "an owned plot is refused for a second buyer");
		ctx.Equal(price * 2, LF_MoneyUtils.GetCash(second), "the second buyer keeps their cash");
		ctx.EqualStr(BUYER_UID, LF_Properties.GetOwnerKey(PLOT), "the first buyer still owns it");

		// Out of reach: a free plot, but the sign is 40 m away.
		LF_Properties.ResetStatics();
		sign.SetOrigin(far.GetOrigin() + "40 0 0");
		SetCash(ctx, far, price * 2);
		ctx.Equal(LF_EPlotBuy.REFUSED_NO_PLOT, Buy(far, FAR_UID, plotKey), "a buyer out of reach is refused");
		ctx.Equal(price * 2, LF_MoneyUtils.GetCash(far), "the far buyer keeps their cash");
		ctx.EqualStr("", LF_Properties.GetOwnerKey(PLOT), "and nobody owns the plot");

		LF_Properties.ResetStatics();
		Cleanup(buyer, second, far, sign);
	}

	//------------------------------------------------------------------------------------------------
	protected LF_EPlotBuy Buy(IEntity buyer, string uid, out string plotKey)
	{
		plotKey = "";
		if (RedProof())
			return -1;

		return LF_PlotComponent.Buy(buyer, uid, plotKey);
	}

	//------------------------------------------------------------------------------------------------
	protected void SetCash(LF_TestContext ctx, IEntity character, int amount)
	{
		int carried = LF_MoneyUtils.GetCash(character);
		if (carried > 0)
			LF_MoneyUtils.RemoveCash(character, carried);

		if (amount > 0)
			LF_MoneyUtils.AddCash(character, amount);

		ctx.Equal(amount, LF_MoneyUtils.GetCash(character), string.Format("the character carries %1 to start", amount));
	}

	//------------------------------------------------------------------------------------------------
	protected void Cleanup(IEntity buyer, IEntity second, IEntity far, IEntity sign)
	{
		LF_TestCharacterFixture.Delete(buyer);
		LF_TestCharacterFixture.Delete(second);
		LF_TestCharacterFixture.Delete(far);
		LF_TestCharacterFixture.Delete(sign);
	}
}
