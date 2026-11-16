// red-proof: Observed 2026-11-16 with `tools\cli test --no-build --red LF_Test_BlackMarketOutlet`: RED-OK catalogue/black-market (fails with the hook on, 87 assertions); the hook lets the buyer hold CRIMINAL_WEAPONS before the first order, so `without the gate the back room refuses` fails. Hook off: green.
// tier: WORLD
//! Weapons are sold only through outlets (#1762, Refs #1065): the back room is the back_room outlet
//! behind CRIMINAL_WEAPONS and names the licence it lacks, the black_market outlet gates both its
//! guns the same way, no shop offers the police rifle, the Prefabs/Weapons/ price rule is gone, and
//! a shop with neither a list nor an outlet sells nothing.
class LF_Test_BlackMarketOutlet : LF_Test
{
	protected static const ResourceName PISTOL = "{1353C6EAD1DCFE43}Prefabs/Weapons/Handguns/M9/Handgun_M9.et";
	protected static const ResourceName AK74 = "{FA5C25BF66A53DCF}Prefabs/Weapons/Rifles/AK74/Rifle_AK74.et";
	protected static const ResourceName M16 = "{3E413771E1834D2F}Prefabs/Weapons/Rifles/M16/Rifle_M16A2.et";
	protected static const string LICENCE_NAME = "#LF-License_CriminalWeapons_Name";

	override string GetName()
	{
		return "catalogue/black-market";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_ConfigService.Reset();
		LF_ConfigStream.Reset();
		LF_ItemCatalogueConfig.Reload();

		CheckOutletData(ctx);
		CheckPriceRule(ctx);
		CheckNoSourceSellsNothing(ctx);
		CheckGeneralStore(ctx);
		CheckBackRoom(ctx);
	}

	//------------------------------------------------------------------------------------------------
	//! The black_market outlet sells the pistol and the AK-74, each behind CRIMINAL_WEAPONS.
	protected void CheckOutletData(LF_TestContext ctx)
	{
		LF_ItemCatalogueConfig catalogue = LF_ItemCatalogueConfig.Get();
		array<LF_OutletOffer> offers = {};
		LF_ItemCatalogue.Offers(catalogue, LF_BackRoomRules.OUTLET_BLACK_MARKET, offers);
		ctx.Equal(2, offers.Count(), "the black_market outlet lists the pistol and the AK-74");
		foreach (LF_OutletOffer offer : offers)
			ctx.EqualStr("CRIMINAL_WEAPONS", offer.m_sLicence, "every black_market offer is behind CRIMINAL_WEAPONS");

		LF_CatalogueItem entry;
		ctx.NotNull(LF_ItemCatalogue.OfferFor(catalogue, LF_BackRoomRules.OUTLET_BLACK_MARKET, AK74, entry), "the black market sells the AK-74");
		if (entry)
			ctx.EqualStr("illegal", entry.m_sLegality, "the AK-74 is an illegal item");

		ctx.True(LF_ItemCatalogue.OfferFor(catalogue, LF_BackRoomRules.OUTLET_BLACK_MARKET, M16, entry) == null, "the black market does not sell the police rifle");
	}

	//------------------------------------------------------------------------------------------------
	//! No weapon is priced by a path rule any more: the flat price is gone, the Prefabs/Weapons/ rule only
	//! hides weapons from price-rule shops, and the only prices a weapon has are catalogue prices.
	protected void CheckPriceRule(LF_TestContext ctx)
	{
		LF_ShopManagerComponent manager = LF_ShopManagerComponent.GetInstance();
		ctx.NotNull(manager, "the shop manager exists");
		if (!manager)
			return;

		LF_ShopPricesConfig prices = manager.GetConfig();
		ctx.NotNull(prices, "the shop price config loads");
		if (!prices || !prices.m_aPriceRules)
			return;

		foreach (LF_ShopPriceRule rule : prices.m_aPriceRules)
		{
			if (!rule)
				continue;

			if (rule.m_sFind.Contains("Prefabs/Weapons/"))
			{
				ctx.True(rule.m_bHidden, "the Prefabs/Weapons/ rule hides weapons from price-rule shops");
			}

			ctx.False(rule.m_sFind.Contains("Handgun_M9"), "no price rule matches the pistol by name");
			ctx.False(rule.m_sFind.Contains("Prefabs/Weapons/") && !rule.m_bHidden, "no visible price rule matches the weapons path");
		}
	}

