// red-proof: RED-OK residents/shop-terms (observed 2026-11-18 `tools/cli test --no-build --red LF_Test_ResidentShopTerms`: with no memory applied the refused buyer is sold to - charged 500, M9 handed over - and the regular pays full price)
//! The resident behind the counter sets the terms (#2911): a regular pays the discounted price at a
//! resident-run shop and full price at one with no resident clerk; a refused buyer gets the key and
//! no money or item moves; the discount never goes under what the shop pays (no profit loop).
//! Drives LF_ShopComponent.Buy on the GridWorld black market with real characters.
class LF_Test_ResidentShopTerms_Finder : LF_ResidentClerkFinder
{
	string m_sClerk;

	override string ClerkAt(notnull IEntity shop)
	{
		return m_sClerk;
	}
}

//------------------------------------------------------------------------------------------------
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_ResidentShopTerms : LF_Test
{
	protected static const ResourceName M9_PREFAB = "{1353C6EAD1DCFE43}Prefabs/Weapons/Handguns/M9/Handgun_M9.et";
	protected static const string CLERK = "t_clerk";

	override string GetName()
	{
		return "residents/shop-terms";
	}

	//------------------------------------------------------------------------------------------------
	//! One purchase by a fresh character with the uid `uid` and exactly `funds` cash.
	protected LF_EShopRefusal Attempt(LF_TestContext ctx, LF_ShopComponent shop, string uid, int funds, out int charged, out int cashLeft, out bool hasItem)
	{
		charged = 0;
		cashLeft = 0;
		hasItem = false;
		IEntity buyer = LF_TestCharacterFixture.Spawn(ctx);
		if (!buyer)
			return LF_EShopRefusal.NO_SHOP;

		LF_Utils.SetUidForTests(buyer, uid);
		// The black market's outlet gate wants the weapons licence; the terms under test come after it.
		LF_LicenseManagerComponent licences = LF_Component<LF_LicenseManagerComponent>.Find(buyer);
		if (licences)
			licences.UnlockLicense(LF_ELicenseType.CRIMINAL_WEAPONS, true);

		LF_MoneyUtils.AddCash(buyer, funds);
		string missing;
		LF_EShopRefusal reason = shop.Buy(M9_PREFAB, 1, buyer, charged, missing);
		cashLeft = LF_MoneyUtils.GetCash(buyer);
		hasItem = LF_TestCharacterFixture.HasItem(buyer, M9_PREFAB);
		LF_Utils.SetUidForTests(null, "");
		LF_TestCharacterFixture.Delete(buyer);
		return reason;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		// The rules, pure: only a regular pays less, never under the floor, never under 1.
		ctx.Equal(90, LF_ResidentShopTerms.UnitPrice(100, "regular", 10, 0), "a regular pays 90 % of 100");
		ctx.Equal(100, LF_ResidentShopTerms.UnitPrice(100, "friendly", 10, 0), "a friendly pays full");
		ctx.Equal(100, LF_ResidentShopTerms.UnitPrice(100, "neutral", 10, 0), "a neutral pays full");
		ctx.Equal(100, LF_ResidentShopTerms.UnitPrice(100, "", 10, 0), "no word (no resident clerk) pays full");
		ctx.Equal(95, LF_ResidentShopTerms.UnitPrice(100, "regular", 10, 95), "the discount stops at what the shop pays");
		ctx.Equal(100, LF_ResidentShopTerms.UnitPrice(100, "regular", 10, 400), "a floor over the price never raises it");
		ctx.Equal(1, LF_ResidentShopTerms.UnitPrice(1, "regular", 90, 0), "a one-coin item stays one coin");
		ctx.Equal(100, LF_ResidentShopTerms.UnitPrice(100, "regular", 0, 0), "0 percent turns the discount off");
		ctx.Equal(0, LF_ResidentShopTerms.UnitPrice(0, "regular", 10, 0), "an unpriced item stays unpriced");
		ctx.True(LF_ResidentShopTerms.Refuses("refuses"), "refuses is refused");
		ctx.False(LF_ResidentShopTerms.Refuses("wary"), "wary is served");
		ctx.True(LF_ResidentShopTermsConfig.Get().m_iRegularDiscountPercent == 10, "the default discount is 10 percent");
		ctx.True(LF_ShopRefusal.Key(LF_EShopRefusal.RESIDENT_REFUSES) == "#LF-Shop_RefuseResident", "the refusal has words");
		ctx.True(LF_ShopRefusal.FromWire(LF_EShopRefusal.RESIDENT_REFUSES) == LF_EShopRefusal.RESIDENT_REFUSES, "the wire keeps it");

		IEntity shopEntity = GetGame().GetWorld().FindEntityByName("Grid_shops_till_BlackMarket");
		ctx.NotNull(shopEntity, "Grid_shops_till_BlackMarket resolves in the GridWorld");
		if (!shopEntity)
			return;

		LF_ShopComponent shop = LF_Component<LF_ShopComponent>.Find(shopEntity);
		ctx.NotNull(shop, "the black market carries LF_ShopComponent");
		if (!shop)
			return;

		int price = shop.GetBuyPrice(M9_PREFAB);
		int floor = shop.GetSellPrice(M9_PREFAB);
		int regularPrice = LF_ResidentShopTerms.UnitPrice(price, LF_ResidentMemory.TIER_REGULAR, 10, floor);
		ctx.True(price >= 2 && regularPrice < price, "the M9's regular price is under its price");
		ctx.True(regularPrice >= floor, "the regular price never goes under what the shop pays (no profit loop)");
		if (ctx.FailureCount() > 0)
			return;

		LF_ResidentMemory.ResetStatics();
		LF_ResidentMemoryConfig memory = new LF_ResidentMemoryConfig();
		memory.m_iCooldownSeconds = 0;
		memory.m_iGossipPercent = 0;
		memory.m_iRefusesBelow = -60;
		memory.m_iWaryBelow = -20;
		memory.m_iFriendlyFrom = 20;
		memory.m_iRegularFrom = 60;
		memory.m_aReasons = {};
		memory.m_aReasons.Insert(LF_ResidentMemoryReason.Make("helped", 10, false));
		memory.m_aReasons.Insert(LF_ResidentMemoryReason.Make("crime_seen", -30, false));
		memory.m_aKnows = {};
		LF_ResidentMemoryConfig.UseForTest(memory);

		// Red-proof hook: with --red the clerk remembers nobody, so everyone reads neutral.
		if (!RedProof())
		{
			int now = 1000;
			for (int i = 0; i < 6; i++)
			{
				now += 10;
				LF_ResidentMemory.Apply(CLERK, "u_regular", "helped", now);
			}
			for (int j = 0; j < 3; j++)
			{
				now += 10;
				LF_ResidentMemory.Apply(CLERK, "u_refused", "crime_seen", now);
			}
		}

		// A resident keeps this shop.
		LF_Test_ResidentShopTerms_Finder finder = new LF_Test_ResidentShopTerms_Finder();
		finder.m_sClerk = CLERK;
		LF_ResidentShopTerms.UseFinderForTest(finder);

		int charged;
		int cashLeft;
		bool hasItem;
		LF_EShopRefusal reason = Attempt(ctx, shop, "u_regular", price, charged, cashLeft, hasItem);
		ctx.True(reason == LF_EShopRefusal.NONE, "a regular buys at the resident's shop");
		ctx.Equal(regularPrice, charged, "the regular is charged the discounted price");
		ctx.Equal(price - regularPrice, cashLeft, "the regular keeps the difference");
		ctx.True(hasItem, "the regular is handed the M9");

		reason = Attempt(ctx, shop, "u_new", price, charged, cashLeft, hasItem);
		ctx.True(reason == LF_EShopRefusal.NONE, "a stranger buys at the resident's shop");
		ctx.Equal(price, charged, "a stranger pays the full price");
		ctx.Equal(0, cashLeft, "a stranger's cash is spent in full");

		reason = Attempt(ctx, shop, "u_refused", price, charged, cashLeft, hasItem);
		ctx.True(reason == LF_EShopRefusal.RESIDENT_REFUSES, "the victim is refused service");
		ctx.Equal(0, charged, "the refused buyer is charged nothing");
		ctx.Equal(price, cashLeft, "no money moves from the refused buyer");
		ctx.False(hasItem, "the refused buyer is handed nothing");

		// No resident keeps this shop: the same people get the old deal.
		finder.m_sClerk = "";
		reason = Attempt(ctx, shop, "u_regular", price, charged, cashLeft, hasItem);
		ctx.True(reason == LF_EShopRefusal.NONE, "elsewhere a regular still buys");
		ctx.Equal(price, charged, "elsewhere a regular pays full price");
		reason = Attempt(ctx, shop, "u_refused", price, charged, cashLeft, hasItem);
		ctx.True(reason == LF_EShopRefusal.NONE, "elsewhere the refused buyer is served");
		ctx.Equal(price, charged, "elsewhere the refused buyer pays full price");

		LF_ResidentShopTerms.ResetStatics();
		LF_ResidentMemory.ResetStatics();
		LF_ResidentMemoryConfig.ResetStatics();
	}
};
