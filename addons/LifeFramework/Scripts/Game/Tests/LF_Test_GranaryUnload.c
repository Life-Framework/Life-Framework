// red-proof: observed 2026-11-19 `tools/cli test --no-build --red LF_Test_GranaryUnload`: RED PROOF OK; RedProof() skips the first sell and the first bag, so the cash, hopper and sack lines fail.
//! harvest/granary-unload (#3294): the server half of the weighbridge prompts, driven through LF_GranaryUnload
//! with a UAZ469 towing a reaper inside and outside an unload box. Sell pays the market curve for 300 kg through
//! the cash path (reason grain-sale in the ledger), empties the hopper and pays nothing the second time; a part
//! lot stays in the hopper; Bag moves exactly what fits into a nearly full vehicle storage and no grain is created
//! or lost; outside the box, with no reaper, or on foot the unload is refused and nothing moves. The test deletes
//! what it spawns.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_GranaryUnload : LF_Test
{
	protected static const ResourceName UAZ = "{C8B16ABCA57679A2}Prefabs/Vehicles/Wheeled/UAZ469/UAZ469_covered_CIV_blue.et";
	protected static const ResourceName BOX_PROP = "{21B9456F3B79410E}Prefabs/Harvest/LF_ImplementRack.et";
	protected static const vector SPOT = "120 0 60";
	protected static const vector HALF = "6 4 10";
	protected static const string UID = "test-granary-unload";
	protected static const int FILL_CHUNK_KG = 1000;
	protected static const int FILL_ROUNDS = 60;

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "harvest/granary-unload";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		array<ref LF_LedgerEntry> ledgerBefore = LF_Ledger.ExportAll();
		LF_Ledger.ApplyAll(null);
		LF_Ledger.EnsureSubscribed();

		IEntity uaz = SpawnAt(UAZ, SPOT + "2 0 3");
		IEntity box = SpawnAt(BOX_PROP, SPOT);
		IEntity seller = LF_TestCharacterFixture.Spawn(ctx);
		ctx.NotNull(uaz, "a UAZ469 spawns");
		ctx.NotNull(box, "the box prop spawns");
		if (uaz && box && seller)
		{
			LF_Utils.SetUidForTests(seller, UID);
			string reason;
			IEntity reaper = LF_ImplementHitch.Hitch(uaz, reason);
			ctx.NotNull(reaper, "the UAZ469 tows a reaper " + reason);
			LF_ImplementComponent hopper = LF_ImplementComponent.Of(reaper);
			if (hopper)
				Check(ctx, uaz, box, seller, hopper);
		}

		LF_Utils.SetUidForTests(null, "");
		LF_MarketConfig.Reload();
		LF_MarketBook.ResetStatics();
		LF_Ledger.ApplyAll(ledgerBefore);
		if (seller)
			LF_TestCharacterFixture.Delete(seller);

		if (box)
			SCR_EntityHelper.DeleteEntityAndChildren(box);

		if (uaz)
			SCR_EntityHelper.DeleteEntityAndChildren(uaz);
	}

	//------------------------------------------------------------------------------------------------
	protected void Check(LF_TestContext ctx, IEntity uaz, IEntity box, IEntity seller, LF_ImplementComponent hopper)
	{
		bool atWheel;
		ctx.True(LF_GranaryUnload.VehicleOf(seller, atWheel) == null && !atWheel, "a character on foot has no vehicle to unload");

		// Grain priced flat $4 a 50 kg lot: 300 kg is six lots, $24.
		UseGrainMarket(4, 4, 4, 0);
		hopper.SetHopperKg(300);
		int cash0 = LF_MoneyUtils.GetCash(seller);
		int paid;
		int soldKg;
		LF_EGranaryUnload result = LF_EGranaryUnload.NOT_SERVER;
		if (!RedProof())
			result = LF_GranaryUnload.Sell(seller, uaz, box, HALF, paid, soldKg);

		ctx.True(result == LF_EGranaryUnload.OK, "300 kg in the box is a sale");
		ctx.Equal(24, paid, "six lots at $4 pay 24");
		ctx.Equal(300, soldKg, "all 300 kg were taken");
		ctx.Equal(cash0 + 24, LF_MoneyUtils.GetCash(seller), "the seller holds the curve sum");
		ctx.Equal(0, hopper.GetHopperKg(), "the hopper is empty");
		array<ref LF_LedgerEntry> last = {};
		LF_Ledger.Last(UID, LF_Ledger.ACCOUNT_CASH, 5, last);
		ctx.True(last.Count() == 1 && last.Get(0).m_sReason == LF_GranaryUnload.REASON_SALE && last.Get(0).m_iDelta == 24, "the ledger has one grain-sale credit of 24");

		LF_EGranaryUnload again = LF_GranaryUnload.Sell(seller, uaz, box, HALF, paid, soldKg);
		ctx.True(again == LF_EGranaryUnload.EMPTY, "a second sell finds an empty hopper");
		ctx.Equal(0, paid, "and pays nothing");
		ctx.Equal(cash0 + 24, LF_MoneyUtils.GetCash(seller), "the cash did not move");

		// A part lot stays: 320 kg sells 300 and keeps 20, which is below one lot.
		hopper.SetHopperKg(320);
		LF_GranaryUnload.Sell(seller, uaz, box, HALF, paid, soldKg);
		ctx.Equal(24, paid, "320 kg pay for six lots");
		ctx.Equal(20, hopper.GetHopperKg(), "the part lot stays in the hopper");
		ctx.True(LF_GranaryUnload.Sell(seller, uaz, box, HALF, paid, soldKg) == LF_EGranaryUnload.BELOW_LOT, "20 kg is below one lot");
		ctx.Equal(20, hopper.GetHopperKg(), "a refused sale keeps the grain");

		// A falling curve: the payout is the market's marginal sum, and the price moves down.
		UseGrainMarket(10, 2, 10, 10);
		hopper.SetHopperKg(300);
		int quote = LF_MarketBook.QuoteAt(LF_GranaryUnload.GRAIN_SACK, 300, System.GetUnixTime());
		ctx.True(quote > 0 && quote < 60, "the quote for six lots is below the flat 6 x 10");
		cash0 = LF_MoneyUtils.GetCash(seller);
		LF_GranaryUnload.Sell(seller, uaz, box, HALF, paid, soldKg);
		ctx.Equal(quote, paid, "the payout is the curve's marginal sum");
		ctx.Equal(cash0 + quote, LF_MoneyUtils.GetCash(seller), "and the seller holds it");
		ctx.True(LF_MarketBook.CurrentPrice(LF_GranaryUnload.GRAIN_SACK) < 10, "the sale moved the price down");

		// Refusals change nothing.
		hopper.SetHopperKg(300);
		cash0 = LF_MoneyUtils.GetCash(seller);
		uaz.SetOrigin(box.GetOrigin() + "60 0 0");
		ctx.True(LF_GranaryUnload.Sell(seller, uaz, box, HALF, paid, soldKg) == LF_EGranaryUnload.OUTSIDE_BOX, "a sell outside the box is refused");
		ctx.True(LF_GranaryUnload.Bag(seller, uaz, box, HALF, soldKg) == LF_EGranaryUnload.OUTSIDE_BOX, "a bag outside the box is refused");
		ctx.Equal(300, hopper.GetHopperKg(), "the hopper keeps its grain");
		ctx.Equal(cash0, LF_MoneyUtils.GetCash(seller), "no cash moved");
		uaz.SetOrigin(box.GetOrigin() + "2 0 3");
		ctx.True(LF_GranaryUnload.Sell(seller, null, box, HALF, paid, soldKg) == LF_EGranaryUnload.NO_VEHICLE, "no vehicle is refused");
		LF_MarketConfig off = new LF_MarketConfig();
		off.m_bEnabled = false;
		LF_MarketConfig.UseForTest(off);
		LF_MarketBook.ResetStatics();
		ctx.True(LF_GranaryUnload.Sell(seller, uaz, box, HALF, paid, soldKg) != LF_EGranaryUnload.OK, "grain nobody prices is not sold");
		ctx.Equal(300, hopper.GetHopperKg(), "and stays in the hopper");

		CheckBag(ctx, uaz, box, seller, hopper);
	}

	//------------------------------------------------------------------------------------------------
	//! Bag 300 kg into a storage that has room for about one sack: only that much moves, the rest stays.
	protected void CheckBag(LF_TestContext ctx, IEntity uaz, IEntity box, IEntity seller, LF_ImplementComponent hopper)
	{
		InventoryStorageManagerComponent storage = LF_InventoryUtils.GetResponsibleStorageManager(uaz);
		ctx.NotNull(storage, "the UAZ469 has a storage manager");
		if (!storage)
			return;

		// Fill it to the brim with grain, then take 60 kg out.
		int filled;
		for (int i = 0; i < FILL_ROUNDS; i++)
		{
			int took = LF_InventoryUtils.AddAmount(storage, LF_GranaryUnload.GRAIN_SACK, FILL_CHUNK_KG);
			filled += took;
			if (took < FILL_CHUNK_KG)
				break;
		}

		LF_Debug.Info("Harvest", "granary test: the storage took " + filled + " kg of grain sacks before it refused");
		ctx.True(filled > 60 && filled < FILL_CHUNK_KG * FILL_ROUNDS, "the UAZ469 storage takes more than 60 kg of grain sacks and has a limit");
		LF_InventoryUtils.RemoveAmount(storage, LF_GranaryUnload.GRAIN_SACK, 60);
		int stored = LF_InventoryUtils.GetAmount(storage, LF_GranaryUnload.GRAIN_SACK);
		ctx.Equal(filled - 60, stored, "the storage now holds 60 kg less");

		hopper.SetHopperKg(300);
		int bagged;
		LF_EGranaryUnload result = LF_EGranaryUnload.NOT_SERVER;
		if (!RedProof())
			result = LF_GranaryUnload.Bag(seller, uaz, box, HALF, bagged);

		ctx.True(result == LF_EGranaryUnload.OK, "bagging into the nearly full storage takes what fits");
		ctx.True(bagged > 0 && bagged < 300, "only part of the 300 kg fits");
		ctx.Equal(300 - bagged, hopper.GetHopperKg(), "what did not fit stays in the hopper");
		ctx.Equal(stored + bagged, LF_InventoryUtils.GetAmount(storage, LF_GranaryUnload.GRAIN_SACK), "the storage gained exactly what left the hopper: no grain created or lost");

		// Full now: a second bag moves nothing.
		int hopperKg = hopper.GetHopperKg();
		int storedNow = LF_InventoryUtils.GetAmount(storage, LF_GranaryUnload.GRAIN_SACK);
		int second;
		LF_EGranaryUnload full = LF_GranaryUnload.Bag(seller, uaz, box, HALF, second);
		if (full == LF_EGranaryUnload.OK)
		{
			ctx.Equal(hopperKg - second, hopper.GetHopperKg(), "a further bag still conserves the hopper");
			ctx.Equal(storedNow + second, LF_InventoryUtils.GetAmount(storage, LF_GranaryUnload.GRAIN_SACK), "and the storage");
		}
		else
		{
			ctx.True(full == LF_EGranaryUnload.NO_ROOM, "a full storage says there is no room");
			ctx.Equal(hopperKg, hopper.GetHopperKg(), "and the hopper keeps everything");
			ctx.Equal(storedNow, LF_InventoryUtils.GetAmount(storage, LF_GranaryUnload.GRAIN_SACK), "and the storage is unchanged");
		}
	}

	//------------------------------------------------------------------------------------------------
	//! A market with one grain row, lot 50, no recovery.
	protected void UseGrainMarket(int base, int floorPrice, int ceilingPrice, int dropPct)
	{
		LF_MarketConfig market = new LF_MarketConfig();
		market.m_bEnabled = true;
		market.m_iMaxRecoveryHours = 24;
		market.m_aGoods = {};
		market.m_aGoods.Insert(LF_MarketGoodDef.Make(LF_GranaryUnload.GRAIN_SACK, base, floorPrice, ceilingPrice, dropPct, 0, 50));
		LF_MarketConfig.UseForTest(market);
		LF_MarketBook.ResetStatics();
	}

	//------------------------------------------------------------------------------------------------
	protected IEntity SpawnAt(ResourceName prefab, vector at)
	{
		at[1] = GetGame().GetWorld().GetSurfaceY(at[0], at[2]);
		return LF_Utils.SpawnEntityPrefab(prefab, at);
	}
}