	//------------------------------------------------------------------------------------------------
	//! A shop with no stock, no outlet and no curated list is not a price-rule shop any more.
	protected void CheckNoSourceSellsNothing(LF_TestContext ctx)
	{
		ctx.False(LF_ShopComponent.HasSellSource(false, false, false), "a shop with no stock, no outlet and no list sells nothing");
		ctx.True(LF_ShopComponent.HasSellSource(false, false, true), "a curated list is a source");
		ctx.True(LF_ShopComponent.HasSellSource(false, true, false), "an outlet is a source");
		ctx.True(LF_ShopComponent.HasSellSource(true, false, false), "a stock is a source");
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckGeneralStore(LF_TestContext ctx)
	{
		IEntity store = GetGame().GetWorld().FindEntityByName("Grid_shops_till_Store");
		ctx.NotNull(store, "Grid_shops_till_Store resolves in the GridWorld");
		if (!store)
			return;

		LF_ShopComponent shop = LF_Component<LF_ShopComponent>.Find(store);
		ctx.NotNull(shop, "the general store carries LF_ShopComponent");
		if (!shop)
			return;

		ctx.False(shop.IsItemSoldHere(M16), "the general store does not sell the M16A2");
		foreach (LF_ShopCatalogItem row : shop.GetShopItems())
			ctx.False(row.m_ItemPrefab == M16, "no row of the general store is the M16A2");
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckBackRoom(LF_TestContext ctx)
	{
		IEntity room = GetGame().GetWorld().FindEntityByName("Grid_shops_till_BlackMarket");
		ctx.NotNull(room, "Grid_shops_till_BlackMarket resolves in the GridWorld");
		if (!room)
			return;

		LF_ShopComponent shop = LF_Component<LF_ShopComponent>.Find(room);
		ctx.NotNull(shop, "the back room carries LF_ShopComponent");
		if (!shop)
			return;

		ctx.True(shop.IsBackRoom(), "the room is a back room");
		ctx.EqualStr(LF_BackRoomRules.OUTLET_BACK_ROOM, shop.GetOutletId(), "the back room is the back_room outlet");
		ctx.True(shop.IsItemSoldHere(PISTOL), "the back room sells the pistol");
		ctx.False(shop.IsItemSoldHere(M16), "the back room does not sell the M16A2");
		ctx.False(shop.IsItemSoldHere(AK74), "the back room does not sell the AK-74");
		int price = shop.GetBuyPrice(PISTOL);
		ctx.Equal(500, price, "the pistol costs the catalogue price");

		IEntity buyer = LF_TestCharacterFixture.Spawn(ctx);
		if (!buyer)
			return;

		LF_MoneyUtils.AddCash(buyer, price);
		int charged;
		string missing;
		LF_LicenseManagerComponent licences = LF_Component<LF_LicenseManagerComponent>.Find(buyer);
		ctx.NotNull(licences, "the test character has a licence manager");
		// Red proof: a buyer who already holds the licence walks through the gate the test says is shut.
		if (RedProof() && licences)
			licences.UnlockLicense(LF_ELicenseType.CRIMINAL_WEAPONS, true);

		ctx.Equal(LF_EShopRefusal.NEEDS_LICENCE, shop.Buy(PISTOL, 1, buyer, charged, missing), "without the gate the back room refuses");
		ctx.EqualStr(LICENCE_NAME, missing, "the refusal names the criminal weapons licence");
		ctx.Equal(0, charged, "a refused order charges nothing");
		ctx.Equal(price, LF_MoneyUtils.GetCash(buyer), "the refusal moved no cash");
		ctx.Equal(0, LF_InventoryUtils.GetAmount(buyer, PISTOL), "the refusal delivered nothing");

		if (licences && !RedProof())
		{
			ctx.True(licences.UnlockLicense(LF_ELicenseType.CRIMINAL_WEAPONS, true), "the licence is granted");
			ctx.Equal(LF_EShopRefusal.NONE, shop.Buy(PISTOL, 1, buyer, charged, missing), "with the licence the order is paid");
			ctx.Equal(price, charged, "the slip reports the catalogue price");
			ctx.Equal(1, LF_InventoryUtils.GetAmount(buyer, PISTOL), "the buyer holds the pistol");
		}

		LF_TestCharacterFixture.Delete(buyer);
	}
}
