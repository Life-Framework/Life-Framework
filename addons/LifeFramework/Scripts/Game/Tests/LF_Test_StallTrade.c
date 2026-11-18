// red-proof: 2026-11-18 `toolscli test --no-build --red LF_Test_StallTrade` printed `RED PROOF OK (all 1 listed test(s) fail with the hook on)`: the hook skips every List, Buy and Delist call (`- a buy after closing refuses (expected 1, got -1)`). `--only` passes. Review of #3014: the magazine case failed on the delete-and-respawn hand-over (`- the holder's own magazine is now in the buyer's inventory`, `- the bought magazine holds the 3 rounds it was sold with`) and passes on the entity move.
//! market/stall-trade (#2926): the holder lists items from their inventory on a rented stall at a price; a
//! buyer in reach buys one: the cash leaves the buyer, exactly one item moves from the holder to the buyer
//! and the seller is paid the price minus the market's cut. A buyer with too little cash moves nothing; a
//! buyer with no room is refunded and the item goes back; a holder who took the item away before the buy is
//! not duped (the buyer is refunded, the stale listing drops); a listing with cash in escrow cannot be
//! taken down; one item cannot be listed twice over the holder's stock.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_StallTrade : LF_Test
{
	protected static const ResourceName STALL = "{5E2A7D9C4B1F8306}Prefabs/Props/Stalls/LF_MarketStall.et";
	protected static const ResourceName APPLE = "{C9D661E5B0714711}Prefabs/Items/Food/Apple.et";
	protected static const ResourceName MAGAZINE = "{9C05543A503DB80E}Prefabs/Weapons/Magazines/Magazine_9x19_M9_15rnd_Ball.et";
	protected static const string UID_HOLDER = "lf-test-trade-holder";
	protected static const string UID_BUYER = "lf-test-trade-buyer";
	protected static const string UID_FAR = "lf-test-trade-far";
	//! Day 12 is day 5 of the week (12 % 7), the market day in `MakeConfig`.
	protected static const int MARKET_DAY = 12;
	protected static const int FEE = 50;
	protected static const int PRICE = 100;
	//! The market's cut is 5 percent: a sale of 100 pays the seller 95.
	protected static const int SELLER_PAY = 95;

	override string GetName()
	{
		return "market/stall-trade";
	}

	//------------------------------------------------------------------------------------------------
	protected LF_StallConfig MakeConfig()
	{
		return LF_StallConfig.Make(true, 5, 8, 18, FEE, 6, 1, 1000, 5);
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
	protected int Apples(IEntity character)
	{
		return LF_InventoryUtils.GetAmount(character, APPLE);
	}

	//------------------------------------------------------------------------------------------------
	//! Empties the character of apples.
	protected void ClearApples(IEntity character)
	{
		int have = Apples(character);
		if (have > 0)
			LF_InventoryUtils.RemoveAmount(character, APPLE, have);
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_StallConfig.UseForTest(MakeConfig());
		LF_StallClock.SetForTest(MARKET_DAY, 10);

		IEntity holder = LF_TestCharacterFixture.Spawn(ctx);
		IEntity buyer = LF_TestCharacterFixture.Spawn(ctx);
		IEntity far = LF_TestCharacterFixture.Spawn(ctx);
		IEntity prop;
		if (holder)
			prop = LF_Utils.SpawnEntityPrefab(STALL, holder.GetOrigin() + "1 0 0");

		LF_StallComponent stall;
		if (prop)
			stall = LF_Component<LF_StallComponent>.Find(prop);

		ctx.NotNull(stall, "the stall prefab spawns with LF_StallComponent");
		if (!holder || !buyer || !far || !stall)
		{
			Cleanup(holder, buyer, far, prop);
			return;
		}

		far.SetOrigin(holder.GetOrigin() + "60 0 0");
		LF_StallTrade.SetHolderForTest(holder, UID_HOLDER);
		ClearApples(holder);
		ClearApples(buyer);
		SetCash(ctx, holder, FEE);
		SetCash(ctx, buyer, 0);

		int listingId;
		LF_EStallResult rule;
		ctx.Equal(LF_EStallResult.OK, stall.Rent(holder, UID_HOLDER), "the holder rents the stall");
		SetCash(ctx, holder, 0);

		// Listing needs stock: nothing carried, nothing listed.
		ctx.Equal(LF_EStallTrade.NO_STOCK, List(stall, holder, UID_HOLDER, APPLE, PRICE, listingId, rule), "listing an item the holder does not carry refuses");
		ctx.Equal(0, listingId, "with no listing made");

		AddApples(ctx, holder, 2);
		ctx.Equal(LF_EStallTrade.NOT_HOLDER, List(stall, buyer, UID_BUYER, APPLE, PRICE, listingId, rule), "somebody else cannot list on the stall");
		ctx.Equal(LF_EStallTrade.RULE, List(stall, holder, UID_HOLDER, APPLE, 100000, listingId, rule), "a price over the cap refuses");
		ctx.Equal(LF_EStallResult.PRICE_HIGH, rule, "with the rules' reason");

		// List: one apple, then the second; a third has no stock behind it.
		ctx.Equal(LF_EStallTrade.OK, List(stall, holder, UID_HOLDER, APPLE, PRICE, listingId, rule), "the holder lists an apple");
		int firstId = listingId;
		ctx.Equal(LF_EStallTrade.OK, List(stall, holder, UID_HOLDER, APPLE, PRICE + 20, listingId, rule), "and a second apple");
		int secondId = listingId;
		ctx.Equal(LF_EStallTrade.NO_STOCK, List(stall, holder, UID_HOLDER, APPLE, PRICE, listingId, rule), "a third listing over two apples refuses");
		ctx.Equal(2, stall.GetStall().m_aListings.Count(), "the stall lists exactly two");
		ctx.Equal(2, Apples(holder), "listing took nothing from the holder");
		ctx.Equal(firstId, LF_StallTrade.CheapestOpen(stall.GetStall()), "the cheapest open listing is the first");

		// Too little cash moves nothing.
		SetCash(ctx, buyer, PRICE - 1);
		int paid;
		int sellerPay;
		ctx.Equal(LF_EStallTrade.RULE, Buy(stall, buyer, UID_BUYER, firstId, paid, sellerPay, rule), "a buyer one short refuses");
		ctx.Equal(LF_EStallResult.CANT_AFFORD, rule, "for the price");
		ctx.Equal(PRICE - 1, LF_MoneyUtils.GetCash(buyer), "no cash moved from the buyer");
		ctx.Equal(0, Apples(buyer), "no item reached the buyer");
		ctx.Equal(2, Apples(holder), "the holder still has both apples");
		ctx.Equal(0, LF_MoneyUtils.GetCash(holder), "the seller was paid nothing");
		ctx.False(IsHeld(stall, firstId), "and the listing is not held");

		// Out of reach and the holder's own purse.
		SetCash(ctx, far, PRICE);
		ctx.Equal(LF_EStallTrade.OUT_OF_REACH, Buy(stall, far, UID_FAR, firstId, paid, sellerPay, rule), "a buyer out of reach refuses");
		ctx.Equal(PRICE, LF_MoneyUtils.GetCash(far), "with no cash moved");
		SetCash(ctx, holder, PRICE);
		ctx.Equal(LF_EStallTrade.RULE, Buy(stall, holder, UID_HOLDER, firstId, paid, sellerPay, rule), "the holder cannot buy their own listing");
		ctx.Equal(LF_EStallResult.SELF_BUY, rule, "the rules say so");
		ctx.Equal(PRICE, LF_MoneyUtils.GetCash(holder), "with no cash moved");
		SetCash(ctx, holder, 0);

		// Buy: exactly one item and the right cash move.
		SetCash(ctx, buyer, PRICE + 50);
		ctx.Equal(LF_EStallTrade.OK, Buy(stall, buyer, UID_BUYER, firstId, paid, sellerPay, rule), "a buyer with the price buys the first listing");
		ctx.Equal(PRICE, paid, "the price was paid");
		ctx.Equal(SELLER_PAY, sellerPay, "the seller is paid the price minus the cut");
		ctx.Equal(50, LF_MoneyUtils.GetCash(buyer), "the buyer lost exactly the price");
		ctx.Equal(1, Apples(buyer), "the buyer holds exactly one apple");
		ctx.Equal(1, Apples(holder), "the holder lost exactly one apple");
		ctx.Equal(SELLER_PAY, LF_MoneyUtils.GetCash(holder), "the holder was paid 95");
		ctx.Equal(1, stall.GetStall().m_aListings.Count(), "the sold listing is gone");
		ctx.Equal(5, stall.GetStall().m_iCutKept, "the market kept its cut");

		// The listing is sold: the same id cannot be bought again.
		ctx.Equal(LF_EStallTrade.RULE, Buy(stall, buyer, UID_BUYER, firstId, paid, sellerPay, rule), "a sold listing cannot be bought twice");
		ctx.Equal(LF_EStallResult.NO_LISTING, rule, "no such listing");
		ctx.Equal(50, LF_MoneyUtils.GetCash(buyer), "with no cash moved");

		// In flight: a listing with cash in escrow cannot be taken down; once refunded it can.
		LF_StallEscrow escrow;
		ctx.Equal(LF_EStallResult.OK, LF_StallRules.Reserve(MakeConfig(), stall.GetStall(), secondId, UID_BUYER, 1000, MARKET_DAY, 10, escrow), "a buyer's cash is reserved for the second listing");
		ctx.Equal(LF_EStallTrade.RULE, Delist(stall, UID_HOLDER, secondId, rule), "the holder cannot take the listing down while cash is in escrow");
		ctx.Equal(LF_EStallResult.RESERVED, rule, "the listing is reserved");
		ctx.Equal(1, Apples(holder), "so the holder's apple cannot be both sold and taken back");
		int refund;
		ctx.Equal(LF_EStallResult.OK, LF_StallRules.Refund(stall.GetStall(), escrow, refund), "the escrow refunds");
		ctx.Equal(LF_EStallTrade.NOT_HOLDER, Delist(stall, UID_BUYER, secondId, rule), "only the holder takes a listing down");

		// The holder takes the item away before the buy: the buyer is refunded, nothing is duped.
		LF_InventoryUtils.RemoveAmount(holder, APPLE, 1);
		ctx.Equal(0, Apples(holder), "the holder took the apple back out of the inventory");
		SetCash(ctx, buyer, PRICE + 50);
		int cashBefore = LF_MoneyUtils.GetCash(buyer);
		int holderCashBefore = LF_MoneyUtils.GetCash(holder);
		ctx.Equal(LF_EStallTrade.NO_STOCK, Buy(stall, buyer, UID_BUYER, secondId, paid, sellerPay, rule), "a buy of an item the holder no longer carries refuses");
		ctx.Equal(cashBefore, LF_MoneyUtils.GetCash(buyer), "the buyer keeps the cash");
		ctx.Equal(1, Apples(buyer), "and gained no second apple");
		ctx.Equal(holderCashBefore, LF_MoneyUtils.GetCash(holder), "the seller was not paid");
		ctx.Equal(0, stall.GetStall().m_aListings.Count(), "the stale listing is dropped");

		// A full buyer is refunded and the item goes back to the holder.
		AddApples(ctx, holder, 1);
		ctx.Equal(LF_EStallTrade.OK, List(stall, holder, UID_HOLDER, APPLE, PRICE, listingId, rule), "the holder lists the new apple");
		LF_StallTrade.SetNoRoomForTest(true);
		SetCash(ctx, buyer, PRICE);
		int buyerApples = Apples(buyer);
		holderCashBefore = LF_MoneyUtils.GetCash(holder);
		ctx.Equal(LF_EStallTrade.BUYER_FULL, Buy(stall, buyer, UID_BUYER, listingId, paid, sellerPay, rule), "a buyer with no room in the inventory is refused (the inventory is simulated full: a test body's pockets never fill)");
		ctx.Equal(PRICE, LF_MoneyUtils.GetCash(buyer), "the buyer's cash is back");
		ctx.Equal(buyerApples, Apples(buyer), "the buyer gained nothing");
		ctx.Equal(1, Apples(holder), "the holder's apple went back");
		ctx.Equal(holderCashBefore, LF_MoneyUtils.GetCash(holder), "the seller was not paid");
		ctx.True(stall.GetStall().Find(listingId) != null, "the listing stays on the stall");
		ctx.False(IsHeld(stall, listingId), "and is open to buyers again");
		LF_StallTrade.SetNoRoomForTest(false);

		// A magazine moves as itself: the buyer gets the holder's 3 rounds, not a fresh full magazine
		// (review of #3014: a delete and respawn by prefab turned an empty magazine into a full one).
		ctx.Equal(1, LF_InventoryUtils.AddAmount(holder, MAGAZINE, 1, false), "the holder carries a magazine");
		IEntity magazine = LF_StallTrade.FindHeldItem(holder, MAGAZINE);
		BaseMagazineComponent rounds;
		if (magazine)
			rounds = BaseMagazineComponent.Cast(magazine.FindComponent(BaseMagazineComponent));
		ctx.NotNull(rounds, "the magazine has a magazine component");
		if (rounds)
			rounds.SetAmmoCount(3);
		int magazineId;
		ctx.Equal(LF_EStallTrade.OK, List(stall, holder, UID_HOLDER, MAGAZINE, PRICE, magazineId, rule), "the holder lists the magazine");
		SetCash(ctx, buyer, PRICE);
		ctx.Equal(LF_EStallTrade.OK, Buy(stall, buyer, UID_BUYER, magazineId, paid, sellerPay, rule), "the buyer buys the magazine");
		ctx.True(magazine && LF_InventoryUtils.GetStorageHierachyRoot(magazine) == buyer, "the holder's own magazine is now in the buyer's inventory");
		IEntity bought = LF_StallTrade.FindHeldItem(buyer, MAGAZINE);
		BaseMagazineComponent boughtRounds;
		if (bought)
			boughtRounds = BaseMagazineComponent.Cast(bought.FindComponent(BaseMagazineComponent));
		ctx.True(boughtRounds && boughtRounds.GetAmmoCount() == 3, "the bought magazine holds the 3 rounds it was sold with");
		ctx.Equal(0, LF_InventoryUtils.GetAmount(holder, MAGAZINE), "the holder no longer has it");
		LF_InventoryUtils.RemoveAmount(buyer, MAGAZINE, LF_InventoryUtils.GetAmount(buyer, MAGAZINE));
		SetCash(ctx, buyer, PRICE);

		// After the closing hour the stall refuses and nothing moves.
		LF_StallClock.SetForTest(MARKET_DAY, 18);
		ctx.Equal(LF_EStallTrade.RULE, Buy(stall, buyer, UID_BUYER, listingId, paid, sellerPay, rule), "a buy after closing refuses");
		ctx.Equal(PRICE, LF_MoneyUtils.GetCash(buyer), "with no cash moved");
		ctx.Equal(1, Apples(holder), "and the unsold apple is still the holder's");

		ClearApples(holder);
		ClearApples(buyer);
		Cleanup(holder, buyer, far, prop);
	}

	//------------------------------------------------------------------------------------------------
	//! \return true when a buyer's cash is reserved for the listing.
	protected bool IsHeld(LF_StallComponent stall, int listingId)
	{
		LF_StallListing listing = stall.GetStall().Find(listingId);
		return listing && listing.m_bReserved;
	}

	//------------------------------------------------------------------------------------------------
	protected void AddApples(LF_TestContext ctx, IEntity character, int count)
	{
		ctx.Equal(count, LF_InventoryUtils.AddAmount(character, APPLE, count, false), string.Format("%1 apples go into the character's inventory", count));
	}

	//------------------------------------------------------------------------------------------------
	//! The list under test; the red proof skips it.
	protected LF_EStallTrade List(LF_StallComponent stall, IEntity holder, string uid, ResourceName item, int price, out int listingId, out LF_EStallResult rule)
	{
		listingId = 0;
		rule = LF_EStallResult.OK;
		if (RedProof())
			return -1;

		return LF_StallTrade.List(stall, holder, uid, item, price, listingId, rule);
	}

	//------------------------------------------------------------------------------------------------
	//! The buy under test; the red proof skips it.
	protected LF_EStallTrade Buy(LF_StallComponent stall, IEntity buyer, string uid, int listingId, out int paid, out int sellerPay, out LF_EStallResult rule)
	{
		paid = 0;
		sellerPay = 0;
		rule = LF_EStallResult.OK;
		if (RedProof())
			return -1;

		return LF_StallTrade.Buy(stall, buyer, uid, listingId, paid, sellerPay, rule);
	}

	//------------------------------------------------------------------------------------------------
	//! The delist under test; the red proof skips it.
	protected LF_EStallTrade Delist(LF_StallComponent stall, string uid, int listingId, out LF_EStallResult rule)
	{
		rule = LF_EStallResult.OK;
		if (RedProof())
			return -1;

		return LF_StallTrade.Delist(stall, uid, listingId, rule);
	}

	//------------------------------------------------------------------------------------------------
	protected void Cleanup(IEntity holder, IEntity buyer, IEntity far, IEntity prop)
	{
		LF_StallTrade.ResetStatics();
		LF_StallClock.ResetStatics();
		LF_StallConfig.Reload();
		LF_TestCharacterFixture.Delete(holder);
		LF_TestCharacterFixture.Delete(buyer);
		LF_TestCharacterFixture.Delete(far);
		LF_TestCharacterFixture.Delete(prop);
	}
}
