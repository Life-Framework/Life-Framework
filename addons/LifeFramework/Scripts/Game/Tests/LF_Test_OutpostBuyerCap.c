// red-proof: RED-OK 2026-11-23 `tools\cli test --no-build --red LF_Test_OutpostBuyerCap`: with RedProof() (no outpost zone, the purse never emptied) it FAILS "the refused crate stays with the seller (expected 1, got 0)" and "a refused crate pays nothing (expected 230, got 460)"; green without the hook.

//! outposts/buyer-cap (#4225, O5): the outpost's contraband buyer pays 10 to 20% above the fence and takes a
//! crate only while its demand purse (LF_BuyerDemand, CR-B1) has room: at the cap it refuses and the crate
//! stays with the seller. The outpost is the far end of a convoy run: a crate with no run, or a run never
//! driven, fetches nothing, and a sale closes the run (one crate per run). An on-duty officer is refused
//! inside the outpost (LF_Outpost.RefusesService) and keeps the crates; a completed sale books the outpost
//! standing (LF_OutpostStanding.OnSale). RedProof() leaves the outpost zone out and the purse full, so the
//! officer is served, nothing is known and the cap never bites.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_OutpostBuyerCap : LF_Test
{
	protected static const string SELLER_UID = "outpost-buyer-cap";
	protected static const string OUTPOST_ID = "buyer-cap-test";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "outposts/buyer-cap";
	}

	//------------------------------------------------------------------------------------------------
	protected static void Cleanup(IEntity seller, string zone, string buyerId, LF_BuyerPurse purseBefore)
	{
		LF_InventoryUtils.RemoveAmount(seller, LF_Contraband.ITEM, LF_Contraband.CountCarried(seller));
		LF_BuyerDemandBook.Restore(buyerId, LF_Contraband.ITEM, purseBefore);
		LF_Zones.Unregister(zone);
		LF_OutpostStanding.Forget(SELLER_UID);
		LF_Reputation.Forget(SELLER_UID);
		LF_OrganizationManager.Reset();
		LF_Hideout.Reset();
		LF_Contraband.Reset();
		LF_Utils.SetUidForTests(null, "");
		LF_MoneyUtils.SetUidForTests(null, "");
		LF_TestCharacterFixture.Delete(seller);
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		// The price rule, pure.
		int fence = LF_OutpostBuyer.FencePrice();
		ctx.True(fence > 0, "the fence pays something for a crate");
		LF_BuyerDemandRow row = LF_OutpostBuyer.DefaultRow(OUTPOST_ID);
		ctx.True(row.Problem().IsEmpty(), "the built-in demand row is usable: " + row.Problem());
		ctx.True(row.m_iPrice * 100 >= fence * 110, string.Format("the buyer pays at least 10 percent above the fence (%1 vs %2)", row.m_iPrice, fence));
		ctx.True(row.m_iPrice * 100 <= fence * 120, string.Format("the buyer pays at most 20 percent above the fence (%1 vs %2)", row.m_iPrice, fence));
		ctx.Equal(110, LF_OutpostBuyer.PriceAboveFence(100, 5), "a premium below 10 percent is held to 10");
		ctx.Equal(120, LF_OutpostBuyer.PriceAboveFence(100, 40), "a premium above 20 percent is held to 20");
		ctx.Equal(0, LF_OutpostBuyer.PriceAboveFence(0, 15), "no fence price, no price");

		IEntity seller = LF_TestCharacterFixture.Spawn(ctx);
		if (!seller)
			return;

		LF_OrganizationManager.Reset();
		LF_Hideout.Reset();
		LF_Contraband.Reset();
		LF_Utils.SetUidForTests(seller, SELLER_UID);
		LF_MoneyUtils.SetUidForTests(seller, SELLER_UID);
		LF_OutpostStanding.Forget(SELLER_UID);
		LF_Reputation.Forget(SELLER_UID);
		LF_PlayerAccount account = LF_PlayerAccountManager.GetOrCreate(SELLER_UID);
		account.SetFaction(LF_Faction.CIVILIAN);
		account.SetOnDuty(false);
		string buyerId = LF_OutpostBuyer.BuyerId(OUTPOST_ID);
		LF_BuyerPurse purseBefore = LF_BuyerDemandBook.Snapshot(buyerId, LF_Contraband.ITEM);
		string zone = LF_Outpost.ZONE_PREFIX + OUTPOST_ID;

		// A convoy run is keyed by the active character (LF_Hideout.CharacterIdOf), not the account.
		if (!account.GetActiveCharacter())
			account.AddCharacter(LF_PlayerCharacter.Create("Prefabs/Characters/Core/Character_Roleplay.et", "Buyer", "Cap", 30), true);

		string characterId = LF_Hideout.CharacterIdOf(seller);
		ctx.False(characterId.IsEmpty(), "the seller has an active character to haul the run");
		LF_Organization org = LF_OrganizationManager.Create(characterId, "Buyer Cap Outfit");
		ctx.NotNull(org, "the seller founds the gang that holds the hideout");
		if (!org)
		{
			Cleanup(seller, zone, buyerId, purseBefore);
			return;
		}

		ctx.True(LF_Hideout.Assign(LF_Hideout.OrgHideoutId(), org.GetId()), "the gang owns the hideout");

		vector table = seller.GetOrigin();
		if (!RedProof())
			LF_Zones.RegisterCircle(zone, LF_ZoneRule.SAFE, table, 20);

		ctx.Equal(2, LF_InventoryUtils.AddAmount(seller, LF_Contraband.ITEM, 2, false), "the seller carries two contraband crates");
		int now = System.GetUnixTime();
		int sold;
		int paid;

		// Crates with no convoy run (a crate farmed at the barn) fetch nothing.
		int cashBefore = LF_MoneyUtils.GetCash(seller);
		ctx.True(LF_OutpostBuyer.Sell(seller, OUTPOST_ID, table, now, sold, paid) == LF_EOutpostSell.NO_RUN, "a crate with no run is refused");
		ctx.Equal(cashBefore, LF_MoneyUtils.GetCash(seller), "a crate with no run pays nothing");
		ctx.Equal(2, LF_Contraband.CountCarried(seller), "the crates stay with the seller");

		// A run that was walked, never driven, fetches nothing either; the purse is untouched.
		ctx.True(LF_Contraband.Start(characterId, null), "the seller starts a convoy run");
		ctx.True(LF_OutpostBuyer.Sell(seller, OUTPOST_ID, table, now, sold, paid) == LF_EOutpostSell.NO_RUN, "an undriven run is refused");
		ctx.Equal(cashBefore, LF_MoneyUtils.GetCash(seller), "the undriven run is not paid");
		ctx.Equal(2, LF_Contraband.CountCarried(seller), "the undriven run keeps its crates");
		ctx.True(LF_BuyerDemandBook.Snapshot(buyerId, LF_Contraband.ITEM) == null, "a refused run leaves the purse untouched");
		LF_Contraband.MarkDriven(characterId);

		// An on-duty officer inside the outpost is refused and keeps the crates.
		account.SetFaction(LF_Faction.POLICE);
		account.SetOnDuty(true);
		ctx.True(LF_OutpostBuyer.Sell(seller, OUTPOST_ID, table, now, sold, paid) == LF_EOutpostSell.REFUSED_OFFICER, "an on-duty officer is refused inside the outpost");
		ctx.Equal(2, LF_Contraband.CountCarried(seller), "the refused officer keeps both crates");
		ctx.Equal(cashBefore, LF_MoneyUtils.GetCash(seller), "the refused officer is paid nothing");
		account.SetOnDuty(false);
		account.SetFaction(LF_Faction.CIVILIAN);

		// The purse has room for one crate: the driven run sells one at the row's price and is closed.
		LF_BuyerDemandRow live = LF_OutpostBuyer.RowFor(OUTPOST_ID);
		int roomLeft = 1;
		if (RedProof())
			roomLeft = live.m_iCap;
		LF_BuyerDemandBook.Restore(buyerId, LF_Contraband.ITEM, LF_BuyerPurse.Make(live, roomLeft, now));
		int pointsBefore = LF_Reputation.Points(SELLER_UID, LF_Reputation.FACTION_UNDERWORLD);
		ctx.True(LF_OutpostBuyer.Sell(seller, OUTPOST_ID, table, now, sold, paid) == LF_EOutpostSell.SOLD, "an off-duty seller on a driven run sells");
		ctx.Equal(1, sold, "one run sells one crate");
		ctx.Equal(live.m_iPrice, paid, "the crate fetches the price above the fence");
		ctx.Equal(cashBefore + paid, LF_MoneyUtils.GetCash(seller), "the seller is paid in cash");
		ctx.Equal(1, LF_Contraband.CountCarried(seller), "the second crate stays with the seller");
		ctx.True(LF_Reputation.Points(SELLER_UID, LF_Reputation.FACTION_UNDERWORLD) > pointsBefore, "the sale earns underworld standing");
		ctx.True(LF_OutpostStanding.IsKnownHere(SELLER_UID, OUTPOST_ID), "the seller is known at this outpost");

		// The run is paid: the second crate cannot ride on it, even with room in the purse.
		LF_BuyerDemandBook.Restore(buyerId, LF_Contraband.ITEM, LF_BuyerPurse.Make(live, live.m_iCap, now));
		cashBefore = LF_MoneyUtils.GetCash(seller);
		ctx.True(LF_OutpostBuyer.Sell(seller, OUTPOST_ID, table, now, sold, paid) == LF_EOutpostSell.NO_RUN, "a paid run sells no second crate");
		ctx.Equal(cashBefore, LF_MoneyUtils.GetCash(seller), "the second crate pays nothing on the paid run");

		// A new driven run after the cooldown, but the purse is at its cap: refused, the crate stays.
		int afterCooldown = System.GetTickCount() + LF_Contraband.COOLDOWN_MS + 1000;
		ctx.True(LF_Contraband.Start(characterId, null, afterCooldown), "a new run starts after the cooldown");
		LF_Contraband.MarkDriven(characterId);
		int emptied = 0;
		if (RedProof())
			emptied = live.m_iCap;
		LF_BuyerDemandBook.Restore(buyerId, LF_Contraband.ITEM, LF_BuyerPurse.Make(live, emptied, now));
		ctx.True(LF_OutpostBuyer.Sell(seller, OUTPOST_ID, table, now, sold, paid) == LF_EOutpostSell.BUYER_FULL, "a buyer at its cap refuses");
		ctx.Equal(1, LF_Contraband.CountCarried(seller), "the refused crate stays with the seller");
		ctx.Equal(cashBefore, LF_MoneyUtils.GetCash(seller), "a refused crate pays nothing");

		// An hour later the purse has refilled by one crate and the same run sells it.
		ctx.True(LF_OutpostBuyer.Sell(seller, OUTPOST_ID, table, now + 3600, sold, paid) == LF_EOutpostSell.SOLD, "the refilled buyer takes the crate an hour later");
		ctx.Equal(0, LF_Contraband.CountCarried(seller), "no crate is left");

		Cleanup(seller, zone, buyerId, purseBefore);
	}
}
