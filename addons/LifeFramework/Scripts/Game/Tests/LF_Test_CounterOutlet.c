// red-proof: observed 2026-11-23 `tools\cli test --no-build --red LF_Test_CounterOutlet`: RED PROOF OK (the refused bottle stays: expected 1, got 3; the refusal pays nothing more: expected 90, got 0; the failed sale gives the bottle back: expected 1, got 3); green with --only. RedProof() skips every ServerSell call.
//! laundering/counter-outlet (#4200, crime B5): a barkeeper with a demand row of two bottles a week buys
//! under the counter. An on-duty officer is refused with every bottle kept; a seller with three bottles
//! sells two for 2 x price in marked notes (no cash) and keeps the third; the spent purse refuses the next
//! sale with nothing moved; a payout that does not land gives the bottles back and restores the purse.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_CounterOutlet : LF_Test
{
	protected static const ResourceName MOONSHINE = "{5280F09CEB005042}Prefabs/Items/Drinks/Moonshine.et";
	protected static const string BUYER = "test-counter-outlet";
	protected static const string KEY = "test-counter-outlet-seller";
	protected static const int PRICE = 45;
	protected static const int CAP = 2;
	protected static const int WEEK_HOURS = 168;
	protected static const float REACH = 4;

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "laundering/counter-outlet";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		IEntity seller = LF_TestCharacterFixture.Spawn(ctx);
		if (!seller)
			return;

		IEntity counter = LF_Utils.SpawnEntityPrefab(LF_Bank.DOOR_PREFAB, seller.GetOrigin() + "1 0 0", "0 0 0", false);
		ctx.NotNull(counter, "the counter fixture spawns");

		LF_BuyerDemandConfig config = new LF_BuyerDemandConfig();
		config.m_aRows = {};
		config.m_aRows.Insert(LF_BuyerDemandRow.Make(BUYER, MOONSHINE, CAP, 0, PRICE, "", 1, WEEK_HOURS));
		LF_BuyerDemandConfig.UseForTest(config);
		LF_BuyerDemandBook.Restore(BUYER, MOONSHINE, null);
		LF_CounterOutlet.ResetStatics();
		LF_LaunderStationComponent.SetKeyForTests(seller, KEY);
		LF_MoneyUtils.SetUidForTests(seller, KEY);
		LF_LaunderConfig.SetCrimePaysMarkedForTests(true);

		if (counter)
			RunWith(ctx, seller, counter);

		SCR_EntityHelper.DeleteEntityAndChildren(counter);
		LF_TestCharacterFixture.Delete(seller);
		LF_MoneyUtils.SetUidForTests(null, "");
		LF_MarkedCash.SetSlotForTests("");
		LF_LaunderConfig.ClearCrimePaysMarkedForTests();
		LF_BuyerDemandConfig.Reload();
		LF_BuyerDemandBook.Restore(BUYER, MOONSHINE, null);
		LF_CounterOutlet.ResetStatics();
	}

	//------------------------------------------------------------------------------------------------
	//! One sale attempt at world second `at`; skipped (FAILED, nothing paid) under the red-proof hook.
	protected LF_ECounterOutletSale Sell(IEntity seller, IEntity counter, bool police, int nowUnix, float at, out int paid)
	{
		paid = 0;
		int wait;
		if (RedProof())
			return LF_ECounterOutletSale.FAILED;

		return LF_CounterOutlet.ServerSell(seller, counter, BUYER, REACH, 1000, police, nowUnix, at, paid, wait);
	}

	//------------------------------------------------------------------------------------------------
	protected void RunWith(LF_TestContext ctx, IEntity seller, IEntity counter)
	{
		ctx.Equal(3, LF_InventoryUtils.AddAmount(seller, MOONSHINE, 3), "the seller carries three bottles");
		int cash = LF_MoneyUtils.GetCash(seller);
		int now = System.GetUnixTime();
		int paid;

		// An on-duty officer: refused, nothing moves, the purse stays full.
		LF_ECounterOutletSale result = Sell(seller, counter, true, now, 100, paid);
		ctx.Equal(LF_ECounterOutletSale.POLICE, result, "the barkeeper refuses an on-duty officer");
		ctx.Equal(3, LF_InventoryUtils.GetAmount(seller, MOONSHINE), "the officer keeps every bottle");
		ctx.Equal(0, LF_MarkedCash.Count(seller), "the officer is paid nothing");
		ctx.Equal(CAP, LF_BuyerDemandBook.RemainingAt(BUYER, MOONSHINE, now), "the refusal leaves the purse full");

		// A civilian with three bottles: the cap of two is taken, paid in marked notes; the third stays.
		result = Sell(seller, counter, false, now, 110, paid);
		ctx.Equal(LF_ECounterOutletSale.OK, result, "the barkeeper buys under the counter");
		ctx.Equal(CAP * PRICE, paid, "the sale pays 2 x price");
		ctx.Equal(CAP * PRICE, LF_MarkedCash.Count(seller), "the pay is marked notes");
		ctx.Equal(cash, LF_MoneyUtils.GetCash(seller), "no clean cash comes with it");
		ctx.Equal(1, LF_InventoryUtils.GetAmount(seller, MOONSHINE), "the bottle past the cap stays with the seller");
		ctx.Equal(0, LF_BuyerDemandBook.RemainingAt(BUYER, MOONSHINE, now), "the week's demand is spent");

		// The spent purse: refused, nothing moves.
		result = Sell(seller, counter, false, now, 120, paid);
		ctx.Equal(LF_ECounterOutletSale.FULL, result, "a spent barkeeper says he has enough");
		ctx.Equal(1, LF_InventoryUtils.GetAmount(seller, MOONSHINE), "the refused bottle stays");
		ctx.Equal(CAP * PRICE, LF_MarkedCash.Count(seller), "the refusal pays nothing more");

		// A new week: a payout that cannot land gives the bottle back and restores the purse.
		int nextWeek = now + WEEK_HOURS * 3600;
		LF_MarkedCash.SetSlotForTests("counter-outlet-test-no-such-slot");
		result = Sell(seller, counter, false, nextWeek, 130, paid);
		LF_MarkedCash.SetSlotForTests("");
		ctx.Equal(LF_ECounterOutletSale.FAILED, result, "a payout that does not land fails the sale");
		ctx.Equal(1, LF_InventoryUtils.GetAmount(seller, MOONSHINE), "the failed sale gives the bottle back");
		ctx.Equal(CAP, LF_BuyerDemandBook.RemainingAt(BUYER, MOONSHINE, nextWeek), "the failed sale puts the new week's purse back");
	}
}
