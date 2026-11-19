// red-proof: 2026-11-18 the first sale is skipped when RedProof() is on (`tools\cli test --no-build --red LF_Test_BuyerSale`): the sale at the open spot fails (expected 0 OK, nothing sold), `the goods left the seller` fails and `the sale paid 120 marked notes` fails. The run is recorded in the PR.
//! laundering/buyer-sale (#2920): two moonshine bottles sold at the open spot pay 2 x price in marked notes
//! with the dirty-money switch on and in cash with it off (ledger reason buyer-sale on the cash side); a
//! spot that is not open, an absent buyer, a cuffed seller, a seller out of reach and a repeat inside the gap
//! are refused with every bottle kept; a payout that does not land gives the goods back; nothing to sell
//! is refused in words.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_BuyerSale : LF_Test
{
	protected static const ResourceName MOONSHINE = "{5280F09CEB005042}Prefabs/Items/Drinks/Moonshine.et";
	protected static const string SEED = "buyer-sale-test-seed";
	protected static const string SPOT_A = "bs-a";
	protected static const string SPOT_B = "bs-b";
	protected static const string KEY = "test-buyer-sale";
	protected static const string SALE_QUEST = "test_buyer_sale_quest";
	protected static const int PERIOD_HOURS = 4;
	protected static const int PRICE = 60;

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "laundering/buyer-sale";
	}

	//------------------------------------------------------------------------------------------------
	protected static IEntity Spawn(vector at)
	{
		return LF_Utils.SpawnEntityPrefab(LF_Bank.DOOR_PREFAB, at, "0 0 0", false);
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		IEntity anchor = GetGame().GetWorld().FindEntityByName(LF_TestAnnex.BREACH_DOOR);
		ctx.NotNull(anchor, "the annex breach door anchors the fixtures");
		IEntity seller = LF_TestCharacterFixture.Spawn(ctx);
		if (!anchor || !seller)
		{
			LF_TestCharacterFixture.Delete(seller);
			return;
		}

		LF_BuyerRotation.ResetStatics();
		LF_BuyerRotation.SetSeedForTests(SEED);
		LF_LaunderConfig.SetBuyerPeriodHoursForTests(PERIOD_HOURS);
		LF_BuyerNpc.ResetStatics();
		LF_BuyerSale.ResetStatics();

		LF_BuyerPrices prices = new LF_BuyerPrices();
		prices.m_iMinActionMs = 1000;
		prices.m_fReach = 4;
		prices.m_aPrices = new array<ref LF_BuyerPriceRow>();
		LF_BuyerPriceRow row = new LF_BuyerPriceRow();
		row.m_Prefab = MOONSHINE;
		row.m_iPrice = PRICE;
		prices.m_aPrices.Insert(row);
		LF_BuyerPrices.Install(prices);

		vector at = anchor.GetOrigin() + "0 -60 0";
		IEntity a = Spawn(at);
		IEntity b = Spawn(at + "400 0 0");
		ctx.True(a && b, "two fixture spots spawn");
		if (a && b)
			RunWith(ctx, seller, a, b);

		LF_BuyerNpc.DespawnAll();
		LF_BuyerNpc.ResetStatics();
		LF_Places.Unregister(LF_BuyerRotation.PLACE_KIND, SPOT_A);
		LF_Places.Unregister(LF_BuyerRotation.PLACE_KIND, SPOT_B);
		SCR_EntityHelper.DeleteEntityAndChildren(a);
		SCR_EntityHelper.DeleteEntityAndChildren(b);
		LF_TestCharacterFixture.Delete(seller);
		LF_MoneyUtils.SetUidForTests(null, "");
		LF_MarkedCash.SetSlotForTests("");
		LF_LaunderConfig.ClearCrimePaysMarkedForTests();
		LF_LaunderConfig.SetBuyerPeriodHoursForTests(0);
		LF_BuyerRotation.ResetStatics();
		LF_BuyerPrices.ResetStatics();
		LF_BuyerSale.ResetStatics();
	}

	//------------------------------------------------------------------------------------------------
	//! A one-step quest closed by the buyer paying for the moonshine, taken by the seller.
	protected void AcceptSaleQuest(LF_TestContext ctx, LF_QuestTrackerComponent tracker)
	{
		LF_QuestObjective objective = new LF_QuestObjective();
		objective.m_eType = LF_EQuestEventType.SIGNAL;
		objective.m_sFilter = LF_QuestSignals.BuyerSold(MOONSHINE);
		objective.m_iTarget = 1;
		LF_QuestDefinition def = new LF_QuestDefinition();
		def.m_sQuestId = SALE_QUEST;
		def.m_aObjectives = {};
		def.m_aObjectives.Insert(objective);
		LF_QuestManager.GetInstance().RegisterDefinition(def);
		tracker.SetUidForTests(KEY);
		ctx.True(tracker.AcceptQuest(SALE_QUEST, false, true), "the seller takes the sale quest");
	}

	//------------------------------------------------------------------------------------------------
	protected void RunWith(LF_TestContext ctx, IEntity seller, IEntity a, IEntity b)
	{
		int period = PERIOD_HOURS * 3600;
		int start = 125001 * period;
		ctx.True(LF_Places.Register(LF_BuyerRotation.PLACE_KIND, SPOT_A, a), "spot a is placed");
		ctx.True(LF_Places.Register(LF_BuyerRotation.PLACE_KIND, SPOT_B, b), "spot b is placed");

		string open = LF_BuyerRotation.OpenSpot(start);
		IEntity spot = LF_Places.Find(LF_BuyerRotation.PLACE_KIND, open);
		ctx.True(spot != null, "the open spot resolves to an entity");
		if (!spot)
			return;

		LF_Test_BuyerSale_Players players = new LF_Test_BuyerSale_Players();
		players.m_aAt.Insert(spot.GetOrigin() + "30 0 0");
		LF_BuyerNpc.SweepAt(start, players);
		IEntity buyer = LF_BuyerNpc.GetLive();
		ctx.NotNull(buyer, "the buyer stands at the open spot");
		if (!buyer)
			return;

		LF_LaunderStationComponent.SetKeyForTests(seller, KEY);
		LF_MoneyUtils.SetUidForTests(seller, KEY);
		LF_Ledger.EnsureSubscribed();
		seller.SetOrigin(buyer.GetOrigin() + "1 0 0");

		// Dirty-money switch on: two bottles pay 2 x price in marked notes, no cash.
		LF_LaunderConfig.SetCrimePaysMarkedForTests(true);
		ctx.Equal(2, LF_InventoryUtils.AddAmount(seller, MOONSHINE, 2), "the seller carries two moonshine bottles");
		int cash = LF_MoneyUtils.GetCash(seller);
		int paid;

		// #3768: a quest that waits for the buyer to pay for this good hears the sale.
		LF_QuestTrackerComponent tracker = LF_Component<LF_QuestTrackerComponent>.Find(seller);
		ctx.NotNull(tracker, "the seller carries a quest tracker");
		if (tracker)
			AcceptSaleQuest(ctx, tracker);

		LF_EBuyerSale result = LF_EBuyerSale.FAILED;
		if (!RedProof())
			result = LF_BuyerSale.ServerSell(seller, buyer, start, 1000, paid);

		if (tracker)
			ctx.True(tracker.GetTracker().GetStatus(SALE_QUEST) == LF_EQuestStatus.COMPLETED, "the buyer paying for the bottles completes a quest that waits for that sale");

		ctx.Equal(LF_EBuyerSale.OK, result, "the sale at the open spot goes through");
		ctx.Equal(2 * PRICE, paid, "the buyer paid 2 x price");
		ctx.Equal(0, LF_InventoryUtils.GetAmount(seller, MOONSHINE), "the goods left the seller");
		ctx.Equal(2 * PRICE, LF_MarkedCash.Count(seller), "the sale paid 120 marked notes");
		ctx.Equal(cash, LF_MoneyUtils.GetCash(seller), "no cash came with the marked notes");

		// Switch off: the same sale pays cash and books buyer-sale on the cash ledger.
		LF_LaunderConfig.SetCrimePaysMarkedForTests(false);
		ctx.Equal(2, LF_InventoryUtils.AddAmount(seller, MOONSHINE, 2), "the seller carries two more bottles");
		int marked = LF_MarkedCash.Count(seller);
		ctx.Equal(LF_EBuyerSale.OK, LF_BuyerSale.ServerSell(seller, buyer, start, 1010, paid), "the sale with the switch off goes through");
		ctx.Equal(cash + 2 * PRICE, LF_MoneyUtils.GetCash(seller), "the sale paid 120 in cash");
		ctx.Equal(marked, LF_MarkedCash.Count(seller), "no marked notes came with the cash");
		array<ref LF_LedgerEntry> entries = {};
		LF_Ledger.Last(KEY, "", 10, entries);
		bool sawSale;
		foreach (LF_LedgerEntry entry : entries)
		{
			if (entry.m_sReason == LF_BuyerSale.REASON_SALE && entry.m_sAccount == LF_Ledger.ACCOUNT_CASH && entry.m_iDelta == 2 * PRICE)
				sawSale = true;
		}

		ctx.True(sawSale, "the cash ledger books buyer-sale: 120 in");

		// A repeat inside the gap is refused; the bottles stay.
		ctx.Equal(2, LF_InventoryUtils.AddAmount(seller, MOONSHINE, 2), "the seller carries two bottles again");
		cash = LF_MoneyUtils.GetCash(seller);
		ctx.Equal(LF_EBuyerSale.TOO_FAST, LF_BuyerSale.ServerSell(seller, buyer, start, 1010.3, paid), "asking again faster than the gap is refused");
		ctx.Equal(2, LF_InventoryUtils.GetAmount(seller, MOONSHINE), "a refused repeat moves no bottle");

		// A spot that is not open (the rotation moved on): refused, nothing moves. The period before `start`
		// is in the same cycle (start is an odd multiple of the period), so it opens the other spot.
		ctx.False(LF_BuyerRotation.OpenSpot(start - period) == open, "the previous period opens the other spot");
		ctx.Equal(LF_EBuyerSale.CLOSED, LF_BuyerSale.ServerSell(seller, buyer, start - period, 1100, paid), "a closed spot refuses the sale");
		ctx.Equal(2, LF_InventoryUtils.GetAmount(seller, MOONSHINE), "a closed spot moves no bottle");
		ctx.Equal(cash, LF_MoneyUtils.GetCash(seller), "a closed spot pays nothing");

		// An absent buyer (an entity that is not the live buyer): refused.
		ctx.Equal(LF_EBuyerSale.ABSENT, LF_BuyerSale.ServerSell(seller, a, start, 1110, paid), "an absent buyer refuses the sale");
		ctx.Equal(2, LF_InventoryUtils.GetAmount(seller, MOONSHINE), "an absent buyer moves no bottle");

		// A cuffed seller is refused.
		SCR_ChimeraCharacter chimera = SCR_ChimeraCharacter.Cast(seller);
		if (chimera)
		{
			chimera.LF_SetRestrained(true);
			ctx.Equal(LF_EBuyerSale.RESTRAINED, LF_BuyerSale.ServerSell(seller, buyer, start, 1120, paid), "a cuffed seller is refused");
			chimera.LF_SetRestrained(false);
			ctx.Equal(2, LF_InventoryUtils.GetAmount(seller, MOONSHINE), "a cuffed seller keeps the bottles");
		}

		// Out of reach: refused.
		seller.SetOrigin(buyer.GetOrigin() + "40 0 0");
		ctx.Equal(LF_EBuyerSale.TOO_FAR, LF_BuyerSale.ServerSell(seller, buyer, start, 1130, paid), "a seller out of reach is refused");
		seller.SetOrigin(buyer.GetOrigin() + "1 0 0");
		ctx.Equal(2, LF_InventoryUtils.GetAmount(seller, MOONSHINE), "a far seller keeps the bottles");

		// A payout that does not land (the marked-notes slot has no prefab): every bottle comes back.
		LF_LaunderConfig.SetCrimePaysMarkedForTests(true);
		LF_MarkedCash.SetSlotForTests("buyer-sale-test-no-such-slot");
		ctx.Equal(LF_EBuyerSale.FAILED, LF_BuyerSale.ServerSell(seller, buyer, start, 1200, paid), "a payout that fails fails the sale");
		ctx.Equal(0, paid, "a failed sale reports nothing paid");
		ctx.Equal(2, LF_InventoryUtils.GetAmount(seller, MOONSHINE), "a failed payout gives the goods back");
		LF_MarkedCash.SetSlotForTests("");

		// All sold, nothing left: refused in words.
		ctx.Equal(LF_EBuyerSale.OK, LF_BuyerSale.ServerSell(seller, buyer, start, 1300, paid), "the bottles sell once the payout works");
		ctx.Equal(LF_EBuyerSale.NOTHING, LF_BuyerSale.ServerSell(seller, buyer, start, 1400, paid), "nothing to sell is refused");
		ctx.True(LF_BuyerSale.RefusalKey(LF_EBuyerSale.NOTHING) == "#LF-BuyerSale_Nothing", "nothing to sell has words");
	}
}

//------------------------------------------------------------------------------------------------
//! Stand-in players for the buyer sale test: wherever the test puts them.
class LF_Test_BuyerSale_Players : LF_ResidentPlayers
{
	ref array<vector> m_aAt = {};

	override int Positions(notnull array<vector> into)
	{
		foreach (vector at : m_aAt)
			into.Insert(at);

		return m_aAt.Count();
	}
}
