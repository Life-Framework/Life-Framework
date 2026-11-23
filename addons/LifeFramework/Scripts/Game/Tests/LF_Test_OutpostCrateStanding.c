// red-proof: observed 2026-11-23 `tools\cli test --no-build --only LF_Test_OutpostCrateStanding` on the unfixed LF_OutpostStock.Buy: FAIL with "the paid buy earns underworld standing" and "the buyer is known at this outpost"; green after the OnSale call in Buy. RedProof() leaves the outpost zone out, so no buy is at an outpost.

//! outposts/crate-standing (#4322, O4, N27): a paid Buy at the quartermaster's crate inside an outpost awards
//! underworld standing (the `fence_sale` row) and marks the buyer "known here" at that outpost, the same as a
//! trader sale. A refused buy (tier, cash) and a buy at a crate outside every outpost award nothing.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_OutpostCrateStanding : LF_Test
{
	protected static const string BUYER_UID = "outpost-crate-standing";
	protected static const string OUTPOST_ID = "crate-standing-test";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "outposts/crate-standing";
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_OutpostStockItem Row(notnull array<ref LF_OutpostStockItem> stock, string id)
	{
		foreach (LF_OutpostStockItem item : stock)
		{
			if (item.m_sId == id)
				return item;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		array<ref LF_OutpostStockItem> stock = {};
		LF_OutpostStock.DefaultStock(stock);
		LF_OutpostStockItem radio = Row(stock, "gang_radio");
		LF_OutpostStockItem rifle = Row(stock, "rifle");
		ctx.True(radio && rifle, "the default crate stocks the gang radio and the rifle");
		if (!radio || !rifle)
			return;

		IEntity buyer = LF_TestCharacterFixture.Spawn(ctx);
		if (!buyer)
			return;

		LF_Utils.SetUidForTests(buyer, BUYER_UID);
		LF_MoneyUtils.SetUidForTests(buyer, BUYER_UID);
		LF_OutpostStanding.Forget(BUYER_UID);
		LF_Reputation.Forget(BUYER_UID);
		LF_PlayerAccount account = LF_PlayerAccountManager.GetOrCreate(BUYER_UID);
		account.SetFaction(LF_Faction.CIVILIAN);
		account.SetOnDuty(false);

		// The outpost: its zone around the crate. RedProof() leaves it out, so no buy is at an outpost.
		string zone = LF_Outpost.ZONE_PREFIX + OUTPOST_ID;
		vector crate = buyer.GetOrigin();
		if (!RedProof())
			LF_Zones.RegisterCircle(zone, LF_ZoneRule.SAFE, crate, 20);

		LF_MoneyUtils.AddCash(buyer, 5000, "test", "", false);
		int pointsBefore = LF_Reputation.Points(BUYER_UID, LF_Reputation.FACTION_UNDERWORLD);

		// A tier refusal (rifle at tier 1) inside the outpost awards nothing.
		int rifleCash = LF_MoneyUtils.GetCash(buyer);
		int rifPaid;
		ctx.True(LF_OutpostStock.Buy(buyer, rifle, crate, 1, rifPaid) == LF_EOutpostBuy.NOT_KNOWN, "the rifle refuses at tier 1");
		ctx.Equal(rifleCash, LF_MoneyUtils.GetCash(buyer), "a refused rifle takes no cash");
		ctx.Equal(pointsBefore, LF_Reputation.Points(BUYER_UID, LF_Reputation.FACTION_UNDERWORLD), "a tier-refused buy earns no standing");
		ctx.False(LF_OutpostStanding.IsKnownHere(BUYER_UID, OUTPOST_ID), "a tier-refused buy does not make the buyer known");

		// A buy by someone short of cash awards nothing.
		LF_MoneyUtils.RemoveAmount(buyer, LF_MoneyUtils.GetCash(buyer), "test", "");
		LF_MoneyUtils.AddCash(buyer, radio.m_iPrice - 1, "test", "", false);
		int shortPaid;
		ctx.True(LF_OutpostStock.Buy(buyer, radio, crate, 0, shortPaid) == LF_EOutpostBuy.CASH_SHORT, "a buyer short of cash is refused");
		ctx.Equal(pointsBefore, LF_Reputation.Points(BUYER_UID, LF_Reputation.FACTION_UNDERWORLD), "a cash-short buy earns no standing");
		ctx.False(LF_OutpostStanding.IsKnownHere(BUYER_UID, OUTPOST_ID), "a cash-short buy does not make the buyer known");

		// A paid buy at the crate in the outpost: standing and the known-here flag.
		LF_MoneyUtils.AddCash(buyer, 100, "test", "", false);
		int paid;
		ctx.True(LF_OutpostStock.Buy(buyer, radio, crate, 0, paid) == LF_EOutpostBuy.OK, "a buyer with cash buys the gang radio");
		ctx.Equal(radio.m_iPrice, paid, "the radio's price is paid");
		ctx.True(LF_Reputation.Points(BUYER_UID, LF_Reputation.FACTION_UNDERWORLD) > pointsBefore, "the paid buy earns underworld standing");
		ctx.Equal(0, LF_Reputation.Points(BUYER_UID, LF_Reputation.FACTION_TOWN), "the buy moves no other track");
		ctx.True(LF_OutpostStanding.IsKnownHere(BUYER_UID, OUTPOST_ID), "the buyer is known at this outpost");
		ctx.False(LF_OutpostStanding.IsKnownHere(BUYER_UID, "other-outpost"), "the buyer is not known at another outpost");

		// The same buy at a crate outside every outpost awards and flags nothing.
		LF_OutpostStanding.Forget(BUYER_UID);
		LF_Reputation.Forget(BUYER_UID);
		int outsideBefore = LF_Reputation.Points(BUYER_UID, LF_Reputation.FACTION_UNDERWORLD);
		vector farCrate = crate + Vector(500, 0, 0);
		int outsidePaid;
		ctx.True(LF_OutpostStock.Buy(buyer, radio, farCrate, 0, outsidePaid) == LF_EOutpostBuy.OK, "the same buy outside an outpost goes through");
		ctx.Equal(outsideBefore, LF_Reputation.Points(BUYER_UID, LF_Reputation.FACTION_UNDERWORLD), "a buy outside every outpost earns no standing");
		ctx.False(LF_OutpostStanding.IsKnownHere(BUYER_UID, OUTPOST_ID), "a buy outside every outpost makes nobody known");

		LF_Zones.Unregister(zone);
		LF_OutpostStanding.Forget(BUYER_UID);
		LF_Reputation.Forget(BUYER_UID);
		LF_Utils.SetUidForTests(null, "");
		LF_MoneyUtils.SetUidForTests(null, "");
		LF_TestCharacterFixture.Delete(buyer);
	}
}
