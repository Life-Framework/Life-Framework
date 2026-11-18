//------------------------------------------------------------------------------------------------
//! Records LF_OnMarketPriceMoved for LF_Test_MarketTrader.
class LF_MarketMoveRecorder : LF_EventSubscriber
{
	int m_iCount;
	int m_iLastOld;
	int m_iLastNew;

	override bool Handle(notnull LF_Event busEvent)
	{
		LF_OnMarketPriceMoved moved = LF_OnMarketPriceMoved.Cast(busEvent);
		if (!moved)
			return true;

		m_iCount++;
		m_iLastOld = moved.m_iOldPrice;
		m_iLastNew = moved.m_iNewPrice;
		return true;
	}
}

// red-proof: new test with no failing state on main (the sale never read the curve). Batched hook run:
// `tools\cli test --no-build --red LF_Test_MarketTrader` skips the curved sale in RedProof() and the
// curve assertions fail. Observed 2026-11-18: `RED PROOF OK (all 1 listed test(s) fail with the hook on)`; green 51 assertions.
// Trickle recovery (review of #2877): red on the stamp-to-now book, `expected 19, got 18` (2026-11-18), green with StampAfterSale.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_MarketTrader : LF_Test
{
	protected static const ResourceName CHARACTER_PREFAB = "{9B5BB216CC7FF18E}Prefabs/Characters/Core/Character_Roleplay.et";
	protected static const ResourceName JACKET_PREFAB = "{9F546CCA2582D16F}Prefabs/Characters/Uniforms/Jacket_M88.et";
	protected static const ResourceName APPLE_PREFAB = "{C9D661E5B0714711}Prefabs/Items/Food/Apple.et";
	protected static const ResourceName IRON_BAR_PREFAB = "{BCD1E8CD7F975074}Prefabs/Resources/Mining/IronBar.et";

	//! The apple curve: base 20, floor 2, ceiling 40, 5 percent of the base (1 cash) off per unit.
	protected static const int BASE = 20;
	protected static const int FLOOR = 2;
	protected static const int CEILING = 40;
	protected static const int DROP_PCT = 5;
	protected static const int RECOVER_PCT = 10;

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "market/trader-sale";
	}

	//------------------------------------------------------------------------------------------------
	protected IEntity SpawnSeller(LF_TestContext ctx, vector position)
	{
		Resource res = Resource.Load(CHARACTER_PREFAB);
		ctx.True(res.IsValid(), "character prefab loads");
		if (!res.IsValid())
			return null;

		EntitySpawnParams params();
		params.TransformMode = ETransformMode.WORLD;
		Math3D.MatrixIdentity4(params.Transform);
		params.Transform[3] = position;

		IEntity character = GetGame().SpawnEntityPrefab(res, GetGame().GetWorld(), params);
		ctx.NotNull(character, "seller character spawns");
		if (!character)
			return null;

		InventoryStorageManagerComponent storage = LF_InventoryUtils.GetResponsibleStorageManager(character);
		IEntity jacket = LF_Utils.SpawnEntityPrefab(JACKET_PREFAB, character.GetOrigin());
		if (!storage || !jacket)
		{
			ctx.Fail("seller has no inventory manager or the jacket did not spawn");
			return null;
		}

		ctx.True(storage.TryInsertItem(jacket, EStoragePurpose.PURPOSE_LOADOUT_PROXY), "jacket goes on the seller");
		ctx.Equal(100, LF_MoneyUtils.AddCash(character, 100), "seller starts with 100 cash");
		return character;
	}

	//------------------------------------------------------------------------------------------------
	//! Gives `count` apples and returns the one stack entity, or null.
	protected IEntity GiveApples(LF_TestContext ctx, IEntity character, int count)
	{
		ctx.Equal(count, LF_InventoryUtils.AddAmount(character, APPLE_PREFAB, count), string.Format("seller gets %1 apples", count));
		InventoryStorageManagerComponent storage = LF_InventoryUtils.GetResponsibleStorageManager(character);
		array<IEntity> items = LF_InventoryUtils.FindItemsByPrefab(storage, APPLE_PREFAB);
		ctx.Equal(1, items.Count(), "the apples are one stack");
		if (items.Count() != 1)
			return null;

		return items[0];
	}

	//------------------------------------------------------------------------------------------------
	protected LF_MarketConfig MakeConfig(bool enabled, ResourceName good)
	{
		LF_MarketConfig config = new LF_MarketConfig();
		config.m_bEnabled = enabled;
		config.m_iMaxRecoveryHours = 168;
		config.m_aGoods = {};
		config.m_aGoods.Insert(LF_MarketGoodDef.Make(good, BASE, FLOOR, CEILING, DROP_PCT, RECOVER_PCT));
		config.Sanitize();
		return config;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		IEntity traderOwner = GetGame().GetWorld().FindEntityByName(LF_TestAnnex.APPLE_TRADE_CONTAINER);
		ctx.NotNull(traderOwner, "the annex apple trader resolves");
		if (!traderOwner)
			return;

		LF_MarketBook.ResetStatics();
		LF_MarketMoveRecorder recorder = new LF_MarketMoveRecorder();
		LF_EventBus.Subscribe(LF_OnMarketPriceMoved, recorder);

		IEntity near = SpawnSeller(ctx, traderOwner.GetOrigin() + Vector(1.5, 0.1, 0));
		IEntity far = SpawnSeller(ctx, traderOwner.GetOrigin() + Vector(LF_TraderSale.REACH + 40, 0.1, 0));
		if (near && far)
			RunSales(ctx, traderOwner, near, far, recorder);

		LF_TraderSale.FailRemoveForTest(false);
		LF_EventBus.Unsubscribe(LF_OnMarketPriceMoved, recorder);
		LF_MarketConfig.Reload();
		LF_MarketBook.ResetStatics();
		SCR_EntityHelper.DeleteEntityAndChildren(near);
		SCR_EntityHelper.DeleteEntityAndChildren(far);
	}

	//------------------------------------------------------------------------------------------------
	protected void RunSales(LF_TestContext ctx, IEntity traderOwner, IEntity near, IEntity far, LF_MarketMoveRecorder recorder)
	{
		// Market off, apples listed: the fixed trader value (5) exactly as before the market.
		LF_MarketConfig.UseForTest(MakeConfig(false, APPLE_PREFAB));
		IEntity apples = GiveApples(ctx, near, 2);
		if (!apples)
			return;
		ctx.True(LF_TraderSale.Sell(near, apples, traderOwner, true) == LF_ETraderSale.SOLD, "market off: the sale goes through");
		ctx.Equal(110, LF_MoneyUtils.GetCash(near), "market off: 2 apples pay the fixed 10");
		ctx.Equal(0, LF_MarketBook.CurrentPrice(APPLE_PREFAB), "market off: apples are not curved");

		// Market on, apples not in the config: the fixed value unchanged.
		LF_MarketConfig.UseForTest(MakeConfig(true, IRON_BAR_PREFAB));
		apples = GiveApples(ctx, near, 2);
		if (!apples)
			return;
		ctx.True(LF_TraderSale.Sell(near, apples, traderOwner, true) == LF_ETraderSale.SOLD, "a good with no curve sells");
		ctx.Equal(120, LF_MoneyUtils.GetCash(near), "a good with no curve pays its fixed value");
		ctx.Equal(0, recorder.m_iCount, "a fixed-price sale moves no price");

		// Market on for apples: a stack of 10 pays the curve's bulk total and lowers the next quote.
		LF_MarketConfig.UseForTest(MakeConfig(true, APPLE_PREFAB));
		int drop = LF_PriceCurve.DropPerUnit(BASE, DROP_PCT);
		int expected = LF_PriceCurve.BulkTotal(BASE, 10, drop, FLOOR, CEILING);
		ctx.Equal(155, expected, "the curve prices 10 apples at 155 (20 + 19 + ... + 11)");
		ctx.Equal(BASE, LF_MarketBook.CurrentPrice(APPLE_PREFAB), "an unsold good quotes its base");
		ctx.Equal(expected, LF_MarketBook.Quote(APPLE_PREFAB, 10), "the book quotes the bulk total");

		apples = GiveApples(ctx, near, 10);
		if (!apples)
			return;
		LF_ETraderSale outcome = LF_ETraderSale.NOT_SERVER;
		if (!RedProof())
			outcome = LF_TraderSale.Sell(near, apples, traderOwner, true);
		ctx.True(outcome == LF_ETraderSale.SOLD, "10 curved apples sell");
		ctx.Equal(120 + expected, LF_MoneyUtils.GetCash(near), "the seller is paid the bulk total");
		ctx.Equal(0, LF_InventoryUtils.GetAmount(near, APPLE_PREFAB), "the sold apples left the seller");
		int after = LF_MarketBook.CurrentPrice(APPLE_PREFAB);
		ctx.Equal(LF_PriceCurve.PriceAfterSale(BASE, 10, drop, FLOOR, CEILING), after, "the price moved down the curve");
		ctx.True(after < BASE, "the next single unit quotes lower");
		ctx.Equal(after, LF_MarketBook.Quote(APPLE_PREFAB, 1), "one unit quotes the new price");
		ctx.Equal(1, recorder.m_iCount, "one price-moved event");
		ctx.Equal(BASE, recorder.m_iLastOld, "the event carries the old price");
		ctx.Equal(after, recorder.m_iLastNew, "the event carries the new price");

		// Recovery is lazy: an hour later the quote has closed part of the gap; quoting moves nothing.
		int inHour = LF_MarketBook.CurrentPriceAt(APPLE_PREFAB, System.GetUnixTime() + LF_PriceCurve.SECONDS_PER_HOUR);
		ctx.Equal(LF_PriceCurve.Recover(after, BASE, RECOVER_PCT, LF_PriceCurve.SECONDS_PER_HOUR, 168, FLOOR, CEILING), inHour, "an hour of recovery");
		ctx.Equal(after, LF_MarketBook.CurrentPrice(APPLE_PREFAB), "a quote does not move the price");

		// Steady trade still recovers: a sale half an hour after the last keeps the part hour, so an
		// hour after the first sale the price has taken one recovery step (review of #2877).
		int t0 = System.GetUnixTime() + 10 * LF_PriceCurve.SECONDS_PER_HOUR;
		int unused;
		int p1 = LF_MarketBook.ApplySaleAt(APPLE_PREFAB, 1, t0, unused);
		int p2 = LF_MarketBook.ApplySaleAt(APPLE_PREFAB, 1, t0 + LF_PriceCurve.SECONDS_PER_HOUR / 2, unused);
		ctx.True(p2 < p1, "the second trickle sale lowers the price");
		int stepped = LF_MarketBook.CurrentPriceAt(APPLE_PREFAB, t0 + LF_PriceCurve.SECONDS_PER_HOUR);
		ctx.Equal(LF_PriceCurve.Recover(p2, BASE, RECOVER_PCT, LF_PriceCurve.SECONDS_PER_HOUR, 168, FLOOR, CEILING), stepped, "a sale inside the hour keeps the part hour toward recovery");
		ctx.True(stepped > p2, "trickle selling does not pin the price");
		// Put the book back where the 10-apple sale left it for the checks below.
		LF_MarketBook.ResetStatics();
		LF_MarketBook.ApplySaleAt(APPLE_PREFAB, 10, System.GetUnixTime(), unused);
		ctx.Equal(after, LF_MarketBook.CurrentPrice(APPLE_PREFAB), "the book is back at the post-sale price");

		// Refusals leave the price where it was.
		int cashFar = LF_MoneyUtils.GetCash(far);
		IEntity farApples = GiveApples(ctx, far, 3);
		if (farApples)
		{
			ctx.True(LF_TraderSale.Sell(far, farApples, traderOwner, true) == LF_ETraderSale.OUT_OF_REACH, "out of reach is refused");
			ctx.Equal(after, LF_MarketBook.CurrentPrice(APPLE_PREFAB), "out of reach leaves the price");
		}

		IEntity ground = LF_Utils.SpawnEntityPrefab(APPLE_PREFAB, near.GetOrigin() + Vector(0.5, 0, 0));
		if (ground)
		{
			ctx.True(LF_TraderSale.Sell(near, ground, traderOwner, true) == LF_ETraderSale.NOT_CARRIED, "an item not carried is refused");
			ctx.Equal(after, LF_MarketBook.CurrentPrice(APPLE_PREFAB), "not carried leaves the price");
			SCR_EntityHelper.DeleteEntityAndChildren(ground);
		}

		// A failed step after the price moved rolls the price back with the cash.
		int cashNear = LF_MoneyUtils.GetCash(near);
		apples = GiveApples(ctx, near, 4);
		if (!apples)
			return;
		LF_TraderSale.FailRemoveForTest(true);
		ctx.True(LF_TraderSale.Sell(near, apples, traderOwner, true) == LF_ETraderSale.NOT_REMOVED, "a sale whose item cannot be removed fails");
		LF_TraderSale.FailRemoveForTest(false);
		ctx.Equal(cashNear, LF_MoneyUtils.GetCash(near), "the rollback takes the payout back");
		ctx.Equal(after, LF_MarketBook.CurrentPrice(APPLE_PREFAB), "the rollback restores the price");
		ctx.Equal(4, LF_InventoryUtils.GetAmount(near, APPLE_PREFAB), "the rollback leaves the apples");
		ctx.Equal(1, recorder.m_iCount, "a rolled-back sale publishes no event");

		// A dead or downed seller is refused (a forged request from a body).
		SCR_CharacterDamageManagerComponent damage = SCR_CharacterDamageManagerComponent.Cast(far.FindComponent(SCR_CharacterDamageManagerComponent));
		if (damage && farApples)
		{
			far.SetOrigin(traderOwner.GetOrigin() + Vector(-1.5, 0.1, 0));
			damage.Kill(Instigator.CreateInstigator(null));
			ctx.True(LF_TraderSale.Sell(far, farApples, traderOwner, true) == LF_ETraderSale.SELLER_DOWN, "a dead or downed seller is refused");
			ctx.Equal(cashFar, LF_MoneyUtils.GetCash(far), "the downed seller is paid nothing");
			ctx.Equal(after, LF_MarketBook.CurrentPrice(APPLE_PREFAB), "a downed seller leaves the price");
		}
	}
};
